#include "mainwindow.h"
#include <QCoreApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("DataDrivenTestQT_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    MainWindow w;

    bool h264Enabled = false;
    bool phase4cAeroMach = false;
    double aeroAltitudeKm = 10.0;
    double aeroMach = 1.0;
    const QStringList arguments = a.arguments();
    for (const QString& argument : arguments)
    {
        const QString h264Prefix = QStringLiteral("--phase1d-h264=");
        if (argument.startsWith(h264Prefix))
        {
            h264Enabled = argument.mid(h264Prefix.size()).toInt() != 0;
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

    return a.exec();
}
