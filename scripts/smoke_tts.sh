#!/usr/bin/env bash
# Проверка TTS-бэкенда espeak-ng: сначала с аудио, при неудаче — в WAV-файл
# (актуально в WSL без звука).
set -uo pipefail

TEXT="Проверка голосового модуля"

if espeak-ng -v ru -s 150 "$TEXT" 2>/dev/null; then
    echo "OK: озвучка воспроизведена (аудио)"
else
    OUT=/tmp/gcs_tts_smoke.wav
    espeak-ng -v ru -s 150 -w "$OUT" "$TEXT"
    echo "OK: без аудио, фраза записана в $OUT ($(stat -c%s "$OUT") байт)"
fi
