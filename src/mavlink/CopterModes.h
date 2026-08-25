#pragma once

#include <QString>

namespace gcs {

// Соответствие custom_mode из HEARTBEAT ArduCopter имени режима полёта.
namespace CopterModes {

// Английское имя режима ("LOITER" и т.п.); для неизвестного кода — "MODE_<n>".
QString name(quint32 customMode);

} // namespace CopterModes
} // namespace gcs
