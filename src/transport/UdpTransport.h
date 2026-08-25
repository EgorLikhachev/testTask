#pragma once

#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>

namespace gcs {

// Транспортный слой: только байты по UDP, никакой семантики MAVLink.
// Пир (адрес отправителя) запоминается по первому входящему датаграмму,
// чтобы можно было отвечать (heartbeat, запросы потоков).
class UdpTransport : public QObject
{
    Q_OBJECT
public:
    explicit UdpTransport(QObject *parent = nullptr);

    bool bind(quint16 port);
    bool hasPeer() const { return m_hasPeer; }

    // Отправка пире (адресату, от которого приходили датаграммы).
    bool send(const QByteArray &data);

signals:
    void datagramReceived(const QByteArray &data);

private slots:
    void onReadyRead();

private:
    QUdpSocket m_socket;
    QHostAddress m_peerAddr;
    quint16 m_peerPort = 0;
    bool m_hasPeer = false;
};

} // namespace gcs
