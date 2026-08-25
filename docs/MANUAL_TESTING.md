> 🇬🇧 English version. · [🇷🇺 Русская версия](MANUAL_TESTING.ru.md)

# Manual acceptance testing methodology

This document describes manual verification of the MAVLink telemetry
receiver with voice announcements against every requirement of the original
specification. It is written for an engineer standing up the test bench for
the first time: installation is covered step by step.

- System under test: the `gcs-tts` application (this repository).
- Telemetry source: ArduPilot SITL (ArduCopter), MAVLink over
  UDP 127.0.0.1:14550.
- Automated counterpart: `scripts/synthetic_test.sh` (14 checks, no
  ArduPilot needed) and `scripts/integration.sh` (11 checks with SITL);
  this manual methodology confirms the same items “by ear and by eye”.

## 1. Test bench

Two supported configurations:

| Configuration | OS | Sound |
|---|---|---|
| A | Ubuntu 22.04 / 24.04 (native or VM) | direct |
| B | Windows 10/11 + WSL2 Ubuntu 24.04 | via WSLg (PulseAudio) |

Minimum resources: 2 CPU cores, 4 GB RAM, ~8 GB disk (ArduPilot 3–5 GB,
Qt ~1.5 GB). All commands below run in an Ubuntu terminal.

## 2. Bench setup (once)

### 2.1. System packages

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git python3 python3-pip \
    espeak-ng libgl1-mesa-dev libgl-dev libopengl-dev libegl-dev libglx-dev \
    libxkbcommon-x11-0 libxcb-xinerama0 libxcb-cursor0 libfontconfig1 libdbus-1-3 \
    libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 \
    libxcb-randr0 libxcb-xfixes0 p7zip-full
```

Sound check (in configuration B the sound must be heard on the Windows host):

```bash
espeak-ng -v ru "Проверка голосового модуля"
```

If the command exits without errors but there is no sound, testing can
proceed in WAV-recording mode (see section 5) — phrases are then verified
by the log and the files.

### 2.2. Qt 6.5+

Ubuntu repositories ship Qt ≤ 6.4 while the spec requires 6.5+. The
standard route is aqtinstall:

```bash
python3 -m pip install --user --break-system-packages aqtinstall
~/.local/bin/aqt install-qt linux desktop 6.5.3 gcc_64 -O ~/Qt
```

If `download.qt.io` is unreachable from your network (aqt fails while
downloading checksums), install Qt manually from any mirror:

```bash
B=https://mirrors.ocf.berkeley.edu/qt/online/qtsdkrepository/linux_x64/desktop/qt6_653/qt.qt6.653.gcc_64
S=6.5.3-0-202309260341
curl -fLO "$B/${S}qtbase-Linux-RHEL_8_4-GCC-Linux-RHEL_8_4-X86_64.7z"
curl -fLO "$B/${S}icu-linux-Rhel7.2-x64.7z"
sudo mkdir -p /opt/qt && sudo 7z x -y -o/opt/qt ./*.7z
sudo chmod -R a+rX /opt/qt
```

### 2.3. This repository

```bash
git clone --recurse-submodules https://github.com/EgorLikhachev/testTask.git
cd testTask
```

The `--recurse-submodules` flag is mandatory: it fetches the MAVLink
headers `c_library_v2` (~10 MB). Without them the build fails on
`#include <ardupilotmega/mavlink.h>`.

### 2.4. Build and unit tests

```bash
./scripts/build.sh
```

The script finds Qt on its own (`~/Qt/6.5.3/gcc_64` or
`/opt/qt/6.5.3/gcc_64`), builds the application and the tests, then runs
`ctest`. Expected output:

```text
100% tests passed, 0 tests failed out of 3
==> Бинарник: ~/build/mav-voice-gcs/gcs-tts   (in WSL)
            build/gcs-tts                     (native)
```

### 2.5. ArduPilot SITL (installed separately, not part of the repo)

```bash
sudo apt-get install -y python3-dev python3-opencv libxml2-dev libxslt1-dev \
    g++ gcc make gawk wget curl python3-matplotlib python3-numpy python3-pyparsing
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git ~/ardupilot
cd ~/ardupilot
Tools/environment_install/install-prereqs-ubuntu.sh -y
python3 -m pip install --user --break-system-packages MAVProxy future 'empy==3.3.4'
./waf configure --board sitl && ./waf copter
```

The build takes 5–30 minutes; the success indicator is
`build/sitl/bin/arducopter`.

## 3. Starting the bench (before every session)

Three terminals (or `tmux`):

**Terminal 1 — simulator** (runs until the end of the session; exit with `Ctrl-C`):

```bash
cd ~/ardupilot
./build/sitl/bin/arducopter --model quad --speedup 1 -I0
```

**Terminal 2 — vehicle console** (MAVProxy):

```bash
mavproxy.py --master tcp:127.0.0.1:5760 --console
```

**Terminal 3 — the application under test**:

```bash
cd <repository directory>
~/build/mav-voice-gcs/gcs-tts --config config/gcs-tts.ini
```

A window opens showing mode, ARM state, battery, altitude, speed, link
state, message rate, the status button, the voice toggle and the event
log. The application console prints `[announce] …` lines — the spoken
phrases.

> For reproducible results between sessions, reset the simulator state:
> `rm -rf /tmp/gcs-sitl-root`, then create the directory and launch
> `arducopter` from inside it — the EEPROM starts clean.

## 4. Test cases

Common precondition for all tests: the bench from section 3 is running and
the window shows “СВЯЗЬ ЕСТЬ” (link up).

### T-01. Telemetry reception

| | |
|---|---|
| Goal | The application connects to SITL and receives the stream |
| Steps | 1. Start the bench (section 3). 2. Watch the window for 10–15 s |
| Expected | 1–3 s after MAVProxy starts: “СВЯЗЬ ЕСТЬ” (green), message rate > 30/s, console shows `[app] запрошен стрим сообщений у борта 1/1`; first phrase “Режим полёта: стабилизация” |
| Pass criteria | Link stays up; the parse-error counter does not grow |

### T-02. Status hotkey (on the ground)

| | |
|---|---|
| Goal | F2 speaks altitude/speed/battery |
| Steps | 1. Press F2 in the application window (or the “Статус” button). 2. Repeat after 1 s and after 5 s |
| Expected | A phrase like “Высота 0 метров. Скорость 0 метров в секунду. Заряд батареи …” (battery may be absent — see T-06); the 1 s repeat is suppressed by anti-spam (log shows “подавлено антиспамом”), the 5 s one is spoken |
| Pass criteria | Hotkey and button behave identically |

### T-03. Flight mode change

| | |
|---|---|
| Goal | Mode changes are spoken |
| Steps | In MAVProxy: `mode GUIDED`, then `mode LOITER`, then `mode RTL` (10 s pause between commands) |
| Expected | Phrases: “Режим полёта: наведение”, “Режим полёта: лойтер”, “Режим полёта: возврат в точку взлёта”; the window mode field changes in sync |
| Pass criteria | All three changes spoken and displayed |

### T-04. Anti-spam for repeated events

| | |
|---|---|
| Goal | The same event is spoken at most once per N seconds |
| Steps | In MAVProxy quickly: `mode GUIDED`, after 2 s `mode LOITER`, after 2 s `mode GUIDED` |
| Expected | The first two changes are spoken; the third produces “смена режима: GUIDED — подавлено антиспамом (10 с)” in the log and is NOT spoken (`mode_change_sec = 10` by default) |
| Pass criteria | Suppression visible in the log; repeating the command after 10 s speaks again |

### T-05. Arm / disarm

| | |
|---|---|
| Preconditions | EKF ready (`arm throttle` produces no PreArm; readiness arrives ~30–60 s after SITL start) |
| Steps | 1. `arm throttle`. 2. Confirm motors spin (SITL console/mavexplorer). 3. `disarm` |
| Expected | “Внимание! Моторы запущены” / “Моторы остановлены”; the window switches ARMED (red) → DISARMED (green) |
| Pass criteria | Both transitions spoken |

### T-06. Battery warning / critical thresholds

By default SITL does not report state of charge (BATT_MONITOR=4), so this
test switches the vehicle to a current-integrating monitor with a small
capacity — the percentage then genuinely drains and the application crosses
both thresholds.

Preconditions: T-01…T-05 completed (arming is not needed anymore).

Steps:

1. Stop the application (terminal 3), restart with the test config
   (thresholds: warn 60 %, critical 45 %):

   ```bash
   ~/build/mav-voice-gcs/gcs-tts --config config/gcs-tts-integration.ini
   ```

2. In MAVProxy reconfigure the battery and reboot the vehicle:

   ```text
   param set BATT_MONITOR 5
   param set BATT_CURR_PIN 12
   param set BATT_CAPACITY 10
   reboot
   ```

   MAVProxy reconnects on its own; restart terminal 2 if it does not. The
   application reconnects automatically.

3. Watch the battery field: the percentage appears and decreases
   (electronics consumption drains 10 mAh in tens of seconds).

Expected: crossing 60 % — “Внимание! Заряд батареи низкий: N процентов”;
crossing 45 % — “Критический заряд батареи! N процентов. Требуется
немедленная посадка”; the battery field color turns orange → red.

Pass criteria: both phrases spoken; hovering at a threshold does not spam
repeats (hysteresis: a repeat requires recovery above threshold + 5 %).

Note: if the percentage drops too fast and a threshold slips between
SYS_STATUS frames, increase `BATT_CAPACITY` (e.g. 30) and restart the
vehicle with a clean EEPROM.

### T-07. STATUSTEXT severity WARNING+ verbatim

| | |
|---|---|
| Goal | Incoming warnings are spoken verbatim |
| Steps | 1. Before EKF readiness (right after an SITL restart) run `arm throttle`. 2. The arm refusal produces PreArm texts. 3. Alternative/addition: `param set SIM_BATT_VOLTAGE 10.5` |
| Expected | Verbatim phrases such as “Критическое событие! PreArm: Need Position Estimate”, “…PreArm: Battery 1 low voltage failsafe”; notice-level messages (severity 6, e.g. “Calibrating barometer”) are NOT spoken |
| Pass criteria | Heavy severity — verbatim; light severity — silence |

### T-08. Altitude and speed in the status speech (flight)

| | |
|---|---|
| Preconditions | EKF ready, GPS 3D fix (`gps status` in MAVProxy) |
| Steps | 1. `mode GUIDED`. 2. `takeoff 20`. 3. Wait for ~20 m climb. 4. Press F2. 5. `land`, press F2 again after touchdown |
| Expected | In flight: “Высота 19–21 метров. Скорость … метров в секунду…”; on the ground: altitude near zero |
| Pass criteria | Spoken values match the window within rounding |

### T-09. Mute

| | |
|---|---|
| Steps | 1. Click “Озвучка: вкл” → the button flips to “Озвучка: выкл”. 2. Change mode in MAVProxy. 3. Press F2. 4. Unmute and change mode again |
| Expected | While muted: no speech, the log shows “✕ отброшено…” or “озвучка выключена”; after unmuting speech resumes |
| Pass criteria | Telemetry reception is unaffected while muted (message rate does not drop) |

### T-10. TTS queue does not block reception

| | |
|---|---|
| Goal | Long synthesis does not disturb telemetry |
| Steps | 1. Provoke an “event storm”: several rapid mode changes + PreArm texts (arm without EKF) + status via F2 in a row. 2. Watch the message-rate field during speech |
| Expected | The rate does not sag while the app is speaking (speech runs in its own thread); the queue is bounded (`queue_limit = 16`, excess phrases are dropped and logged) |
| Pass criteria | No window freezes; phrase delay is acceptable, telemetry loss is not |

### T-11. Anti-spam interval comes from the config

| | |
|---|---|
| Goal | The N-second interval is configurable |
| Steps | 1. Set `mode_change_sec = 30` in `config/gcs-tts.ini`. 2. Restart the application. 3. `mode GUIDED`, after 15 s `mode LOITER` (both spoken), after another 10 s `mode GUIDED` |
| Expected | The last change is suppressed (“подавлено антиспамом (30 с)”) because 25 s < 30 s elapsed since the last spoken mode |
| Pass criteria | Restore the config afterwards (10) |

## 5. Sound-less mode (WAV)

When the bench has no audio output, run the application with the
`GCS_TTS_WAV_DIR` environment variable — each phrase is additionally
written to a WAV file:

```bash
mkdir -p /tmp/gcs-wav
GCS_TTS_WAV_DIR=/tmp/gcs-wav ~/build/mav-voice-gcs/gcs-tts --config config/gcs-tts.ini
# verify: ls /tmp/gcs-wav  (one wav per phrase), play back:
aplay /tmp/gcs-wav/*.wav
```

A test passes in this mode when the phrase is present in the window log
AND the corresponding WAV file exists.

## 6. Test protocol

| ID | Name | Result (PASS/FAIL) | Notes |
|---|---|---|---|
| T-01 | Telemetry reception | | |
| T-02 | Status hotkey (ground) | | |
| T-03 | Flight mode change | | |
| T-04 | Anti-spam repeat | | |
| T-05 | Arm / disarm | | |
| T-06 | Battery thresholds | | |
| T-07 | STATUSTEXT verbatim | | |
| T-08 | Altitude/speed (flight) | | |
| T-09 | Mute | | |
| T-10 | TTS does not block reception | | |
| T-11 | Anti-spam config | | |

Bench: OS ____, Qt ____, ArduPilot ____, date ____, tester ____.

The session is successful when all tests are PASS (for T-06, tuning
`BATT_CAPACITY` to the bench speed is acceptable — note it in the notes).

## 7. Known limitations

- State of charge in SITL requires `BATT_MONITOR=5` (T-06 step 2); a real
  vehicle with a proper monitor reports it without extra setup.
- The F2 hotkey works while the application window is focused (the spec
  asks for a minimal window; global X11 hotkeys are out of scope).
- If several copies of the application run simultaneously, port 14550 is
  shared but only the first responder acts as the GCS peer.
