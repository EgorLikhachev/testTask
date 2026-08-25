#pragma once

#include <QObject>
#include <QTimer>

#include "announce/Announcer.h"
#include "config/AppConfig.h"
#include "domain/EventDetector.h"
#include "domain/LinkMonitor.h"
#include "domain/VehicleState.h"
#include "mavlink/MavlinkParser.h"
#include "telemetry/TlogWriter.h"
#include "transport/UdpTransport.h"
#include "tts/EspeakBackend.h"
#include "tts/ITtsBackend.h"
#include "tts/PiperBackend.h"
#include "tts/TtsQueue.h"

class QThread;

namespace gcs {

// Связывает слои: транспорт -> парсер -> доменная модель -> TTS.
// Живёт в главном потоке; TTS-очередь с бэкендом — в отдельном потоке.
// UI сюда не входит: окно создаёт main() и подключается через аксессоры.
class Application : public QObject
{
    Q_OBJECT
public:
    explicit Application(const AppConfig &cfg, QObject *parent = nullptr);
    ~Application() override;

    bool start();

    const AppConfig &config() const { return m_cfg; }
    VehicleState *vehicleState() { return &m_state; }
    Announcer *announcer() { return &m_announcer; }
    TtsQueue *ttsQueue() { return m_ttsQueue; }

private slots:
    void sendGcsHeartbeat();
    void onVehicleHeartbeat(gcs::HeartbeatInfo info);

private:
    void requestStreams(quint8 targetSysid, quint8 targetCompid);

    AppConfig m_cfg;
    UdpTransport m_transport;
    MavlinkParser m_parser;
    VehicleState m_state;
    EventDetector m_detector;
    Announcer m_announcer;
    LinkMonitor *m_linkMonitor = nullptr;
    TlogWriter *m_tlog = nullptr;

    QThread *m_ttsThread = nullptr;
    TtsQueue *m_ttsQueue = nullptr;
    ITtsBackend *m_backend = nullptr;

    QTimer m_heartbeatTimer;
    bool m_streamsRequested = false;
};

} // namespace gcs
