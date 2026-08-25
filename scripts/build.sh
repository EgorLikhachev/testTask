#!/usr/bin/env bash
# Конфигурирование + сборка + тесты.
# Переменная QT_DIR необязательна: скрипт сам ищет Qt 6.5 в типовых местах.
set -euo pipefail
cd "$(dirname "$0")/.."

if [ -z "${QT_DIR:-}" ]; then
    for c in "$HOME/Qt/6.5.3/gcc_64" \
             "/opt/qt/6.5.3/gcc_64" \
             "/mnt/c/dev/Qt653/6.5.3/gcc_64"; do
        if [ -x "$c/bin/qmake" ]; then
            QT_DIR="$c"
            break
        fi
    done
fi

if [ -z "${QT_DIR:-}" ]; then
    echo "Ошибка: Qt 6.5 не найдена. Укажите QT_DIR или запустите scripts/setup_wsl.sh" >&2
    exit 1
fi
echo "Использую Qt: $QT_DIR"

# /usr/lib/wsl — подставы WSLg, ломающие find_package(OpenGL) внутри WSL2.
EXTRA_ARGS=()
if [ -d /usr/lib/wsl ]; then
    EXTRA_ARGS+=(-DCMAKE_IGNORE_PREFIX_PATH=/usr/lib/wsl)
fi

# Каталог сборки: на /mnt/* (DrvFS в WSL) CMake неверно определяет
# архитектуру библиотек и ломает find_package(OpenGL) — собираем в ext4.
BUILD_DIR="${GCS_BUILD_DIR:-}"
if [ -z "$BUILD_DIR" ]; then
    case "$PWD" in
        /mnt/*) BUILD_DIR="$HOME/build/mav-voice-gcs" ;;
        *)      BUILD_DIR="$PWD/build" ;;
    esac
fi

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    "${EXTRA_ARGS[@]}"
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
echo "==> Бинарник: $BUILD_DIR/gcs-tts"
