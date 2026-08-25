#pragma once

#include <QObject>
#include <QString>

class QFile;

namespace gcs {

// Запись телеметрии сессии в .tlog: перед каждым MAVLink-кадром 8 байт
// little-endian — микросекунды с Unix-эпохи. Формат совместим по духу
// с tlog MAVProxy (timestamp + кадр), файл можно читать mavlogdump'ом
// или собственным парсером.
class TlogWriter : public QObject
{
    Q_OBJECT
public:
    // nullptr, если каталог создать/файл открыть не удалось.
    static TlogWriter *create(const QString &dir, QObject *parent = nullptr);

    ~TlogWriter() override;

    QString filePath() const { return m_path; }
    quint64 framesWritten() const { return m_frames; }

public slots:
    void write(const QByteArray &frame);

private:
    explicit TlogWriter(const QString &path, QObject *parent);

    QFile *m_file = nullptr;
    QString m_path;
    quint64 m_frames = 0;
};

} // namespace gcs
