#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace gcs {

// Построение исходящих MAVLink-сообщений (байты для транспорта).
namespace MavlinkCommands {

// Heartbeat типа GCS.
QByteArray heartbeat(quint8 sysid, quint8 compid);

// COMMAND_LONG MAV_CMD_SET_MESSAGE_INTERVAL: запрос периода сообщения.
QByteArray setMessageInterval(quint8 sysid, quint8 compid,
                              quint8 targetSysid, quint8 targetCompid,
                              quint16 msgId, qint32 intervalUs);

} // namespace MavlinkCommands
} // namespace gcs
