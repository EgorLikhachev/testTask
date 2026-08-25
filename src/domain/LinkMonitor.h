#pragma once

#include <QObject>
#include <QTimer>

namespace gcs {

class VehicleState;

// Наблюдатель канала связи: опрашивает VehicleState по таймеру и превращает
// «живость» потока в события-фронты. До первого сообщения молчит.
class LinkMonitor : public QObject
{
    Q_OBJECT
public:
    // lossMs — окно отсутствия сообщений, после которого связь считается
    // потерянной; checkMs — период опроса.
    LinkMonitor(VehicleState *state, int lossMs, int checkMs = 1000,
                QObject *parent = nullptr);

signals:
    void linkEstablished(); // первый трафик после запуска
    void linkLost();        // сообщения пропали дольше окна
    void linkRegained();    // трафик вернулся после потери

private slots:
    void check();

private:
    enum class St { Idle, Up, Down };

    VehicleState *m_state = nullptr;
    int m_lossMs = 4000;
    St m_st = St::Idle;
    QTimer m_timer;
};

} // namespace gcs
