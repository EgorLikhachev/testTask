#include "mavlink/MavlinkCommands.h"

#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
extern "C" {
#include <ardupilotmega/mavlink.h>
}
#pragma GCC diagnostic pop

namespace gcs {
namespace MavlinkCommands {

namespace {
QByteArray toBytes(const mavlink_message_t &msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    return QByteArray(reinterpret_cast<const char *>(buf), len);
}
} // namespace

QByteArray heartbeat(quint8 sysid, quint8 compid)
{
    mavlink_message_t msg;
    mavlink_msg_heartbeat_pack(sysid, compid, &msg,
                               MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID,
                               0, 0, MAV_STATE_ACTIVE);
    return toBytes(msg);
}

QByteArray setMessageInterval(quint8 sysid, quint8 compid,
                              quint8 targetSysid, quint8 targetCompid,
                              quint16 msgId, qint32 intervalUs)
{
    mavlink_message_t msg;
    mavlink_msg_command_long_pack(sysid, compid, &msg,
                                  targetSysid, targetCompid,
                                  MAV_CMD_SET_MESSAGE_INTERVAL,
                                  0, // confirmation
                                  float(msgId), float(intervalUs),
                                  0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    return toBytes(msg);
}

} // namespace MavlinkCommands
} // namespace gcs
