# mav-voice-gcs

[![CI](https://github.com/EgorLikhachev/testTask/actions/workflows/ci.yml/badge.svg)](https://github.com/EgorLikhachev/testTask/actions/workflows/ci.yml)
[![Docs](https://github.com/EgorLikhachev/testTask/actions/workflows/docs.yml/badge.svg)](https://github.com/EgorLikhachev/testTask/actions/workflows/docs.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Release](https://img.shields.io/badge/release-v0.2.0-blue.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-linux-lightgrey.svg)](#prerequisites)

**mav-voice-gcs** is a lightweight ground-control station (GCS) for ArduPilot
vehicles that receives MAVLink telemetry over UDP and speaks out key flight
events in Russian with a synthesized voice — like a minimal, voice-first
cousin of QGroundControl or Mission Planner. Voice announcements cover flight
mode changes, arming, battery warnings, onboard STATUSTEXT messages and a
status speech on a hotkey.

> 🇷🇺 Этот документ на русском: [README.ru.md](README.ru.md)

## Table of contents

- [Features](#features)
- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Testing](#testing)
- [Project structure](#project-structure)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgements](#acknowledgements)

## Features

- **MAVLink telemetry receiver** — UDP transport, parsing with the official
  `mavlink/c_library_v2` headers (MAVLink v1 and v2 frames).
- **Voice announcements (Russian)** via espeak-ng:
  - flight mode change;
  - arm / disarm;
  - battery below warning and critical thresholds (with hysteresis);
  - incoming STATUSTEXT of severity WARNING or worse — verbatim;
  - link established / lost / regained;
  - status speech on a hotkey (altitude, ground speed, battery charge).
- **Anti-spam** — each event type is spoken at most once per N seconds
  (intervals live in the INI config).
- **Non-blocking speech** — the TTS queue runs in its own thread; telemetry
  reception never waits for speech to finish.
- **Single-vehicle filtering** by MAVLink system id (auto-lock or explicit).
- **Session logging** to `.tlog` (raw frames with timestamps).
- **English UI** translation, selected automatically by system locale.
- **Qt Widgets UI**: live telemetry, status button, mute toggle, event log.

## Prerequisites

| Requirement | Version | Notes |
|---|---|---|
| Ubuntu Linux | 22.04 or 24.04 | native or WSL2; other distros may work |
| CMake | ≥ 3.20 | |
| C++ compiler | C++17 (GCC ≥ 11) | `build-essential` is enough |
| Qt | ≥ 6.5 | Ubuntu repos only have 6.2/6.4 — install via aqtinstall below |
| espeak-ng | any recent | speech synthesizer |
| Python 3 | ≥ 3.10 | only for the synthetic integration test (`pymavlink`) |
| ArduPilot SITL | optional | needed only for full integration testing |

## Installation

### 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/EgorLikhachev/testTask.git
cd testTask
```

`--recurse-submodules` fetches the MAVLink C headers (`extern/c_library_v2`,
~10 MB). Without them the build fails on
`#include <ardupilotmega/mavlink.h>`.

### 2. Install system packages

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git python3 python3-pip \
    espeak-ng libgl1-mesa-dev libgl-dev libopengl-dev libegl-dev libglx-dev \
    libxkbcommon-x11-0 libxcb-xinerama0 libxcb-cursor0 libfontconfig1 libdbus-1-3 \
    libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0 \
    libxcb-randr0 libxcb-xfixes0 p7zip-full
```

Quick voice check (in WSL2 the sound plays on the Windows host via WSLg):

```bash
espeak-ng -v ru "Проверка голосового модуля"
```

### 3. Install Qt 6.5+

Ubuntu repositories ship Qt 6.2/6.4, which is below the required 6.5.
Use [aqtinstall](https://aqtinstall.readthedocs.io/):

```bash
python3 -m pip install --user --break-system-packages aqtinstall
~/.local/bin/aqt install-qt linux desktop 6.5.3 gcc_64 -O ~/Qt
```

If `download.qt.io` is unreachable from your network (aqt fails downloading
checksums), fetch two archives from any mirror, e.g.
`mirrors.ocf.berkeley.edu/qt/online/qtsdkrepository/linux_x64/desktop/qt6_653/qt.qt6.653.gcc_64/`:

```bash
S=6.5.3-0-202309260341
curl -fLO "${S}qtbase-Linux-RHEL_8_4-GCC-Linux-RHEL_8_4-X86_64.7z"
curl -fLO "${S}icu-linux-Rhel7.2-x64.7z"
curl -fLO "${S}qttools-Linux-RHEL_8_4-GCC-Linux-RHEL_8_4-X86_64.7z"
sudo mkdir -p /opt/qt && sudo 7z x -y -o/opt/qt ./*.7z && sudo chmod -R a+rX /opt/qt
```

### 4. Build and run unit tests

```bash
./scripts/build.sh
```

The script auto-detects Qt (`~/Qt/6.5.3/gcc_64`, `/opt/qt/6.5.3/gcc_64`),
builds with Ninja and runs `ctest`. Expected result:

```text
100% tests passed, 0 tests failed out of 3
==> Бинарник: ~/build/mav-voice-gcs/gcs-tts   (in WSL)
            build/gcs-tts                     (native)
```

> Under WSL2 the build directory is placed in the Linux home (`~/build/...`)
> automatically: CMake cannot resolve system libraries on `/mnt/*` mounts.

## Configuration

The application reads an INI file; every key is optional and falls back to a
built-in default. Full reference with all keys, defaults and units:
[docs/CONFIGURATION.md](docs/CONFIGURATION.md).

Config lookup order (first existing wins):

1. path passed with `--config <path>`;
2. `<binary dir>/gcs-tts.ini`;
3. `<binary dir>/../config/gcs-tts.ini`;
4. `<binary dir>/../../config/gcs-tts.ini`;
5. `<binary dir>/../share/mav-voice-gcs/gcs-tts.ini` (AppImage).

Most-used sections of `config/gcs-tts.ini`:

| Section | Purpose |
|---|---|
| `[udp]` | listen port, GCS sysid/compid, target `vehicle_sysid` (0 = auto) |
| `[battery]` | warning / critical thresholds, hysteresis margin |
| `[link]` | silence window that triggers "link lost" |
| `[antispam]` | per-event minimum repeat intervals (seconds) |
| `[tts]` | espeak-ng program, voice, speed, queue limit, WAV retention |
| `[log]` | `.tlog` session recording on/off and directory |
| `[hotkey]` | status speech hotkey (default `F2`) |

Environment variables:

| Variable | Default | Description |
|---|---|---|
| `GCS_TTS_WAV_DIR` | unset | when set, every phrase is additionally written as a WAV file into this directory (useful where audio output is unavailable); only the last `wav_keep` files are kept |
| `QT_QPA_PLATFORM` | unset | Qt platform plugin; set `offscreen` for headless runs |

## Usage

### Start the simulator (optional, for testing)

See [Setting up ArduPilot SITL](docs/MANUAL_TESTING.md#2-подготовка-стенда-однократно)
or the quick version:

```bash
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git ~/ardupilot
cd ~/ardupilot && Tools/environment_install/install-prereqs-ubuntu.sh -y
./waf configure --board sitl && ./waf copter
./build/sitl/bin/arducopter --model quad -I0          # TCP 5760
mavproxy.py --master tcp:127.0.0.1:5760 --out=udp:127.0.0.1:14550
```

### Start the application

```bash
~/build/mav-voice-gcs/gcs-tts --config config/gcs-tts.ini
```

The window shows mode, ARM state, battery, altitude, speed, link status and
message rate. Press `F2` (or the **Статус** button) for the status speech,
toggle **Озвучка** to mute. Announced phrases are printed to the console log
and to the in-window event log.

Headless run with phrases captured as WAV files:

```bash
mkdir -p /tmp/gcs-wav
GCS_TTS_WAV_DIR=/tmp/gcs-wav QT_QPA_PLATFORM=offscreen \
    ~/build/mav-voice-gcs/gcs-tts --config config/gcs-tts.ini
ls /tmp/gcs-wav    # one WAV per spoken phrase
```

### Prebuilt AppImage

CI builds a self-contained AppImage (see [Releases](https://github.com/EgorLikhachev/testTask/releases)).
On the target machine only `espeak-ng` is required:

```bash
sudo apt-get install -y espeak-ng libxcb-xinerama0
./mav-voice-gcs-x86_64.AppImage
```

Build one locally: `packaging/make_appimage.sh <qt-dir> <build-dir>`.

## Testing

| Level | Command | What it covers |
|---|---|---|
| Unit tests | `ctest --test-dir build --output-on-failure` | parser (junk/split frames, chunked STATUSTEXT, sysid filter, tlog round-trip), event detector, anti-spam, link monitor, TTS queue |
| Synthetic integration | `./scripts/synthetic_test.sh` | full pipeline against generated MAVLink frames — no ArduPilot needed |
| Full SITL integration | `./scripts/integration.sh` | application + ArduCopter SITL + scripted flight events, 11 checks |
| Manual acceptance | [docs/MANUAL_TESTING.md](docs/MANUAL_TESTING.md) | step-by-step manual test methodology |
| Documentation checks | CI workflow `Docs` | markdownlint + link checking |

## Project structure

```text
src/
  config/      AppConfig — INI file to typed configuration
  transport/   UdpTransport — QUdpSocket, bytes in/out only
  mavlink/     MavlinkParser (c_library_v2, sysid filter), CopterModes, MavlinkCommands
  domain/      VehicleState, EventDetector, AntiSpamFilter, LinkMonitor
  telemetry/   TlogWriter — session recording to .tlog
  announce/    Announcer — events to Russian phrases + anti-spam
  tts/         ITtsBackend, EspeakBackend (QProcess), TtsQueue (worker thread)
  ui/          MainWindow — telemetry, status button, mute, event log
  app/         Application — wiring of all layers
tests/         tst_parser, tst_domain, tst_tts
scripts/       build, setup, SITL runner, synthetic & full integration tests
packaging/     desktop file, icon, AppImage script
translations/  English UI translation (Qt .ts/.qm)
config/        gcs-tts.ini (runtime), gcs-tts-integration.ini (testing)
docs/          architecture, configuration, testing methodology
extern/        mavlink c_library_v2 (git submodule)
```

Layer contract and data flow are described in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — layers, threading, event pipeline
- [docs/CONFIGURATION.md](docs/CONFIGURATION.md) — every configuration key explained
- [docs/MANUAL_TESTING.md](docs/MANUAL_TESTING.md) — manual acceptance testing methodology
- [CHANGELOG.md](CHANGELOG.md) — release history
- [README.ru.md](README.ru.md) — README in Russian

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Pull requests are checked by CI
(build, unit tests, synthetic integration test, documentation lint).

## License

Distributed under the [MIT License](LICENSE).

## Acknowledgements

- [ArduPilot](https://ardupilot.org/) — SITL simulator and flight-mode semantics
- [MAVLink](https://mavlink.io/) / [c_library_v2](https://github.com/mavlink/c_library_v2) — protocol and C headers
- [Qt](https://www.qt.io/) — application and UI framework
- [espeak-ng](https://github.com/espeak-ng/espeak-ng) — speech synthesis
- [aqtinstall](https://aqtinstall.readthedocs.io/) — Qt installation tooling
