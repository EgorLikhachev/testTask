#include "mavlink/MavlinkParser.h"

#include <cstring>

#include "mavlink/CopterModes.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
extern "C" {
#include <ardupilotmega/mavlink.h>
}
#pragma GCC diagnostic pop

namespace gcs {

struct MavlinkParser::Impl {
    mavlink_message_t msg{};
    mavlink_status_t status{};
};

namespace {
StatusSeverity toSeverity(int sev)
{
    if (sev < 0)
        return StatusSeverity::Emergency;
    if (sev > 7)
        return StatusSeverity::Debug;
    return static_cast<StatusSeverity>(sev);
}

QString fixedText(const char *text, size_t cap, bool trim = true)
{
    size_t n = 0;
    while (n < cap && text[n] != '\0')
        ++n;
    const QString s = QString::fromLatin1(text, int(n));
    return trim ? s.trimmed() : s;
}
} // namespace

MavlinkParser::MavlinkParser(QObject *parent)
    : QObject(parent)
    , d(new Impl)
{
    m_flushTimer = new QTimer(this);
    m_flushTimer->setSingleShot(true);
    m_flushTimer->setInterval(500);
    connect(m_flushTimer, &QTimer::timeout, this, &MavlinkParser::flushPendingStatusText);
}

MavlinkParser::~MavlinkParser()
{
    delete d;
}

void MavlinkParser::feed(const QByteArray &bytes)
{
    const auto *data = reinterpret_cast<const uint8_t *>(bytes.constData());
    for (int i = 0; i < bytes.size(); ++i) {
        if (!mavlink_parse_char(MAVLINK_COMM_0, data[i], &d->msg, &d->status))
            continue;

        // Фильтр борта: после захвата принимаем только его систему.
        if (m_targetSysid != 0 && d->msg.sysid != m_targetSysid)
            continue;

        // Авто-захват: первый heartbeat валидного борта фиксирует цель.
        if (m_targetSysid == 0 && d->msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
            mavlink_heartbeat_t probe;
            mavlink_msg_heartbeat_decode(&d->msg, &probe);
            if (probe.autopilot != MAV_AUTOPILOT_INVALID
                && probe.type != MAV_TYPE_GCS) {
                m_targetSysid = d->msg.sysid;
                qInfo("[parser] борт зафиксирован: sysid %u", unsigned(m_targetSysid));
                emit targetLocked(m_targetSysid);
            }
        }

        // Полный кадр — наружу (tlog-запись). to_send_buffer корректен
        // для неподписанных кадров; подпись v2, если была, не сохраняется.
        {
            uint8_t buf[MAVLINK_MAX_PACKET_LEN];
            const int len = mavlink_msg_to_send_buffer(buf, &d->msg);
            if (len > 0)
                emit rawFrame(QByteArray(reinterpret_cast<const char *>(buf), len));
        }

        m_totalMessages++;

        switch (d->msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t m;
            mavlink_msg_heartbeat_decode(&d->msg, &m);
            HeartbeatInfo info;
            info.customMode = m.custom_mode;
            info.baseMode = m.base_mode;
            info.systemStatus = m.system_status;
            info.autopilot = m.autopilot;
            info.mavType = m.type;
            info.sysid = d->msg.sysid;
            info.compid = d->msg.compid;
            info.armed = (m.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0;
            info.modeName = CopterModes::name(m.custom_mode);
            emit heartbeatReceived(info);
            break;
        }
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t m;
            mavlink_msg_sys_status_decode(&d->msg, &m);
            BatteryInfo info;
            if (m.voltage_battery > 0)
                info.voltageV = m.voltage_battery / 1000.0f;
            if (m.battery_remaining >= 0)
                info.remainingPercent = m.battery_remaining;
            info.valid = info.remainingPercent >= 0 || info.voltageV > 0.0f;
            emit batteryReceived(info);
            break;
        }
        case MAVLINK_MSG_ID_BATTERY_STATUS: {
            mavlink_battery_status_t m;
            mavlink_msg_battery_status_decode(&d->msg, &m);
            BatteryInfo info;
            if (m.voltages[0] != UINT16_MAX && m.voltages[0] > 0)
                info.voltageV = m.voltages[0] / 1000.0f;
            if (m.battery_remaining >= 0)
                info.remainingPercent = m.battery_remaining;
            info.currentA = m.current_battery / 100.0f;
            info.valid = info.remainingPercent >= 0 || info.voltageV > 0.0f;
            emit batteryReceived(info);
            break;
        }
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t m;
            mavlink_msg_global_position_int_decode(&d->msg, &m);
            PositionInfo info;
            info.latDeg = m.lat / 1e7;
            info.lonDeg = m.lon / 1e7;
            info.relAltM = m.relative_alt / 1000.0f;
            info.absAltM = m.alt / 1000.0f;
            info.velXMps = m.vx / 100.0f;
            info.velYMps = m.vy / 100.0f;
            info.velZMps = m.vz / 100.0f;
            info.valid = true;
            emit positionReceived(info);
            break;
        }
        case MAVLINK_MSG_ID_VFR_HUD: {
            mavlink_vfr_hud_t m;
            mavlink_msg_vfr_hud_decode(&d->msg, &m);
            VfrHudInfo info;
            info.airspeedMps = m.airspeed;
            info.groundspeedMps = m.groundspeed / 100.0f;
            info.altM = m.alt;
            info.climbMps = m.climb;
            info.headingDeg = m.heading;
            info.valid = true;
            emit vfrHudReceived(info);
            break;
        }
        case MAVLINK_MSG_ID_STATUSTEXT: {
            mavlink_statustext_t m;
            mavlink_msg_statustext_decode(&d->msg, &m);
            const int id = m.id;
            const QString text =
                fixedText(m.text, sizeof(m.text), id != 0 ? false : true);
            if (id == 0 || m_pendingId == 0 || id != m_pendingId) {
                if (id == 0) {
                    flushPendingStatusText();
                    StatusTextInfo info;
                    info.severity = toSeverity(m.severity);
                    info.text = text;
                    emit statusTextReceived(info);
                } else {
                    flushPendingStatusText();
                    m_pendingId = id;
                    m_pendingText = text;
                    m_pendingSeverity = m.severity;
                }
            } else {
                // Продолжение чанкованного сообщения.
                m_pendingText += text;
            }
            if (m_pendingId != 0)
                m_flushTimer->start();
            break;
        }
        default:
            break;
        }

        emit anyMessage();
    }

    m_parseErrors = d->status.parse_error;
}

void MavlinkParser::flushPendingStatusText()
{
    if (m_pendingId == 0)
        return;
    StatusTextInfo info;
    info.severity = toSeverity(m_pendingSeverity);
    info.text = m_pendingText.trimmed();
    m_pendingId = 0;
    m_pendingText.clear();
    if (!info.text.isEmpty())
        emit statusTextReceived(info);
}

} // namespace gcs
