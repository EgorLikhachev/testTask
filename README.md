# mav-voice-gcs

Приёмник MAVLink-телеметрии с голосовым сопровождением ключевых событий —
компактный аналог наземной станции (в духе QGroundControl / Mission Planner),
заточенный под озвучку.

Репозиторий: https://github.com/EgorLikhachev/testTask
Методика ручных испытаний: [docs/MANUAL_TESTING.md](docs/MANUAL_TESTING.md)

## Клонирование и «лёгкая» передача

Репозиторий содержит только исходный код, скрипты и документацию (сотни КБ).
Тяжёлые компоненты в него не входят и ставятся отдельно по инструкции ниже:

| Компонент | Размер | Как попадает на станцию |
|---|---|---|
| Код проекта + тесты + скрипты | ~100 КБ | сам репозиторий |
| MAVLink C-заголовки `c_library_v2` | ~10 МБ | git-сабмодуль (`--recurse-submodules`) |
| Qt 6.5.3 | ~1,5 ГБ | `scripts/setup_wsl.sh` (aqtinstall) или зеркало |
| ArduPilot SITL | 3–5 ГБ | отдельный клон (в README, раздел «Симулятор») |
| espeak-ng | ~1 МБ | apt |

```bash
git clone --recurse-submodules https://github.com/EgorLikhachev/testTask.git
```

Полностью автономный проект: никаких зависимостей от других репозиториев
пользователя. Связь только с внешними открытыми компонентами:

- **ArduPilot SITL (ArduCopter)** — симулятор, источник телеметрии;
- **mavlink/c_library_v2** — официальные сгенерированные C-заголовки MAVLink 2
  (подключена git-сабмодулем в `extern/c_library_v2`);
- **espeak-ng** — синтезатор речи (внешний процесс);
- **Qt 6.5+** — событийный цикл, UDP, GUI.

## Архитектура

Слои строго разделены по каталогам, поток данных слева направо:

```
транспорт          парсер MAVLink          доменная модель           TTS
UdpTransport  ->   MavlinkParser      ->   VehicleState /        ->  Announcer -> TtsQueue -> EspeakBackend
(QUdpSocket)       (c_library_v2,           EventDetector /           (антиспам,   (QThread,   (QProcess
                    + CopterModes,           AntiSpamFilter             фразы ru)    очередь)    espeak-ng)
                    + MavlinkCommands)
```

| Слой | Файлы | Ответственность |
|---|---|---|
| Транспорт | `src/transport/UdpTransport.*` | UDP-сокет: приём/отправка байтов, адрес пира |
| Парсер | `src/mavlink/*` | `mavlink_parse_char`, декодирование, имена режимов, сборка исходящих сообщений |
| Домен | `src/domain/*` | `VehicleState` (снимок телеметрии), `EventDetector` (события), `AntiSpamFilter` |
| Озвучка | `src/announce/Announcer.*` | события → русские фразы + антиспам |
| TTS | `src/tts/*` | очередь в отдельном потоке, бэкенд espeak-ng |
| UI | `src/ui/MainWindow.*` | телеметрия, кнопка статуса, мьют, лог |
| Связка | `src/app/Application.*` | соединение всех слоёв |

Ключевое свойство потоков: приём и парсинг телеметрии идут в главном потоке
(событийный цикл, лёгкие операции), а говорение — в отдельном `QThread`
с очередью ограниченного размера. Долгий синтез никогда не блокирует приём.

## Озвучиваемые события

- смена режима полёта (HEARTBEAT → custom_mode, таблица ArduCopter);
- arm / disarm (HEARTBEAT → `MAV_MODE_FLAG_SAFETY_ARMED`);
- падение заряда ниже порогов warning / critical (SYS_STATUS / BATTERY_STATUS),
  с гистерезисом, чтобы предупреждающий порог не «мигал» на границе;
- входящие STATUSTEXT уровня WARNING и тяжелее — дословно;
- статус по горячей клавише (по умолчанию F2): высота, скорость, заряд.

Антиспам: каждое событие озвучивается не чаще раза в N секунд; интервалы —
в `config/gcs-tts.ini`. Для STATUSTEXT ключ антиспама строится по тексту
сообщения, разные сообщения друг друга не блокируют.

## Сборка (Ubuntu 22.04/24.04, в т.ч. WSL2)

Требования: CMake 3.20+, gcc/g++ с C++17, Qt 6.5+ (в репозиториях Ubuntu
максимум 6.4, поэтому Qt ставится через aqtinstall), espeak-ng.

```bash
# 1. один раз — окружение (пакеты + Qt 6.5.3)
./scripts/setup_wsl.sh

# 2. сборка + юнит-тесты
./scripts/build.sh
```

Запуск:

```bash
./build/gcs-tts --config config/gcs-tts.ini
```

В WSL скрипт сборки кладёт каталог сборки в `~/build/mav-voice-gcs`
(на `/mnt/*` CMake неверно определяет архитектуру библиотек — см. раздел
«Грабли» ниже). Озвучку без аудио можно проверить записью в WAV:

```bash
GCS_TTS_WAV_DIR=/tmp/gcs-wav ./build/gcs-tts   # фразы -> WAV-файлы
```

## Симулятор (SITL)

```bash
# один раз: клонирование и зависимости ArduPilot
sudo apt install -y git python3 python3-pip
git clone --recurse-submodules https://github.com/ArduPilot/ardupilot.git ~/ardupilot
cd ~/ardupilot
Tools/environment_install/install-prereqs-ubuntu.sh -y

# каждый запуск: ArduCopter, MAVLink на udp:127.0.0.1:14550
./scripts/sitl.sh          # из каталога проекта
# (скрипт сам найдёт sim_vehicle.py — в свежем ArduPilot он в Tools/autotest)
```

Управление бортом для проверки сценариев — в консоли MAVProxy:
`mode LOITER`, `arm throttle`, `disarm`, `param set ...`.

## Проверка сценариев ТЗ (интеграционная)

| Пункт ТЗ | Как проверить |
|---|---|
| Смена режима | MAVProxy: `mode GUIDED` → фраза «Режим полёта: наведение» |
| Arm / disarm | MAVProxy: `arm throttle` → «Внимание! Моторы запущены»; `disarm` → «Моторы остановлены» |
| Батарея warning/critical | поднять пороги: `param set BATT_LOW 99` → фраза о низком заряде; вернуть обратно |
| STATUSTEXT WARNING+ | ArduPilot сам шлёт их (например, при разряде); MAVProxy: `param set SIM_BATT_VOLTAGE 10.5` |
| Статус по хоткею | F2 в окне приложения → высота/скорость/заряд |
| Антиспам N секунд | дважды сменить режим туда-обратно быстрее `mode_change_sec` — вторая фраза подавится (видно в логе) |
| TTS не блокирует приём | поколачивать MAVProxy statustext'ами и следить, что «сообщ/с» в окне не проседает |

Проверка TTS без аудио (WSL): `./scripts/smoke_tts.sh` или запуск приложения
с `GCS_TTS_WAV_DIR=/tmp/gcs-wav` — фразы пишутся в WAV-файлы.

## Интеграционная проверка (автоматическая)

`scripts/integration.sh` поднимает всё окружение целиком: приложение
(headless, озвучка в WAV), SITL `arducopter` и python-мост
`scripts/sitl_bridge.py` (TCP 5760 ↔ UDP 14550), который по расписанию
выполняет сценарий и автоматически проверяет в логе приложения фразы для
всех пунктов ТЗ: смена режимов, arm/disarm, пороги батареи, STATUSTEXT,
антиспам, работоспособность очереди TTS. Последний прогон: 11/11 PASS.

Оговорка: процент заряда (SYS_STATUS.battery_remaining) в сценарии
инжектируется мостом синтетически — SITL с дефолтным `BATT_MONITOR=4`
не отдаёт процент; задача проверки — цепочка приложения, а не симулятор
батареи ArduPilot.

## Грабли, на которые наступили (WSL2)

- **Сеть WSL пропала** (хост-интернет есть): файрвол Windows фильтровал
  vEthernet (WSL) профилем Public. Лечится разрешающим правилом:
  `New-NetFirewallRule -DisplayName 'WSL allow' -Direction Inbound
  -Action Allow -InterfaceAlias 'vEthernet (WSL)' -Profile Any`.
- **download.qt.io недоступен** (aqt падает на скачивании контрольных
  сумм): Qt ставится вручную с зеркала, например
  `mirrors.ocf.berkeley.edu/qt/online/qtsdkrepository/linux_x64/desktop/
  qt6_653/qt.qt6.653.gcc_64/` — архивы `qtbase` и `icu`, распаковать в
  `/opt/qt` (структура `6.5.3/gcc_64` сохраняется).
- **Сборка в каталоге на `/mnt/c`**: CMake не находит системные библиотеки
  (неверно определяет `CMAKE_LIBRARY_ARCHITECTURE`) — каталог сборки
  должен лежать в ext4 (build.sh делает это сам).
- **Свежий ArduPilot**: `sim_vehicle.py` теперь только в
  `Tools/autotest/` (корневой обёртки нет) — scripts/sitl.sh это учитывает.

## Конфигурация

`config/gcs-tts.ini`; все ключи опциональны. Основные секции: `[udp]`
(порт/адрес), `[battery]` (пороги и гистерезис), `[antispam]` (интервалы),
`[tts]` (программа, голос, скорость, размер очереди), `[hotkey]` (клавиша).

## Тесты

```bash
ctest --test-dir build --output-on-failure   # или ~/build/mav-voice-gcs в WSL
```

`tst_parser` — кадры с мусором и разрывами, склейка чанков STATUSTEXT;
`tst_domain` — детектор событий (гистерезис батареи, arm-фронты), антиспам,
формулировки фраз.

## Состав репозитория

```
src/config      AppConfig: ini → параметры (порт, пороги, антиспам, TTS)
src/transport   UdpTransport: QUdpSocket, только байты
src/mavlink     MavlinkParser (c_library_v2), CopterModes, MavlinkCommands
src/domain      VehicleState, EventDetector, AntiSpamFilter
src/announce    Announcer: события → русские фразы + антиспам
src/tts         ITtsBackend, EspeakBackend (QProcess), TtsQueue (QThread)
src/ui          MainWindow: телеметрия, «Статус (F2)», мьют, лог
src/app         Application: связка слоёв
tests/          tst_parser, tst_domain
scripts/        setup_wsl.sh, build.sh, sitl.sh, integration.sh, sitl_bridge.py
extern/         c_library_v2 (git-сабмодуль)
config/         gcs-tts.ini (рабочий), gcs-tts-integration.ini (тестовый)
```
