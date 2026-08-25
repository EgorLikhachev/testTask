#include <QtTest>

#include "announce/Announcer.h"
#include "config/AppConfig.h"
#include "domain/AntiSpamFilter.h"
#include "domain/EventDetector.h"
#include "domain/VehicleState.h"
#include "mavlink/CopterModes.h"
#include "tts/ITtsBackend.h"

using namespace gcs;

namespace {
HeartbeatInfo hb(quint32 mode, bool armed)
{
    HeartbeatInfo i;
    i.customMode = mode;
    i.modeName = CopterModes::name(mode);
    i.armed = armed;
    return i;
}

BatteryInfo batt(int pct)
{
    BatteryInfo b;
    b.remainingPercent = pct;
    b.voltageV = 12.0f;
    b.valid = true;
    return b;
}
} // namespace

class TestDomain : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void modeChangeDetected();
    void armDisarmDetected();
    void batteryThresholdsWithHysteresis();
    void statustextSeverityFilter();
    void antispamRealClock();
    void antispamInjectedTime();
    void copterModesTable();
    void statusPhrase();
    void modePhrase();
};

void TestDomain::initTestCase()
{
    qRegisterMetaType<HeartbeatInfo>();
    qRegisterMetaType<BatteryInfo>();
    qRegisterMetaType<StatusTextInfo>();
}

void TestDomain::modeChangeDetected()
{
    EventDetector d(BatteryThresholds{25, 15, 5});
    QSignalSpy spy(&d, &EventDetector::flightModeChanged);

    d.onHeartbeat(hb(5, false));  // первый — база, но режим объявляется
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("LOITER"));

    d.onHeartbeat(hb(5, false));  // без изменений
    QCOMPARE(spy.count(), 1);

    d.onHeartbeat(hb(6, false));
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("RTL"));
}

void TestDomain::armDisarmDetected()
{
    EventDetector d(BatteryThresholds{25, 15, 5});
    QSignalSpy spy(&d, &EventDetector::armedChanged);

    d.onHeartbeat(hb(3, true));   // первое состояние — база, не событие
    QCOMPARE(spy.count(), 0);

    d.onHeartbeat(hb(3, true));   // без изменений
    QCOMPARE(spy.count(), 0);

    d.onHeartbeat(hb(3, false));  // disarm
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toBool(), false);

    d.onHeartbeat(hb(3, true));   // arm
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toBool(), true);
}

void TestDomain::batteryThresholdsWithHysteresis()
{
    // warn=25, critical=15, margin=5
    EventDetector d(BatteryThresholds{25, 15, 5});
    QSignalSpy spy(&d, &EventDetector::batteryLevelChanged);

    d.onBattery(batt(100));
    QCOMPARE(spy.count(), 0);

    d.onBattery(batt(25));        // вход в warning
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), EventDetector::LevelWarning);

    d.onBattery(batt(24));        // дребезг около порога — не повтор
    QCOMPARE(spy.count(), 1);

    d.onBattery(batt(14));        // вход в critical
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toInt(), EventDetector::LevelCritical);

    d.onBattery(batt(18));        // ниже критического+гистерезис — не снятие
    QCOMPARE(spy.count(), 2);

    d.onBattery(batt(21));        // critical -> warning (тихо)
    QCOMPARE(spy.count(), 2);

    d.onBattery(batt(31));        // warning -> normal (тихо), 31 >= 25+5
    QCOMPARE(spy.count(), 2);

    d.onBattery(batt(24));        // повторный вход в warning — снова событие
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).toInt(), EventDetector::LevelWarning);
}

void TestDomain::statustextSeverityFilter()
{
    EventDetector d(BatteryThresholds{25, 15, 5});
    QSignalSpy spy(&d, &EventDetector::statusWarning);

    StatusTextInfo notice;
    notice.severity = StatusSeverity::Notice;
    notice.text = "info message";
    d.onStatusText(notice);
    QCOMPARE(spy.count(), 0);

    StatusTextInfo warning;
    warning.severity = StatusSeverity::Warning;
    warning.text = "warn message";
    d.onStatusText(warning);
    QCOMPARE(spy.count(), 1);

    StatusTextInfo emergency;
    emergency.severity = StatusSeverity::Emergency;
    emergency.text = "emergency";
    d.onStatusText(emergency);
    QCOMPARE(spy.count(), 2);
}

void TestDomain::antispamRealClock()
{
    AntiSpamFilter f;
    QVERIFY(f.allow("mode", 3600));
    QVERIFY(!f.allow("mode", 3600));     // сразу повтор — блок
    QVERIFY(f.allow("other", 3600));     // другой ключ не влияет
}

void TestDomain::antispamInjectedTime()
{
    AntiSpamFilter f;
    QVERIFY(f.allowMs("k", 5000, 0));
    QVERIFY(!f.allowMs("k", 5000, 100));
    QVERIFY(!f.allowMs("k", 5000, 4999));
    QVERIFY(f.allowMs("k", 5000, 5000)); // ровно интервал — уже можно
}

void TestDomain::copterModesTable()
{
    QCOMPARE(CopterModes::name(0), QStringLiteral("STABILIZE"));
    QCOMPARE(CopterModes::name(5), QStringLiteral("LOITER"));
    QCOMPARE(CopterModes::name(9), QStringLiteral("LAND"));
    QVERIFY(CopterModes::name(999).contains(QLatin1String("999")));
}

void TestDomain::statusPhrase()
{
    AppConfig cfg;
    VehicleState state;
    state.onPosition(PositionInfo{55.75, 37.61, 12.34f, 140.0f, 0, 0, 0, true});
    VfrHudInfo v;
    v.groundspeedMps = 5.4f;
    v.valid = true;
    state.onVfrHud(v);
    state.onBattery(batt(87));

    Announcer a(cfg);
    a.setVehicleState(&state);
    QSignalSpy spy(&a, &Announcer::announce);
    a.onStatusRequested();

    QCOMPARE(spy.count(), 1);
    const QString phrase = spy.at(0).at(0).toString();
    QVERIFY(phrase.contains(QStringLiteral("12 метров")));
    QVERIFY(phrase.contains(QStringLiteral("5 метров в секунду")));
    QVERIFY(phrase.contains(QStringLiteral("87 процентов")));
    QCOMPARE(spy.at(0).at(1).toInt(), tts::PriorityImmediate);
}

void TestDomain::modePhrase()
{
    AppConfig cfg;
    Announcer a(cfg);
    QSignalSpy spy(&a, &Announcer::announce);

    a.onFlightModeChanged(QStringLiteral("RTL"));
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString()
                .contains(QStringLiteral("возврат в точку взлёта")));

    // Повтор той же смены внутри интервала антиспама — блокируется.
    a.onFlightModeChanged(QStringLiteral("RTL"));
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestDomain)
#include "tst_domain.moc"
