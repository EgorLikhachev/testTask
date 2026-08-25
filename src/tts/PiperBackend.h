#pragma once

#include <QProcess>
#include <QQueue>
#include <QString>

#include "tts/ITtsBackend.h"

class QTimer;

namespace gcs {

// Нейросетевой синтез через внешний процесс piper (rhasspy/piper).
// Конвейер из двух стадий: piper --model X --output_file F (текст в stdin),
// затем воспроизведение F командой piperPlayCmd (по умолчанию paplay).
// В режиме GCS_TTS_WAV_DIR файл пишется в каталог отладки и не
// воспроизводится — как у EspeakBackend.
class PiperBackend : public ITtsBackend
{
    Q_OBJECT
public:
    PiperBackend(const QString &bin, const QString &model,
                 const QString &playCmd, double lengthScale,
                 QObject *parent = nullptr);
    ~PiperBackend() override;

    QString name() const override { return QStringLiteral("piper"); }
    bool isReady() override;
    void speak(const QString &text) override;

public slots:
    // Вызывается уже в рабочем потоке (после moveToThread).
    void initInWorkerThread() override;

    void setWaveDir(const QString &dir) { m_waveDir = dir; }
    void setWaveKeep(int keep) { m_waveKeep = qMax(0, keep); }

private:
    void startSynthesis(const QString &text);
    void startPlayback(const QString &wav);
    void finishCycle();

    QString m_bin;
    QString m_model;
    QString m_playCmd;
    double m_lengthScale = 1.0;
    QString m_waveDir;
    int m_waveKeep = 64;

    QProcess *m_proc = nullptr;
    QTimer *m_failSafeTimer = nullptr;
    QString m_currentWav;
    bool m_waveModeFile = false; // текущий wav — файл отладки, не удалять
    QQueue<QString> m_keptWav;
    int m_waveCounter = 0;
    bool m_ready = false;
    bool m_checked = false;

    enum class Stage { Idle, Synth, Play };
    Stage m_stage = Stage::Idle;
};

} // namespace gcs
