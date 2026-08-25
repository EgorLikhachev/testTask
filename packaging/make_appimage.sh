#!/usr/bin/env bash
# Сборка AppImage через linuxdeploy (+ qt-плагин).
# Аргументы: <путь Qt> <каталог сборки с gcs-tts>
# Пример: packaging/make_appimage.sh "$HOME/Qt/6.5.3/gcc_64" build
set -euo pipefail
cd "$(dirname "$0")/.."

# Каталог Qt: из аргумента, либо по qmake из PATH (install-qt-action
# всегда добавляет Qt bin в PATH — раскладка dir у action'а менялась).
QT_DIR="${1:-}"
if [ -z "$QT_DIR" ] || [ ! -x "$QT_DIR/bin/qmake" ]; then
    QBIN="$(command -v qmake || command -v qmake6 || true)"
    if [ -n "$QBIN" ]; then
        QT_DIR="$(cd "$(dirname "$QBIN")/.." && pwd)"
        echo "Qt определена через PATH: $QT_DIR"
    fi
fi
if [ -n "$QT_DIR" ] && [ -x "$QT_DIR/bin/qmake" ]; then
    QT_DIR="$(cd "$QT_DIR" && pwd)"
else
    echo "ОШИБКА: qmake не найдена ни в '$QT_DIR', ни в PATH" >&2
    exit 1
fi
BUILD_DIR="${2:-build}"
APPDIR=AppDir

# На CI нет FUSE — AppImage-инструменты запускаем через извлечение.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$QT_DIR/bin/qmake"
export PATH="$QT_DIR/bin:$PATH"

echo "== linuxdeploy"
# Пиннутые стабильные релизы: continuous периодически ломается
# (пример: сборка 2026-08-22 теряла $QMAKE в плагине qt).
# Обновлять: последние теги см. github.com/linuxdeploy/linuxdeploy/releases
LD_TAG=1-alpha-20251107-1
LDQT_TAG=1-alpha-20250213-1
declare -A TOOL_URL=(
    [linuxdeploy-x86_64.AppImage]="https://github.com/linuxdeploy/linuxdeploy/releases/download/${LD_TAG}/linuxdeploy-x86_64.AppImage"
    [linuxdeploy-plugin-qt-x86_64.AppImage]="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LDQT_TAG}/linuxdeploy-plugin-qt-x86_64.AppImage"
)
for tool in "${!TOOL_URL[@]}"; do
    if [ ! -x "$tool" ]; then
        curl -fsSL --retry 5 --connect-timeout 10 -o "$tool" "${TOOL_URL[$tool]}"
        chmod +x "$tool"
    fi
done

# Плагин qt читает qmake из $QMAKE; проверяем заранее.
echo "== qmake: $QMAKE"
ls -la "$QMAKE" || true
"$QMAKE" -v || { echo "ОШИБКА: qmake недоступен по пути \$QMAKE" >&2; exit 1; }

echo "== AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin/translations" "$APPDIR/usr/share/mav-voice-gcs"
cp "$BUILD_DIR/gcs-tts" "$APPDIR/usr/bin/"
# конфиг: usr/bin/../share/mav-voice-gcs — один из кандидатов main.cpp
cp config/gcs-tts.ini "$APPDIR/usr/share/mav-voice-gcs/"
# переводы: usr/bin/translations — куда смотрит QTranslator
if compgen -G "$BUILD_DIR/translations/*.qm" > /dev/null; then
    cp "$BUILD_DIR"/translations/*.qm "$APPDIR/usr/bin/translations/"
fi

echo "== сборка AppImage"
./linuxdeploy-x86_64.AppImage \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/gcs-tts" \
    --desktop-file packaging/mav-voice-gcs.desktop \
    --icon-file packaging/mav-voice-gcs.png \
    --plugin qt \
    --output appimage

ls -la mav-voice-gcs-*.AppImage
echo "==> Готово. Запуск: ./mav-voice-gcs-x86_64.AppImage (espeak-ng должен быть в системе)"
