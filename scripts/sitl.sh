#!/usr/bin/env bash
# Запуск ArduPilot SITL (ArduCopter) со стримом MAVLink на UDP 127.0.0.1:14550.
# Требуется ~/ardupilot (см. README). Дополнительные аргументы пробрасываются
# в sim_vehicle.py, например: ./scripts/sitl.sh -f quad
set -euo pipefail

ARDUPILOT_DIR="${ARDUPILOT_DIR:-$HOME/ardupilot}"
cd "$ARDUPILOT_DIR"

# В свежих версиях ArduPilot скрипт лежит в Tools/autotest (корневой обёртки нет).
SIM_VEHICLE="./sim_vehicle.py"
[ -f "$SIM_VEHICLE" ] || SIM_VEHICLE="./Tools/autotest/sim_vehicle.py"

exec "$SIM_VEHICLE" -v Copter --map --console --out=udp:127.0.0.1:14550 "$@"
