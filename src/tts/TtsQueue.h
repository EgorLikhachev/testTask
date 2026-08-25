#pragma once

#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

#include "tts/ITtsBackend.h"

namespace gcs {

// Очередь озвучки. Объект переносится в отдельный QThread:
// говорение (секунды) никогда не блокирует приём телеметрии.
class TtsQueue : public QObject
{
    Q_OBJECT
public:
    explicit TtsQueue(int queueLimit = 16, QObject *parent = nullptr);

    // Бэкенд должен жить в том же потоке.
    void setBackend(ITtsBackend *backend);

    int queuedCount() const { return m_queue.size(); }
    bool isMuted() const { return m_muted; }

public slots:
    void enqueue(const QString &phrase, int priority);
    void setMuted(bool muted);
    void clear();

signals:
    void phraseStarted(const QString &phrase);
    void phraseDropped(const QString &phrase);
    void queueChanged(int queued);

private:
    void pump();
    void onBackendFinished();

    struct Item {
        QString text;
        int priority = 0;
    };
    QQueue<Item> m_queue;
    int m_limit = 16;
    bool m_busy = false;
    QString m_speaking; // фраза, произносимая прямо сейчас (для дедупликации)
    bool m_muted = false;
    ITtsBackend *m_backend = nullptr;
};

} // namespace gcs
