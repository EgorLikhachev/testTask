#include "tts/EspeakBackend.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

namespace gcs {

EspeakBackend::EspeakBackend(const QString &program, const QString &voice, int speed,
                             QObject *parent)
    : ITtsBackend(parent)
    , m_program(program)
    , m_voice(voice)
    , m_speed(speed)
{
}

EspeakBackend::~EspeakBackend()
{
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
        delete m_proc;
    }
}

bool EspeakBackend::isReady()
{
    if (!m_checked) {
        m_checked = true;
        m_ready = !QStandardPaths::findExecutable(m_program).isEmpty();
        if (!m_ready)
            qWarning("[tts] программа '%s' не найдена в PATH",
                     qPrintable(m_program));
    }
    return m_ready;
}

// Отложенное создание QProcess/QTimer: объект переносится в рабочий поток
// после конструирования, поэтому создавать детей нужно уже в нём.
void EspeakBackend::initInWorkerThread()
{
    if (m_proc)
        return;

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished,
            this, &EspeakBackend::onProcessFinished);
    connect(m_proc, &QProcess::errorOccurred,
            this, &EspeakBackend::onProcessError);

    m_failSafeTimer = new QTimer(this);
    m_failSafeTimer->setSingleShot(true);
    m_failSafeTimer->setInterval(30000);
    connect(m_failSafeTimer, &QTimer::timeout, this, [this]() {
        qWarning("[tts] espeak-ng завис, завершаю принудительно");
        m_proc->kill();
    });
    qInfo("[tts] бэкенд %s инициализирован в рабочем потоке", qPrintable(name()));
}

void EspeakBackend::speak(const QString &text)
{
    initInWorkerThread();

    if (!isReady()) {
        emit failed(QStringLiteral("TTS-программа недоступна"));
        emit finished();
        return;
    }

    QStringList args;
    if (!m_waveDir.isEmpty()) {
        QDir().mkpath(m_waveDir);
        const QString file = QDir(m_waveDir).filePath(
            QStringLiteral("%1_%2.wav")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("hhmmss")))
                .arg(++m_waveCounter));
        args << QStringLiteral("-w") << file;
    }
    args << QStringLiteral("-v") << m_voice
         << QStringLiteral("-s") << QString::number(m_speed)
         << text;

    m_failSafeTimer->start();
    m_proc->start(m_program, args);
}

void EspeakBackend::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status);
    m_failSafeTimer->stop();
    if (exitCode != 0)
        qWarning("[tts] espeak-ng завершился с кодом %d", exitCode);
    emit finished();
}

void EspeakBackend::onProcessError(QProcess::ProcessError error)
{
    m_failSafeTimer->stop();
    qWarning("[tts] ошибка процесса espeak-ng: %d", int(error));
    emit failed(QStringLiteral("ошибка запуска espeak-ng"));
    emit finished();
}

} // namespace gcs
