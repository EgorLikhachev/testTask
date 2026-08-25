#!/usr/bin/env bash
# Установка нейросетевого TTS-бэкенда piper (rhasspy/piper) с русским
# голосом irina. Скрипт идемпотентен. Запуск: scripts/setup_piper.sh
set -euo pipefail

PIPER_TAG=2023.11.14-2
PIPER_URL="https://github.com/rhasspy/piper/releases/download/${PIPER_TAG}/piper_linux_x86_64.tar.gz"
VOICE_BASE="https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/ru/ru_RU/irina/medium"
VOICE=ru_RU-irina-medium

PREFIX="$HOME/.local/opt/piper"
MODELS="$HOME/.local/share/mav-voice-gcs/voices"
BIN_LINK="$HOME/.local/bin/piper"

echo "== piper -> $PREFIX"
mkdir -p "$PREFIX" "$HOME/.local/bin"
if [ ! -x "$PREFIX/piper/piper" ]; then
    TMP=$(mktemp -d)
    curl -fsSL --retry 5 -o "$TMP/piper.tar.gz" "$PIPER_URL"
    tar -xzf "$TMP/piper.tar.gz" -C "$PREFIX" --strip-components=1
    rm -rf "$TMP"
fi
ln -sf "$PREFIX/piper" "$BIN_LINK"

echo "== модель $VOICE -> $MODELS"
mkdir -p "$MODELS"
for ext in onnx onnx.json; do
    f="$MODELS/$VOICE.$ext"
    if [ ! -s "$f" ]; then
        curl -fsSL --retry 5 -o "$f" "$VOICE_BASE/$VOICE.$ext"
    fi
done

echo "== проверка"
"$BIN_LINK" --help >/dev/null && echo "piper: OK"
ls -la "$MODELS/$VOICE.onnx"

cat <<EOF

Готово. Включите бэкенд в config/gcs-tts.ini:

  [tts]
  backend = piper
  piper_model = $MODELS/$VOICE.onnx

Воспроизведение: tts/piper_play (по умолчанию paplay; в Ubuntu:
sudo apt install pulseaudio-utils). Проверка голоса:
  echo 'Проверка голоса' | $BIN_LINK --model $MODELS/$VOICE.onnx --output_file /tmp/t.wav && paplay /tmp/t.wav
EOF
