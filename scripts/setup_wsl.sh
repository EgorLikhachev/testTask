#!/usr/bin/env bash
# Установка окружения сборки mav-voice-gcs в Ubuntu (WSL2 или нативно).
# Запуск: ./scripts/setup_wsl.sh
set -euo pipefail

echo "==> Системные пакеты"
sudo apt-get update -qq
sudo apt-get install -y \
    build-essential cmake ninja-build git python3 python3-pip \
    espeak-ng \
    libgl1-mesa-dev libxkbcommon-x11-0 libxcb-xinerama0 libxcb-cursor0 \
    libfontconfig1 libdbus-1-3 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
    libxcb-render-util0 libxcb-shape0 libxcb-randr0 libxcb-xfixes0 xvfb

echo "==> aqtinstall + Qt 6.5.3 (в репозиториях Ubuntu Qt <= 6.4, а нужно 6.5+)"
python3 -m pip install --user --upgrade aqtinstall 2>/dev/null \
    || python3 -m pip install --break-system-packages --upgrade aqtinstall
AQT="$(command -v aqt || echo "$HOME/.local/bin/aqt")"
"$AQT" install-qt linux desktop 6.5.3 gcc_64 -O "$HOME/Qt"

echo "==> Готово. Qt: $HOME/Qt/6.5.3/gcc_64"
echo "    Сборка: ./scripts/build.sh"
