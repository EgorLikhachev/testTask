#!/usr/bin/env python3
"""Мост SITL -> приложение и драйвер интеграционного сценария.

Соединяется с arducopter SITL по TCP 5760, пробрасывает весь MAVLink-трафик
в UDP 127.0.0.1:14550 (и обратно), а по расписанию выполняет сценарий:
смена режимов, быстрый повтор (антиспам), arm/disarm после готовности EKF,
просадка батареи и фейлсейф-STATUSTEXT.

Оговорка: процент заряда (SYS_STATUS.battery_remaining) инжектируется
синтетически — в SITL с дефолтным BATT_MONITOR=4 борт не отдаёт %, а задача
сценария — проверить цепочку приложения, а не симулятор батареи ArduPilot.

Запуск: python3 scripts/sitl_bridge.py [--max-sec 180]
"""
import argparse
import os
import select
import socket
import sys
import time

sys.path.insert(0, os.path.join(os.path.expanduser("~"), ".local/lib/python3.12/site-packages"))

from pymavlink.dialects.v20 import ardupilotmega as ap20  # noqa: E402

mavlink = ap20  # enums, константы и классы сообщений (v2.0)

TCP_ADDR = ("127.0.0.1", 5760)
UDP_ADDR = ("127.0.0.1", 14550)
# 254, чтобы не конфликтовать с приложением (255/190) на одной связи.
SYSID, COMPID = 254, 190

MODE_GUIDED, MODE_LOITER = 4, 5


class Bridge:
    def __init__(self):
        self.tcp = socket.create_connection(TCP_ADDR, timeout=5)
        self.tcp.setblocking(False)
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.bind(("127.0.0.1", 0))
        self.parser = ap20.MAVLink(self)   # разбор входящего потока
        self.parser.robust_parsing = True  # мусор -> BAD_DATA вместо исключений
        self.packer = ap20.MAVLink(self)   # исходящие команды
        self.packer.srcSystem = SYSID
        self.packer.srcComponent = COMPID
        self.batt_voltage_max = None
        self.armed = None
        self.mode = None
        self.got_ekf_gps = False
        self.last_sys_status = None
        self.last_hb_sent = 0.0

    # ---- отправка ----
    def send_to_fc(self, msg):
        try:
            self.tcp.sendall(msg.pack(self.packer))
        except (BlockingIOError, OSError):
            pass

    def send_to_app(self, msg):
        self.udp.sendto(msg.pack(self.packer), UDP_ADDR)

    def send_heartbeat(self):
        self.send_to_fc(self.packer.heartbeat_encode(
            mavlink.MAV_TYPE_GCS, mavlink.MAV_AUTOPILOT_INVALID, 0, 0,
            mavlink.MAV_STATE_ACTIVE))

    def set_mode(self, custom_mode):
        self.send_to_fc(mavlink.MAVLink_command_long_message(
            1, 1, mavlink.MAV_CMD_DO_SET_MODE, 0,
            mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, float(custom_mode),
            0, 0, 0, 0, 0))

    def set_arm(self, arm):
        self.send_to_fc(mavlink.MAVLink_command_long_message(
            1, 1, mavlink.MAV_CMD_COMPONENT_ARM_DISARM, 0,
            1.0 if arm else 0.0, 0, 0, 0, 0, 0, 0))

    def set_param(self, name, value):
        self.send_to_fc(mavlink.MAVLink_param_set_message(
            1, 1, name.encode(), value, mavlink.MAV_PARAM_TYPE_REAL32))

    def request_param(self, name):
        self.send_to_fc(mavlink.MAVLink_param_request_read_message(
            1, 1, name.encode(), -1))

    # ---- цикл и разбор ----
    def pump(self, timeout=0.2):
        now = time.monotonic()
        if now - self.last_hb_sent > 1.0:
            self.last_hb_sent = now
            self.send_heartbeat()

        try:
            r, _, _ = select.select([self.tcp, self.udp], [], [], timeout)
        except OSError:
            return
        for s in r:
            if s is self.tcp:
                try:
                    data = self.tcp.recv(65536)
                except (BlockingIOError, ConnectionResetError):
                    continue
                if not data:
                    raise ConnectionError("SITL закрыл TCP-соединение")
                self.udp.sendto(data, UDP_ADDR)
                for msg in (self.parser.parse_buffer(data) or []):
                    self.handle(msg)
            else:
                data, _ = self.udp.recvfrom(65536)
                try:
                    self.tcp.sendall(data)
                except (BlockingIOError, OSError):
                    pass

    def handle(self, msg):
        t = msg.get_type()
        if t == "BAD_DATA":
            return
        if t == "HEARTBEAT":
            self.armed = bool(msg.base_mode & mavlink.MAV_MODE_FLAG_SAFETY_ARMED)
            self.mode = msg.custom_mode
        elif t == "PARAM_VALUE":
            if msg.param_id == b"SIM_BATT_VOLTAGE" and self.batt_voltage_max is None:
                self.batt_voltage_max = float(msg.param_value)
                print(f"[bridge] SIM_BATT_VOLTAGE (полный) = {self.batt_voltage_max} В")
        elif t == "STATUSTEXT":
            raw = msg.text
            if isinstance(raw, str):
                text = raw.split("\0")[0]
            else:
                text = bytes(raw).split(b"\0")[0].decode("latin1", "replace")
            print(f"[bridge] STATUSTEXT sev={msg.severity}: {text}")
            if "using GPS" in text:
                self.got_ekf_gps = True
        elif t == "COMMAND_ACK":
            name = {mavlink.MAV_CMD_DO_SET_MODE: "SET_MODE",
                    mavlink.MAV_CMD_COMPONENT_ARM_DISARM: "ARM_DISARM"}.get(
                        msg.command, msg.command)
            print(f"[bridge] ACK {name} result={msg.result}")
        elif t == "SYS_STATUS":
            self.last_sys_status = msg


def wait_for(fn, bridge, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        bridge.pump()
        if fn():
            return True
    print(f"[bridge] таймаут ожидания: {what}")
    return False


def sleep_pump(br, sec):
    end = time.monotonic() + sec
    while time.monotonic() < end:
        br.pump()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-sec", type=float, default=180)
    args = ap.parse_args()

    br = Bridge()
    print("[bridge] подключён к SITL")
    wait_for(lambda: br.armed is not None, br, 15, "heartbeat")
    br.request_param("SIM_BATT_VOLTAGE")
    wait_for(lambda: br.batt_voltage_max is not None, br, 10, "SIM_BATT_VOLTAGE")

    # 1) смены режимов (первый уже озвучен приложением при коннекте)
    sleep_pump(br, 3)
    print("[bridge] mode GUIDED")
    br.set_mode(MODE_GUIDED)
    sleep_pump(br, 6)
    print("[bridge] mode LOITER")
    br.set_mode(MODE_LOITER)
    sleep_pump(br, 2)
    print("[bridge] mode GUIDED (повтор — антиспам)")
    br.set_mode(MODE_GUIDED)

    # 2) ждать готовности EKF
    print("[bridge] жду готовности EKF...")
    wait_for(lambda: br.got_ekf_gps, br, 80, "EKF/GPS")
    sleep_pump(br, 3)

    # 3) arm / disarm
    print("[bridge] arm throttle")
    br.set_arm(True)
    wait_for(lambda: br.armed is True, br, 10, "arm")
    if br.armed is not True:
        print("[bridge] arm не прошёл, повтор")
        sleep_pump(br, 3)
        br.set_arm(True)
        wait_for(lambda: br.armed is True, br, 10, "arm (повтор)")
    sleep_pump(br, 6)
    print("[bridge] disarm")
    br.set_arm(False)
    wait_for(lambda: br.armed is False, br, 10, "disarm")
    sleep_pump(br, 2)

    # 4) просадка заряда: синтетический SYS_STATUS в сторону приложения
    #    (пороги в gcs-tts-integration.ini: warn=60, critical=45)
    if br.last_sys_status is not None:
        for pct in (50, 30):
            print(f"[bridge] инжекция SYS_STATUS battery_remaining={pct}%")
            s = br.last_sys_status
            s.battery_remaining = pct
            for _ in range(3):  # несколько кадров, приложение берёт последний
                br.send_to_app(s)
                time.sleep(0.3)
            sleep_pump(br, 3)

    # 5) просадка напряжения: фейлсейф ArduPilot -> STATUSTEXT WARNING+
    if br.batt_voltage_max:
        print("[bridge] SIM_BATT_VOLTAGE=0.4*max (фейлсейф/STATUSTEXT)")
        br.set_param("SIM_BATT_VOLTAGE", round(br.batt_voltage_max * 0.4, 2))
    sleep_pump(br, 8)

    print("[bridge] сценарий завершён")
    return 0


if __name__ == "__main__":
    sys.exit(main())
