#include "mavlink/CopterModes.h"

namespace gcs {
namespace CopterModes {

QString name(quint32 customMode)
{
    switch (customMode) {
    case 0: return QStringLiteral("STABILIZE");
    case 1: return QStringLiteral("ACRO");
    case 2: return QStringLiteral("ALT_HOLD");
    case 3: return QStringLiteral("AUTO");
    case 4: return QStringLiteral("GUIDED");
    case 5: return QStringLiteral("LOITER");
    case 6: return QStringLiteral("RTL");
    case 7: return QStringLiteral("CIRCLE");
    case 9: return QStringLiteral("LAND");
    case 11: return QStringLiteral("DRIFT");
    case 13: return QStringLiteral("SPORT");
    case 14: return QStringLiteral("FLIP");
    case 15: return QStringLiteral("AUTOTUNE");
    case 16: return QStringLiteral("POSHOLD");
    case 17: return QStringLiteral("BRAKE");
    case 18: return QStringLiteral("THROW");
    case 19: return QStringLiteral("AVOID_ADLB");
    case 20: return QStringLiteral("GUIDED_NOGPS");
    case 21: return QStringLiteral("SMART_RTL");
    case 22: return QStringLiteral("FLOWHOLD");
    case 23: return QStringLiteral("FOLLOW");
    case 24: return QStringLiteral("ZIGZAG");
    case 25: return QStringLiteral("SYSTEMID");
    case 26: return QStringLiteral("AUTOROTATE");
    case 27: return QStringLiteral("AUTO_RTL");
    case 28: return QStringLiteral("TURTLE");
    default: return QStringLiteral("MODE_%1").arg(customMode);
    }
}

} // namespace CopterModes
} // namespace gcs
