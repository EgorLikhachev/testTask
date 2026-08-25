#include "domain/LinkMonitor.h"

#include "domain/VehicleState.h"

namespace gcs {

LinkMonitor::LinkMonitor(VehicleState *state, int lossMs, int checkMs,
                         QObject *parent)
    : QObject(parent)
    , m_state(state)
    , m_lossMs(qMax(100, lossMs))
{
    m_timer.setInterval(qMax(50, checkMs));
    connect(&m_timer, &QTimer::timeout, this, &LinkMonitor::check);
    m_timer.start();
}

void LinkMonitor::check()
{
    if (!m_state)
        return;
    const bool up = m_state->linkAlive(m_lossMs);

    switch (m_st) {
    case St::Idle:
        if (up) {
            m_st = St::Up;
            emit linkEstablished();
        }
        break;
    case St::Up:
        if (!up) {
            m_st = St::Down;
            emit linkLost();
        }
        break;
    case St::Down:
        if (up) {
            m_st = St::Up;
            emit linkRegained();
        }
        break;
    }
}

} // namespace gcs
