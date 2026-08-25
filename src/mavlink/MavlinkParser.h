#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "domain/Telemetry.h"

namespace gcs {

// Слой парсинга MAVLink: единственное место, зависящее от c_library_v2.
// Принимает сырые байты, отдаёт декодированные DTO доменному слою.
class MavlinkParser : public QObject
{
    Q_OBJECT
public:
    explicit MavlinkParser(QObject *parent = nullptr);
    ~MavlinkParser() override;

    void feed(const QByteArray &bytes);

    quint64 totalMessages() const { return m_totalMessages; }
    quint64 parseErrors() const { return m_parseErrors; }

signals:
    void heartbeatReceived(gcs::HeartbeatInfo info);
    void batteryReceived(gcs::BatteryInfo info);
    void positionReceived(gcs::PositionInfo info);
    void vfrHudReceived(gcs::VfrHudInfo info);
    void statusTextReceived(gcs::StatusTextInfo info);
    void anyMessage();

private:
    void flushPendingStatusText();

    // Прячет mavlink-типы из заголовка (pimpl).
    struct Impl;
    Impl *d = nullptr;

    quint64 m_totalMessages = 0;
    quint64 m_parseErrors = 0;

    // Склейка чанков длинных STATUSTEXT (общий id у последовательных кусков).
    QString m_pendingText;
    int m_pendingSeverity = 0;
    int m_pendingId = 0;
    QTimer *m_flushTimer = nullptr;
};

} // namespace gcs
