#pragma once

#include <QObject>
#include <QString>

#include "config/AppConfig.h"
#include "domain/AntiSpamFilter.h"
#include "domain/Telemetry.h"

namespace gcs {

class VehicleState;

// Слой озвучки: превращает события детектора в русские фразы,
// применяет антиспам и отдаёт готовые фразы в очередь TTS.
class Announcer : public QObject
{
    Q_OBJECT
public:
    Announcer(const AppConfig &cfg, QObject *parent = nullptr);

    // Нужен для статус-спича и текущего заряда батареи.
    void setVehicleState(VehicleState *state) { m_state = state; }

signals:
    // Фраза, прошедшая антиспам, — в очередь TTS.
    void announce(QString phrase, int priority);
    // Любое событие (и озвученное, и подавленное) — в лог UI.
    void eventLogged(QString line);

public slots:
    void onFlightModeChanged(const QString &modeName);
    void onArmedChanged(bool armed);
    void onBatteryLevel(int level);
    void onStatusWarning(gcs::StatusTextInfo info);
    void onStatusRequested();
    void onLinkEstablished();
    void onLinkLost();
    void onLinkRegained();

private:
    bool tryAnnounce(const QString &antispamKey, int minIntervalSec,
                     const QString &phrase, int priority, const QString &eventLine);

    const AppConfig &m_cfg;
    AntiSpamFilter m_antispam;
    VehicleState *m_state = nullptr;
};

} // namespace gcs
