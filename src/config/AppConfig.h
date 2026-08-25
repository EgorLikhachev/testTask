#pragma once

#include <QString>

namespace gcs {

struct AppConfig {
    // [udp]
    QString udpHost = QStringLiteral("127.0.0.1");
    quint16 udpPort = 14550;
    quint8 sysid = 255;
    quint8 compid = 190;
    // 0 — авто-захват первого валидного борта; 1..255 — слушать только его.
    quint8 vehicleSysid = 0;

    // [battery]
    int batteryWarnPercent = 25;
    int batteryCriticalPercent = 15;
    int batteryRecoverMarginPercent = 5;

    // [link]
    // Нет сообщений дольше loss_sec -> потеря связи (и фраза).
    int linkLossSec = 4;

    // [antispam]
    int antispamDefaultSec = 8;
    int antispamModeSec = 10;
    int antispamArmSec = 5;
    int antispamBatterySec = 30;
    int antispamStatustextSec = 15;
    int antispamStatusHotkeySec = 2;
    int antispamLinkSec = 10;

    // [tts]
    bool ttsEnabled = true;
    // espeak | piper (см. ITtsBackend-реализации)
    QString ttsBackend = QStringLiteral("espeak");
    QString ttsProgram = QStringLiteral("espeak-ng");
    QString ttsVoice = QStringLiteral("ru");
    int ttsSpeed = 150;
    int ttsQueueLimit = 16;
    // В режиме GCS_TTS_WAV_DIR хранить не более N последних WAV-файлов.
    int ttsWavKeep = 64;
    // piper: путь к бинарю, ONNX-модели, команде воспроизведения и темп речи.
    QString piperBin = QStringLiteral("piper");
    QString piperModel;
    QString piperPlayCmd = QStringLiteral("paplay");
    double piperLengthScale = 1.0;

    // [log]
    bool logEnabled = true;
    QString logDir = QStringLiteral("logs");

    // [hotkey]
    QString statusHotkey = QStringLiteral("F2");
    // Глобальный хоткей (X11/XWayland). При неудаче — тихий откат
    // к хоткею окна.
    bool hotkeyGlobal = true;

    // [ui]
    // Закрытие окна сворачивает в трей (если трей доступен), а не выходит.
    bool uiHideOnClose = true;

    // Читает ini-файл; отсутствующие ключи остаются со значениями по умолчанию.
    static AppConfig load(const QString &iniPath);
};

} // namespace gcs
