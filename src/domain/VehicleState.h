#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include "domain/Telemetry.h"

namespace gcs {

// Доменная модель: текущий агрегированный снимок состояния БПЛА.
// Пишется парсером, читается UI и Announcer'ом. Никакой логики событий —
// за события отвечает EventDetector.
class VehicleState : public QObject
{
    Q_OBJECT
public:
    explicit VehicleState(QObject *parent = nullptr);

    QString modeName() const { return m_modeName; }
    bool armed() const { return m_armed; }
    int batteryPercent() const { return m_batteryPercent; }
    float batteryVoltage() const { return m_batteryVoltage; }
    bool hasBattery() const { return m_batteryValid; }
    float relativeAltitudeM() const { return m_relAltM; }
    bool hasPosition() const { return m_positionValid; }
    float groundSpeedMps() const { return m_groundSpeedMps; }
    bool hasVfr() const { return m_vfrValid; }

    quint64 totalMessages() const { return m_totalMessages; }
    quint64 parseErrors() const { return m_parseErrors; }

    // true, если сообщения приходили за последние windowMs.
    bool linkAlive(int windowMs = 3000) const;

public slots:
    void onHeartbeat(gcs::HeartbeatInfo info);
    void onBattery(gcs::BatteryInfo info);
    void onPosition(gcs::PositionInfo info);
    void onVfrHud(gcs::VfrHudInfo info);
    void onAnyMessage();
    void onParseError();

private:
    QString m_modeName = QStringLiteral("—");
    bool m_armed = false;
    int m_batteryPercent = -1;
    float m_batteryVoltage = 0.0f;
    bool m_batteryValid = false;
    float m_relAltM = 0.0f;
    bool m_positionValid = false;
    float m_groundSpeedMps = 0.0f;
    bool m_vfrValid = false;

    quint64 m_totalMessages = 0;
    quint64 m_parseErrors = 0;
    qint64 m_lastMessageMs = -1;

    QElapsedTimer m_clock;
};

} // namespace gcs
