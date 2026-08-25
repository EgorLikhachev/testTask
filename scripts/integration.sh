#!/usr/bin/env bash
# Интеграционная проверка mav-voice-gcs против ArduPilot SITL.
# Запуск в WSL: bash scripts/integration.sh
set -u
cd "$(dirname "$0")/.."

APP_BIN=$HOME/build/mav-voice-gcs/gcs-tts
CFG=$(pwd)/config/gcs-tts-integration.ini
WAVDIR=/tmp/gcs-wav
APPLOG=/tmp/gcs-app.log
SITLLOG=/tmp/sitl.log

pkill -f arducopter 2>/dev/null
pkill -f gcs-tts 2>/dev/null
sleep 1

rm -rf "$WAVDIR" "$APPLOG" "$SITLLOG" /tmp/gcs-sitl-root
mkdir -p "$WAVDIR"

echo "== [1/4] приложение (headless, озвучка в WAV)"
export QT_QPA_PLATFORM=offscreen
export GCS_TTS_WAV_DIR=$WAVDIR
"$APP_BIN" --config "$CFG" > "$APPLOG" 2>&1 &
APP_PID=$!
sleep 2

echo "== [2/4] SITL (arducopter, чистый eeprom)"
mkdir -p /tmp/gcs-sitl-root && cd /tmp/gcs-sitl-root
cp "$HOME/ardupilot/Tools/autotest/default_params/copter.parm" . 2>/dev/null || true
"$HOME/ardupilot/build/sitl/bin/arducopter" --model quad --speedup 1 -I0 > "$SITLLOG" 2>&1 &
SITL_PID=$!
cd - >/dev/null
sleep 6

echo "== [3/4] мост + сценарий"
python3 scripts/sitl_bridge.py --max-sec 150
BRIDGE_EXIT=$?

sleep 3
kill $SITL_PID $APP_PID 2>/dev/null
sleep 1
cp "$APPLOG" ~/gcs-app-last.log 2>/dev/null || true
cp "$SITLLOG" ~/gcs-sitl-last.log 2>/dev/null || true

echo "== [4/4] результаты"
python3 - <<'PYEOF'
import glob, sys
log = open('/tmp/gcs-app.log', encoding='utf-8', errors='replace').read()
wavs = glob.glob('/tmp/gcs-wav/*.wav')

checks = [
    ("приём телеметрии (запрос стрима)", "запрошен стрим"),
    ("смена режима GUIDED",               "Режим полёта: наведение"),
    ("смена режима LOITER",               "Режим полёта: лойтер"),
    ("arm (моторы запущены)",             "Моторы запущены"),
    ("disarm (моторы остановлены)",       "Моторы остановлены"),
    ("батарея: предупреждение",           "Заряд батареи низкий"),
    ("батарея: критический",              "Критический заряд батареи"),
    ("STATUSTEXT WARNING+ дословно",      "Критическое событие"),
    ("антиспам (подавление)",             "подавлено антиспамом"),
    ("потоки: без предупреждений",        None),
]
ok = True
for name, needle in checks:
    if needle is None:  # негативная проверка: не должно быть гонок потоков
        hit = "Timers cannot be started from another thread" not in log
        name = "нет ошибок аффинности потоков"
    else:
        hit = needle.lower() in log.lower()
    print(("PASS  " if hit else "FAIL  ") + name)
    ok = ok and hit

waved = len(wavs) > 0
print(("PASS  " if waved else "FAIL  ") + f"очередь TTS дошла до синтеза ({len(wavs)} WAV)")
ok = ok and waved
print("РЕЗУЛЬТАТ: " + ("ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ" if ok else "ЕСТЬ ПРОПУЩЕННЫЕ ПРОВЕРКИ"))
sys.exit(0 if ok else 1)
PYEOF
