#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    int autoSeconds = 0;
    bool h264Enabled = false;
    bool phase4cAeroMach = false;
    double aeroAltitudeKm = 10.0;
    double aeroMach = 1.0;
    const QStringList arguments = a.arguments();
    for (const QString& argument : arguments)
    {
        const QString prefix = QStringLiteral("--phase1b-auto-seconds=");
        if (argument.startsWith(prefix))
        {
            autoSeconds = qBound(1, argument.mid(prefix.size()).toInt(), 3600);
        }
        const QString h264Prefix = QStringLiteral("--phase1d-h264=");
        if (argument.startsWith(h264Prefix))
        {
            h264Enabled = argument.mid(h264Prefix.size()).toInt() != 0;
        }
        const QString durationPrefix = QStringLiteral("--duration-sec=");
        if (argument.startsWith(durationPrefix))
        {
            autoSeconds = qBound(1, argument.mid(durationPrefix.size()).toInt(), 3600);
        }
        if (argument == QStringLiteral("--phase4c-aero-mach"))
        {
            phase4cAeroMach = true;
        }
        const QString aeroAltPrefix = QStringLiteral("--aero-alt-km=");
        if (argument.startsWith(aeroAltPrefix))
        {
            bool ok = false;
            const double value = argument.mid(aeroAltPrefix.size()).toDouble(&ok);
            if (ok)
            {
                aeroAltitudeKm = qBound(0.0, value, 20.0);
            }
        }
        const QString aeroMachPrefix = QStringLiteral("--aero-mach=");
        if (argument.startsWith(aeroMachPrefix))
        {
            bool ok = false;
            const double value = argument.mid(aeroMachPrefix.size()).toDouble(&ok);
            if (ok)
            {
                aeroMach = qBound(0.0, value, 4.0);
            }
        }
    }
    w.setH264EnabledForTest(h264Enabled);
    w.configurePhase4cAeroMachTest(phase4cAeroMach, aeroAltitudeKm, aeroMach);
    if (autoSeconds > 0)
    {
        QTimer::singleShot(500, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onInitButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6000, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onStartButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6000 + autoSeconds * 1000, &w, [&w]() {
            QMetaObject::invokeMethod(&w, "onStopButtonClicked", Qt::DirectConnection);
        });
        QTimer::singleShot(6500 + autoSeconds * 1000, &a, &QApplication::quit);
    }
    return a.exec();
}
