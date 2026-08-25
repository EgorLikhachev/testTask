#include "domain/EventDetector.h"

namespace gcs {

EventDetector::EventDetector(const BatteryThresholds &thresholds, QObject *parent)
    : QObject(parent)
    , m_th(thresholds)
{
}

void EventDetector::onHeartbeat(HeartbeatInfo info)
{
    if (!m_haveHeartbeat) {
        // Первое heartbeat — базовое состояние: режим озвучиваем, arm — нет.
        m_haveHeartbeat = true;
        m_lastMode = info.modeName;
        m_lastArmed = info.armed;
        emit flightModeChanged(info.modeName);
        return;
    }
    if (info.modeName != m_lastMode) {
        m_lastMode = info.modeName;
        emit flightModeChanged(info.modeName);
    }
    if (info.armed != m_lastArmed) {
        m_lastArmed = info.armed;
        emit armedChanged(info.armed);
    }
}

void EventDetector::onBattery(BatteryInfo info)
{
    if (!info.valid || info.remainingPercent < 0)
        return;
    updateBattery(info.remainingPercent);
}

void EventDetector::updateBattery(int pct)
{
    switch (m_batteryState) {
    case BatteryState::Normal:
        if (pct <= m_th.criticalPercent) {
            m_batteryState = BatteryState::Critical;
            emit batteryLevelChanged(LevelCritical);
        } else if (pct <= m_th.warnPercent) {
            m_batteryState = BatteryState::Warning;
            emit batteryLevelChanged(LevelWarning);
        }
        break;
    case BatteryState::Warning:
        if (pct <= m_th.criticalPercent) {
            m_batteryState = BatteryState::Critical;
            emit batteryLevelChanged(LevelCritical);
        } else if (pct >= m_th.warnPercent + m_th.recoverMarginPercent) {
            m_batteryState = BatteryState::Normal;
        }
        break;
    case BatteryState::Critical:
        // Выходим из тревоги только с запасом (гистерезис).
        if (pct >= m_th.criticalPercent + m_th.recoverMarginPercent) {
            if (pct <= m_th.warnPercent) {
                m_batteryState = BatteryState::Warning;
            } else if (pct >= m_th.warnPercent + m_th.recoverMarginPercent) {
                m_batteryState = BatteryState::Normal;
            }
        }
        break;
    }
}

void EventDetector::onStatusText(StatusTextInfo info)
{
    // MAV_SEVERITY: 0 — самый тяжёлый; WARNING = 4. Пропускаем WARNING и выше.
    if (info.severity <= StatusSeverity::Warning && !info.text.trimmed().isEmpty())
        emit statusWarning(info);
}

} // namespace gcs
