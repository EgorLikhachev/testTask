#!/usr/bin/env bash
# Проверка результатов интеграционного прогона (запускать после integration.sh).
set -u
pkill -f mavproxy 2>/dev/null
pkill -f arducopter 2>/dev/null
pkill -f gcs-tts 2>/dev/null
sleep 1

echo "----- лог приложения (ключевые строки) -----"
grep -E 'announce|транспорт|app\]|говорю|ттс|tts' /tmp/gcs-app.log | head -50
echo "----- все строки приложения -----"
cat /tmp/gcs-app.log
echo "----- WAV-файлы -----"
ls -la /tmp/gcs-wav/ | tail -n +2 | head -15

echo "----- ИТОГИ -----"
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
    ("низкая батарея",                    "заряд батареи низкий"),
    ("STATUSTEXT WARNING+ дословно",      "STATUSTEXT"),
    ("антиспам (подавление)",             "подавлено антиспамом"),
]
ok = True
for name, needle in checks:
    hit = needle.lower() in log.lower()
    print(("PASS  " if hit else "FAIL  ") + name)
    ok = ok and hit

waved = len(wavs) > 0
print(("PASS  " if waved else "FAIL  ") + f"очередь TTS дошла до синтеза ({len(wavs)} WAV)")
ok = ok and waved
print("РЕЗУЛЬТАТ: " + ("ВСЕ ПРОВЕРКИ ПРОЙДЕНЫ" if ok else "ЕСТЬ ПРОПУЩЕННЫЕ ПРОВЕРКИ"))
sys.exit(0 if ok else 1)
PYEOF
