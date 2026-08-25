#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>

namespace gcs {

// Ограничитель частоты повторов: одно и то же событие (ключ) проходит
// не чаще, чем раз в заданный интервал.
class AntiSpamFilter
{
public:
    AntiSpamFilter() { m_clock.start(); }

    bool allow(const QString &key, int minIntervalSec)
    {
        return allowMs(key, qint64(minIntervalSec) * 1000, m_clock.elapsed());
    }

    // Вариант с инъекцией времени для тестов.
    bool allowMs(const QString &key, qint64 minIntervalMs, qint64 nowMs)
    {
        const auto it = m_lastAllowedMs.constFind(key);
        if (it != m_lastAllowedMs.constEnd() && nowMs - it.value() < minIntervalMs)
            return false;
        m_lastAllowedMs.insert(key, nowMs);
        return true;
    }

    void reset() { m_lastAllowedMs.clear(); }

private:
    QHash<QString, qint64> m_lastAllowedMs;
    QElapsedTimer m_clock;
};

} // namespace gcs
