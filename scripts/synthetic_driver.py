#!/usr/bin/env python3
"""Синтетический интеграционный тест mav-voice-gcs без ArduPilot.

Запускает приложение, затем сам синтезирует MAVLink-кадры (heartbeat,
SYS_STATUS, STATUSTEXT) и отправляет их на UDP 127.0.0.1:14550 по
сценарию. Проверяет фразы в логе приложения — покрывает всю цепочку
«транспорт -> парсер -> домен -> озвучка» без тяжёлых зависимостей.

Используется в CI; локально — scripts/synthetic_test.sh.
"""
import argparse
import glob
import inspect
import os
import signal
import socket
import subprocess
import sys
import time

try:
    from pymavlink.dialects.v20 import ardupilotmega as ap20
except ImportError:
    sys.path.insert(
        0, os.path.expanduser("~/.local/lib/python3.12/site-packages"))
    from pymavlink.dialects.v20 import ardupilotmega as ap20

mavlink = ap20

UDP_ADDR = ("127.0.0.1", 14550)
MODE_STABILIZE, MODE_GUIDED, MODE_LOITER = 0, 4, 5


def make_msg(cls, **kw):
    """Конструирует сообщение по именованным полям; отсутствующие в данной
    версии диалекта поля отбрасываются, недостающие дополняются нулями
    (схема SYS_STATUS менялась между версиями pymavlink)."""
    params = [p for p in inspect.signature(cls.__init__).parameters
              if p != "self"]
    values = [kw.get(p, 0) for p in params]
    return cls(*values)


class Sender:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.packer = ap20.MAVLink(self)
        self.packer.srcSystem = 1
        self.packer.srcComponent = 1

    def send(self, msg):
        self.sock.sendto(msg.pack(self.packer), UDP_ADDR)

    def heartbeat(self, mode, armed):
        base = mavlink.MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
        if armed:
            base |= mavlink.MAV_MODE_FLAG_SAFETY_ARMED
        self.send(make_msg(
            mavlink.MAVLink_heartbeat_message,
            type=mavlink.MAV_TYPE_QUADROTOR,
            autopilot=mavlink.MAV_AUTOPILOT_ARDUPILOTMEGA,
            base_mode=base, custom_mode=mode,
            system_status=mavlink.MAV_STATE_ACTIVE))

    def sys_status(self, remaining_pct):
        self.send(make_msg(
            mavlink.MAVLink_sys_status_message,
            load=500, voltage_battery=12000, current_battery=400,
            battery_remaining=remaining_pct))

    def statustext(self, text, severity=mavlink.MAV_SEVERITY_WARNING):
        self.send(make_msg(
            mavlink.MAVLink_statustext_message,
            severity=severity, text=text.encode()[:50]))


def run_scenario(snd, loss_sec):
    """Сценарий по тактам; между действиями гонит heartbeat 2 Гц."""
    t0 = time.monotonic()
    # Начальное состояние совпадает с первым плановым тактом (STABILIZE),
    # иначе keep-alive опережает сценарий и портит ожидания.
    state = {"mode": MODE_STABILIZE, "armed": False}

    def hb(mode, armed):
        state["mode"], state["armed"] = mode, armed
        snd.heartbeat(mode, armed)

    plan = [
        (1.0, lambda: hb(MODE_STABILIZE, False)),   # первая фраза режима
        (2.0, lambda: snd.sys_status(100)),         # батарея норма
        (5.5, lambda: hb(MODE_GUIDED, False)),      # >4 c после первой фразы
        (10.5, lambda: hb(MODE_LOITER, False)),
        (12.5, lambda: hb(MODE_GUIDED, False)),     # 2 c после LOITER: антиспам
        (14.5, lambda: hb(MODE_GUIDED, True)),      # arm
        (18.5, lambda: hb(MODE_GUIDED, False)),     # disarm
        (20.5, lambda: snd.sys_status(50)),         # warning (порог 60)
        (23.5, lambda: snd.sys_status(30)),         # critical (порог 45)
        (25.5, lambda: snd.statustext("Synthetic test warning")),
    ]

    last_hb = 0.0
    idx = 0
    quiet_from = 26.5                # с этого момента кадры прекращаются
    total = quiet_from + loss_sec + 3.0
    while time.monotonic() - t0 < total:
        now = time.monotonic() - t0
        while idx < len(plan) and plan[idx][0] <= now:
            plan[idx][1]()
            idx += 1
        # держим канал живым текущим состоянием (до финальной тишины)
        if now < quiet_from and now - last_hb >= 0.5:
            snd.heartbeat(state["mode"], state["armed"])
            last_hb = now
        time.sleep(0.05)
    # Тишина дольше loss_sec уже дала «потерю связи» — возвращаем трафик.
    snd.heartbeat(state["mode"], state["armed"])
    time.sleep(2.0)
    snd.heartbeat(state["mode"], state["armed"])
    time.sleep(2.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--app-bin", required=True)
    ap.add_argument("--config", required=True)
    ap.add_argument("--loss-sec", type=float, default=3.0)
    args = ap.parse_args()

    wav_dir = "/tmp/gcs-synth-wav"
    log_path = os.path.expanduser("~/gcs-synth-app.log")
    subprocess.run(["rm", "-rf", wav_dir], check=False)
    if os.path.exists(log_path):
        os.remove(log_path)

    env = dict(os.environ)
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["GCS_TTS_WAV_DIR"] = wav_dir

    print(f"[synth] запуск приложения: {args.app_bin}")
    with open(log_path, "wb") as log:
        app = subprocess.Popen([args.app_bin, "--config", args.config],
                               stdout=log, stderr=subprocess.STDOUT, env=env)
        try:
            time.sleep(2.0)
            print("[synth] сценарий (~35 c)…")
            snd = Sender()
            run_scenario(snd, args.loss_sec)
        finally:
            app.send_signal(signal.SIGTERM)
            try:
                app.wait(timeout=5)
            except subprocess.TimeoutExpired:
                app.kill()

    log_text = open(log_path, encoding="utf-8", errors="replace").read()
    wavs = glob.glob(os.path.join(wav_dir, "*.wav"))

    checks = [
        ("захват борта (sysid)", "борт зафиксирован: sysid 1"),
        ("связь установлена", "Связь с бортом установлена"),
        ("режим стабилизация", "Режим полёта: стабилизация"),
        ("режим наведение", "Режим полёта: наведение"),
        ("режим лойтер", "Режим полёта: лойтер"),
        ("антиспам повтора", "подавлено антиспамом"),
        ("arm", "Моторы запущены"),
        ("disarm", "Моторы остановлены"),
        ("батарея warning", "Заряд батареи низкий"),
        ("батарея critical", "Критический заряд батареи"),
        ("STATUSTEXT дословно", "Synthetic test warning"),
        ("потеря связи", "Потеря связи с бортом"),
        ("восстановление связи", "Связь с бортом восстановлена"),
        ("очередь дошла до синтеза", None),  # по WAV ниже
    ]
    ok = True
    for name, needle in checks:
        if needle is None:
            hit = len(wavs) > 0
            name = f"очередь дошла до синтеза ({len(wavs)} WAV)"
        else:
            hit = needle in log_text
        print(("PASS  " if hit else "FAIL  ") + name)
        ok = ok and hit

    print("РЕЗУЛЬТАТ: " + ("ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ" if ok else "ЕСТЬ ОТКАЗЫ"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
