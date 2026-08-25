#pragma once

#include <QString>

namespace gcs {

struct AppConfig {
    // [udp]
    QString udpHost = QStringLiteral("127.0.0.1");
    quint16 udpPort = 14550;
    quint8 sysid = 255;
    quint8 compid = 190;

    // [battery]
    int batteryWarnPercent = 25;
    int batteryCriticalPercent = 15;
    int batteryRecoverMarginPercent = 5;

    // [antispam]
    int antispamDefaultSec = 8;
    int antispamModeSec = 10;
    int antispamArmSec = 5;
    int antispamBatterySec = 30;
    int antispamStatustextSec = 15;
    int antispamStatusHotkeySec = 2;

    // [tts]
    bool ttsEnabled = true;
    QString ttsProgram = QStringLiteral("espeak-ng");
    QString ttsVoice = QStringLiteral("ru");
    int ttsSpeed = 150;
    int ttsQueueLimit = 16;

    // [hotkey]
    QString statusHotkey = QStringLiteral("F2");

    // Читает ini-файл; отсутствующие ключи остаются со значениями по умолчанию.
    static AppConfig load(const QString &iniPath);
};

} // namespace gcs
