#include <QtTest>

#include "tts/EspeakBackend.h"
#include "tts/ITtsBackend.h"
#include "tts/PiperBackend.h"
#include "tts/TtsQueue.h"

using namespace gcs;

namespace {

// Бэкенд-заглушка: имитирует асинхронное завершение говорения.
class DummyBackend : public ITtsBackend
{
    Q_OBJECT
public:
    using ITtsBackend::ITtsBackend;

    QString name() const override { return QStringLiteral("dummy"); }
    bool isReady() override { return true; }
    void speak(const QString &text) override
    {
        spoken << text;
        // Асинхронно, как реальный процесс: без реентерабельности pump().
        QTimer::singleShot(10, this, &ITtsBackend::finished);
    }

    QStringList spoken;
};

} // namespace

class TestTts : public QObject
{
    Q_OBJECT

private slots:
    void dedupSkipsIdenticalQueued();
    void dedupSkipsCurrentlySpeaking();
    void overflowDropsOldest();
    void muteDropsEverything();
    void piperNotReadyFailsGracefully();
};

void TestTts::dedupSkipsIdenticalQueued()
{
    DummyBackend be;
    TtsQueue q(16);
    q.setBackend(&be);

    // Первая фраза уходит в говорение сразу.
    q.enqueue(QStringLiteral("Режим полёта: лойтер"), tts::PriorityNormal);
    QCOMPARE(be.spoken.count(), 1);

    // Дубль стоящей в произнесении фразы отбрасывается…
    q.enqueue(QStringLiteral("Режим полёта: лойтер"), tts::PriorityNormal);
    // …и то же самое после того, как бэкенд освободился, но фраза ещё в очереди.
    q.enqueue(QStringLiteral("Критический заряд"), tts::PriorityCritical);
    q.enqueue(QStringLiteral("Критический заряд"), tts::PriorityCritical);
    QCOMPARE(q.queuedCount(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(be.spoken.count(), 2, 2000);
    QCOMPARE(be.spoken.at(1), QStringLiteral("Критический заряд"));
}

void TestTts::dedupSkipsCurrentlySpeaking()
{
    DummyBackend be;
    TtsQueue q(16);
    q.setBackend(&be);

    q.enqueue(QStringLiteral("Моторы запущены"), tts::PriorityNormal);
    QCOMPARE(be.spoken.count(), 1);
    q.enqueue(QStringLiteral("Моторы запущены"), tts::PriorityNormal);
    QCOMPARE(q.queuedCount(), 0);
}

void TestTts::overflowDropsOldest()
{
    DummyBackend be;
    TtsQueue q(4);
    q.setBackend(&be);

    QSignalSpy dropped(&q, &TtsQueue::phraseDropped);
    for (int i = 0; i < 10; ++i)
        q.enqueue(QStringLiteral("фраза %1").arg(i), tts::PriorityNormal);

    // 1 ушла в говорение, очередь ограничена 4, остальные выброшены.
    QVERIFY(dropped.count() >= 5);
    QVERIFY(q.queuedCount() <= 4);
}

void TestTts::muteDropsEverything()
{
    DummyBackend be;
    TtsQueue q(16);
    q.setBackend(&be);
    q.setMuted(true);
    q.enqueue(QStringLiteral("Режим"), tts::PriorityNormal);
    QCOMPARE(q.queuedCount(), 0);
    QCOMPARE(be.spoken.count(), 0);
}

void TestTts::piperNotReadyFailsGracefully()
{
    // Модель не задана -> бэкенд не готов, но контракт соблюдается:
    // speak() обязан выдать failed и finished, не уронив очередь.
    PiperBackend piper(QStringLiteral("piper"), QString(),
                       QStringLiteral("paplay"), 1.0);
    QVERIFY(!piper.isReady());

    TtsQueue q(16);
    q.setBackend(&piper);
    QSignalSpy failed(&piper, &ITtsBackend::failed);
    QSignalSpy started(&q, &TtsQueue::phraseStarted);

    q.enqueue(QStringLiteral("Проверка"), tts::PriorityNormal);
    QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 2000);
    QVERIFY(started.count() >= 1);
    // Очередь освободилась и готова к следующей фразе.
    QCOMPARE(q.queuedCount(), 0);
}

QTEST_MAIN(TestTts)
#include "tst_tts.moc"
