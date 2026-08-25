#include "transport/UdpTransport.h"

#include <QNetworkDatagram>

namespace gcs {

UdpTransport::UdpTransport(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpTransport::onReadyRead);
}

bool UdpTransport::bind(quint16 port)
{
    if (!m_socket.bind(QHostAddress::AnyIPv4, port,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning("[transport] не удалось занять UDP-порт %u: %s",
                 unsigned(port), qPrintable(m_socket.errorString()));
        return false;
    }
    qInfo("[transport] слушаю UDP %u", unsigned(port));
    return true;
}

void UdpTransport::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        QNetworkDatagram dg = m_socket.receiveDatagram(int(m_socket.pendingDatagramSize()));
        if (dg.data().isEmpty())
            continue;
        m_peerAddr = dg.senderAddress();
        m_peerPort = quint16(dg.senderPort());
        m_hasPeer = true;
        emit datagramReceived(dg.data());
    }
}

bool UdpTransport::send(const QByteArray &data)
{
    if (!m_hasPeer)
        return false;
    const qint64 written = m_socket.writeDatagram(data, m_peerAddr, m_peerPort);
    return written == qint64(data.size());
}

} // namespace gcs
