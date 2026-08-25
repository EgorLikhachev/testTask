#pragma once

#include <QObject>
#include <QString>

#include "domain/Telemetry.h"

namespace gcs {

// Пороговые значения батареи из конфигурации.
struct BatteryThresholds {
    int warnPercent = 25;
    int criticalPercent = 15;
    int recoverMarginPercent = 5;
};

// Выявление событий из потока обновлений телеметрии:
//  - смена режима полёта;
//  - arm / disarm (первое состояние берётся за базу, не озвучивается);
//  - пересечение порогов батареи (с гистерезисом);
//  - STATUSTEXT уровня WARNING и выше.
class EventDetector : public QObject
{
    Q_OBJECT
public:
    explicit EventDetector(const BatteryThresholds &thresholds,
                           QObject *parent = nullptr);

    enum BatteryLevel { LevelWarning = 0, LevelCritical = 1 };

signals:
    void flightModeChanged(const QString &modeName);
    void armedChanged(bool armed);
    // level: LevelWarning или LevelCritical — момент входа в зону.
    void batteryLevelChanged(int level);
    void statusWarning(gcs::StatusTextInfo info);

public slots:
    void onHeartbeat(gcs::HeartbeatInfo info);
    void onBattery(gcs::BatteryInfo info);
    void onStatusText(gcs::StatusTextInfo info);

private:
    void updateBattery(int percent);

    BatteryThresholds m_th;

    QString m_lastMode;
    bool m_haveHeartbeat = false;
    bool m_lastArmed = false;

    enum class BatteryState { Normal, Warning, Critical };
    BatteryState m_batteryState = BatteryState::Normal;
};

} // namespace gcs
