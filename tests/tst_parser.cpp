#include <QtTest>

#include <cstring>

#include "mavlink/CopterModes.h"
#include "mavlink/MavlinkParser.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
extern "C" {
#include <ardupilotmega/mavlink.h>
}
#pragma GCC diagnostic pop

using namespace gcs;

namespace {

QByteArray pack(const mavlink_message_t &msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    return QByteArray(reinterpret_cast<const char *>(buf), len);
}

QByteArray heartbeatFrame(quint32 customMode, bool armed)
{
    mavlink_message_t msg;
    const quint8 base = MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
                        | (armed ? MAV_MODE_FLAG_SAFETY_ARMED : 0);
    mavlink_msg_heartbeat_pack(1, 1, &msg, MAV_TYPE_QUADROTOR,
                               MAV_AUTOPILOT_ARDUPILOTMEGA, base, customMode,
                               MAV_STATE_ACTIVE);
    return pack(msg);
}

QByteArray sysStatusFrame(int16_t batteryRemaining, uint16_t voltageMv)
{
    mavlink_message_t msg;
    mavlink_sys_status_t st{};
    st.load = 500;
    st.voltage_battery = voltageMv;
    st.current_battery = 900;
    st.battery_remaining = int8_t(batteryRemaining);
    mavlink_msg_sys_status_encode(1, 1, &msg, &st);
    return pack(msg);
}

QByteArray statustextFrame(uint8_t severity, const QByteArray &text, uint8_t id)
{
    mavlink_message_t msg;
    mavlink_statustext_t st{};
    st.severity = severity;
    st.id = id;
    std::memcpy(st.text, text.constData(), size_t(qMin(text.size(), 50)));
    mavlink_msg_statustext_encode(1, 1, &msg, &st);
    return pack(msg);
}

} // namespace

class TestParser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void heartbeatWithJunkAndSplit();
    void batteryFromSysStatus();
    void statustextSingle();
    void statustextChunked();
    void vfrHudAndPosition();
    void garbageDoesNotCrash();
};

void TestParser::initTestCase()
{
    // QSignalSpy требуется зарегистрированный метатип для аргументов сигналов.
    qRegisterMetaType<HeartbeatInfo>();
    qRegisterMetaType<BatteryInfo>();
    qRegisterMetaType<PositionInfo>();
    qRegisterMetaType<VfrHudInfo>();
    qRegisterMetaType<StatusTextInfo>();
}

void TestParser::heartbeatWithJunkAndSplit()
{
    MavlinkParser parser;
    QSignalSpy hb(&parser, &MavlinkParser::heartbeatReceived);
    QSignalSpy any(&parser, &MavlinkParser::anyMessage);

    // Мусор перед кадром + разрыв кадра на две порции байт:
    // парсер обязан ресинхронизироваться и декодировать сообщение.
    // (в мусоре нет магических байтов 0xFE/0xFD)
    QByteArray raw = QByteArrayLiteral("AAAjunk-bytes!!")
                     + heartbeatFrame(5 /* LOITER */, true);
    parser.feed(raw.left(raw.size() / 3));
    parser.feed(raw.mid(raw.size() / 3));

    QCOMPARE(hb.count(), 1);
    const auto info =
        qvariant_cast<HeartbeatInfo>(hb.at(0).at(0));
    QCOMPARE(info.customMode, quint32(5));
    QCOMPARE(info.modeName, QStringLiteral("LOITER"));
    QCOMPARE(info.armed, true);
    QCOMPARE(info.sysid, quint8(1));
    QVERIFY(any.count() >= 1);
    QCOMPARE(parser.totalMessages(), quint64(1));
}

void TestParser::batteryFromSysStatus()
{
    MavlinkParser parser;
    QSignalSpy batt(&parser, &MavlinkParser::batteryReceived);
    parser.feed(sysStatusFrame(77, 11800));

    QCOMPARE(batt.count(), 1);
    const auto info = qvariant_cast<BatteryInfo>(batt.at(0).at(0));
    QCOMPARE(info.remainingPercent, 77);
    QCOMPARE(info.voltageV, 11.8f);
    QVERIFY(info.valid);
}

void TestParser::statustextSingle()
{
    MavlinkParser parser;
    QSignalSpy st(&parser, &MavlinkParser::statusTextReceived);
    parser.feed(statustextFrame(MAV_SEVERITY_WARNING, "Low battery", 0));

    QCOMPARE(st.count(), 1);
    const auto info = qvariant_cast<StatusTextInfo>(st.at(0).at(0));
    QCOMPARE(info.text, QStringLiteral("Low battery"));
    QCOMPARE(info.severity, StatusSeverity::Warning);
}

void TestParser::statustextChunked()
{
    MavlinkParser parser;
    QSignalSpy st(&parser, &MavlinkParser::statusTextReceived);
    // Два чанка с одним id должны склеиться в одно сообщение после таймаута.
    parser.feed(statustextFrame(MAV_SEVERITY_ERROR, "Part one ", 7));
    parser.feed(statustextFrame(MAV_SEVERITY_ERROR, "Part two", 7));
    QCOMPARE(st.count(), 0);

    QTRY_COMPARE_WITH_TIMEOUT(st.count(), 1, 2000);
    const auto info = qvariant_cast<StatusTextInfo>(st.at(0).at(0));
    QCOMPARE(info.text, QStringLiteral("Part one Part two"));
    QCOMPARE(info.severity, StatusSeverity::Error);
}

void TestParser::vfrHudAndPosition()
{
    MavlinkParser parser;
    QSignalSpy vfr(&parser, &MavlinkParser::vfrHudReceived);
    QSignalSpy pos(&parser, &MavlinkParser::positionReceived);

    mavlink_message_t v;
    mavlink_msg_vfr_hud_pack(1, 1, &v, 0.0f, 540 /* см/с */, 100, 50, 12.5f, 0.5f);
    parser.feed(pack(v));
    mavlink_message_t p;
    // time_boot, lat, lon, alt=40м, relative_alt=15м, vx, vy, vz, hdg
    mavlink_msg_global_position_int_pack(1, 1, &p, 0, 0, 0, 40000, 15000, 0, 0, 0, 100);
    parser.feed(pack(p));

    QCOMPARE(vfr.count(), 1);
    QCOMPARE(qvariant_cast<VfrHudInfo>(vfr.at(0).at(0)).groundspeedMps, 5.4f);
    QCOMPARE(pos.count(), 1);
    QCOMPARE(qvariant_cast<PositionInfo>(pos.at(0).at(0)).relAltM, 15.0f);
}

void TestParser::garbageDoesNotCrash()
{
    MavlinkParser parser;
    QSignalSpy hb(&parser, &MavlinkParser::heartbeatReceived);

    // Печатный ASCII без магических байт MAVLink — гарантированно мусор.
    QByteArray junk(4096, Qt::Uninitialized);
    for (int i = 0; i < junk.size(); ++i)
        junk[i] = char(32 + (i % 90));
    parser.feed(junk);
    parser.feed(heartbeatFrame(6, false));
    parser.feed(junk);

    QCOMPARE(hb.count(), 1);
    QCOMPARE(qvariant_cast<HeartbeatInfo>(hb.at(0).at(0)).modeName,
             QStringLiteral("RTL"));
}

QTEST_MAIN(TestParser)
#include "tst_parser.moc"
