#include "tts/PiperBackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QQueue>
#include <QStandardPaths>
#include <QTimer>

namespace gcs {

namespace {
constexpr int kFailSafeMs = 30000;
}

PiperBackend::PiperBackend(const QString &bin, const QString &model,
                           const QString &playCmd, double lengthScale,
                           QObject *parent)
    : ITtsBackend(parent)
    , m_bin(bin)
    , m_model(model)
    , m_playCmd(playCmd)
    , m_lengthScale(lengthScale)
{
}

PiperBackend::~PiperBackend()
{
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

void PiperBackend::initInWorkerThread()
{
    if (m_proc)
        return;

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished,
            this, [this](int exitCode, QProcess::ExitStatus status) {
                Q_UNUSED(status);
                m_failSafeTimer->stop();
                if (m_stage == Stage::Play) {
                    if (exitCode != 0)
                        qWarning("[tts/piper] воспроизведение: код %d", exitCode);
                    finishCycle();
                    return;
                }
                if (m_stage != Stage::Synth) {
                    finishCycle();
                    return;
                }
                // Стадия 1 (синтез) завершена — дальше либо воспроизведение,
                // либо конец цикла (режим WAV).
                if (exitCode != 0) {
                    qWarning("[tts/piper] синтез завершился с кодом %d", exitCode);
                    emit failed(QStringLiteral("piper: ошибка синтеза"));
                    finishCycle();
                } else if (m_waveModeFile) {
                    qInfo("[tts/piper] wav готов: %s", qPrintable(m_currentWav));
                    finishCycle();
                } else {
                    startPlayback(m_currentWav);
                }
            });
    connect(m_proc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                Q_UNUSED(error);
                m_failSafeTimer->stop();
                qWarning("[tts/piper] ошибка процесса: %d", int(error));
                emit failed(QStringLiteral("piper: ошибка процесса"));
                finishCycle();
            });

    m_failSafeTimer = new QTimer(this);
    m_failSafeTimer->setSingleShot(true);
    m_failSafeTimer->setInterval(kFailSafeMs);
    connect(m_failSafeTimer, &QTimer::timeout, this, [this]() {
        qWarning("[tts/piper] процесс завис, завершаю принудительно");
        m_proc->kill();
    });
}

bool PiperBackend::isReady()
{
    if (!m_checked) {
        m_checked = true;
        if (m_model.isEmpty()) {
            qWarning("[tts/piper] не задан tts/piper_model — см. scripts/setup_piper.sh");
        } else if (!QFile::exists(m_model)) {
            qWarning("[tts/piper] модель не найдена: %s", qPrintable(m_model));
        } else if (QStandardPaths::findExecutable(m_bin).isEmpty()) {
            qWarning("[tts/piper] программа '%s' не найдена в PATH", qPrintable(m_bin));
        } else {
            m_ready = true;
        }
    }
    return m_ready;
}

void PiperBackend::speak(const QString &text)
{
    initInWorkerThread();

    if (!isReady()) {
        emit failed(QStringLiteral("piper недоступен"));
        finishCycle();
        return;
    }

    if (!m_waveDir.isEmpty()) {
        QDir().mkpath(m_waveDir);
        m_currentWav = QDir(m_waveDir).filePath(
            QStringLiteral("%1_%2.wav")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("hhmmss")))
                .arg(++m_waveCounter));
        m_waveModeFile = true;
        if (m_waveKeep > 0) {
            m_keptWav.enqueue(m_currentWav);
            while (m_keptWav.size() > m_waveKeep)
                QFile::remove(m_keptWav.dequeue());
        }
    } else {
        m_currentWav = QDir::temp().filePath(
            QStringLiteral("gcs-piper-%1.wav")
                .arg(QDateTime::currentDateTime()
                         .toString(QStringLiteral("hhmmsszzz"))));
        m_waveModeFile = false;
    }
    startSynthesis(text);
}

void PiperBackend::startSynthesis(const QString &text)
{
    // piper читает текст из stdin; конфиг .onnx.json лежит рядом с моделью.
    m_stage = Stage::Synth;
    m_proc->setProgram(m_bin);
    m_proc->setArguments({
        QStringLiteral("--model"), m_model,
        QStringLiteral("--length_scale"), QString::number(m_lengthScale),
        QStringLiteral("--output_file"), m_currentWav,
    });
    m_failSafeTimer->start();
    m_proc->start();
    m_proc->write(text.toUtf8() + '\n');
    m_proc->closeWriteChannel();
}

void PiperBackend::startPlayback(const QString &wav)
{
    // Команда воспроизведения может содержать аргументы ("pw-play -q").
    QStringList parts = m_playCmd.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        parts << QStringLiteral("paplay");
    const QString prog = parts.takeFirst();
    parts << wav;

    m_stage = Stage::Play;
    m_failSafeTimer->start();
    m_proc->setProgram(prog);
    m_proc->setArguments(parts);
    m_proc->start();
}

void PiperBackend::finishCycle()
{
    if (!m_waveModeFile && !m_currentWav.isEmpty())
        QFile::remove(m_currentWav); // временный файл больше не нужен
    m_currentWav.clear();
    m_stage = Stage::Idle;
    emit finished();
}

} // namespace gcs
