#pragma once

#include <QObject>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace gcs {

class MainWindow;

// Иконка в системном трее: показать/скрыть окно, статус-спич, мьют, выход.
// Создаётся только если трей доступен (isAvailable).
class SystemTray : public QObject
{
    Q_OBJECT
public:
    // nullptr, если системный трей недоступен.
    static SystemTray *create(MainWindow *window, QObject *parent = nullptr);

signals:
    void statusRequested();
    void quitRequested();
    void muteToggled(bool muted); // согласовано с кнопкой окна

public slots:
    void setMuted(bool muted);

private:
    explicit SystemTray(MainWindow *window, QObject *parent);

    MainWindow *m_window = nullptr;
    QSystemTrayIcon *m_icon = nullptr;
    QMenu *m_menu = nullptr;
    QAction *m_muteAction = nullptr;
};

} // namespace gcs
