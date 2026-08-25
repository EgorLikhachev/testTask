#include "domain/VehicleState.h"

namespace gcs {

VehicleState::VehicleState(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
}

bool VehicleState::linkAlive(int windowMs) const
{
    if (m_lastMessageMs < 0)
        return false;
    return m_clock.elapsed() - m_lastMessageMs <= windowMs;
}

void VehicleState::onHeartbeat(HeartbeatInfo info)
{
    m_modeName = info.modeName;
    m_armed = info.armed;
}

void VehicleState::onBattery(BatteryInfo info)
{
    if (info.remainingPercent >= 0)
        m_batteryPercent = info.remainingPercent;
    if (info.voltageV > 0.0f)
        m_batteryVoltage = info.voltageV;
    m_batteryValid = m_batteryValid || info.valid;
}

void VehicleState::onPosition(PositionInfo info)
{
    if (!info.valid)
        return;
    m_relAltM = info.relAltM;
    m_positionValid = true;
}

void VehicleState::onVfrHud(VfrHudInfo info)
{
    if (!info.valid)
        return;
    m_groundSpeedMps = info.groundspeedMps;
    m_vfrValid = true;
}

void VehicleState::onAnyMessage()
{
    m_totalMessages++;
    m_lastMessageMs = m_clock.elapsed();
}

void VehicleState::onParseError()
{
    m_parseErrors++;
}

} // namespace gcs
