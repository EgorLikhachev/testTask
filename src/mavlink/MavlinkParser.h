#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "domain/Telemetry.h"

namespace gcs {

// Слой парсинга MAVLink: единственное место, зависящее от c_library_v2.
// Принимает сырые байты, отдаёт декодированные DTO доменному слою.
//
// Фильтр борта: setTargetSysid(0) — авто-захват первого heartbeat валидного
// борта (autopilot != INVALID, тип != GCS); setTargetSysid(N) — слушать
// только систему N. Сообщения остальных систем отбрасываются.
class MavlinkParser : public QObject
{
    Q_OBJECT
public:
    explicit MavlinkParser(QObject *parent = nullptr);
    ~MavlinkParser() override;

    void setTargetSysid(quint8 sysid) { m_targetSysid = sysid; }
    quint8 targetSysid() const { return m_targetSysid; }

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
    // Каждый успешно разобранный кадр (перепакованный MAVLink-фрейм)
    // — для записи телеметрии (tlog).
    void rawFrame(const QByteArray &frame);
    // Зафиксирован прослушиваемый борт (при авто-захвате).
    void targetLocked(quint8 sysid);

private slots:
    void flushPendingStatusText();

private:
    // Прячет mavlink-типы из заголовка (pimpl).
    struct Impl;
    Impl *d = nullptr;

    quint64 m_totalMessages = 0;
    quint64 m_parseErrors = 0;
    quint8 m_targetSysid = 0;

    // Склейка чанков длинных STATUSTEXT (общий id у последовательных кусков).
    QString m_pendingText;
    int m_pendingSeverity = 0;
    int m_pendingId = 0;
    QTimer *m_flushTimer = nullptr;
};

} // namespace gcs
