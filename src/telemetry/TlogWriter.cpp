#include "telemetry/TlogWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace gcs {

TlogWriter *TlogWriter::create(const QString &dir, QObject *parent)
{
    if (!QDir().mkpath(dir)) {
        qWarning("[tlog] не удалось создать каталог %s", qPrintable(dir));
        return nullptr;
    }
    const QString path = QDir(dir).filePath(
        QStringLiteral("tlog-%1.tlog")
            .arg(QDateTime::currentDateTime()
                     .toString(QStringLiteral("yyyyMMdd-hhmmsszzz"))));
    auto *w = new TlogWriter(path, parent);
    if (!w->m_file->open(QIODevice::WriteOnly)) {
        qWarning("[tlog] не удалось открыть %s: %s",
                 qPrintable(path), qPrintable(w->m_file->errorString()));
        delete w;
        return nullptr;
    }
    qInfo("[tlog] запись телеметрии: %s", qPrintable(path));
    return w;
}

TlogWriter::TlogWriter(const QString &path, QObject *parent)
    : QObject(parent)
    , m_file(new QFile(path, this))
    , m_path(path)
{
}

TlogWriter::~TlogWriter()
{
    if (m_file && m_file->isOpen())
        m_file->close();
}

void TlogWriter::write(const QByteArray &frame)
{
    if (!m_file->isOpen() || frame.isEmpty())
        return;
    const quint64 usec =
        quint64(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
    m_file->write(reinterpret_cast<const char *>(&usec), sizeof(usec));
    m_file->write(frame);
    m_frames++;
    if (m_frames % 512 == 0)
        m_file->flush();
}

} // namespace gcs
