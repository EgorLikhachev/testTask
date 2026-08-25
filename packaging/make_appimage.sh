#!/usr/bin/env bash
# Сборка AppImage через linuxdeploy (+ qt-плагин).
# Аргументы: <путь Qt> <каталог сборки с gcs-tts>
# Пример: packaging/make_appimage.sh "$HOME/Qt/6.5.3/gcc_64" build
set -euo pipefail
cd "$(dirname "$0")/.."

QT_DIR="${1:?укажите путь Qt, например ~/Qt/6.5.3/gcc_64}"
BUILD_DIR="${2:-build}"
APPDIR=AppDir

# На CI нет FUSE — AppImage-инструменты запускаем через извлечение.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="$QT_DIR/bin/qmake"
export PATH="$QT_DIR/bin:$PATH"

echo "== linuxdeploy"
for tool in linuxdeploy-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage; do
    if [ ! -x "$tool" ]; then
        case "$tool" in
            linuxdeploy-x86_64.AppImage)
                url=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/$tool ;;
            *)
                url=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/$tool ;;
        esac
        curl -fL --retry 5 -o "$tool" "$url"
        chmod +x "$tool"
    fi
done

echo "== AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/share/mav-voice-gcs/translations"
cp "$BUILD_DIR/gcs-tts" "$APPDIR/usr/bin/" 2>/dev/null \
    || { mkdir -p "$APPDIR/usr/bin"; cp "$BUILD_DIR/gcs-tts" "$APPDIR/usr/bin/"; }
cp config/gcs-tts.ini "$APP_DIR/usr/share/mav-voice-gcs/"
if compgen -G "$BUILD_DIR/translations/*.qm" > /dev/null; then
    cp "$BUILD_DIR"/translations/*.qm "$APPDIR/usr/share/mav-voice-gcs/translations/"
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
