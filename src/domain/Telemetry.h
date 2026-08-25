#pragma once

#include <QMetaType>
#include <QString>
#include <QtGlobal>

namespace gcs {

// Чистые DTO телеметрии: доменный слой не зависит от mavlink-заголовков,
// декодирование сообщений выполняет слой парсера (src/mavlink).

struct HeartbeatInfo {
    quint32 customMode = 0;
    quint8 baseMode = 0;
    quint8 systemStatus = 0;
    quint8 autopilot = 0;
    quint8 mavType = 0;
    quint8 sysid = 0;
    quint8 compid = 0;
    bool armed = false;
    QString modeName; // человекочитаемое имя режима (заполняет парсер)
};

struct BatteryInfo {
    float voltageV = 0.0f;
    float currentA = 0.0f;
    int remainingPercent = -1; // -1 — неизвестно
    bool valid = false;
};

struct PositionInfo {
    double latDeg = 0.0;
    double lonDeg = 0.0;
    float relAltM = 0.0f;
    float absAltM = 0.0f;
    float velXMps = 0.0f;
    float velYMps = 0.0f;
    float velZMps = 0.0f;
    bool valid = false;
};

struct VfrHudInfo {
    float airspeedMps = 0.0f;
    float groundspeedMps = 0.0f;
    float altM = 0.0f;
    float climbMps = 0.0f;
    qint16 headingDeg = 0;
    bool valid = false;
};

// Порядок значений повторяет MAV_SEVERITY (0 — самый тяжёлый).
enum class StatusSeverity {
    Emergency = 0,
    Alert = 1,
    Critical = 2,
    Error = 3,
    Warning = 4,
    Notice = 5,
    Info = 6,
    Debug = 7,
};

struct StatusTextInfo {
    StatusSeverity severity = StatusSeverity::Notice;
    QString text;
};

} // namespace gcs

Q_DECLARE_METATYPE(gcs::HeartbeatInfo)
Q_DECLARE_METATYPE(gcs::BatteryInfo)
Q_DECLARE_METATYPE(gcs::PositionInfo)
Q_DECLARE_METATYPE(gcs::VfrHudInfo)
Q_DECLARE_METATYPE(gcs::StatusTextInfo)
