#include <QApplication>
#include <QCommandLineParser>
#include <QFile>

#include "app/Application.h"
#include "config/AppConfig.h"
#include "domain/Telemetry.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("mav-voice-gcs"));
    app.setOrganizationName(QStringLiteral("mav-voice-gcs"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

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

    return app.exec();
}
