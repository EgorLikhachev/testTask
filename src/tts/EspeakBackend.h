#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

#include "tts/ITtsBackend.h"

class QTimer;

namespace gcs {

// Синтез через внешний процесс espeak-ng (Linux).
// Поддерживает режим отладки без аудио: озвучка пишется в WAV-файлы
// (setWaveDir), полезно в WSL без звука.
class EspeakBackend : public ITtsBackend
{
    Q_OBJECT
public:
    EspeakBackend(const QString &program, const QString &voice, int speed,
                  QObject *parent = nullptr);
    ~EspeakBackend() override;

    QString name() const override { return QStringLiteral("espeak-ng"); }
    bool isReady() override;
    void speak(const QString &text) override;

    void setWaveDir(const QString &dir) { m_waveDir = dir; }

public slots:
    // Вызывается уже в рабочем потоке (после moveToThread).
    void initInWorkerThread();

private:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

    QString m_program;
    QString m_voice;
    int m_speed = 150;
    QString m_waveDir;
    bool m_ready = false;
    bool m_checked = false;
    QProcess *m_proc = nullptr;
    QTimer *m_failSafeTimer = nullptr;
    int m_waveCounter = 0;
};

} // namespace gcs
