#include "config/AppConfig.h"

#include <QSettings>

namespace gcs {

namespace {
int clamped(const QSettings &s, const char *key, int def, int lo, int hi)
{
    int v = s.value(QLatin1String(key), def).toInt();
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}
} // namespace

AppConfig AppConfig::load(const QString &iniPath)
{
    AppConfig cfg;
    QSettings s(iniPath, QSettings::IniFormat);

    cfg.udpHost = s.value("udp/host", cfg.udpHost).toString();
    const quint64 port = s.value("udp/port", cfg.udpPort).toULongLong();
    cfg.udpPort = (port > 0 && port <= 65535) ? quint16(port) : cfg.udpPort;
    cfg.sysid = quint8(clamped(s, "udp/sysid", cfg.sysid, 1, 255));
    cfg.compid = quint8(clamped(s, "udp/compid", cfg.compid, 1, 255));

    cfg.batteryWarnPercent = clamped(s, "battery/warn_percent", cfg.batteryWarnPercent, 2, 99);
    cfg.batteryCriticalPercent =
        clamped(s, "battery/critical_percent", cfg.batteryCriticalPercent, 1, cfg.batteryWarnPercent - 1);
    cfg.batteryRecoverMarginPercent =
        clamped(s, "battery/recover_margin_percent", cfg.batteryRecoverMarginPercent, 0, 50);

    cfg.antispamDefaultSec = clamped(s, "antispam/default_sec", cfg.antispamDefaultSec, 0, 3600);
    cfg.antispamModeSec = clamped(s, "antispam/mode_change_sec", cfg.antispamModeSec, 0, 3600);
    cfg.antispamArmSec = clamped(s, "antispam/arm_sec", cfg.antispamArmSec, 0, 3600);
    cfg.antispamBatterySec = clamped(s, "antispam/battery_sec", cfg.antispamBatterySec, 0, 3600);
    cfg.antispamStatustextSec = clamped(s, "antispam/statustext_sec", cfg.antispamStatustextSec, 0, 3600);
    cfg.antispamStatusHotkeySec = clamped(s, "antispam/status_hotkey_sec", cfg.antispamStatusHotkeySec, 0, 3600);

    cfg.ttsEnabled = s.value("tts/enabled", cfg.ttsEnabled).toBool();
    cfg.ttsProgram = s.value("tts/program", cfg.ttsProgram).toString();
    cfg.ttsVoice = s.value("tts/voice", cfg.ttsVoice).toString();
    cfg.ttsSpeed = clamped(s, "tts/speed", cfg.ttsSpeed, 80, 400);
    cfg.ttsQueueLimit = clamped(s, "tts/queue_limit", cfg.ttsQueueLimit, 2, 256);

    cfg.statusHotkey = s.value("hotkey/status_key", cfg.statusHotkey).toString();

    return cfg;
}

} // namespace gcs
