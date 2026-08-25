#!/usr/bin/env bash
# Синтетический интеграционный тест: без ArduPilot/SITL.
# Требует: собранное приложение (scripts/build.sh), espeak-ng, python3 + pymavlink.
set -euo pipefail
cd "$(dirname "$0")/.."

APP_BIN="${APP_BIN:-$HOME/build/mav-voice-gcs/gcs-tts}"
if [ ! -x "$APP_BIN" ]; then
    # нативная сборка — бинарник в build/ рядом с исходниками
    APP_BIN="$PWD/build/gcs-tts"
fi

python3 -c 'import pymavlink' 2>/dev/null \
    || python3 -m pip install --user --break-system-packages pymavlink

python3 scripts/synthetic_driver.py \
    --app-bin "$APP_BIN" \
    --config "$PWD/config/gcs-tts-integration.ini" \
    --loss-sec 3
