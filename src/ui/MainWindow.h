#pragma once

#include <QWidget>

#include "config/AppConfig.h"

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

namespace gcs {

class VehicleState;

// Минимальное окно станции: текущая телеметрия, кнопка статуса,
// мьют и лог событий/озвученного. Хоткей статуса — из конфига (по умолчанию F2).
// При наличии трея закрытие окна сворачивает в трей (см. setCloseToTray).
class MainWindow : public QWidget
{
    Q_OBJECT
public:
    MainWindow(VehicleState *state, const AppConfig &cfg, QWidget *parent = nullptr);

    // true, если закрытие должно прятать окно, а не завершать приложение.
    void setCloseToTray(bool enabled) { m_closeToTray = enabled; }

signals:
    void statusRequested();
    void muteToggled(bool muted);

public slots:
    void appendLog(const QString &line);
    void appendSpeaking(const QString &phrase);
    void appendDropped(const QString &phrase);

private slots:
    void refresh();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    VehicleState *m_state = nullptr;
    AppConfig m_cfg;

    QLabel *m_modeLabel = nullptr;
    QLabel *m_armLabel = nullptr;
    QLabel *m_battLabel = nullptr;
    QLabel *m_altLabel = nullptr;
    QLabel *m_spdLabel = nullptr;
    QLabel *m_linkLabel = nullptr;
    QLabel *m_rxLabel = nullptr;
    QPushButton *m_statusBtn = nullptr;
    QPushButton *m_muteBtn = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QTimer *m_refreshTimer = nullptr;

    quint64 m_prevMessages = 0;
    double m_msgRate = 0.0;
    bool m_muted = false;
    bool m_closeToTray = false;
};

} // namespace gcs
