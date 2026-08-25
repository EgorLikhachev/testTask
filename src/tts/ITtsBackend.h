#pragma once

#include <QObject>
#include <QString>

namespace gcs {
namespace tts {

// Приоритет фразы: влияет на позицию в очереди и политику отбрасывания.
enum Priority {
    PriorityNormal = 0,    // обычные события
    PriorityCritical = 1,  // критические события — вперёд очереди
    PriorityImmediate = 2, // статус по хоткею — говорит следующим
};

} // namespace tts

// Абстракция синтеза речи. Живёт в рабочем потоке TtsQueue,
// поэтому все методы вызываются из этого потока.
class ITtsBackend : public QObject
{
    Q_OBJECT
public:
    explicit ITtsBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~ITtsBackend() override = default;

    virtual QString name() const = 0;
    virtual bool isReady() = 0;

    // Асинхронно: по завершении (или ошибке) обязан испустить finished().
    virtual void speak(const QString &text) = 0;

public slots:
    // Вызывается один раз в рабочем потоке после moveToThread — здесь
    // бэкенды создают свои QProcess/QTimer (аффинность потоков Qt).
    virtual void initInWorkerThread() {}

signals:
    void finished();
    void failed(const QString &error);
};

} // namespace gcs
