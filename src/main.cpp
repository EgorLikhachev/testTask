#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QTranslator>

#include "app/Application.h"
#include "config/AppConfig.h"
#include "domain/Telemetry.h"
#include "ui/MainWindow.h"
#include "ui/SystemTray.h"
#include "ui/X11GlobalShortcut.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("mav-voice-gcs"));
    app.setOrganizationName(QStringLiteral("mav-voice-gcs"));
    app.setApplicationVersion(QStringLiteral("0.2.0"));

    // Английская локаль получает перевод (исходный язык — русский).
    QTranslator translator;
    const QString qmDir =
        QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
    if (translator.load(QLocale::system(), QStringLiteral("mav-voice-gcs"),
                        QStringLiteral("_"), qmDir))
        app.installTranslator(&translator);

    qSetMessagePattern(QStringLiteral("[%{time hh:mm:ss.zzz} %{type}] %{message}"));

    // Метатипы — на случай межпоточных соединений с DTO телеметрии.
    qRegisterMetaType<gcs::HeartbeatInfo>();
    qRegisterMetaType<gcs::BatteryInfo>();
    qRegisterMetaType<gcs::PositionInfo>();
    qRegisterMetaType<gcs::VfrHudInfo>();
    qRegisterMetaType<gcs::StatusTextInfo>();

    QCommandLineParser cli;
    cli.setApplicationDescription(
        QStringLiteral("Приёмник MAVLink-телеметрии с голосовым сопровождением"));
    QCommandLineOption cfgOpt(
        QStringLiteral("config"),
        QStringLiteral("путь к ini-конфигу"),
        QStringLiteral("path"));
    cli.addOption(cfgOpt);
    cli.addHelpOption();
    cli.process(app);

    QString cfgPath = cli.value(cfgOpt);
    if (cfgPath.isEmpty()) {
        const QStringList candidates = {
            QCoreApplication::applicationDirPath()
                + QStringLiteral("/gcs-tts.ini"),
            QCoreApplication::applicationDirPath()
                + QStringLiteral("/../config/gcs-tts.ini"),
            QCoreApplication::applicationDirPath()
                + QStringLiteral("/../../config/gcs-tts.ini"),
            // AppImage: usr/share/mav-voice-gcs/gcs-tts.ini
            QCoreApplication::applicationDirPath()
                + QStringLiteral("/../share/mav-voice-gcs/gcs-tts.ini"),
        };
        for (const QString &c : candidates) {
            if (QFile::exists(c)) {
                cfgPath = c;
                break;
            }
        }
    }

    gcs::AppConfig cfg;
    if (!cfgPath.isEmpty() && QFile::exists(cfgPath)) {
        cfg = gcs::AppConfig::load(cfgPath);
        qInfo("[main] конфигурация: %s", qPrintable(cfgPath));
    } else {
        qWarning("[main] конфиг не найден, используются значения по умолчанию");
    }

    gcs::Application gcsApp(cfg);
    if (!gcsApp.start())
        return 1;

    // UI живёт здесь: ядро (Application) от интерфейса не зависит.
    auto *window = new gcs::MainWindow(gcsApp.vehicleState(), cfg);
    QObject::connect(gcsApp.announcer(), &gcs::Announcer::eventLogged,
                     window, &gcs::MainWindow::appendLog);
    QObject::connect(gcsApp.ttsQueue(), &gcs::TtsQueue::phraseStarted,
                     window, &gcs::MainWindow::appendSpeaking);
    QObject::connect(gcsApp.ttsQueue(), &gcs::TtsQueue::phraseDropped,
                     window, &gcs::MainWindow::appendDropped);
    QObject::connect(window, &gcs::MainWindow::statusRequested,
                     gcsApp.announcer(), &gcs::Announcer::onStatusRequested);
    QObject::connect(window, &gcs::MainWindow::muteToggled,
                     gcsApp.ttsQueue(), &gcs::TtsQueue::setMuted);
    window->show();

    // Глобальный хоткей (X11/XWayland): статус даже когда окно не в фокусе.
    if (cfg.hotkeyGlobal) {
        auto *global = gcs::X11GlobalShortcut::create(cfg.statusHotkey, &gcsApp);
        if (global)
            QObject::connect(global, &gcs::X11GlobalShortcut::activated,
                             gcsApp.announcer(),
                             &gcs::Announcer::onStatusRequested);
    }

    // Трей: показать/скрыть, статус, мьют, выход; крест окна прячет в трей.
    if (auto *tray = gcs::SystemTray::create(window, &app)) {
        window->setCloseToTray(cfg.uiHideOnClose);
        QObject::connect(tray, &gcs::SystemTray::statusRequested,
                         gcsApp.announcer(), &gcs::Announcer::onStatusRequested);
        QObject::connect(tray, &gcs::SystemTray::quitRequested,
                         &app, &QCoreApplication::quit);
        QObject::connect(tray, &gcs::SystemTray::muteToggled,
                         gcsApp.ttsQueue(), &gcs::TtsQueue::setMuted);
        // Кнопка окна держит чекбокс трея в согласии (без эха).
        QObject::connect(window, &gcs::MainWindow::muteToggled,
                         tray, &gcs::SystemTray::setMuted);
    }

    return app.exec();
}
