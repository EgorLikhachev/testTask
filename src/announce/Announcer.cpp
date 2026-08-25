#include "announce/Announcer.h"

#include <QHash>

#include "domain/EventDetector.h"
#include "domain/VehicleState.h"
#include "tts/ITtsBackend.h"

namespace gcs {

namespace {

// Русская форма множественного числа: 1 процент / 2 процента / 5 процентов.
QString plural(int n, const QString &one, const QString &few, const QString &many)
{
    const int abs = qAbs(n);
    const int mod10 = abs % 10;
    const int mod100 = abs % 100;
    if (mod10 == 1 && mod100 != 11)
        return one;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        return few;
    return many;
}

QString percentPhrase(int n)
{
    return QStringLiteral("%1 %2").arg(n).arg(plural(n, QStringLiteral("процент"),
                                                       QStringLiteral("процента"),
                                                       QStringLiteral("процентов")));
}

QString metersPhrase(int n)
{
    return QStringLiteral("%1 %2").arg(n).arg(plural(n, QStringLiteral("метр"),
                                                      QStringLiteral("метра"),
                                                      QStringLiteral("метров")));
}

QString speedPhrase(int n)
{
    return QStringLiteral("%1 %2").arg(n).arg(
        plural(n, QStringLiteral("метр в секунду"),
               QStringLiteral("метра в секунду"),
               QStringLiteral("метров в секунду")));
}

// Русские названия режимов для озвучки; незнакомые читаем как есть.
QString russianModeName(const QString &modeName)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("STABILIZE"), QStringLiteral("стабилизация")},
        {QStringLiteral("ACRO"), QStringLiteral("акро")},
        {QStringLiteral("ALT_HOLD"), QStringLiteral("удержание высоты")},
        {QStringLiteral("AUTO"), QStringLiteral("авто")},
        {QStringLiteral("GUIDED"), QStringLiteral("наведение")},
        {QStringLiteral("LOITER"), QStringLiteral("лойтер")},
        {QStringLiteral("RTL"), QStringLiteral("возврат в точку взлёта")},
        {QStringLiteral("CIRCLE"), QStringLiteral("круг")},
        {QStringLiteral("LAND"), QStringLiteral("посадка")},
        {QStringLiteral("DRIFT"), QStringLiteral("дрейф")},
        {QStringLiteral("SPORT"), QStringLiteral("спорт")},
        {QStringLiteral("FLIP"), QStringLiteral("переворот")},
        {QStringLiteral("AUTOTUNE"), QStringLiteral("автонастройка")},
        {QStringLiteral("POSHOLD"), QStringLiteral("удержание позиции")},
        {QStringLiteral("BRAKE"), QStringLiteral("торможение")},
        {QStringLiteral("THROW"), QStringLiteral("бросок")},
        {QStringLiteral("GUIDED_NOGPS"), QStringLiteral("наведение без GPS")},
        {QStringLiteral("SMART_RTL"), QStringLiteral("умный возврат")},
        {QStringLiteral("FLOWHOLD"), QStringLiteral("удержание по потоку")},
        {QStringLiteral("FOLLOW"), QStringLiteral("сопровождение")},
        {QStringLiteral("ZIGZAG"), QStringLiteral("зигзаг")},
        {QStringLiteral("SYSTEMID"), QStringLiteral("идентификация системы")},
        {QStringLiteral("AUTOROTATE"), QStringLiteral("авторотация")},
        {QStringLiteral("AUTO_RTL"), QStringLiteral("автоматический возврат")},
        {QStringLiteral("TURTLE"), QStringLiteral("черепаха")},
    };
    return map.value(modeName, modeName.toLower());
}

QString severityPrefix(StatusSeverity sev)
{
    switch (sev) {
    case StatusSeverity::Emergency:
        return QStringLiteral("Авария!");
    case StatusSeverity::Alert:
        return QStringLiteral("Тревога!");
    case StatusSeverity::Critical:
        return QStringLiteral("Критическое событие!");
    default:
        return QString();
    }
}

} // namespace

Announcer::Announcer(const AppConfig &cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
{
}

bool Announcer::tryAnnounce(const QString &antispamKey, int minIntervalSec,
                            const QString &phrase, int priority, const QString &eventLine)
{
    if (!m_cfg.ttsEnabled) {
        qInfo("[event] %s — озвучка выключена в конфиге", qPrintable(eventLine));
        emit eventLogged(QStringLiteral("%1 — озвучка выключена в конфиге").arg(eventLine));
        return false;
    }
    if (m_antispam.allow(antispamKey, minIntervalSec)) {
        qInfo("[announce] %s", qPrintable(phrase));
        emit eventLogged(QStringLiteral("%1 — озвучено").arg(eventLine));
        emit announce(phrase, priority);
        return true;
    }
    qInfo("[event] %s — подавлено антиспамом (%d с)", qPrintable(eventLine), minIntervalSec);
    emit eventLogged(QStringLiteral("%1 — подавлено антиспамом").arg(eventLine));
    return false;
}

void Announcer::onFlightModeChanged(const QString &modeName)
{
    const QString phrase =
        QStringLiteral("Режим полёта: %1").arg(russianModeName(modeName));
    tryAnnounce(QStringLiteral("mode"), m_cfg.antispamModeSec, phrase,
                tts::PriorityNormal, QStringLiteral("смена режима: %1").arg(modeName));
}

void Announcer::onArmedChanged(bool armed)
{
    const QString phrase = armed ? QStringLiteral("Внимание! Моторы запущены")
                                 : QStringLiteral("Моторы остановлены");
    tryAnnounce(QStringLiteral("arm"), m_cfg.antispamArmSec, phrase,
                armed ? tts::PriorityCritical : tts::PriorityNormal,
                armed ? QStringLiteral("ARM") : QStringLiteral("DISARM"));
}

void Announcer::onBatteryLevel(int level)
{
    const int pct = m_state ? m_state->batteryPercent() : -1;
    if (level == EventDetector::LevelCritical) {
        const QString phrase = QStringLiteral(
            "Критический заряд батареи! %1. Требуется немедленная посадка")
                                   .arg(pct >= 0 ? percentPhrase(pct)
                                                 : QStringLiteral("заряд неизвестен"));
        tryAnnounce(QStringLiteral("battery_crit"), m_cfg.antispamBatterySec, phrase,
                    tts::PriorityCritical, QStringLiteral("батарея: критический уровень"));
    } else {
        const QString phrase =
            QStringLiteral("Внимание! Заряд батареи низкий: %1")
                .arg(pct >= 0 ? percentPhrase(pct) : QStringLiteral("заряд неизвестен"));
        tryAnnounce(QStringLiteral("battery_warn"), m_cfg.antispamBatterySec, phrase,
                    tts::PriorityNormal, QStringLiteral("батарея: предупреждение"));
    }
}

void Announcer::onStatusWarning(StatusTextInfo info)
{
    // По ТЗ — дословно; добавляем только короткий префикс тяжёлых уровней.
    const QString prefix = severityPrefix(info.severity);
    const QString phrase =
        prefix.isEmpty() ? info.text : QStringLiteral("%1 %2").arg(prefix, info.text);
    // Ключ антиспама — по тексту, чтобы разные сообщения не блокировали друг друга.
    const QString key = QStringLiteral("st:%1")
                            .arg(info.text.left(40).toLower().simplified());
    tryAnnounce(key, m_cfg.antispamStatustextSec, phrase, tts::PriorityCritical,
                QStringLiteral("STATUSTEXT(%1): %2")
                    .arg(int(info.severity))
                    .arg(info.text));
}

void Announcer::onLinkEstablished()
{
    tryAnnounce(QStringLiteral("link_up"), m_cfg.antispamLinkSec,
                QStringLiteral("Связь с бортом установлена"),
                tts::PriorityNormal, QStringLiteral("связь установлена"));
}

void Announcer::onLinkLost()
{
    // Ключи link_down/link_up раздельные: потеря не должна антиспамить
    // скорое восстановление (и наоборот).
    tryAnnounce(QStringLiteral("link_down"), m_cfg.antispamLinkSec,
                QStringLiteral("Потеря связи с бортом"),
                tts::PriorityCritical, QStringLiteral("потеря связи"));
}

void Announcer::onLinkRegained()
{
    tryAnnounce(QStringLiteral("link_up"), m_cfg.antispamLinkSec,
                QStringLiteral("Связь с бортом восстановлена"),
                tts::PriorityCritical, QStringLiteral("связь восстановлена"));
}

void Announcer::onStatusRequested()
{
    if (!m_state) {
        tryAnnounce(QStringLiteral("status"), m_cfg.antispamStatusHotkeySec,
                    QStringLiteral("Данные телеметрии отсутствуют"),
                    tts::PriorityImmediate, QStringLiteral("статус: нет данных"));
        return;
    }

    QStringList parts;
    if (m_state->hasPosition())
        parts << QStringLiteral("Высота %1").arg(metersPhrase(qRound(m_state->relativeAltitudeM())));
    if (m_state->hasVfr())
        parts << QStringLiteral("Скорость %1").arg(speedPhrase(qRound(m_state->groundSpeedMps())));
    if (m_state->hasBattery() && m_state->batteryPercent() >= 0)
        parts << QStringLiteral("Заряд батареи %1").arg(percentPhrase(m_state->batteryPercent()));

    const QString phrase = parts.isEmpty()
                               ? QStringLiteral("Телеметрия ещё не поступала")
                               : parts.join(QStringLiteral(". ")) + QStringLiteral(".");
    tryAnnounce(QStringLiteral("status"), m_cfg.antispamStatusHotkeySec, phrase,
                tts::PriorityImmediate, QStringLiteral("статус по запросу"));
}

} // namespace gcs
