#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    int autoSeconds = 0;
    bool h264Enabled = true;
	bool saveMP4Enabled = true;
    bool phase4cAeroMach = false;
    double aeroAltitudeKm = 10.0;
    double aeroMach = 1.0;
    int platID = -1;
    int sensorID = -1;
    int simMode = -1;
    int videoFps = -1;
	QString networkConfigPath;
	QString channel;
    const QStringList arguments = a.arguments();
	for (int argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
    {
		const QString& argument = arguments.at(argumentIndex);
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
		const QString saveMP4Prefix = QStringLiteral("--save-mp4=");
		if (argument.startsWith(saveMP4Prefix))
		{
			saveMP4Enabled = argument.mid(saveMP4Prefix.size()).toInt() != 0;
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
        const QString platPrefix = QStringLiteral("--plat-id=");
        if (argument.startsWith(platPrefix))
        {
            platID = argument.mid(platPrefix.size()).toInt();
        }
        const QString sensorPrefix = QStringLiteral("--sensor-id=");
        if (argument.startsWith(sensorPrefix))
        {
            sensorID = argument.mid(sensorPrefix.size()).toInt();
        }
        const QString simModePrefix = QStringLiteral("--sim-mode=");
        if (argument.startsWith(simModePrefix))
        {
            const int requested = argument.mid(simModePrefix.size()).toInt();
            if (requested == 1 || requested == 2)
            {
                simMode = requested;
            }
        }
        const QString videoFpsPrefix = QStringLiteral("--video-fps=");
        if (argument.startsWith(videoFpsPrefix))
        {
            videoFps = qBound(1, argument.mid(videoFpsPrefix.size()).toInt(), 240);
        }
		const QString networkConfigPrefix = QStringLiteral("--network-config=");
		if (argument.startsWith(networkConfigPrefix))
		{
			networkConfigPath = argument.mid(networkConfigPrefix.size());
		}
		else if (argument == QStringLiteral("--network-config") && argumentIndex + 1 < arguments.size())
		{
			networkConfigPath = arguments.at(++argumentIndex);
		}
		const QString channelPrefix = QStringLiteral("--channel=");
		if (argument.startsWith(channelPrefix))
		{
			channel = argument.mid(channelPrefix.size()).trimmed().toLower();
		}
		else if (argument == QStringLiteral("--channel") && argumentIndex + 1 < arguments.size())
		{
			channel = arguments.at(++argumentIndex).trimmed().toLower();
		}
    }
    qInfo().noquote()
        << QStringLiteral("[ProtocolLayout] component=DataDrivenTestQT ControlP2cX1ObjTrackingCmd=%1 InitP2cObjectTrackingCmd=%2 DisplayC2cObjTrackingData=%3 InitAckC2pObjectTrackingCmd=%4")
            .arg(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd))
            .arg(sizeof(BYHWICD::InitP2cObjectTrackingCmd))
            .arg(sizeof(BYHWICD::DisplayC2cObjTrackingData))
            .arg(sizeof(BYHWICD::InitAckC2pObjectTrackingCmd));
	MainWindow w(networkConfigPath, channel);
	w.show();
    w.setH264EnabledForTest(h264Enabled);
	w.setSaveMP4EnabledForTest(saveMP4Enabled);
    w.configureProtocolForTest(platID, sensorID, simMode, videoFps);
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
