#include "HwaSim_IR_VideoDisplay.h"
#include <QtWidgets/QApplication>
#include <QTextCodec>
#include <QtGlobal>
#include <QDebug>
#include <QTimer>
#include "CommonData.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5 运行时统一使用 UTF-8，避免中文界面和日志在不同系统代码页下乱码。
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
#endif
    QApplication app(argc, argv);
    QString networkConfigPath;
    QString channel;
    int platID = -1;
    int sensorID = -1;
	QString receiveTransport, streamRole, ddsTopic, ddsCodec, ddsQos, ddsDumpFirstFrame;
	int ddsDomain = -1, ddsWidth = -1, ddsHeight = -1, ddsFps = -1;
	int acceptanceExitMs = 0;
    const QStringList arguments = app.arguments();
    for (int argumentIndex = 0; argumentIndex < arguments.size(); ++argumentIndex)
    {
        const QString& argument = arguments.at(argumentIndex);
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
		auto stringOption = [&](const QString& name, QString& target) {
			const QString prefix = name + QStringLiteral("=");
			if (argument.startsWith(prefix)) { target = argument.mid(prefix.size()); return true; }
			if (argument == name && argumentIndex + 1 < arguments.size()) { target = arguments.at(++argumentIndex); return true; }
			return false;
		};
		auto intOption = [&](const QString& name, int& target) {
			QString value;
			if (!stringOption(name, value)) return false;
			target = value.toInt();
			return true;
		};
		if (stringOption(QStringLiteral("--receive-transport"), receiveTransport)) continue;
		if (stringOption(QStringLiteral("--stream-role"), streamRole)) continue;
		if (stringOption(QStringLiteral("--dds-topic"), ddsTopic)) continue;
		if (stringOption(QStringLiteral("--dds-codec"), ddsCodec)) continue;
		if (stringOption(QStringLiteral("--dds-qos"), ddsQos)) continue;
		if (stringOption(QStringLiteral("--dds-dump-first-frame"), ddsDumpFirstFrame)) continue;
		if (intOption(QStringLiteral("--dds-domain"), ddsDomain)) continue;
		if (intOption(QStringLiteral("--dds-width"), ddsWidth)) continue;
		if (intOption(QStringLiteral("--dds-height"), ddsHeight)) continue;
		if (intOption(QStringLiteral("--dds-fps"), ddsFps)) continue;
		if (intOption(QStringLiteral("--acceptance-exit-ms"), acceptanceExitMs)) continue;
    }
    qInfo().noquote()
        << QStringLiteral("[ProtocolLayout] component=HwaSim_IR_VideoDisplay channel=%1 platID=%2 sensorID=%3 pid=%4 ControlP2cX1ObjTrackingCmd=%5 InitP2cObjectTrackingCmd=%6 DisplayC2cObjTrackingData=%7 InitAckC2pObjectTrackingCmd=%8")
            .arg(channel.isEmpty() ? QStringLiteral("config") : channel)
            .arg(platID)
            .arg(sensorID)
            .arg(QCoreApplication::applicationPid())
            .arg(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd))
            .arg(sizeof(BYHWICD::InitP2cObjectTrackingCmd))
            .arg(sizeof(BYHWICD::DisplayC2cObjTrackingData))
            .arg(sizeof(BYHWICD::InitAckC2pObjectTrackingCmd));
	HwaSim_IR_VideoDisplay window(networkConfigPath, channel, platID, sensorID,
		receiveTransport, streamRole, ddsTopic, ddsCodec, ddsQos, ddsDomain, ddsWidth, ddsHeight, ddsFps,
		ddsDumpFirstFrame);
    window.show();
	if (acceptanceExitMs > 0)
	{
		qInfo().noquote() << QStringLiteral("[AcceptanceExit] scheduledMs=%1").arg(acceptanceExitMs);
		QTimer::singleShot(acceptanceExitMs, &app, &QCoreApplication::quit);
	}
    return app.exec();
}
