#include "ui/MainWindow.h"

#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include "domain/VehicleState.h"

namespace gcs {

MainWindow::MainWindow(VehicleState *state, const AppConfig &cfg, QWidget *parent)
    : QWidget(parent)
    , m_state(state)
    , m_cfg(cfg)
{
    setWindowTitle(tr("mav-voice-gcs — голосовая станция БПЛА"));
    setMinimumWidth(520);

    auto *modeVal = new QLabel(QStringLiteral("—"));
    auto *armVal = new QLabel(QStringLiteral("—"));
    auto *battVal = new QLabel(QStringLiteral("—"));
    auto *altVal = new QLabel(QStringLiteral("—"));
    auto *spdVal = new QLabel(QStringLiteral("—"));
    m_modeLabel = modeVal;
    m_armLabel = armVal;
    m_battLabel = battVal;
    m_altLabel = altVal;
    m_spdLabel = spdVal;

    auto *linkVal = new QLabel(QStringLiteral("НЕТ СВЯЗИ"));
    auto *rxVal = new QLabel(QStringLiteral("0 сообщ/с"));
    m_linkLabel = linkVal;
    m_rxLabel = rxVal;

    auto *form = new QFormLayout;
    form->addRow(tr("Режим полёта:"), modeVal);
    form->addRow(tr("Состояние:"), armVal);
    form->addRow(tr("Батарея:"), battVal);
    form->addRow(tr("Высота (отн.):"), altVal);
    form->addRow(tr("Скорость:"), spdVal);
    form->addRow(tr("Канал:"), linkVal);
    form->addRow(tr("Приём:"), rxVal);

    m_statusBtn = new QPushButton(
        tr("Статус (%1)").arg(QKeySequence(m_cfg.statusHotkey).toString()));
    connect(m_statusBtn, &QPushButton::clicked,
            this, &MainWindow::statusRequested);

    m_muteBtn = new QPushButton(tr("Озвучка: вкл"));
    m_muteBtn->setCheckable(true);
    connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
        m_muted = on;
        m_muteBtn->setText(on ? tr("Озвучка: выкл") : tr("Озвучка: вкл"));
        emit muteToggled(on);
    });

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_statusBtn);
    buttons->addWidget(m_muteBtn);
    buttons->addStretch();

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);

    auto *root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addLayout(buttons);
    root->addWidget(m_log, 1);

    QKeySequence hotkey(m_cfg.statusHotkey);
    if (!hotkey.isEmpty()) {
        auto *sc = new QShortcut(hotkey, this);
        connect(sc, &QShortcut::activated,
                this, &MainWindow::statusRequested);
    } else {
        qWarning("[ui] некорректный хоткей '%s', статус — только кнопкой",
                 qPrintable(m_cfg.statusHotkey));
    }

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::refresh);
    m_refreshTimer->start();
    refresh();
}

void MainWindow::refresh()
{
    if (!m_state)
        return;

    m_modeLabel->setText(m_state->modeName());

    if (m_state->armed()) {
        m_armLabel->setText(tr("ARMED"));
        m_armLabel->setStyleSheet(QStringLiteral("color: #b00020; font-weight: bold;"));
    } else {
        m_armLabel->setText(tr("DISARMED"));
        m_armLabel->setStyleSheet(QStringLiteral("color: #2e7d32;"));
    }

    if (m_state->hasBattery() && m_state->batteryPercent() >= 0) {
        const int pct = m_state->batteryPercent();
        QString text = tr("%1 %").arg(pct);
        if (m_state->batteryVoltage() > 0)
            text += tr(" (%1 В)").arg(m_state->batteryVoltage(), 0, 'f', 1);
        m_battLabel->setText(text);
        m_battLabel->setStyleSheet(
            pct <= m_cfg.batteryCriticalPercent
                ? QStringLiteral("color: #b00020; font-weight: bold;")
                : (pct <= m_cfg.batteryWarnPercent
                       ? QStringLiteral("color: #e65100;")
                       : QString()));
    } else {
        m_battLabel->setText(tr("—"));
        m_battLabel->setStyleSheet(QString());
    }

    m_altLabel->setText(m_state->hasPosition()
                            ? tr("%1 м").arg(m_state->relativeAltitudeM(), 0, 'f', 1)
                            : tr("—"));
    m_spdLabel->setText(m_state->hasVfr()
                            ? tr("%1 м/с").arg(m_state->groundSpeedMps(), 0, 'f', 1)
                            : tr("—"));

    const quint64 now = m_state->totalMessages();
    const double inst = (now - m_prevMessages) / 0.5; // тик — 0.5 с
    m_prevMessages = now;
    m_msgRate = 0.7 * m_msgRate + 0.3 * inst; // сглаживание
    m_rxLabel->setText(tr("%1 сообщ/с, ошибок парсинга: %2")
                           .arg(QString::number(m_msgRate, 'f', 0))
                           .arg(m_state->parseErrors()));

    const bool alive = m_state->linkAlive();
    m_linkLabel->setText(alive ? tr("СВЯЗЬ ЕСТЬ") : tr("НЕТ СВЯЗИ"));
    m_linkLabel->setStyleSheet(alive
                                   ? QStringLiteral("color: #2e7d32; font-weight: bold;")
                                   : QStringLiteral("color: #b00020; font-weight: bold;"));
}

void MainWindow::appendLog(const QString &line)
{
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"));
    m_log->appendPlainText(QStringLiteral("[%1] %2").arg(stamp, line));
}

void MainWindow::appendSpeaking(const QString &phrase)
{
    appendLog(tr("▶ %1").arg(phrase));
}

void MainWindow::appendDropped(const QString &phrase)
{
    appendLog(tr("✕ отброшено: %1").arg(phrase));
}

} // namespace gcs
