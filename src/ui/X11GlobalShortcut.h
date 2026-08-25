#pragma once

#include <QObject>

class QSocketNotifier;

namespace gcs {

// Глобальный хоткей X11 (работает и через XWayland). Отдельное соединение
// к X-серверу + XGrabKey на корневом окне; события читаются по готовности
// сокета (QSocketNotifier) и не проходят через Qt-очередь.
// Собирается только при найденном X11 (HAVE_X11 в CMake); иначе фабрика
// create() возвращает nullptr и приложение работает с оконным хоткеем.
class X11GlobalShortcut : public QObject
{
    Q_OBJECT
public:
    // Разбирает QKeySequence вида "F2", "Ctrl+Shift+S" (одна комбинация).
    // nullptr, если глобальный перехват невозможен (нет X11/DISPLAY).
    static X11GlobalShortcut *create(const QString &keys, QObject *parent = nullptr);

    ~X11GlobalShortcut() override;

signals:
    void activated();

private slots:
    void onReadable();

private:
    X11GlobalShortcut(void *display, int keycode, QObject *parent);

    void *m_dpy = nullptr; // Display* (скрыт, чтобы не тянуть Xlib в заголовок)
    int m_keycode = 0;
    QSocketNotifier *m_notifier = nullptr;
};

} // namespace gcs
