#include "ui/X11GlobalShortcut.h"

#include <QKeySequence>
#include <QSocketNotifier>
#include <QTimer>

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

namespace gcs {

#ifdef HAVE_X11

X11GlobalShortcut *X11GlobalShortcut::create(const QString &keys, QObject *parent)
{
    const QKeySequence seq(keys);
    if (seq.isEmpty() || seq.count() != 1) {
        qWarning("[hotkey] некорректная комбинация '%s'", qPrintable(keys));
        return nullptr;
    }

    Display *dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        qInfo("[hotkey] X11 недоступен (нет DISPLAY или Wayland без XWayland) — "
              "глобальный хоткей выключен, работает оконный");
        return nullptr;
    }

    // Имя клавиши — последний элемент последовательности ("Ctrl+Shift+S" -> "s").
    const QString name = seq.toString().section(QLatin1Char('+'), -1).toLower();

    const KeySym sym = XStringToKeysym(name.toLatin1().constData());
    if (sym == NoSymbol) {
        qWarning("[hotkey] неизвестная клавиша '%s'", qPrintable(name));
        XCloseDisplay(dpy);
        return nullptr;
    }
    const KeyCode keycode = XKeysymToKeycode(dpy, sym);
    if (keycode == 0) {
        qWarning("[hotkey] клавиша '%s' отсутствует на этой клавиатуре",
                 qPrintable(name));
        XCloseDisplay(dpy);
        return nullptr;
    }

    // AnyModifier: перехват с любым модификатором (не ломается от NumLock),
    // зато комбинации с Ctrl/Alt/Shift тоже попадут под перехват.
    const Window root = DefaultRootWindow(dpy);
    const int grab_result = XGrabKey(dpy, keycode, AnyModifier, root,
                                     False, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
    if (grab_result == BadAccess) {
        qWarning("[hotkey] комбинация '%s' уже перехвачена другим приложением",
                 qPrintable(keys));
        XCloseDisplay(dpy);
        return nullptr;
    }

    qInfo("[hotkey] глобальный перехват активен: %s", qPrintable(keys));
    return new X11GlobalShortcut(dpy, keycode, parent);
}

X11GlobalShortcut::X11GlobalShortcut(void *display, int keycode, QObject *parent)
    : QObject(parent)
    , m_dpy(display)
    , m_keycode(keycode)
{
    // Событие перехваченной клавиши приходит в НАШ коннект — слушаем сокет.
    const int fd = ConnectionNumber(static_cast<Display *>(m_dpy));
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &X11GlobalShortcut::onReadable);
}

X11GlobalShortcut::~X11GlobalShortcut()
{
    Display *dpy = static_cast<Display *>(m_dpy);
    XUngrabKey(dpy, m_keycode, AnyModifier, DefaultRootWindow(dpy));
    XSync(dpy, False);
    XCloseDisplay(dpy);
}

void X11GlobalShortcut::onReadable()
{
    Display *dpy = static_cast<Display *>(m_dpy);
    while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        if (ev.type != KeyPress || ev.xkey.keycode != m_keycode)
            continue;
        emit activated();
    }
}

#else // !HAVE_X11

X11GlobalShortcut *X11GlobalShortcut::create(const QString &, QObject *)
{
    qInfo("[hotkey] сборка без X11 — глобальный хоткей недоступен");
    return nullptr;
}

X11GlobalShortcut::~X11GlobalShortcut() = default;

#endif

} // namespace gcs
