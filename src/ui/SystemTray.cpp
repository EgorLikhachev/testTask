#include "ui/SystemTray.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

#include "ui/MainWindow.h"

namespace gcs {

SystemTray *SystemTray::create(MainWindow *window, QObject *parent)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qInfo("[tray] системный трей недоступен — сворачивание в трей выключено");
        return nullptr;
    }
    return new SystemTray(window, parent);
}

SystemTray::SystemTray(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    m_menu = new QMenu(m_window);

    QAction *showAction = m_menu->addAction(tr("Показать/скрыть окно"));
    connect(showAction, &QAction::triggered, this, [this]() {
        if (m_window->isVisible())
            m_window->hide();
        else
            m_window->showNormal();
    });

    QAction *statusAction = m_menu->addAction(tr("Статус"));
    connect(statusAction, &QAction::triggered,
            this, &SystemTray::statusRequested);

    m_muteAction = m_menu->addAction(tr("Озвучка: вкл"));
    m_muteAction->setCheckable(true);
    connect(m_muteAction, &QAction::toggled, this, [this](bool on) {
        m_muteAction->setText(on ? tr("Озвучка: выкл") : tr("Озвучка: вкл"));
        emit muteToggled(on);
    });

    m_menu->addSeparator();
    QAction *quitAction = m_menu->addAction(tr("Выход"));
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitRequested);

    m_icon = new QSystemTrayIcon(
        QIcon(QStringLiteral(":/icons/mav-voice-gcs.png")), this);
    m_icon->setToolTip(QStringLiteral("mav-voice-gcs"));
    m_icon->setContextMenu(m_menu);

    // Одиночный клик по иконке — показать/скрыть окно.
    connect(m_icon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason != QSystemTrayIcon::Trigger)
                    return;
                if (m_window->isVisible())
                    m_window->hide();
                else
                    m_window->showNormal();
            });

    m_icon->show();
}

void SystemTray::setMuted(bool muted)
{
    // Синхронизация с кнопкой окна без обратного эха.
    m_muteAction->blockSignals(true);
    m_muteAction->setChecked(muted);
    m_muteAction->setText(muted ? tr("Озвучка: выкл") : tr("Озвучка: вкл"));
    m_muteAction->blockSignals(false);
}

} // namespace gcs
