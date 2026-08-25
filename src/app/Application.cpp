#include "app/Application.h"

#include <QThread>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
extern "C" {
#include <ardupilotmega/mavlink.h>
}
#pragma GCC diagnostic pop

#include "domain/Telemetry.h"
#include "mavlink/MavlinkCommands.h"
#include "tts/EspeakBackend.h"

namespace gcs {

namespace {
// Период запрашиваемых сообщений, мкс (2 Гц).
constexpr qint32 kStreamIntervalUs = 500000;

// Сообщения, которых достаточно для всех сценариев ТЗ.
const quint16 kStreamedMessages[] = {
    MAVLINK_MSG_ID_HEARTBEAT,           // режим, arm/disarm
    MAVLINK_MSG_ID_SYS_STATUS,          // заряд батареи
    MAVLINK_MSG_ID_BATTERY_STATUS,      // заряд батареи (детально)
    MAVLINK_MSG_ID_GLOBAL_POSITION_INT, // высота
    MAVLINK_MSG_ID_VFR_HUD,             // скорость
    MAVLINK_MSG_ID_STATUSTEXT,          // события WARNING+
};
} // namespace

Application::Application(const AppConfig &cfg, QObject *parent)
    : QObject(parent)
    , m_cfg(cfg)
    , m_detector(BatteryThresholds{cfg.batteryWarnPercent,
                                   cfg.batteryCriticalPercent,
                                   cfg.batteryRecoverMarginPercent})
    , m_announcer(cfg)
{
}

Application::~Application()
{
    if (m_ttsThread) {
        m_ttsThread->quit();
        m_ttsThread->wait(5000);
    }
}

bool Application::start()
{
    if (!m_transport.bind(m_cfg.udpPort))
        return false;

    // Какой борт слушаем: явный sysid или авто-захват первого валидного.
    if (m_cfg.vehicleSysid != 0)
        m_parser.setTargetSysid(m_cfg.vehicleSysid);

    // Запись телеметрии сессии (.tlog), если включена.
    if (m_cfg.logEnabled) {
        m_tlog = TlogWriter::create(m_cfg.logDir, this);
        if (m_tlog)
            connect(&m_parser, &MavlinkParser::rawFrame,
                    m_tlog, &TlogWriter::write);
    }

    // transport -> parser
    connect(&m_transport, &UdpTransport::datagramReceived,
            this, [this](const QByteArray &bytes) { m_parser.feed(bytes); });

    // parser -> доменная модель (отображение)
    connect(&m_parser, &MavlinkParser::heartbeatReceived,
            &m_state, &VehicleState::onHeartbeat);
    connect(&m_parser, &MavlinkParser::batteryReceived,
            &m_state, &VehicleState::onBattery);
    connect(&m_parser, &MavlinkParser::positionReceived,
            &m_state, &VehicleState::onPosition);
    connect(&m_parser, &MavlinkParser::vfrHudReceived,
            &m_state, &VehicleState::onVfrHud);
    connect(&m_parser, &MavlinkParser::anyMessage,
            &m_state, &VehicleState::onAnyMessage);

    // parser -> детектор событий
    connect(&m_parser, &MavlinkParser::heartbeatReceived,
            &m_detector, &EventDetector::onHeartbeat);
    connect(&m_parser, &MavlinkParser::batteryReceived,
            &m_detector, &EventDetector::onBattery);
    connect(&m_parser, &MavlinkParser::statusTextReceived,
            &m_detector, &EventDetector::onStatusText);

    // Первый heartbeat от борта — запрашиваем детерминированный стрим.
    connect(&m_parser, &MavlinkParser::heartbeatReceived,
            this, &Application::onVehicleHeartbeat);

    // TTS: очередь в отдельном потоке, чтобы говорение не блокировало приём.
    m_ttsThread = new QThread(this);
    m_ttsQueue = new TtsQueue(m_cfg.ttsQueueLimit);
    const QString waveDir = qEnvironmentVariable("GCS_TTS_WAV_DIR");
    if (m_cfg.ttsBackend == QStringLiteral("piper")) {
        auto *piper = new PiperBackend(m_cfg.piperBin, m_cfg.piperModel,
                                       m_cfg.piperPlayCmd, m_cfg.piperLengthScale);
        if (!waveDir.isEmpty()) {
            piper->setWaveDir(waveDir);
            piper->setWaveKeep(m_cfg.ttsWavKeep);
        }
        m_backend = piper;
    } else {
        auto *espeak = new EspeakBackend(m_cfg.ttsProgram, m_cfg.ttsVoice,
                                         m_cfg.ttsSpeed);
        if (!waveDir.isEmpty()) {
            espeak->setWaveDir(waveDir);
            espeak->setWaveKeep(m_cfg.ttsWavKeep);
        }
        m_backend = espeak;
    }
    m_ttsQueue->setBackend(m_backend);
    m_ttsQueue->moveToThread(m_ttsThread);
    m_backend->moveToThread(m_ttsThread);
    // Создание QProcess/QTimer — уже в рабочем потоке.
    connect(m_ttsThread, &QThread::started,
            m_backend, &ITtsBackend::initInWorkerThread);
    connect(m_ttsThread, &QThread::finished, m_ttsQueue, &QObject::deleteLater);
    connect(m_ttsThread, &QThread::finished, m_backend, &QObject::deleteLater);
    m_ttsThread->start();

    // домен -> озвучка (антиспам внутри Announcer)
    m_announcer.setVehicleState(&m_state);
    connect(&m_detector, &EventDetector::flightModeChanged,
            &m_announcer, &Announcer::onFlightModeChanged);
    connect(&m_detector, &EventDetector::armedChanged,
            &m_announcer, &Announcer::onArmedChanged);
    connect(&m_detector, &EventDetector::batteryLevelChanged,
            &m_announcer, &Announcer::onBatteryLevel);
    connect(&m_detector, &EventDetector::statusWarning,
            &m_announcer, &Announcer::onStatusWarning);

    // Наблюдатель канала: озвучка установления/потери/восстановления связи.
    m_linkMonitor = new LinkMonitor(&m_state, m_cfg.linkLossSec * 1000, 1000, this);
    connect(m_linkMonitor, &LinkMonitor::linkEstablished,
            &m_announcer, &Announcer::onLinkEstablished);
    connect(m_linkMonitor, &LinkMonitor::linkLost,
            &m_announcer, &Announcer::onLinkLost);
    connect(m_linkMonitor, &LinkMonitor::linkRegained,
            &m_announcer, &Announcer::onLinkRegained);
    // После потери связи запрос стримов придётся повторять — борт мог
    // перезапуститься и потерять наши интервалы.
    connect(m_linkMonitor, &LinkMonitor::linkLost, this, [this]() {
        m_streamsRequested = false;
    });

    // озвучка -> очередь TTS (межпоточное соединение — автоматически queued)
    connect(&m_announcer, &Announcer::announce,
            m_ttsQueue, &TtsQueue::enqueue);

    // Наш heartbeat GCS: держит связь живой и даёт борту наш адрес.
    connect(&m_heartbeatTimer, &QTimer::timeout,
            this, &Application::sendGcsHeartbeat);
    m_heartbeatTimer.start(1000);

    qInfo("[app] запущен: UDP %s:%u, TTS: %s",
          qPrintable(m_cfg.udpHost), unsigned(m_cfg.udpPort),
          m_cfg.ttsEnabled ? "включён" : "выключен");
    return true;
}

void Application::sendGcsHeartbeat()
{
    m_transport.send(MavlinkCommands::heartbeat(m_cfg.sysid, m_cfg.compid));
}

void Application::onVehicleHeartbeat(HeartbeatInfo info)
{
    if (m_streamsRequested)
        return;
    m_streamsRequested = true;
    requestStreams(info.sysid, info.compid);
}

void Application::requestStreams(quint8 targetSysid, quint8 targetCompid)
{
    for (quint16 msgId : kStreamedMessages) {
        m_transport.send(MavlinkCommands::setMessageInterval(
            m_cfg.sysid, m_cfg.compid, targetSysid, targetCompid,
            msgId, kStreamIntervalUs));
    }
    qInfo("[app] запрошен стрим сообщений у борта %u/%u",
          unsigned(targetSysid), unsigned(targetCompid));
}

} // namespace gcs
