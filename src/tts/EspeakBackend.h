#pragma once

#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>

#include "tts/ITtsBackend.h"

class QTimer;

namespace gcs {

// Синтез через внешний процесс espeak-ng (Linux).
// Поддерживает режим отладки без аудио: озвучка пишется в WAV-файлы
// (setWaveDir), полезно в WSL без звука. Хранятся только последние
// wavKeep файлов (0 — без очистки).
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
    void setWaveKeep(int keep) { m_waveKeep = qMax(0, keep); }

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
    int m_waveKeep = 64;
    QQueue<QString> m_waveFiles; // журналируется только в режиме WAV
    bool m_ready = false;
    bool m_checked = false;
    QProcess *m_proc = nullptr;
    QTimer *m_failSafeTimer = nullptr;
    int m_waveCounter = 0;
};

} // namespace gcs
