#include "tts/TtsQueue.h"

#include <utility>

namespace gcs {

TtsQueue::TtsQueue(int queueLimit, QObject *parent)
    : QObject(parent)
    , m_limit(qMax(2, queueLimit))
{
}

void TtsQueue::setBackend(ITtsBackend *backend)
{
    m_backend = backend;
    if (m_backend)
        connect(m_backend, &ITtsBackend::finished,
                this, &TtsQueue::onBackendFinished);
}

void TtsQueue::enqueue(const QString &phrase, int priority)
{
    const QString text = phrase.trimmed();
    if (text.isEmpty())
        return;

    if (m_muted) {
        emit phraseDropped(text);
        return;
    }

    if (priority >= tts::PriorityImmediate || priority == tts::PriorityCritical)
        m_queue.prepend(Item{text, priority});
    else
        m_queue.append(Item{text, priority});

    // Переполнение: выбрасываем самую старую некритичную фразу,
    // иначе — самую старую.
    while (m_queue.size() > m_limit) {
        int victim = -1;
        for (int i = 0; i < m_queue.size(); ++i) {
            if (m_queue.at(i).priority == tts::PriorityNormal) {
                victim = i;
                break;
            }
        }
        if (victim < 0)
            victim = 0;
        emit phraseDropped(m_queue.takeAt(victim).text);
    }

    emit queueChanged(m_queue.size());
    pump();
}

void TtsQueue::setMuted(bool muted)
{
    m_muted = muted;
    if (m_muted)
        clear();
}

void TtsQueue::clear()
{
    if (m_queue.isEmpty())
        return;
    for (const Item &it : std::as_const(m_queue))
        emit phraseDropped(it.text);
    m_queue.clear();
    emit queueChanged(0);
}

void TtsQueue::pump()
{
    if (m_busy || m_queue.isEmpty() || !m_backend)
        return;
    m_busy = true;
    const Item it = m_queue.dequeue();
    emit queueChanged(m_queue.size());
    qInfo("[tts] говорю: %s", qPrintable(it.text));
    emit phraseStarted(it.text);
    m_backend->speak(it.text);
}

void TtsQueue::onBackendFinished()
{
    m_busy = false;
    pump();
}

} // namespace gcs
