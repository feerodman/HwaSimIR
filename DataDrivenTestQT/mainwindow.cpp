// mainwindow.cpp
#include "mainwindow.h"
#include <QSpinBox>
#include <QScreen>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QScrollArea>
#include <QDoubleValidator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QFile>
#include "../DDS/Protocol/SensorFieldSchema.h"
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>
#include <QHostAddress>
#include <QApplication>
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QNetworkInterface>
#include <QStringList>
#include <algorithm>
#include <limits>

#include "ICD/math_algorithm.h"
#include "OrdinaryWeatherInput.h"
#include "ReplayCsvSchema.h"
#include "../Shared/NumericCsvReader.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRSolarPosition.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

//#define M_PI 3.1415926

namespace
{
QString targetTypeHex(int targetType)
{
	return QStringLiteral("0x%1").arg(targetType, 0, 16).toUpper();
}

bool targetUsesRedSpeed(int targetType)
{
	return targetType == 0x11;
}

double clampDouble(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

double isaAirTemperatureK(double altitudeM)
{
	const double clampedAltitude = clampDouble(altitudeM, 0.0, 20000.0);
	if (clampedAltitude <= 11000.0)
	{
		return 288.15 - 0.0065 * clampedAltitude;
	}
	return 216.65;
}

double speedOfSoundMps(double airTempK)
{
	const double gamma = 1.4;
	const double gasConstantDryAir = 287.05287;
	return std::sqrt(gamma * gasConstantDryAir * clampDouble(airTempK, 120.0, 400.0));
}

bool isAnyBindAddress(const QHostAddress& address)
{
	return address == QHostAddress::Any ||
		address == QHostAddress::AnyIPv4 ||
		address == QHostAddress::AnyIPv6;
}

bool isLoopbackAddress(const QHostAddress& address)
{
	bool ok = false;
	const quint32 ipv4 = address.toIPv4Address(&ok);
	if (ok)
	{
		return (ipv4 & 0xFF000000u) == 0x7F000000u;
	}
	return address == QHostAddress::LocalHostIPv6;
}

bool isAddressAssignedToThisHost(const QHostAddress& address)
{
	if (address.isNull())
	{
		return false;
	}
	if (isAnyBindAddress(address) || isLoopbackAddress(address))
	{
		return true;
	}
	const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
	for (const QHostAddress& hostAddress : addresses)
	{
		if (hostAddress == address)
		{
			return true;
		}
	}
	return false;
}

QString localIpv4Summary()
{
	QStringList items;
	const QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
	for (const QHostAddress& hostAddress : addresses)
	{
		if (hostAddress.protocol() == QAbstractSocket::IPv4Protocol)
		{
			items << hostAddress.toString();
		}
	}
	return items.isEmpty() ? QStringLiteral("none") : items.join(QStringLiteral(","));
}

double geodeticLosMeters(const ICD::Position& observer, const ICD::Position& target)
{
	const double pi = 3.14159265358979323846;
	const double radians = pi / 180.0;
	const double lat1 = observer.lat * radians;
	const double lat2 = target.lat * radians;
	const double deltaLat = (target.lat - observer.lat) * radians;
	const double deltaLon = (target.lon - observer.lon) * radians;
	const double sinLat = std::sin(deltaLat * 0.5);
	const double sinLon = std::sin(deltaLon * 0.5);
	const double haversine = sinLat * sinLat +
		std::cos(lat1) * std::cos(lat2) * sinLon * sinLon;
	const double central = 2.0 * std::asin(std::sqrt(clampDouble(haversine, 0.0, 1.0)));
	const double horizontal = 6371008.8 * central;
	const double vertical = target.alt - observer.alt;
	return std::sqrt(horizontal * horizontal + vertical * vertical);
}

QString sha256File(const QString& path, QString& error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
	{
		error = QStringLiteral("无法读取文件 %1: %2").arg(path, file.errorString());
		return QString();
	}
	QCryptographicHash digest(QCryptographicHash::Sha256);
	while (!file.atEnd())
	{
		const QByteArray block = file.read(1024 * 1024);
		if (block.isEmpty() && file.error() != QFile::NoError)
		{
			error = QStringLiteral("读取文件失败 %1: %2").arg(path, file.errorString());
			return QString();
		}
		digest.addData(block);
	}
	return QString::fromLatin1(digest.result().toHex());
}

QString p15BuildIdentifier()
{
	return QStringLiteral("P15-UI-NUMERIC | base 633048417108 | built ") +
		QString::fromLatin1(__DATE__) + QStringLiteral(" ") + QString::fromLatin1(__TIME__);
}

QString resolveApplicationRelativePath(const QString& configured)
{
	const QFileInfo info(configured);
	return info.isAbsolute()
		? info.absoluteFilePath()
		: QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(configured);
}
}

MainWindow::MainWindow(
	const QString& networkConfigPath,
	const QString& channel,
	const QString& inputDataPath,
	const QString& controlTransport,
	QWidget *parent)
	: QMainWindow(parent),
	  m_networkConfigPath(networkConfigPath.trimmed()),
	  m_inputDataPath(inputDataPath.trimmed()),
	  m_channel(channel.trimmed().toLower()),
	  m_controlTransport(controlTransport.trimmed().toLower())
{
	if (m_controlTransport != QStringLiteral("udp") &&
		m_controlTransport != QStringLiteral("dds") &&
		m_controlTransport != QStringLiteral("both"))
		qFatal("invalid --control-transport; expected udp|dds|both");
	loadNetworkConfig();
	setupUI();
    BYHWICD::trackerSensorParam initialSensor={};QString initialError;
    if(!sensorFormSnapshot(initialSensor,initialError))qFatal("Invalid initial sensor form");
    m_protocolSensorBand=initialSensor.trackerSensorBand;
    m_h264Enabled=initialSensor.h264En;
	if (m_controlTransport == QStringLiteral("udp") || m_controlTransport == QStringLiteral("both"))
		setupUDP();
	if (m_controlTransport == QStringLiteral("dds") || m_controlTransport == QStringLiteral("both"))
		setupDDS();


    //讀取文件内容
    QString tmp = m_inputDataPath;
    readData(tmp);
	qInfo().noquote() << QStringLiteral("[StimInput] path=%1 rows=%2 source=%3")
		.arg(QFileInfo(tmp).absoluteFilePath())
		.arg(realTimeData.size())
		.arg(QStringLiteral("resolved_input_file"));
	if (realTimeData.isEmpty())
	{
		qFatal("实时激励文件没有有效数据行: %s", qPrintable(QFileInfo(tmp).absoluteFilePath()));
	}

	// 初始化位置参数（构造时同步UI初始值）
	m_targetType = m_targetTypeBox->currentData().toInt();
	m_fovH = m_fovHEdit->text().toDouble();
	m_fovV = m_fovVEdit->text().toDouble();
    plane_init_pos.x = realTimeData.at(0).platPos.lat;
    plane_init_pos.y = realTimeData.at(0).platPos.lon;
    plane_init_pos.z = realTimeData.at(0).platPos.alt;
    plane_init_attitude.yaw = realTimeData.at(0).platEul.yaw;
    plane_init_attitude.pitch = realTimeData.at(0).platEul.pitch;
    plane_init_attitude.roll = realTimeData.at(0).platEul.roll;
    missile_init_pos.x = realTimeData.at(0).tarPos.lat;
    missile_init_pos.y = realTimeData.at(0).tarPos.lon;
    missile_init_pos.z = realTimeData.at(0).tarPos.alt;
    missile_init_attitude.yaw = realTimeData.at(0).tarEul.yaw;
    missile_init_attitude.pitch = realTimeData.at(0).tarEul.pitch;
    missile_init_attitude.roll = realTimeData.at(0).tarEul.roll;
    plane_speed_y = realTimeData.at(0).platSpeed;
//	collision_time = m_collisionTime->text().toDouble();
	m_targetVideoFps = targetVideoFps();
	setSendStepMs(m_timeStep->text().toDouble());


	m_realTimeTimer = new QTimer(this);
	m_realTimeTimer->setSingleShot(true);
	m_realTimeTimer->setTimerType(Qt::PreciseTimer);
	connect(m_realTimeTimer, &QTimer::timeout, this, &MainWindow::onSendRealTimeData);

	// 状态初始化
	m_statusLabel->setText(QStringLiteral("状态: 就绪"));

}

MainWindow::~MainWindow()
{
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_ddsStim) {
        // A transport ACK is not an application STOP acknowledgment. Keep the
        // writers alive while the renderer drains its ordered input queue.
        std::string drainError;
		QElapsedTimer lifecycleClock;
		lifecycleClock.start();
		qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=wait_renderer_stop begin=1 timeoutMs=30000");
		if(!m_ddsStim->waitForStopStatus(30000,drainError))
            qCritical().noquote()<<QString("[StimDrain][ERROR] %1").arg(QString::fromStdString(drainError));
		else
			qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=renderer_stop_status result=PASS elapsedMs=%1")
				.arg(lifecycleClock.elapsed());
		qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=wait_dds_ack begin=1 timeoutMs=10000 elapsedMs=%1")
			.arg(lifecycleClock.elapsed());
		if(!m_ddsStim->waitForAcknowledgments(10000,drainError))
            qCritical().noquote()<<QString("[StimDrain][ERROR] %1").arg(QString::fromStdString(drainError));
		else
			qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=dds_ack_drain result=PASS elapsedMs=%1")
				.arg(lifecycleClock.elapsed());
        m_ddsStim->shutdown();
		qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=dds_shutdown result=PASS elapsedMs=%1")
			.arg(lifecycleClock.elapsed());
    }
#endif
	if (m_udpSocket) {
		m_udpSocket->close();
		delete m_udpSocket;
	}
}

void MainWindow::setupDDS()
{
#if !defined(HWASIMIR_HAS_ZRDDS)
	qFatal("DDS control transport selected but DataDrivenTestQT lacks HWASIMIR_HAS_ZRDDS");
#else
	QSettings settings(m_networkConfigPath, QSettings::IniFormat);
	DdsStimConfig config;
	config.domainId = settings.value(QStringLiteral("DdsProtocol/DomainId"), 150).toInt();
	QString qos = settings.value(QStringLiteral("DdsProtocol/QosFile"),
		QStringLiteral("Config/DDS/ZRDDS_PROTOCOL_QOS.xml")).toString();
	if (QFileInfo(qos).isRelative())
		qos = QDir(QCoreApplication::applicationDirPath()).filePath(qos);
	config.qosFile = qos.toLocal8Bit().constData();
	config.topicControl = settings.value(QStringLiteral("DdsProtocol/TopicControl"),
		QStringLiteral("HwaSimIR.Control")).toString().toLatin1().constData();
	config.topicInit = settings.value(QStringLiteral("DdsProtocol/TopicInit"),
		QStringLiteral("HwaSimIR.Init")).toString().toLatin1().constData();
	config.topicRealtime = settings.value(QStringLiteral("DdsProtocol/TopicRealtime"),
		QStringLiteral("HwaSimIR.Realtime")).toString().toLatin1().constData();
	config.topicInitAck = settings.value(QStringLiteral("DdsProtocol/TopicInitAck"),
		QStringLiteral("HwaSimIR.InitAck")).toString().toLatin1().constData();
    config.observeStopStatus = true;
    if(m_channel != QStringLiteral("unknown")) config.channel = m_channel.toStdString();
    config.topicVideoStatus = settings.value(QStringLiteral("DdsProtocol/TopicVideoStatus"),
        QStringLiteral("HwaSimIR.VideoStatus")).toString().toLatin1().constData();
	m_ddsStim.reset(new DdsStimClient());
	m_ddsStim->setAckCallback([this](const BYHWICD::InitAckC2pObjectTrackingCmd& ack) {
		QMetaObject::invokeMethod(this, [this, ack]() {
            if (ack.platID != m_protocolPlatID || ack.sensorID != m_protocolSensorID) return;
			qInfo().noquote() << QStringLiteral("[StimInitAck] transport=dds received=1 platID=%1 sensorID=%2 ready=%3")
				.arg(ack.platID).arg(ack.sensorID).arg(ack.trackingReady ? 1 : 0);
			m_lastReceivedLabel->setText(QStringLiteral("↓ 接收: DDS 初始化应答 (0x37)"));
			if (!ack.trackingReady)
			{
				m_statusLabel->setText(QStringLiteral("● 状态: DDS 初始化被渲染端拒绝 | 未发送 START"));
				m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
				qCritical().noquote() << QStringLiteral("[StimInitAck][ERROR] ready=0 action=start_suppressed");
				return;
			}
			m_statusLabel->setText(QStringLiteral("● 状态: DDS 初始化完成 | 等待 START/首个有效实时位置（INIT 仅预热）"));
			m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
			emit initAckReceived();
		}, Qt::QueuedConnection);
	});
	std::string error;
	if (!m_ddsStim->start(config, error))
		qFatal("DataDriven DDS start failed: %s", error.c_str());
	qInfo().noquote() << QStringLiteral("[StimTransport] mode=%1 ddsRuntimeInitCount=%2 domain=%3 qos=%4")
		.arg(m_controlTransport).arg(m_ddsStim->runtimeInitCount()).arg(config.domainId).arg(qos);
#endif
}

void MainWindow::configurePhase4cAeroMachTest(bool enabled, double altitudeKm, double mach)
{
	m_phase4cAeroMachMode = enabled;
	m_phase4cAltitudeKm = clampDouble(altitudeKm, 0.0, 20.0);
	m_phase4cMach = clampDouble(mach, 0.0, 4.0);
	const double altitudeM = m_phase4cAltitudeKm * 1000.0;
	const double airTempK = isaAirTemperatureK(altitudeM);
	m_phase4cSpeedMps = m_phase4cMach * speedOfSoundMps(airTempK);
	m_phase4cSpeedKmh = m_phase4cSpeedMps * 3.6;
	if (m_phase4cAeroMachMode)
	{
		qInfo().noquote()
			<< QStringLiteral("[Phase4C AeroMachConfig] enabled=1 altitudeKm=%1 machCommand=%2 speedMps=%3 speedKmh=%4 speedUnit=km/h")
				.arg(m_phase4cAltitudeKm, 0, 'f', 3)
				.arg(m_phase4cMach, 0, 'f', 3)
				.arg(m_phase4cSpeedMps, 0, 'f', 3)
				.arg(m_phase4cSpeedKmh, 0, 'f', 3);
	}
}

void MainWindow::configureProtocolForTest(int platID, int sensorID, int simMode, int videoFps)
{
	if (platID >= 0)
	{
		m_protocolPlatID = platID;
	}
	if (sensorID >= 0)
	{
		m_protocolSensorID = sensorID;
	}
	m_protocolSimMode = (simMode == 1 || simMode == 2) ? simMode : m_protocolSimMode;
    if(m_simModeBox)m_simModeBox->setCurrentIndex(m_simModeBox->findData(m_protocolSimMode));
	if (videoFps >= 0)
	{
		m_protocolVideoFps = qBound(0, videoFps, 240);
	}
	if (m_videoFpsEdit)
	{
		m_videoFpsEdit->setText(QString::number(m_protocolVideoFps));
	}
    if(m_identitySourceLabel)m_identitySourceLabel->setText(QString("%1 / %2 / %3").arg(m_channel).arg(m_protocolPlatID).arg(m_protocolSensorID));
	qInfo().noquote()
		<< QStringLiteral("[StimProtocol] channel=%1 platID=%2 sensorID=%3 pid=%4 simMode=%5 videoFps=%6 saveMP4En=%7 h264En=%8 source=cli_or_config")
			.arg(m_channel)
			.arg(m_protocolPlatID)
			.arg(m_protocolSensorID)
			.arg(QCoreApplication::applicationPid())
			.arg(m_protocolSimMode)
			.arg(m_protocolVideoFps)
			.arg(m_saveMP4Enabled ? 1 : 0)
			.arg(m_h264Enabled ? 1 : 0);
}

void MainWindow::configureIlluminatorForTest(double angleMrad, double spotRad,
	int forceEnabled, double onStartSec, double onEndSec)
{
	if (angleMrad >= 0.0) {m_protocolIlluminatorAngleMrad = angleMrad;setSensorField("illuminatorAngle",angleMrad);}
	if (spotRad >= 0.0) {m_protocolIlluminatorSpotRad = spotRad;setSensorField("illuminatorSpotRad",spotRad);}
	m_protocolIlluminatorForceEnabled = forceEnabled > 0 ? 1 : 0;
	m_protocolIlluminatorOnStartSec = onStartSec;
	m_protocolIlluminatorOnEndSec = onEndSec;
	qInfo().noquote() << QStringLiteral("[StimIlluminatorConfig] angleMrad=%1 spotRad=%2 forceEnabled=%3 onStartSec=%4 onEndSec=%5 protocolLayoutUnchanged=1")
		.arg(m_protocolIlluminatorAngleMrad, 0, 'f', 6)
		.arg(m_protocolIlluminatorSpotRad, 0, 'f', 6)
		.arg(m_protocolIlluminatorForceEnabled)
		.arg(m_protocolIlluminatorOnStartSec, 0, 'f', 3)
		.arg(m_protocolIlluminatorOnEndSec, 0, 'f', 3);
}

void MainWindow::configureEnvironmentForTest(int envSky, int sensorBand)
{
	if (envSky >= 0 && envSky <= 5)
	{
		m_protocolEnvSky = envSky;
	}
	if (sensorBand >= 0 && sensorBand <= 4)
	{
		m_protocolSensorBand = sensorBand;setSensorField("trackerSensorBand",sensorBand);
	}
	qInfo().noquote()
		<< QStringLiteral("[StimWeather] envSky=%1 sensorBand=%2 source=cli_or_default")
			.arg(m_protocolEnvSky)
			.arg(m_protocolSensorBand);
}

void MainWindow::setSensorPixelAngleForTest(double pixelAngleUrad)
{
	if (std::isfinite(pixelAngleUrad) && pixelAngleUrad > 0.0)
	{
		m_protocolSensorPixelAngleUrad = qBound(0.1, pixelAngleUrad, 1000.0);setSensorField("trackerSensorPixelAngle",m_protocolSensorPixelAngleUrad);
	}
	qInfo().noquote()
		<< QStringLiteral("[StimSensorGeometry] pixelAngleUrad=%1 source=cli_or_default")
			.arg(m_protocolSensorPixelAngleUrad, 0, 'f', 3);
}

// ==================== 核心修正：补充缺失的槽函数 ====================
void MainWindow::onSendRealTimeData()
{
	if (!m_isRealtimeSending)
	{
		return;
	}
//    while (dataNum == 7000) {


//    }
	const double elapsedSec = m_sendClock.isValid()
		? static_cast<double>(m_sendClock.nsecsElapsed()) / 1.0e9 : 0.0;
	const bool pauseWindow = m_pauseStartSec >= 0.0 && m_pauseDurationSec > 0.0 &&
		elapsedSec >= m_pauseStartSec && elapsedSec < m_pauseStartSec + m_pauseDurationSec;
	if (pauseWindow)
	{
		if (!m_pauseActiveLogged)
		{
			m_pauseActiveLogged = true;
			qInfo().noquote() << QStringLiteral("[StimPause] state=paused startSec=%1 durationSec=%2 sentFrames=%3")
				.arg(m_pauseStartSec, 0, 'f', 3).arg(m_pauseDurationSec, 0, 'f', 3).arg(m_sentFrameCount);
		}
	}
	else
	{
		if (m_pauseActiveLogged && !m_pauseResumeLogged)
		{
			m_pauseResumeLogged = true;
			qInfo().noquote() << QStringLiteral("[StimPause] state=resumed elapsedSec=%1 sentFrames=%2 catchUpBurst=0")
				.arg(elapsedSec, 0, 'f', 3).arg(m_sentFrameCount);
		}
		sendRealTimeData();
	}

	if (m_isRealtimeSending)
	{
		scheduleNextRealTimeFrame();
	}
}
// ===============================================================

int MainWindow::targetVideoFps() const
{
	const int requested = m_videoFpsEdit ? m_videoFpsEdit->text().toInt() : 60;
	return qBound(0, requested, 240);
}

void MainWindow::scheduleNextRealTimeFrame()
{
	++m_sendDeadlineIndex;
	const qint64 targetNs = static_cast<qint64>(
		(static_cast<long double>(m_sendDeadlineIndex) * 1000000000.0L) /
		static_cast<long double>(m_inputHz));
	const qint64 remainingNs = qMax<qint64>(0, targetNs - m_sendClock.nsecsElapsed());
	const int delayMs = static_cast<int>((remainingNs + 999999LL) / 1000000LL);
	m_realTimeTimer->start(delayMs);
}

void MainWindow::setupUI()
{
	setWindowTitle(QStringLiteral("激励数据软件 - 红方仿真激励端"));
	resize(850, 700);
	setStyleSheet("QGroupBox { font-weight: bold; padding: 10px; }"
		"QPushButton { min-height: 30px; font-size: 14px; }"
		"QLineEdit { padding: 3px; }");

	// 配置组
	//192.168.1.189
	//192.168.1.10
	//127.0.0.1
	m_configGroup = new QGroupBox(m_controlTransport==QStringLiteral("dds")?QStringLiteral("DDS 通信配置"):QStringLiteral("UDP 兼容通信配置"));
	QFormLayout *configLayout = new QFormLayout;
	configLayout->addRow(QStringLiteral("本地IP:"), m_localIpEdit = new QLineEdit(m_udpLocalIp));
	configLayout->addRow(QStringLiteral("本地端口:"), m_localPortEdit = new QLineEdit(QString::number(m_udpLocalPort)));
	configLayout->addRow(QStringLiteral("目标IP:"), m_remoteIpEdit = new QLineEdit(m_udpRemoteIp));
	configLayout->addRow(QStringLiteral("目标端口:"), m_remotePortEdit = new QLineEdit(QString::number(m_udpRemotePort)));
	m_configGroup->setLayout(configLayout);
    if(m_controlTransport==QStringLiteral("dds")){
        for(auto* edit:{m_localIpEdit,m_localPortEdit,m_remoteIpEdit,m_remotePortEdit}){edit->setReadOnly(true);edit->hide();}
        for(int r=0;r<configLayout->rowCount();++r)if(auto* item=configLayout->itemAt(r,QFormLayout::LabelRole))item->widget()->hide();
        configLayout->addRow(QStringLiteral("实例"),m_identitySourceLabel=new QLabel(QString("%1 / %2 / %3").arg(m_channel).arg(m_protocolPlatID).arg(m_protocolSensorID)));
        configLayout->addRow(QStringLiteral("DDS"),new QLabel(QStringLiteral("配置来源：")+m_networkConfigPath));
    }

	// 控制组
	m_controlGroup = new QGroupBox(QStringLiteral("仿真控制"));
	QHBoxLayout *controlLayout = new QHBoxLayout;
	m_resetButton = new QPushButton(QStringLiteral("□ 复位 (1)"));
	m_initButton = new QPushButton(QStringLiteral("○ 初始化 (0x36)"));
	m_startButton = new QPushButton(QStringLiteral("▲▼ 开始仿真 (2)"));
	m_stopButton = new QPushButton(QStringLiteral("■ 停止仿真 (3)"));
	QPushButton *versionInfoButton = new QPushButton(QStringLiteral("ⓘ 版本信息"));
	m_resetButton->setObjectName(QStringLiteral("resetButton"));
	m_initButton->setObjectName(QStringLiteral("initButton"));
	m_startButton->setObjectName(QStringLiteral("startButton"));
	m_stopButton->setObjectName(QStringLiteral("stopButton"));
	versionInfoButton->setObjectName(QStringLiteral("versionInfoButton"));

	// 按钮样式
	m_startButton->setStyleSheet("background-color: #4CAF50; color: white;");
	m_stopButton->setStyleSheet("background-color: #f44336; color: white;");

	controlLayout->addWidget(m_resetButton);
	controlLayout->addWidget(m_initButton);
	controlLayout->addWidget(m_startButton);
	controlLayout->addWidget(m_stopButton);
	controlLayout->addWidget(versionInfoButton);
	controlLayout->addStretch();
	m_controlGroup->setLayout(controlLayout);

	// 测试目标是普通操作入口，必须始终位于折叠的文件预览之外。
	m_testTargetGroup = new QGroupBox(QStringLiteral("测试目标"));
	m_testTargetGroup->setObjectName(QStringLiteral("testTargetGroup"));
	QFormLayout *testTargetLayout = new QFormLayout;
	testTargetLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	m_targetTypeBox = new QComboBox;
	m_targetTypeBox->setObjectName(QStringLiteral("targetTypeCombo"));
	m_targetTypeBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	m_targetTypeBox->setMinimumContentsLength(22);
	m_targetTypeBox->addItem(QStringLiteral("F-35 飞机 — 协议 0x11"), 0x11);
	m_targetTypeBox->addItem(QStringLiteral("F-22 飞机 — 协议 0x12"), 0x12);
	m_targetTypeBox->addItem(QStringLiteral("AIM-120 通用雷达弹模型 — 协议 0x22"), 0x22);
	m_targetTypeBox->addItem(QStringLiteral("AIM-9 通用红外弹模型 — 协议 0x33"), 0x33);
	m_targetTypeBox->addItem(QStringLiteral("MMD — 协议 0x44"), 0x44);
	m_targetTypeBox->addItem(QStringLiteral("民用厢式车/卡车测试载体 — 协议 0x55"), 0x55);
	m_targetTypeBox->addItem(QStringLiteral("通用材料/热源样件 — 协议 0x66"), 0x66);
	bool configuredTargetOk = false;
	const int configuredTarget = QSettings(m_networkConfigPath, QSettings::IniFormat)
		.value(QStringLiteral("Demo/TargetType"), QStringLiteral("0x22"))
		.toString().toInt(&configuredTargetOk, 0);
	const int configuredTargetIndex = configuredTargetOk ? m_targetTypeBox->findData(configuredTarget) : -1;
	m_targetTypeBox->setCurrentIndex(configuredTargetIndex >= 0 ? configuredTargetIndex : m_targetTypeBox->findData(0x22));
	testTargetLayout->addRow(QStringLiteral("目标类型（INIT 后冻结）:"), m_targetTypeBox);
	m_forceVisibleForDemoCheck = new QCheckBox(QStringLiteral("演示强制显示（测试端覆盖 ViewValid；不修改 1.txt）"));
	m_forceVisibleForDemoCheck->setObjectName(QStringLiteral("forceVisibleForDemoCheck"));
	m_forceVisibleForDemoCheck->setChecked(false);
	m_forceVisibleForDemoCheck->setToolTip(QStringLiteral("默认关闭并严格回放源 ViewValid。仅显式勾选时，把发送包中的显示标志强制为 1；位置有效性仍独立检查。"));
	testTargetLayout->addRow(QStringLiteral("显示策略:"), m_forceVisibleForDemoCheck);
	m_testTargetGroup->setLayout(testTargetLayout);

	// 文件位置/姿态只读预览仍可折叠，但不再拥有或隐藏目标选择控件。
	m_realTimeDataGroup = new QGroupBox(QStringLiteral("文件数据与身份 · 位置来自输入文件"));
	m_realTimeDataGroup->setObjectName(QStringLiteral("fileDataPreviewGroup"));
	QFormLayout *realTimeLayout = new QFormLayout;
	m_videoFpsEdit = new QLineEdit(QString::number(m_protocolVideoFps));
    realTimeLayout->addRow(QStringLiteral("横向视场角:"), m_fovHEdit = new QLineEdit("0.1"));
    realTimeLayout->addRow(QStringLiteral("纵向视场角:"), m_fovVEdit = new QLineEdit("0.1"));
	//realTimeLayout->addRow(QStringLiteral("平台ID:"), m_platIDEdit = new QLineEdit("1"));
	//realTimeLayout->addRow(QStringLiteral("传感器ID:"), m_sensorIDEdit = new QLineEdit("0"));
	//realTimeLayout->addRow(QStringLiteral("当前回合:"), m_currentRoundEdit = new QLineEdit("1"));
	//realTimeLayout->addRow(QStringLiteral("总回合数:"), m_roundCutEdit = new QLineEdit("10"));
	realTimeLayout->addRow(QStringLiteral("平台纬度(°)/X(m):"), m_latEdit = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("平台经度(°)/Y(m):"), m_lonEdit = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("平台高度/Z(m):"), m_altEdit = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("平台航向(°):"), m_yawEdit = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("平台俯仰(°):"), m_pitchEdit = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("平台横滚(°):"), m_rollEdit = new QLineEdit("0.0"));
    realTimeLayout->addRow(QStringLiteral("目标纬度(°)/X(m):"), m_latEditTarget = new QLineEdit("50000.0"));//可以看作是目標與平臺的初始距離
	realTimeLayout->addRow(QStringLiteral("目标经度(°)/Y(m):"), m_lonEditTarget = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("目标高度/Z(m):"), m_altEditTarget = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("目标航向(°):"), m_yawEditTarget = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("目标俯仰(°):"), m_pitchEditTarget = new QLineEdit("0.0"));
	realTimeLayout->addRow(QStringLiteral("目标横滚(°):"), m_rollEditTarget = new QLineEdit("0.0"));
//    realTimeLayout->addRow(QStringLiteral("相撞时间(s):"), m_collisionTime = new QLineEdit("30.0"));//相撞時間
	realTimeLayout->addRow(QStringLiteral("平台速度/Z(m):"), m_speed = new QLineEdit("100.0"));
    m_timeStep = new QLineEdit(QString::number(m_sendStepMs,'f',6));
    m_timeStep->setValidator(new QDoubleValidator(.1,100000,6,m_timeStep));
	

	m_realTimeDataGroup->setLayout(realTimeLayout);

	// 状态组
	m_statusGroup = new QGroupBox(QStringLiteral("运行状态"));
	QVBoxLayout *statusLayout = new QVBoxLayout;
	m_statusLabel = new QLabel(QStringLiteral("● 状态: 就绪 | 未开始发送"));
	m_statusLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
	m_lastSentLabel = new QLabel(QStringLiteral("↑ 最后发送: 无"));
	m_lastReceivedLabel = new QLabel(QStringLiteral("↓ 最后接收: 无"));
	QLabel *versionSummaryLabel = new QLabel(QStringLiteral("版本: ") + p15BuildIdentifier());
	versionSummaryLabel->setObjectName(QStringLiteral("programIdentityLabel"));
	versionSummaryLabel->setWordWrap(true);
	versionSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
	statusLayout->addWidget(m_statusLabel);
	statusLayout->addWidget(m_lastSentLabel);
	statusLayout->addWidget(m_lastReceivedLabel);
	statusLayout->addWidget(versionSummaryLabel);
	m_statusGroup->setLayout(statusLayout);

	// 主布局
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(m_configGroup);
	mainLayout->addWidget(m_controlGroup);
	mainLayout->addWidget(m_testTargetGroup);
	    setupSensorForm(mainLayout);
    auto* realtimeGroup=new QGroupBox(QStringLiteral("实时发送配置"));
    auto* pacing=new QFormLayout(realtimeGroup);pacing->addRow(QStringLiteral("发送步长 (ms)"),m_timeStep);mainLayout->addWidget(realtimeGroup);
    for(auto* edit:{m_latEdit,m_lonEdit,m_altEdit,m_yawEdit,m_pitchEdit,m_rollEdit,
        m_latEditTarget,m_lonEditTarget,m_altEditTarget,m_yawEditTarget,m_pitchEditTarget,m_rollEditTarget,m_speed})edit->setReadOnly(true);
    auto* sourceTitle=new QLabel(QStringLiteral("输入文件：")+QFileInfo(m_inputDataPath).absoluteFilePath());
    sourceTitle->setWordWrap(true);mainLayout->addWidget(sourceTitle);
    // Existing file-driven compatibility controls stay available, outside pacing.
    m_realTimeDataGroup->setCheckable(true);m_realTimeDataGroup->setChecked(false);
    auto setPreviewVisible=[this](bool visible){for(auto* child:m_realTimeDataGroup->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))child->setVisible(visible);};
    m_realTimeDataGroup->setMaximumHeight(28);
    connect(m_realTimeDataGroup,&QGroupBox::toggled,this,[this](bool on){m_realTimeDataGroup->setMaximumHeight(on?QWIDGETSIZE_MAX:28);});
    setPreviewVisible(false);connect(m_realTimeDataGroup,&QGroupBox::toggled,this,setPreviewVisible);
    mainLayout->addWidget(m_realTimeDataGroup);
	mainLayout->addWidget(m_statusGroup);
	mainLayout->addStretch();

	QWidget *centralWidget = new QWidget;
	centralWidget->setLayout(mainLayout);
	auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(centralWidget);setCentralWidget(scroll);
    // Use the actual available screen; scrolling remains for smaller windows.
    const auto available=QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1180,available.width()-20),qMin(800,available.height()-40));
    setStyleSheet("QWidget{font-family:'Microsoft YaHei UI';font-size:12px;}"
        "QGroupBox{font-weight:bold;margin-top:6px;padding:0px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;}"
        "QPushButton{min-height:24px;}QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{min-height:18px;padding:1px;}" );

	// 信号连接
	connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::onResetButtonClicked);
	connect(m_initButton, &QPushButton::clicked, this, &MainWindow::onInitButtonClicked);
	connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
	connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
	connect(versionInfoButton, &QPushButton::clicked, this, [this]() {
		QString hashError;
		const QString programPath = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();
		const QString programHash = sha256File(programPath, hashError);
		QString details = QStringLiteral(
			"程序路径：%1\n\n构建标识：%2\n\n实际配置路径：%3\n\n工作目录：%4\n\n程序 SHA-256：%5")
			.arg(programPath, p15BuildIdentifier(), QFileInfo(m_networkConfigPath).absoluteFilePath(),
				QDir::currentPath(), programHash.isEmpty() ? hashError : programHash);
		QMessageBox *box = new QMessageBox(QMessageBox::Information,
			QStringLiteral("DataDrivenTestQT 版本信息"), details, QMessageBox::Ok, this);
		box->setObjectName(QStringLiteral("versionInfoDialog"));
		box->setTextFormat(Qt::PlainText);
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->show();
	});
}

void MainWindow::loadNetworkConfig()
{
	const QString configPath = m_networkConfigPath.isEmpty()
		? QDir(QCoreApplication::applicationDirPath()).filePath("NetworkConfig.ini")
		: QFileInfo(m_networkConfigPath).absoluteFilePath();
	m_networkConfigPath = configPath;
	const bool configExists = QFileInfo::exists(configPath);
	QSettings settings(configPath, QSettings::IniFormat);
	settings.setIniCodec("UTF-8");

    if(m_inputDataPath.isEmpty()){
        const QString configured=settings.value("Demo/InputFile",QStringLiteral("1.txt")).toString();
        m_inputDataPath=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(configured);
    }else m_inputDataPath=QFileInfo(m_inputDataPath).absoluteFilePath();
    if(!QFileInfo(m_inputDataPath).isFile())qFatal("Missing input file: %s",qPrintable(m_inputDataPath));
    m_protocolEnvSky=settings.value("Demo/envSky",0).toInt();
	bool visibilityOk = false;
	bool humidityOk = false;
	m_protocolEnvVisibilityM = settings.value(
		QStringLiteral("WeatherInit/envVisibility"), 6000.0).toDouble(&visibilityOk);
	m_protocolEnvHumidityPercent = settings.value(
		QStringLiteral("WeatherInit/envHumidity"), 85.0).toDouble(&humidityOk);
	if (!visibilityOk || !std::isfinite(m_protocolEnvVisibilityM) || m_protocolEnvVisibilityM <= 0.0)
		qFatal("Invalid WeatherInit/envVisibility; expected finite metres > 0");
	if (!humidityOk || !std::isfinite(m_protocolEnvHumidityPercent) ||
		m_protocolEnvHumidityPercent < 0.0 || m_protocolEnvHumidityPercent > 100.0)
		qFatal("Invalid WeatherInit/envHumidity; expected percent in [0,100]");
	bool utcHourOk = false;
	const double configuredUtcHour = settings.value(QStringLiteral("Demo/UtcHour"), -1.0).toDouble(&utcHourOk);
	if (!utcHourOk || !std::isfinite(configuredUtcHour) || configuredUtcHour < -1.0 || configuredUtcHour >= 24.0)
		qFatal("Invalid Demo/UtcHour; expected -1 for wall clock or [0,24)");
	m_testUtcHour = configuredUtcHour;
	const QString configuredUtcDate = settings.value(QStringLiteral("Demo/UtcDate"), QString()).toString().trimmed();
	if (configuredUtcDate.isEmpty())
	{
		m_simulationUtcDate = QDateTime::currentDateTimeUtc().date();
		m_simulationDateSource = QStringLiteral("wall_clock_utc_date");
	}
	else
	{
		m_simulationUtcDate = QDate::fromString(configuredUtcDate, QStringLiteral("yyyy-MM-dd"));
		if (!m_simulationUtcDate.isValid() || m_simulationUtcDate.toString(QStringLiteral("yyyy-MM-dd")) != configuredUtcDate)
			qFatal("Invalid Demo/UtcDate; expected yyyy-MM-dd");
		m_simulationDateSource = QStringLiteral("NetworkConfig.ini:Demo/UtcDate");
	}
	m_simulationTimeSource = configuredUtcHour >= 0.0
		? QStringLiteral("NetworkConfig.ini:Demo/UtcHour")
		: QStringLiteral("wall_clock_utc");
	m_formalAtmosphereLutPath = resolveApplicationRelativePath(
		settings.value(QStringLiteral("Atmosphere/FormalLut"),
			QStringLiteral("Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
		.toString().trimmed());
	m_atmosphereCoverageManifestPath = resolveApplicationRelativePath(
		settings.value(QStringLiteral("Atmosphere/CoverageManifest"),
			QStringLiteral("Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json"))
		.toString().trimmed());
	m_expectedFormalAtmosphereLutSha256 = settings.value(
		QStringLiteral("Atmosphere/ExpectedFormalLutSha256"), QString()).toString().trimmed().toLower();
	m_expectedAtmosphereCoverageManifestSha256 = settings.value(
		QStringLiteral("Atmosphere/ExpectedCoverageManifestSha256"), QString()).toString().trimmed().toLower();
	if (m_expectedFormalAtmosphereLutSha256.size() != 64 ||
		m_expectedAtmosphereCoverageManifestSha256.size() != 64)
		qFatal("Atmosphere expected SHA-256 values must both be configured as 64 hex characters");
    m_sendStepMs=settings.value("RenderControl/sendStepMs",1000.0/60.0).toDouble();
    if(!std::isfinite(m_sendStepMs)||m_sendStepMs<.1||m_sendStepMs>100000)qFatal("Invalid sendStepMs");
    m_inputHz=1000.0/m_sendStepMs;
	const QString defaultLocalIp = QStringLiteral("0.0.0.0");
	const quint16 defaultLocalPort = 9999;
	const QString defaultRemoteIp = QStringLiteral("127.0.0.1");
	const quint16 defaultRemotePort = 8888;

	if (!configExists)
	{
		settings.setValue(QStringLiteral("Identity/platID"), 1001);
		settings.setValue(QStringLiteral("Identity/sensorID"), 2);
		settings.setValue(QStringLiteral("RenderControl/simMode"), 2);
		settings.setValue(QStringLiteral("RenderControl/videoFps"), 60);
		settings.setValue(QStringLiteral("UDP/localIp"), defaultLocalIp);
		settings.setValue(QStringLiteral("UDP/localPort"), defaultLocalPort);
		settings.setValue(QStringLiteral("UDP/remoteIp"), defaultRemoteIp);
		settings.setValue(QStringLiteral("UDP/remotePort"), defaultRemotePort);
		settings.sync();
	}

	QString localIp = settings.value(QStringLiteral("UDP/localIp"), defaultLocalIp).toString().trimmed();
	QString remoteIp = settings.value(QStringLiteral("UDP/remoteIp"), defaultRemoteIp).toString().trimmed();
	int localPort = settings.value(QStringLiteral("UDP/localPort"), defaultLocalPort).toInt();
	int remotePort = settings.value(QStringLiteral("UDP/remotePort"), defaultRemotePort).toInt();
	m_protocolPlatID = settings.value(QStringLiteral("Identity/platID"), 1001).toInt();
	m_protocolSensorID = settings.value(QStringLiteral("Identity/sensorID"), 2).toInt();
	if (m_channel.isEmpty())
	{
		m_channel = settings.value(QStringLiteral("Identity/channel"), QStringLiteral("unknown"))
			.toString().trimmed().toLower();
	}
	m_protocolSimMode = settings.value(QStringLiteral("RenderControl/simMode"), 2).toInt();
	m_protocolVideoFps = settings.value(QStringLiteral("RenderControl/videoFps"), 60).toInt();
	if (m_protocolSimMode != 1 && m_protocolSimMode != 2)
	{
		qWarning() << "Invalid RenderControl/simMode in" << configPath
			<< ":" << m_protocolSimMode << "- using 2";
		m_protocolSimMode = 2;
	}
	m_protocolVideoFps = qBound(0, m_protocolVideoFps, 240);

	if (QHostAddress(localIp).isNull())
	{
		qWarning() << "Invalid UDP localIp in" << configPath << ":" << localIp
			<< "- using" << defaultLocalIp;
		localIp = defaultLocalIp;
	}
	if (QHostAddress(remoteIp).isNull())
	{
		qWarning() << "Invalid UDP remoteIp in" << configPath << ":" << remoteIp
			<< "- using" << defaultRemoteIp;
		remoteIp = defaultRemoteIp;
	}
	if (localPort <= 0 || localPort > 65535)
	{
		qWarning() << "Invalid UDP localPort in" << configPath << ":" << localPort
			<< "- using" << defaultLocalPort;
		localPort = defaultLocalPort;
	}
	if (remotePort <= 0 || remotePort > 65535)
	{
		qWarning() << "Invalid UDP remotePort in" << configPath << ":" << remotePort
			<< "- using" << defaultRemotePort;
		remotePort = defaultRemotePort;
	}

	m_udpLocalIp = localIp;
	m_udpLocalPort = static_cast<quint16>(localPort);
	m_udpRemoteIp = remoteIp;
	m_udpRemotePort = static_cast<quint16>(remotePort);

	qInfo().noquote()
		<< QStringLiteral("[RuntimeInstance] component=DataDrivenTestQT channel=%1 platID=%2 sensorID=%3 pid=%4 configPath=%5 udpLocal=%6:%7 udpRemote=%8:%9 configSource=%10")
			.arg(m_channel)
			.arg(m_protocolPlatID)
			.arg(m_protocolSensorID)
			.arg(QCoreApplication::applicationPid())
			.arg(configPath)
			.arg(localIp)
			.arg(localPort)
			.arg(remoteIp)
			.arg(remotePort)
			.arg(configExists ? QStringLiteral("ini") : QStringLiteral("generated_default"));
	qInfo().noquote() << QStringLiteral("[StimTimeConfig] utcDate=%1 dateSource=%2 utcHour=%3 hourSource=%4 sourceTimeColumn=Time(ms) simulationTime=source_offset sendWallTime=pacing_clock videoPts=encoder_timebase")
		.arg(m_simulationUtcDate.toString(QStringLiteral("yyyy-MM-dd")))
		.arg(m_simulationDateSource)
		.arg(m_testUtcHour, 0, 'f', 6).arg(m_simulationTimeSource);
	qInfo().noquote() << QStringLiteral(
		"[StimAtmosphereIdentity] formalLut=%1 expectedLutSha256=%2 coverageManifest=%3 expectedManifestSha256=%4 binding=control_and_renderer_same_identity")
		.arg(m_formalAtmosphereLutPath, m_expectedFormalAtmosphereLutSha256,
			m_atmosphereCoverageManifestPath, m_expectedAtmosphereCoverageManifestSha256);
	qInfo().noquote() << QStringLiteral(
		"[StimWeatherInitConfig] envSky=%1 visibilityM=%2 relativeHumidityPercent=%3 source=NetworkConfig.ini noHiddenOverride=1")
		.arg(m_protocolEnvSky).arg(m_protocolEnvVisibilityM, 0, 'f', 3)
		.arg(m_protocolEnvHumidityPercent, 0, 'f', 3);
}

void MainWindow::setupUDP()
{
	m_udpSocket = new QUdpSocket(this);

	// 绑定本地端口（可选，用于接收应答）。本机测试时 0.0.0.0 更稳，避免配置到不存在网卡 IP 后启动失败。
	QString requestedLocalIp = m_localIpEdit->text().trimmed();
	const quint16 requestedLocalPort = m_localPortEdit->text().toUShort();
	QHostAddress bindAddress(requestedLocalIp);
	QString effectiveLocalIp = requestedLocalIp;
	if (!isAddressAssignedToThisHost(bindAddress))
	{
		qWarning().noquote()
			<< QStringLiteral("[UDP][WARN] localIp=%1 不在本机地址列表中，改为绑定 0.0.0.0:%2；本机IPv4=%3")
				.arg(requestedLocalIp)
				.arg(requestedLocalPort)
				.arg(localIpv4Summary());
		bindAddress = QHostAddress::AnyIPv4;
		effectiveLocalIp = QStringLiteral("0.0.0.0");
	}
	bool bound = m_udpSocket->bind(bindAddress, requestedLocalPort);
	if (!bound && effectiveLocalIp != QStringLiteral("0.0.0.0"))
	{
		const QString firstError = m_udpSocket->errorString();
		qWarning().noquote()
			<< QStringLiteral("[UDP][WARN] 绑定 %1:%2 失败：%3；重试 0.0.0.0:%2")
				.arg(effectiveLocalIp)
				.arg(requestedLocalPort)
				.arg(firstError);
		m_udpSocket->close();
		bindAddress = QHostAddress::AnyIPv4;
		effectiveLocalIp = QStringLiteral("0.0.0.0");
		bound = m_udpSocket->bind(bindAddress, requestedLocalPort);
	}
	if (!bound) {
		QMessageBox::warning(this, QStringLiteral("UDP绑定失败"),
			QString(QStringLiteral("无法绑定到 %1:%2\n错误：%3\n本机IPv4：%4"))
				.arg(effectiveLocalIp)
				.arg(requestedLocalPort)
				.arg(m_udpSocket->errorString())
				.arg(localIpv4Summary()));
	}
	else
	{
		m_localIpEdit->setText(effectiveLocalIp);
		qInfo().noquote()
			<< QStringLiteral("[UDP] bound local=%1:%2 requestedLocal=%3 remote=%4:%5")
				.arg(effectiveLocalIp)
				.arg(requestedLocalPort)
				.arg(requestedLocalIp)
				.arg(m_remoteIpEdit->text().trimmed())
				.arg(m_remotePortEdit->text().trimmed());
	}

	connect(m_udpSocket, &QUdpSocket::readyRead, [=]() {
		while (m_udpSocket->hasPendingDatagrams()) {
			QByteArray datagram;
			datagram.resize(m_udpSocket->pendingDatagramSize());
			QHostAddress sender;
			quint16 senderPort;
			m_udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

			if (datagram.size() >= sizeof(int)) {
				int flag = *reinterpret_cast<const int*>(datagram.data());
				if (flag == 0x37) { // 初始化应答
					qInfo().noquote()
						<< QStringLiteral("[StimInitAck] received=1 from=%1:%2 bytes=%3")
							.arg(sender.toString()).arg(senderPort).arg(datagram.size());
					m_lastReceivedLabel->setText(QString(QStringLiteral("↓ 接收: 初始化应答 (0x37) 来自 %1:%2"))
						.arg(sender.toString()).arg(senderPort));
					m_statusLabel->setText(QStringLiteral("● 状态: 初始化完成 | 等待 START/首个有效实时位置（INIT 仅预热）"));
					m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
					emit initAckReceived();
				}
			}
		}
	});
}

void MainWindow::sendControlCommand(int command)
{
	BYHWICD::ControlP2cX1ObjTrackingCmd cmd = {};
	cmd.flag = 0x41;
	cmd.JB = 1; // 红方
	cmd.platID = m_protocolPlatID;
	cmd.simCommand = command;
	//cmd.roundCut = m_roundCutEdit->text().toInt();
	//cmd.currentRound = m_currentRoundEdit->text().toInt();
	cmd.roundCut = 1;
	cmd.currentRound = 1;
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_ddsStim)
	{
		std::string error;
		if (!m_ddsStim->sendControl(cmd, error))
			qCritical().noquote() << QStringLiteral("[StimDDS][ERROR] type=control reason=%1")
				.arg(QString::fromStdString(error));
		else
			qInfo().noquote() << QStringLiteral("[StimDDS] type=control command=%1 sent=1").arg(command);
	}
#endif
	if (m_udpSocket)
	{
		QHostAddress remoteIp(m_remoteIpEdit->text());
		quint16 remotePort = m_remotePortEdit->text().toUShort();
		qint64 sent = m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&cmd), sizeof(cmd), remoteIp, remotePort);

		QString cmdStr = (command == 1) ? QStringLiteral("复位") : (command == 2) ? QStringLiteral("开始") : QStringLiteral("停止");
		m_lastSentLabel->setText(QString(QStringLiteral("↑ 发送: 控制命令 %1 (0x%2) | %3 bytes"))
			.arg(cmdStr).arg(cmd.flag, 0, 16).arg(sent));

		if (sent < 0) {
			m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！检查网络配置"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		}
		qDebug() << "Sent Control Command:" << cmdStr << "Bytes:" << sent;
	}
	else if (m_controlTransport == QStringLiteral("udp") || m_controlTransport == QStringLiteral("both"))
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！ | udpSocket错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	

	
}

void MainWindow::sendInitCommand()
{
	const int selectedTargetType = m_targetTypeBox->currentData().toInt();
	const bool selectedTargetTypeSupported =
		(selectedTargetType == 0x11 || selectedTargetType == 0x12 ||
		 selectedTargetType == 0x22 || selectedTargetType == 0x33 ||
		 selectedTargetType == 0x44 || selectedTargetType == 0x55 ||
		 selectedTargetType == 0x66);
	if (!selectedTargetTypeSupported)
	{
		const QString message = QStringLiteral(
			"目标类型无效：%1；支持 0x11/0x12/0x22/0x33/0x44/0x55/0x66")
			.arg(targetTypeHex(selectedTargetType));
		m_statusLabel->setText(QStringLiteral("● 状态: 初始化失败 | ") + message);
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		qCritical().noquote() << QStringLiteral("[StimInit][ERROR] targetType=%1 reason=unsupported_target_type")
			.arg(targetTypeHex(selectedTargetType));
		return;
	}
	// The ordinary target selector is part of the production UI/INI contract.
	// Freeze its value at INIT so the same identity is used for the whole round.
	m_targetType = selectedTargetType;
	m_forceVisibleForDemo = m_forceVisibleForDemoCheck->isChecked();

	BYHWICD::InitP2cObjectTrackingCmd cmd = {};
	cmd.flag = 0x36;
	cmd.JB = 1;
	cmd.platID = m_protocolPlatID;
	cmd.sensorID = m_protocolSensorID;
    //cmd.platNumValid = 1;

//	// 从UI实时读取初始位置
//	cmd.platParam[0].id = 1;
//	cmd.platParam[0].type = 0x11;
//	cmd.platParam[0].spatial.lat = m_latEdit->text().toDouble();
//	cmd.platParam[0].spatial.lon = m_lonEdit->text().toDouble();
//	cmd.platParam[0].spatial.alt = m_altEdit->text().toDouble();
//	cmd.platParam[0].spatial.yaw = m_yawEdit->text().toDouble();
//	cmd.platParam[0].spatial.pitch = m_pitchEdit->text().toDouble();
//	cmd.platParam[0].spatial.roll = m_rollEdit->text().toDouble();
//	cmd.platParam[0].spatial.speed = 0.0;

//	// 传感器参数（简化配置）
//	cmd.trackingInit.enable = true;
//	cmd.trackingInit.envTerrain = 0; // 戈壁
//	cmd.trackingInit.envSky = 0;    // 晴
//	cmd.trackingInit.envTemp = 25.0;
//	cmd.trackingInit.videoFps = 30;
//	cmd.trackingInit.trackerSensor[0].index = 0;
//	cmd.trackingInit.trackerSensor[0].trackerSensorBand = 2; // 中波红外
//	cmd.trackingInit.trackerSensor[0].trackerSensorWidth = 640;
//    cmd.trackingInit.trackerSensor[0].trackerSensorHeight = 512;//hml
//	cmd.trackingInit.trackerSensor[0].coarseTrackEn = true;
//	cmd.trackingInit.trackerSensor[0].preciseTrackEn = true;
//	cmd.trackingInit.trackerSensor[0].coarseTrackResolution = m_fovHEdit->text().toDouble();
//	cmd.trackingInit.trackerSensor[0].preciseTrackResolution = m_fovVEdit->text().toDouble();

    // 从UI实时读取初始位置
    cmd.platParamInit.id = m_protocolPlatID;
    cmd.platParamInit.type = 1;
    cmd.platParamInit.spatial.lat = realTimeData.at(0).platPos.lat;
    cmd.platParamInit.spatial.lon = realTimeData.at(0).platPos.lon;
    cmd.platParamInit.spatial.alt = realTimeData.at(0).platPos.alt;
    cmd.platParamInit.spatial.yaw = realTimeData.at(0).platEul.yaw;
    cmd.platParamInit.spatial.pitch = realTimeData.at(0).platEul.pitch;
    cmd.platParamInit.spatial.roll = realTimeData.at(0).platEul.roll;
    cmd.platParamInit.spatial.speed = realTimeData.at(0).platSpeed;

    // 传感器参数（简化配置）
    cmd.trackingInit.enable = true;
    cmd.trackingInit.envTerrain = 0; // 戈壁
    // P1 weather A/B must honor the existing protocol field selected by the
    // CLI/UI test control.  The previous hard-coded Overcast value made every
    // --env-sky run exercise the same weather and invalidated Clear/Cloudy
    // performance comparisons without changing the packet layout.
    cmd.trackingInit.envSky = m_protocolEnvSky;
    cmd.trackingInit.envTemp = 25.0;
	cmd.trackingInit.simMode = m_simModeBox?m_simModeBox->currentData().toInt():m_protocolSimMode;
	cmd.trackingInit.videoFps = targetVideoFps();

    cmd.trackingInit.envVisibility = m_protocolEnvVisibilityM;
    cmd.trackingInit.envHumidity = m_protocolEnvHumidityPercent;
    cmd.trackingInit.envWindV = 8;
    cmd.trackingInit.envWindDir = 30;
    cmd.trackingInit.envRadScaleSky = 1.0;
    cmd.trackingInit.envRadScaleTerrain = 1.0;
    ApplyOrdinaryWeatherInitialization(cmd.trackingInit,m_networkConfigPath);


    QString sensorError;
    if(!sensorFormSnapshot(cmd.trackingInit.trackerSensor[0],sensorError)){
        qCritical()<<"[StimInit] invalid sensor form"<<sensorError;return;
    }
	QJsonObject coverageAudit;
	QString coverageError;
	if (!validateFormalAtmosphereCoverage(
		cmd.trackingInit, cmd.trackingInit.trackerSensor[0], coverageError, coverageAudit))
	{
		showInitCoverageError(coverageError, coverageAudit);
		return;
	}
    QJsonObject sensorAudit;
    for(int i=0;i<HwaSensorFields::count;++i){const auto& field=HwaSensorFields::fields()[i];
        sensorAudit.insert(QString::fromLatin1(field.name),HwaSensorFields::get(cmd.trackingInit.trackerSensor[0],field));}
    sensorAudit.insert("simMode",cmd.trackingInit.simMode);sensorAudit.insert("videoFps",cmd.trackingInit.videoFps);
    qInfo().noquote()<<"[P7SensorConstructed]"<<QJsonDocument(sensorAudit).toJson(QJsonDocument::Compact);
    const QString uiDump=qEnvironmentVariable("P7SenderUiDump");
    if(!uiDump.isEmpty())QTimer::singleShot(250,this,[this,uiDump,sensorAudit]{
        QJsonObject report;report.insert("fields",sensorAudit);
        report.insert("logicalWidth",width());report.insert("logicalHeight",height());report.insert("devicePixelRatio",devicePixelRatioF());
        report.insert("screenLogicalWidth",QGuiApplication::primaryScreen()->geometry().width());report.insert("screenLogicalHeight",QGuiApplication::primaryScreen()->geometry().height());
        report.insert("logicalDpi",QGuiApplication::primaryScreen()->logicalDotsPerInch());
        report.insert("captureKind","actual_widget_on_current_screen");
        report.insert("sendStepMs",m_timeStep->text());report.insert("inputFile",m_inputDataPath);report.insert("config",m_networkConfigPath);
        QJsonObject controls;for(auto it=m_sensorControls.begin();it!=m_sensorControls.end();++it){
            QJsonObject entry;entry.insert("class",it.value()->metaObject()->className());entry.insert("enabled",it.value()->isEnabled());
            entry.insert("visible",it.value()->isVisible());entry.insert("value",sensorAudit.value(it.key()));controls.insert(it.key(),entry);
        }report.insert("controls",controls);
        grab().save(uiDump);QFile output(uiDump+".json");if(output.open(QIODevice::WriteOnly))output.write(QJsonDocument(report).toJson());
    });

    cmd.MissileMaxCount120 = 3;
    cmd.MissileMaxCount9 = 3;
    cmd.MissileMaxCountF35 = 3;
    cmd.MissileMaxCountF22 = 3;
	cmd.MissileMaxCountMMD = 0;
	// Reserved pools are selected by the visible UI identity.  A hidden process
	// environment variable must not silently change production INIT or realtime keys.
	cmd.MissileMaxCountResv1 = m_targetType == 0x55 ? 1 : 0;
	cmd.MissileMaxCountResv2 = m_targetType == 0x66 ? 1 : 0;
	qInfo().noquote() << QStringLiteral(
		"[StimTargetPool] requestedType=%1 source=visible_ui_selection resv1Count=%2 resv2Count=%3 protocolLayoutUnchanged=1 hiddenEnvOverride=disabled")
		.arg(targetTypeHex(m_targetType))
		.arg(cmd.MissileMaxCountResv1)
		.arg(cmd.MissileMaxCountResv2);
	qInfo().noquote() << QStringLiteral(
		"[StimViewPolicy] event=init_frozen source=visible_ui_selection targetType=%1 inputPolicy=%2 productionFilterUnchanged=1 originalFileModified=0")
		.arg(targetTypeHex(m_targetType))
		.arg(m_forceVisibleForDemo ? QStringLiteral("force_visible_demo") : QStringLiteral("follow_input_view_valid"));

	bool ddsInitSent = false;
	bool udpInitSent = false;
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_ddsStim)
	{
		std::string error;
		if (!m_ddsStim->sendInit(cmd, error))
			qCritical().noquote() << QStringLiteral("[StimDDS][ERROR] type=init reason=%1")
				.arg(QString::fromStdString(error));
		else
		{
			ddsInitSent = true;
			qInfo().noquote() << QStringLiteral("[StimDDS] type=init sent=1 platID=%1 sensorID=%2")
				.arg(cmd.platID).arg(cmd.sensorID);
		}
	}
#endif

	if (m_udpSocket)
	{
		QHostAddress remoteIp(m_remoteIpEdit->text());
		quint16 remotePort = m_remotePortEdit->text().toUShort();
		qint64 sent = m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&cmd), sizeof(cmd), remoteIp, remotePort);
		udpInitSent = sent >= 0;

		m_lastSentLabel->setText(QString(QStringLiteral("↑ 发送: 初始化命令 (0x36) | %1 bytes")).arg(sent));
		m_statusLabel->setText(QStringLiteral("● 状态: 已发送初始化 | 等待边缘端应答"));
		m_statusLabel->setStyleSheet("color: #FF9800; font-weight: bold;");

		if (sent < 0) {
			m_statusLabel->setText(QStringLiteral("● 状态: 初始化发送失败！"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		}

		qDebug() << "Sent Init Command, Bytes:" << sent;
		qInfo().noquote()
			<< QStringLiteral("[StimInit] platID=%1 sensorID=%2 simMode=%3 videoFps=%4 h264En=%5 envSky=%6 sensorBand=%7 bytes=%8")
				.arg(cmd.platID)
				.arg(cmd.sensorID)
				.arg(cmd.trackingInit.simMode)
				.arg(cmd.trackingInit.videoFps)
				.arg(cmd.trackingInit.trackerSensor[0].h264En ? 1 : 0)
				.arg(cmd.trackingInit.envSky)
				.arg(cmd.trackingInit.trackerSensor[0].trackerSensorBand)
				.arg(sent);
	}
	else if (m_controlTransport == QStringLiteral("udp") || m_controlTransport == QStringLiteral("both"))
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 初始化发送失败！ | udpSocket错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	else if (ddsInitSent)
	{
		m_lastSentLabel->setText(QStringLiteral("↑ 发送: DDS 初始化命令 (0x36)"));
		m_statusLabel->setText(QStringLiteral("● 状态: 已发送 DDS 初始化 | 等待渲染端应答"));
		m_statusLabel->setStyleSheet("color: #FF9800; font-weight: bold;");
	}
	const bool initSent = ddsInitSent || udpInitSent;
	if (initSent)
	{
		m_initSelectionFrozen = true;
		m_targetTypeBox->setEnabled(false);
		m_forceVisibleForDemoCheck->setEnabled(false);
	}
	


	initStepSimData();
}

bool MainWindow::validateFormalAtmosphereCoverage(
	const BYHWICD::InitObjectTrackingParam& initialization,
	const BYHWICD::trackerSensorParam& sensor,
	QString& error,
	QJsonObject& audit) const
{
	audit.insert(QStringLiteral("inputFile"), QFileInfo(m_inputDataPath).absoluteFilePath());
	audit.insert(QStringLiteral("band"), sensor.trackerSensorBand);
	audit.insert(QStringLiteral("rows"), realTimeData.size());
	audit.insert(QStringLiteral("visibilityKm"), initialization.envVisibility / 1000.0);
	audit.insert(QStringLiteral("relativeHumidityPercent"), initialization.envHumidity);
	audit.insert(QStringLiteral("utcHour"), m_testUtcHour);
	audit.insert(QStringLiteral("timeSource"), m_simulationTimeSource);
	audit.insert(QStringLiteral("utcDate"), m_simulationUtcDate.toString(QStringLiteral("yyyy-MM-dd")));
	audit.insert(QStringLiteral("dateSource"), m_simulationDateSource);
	audit.insert(QStringLiteral("formalLut"), m_formalAtmosphereLutPath);
	audit.insert(QStringLiteral("coverageManifest"), m_atmosphereCoverageManifestPath);

	// Other bands use their own compatibility tables.  Explicit camera fixtures
	// remain diagnostic-only and are validated by the renderer that owns them.
	if (sensor.trackerSensorBand != 0 && sensor.trackerSensorBand != 2)
	{
		audit.insert(QStringLiteral("result"), QStringLiteral("not_applicable_non_swir_mwir"));
		qInfo().noquote() << QStringLiteral("[StimAtmosphereCoverage] result=NOT_APPLICABLE band=%1 reason=non_swir_mwir")
			.arg(sensor.trackerSensorBand);
		return true;
	}
	if (!qEnvironmentVariable("WeatherCameraInput").trimmed().isEmpty())
	{
		audit.insert(QStringLiteral("result"), QStringLiteral("explicit_fixture_deferred"));
		qInfo().noquote() << QStringLiteral("[StimAtmosphereCoverage] result=DEFERRED source=WeatherCameraInput reason=effective_geometry_owned_by_explicit_fixture");
		return true;
	}

	QString hashError;
	const QString manifestHash = sha256File(m_atmosphereCoverageManifestPath, hashError);
	if (manifestHash.isEmpty() || manifestHash != m_expectedAtmosphereCoverageManifestSha256)
	{
		error = manifestHash.isEmpty() ? hashError : QStringLiteral(
			"大气覆盖 manifest 身份不匹配：实际 %1，配置要求 %2。INIT 未发送。")
			.arg(manifestHash, m_expectedAtmosphereCoverageManifestSha256);
		audit.insert(QStringLiteral("failureAxis"), QStringLiteral("coverageManifestSha256"));
		audit.insert(QStringLiteral("actualManifestSha256"), manifestHash);
		return false;
	}
	QFile manifestFile(m_atmosphereCoverageManifestPath);
	if (!manifestFile.open(QIODevice::ReadOnly))
	{
		error = QStringLiteral("无法读取大气覆盖 manifest：%1。INIT 未发送。").arg(manifestFile.errorString());
		return false;
	}
	QJsonParseError parseError;
	const QJsonDocument manifestDocument = QJsonDocument::fromJson(manifestFile.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !manifestDocument.isObject())
	{
		error = QStringLiteral("大气覆盖 manifest 不是有效 JSON：%1。INIT 未发送。").arg(parseError.errorString());
		return false;
	}
	const QJsonObject manifest = manifestDocument.object();
	const QJsonObject dataIdentity = manifest.value(QStringLiteral("dataIdentity")).toObject();
	const QJsonObject immutableInput = manifest.value(QStringLiteral("immutableInput")).toObject();
	const QJsonObject measuredGrid = manifest.value(QStringLiteral("p13MeasuredGrid")).toObject();
	const QString declaredLutHash = dataIdentity.value(QStringLiteral("formalLutSha256")).toString().toLower();
	const QString lutHash = sha256File(m_formalAtmosphereLutPath, hashError);
	if (lutHash.isEmpty() || lutHash != m_expectedFormalAtmosphereLutSha256 || lutHash != declaredLutHash)
	{
		error = lutHash.isEmpty() ? hashError : QStringLiteral(
			"正式 MODTRAN LUT 身份不匹配：实际 %1，配置 %2，manifest %3。INIT 未发送。")
			.arg(lutHash, m_expectedFormalAtmosphereLutSha256, declaredLutHash);
		audit.insert(QStringLiteral("failureAxis"), QStringLiteral("formalLutSha256"));
		audit.insert(QStringLiteral("actualFormalLutSha256"), lutHash);
		return false;
	}
	const QString inputHash = sha256File(m_inputDataPath, hashError);
	const QString declaredInputHash = immutableInput.value(QStringLiteral("sha256")).toString().toLower();
	QString inputRole = QStringLiteral("immutable_original_replay");
	if (inputHash.isEmpty())
	{
		error = hashError;
		audit.insert(QStringLiteral("failureAxis"), QStringLiteral("inputSha256"));
		audit.insert(QStringLiteral("actualInputSha256"), inputHash);
		return false;
	}
	if (inputHash != declaredInputHash)
	{
		bool additionalInputAccepted = false;
		const QJsonArray additionalInputs = manifest.value(
			QStringLiteral("additionalValidatedInputs")).toArray();
		for (const QJsonValue& value : additionalInputs)
		{
			const QJsonObject candidate = value.toObject();
			if (candidate.value(QStringLiteral("sha256")).toString().toLower() != inputHash)
				continue;
			const QString expectedName = candidate.value(QStringLiteral("name")).toString();
			const QString candidateRole = candidate.value(QStringLiteral("role")).toString();
			const int acceptedRows = candidate.value(QStringLiteral("acceptedRows")).toInt(-1);
			const int queryRows = candidate.value(QStringLiteral("productionQueryRows")).toInt(-1);
			const int queryValid = candidate.value(QStringLiteral("productionQueryValid")).toInt(-1);
			const int queryFailures = candidate.value(QStringLiteral("productionQueryFailures")).toInt(-1);
			const bool explicitBoundary = candidate.value(QStringLiteral("notOriginalReplay")).toBool(false) &&
				!candidate.value(QStringLiteral("mayReplaceOriginal1Txt")).toBool(true);
			const bool performanceRole = candidateRole ==
				QStringLiteral("performance_only_300s_complex_weather");
			const bool groundRole = candidateRole ==
				QStringLiteral("p14_ground_truck_mixed_weather_30s");
			const bool provenanceBound = performanceRole
				? candidate.value(QStringLiteral("sourceInputSha256")).toString().toLower() == declaredInputHash
				: groundRole && candidate.value(QStringLiteral("independentGeneratedInput")).toBool(false) &&
					!candidate.value(QStringLiteral("sourceInputBusinessDependency")).toBool(true);
			const bool queryEvidenceBound =
				candidate.value(QStringLiteral("queryManifestSha256")).toString().size() == 64 &&
				candidate.value(QStringLiteral("productionQueryCheckLogSha256")).toString().size() == 64 &&
				queryRows >= realTimeData.size() * 2 && queryValid == queryRows && queryFailures == 0;
			const bool calibrationBound = candidate.value(QStringLiteral("calibrationStatus"))
				.toString() == QStringLiteral("NOT_VERIFIED_CALIBRATION");
			bool environmentBound = true;
			if (groundRole)
			{
				auto containsNumber = [](const QJsonArray& values, double expected) {
					for (const QJsonValue& item : values)
						if (std::abs(item.toDouble(std::numeric_limits<double>::quiet_NaN()) - expected) < 1.0e-6)
							return true;
					return false;
				};
				environmentBound = containsNumber(candidate.value(
					QStringLiteral("validatedVisibilityKm")).toArray(), initialization.envVisibility / 1000.0) &&
					containsNumber(candidate.value(
						QStringLiteral("validatedRelativeHumidityPercent")).toArray(), initialization.envHumidity);
			}
			if (QFileInfo(m_inputDataPath).fileName() != expectedName ||
				(!performanceRole && !groundRole) ||
				acceptedRows != realTimeData.size() || !explicitBoundary || !provenanceBound ||
				!queryEvidenceBound || !calibrationBound || !environmentBound)
			{
				error = QStringLiteral(
					"附加输入身份存在但边界、文件名、行数或逐行生产查询证据不完整；不得改名冒充原 1.txt。INIT 未发送。");
				audit.insert(QStringLiteral("failureAxis"), QStringLiteral("additionalInputContract"));
				audit.insert(QStringLiteral("actualInputSha256"), inputHash);
				return false;
			}
			inputRole = candidateRole;
			additionalInputAccepted = true;
			break;
		}
		if (!additionalInputAccepted)
		{
			error = QStringLiteral(
				"输入文件不属于覆盖 manifest：实际 %1，原始 1.txt 为 %2。不得改名或修改轨迹冒充原 1.txt；INIT 未发送。")
				.arg(inputHash, declaredInputHash);
			audit.insert(QStringLiteral("failureAxis"), QStringLiteral("inputSha256"));
			audit.insert(QStringLiteral("actualInputSha256"), inputHash);
			return false;
		}
	}

	IRModtranRadianceLut lut;
	if (!lut.load(m_formalAtmosphereLutPath.toStdString()))
	{
		error = QStringLiteral("正式 MODTRAN LUT 无法由生产查询器加载。INIT 未发送。");
		audit.insert(QStringLiteral("failureAxis"), QStringLiteral("formalLutLoad"));
		return false;
	}
	const double visibilityKm = initialization.envVisibility / 1000.0;
	const double humidity = initialization.envHumidity;
	const IRBand requestedBand = sensor.trackerSensorBand == 0
		? IRBand::ShortWaveInfrared : IRBand::MidWaveInfrared;
	const std::string atmosphereModel = measuredGrid.value(QStringLiteral("atmosphereModel"))
		.toString(QStringLiteral("Mid-Latitude Summer")).toStdString();
	const std::string aerosolModel = measuredGrid.value(QStringLiteral("aerosolModel"))
		.toString(QStringLiteral("Rural")).toStdString();

	double minRange = std::numeric_limits<double>::infinity();
	double maxRange = 0.0;
	double minObserverAlt = std::numeric_limits<double>::infinity();
	double maxObserverAlt = -std::numeric_limits<double>::infinity();
	double minTargetAlt = std::numeric_limits<double>::infinity();
	double maxTargetAlt = -std::numeric_limits<double>::infinity();
	double minZenith = std::numeric_limits<double>::infinity();
	double maxZenith = -std::numeric_limits<double>::infinity();
	double minTau = std::numeric_limits<double>::infinity();
	double maxTau = 0.0;
	QString interpolationMode;
	const QDateTime currentUtc = QDateTime::currentDateTimeUtc();
	const QDate currentUtcDate = m_simulationUtcDate.isValid() ? m_simulationUtcDate : currentUtc.date();
	const double configuredHour = m_testUtcHour >= 0.0
		? m_testUtcHour
		: currentUtc.time().msecsSinceStartOfDay() / 3600000.0;
	for (int index = 0; index < realTimeData.size(); ++index)
	{
		const realtimeInfo& row = realTimeData.at(index);
		const double rangeM = geodeticLosMeters(row.platPos, row.tarPos);
		minRange = std::min(minRange, rangeM);
		maxRange = std::max(maxRange, rangeM);
		minObserverAlt = std::min(minObserverAlt, row.platPos.alt);
		maxObserverAlt = std::max(maxObserverAlt, row.platPos.alt);
		minTargetAlt = std::min(minTargetAlt, row.tarPos.alt);
		maxTargetAlt = std::max(maxTargetAlt, row.tarPos.alt);

		IRSolarPositionInput solarInput;
		solarInput.latitudeDeg = row.platPos.lat;
		solarInput.longitudeDeg = row.platPos.lon;
		solarInput.altitudeM = row.platPos.alt;
		solarInput.utcYear = currentUtcDate.year();
		solarInput.utcMonth = currentUtcDate.month();
		solarInput.utcDay = currentUtcDate.day();
		solarInput.utcHour = configuredHour;
		const IRSolarPositionOutput solar = IRSolarPosition().evaluate(solarInput);
		const double zenith = solar.zenithDeg;
		minZenith = std::min(minZenith, zenith);
		maxZenith = std::max(maxZenith, zenith);

		IRModtranRadianceQuery query;
		query.band = requestedBand;
		query.atmosphereModel = atmosphereModel;
		query.aerosolModel = aerosolModel;
		query.observerAltKm = row.platPos.alt / 1000.0;
		query.targetAltKm = row.tarPos.alt / 1000.0;
		query.rangeKm = rangeM / 1000.0;
		query.visibilityKm = visibilityKm;
		query.solarZenithDeg = zenith;
		const IRModtranRadianceResult result = (solar.valid && std::isfinite(zenith))
			? lut.queryRelativeHumidity(query, humidity)
			: IRModtranRadianceResult();
		if (!solar.valid || !result.valid)
		{
			audit.insert(QStringLiteral("result"), QStringLiteral("rejected"));
			audit.insert(QStringLiteral("failureAxis"), QString::fromStdString(result.fallbackAxis));
			audit.insert(QStringLiteral("failureReason"), solar.valid
				? QString::fromStdString(result.fallbackReason) : QStringLiteral("invalid_solar_position"));
			audit.insert(QStringLiteral("failureValue"), result.fallbackQuery);
			audit.insert(QStringLiteral("failureMinimum"), result.fallbackMin);
			audit.insert(QStringLiteral("failureMaximum"), result.fallbackMax);
			audit.insert(QStringLiteral("firstInvalidDataRow"), row.sourceLine);
			error = QStringLiteral(
				"正式 SWIR/MWIR LUT 不覆盖原输入第 %1 行：reason=%2 axis=%3 value=%4 bounds=%5..%6；"
				"必须补齐真实 MODTRAN 数据，不能夹距、填零或改成 tau=1。INIT 未发送。")
				.arg(row.sourceLine)
				.arg(solar.valid ? QString::fromStdString(result.fallbackReason)
					: QStringLiteral("invalid_solar_position"))
				.arg(QString::fromStdString(result.fallbackAxis))
				.arg(result.fallbackQuery, 0, 'g', 12)
				.arg(result.fallbackMin, 0, 'g', 12)
				.arg(result.fallbackMax, 0, 'g', 12);
			return false;
		}
		minTau = std::min(minTau, result.tauUp);
		maxTau = std::max(maxTau, result.tauUp);
		interpolationMode = QString::fromStdString(result.interpolationMode);
	}
	audit.insert(QStringLiteral("result"), QStringLiteral("accepted"));
	audit.insert(QStringLiteral("validationMethod"), QStringLiteral("production_IRModtranRadianceLut_query_every_input_row"));
	audit.insert(QStringLiteral("formalLutSha256"), lutHash);
	audit.insert(QStringLiteral("coverageManifestSha256"), manifestHash);
	audit.insert(QStringLiteral("inputSha256"), inputHash);
	audit.insert(QStringLiteral("inputRole"), inputRole);
	audit.insert(QStringLiteral("validQueries"), realTimeData.size());
	audit.insert(QStringLiteral("rangeMinM"), minRange);
	audit.insert(QStringLiteral("rangeMaxM"), maxRange);
	audit.insert(QStringLiteral("observerAltitudeMinM"), minObserverAlt);
	audit.insert(QStringLiteral("observerAltitudeMaxM"), maxObserverAlt);
	audit.insert(QStringLiteral("targetAltitudeMinM"), minTargetAlt);
	audit.insert(QStringLiteral("targetAltitudeMaxM"), maxTargetAlt);
	audit.insert(QStringLiteral("solarZenithMinDeg"), minZenith);
	audit.insert(QStringLiteral("solarZenithMaxDeg"), maxZenith);
	audit.insert(QStringLiteral("tauMin"), minTau);
	audit.insert(QStringLiteral("tauMax"), maxTau);
	audit.insert(QStringLiteral("interpolationMode"), interpolationMode);
	qInfo().noquote() << QStringLiteral("[StimAtmosphereCoverage] result=ACCEPTED band=%1 rows=%2 validQueries=%3 rangeM=%4..%5 observerAltM=%6..%7 targetAltM=%8..%9 visibilityKm=%10 humidityPercent=%11 solarZenithDeg=%12..%13 tau=%14..%15 mode=%16 lutSha256=%17 manifestSha256=%18 inputSha256=%19 inputRole=%20 timeSource=%21")
		.arg(sensor.trackerSensorBand).arg(realTimeData.size())
		.arg(realTimeData.size()).arg(minRange, 0, 'f', 3).arg(maxRange, 0, 'f', 3)
		.arg(minObserverAlt, 0, 'f', 3).arg(maxObserverAlt, 0, 'f', 3)
		.arg(minTargetAlt, 0, 'f', 3).arg(maxTargetAlt, 0, 'f', 3)
		.arg(visibilityKm, 0, 'f', 3).arg(humidity, 0, 'f', 3)
		.arg(minZenith, 0, 'f', 3).arg(maxZenith, 0, 'f', 3)
		.arg(minTau, 0, 'g', 10).arg(maxTau, 0, 'g', 10).arg(interpolationMode)
		.arg(lutHash, manifestHash, inputHash, inputRole, m_simulationTimeSource);
	return true;
}

void MainWindow::showInitCoverageError(const QString& error, const QJsonObject& audit)
{
	m_statusLabel->setText(QStringLiteral("● 状态: INIT 未发送 | 大气覆盖域错误"));
	m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	m_lastSentLabel->setText(QStringLiteral("↑ 未发送: INIT 被正式大气域预检拒绝"));
	m_lastReceivedLabel->setText(QStringLiteral("↓ 处理建议: 补齐正式 MODTRAN 网格或修正身份；不得替换原 1.txt"));
	qCritical().noquote() << QStringLiteral("[StimAtmosphereCoverage][ERROR] %1 audit=%2")
		.arg(error, QString::fromUtf8(QJsonDocument(audit).toJson(QJsonDocument::Compact)));
	QMessageBox* box = new QMessageBox(
		QMessageBox::Critical,
		QStringLiteral("INIT 未发送：正式大气域不覆盖"),
		error,
		QMessageBox::Ok,
		this);
	box->setAttribute(Qt::WA_DeleteOnClose);
	box->setModal(false);
	box->show();
	const QString uiDump = qEnvironmentVariable("P7SenderUiDump");
	if (!uiDump.isEmpty())
	{
		QTimer::singleShot(250, this, [this, uiDump, audit]() {
			grab().save(uiDump);
			QFile output(uiDump + ".json");
			if (output.open(QIODevice::WriteOnly)) output.write(QJsonDocument(audit).toJson());
		});
	}
}

void MainWindow::sendRealTimeData()
{
	BYHWICD::DisplayC2cObjTrackingData data = {};
	data.flag = 0x38;
	data.platID = m_protocolPlatID;
	data.sensorID = m_protocolSensorID;
	if (m_testUtcHour >= 0.0)
	{
		QDateTime utc(m_simulationUtcDate, QTime(0, 0), Qt::UTC);
		const int totalMs = qBound(0, static_cast<int>(m_testUtcHour * 3600000.0), 86399999);
		utc.setTime(QTime(0, 0).addMSecs(totalMs));
		// Preserve the immutable source time scale even when rows are paced at 60 Hz.
		// The first source row maps to the configured UTC instant; no row is skipped.
		const realtimeInfo& currentSample = realTimeData.at(dataNum - 1);
		data.time = utc.toMSecsSinceEpoch() +
			(currentSample.sourceTimeMs - realTimeData.first().sourceTimeMs);
	}
	else
	{
		data.time = QDateTime::currentMSecsSinceEpoch();
	}


	// 使用当前累积位置（关键：发送前使用当前值）
	data.platLoc.lat = m_currPlane_pos.x;
	data.platLoc.lon = m_currPlane_pos.y;
	data.platLoc.alt = m_currPlane_pos.z;
	data.platLoc.yaw = m_currPlane_att.yaw;
	data.platLoc.pitch = m_currPlane_att.pitch;
	data.platLoc.roll = m_currPlane_att.roll;
    data.platLoc.speed = realTimeData.at(dataNum-1).platSpeed;

	// Wg信息
	data.weaponState.targetType = m_targetType;
	data.weaponState.targetPlatID = 3;
    data.weaponState.targetID = 3;
	data.weaponState.xxOutAng[0] = 0.0;
	data.weaponState.xxOutAng[1] = 0.0;
	data.weaponState.lookatEn = true;
	bool illuminatorEnabled = m_protocolIlluminatorForceEnabled != 0;
	if (m_protocolIlluminatorOnStartSec >= 0.0)
	{
		illuminatorEnabled = current_time >= m_protocolIlluminatorOnStartSec &&
			(m_protocolIlluminatorOnEndSec < 0.0 || current_time < m_protocolIlluminatorOnEndSec);
	}
	data.weaponState.illuminatorEn = illuminatorEnabled;
//    if(current_time > 5){
//        //5秒后發動機熄火
//        data.weaponState.strikeFlag = true;
//        data.weaponState.strikePart=2;
//    }else{
//        data.weaponState.strikeFlag = false;
//    }
	const realtimeInfo& currentSample = realTimeData.at(dataNum - 1);
	data.weaponState.strikeFlag = m_strikeFlagOverride >= 0
		? m_strikeFlagOverride != 0 : currentSample.strikeFlag;
	data.weaponState.strikePart = m_testStrikePart;
	const bool inputViewValid = currentSample.viewValid;
	const bool effectiveViewValid = m_forceVisibleForDemo || inputViewValid;
	data.weaponState.viewValid = effectiveViewValid;


	// 目标状态（相对平台偏移）
    data.targetNumValid = 1/*5*/;
	data.targetState[0].targetType = m_targetType;
	data.targetState[0].targetPlatID = 3;
	data.targetState[0].targetID = 3;
	if (m_sentFrameCount <= 3 || (m_sentFrameCount % 120) == 0)
	{
		qInfo().noquote() << QStringLiteral(
			"[StimTargetSelection] sourceSeq=%1 targetType=%2 source=%3 targetPlatID=%4 targetID=%5")
			.arg(m_sentFrameCount + 1)
			.arg(targetTypeHex(data.targetState[0].targetType))
			.arg(QStringLiteral("init_frozen_visible_ui_selection"))
			.arg(data.targetState[0].targetPlatID)
			.arg(data.targetState[0].targetID);
	}
//    if(current_time > 5){
//        //5秒后發動機熄火
		data.targetState[0].engineState = m_engineStateOverride >= 0
			? m_engineStateOverride != 0 : true;
//    }else{
//        data.targetState[0].engineState = false;
//    }

	data.targetState[0].viewValid = effectiveViewValid;
	if (m_sentFrameCount < 3 || inputViewValid != effectiveViewValid ||
		(m_sentFrameCount % 120) == 0)
	{
		qInfo().noquote() << QStringLiteral(
			"[StimViewPolicy] sourceSeq=%1 sourceLine=%2 inputViewValid=%3 effectiveViewValid=%4 policy=%5 geometryRowAccepted=1 noRowSkip=1")
			.arg(m_sentFrameCount + 1)
			.arg(currentSample.sourceLine)
			.arg(inputViewValid ? 1 : 0)
			.arg(effectiveViewValid ? 1 : 0)
			.arg(m_forceVisibleForDemo ? QStringLiteral("force_visible_demo") : QStringLiteral("follow_input_view_valid"));
		m_statusLabel->setText(!inputViewValid && !m_forceVisibleForDemo
			? QStringLiteral("● 状态: 位置有效；源显示标志为 0，目标隐藏（业务行仍发送）")
			: (!inputViewValid
				? QStringLiteral("● 状态: 位置有效；演示强制显示已覆盖源标志 0")
				: QStringLiteral("● 状态: 位置有效；显示标志为 1")));
		m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
	}

	data.targetState[0].targetLoc.lat = m_currMissile_pos.x;
	data.targetState[0].targetLoc.lon = m_currMissile_pos.y;
	data.targetState[0].targetLoc.alt = m_currMissile_pos.z;
//    adddate = adddate+0.5;
//    if(adddate>360)
//    {
//        adddate=1.0;
//    }
    data.targetState[0].targetLoc.yaw = m_currMissile_att.yaw/*+adddate*/;
	data.targetState[0].targetLoc.pitch = m_currMissile_att.pitch;
	data.targetState[0].targetLoc.roll = m_currMissile_att.roll;
	data.targetState[0].targetLoc.speed = targetUsesRedSpeed(data.targetState[0].targetType)
		? currentSample.platSpeed : currentSample.tarSpeed;
	data.targetState[0].targetState = 0x01;
	if (m_sentFrameCount < 3 || dataNum == realTimeData.size() || (m_sentFrameCount % 600) == 0)
	{
		qInfo().noquote() << QStringLiteral("[StimFrameTime] sourceSeq=%1 rowIndex=%2 sourceLine=%3 sourceTimeMs=%4 sourceOffsetMs=%5 simulationEpochMs=%6 sendWallMs=%7 nominalVideoPtsMs=%8 noRowSkip=1")
			.arg(m_sentFrameCount + 1).arg(dataNum - 1).arg(currentSample.sourceLine)
			.arg(currentSample.sourceTimeMs, 0, 'f', 3)
			.arg(currentSample.sourceTimeMs - realTimeData.first().sourceTimeMs, 0, 'f', 3)
			.arg(data.time, 0, 'f', 3)
			.arg(m_sendClock.isValid() ? m_sendClock.elapsed() : 0)
			.arg(static_cast<double>(m_sentFrameCount) * 1000.0 / std::max(1, m_targetVideoFps), 0, 'f', 3);
	}


//    data.targetState[1].targetType = 0x22;
//    data.targetState[1].targetPlatID = 3;
//    data.targetState[1].targetID = 34;
//    data.targetState[1].viewValid = realTimeData.at(dataNum-1).viewValid;
//    data.targetState[1].targetLoc.lat = 0.0;
//    data.targetState[1].targetLoc.lon = 0.0;
//    data.targetState[1].targetLoc.alt = 0.0;
//    data.targetState[1].targetLoc.yaw = 0.0;
//    data.targetState[1].targetLoc.pitch = 0.0;
//    data.targetState[1].targetLoc.roll = 0.0;
//    data.targetState[1].targetState = 0x01;


//    data.targetState[2].targetType = 0x33;
//    data.targetState[2].targetPlatID = 3;
//    data.targetState[2].targetID = 4;
//    data.targetState[2].viewValid = realTimeData.at(dataNum-1).viewValid;
//    data.targetState[2].targetLoc.lat = 0.0;
//    data.targetState[2].targetLoc.lon = 0.0;
//    data.targetState[2].targetLoc.alt = 0.0;
//    data.targetState[2].targetLoc.yaw = 0.0;
//    data.targetState[2].targetLoc.pitch = 0.0;
//    data.targetState[2].targetLoc.roll = 0.0;
//    data.targetState[2].targetState = 0x01;

//    data.targetState[3].targetType = 0x33;
//    data.targetState[3].targetPlatID = 3;
//    data.targetState[3].targetID = 34;
//    data.targetState[3].viewValid = realTimeData.at(dataNum-1).viewValid;
//    data.targetState[3].targetLoc.lat = 0.0;
//    data.targetState[3].targetLoc.lon = 0.0;
//    data.targetState[3].targetLoc.alt = 0.0;
//    data.targetState[3].targetLoc.yaw = 0.0;
//    data.targetState[3].targetLoc.pitch = 0.0;
//    data.targetState[3].targetLoc.roll = 0.0;
//    data.targetState[3].targetState = 0x01;

//    data.targetState[4].targetType = 0x22;
//    data.targetState[4].targetPlatID = 3;
//    data.targetState[4].targetID = 4;
//    data.targetState[4].viewValid = realTimeData.at(dataNum-1).viewValid;
//    data.targetState[4].targetLoc.lat = 0.0;
//    data.targetState[4].targetLoc.lon = 0.0;
//    data.targetState[4].targetLoc.alt = 0.0;
//    data.targetState[4].targetLoc.yaw = 0.0;
//    data.targetState[4].targetLoc.pitch = 0.0;
//	data.targetState[4].targetLoc.roll = 0.0;
//	data.targetState[4].targetState = 0x01;

//	const realtimeInfo& currentSample = realTimeData.at(dataNum - 1);
//	for (int targetIndex = 0; targetIndex < 5; ++targetIndex)
//	{
//		const bool useRedSpeed = targetUsesRedSpeed(data.targetState[targetIndex].targetType);
//		data.targetState[targetIndex].targetLoc.speed = useRedSpeed
//			? currentSample.platSpeed
//			: currentSample.tarSpeed;
//	}

//    if(current_time > 3)
//    {
//        data.targetState[1].targetLoc.lat = m_currMissile_pos.x;
//        data.targetState[1].targetLoc.lon = m_currMissile_pos.y;
//        data.targetState[1].targetLoc.alt = m_currMissile_pos.z + 1.0;
//        data.targetState[1].targetLoc.yaw = m_currMissile_att.yaw;
//        data.targetState[1].targetLoc.pitch = m_currMissile_att.pitch;
//        data.targetState[1].targetLoc.roll = m_currMissile_att.roll;
//    }

//    if(current_time > 5)
//    {
//        data.targetState[2].targetLoc.lat = m_currMissile_pos.x;
//        data.targetState[2].targetLoc.lon = m_currMissile_pos.y+0.00002;
//        data.targetState[2].targetLoc.alt = m_currMissile_pos.z - 1.0;
//        data.targetState[2].targetLoc.yaw = m_currMissile_att.yaw;
//        data.targetState[2].targetLoc.pitch = m_currMissile_att.pitch;
//        data.targetState[2].targetLoc.roll = m_currMissile_att.roll;
//    }

//    if(current_time > 7)
//    {
//        data.targetState[3].targetLoc.lat = m_currMissile_pos.x;
//        data.targetState[3].targetLoc.lon = m_currMissile_pos.y-0.00002;
//        data.targetState[3].targetLoc.alt = m_currMissile_pos.z - 1.0;
//        data.targetState[3].targetLoc.yaw = m_currMissile_att.yaw;
//        data.targetState[3].targetLoc.pitch = m_currMissile_att.pitch;
//        data.targetState[3].targetLoc.roll = m_currMissile_att.roll;
//    }

//    if(current_time > 9)
//    {
//        data.targetState[4].targetLoc.lat = m_currMissile_pos.x;
//        data.targetState[4].targetLoc.lon = m_currMissile_pos.y-0.00002;
//        data.targetState[4].targetLoc.alt = m_currMissile_pos.z;
//        data.targetState[4].targetLoc.yaw = m_currMissile_att.yaw;
//        data.targetState[4].targetLoc.pitch = m_currMissile_att.pitch;
//        data.targetState[4].targetLoc.roll = m_currMissile_att.roll;
//    }
//    if(current_time > 11)
//    {
//         data.weaponState.targetID = 34;
//    }

    //applyPhase4cAeroMachOverride(data);
    if (qEnvironmentVariableIntValue("P5NoTargets") == 1) data.targetNumValid = 0;
    ApplyOrdinaryWeatherInput(data,m_sendClock.isValid()?m_sendClock.elapsed()*.001:0.0);
	bool ddsSent = false;
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_ddsStim)
	{
		std::string error;
		ddsSent = m_ddsStim->sendRealtime(data, error);
		if (!ddsSent)
			qCritical().noquote() << QStringLiteral("[StimDDS][ERROR] type=realtime reason=%1")
				.arg(QString::fromStdString(error));
	}
#endif

	if (m_udpSocket)
	{
		// 发送
		QHostAddress remoteIp(m_remoteIpEdit->text());
		quint16 remotePort = m_remotePortEdit->text().toUShort();
		qint64 sent = m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&data), sizeof(data), remoteIp, remotePort);

		if (sent > 0) {
			++m_sentFrameCount;
			logAeroSpeedSend(data);
			const qint64 nowNs = m_sendClock.isValid() ? m_sendClock.nsecsElapsed() : 0;
			const bool shouldLog = nowNs - m_lastSendPerfLogNs >= 2000000000LL;
			if (shouldLog)
			{
				const qint64 intervalNs = qMax<qint64>(1, nowNs - m_lastSendPerfLogNs);
				const quint64 intervalFrames = m_sentFrameCount - m_lastSendPerfFrameCount;
				const double sentFpsInstant =
					static_cast<double>(intervalFrames) * 1.0e9 / static_cast<double>(intervalNs);
				const double sentFpsAvg =
					static_cast<double>(m_sentFrameCount) /
					qMax(0.001, static_cast<double>(nowNs) / 1.0e9);
				const qint64 expectedSendNs = static_cast<qint64>(
					(static_cast<long double>(m_sentFrameCount - 1) * 1000000000.0L) /
					static_cast<long double>(m_inputHz));
				const double behindMs =
					static_cast<double>(nowNs - expectedSendNs) / 1.0e6;
				qInfo().noquote()
					<< QStringLiteral("[StimPerf] channel=%1 platID=%2 sensorID=%3 pid=%4 targetFps=%5 sentFpsInstant=%6 sentFpsAvg=%7 packetSeq=%8 timerIntervalMs=%9 behindMs=%10")
						.arg(m_channel)
						.arg(m_protocolPlatID)
						.arg(m_protocolSensorID)
						.arg(QCoreApplication::applicationPid())
						.arg(m_inputHz)
						.arg(sentFpsInstant, 0, 'f', 3)
						.arg(sentFpsAvg, 0, 'f', 3)
						.arg(m_sentFrameCount)
						.arg(1000.0 / static_cast<double>(m_inputHz), 0, 'f', 3)
						.arg(behindMs, 0, 'f', 3);
				m_lastSendPerfLogNs = nowNs;
				m_lastSendPerfFrameCount = m_sentFrameCount;
			}
			if (m_sentFrameCount <= 3 || (m_sentFrameCount % m_uiUpdateEveryFrames) == 0)
			{
				m_lastSentLabel->setText(QString(QStringLiteral("↑ 发送: 实时数据 (0x38) | Lat:%1° | %2 bytes"))
					.arg(m_currentLat, 0, 'f', 4).arg(sent));
				m_statusLabel->setText(QString(QStringLiteral("● 状态: 仿真中 | 已发送 %1 帧 | 目标 %2 FPS"))
					.arg(m_sentFrameCount)
					.arg(m_inputHz));
				m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
			}
		}
		else {
			m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！检查网络"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		}
	}
	else if (!ddsSent)
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！ | transport错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	else
	{
		++m_sentFrameCount;
		const qint64 nowNs = m_sendClock.isValid() ? m_sendClock.nsecsElapsed() : 0;
		if (nowNs - m_lastSendPerfLogNs >= 2000000000LL)
		{
			const qint64 intervalNs = qMax<qint64>(1, nowNs - m_lastSendPerfLogNs);
			const quint64 intervalFrames = m_sentFrameCount - m_lastSendPerfFrameCount;
			qInfo().noquote() << QStringLiteral(
				"[StimPerf] transport=dds targetFps=%1 sentFpsInstant=%2 packetSeq=%3 runtimeInitCount=%4")
				.arg(m_inputHz)
				.arg(static_cast<double>(intervalFrames) * 1.0e9 / intervalNs, 0, 'f', 3)
				.arg(m_sentFrameCount)
#if defined(HWASIMIR_HAS_ZRDDS)
				.arg(m_ddsStim ? m_ddsStim->runtimeInitCount() : 0);
#else
				.arg(0);
#endif
			m_lastSendPerfLogNs = nowNs;
			m_lastSendPerfFrameCount = m_sentFrameCount;
		}
	}
	

	// A successful SDK write is not delivery confirmation during a TCP fault.
	// Preserve its raw success counter, stop future inputs, and never replay it.
#if defined(HWASIMIR_HAS_ZRDDS)
    if(m_ddsStim && m_ddsStim->hasTransportFault()) {
        qCritical().noquote()<<QStringLiteral("[StimTransportFault][ERROR] action=stop_input noReplay=1 currentWriteDelivery=unconfirmed apiSuccessCount=%1").arg(m_sentFrameCount);
        onStopButtonClicked();
        m_statusLabel->setText(QStringLiteral("● DDS传输异常：已停止输入，本轮完整性未确认"));
        m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
        return;
    }
#endif
	// 关键：发送后立即更新位置（为下一次发送准备）
	updatePosition();
}

void MainWindow::applyPhase4cAeroMachOverride(BYHWICD::DisplayC2cObjTrackingData& data) const
{
	if (!m_phase4cAeroMachMode)
	{
		return;
	}
	const double altitudeM = m_phase4cAltitudeKm * 1000.0;
	data.platLoc.speed = m_phase4cSpeedKmh;
	for (int targetIndex = 0; targetIndex < qBound(0, data.targetNumValid, 5); ++targetIndex)
	{
		BYHWICD::TargetState& target = data.targetState[targetIndex];
		const double relativeOffsetM = target.targetLoc.alt - m_currMissile_pos.z;
		target.targetLoc.alt = altitudeM + clampDouble(relativeOffsetM, -5.0, 5.0);
		target.targetLoc.speed = m_phase4cSpeedKmh;
	}
}

void MainWindow::logAeroSpeedSend(const BYHWICD::DisplayC2cObjTrackingData& data) const
{
	const bool shouldLog =
		m_sentFrameCount <= 3 ||
		(m_sentFrameCount % 600) == 0;
	if (!shouldLog)
	{
		return;
	}
	const int targetCount = qBound(0, data.targetNumValid, 5);
	for (int targetIndex = 0; targetIndex < targetCount; ++targetIndex)
	{
		const BYHWICD::TargetState& target = data.targetState[targetIndex];
		const bool useRedSpeed = targetUsesRedSpeed(target.targetType);
		const QString sourceColumn = useRedSpeed
			? QStringLiteral("RedSpeedAir(km/h)")
			: QStringLiteral("MissileSpeedAir(km/h)");
		qInfo().noquote()
			<< QStringLiteral("[AeroSpeedSend] sourceSeq=%1 targetIndex=%2 targetID=%3 targetType=%4 speedSourceColumn=%5 speedRawKmh=%6 altitudeM=%7 lat=%8 lon=%9 phase4cAeroMach=%10 altitudeKm=%11 machCommand=%12 speedKmh=%13 speedMps=%14 speedUnit=km/h")
				.arg(m_sentFrameCount)
				.arg(targetIndex)
				.arg(target.targetID)
				.arg(targetTypeHex(target.targetType))
				.arg(m_phase4cAeroMachMode ? QStringLiteral("Phase4CProtocolMach") : sourceColumn)
				.arg(target.targetLoc.speed, 0, 'f', 3)
				.arg(target.targetLoc.alt, 0, 'f', 3)
				.arg(target.targetLoc.lat, 0, 'f', 9)
				.arg(target.targetLoc.lon, 0, 'f', 9)
				.arg(m_phase4cAeroMachMode ? 1 : 0)
				.arg(target.targetLoc.alt / 1000.0, 0, 'f', 3)
				.arg(m_phase4cAeroMachMode ? m_phase4cMach : -1.0, 0, 'f', 3)
				.arg(target.targetLoc.speed, 0, 'f', 3)
				.arg(target.targetLoc.speed / 3.6, 0, 'f', 3);
	}
}

void MainWindow::updatePosition()
{
	
	if (step(m_currPlane_pos, m_currPlane_att, m_currMissile_pos, m_currMissile_att))
	{
		onStopButtonClicked();
	}

	if (m_sentFrameCount <= 3 || (m_sentFrameCount % m_uiUpdateEveryFrames) == 0)
	{
		m_latEdit->setText(QString::number(m_currPlane_pos.x, 'f', 6));
		m_lonEdit->setText(QString::number(m_currPlane_pos.y, 'f', 6));
		m_altEdit->setText(QString::number(m_currPlane_pos.z, 'f', 6));
		m_yawEdit->setText(QString::number(m_currPlane_att.yaw, 'f', 6));
		m_pitchEdit->setText(QString::number(m_currPlane_att.pitch, 'f', 6));
		m_rollEdit->setText(QString::number(m_currPlane_att.roll, 'f', 6));
		m_latEditTarget->setText(QString::number(m_currMissile_pos.x, 'f', 6));
		m_lonEditTarget->setText(QString::number(m_currMissile_pos.y, 'f', 6));
		m_altEditTarget->setText(QString::number(m_currMissile_pos.z, 'f', 6));
		m_yawEditTarget->setText(QString::number(m_currMissile_att.yaw, 'f', 6));
		m_pitchEditTarget->setText(QString::number(m_currMissile_att.pitch, 'f', 6));
		m_rollEditTarget->setText(QString::number(m_currMissile_att.roll, 'f', 6));
	}
}

void MainWindow::onResetButtonClicked()
{
	sendControlCommand(1);
	//initStepSimData();
	

    dataNum = 1;

	const int targetIndex = m_targetTypeBox->findData(m_targetType);
	if (targetIndex >= 0) m_targetTypeBox->setCurrentIndex(targetIndex);
	m_targetTypeBox->setEnabled(true);
	m_forceVisibleForDemoCheck->setEnabled(true);
	m_initSelectionFrozen = false;
	m_fovHEdit->setText(QString::number(m_fovH, 'f', 2));
	m_fovVEdit->setText(QString::number(m_fovV, 'f', 2));
    m_latEdit->setText(QString::number(plane_init_pos.x, 'f', 6));
    m_lonEdit->setText(QString::number(plane_init_pos.y, 'f', 6));
    m_altEdit->setText(QString::number(plane_init_pos.z, 'f', 6));
    m_yawEdit->setText(QString::number(plane_init_attitude.yaw, 'f', 6));
    m_pitchEdit->setText(QString::number(plane_init_attitude.pitch, 'f', 6));
    m_rollEdit->setText(QString::number(plane_init_attitude.roll, 'f', 6));
	m_latEditTarget->setText(QString::number(missile_init_pos.x, 'f', 6));
	m_lonEditTarget->setText(QString::number(missile_init_pos.y, 'f', 6));
	m_altEditTarget->setText(QString::number(missile_init_pos.z, 'f', 6));
	m_yawEditTarget->setText(QString::number(missile_init_attitude.yaw, 'f', 6));
	m_pitchEditTarget->setText(QString::number(missile_init_attitude.pitch, 'f', 6));
	m_rollEditTarget->setText(QString::number(missile_init_attitude.roll, 'f', 6));
//	m_collisionTime->setText(QString::number(collision_time, 'f', 2));
	m_speed->setText(QString::number(plane_speed_y, 'f', 2));
	m_timeStep->setText(QString::number(time_step));

	m_statusLabel->setText(QStringLiteral("● 状态: 已发送复位指令"));
	m_statusLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
}

void MainWindow::onInitButtonClicked()
{
	// 重置当前位置为UI起始值（重要！避免累积误差）
	m_currentLat = m_latEdit->text().toDouble();
	m_currentLon = m_lonEdit->text().toDouble();
	m_currentAlt = m_altEdit->text().toDouble();
	m_currentYaw = m_yawEdit->text().toDouble();
	m_currentPitch = m_pitchEdit->text().toDouble();
	m_currentRoll = m_rollEdit->text().toDouble();

    dataNum = 1;

	sendInitCommand();
}

void MainWindow::onStartButtonClicked()
{
	// 关键：开始前同步UI当前值到内部变量（解决起始点问题）
	m_currentLat = m_latEdit->text().toDouble();
	m_currentLon = m_lonEdit->text().toDouble();
	m_currentAlt = m_altEdit->text().toDouble();
	m_currentYaw = m_yawEdit->text().toDouble();
	m_currentPitch = m_pitchEdit->text().toDouble();
	m_currentRoll = m_rollEdit->text().toDouble();

	sendControlCommand(2); // 发送开始命令

#if defined(HWASIMIR_HAS_ZRDDS)
	// START is accepted on an independent DDS topic.  Wait for the renderer's
	// status transition before publishing source row 1 so its latency does not
	// include resource/GPU readiness work and no initial FIFO burst is created.
	// This barrier is sender-agnostic: it does not inspect or predict 1.txt.
	if (m_ddsStim)
	{
		std::string startError;
		qInfo().noquote() << QStringLiteral(
			"[StimStartLifecycle] phase=wait_renderer_running begin=1 timeoutMs=5000 realtimePublished=0");
		if (!m_ddsStim->waitForRunningStatus(5000, startError))
		{
			qCritical().noquote() << QStringLiteral(
				"[StimStartLifecycle][ERROR] phase=renderer_running result=FAIL reason=%1 realtimePublished=0")
				.arg(QString::fromStdString(startError));
			m_statusLabel->setText(QStringLiteral("● 状态: START 未就绪 | 未发送实时行"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
			return;
		}
		qInfo().noquote() << QStringLiteral(
			"[StimStartLifecycle] phase=renderer_running result=PASS realtimePublished=0 action=begin_source_pacing");
	}
#endif

	if (!m_isRealtimeSending) {
		m_targetVideoFps = targetVideoFps();
		setSendStepMs(m_timeStep->text().toDouble());
		m_timeStep->setText(QString::number(time_step));
		m_isRealtimeSending = true;
		m_sentFrameCount = 0;
		m_sendDeadlineIndex = 0;
		m_pauseActiveLogged = false;
		m_pauseResumeLogged = false;
		m_sendClock.restart();
		m_lastSendPerfLogNs = 0;
		m_lastSendPerfFrameCount = 0;
		m_uiUpdateEveryFrames = qMax(1, static_cast<int>(m_inputHz / 5));
		m_startButton->setEnabled(false);
		m_stopButton->setEnabled(true);
		m_statusLabel->setText(QString(QStringLiteral("● 状态: START 已发；等待/发送首个有效实时位置 (%1 FPS)")).arg(m_inputHz));
		m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
		onSendRealTimeData();
	}
}

void MainWindow::onStopButtonClicked()
{
	if (m_isRealtimeSending || m_realTimeTimer->isActive()) {
		m_isRealtimeSending = false;
		m_realTimeTimer->stop();
		QString transportName = QStringLiteral("compat");
#if defined(HWASIMIR_HAS_ZRDDS)
		if (m_ddsStim)
			transportName = QStringLiteral("dds");
#endif
        qInfo().noquote()<<QString("[StimFinal] transport=%1 successfulRealtimeWrites=%2 elapsedMs=%3 targetHz=%4")
			.arg(transportName).arg(m_sentFrameCount).arg(m_sendClock.elapsed()).arg(m_inputHz);
		sendControlCommand(3); // 发送停止命令
		qInfo().noquote() << QStringLiteral("[StimStopLifecycle] phase=stop_command_sent result=PASS successfulRealtimeWrites=%1 elapsedMs=%2")
			.arg(m_sentFrameCount).arg(m_sendClock.elapsed());
		emit roundStopSent();
		m_startButton->setEnabled(true);
		m_stopButton->setEnabled(false);
		m_statusLabel->setText(QStringLiteral("● 状态: 仿真已停止"));
		m_statusLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
	}
}

// 单次步进，返回是否相撞，并输出当前位置/姿态
bool MainWindow::step(BYHWICD::CartesianCoordinate& plane_pos, BYHWICD::Euler& plane_att,
	BYHWICD::CartesianCoordinate& missile_pos, BYHWICD::Euler& missile_att) {
	// 如果已相撞，直接返回
	if (is_collided) {
		std::cout << "已相撞，停止模拟！" << std::endl;
		return true;
	}
	if (!m_freezeGeometryForTest && dataNum >= realTimeData.size())
	{
		is_collided = true;
		std::cout << "===== data is over; all accepted rows were sent =====" << std::endl;
		return true;
	}

//	// 计算飞机当前位置和姿态（姿态固定）
//	plane_pos.x = plane_init_pos.x;
//	plane_pos.y = plane_init_pos.y + plane_speed_y * current_time;
//	plane_pos.z = plane_init_pos.z;
//	plane_att = plane_init_attitude;//飛機姿態

//	// 计算导弹当前位置（xy平面抛物线，z固定）
//	// 抛物线方程：x(t) = x0 - k*t² （t=collision_time时x=飞机x）
//	missile_pos.x = missile_init_pos.x - parabola_k * pow(current_time, 2);
//	// y坐标和飞机同步（确保最终相撞）
//	missile_pos.y = plane_pos.y;
//	missile_pos.z = plane_init_pos.z;

//	// 计算导弹速度矢量（用于计算yaw）
//	double vx = -2.0 * parabola_k * current_time;  // x方向速度（dx/dt）
//	double vy = plane_speed_y;                  // y方向速度（dy/dt）
//	double vz = 0.0;                            // z方向速度（无运动）

//	// 计算导弹yaw角（绕z轴，顺时针为正，单位：度）
//	// 公式：yaw = atan2(vx, vy) * 180/π （atan2(vy, vx)是逆时针，这里反转参数）
//	missile_att.yaw = atan2(vx, vy) * 180.0 / M_PI;
//	// 俯仰/滚转固定（无变化）
//	missile_att.pitch = 0.0;
//	missile_att.roll = 0.0;

//	// 检查是否相撞（位置误差小于1米则判定相撞）
//	double dx = fabs(missile_pos.x - plane_pos.x);
//	double dy = fabs(missile_pos.y - plane_pos.y);
//	double dz = fabs(missile_pos.z - plane_pos.z);

	if (m_freezeGeometryForTest)
	{
		plane_pos = plane_init_pos;
		plane_att = plane_init_attitude;
		missile_pos = missile_init_pos;
		missile_att = missile_init_attitude;
	}
	else
	{
		plane_pos.x = realTimeData.at(dataNum).platPos.lat;
		plane_pos.y = realTimeData.at(dataNum).platPos.lon;
		plane_pos.z = realTimeData.at(dataNum).platPos.alt;
		plane_att.pitch = realTimeData.at(dataNum).platEul.pitch;
		plane_att.yaw = realTimeData.at(dataNum).platEul.yaw;
		plane_att.roll = realTimeData.at(dataNum).platEul.roll;

		missile_pos.x = realTimeData.at(dataNum).tarPos.lat;
		missile_pos.y = realTimeData.at(dataNum).tarPos.lon;
		missile_pos.z = realTimeData.at(dataNum).tarPos.alt;
		missile_att.pitch = realTimeData.at(dataNum).tarEul.pitch;
		missile_att.yaw = realTimeData.at(dataNum).tarEul.yaw;
		missile_att.roll = realTimeData.at(dataNum).tarEul.roll;

		// Do not stop when the last row is merely loaded: it still must be sent.
		dataNum++;
	}

	// Physical simulation time follows the source Time(ms), not the 60 Hz wall pacing.
	current_time = (realTimeData.at(dataNum - 1).sourceTimeMs -
		realTimeData.first().sourceTimeMs) / 1000.0;
	return is_collided;
}

void MainWindow::initStepSimData()
{
	is_collided = false;
	current_time = 0.0;
	dataNum = 1;
	if (!m_initSelectionFrozen)
	{
		m_targetType = m_targetTypeBox->currentData().toInt();
		m_forceVisibleForDemo = m_forceVisibleForDemoCheck->isChecked();
	}
	m_fovH = m_fovHEdit->text().toDouble();
	m_fovV = m_fovVEdit->text().toDouble();
	const realtimeInfo& firstSample = realTimeData.at(0);
	plane_init_pos.x = firstSample.platPos.lat;
	plane_init_pos.y = firstSample.platPos.lon;
	plane_init_pos.z = firstSample.platPos.alt;
	plane_init_attitude.yaw = firstSample.platEul.yaw;
	plane_init_attitude.pitch = firstSample.platEul.pitch;
	plane_init_attitude.roll = firstSample.platEul.roll;
	missile_init_pos.x = firstSample.tarPos.lat;
	missile_init_pos.y = firstSample.tarPos.lon;
	missile_init_pos.z = firstSample.tarPos.alt;
	missile_init_attitude.yaw = firstSample.tarEul.yaw;
	missile_init_attitude.pitch = firstSample.tarEul.pitch;
	missile_init_attitude.roll = firstSample.tarEul.roll;
	plane_speed_y = firstSample.platSpeed;

	// 第一帧必须直接使用输入文件第 0 行；否则零初始化位置会被接收端误当作地理参考点。
	m_currPlane_pos = plane_init_pos;
	m_currPlane_att = plane_init_attitude;
	m_currMissile_pos = missile_init_pos;
	m_currMissile_att = missile_init_attitude;

//	collision_time = m_collisionTime->text().toDouble();
	m_targetVideoFps = targetVideoFps();
	setSendStepMs(m_timeStep->text().toDouble());
	m_timeStep->setText(QString::number(time_step));

	// 抛物线参数：确保t=collision_time时导弹x坐标等于飞机x坐标
//    parabola_k = (missile_init_pos.x - plane_init_pos.x) / (collision_time * collision_time);
}

void MainWindow::readData(QString tmp)
{
	QFile file(tmp);
	if (!file.open(QIODevice::ReadOnly))
		qFatal("Cannot open replay input: %s", qPrintable(QFileInfo(tmp).absoluteFilePath()));
	const QByteArray bytes = file.readAll();
	const QVector<int> integerColumns = {17, 28, 29, 53, 54, 55, 62};
	const NumericCsv::Result parsed = NumericCsv::read(
		QString::fromUtf8(bytes), replayCsvSchema(), integerColumns);
	if (!parsed.ok())
	{
		for (const NumericCsv::Error& parseError : parsed.errors)
		{
			qCritical().noquote() << QStringLiteral(
				"[StimInputParse][ERROR] sourceLine=%1 column=%2 field=%3 reason=%4")
				.arg(parseError.sourceLine).arg(parseError.column)
				.arg(parseError.field.isEmpty() ? QStringLiteral("line") : parseError.field)
				.arg(parseError.reason);
		}
		qFatal("Replay input rejected; malformed files are never partially replayed: %s",
			qPrintable(QFileInfo(tmp).absoluteFilePath()));
	}

	realTimeData.clear();
	realTimeData.reserve(parsed.rows.size());
	for (const NumericCsv::Row& row : parsed.rows)
	{
		const QStringList& fields = row.fields;
		realtimeInfo data;
		data.sourceLine = row.sourceLine;
		data.sourceTimeMs = fields[0].toDouble();
		data.distance = fields[1].toDouble();
		data.platPos.lat = fields[2].toDouble();
		data.platPos.lon = fields[3].toDouble();
		data.platPos.alt = fields[4].toDouble();
		data.platEul.yaw = fields[5].toDouble();
		data.platEul.pitch = fields[6].toDouble();
		data.platEul.roll = fields[7].toDouble();
		data.platSpeed = fields[8].toDouble();
		data.tarPos.lat = fields[10].toDouble();
		data.tarPos.lon = fields[11].toDouble();
		data.tarPos.alt = fields[12].toDouble();
		data.tarEul.yaw = fields[13].toDouble();
		data.tarEul.pitch = fields[14].toDouble();
		data.tarEul.roll = fields[15].toDouble();
		data.tarSpeed = fields[16].toDouble();
		switch (fields[17].toInt())
		{
		case 0: data.targetType = 0x22; break;
		case 1: data.targetType = 0x33; break;
		default:
			qFatal("Unsupported MissileType at source line %d: %s",
				row.sourceLine, qPrintable(fields[17]));
		}
		data.strikeFlag = fields[28].toInt() != 0;
		data.damageFlag = fields[29].toInt() != 0; // retained for audit; not connected to rendering
		data.viewValid = fields[53].toInt() != 0;
		realTimeData.push_back(data);
	}
	for (int index = 1; index < realTimeData.size(); ++index)
	{
		if (realTimeData[index].sourceTimeMs <= realTimeData[index - 1].sourceTimeMs)
			qFatal("Non-increasing Time(ms) at source line %d", realTimeData[index].sourceLine);
	}
	qInfo().noquote() << QStringLiteral(
		"[StimInputParse] result=ACCEPTED path=%1 bytes=%2 headerLine=%3 blankLines=%4 rows=%5 columns=%6 sourceLineFirst=%7 sourceLineLast=%8 sourceTimeMs=%9..%10 malformedRows=0 partialReplay=0")
		.arg(QFileInfo(tmp).absoluteFilePath()).arg(bytes.size()).arg(parsed.headerLine)
		.arg(parsed.blankLines).arg(realTimeData.size()).arg(replayCsvSchema().size())
		.arg(realTimeData.first().sourceLine).arg(realTimeData.last().sourceLine)
		.arg(realTimeData.first().sourceTimeMs, 0, 'f', 3)
		.arg(realTimeData.last().sourceTimeMs, 0, 'f', 3);
}

#include "p7_sensor_form.inl"
