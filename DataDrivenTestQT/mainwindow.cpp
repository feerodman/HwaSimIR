// mainwindow.cpp
#include "mainwindow.h"
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

#include "ICD/math_algorithm.h"

//#define M_PI 3.1415926

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setupUI();
	setupUDP();


    //讀取文件内容
    QString tmp = "./1.txt";
    readData(tmp);

	QRegularExpression regExp(R"(0x[0-9A-Fa-f]+|[0-9A-Fa-f]+)");
	QRegularExpressionValidator *validator = new QRegularExpressionValidator(regExp, this);
	m_targetTypeEdit->setValidator(validator);
	// 初始化位置参数（构造时同步UI初始值）
	m_targetType= m_targetTypeEdit->text().toInt(nullptr,16);
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
	time_step = m_timeStep->text().toInt();


	m_realTimeTimer = new QTimer(this);
	connect(m_realTimeTimer, &QTimer::timeout, this, &MainWindow::onSendRealTimeData);

	// 状态初始化
	m_statusLabel->setText(QStringLiteral("状态: 就绪"));

}

MainWindow::~MainWindow()
{
	if (m_udpSocket) {
		m_udpSocket->close();
		delete m_udpSocket;
	}
}

// ==================== 核心修正：补充缺失的槽函数 ====================
void MainWindow::onSendRealTimeData()
{
	sendRealTimeData(); // 直接调用发送逻辑
}
// ===============================================================

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
	m_configGroup = new QGroupBox(QStringLiteral("UDP通信配置"));
	QFormLayout *configLayout = new QFormLayout;
    //configLayout->addRow(QStringLiteral("本地IP:"), m_localIpEdit = new QLineEdit("192.168.1.188"));
    configLayout->addRow(QStringLiteral("本地IP:"), m_localIpEdit = new QLineEdit("127.0.0.1"));
	configLayout->addRow(QStringLiteral("本地端口:"), m_localPortEdit = new QLineEdit("9999"));
    //configLayout->addRow(QStringLiteral("目标IP:"), m_remoteIpEdit = new QLineEdit("192.168.1.121"));
    configLayout->addRow(QStringLiteral("目标IP:"), m_remoteIpEdit = new QLineEdit("127.0.0.1"));
	configLayout->addRow(QStringLiteral("目标端口:"), m_remotePortEdit = new QLineEdit("8888"));
	m_configGroup->setLayout(configLayout);

	// 控制组
	m_controlGroup = new QGroupBox(QStringLiteral("仿真控制"));
	QVBoxLayout *controlLayout = new QVBoxLayout;
	m_resetButton = new QPushButton(QStringLiteral("□ 复位 (1)"));
	m_initButton = new QPushButton(QStringLiteral("○ 初始化 (0x36)"));
	m_startButton = new QPushButton(QStringLiteral("▲▼ 开始仿真 (2)"));
	m_stopButton = new QPushButton(QStringLiteral("■ 停止仿真 (3)"));

	// 按钮样式
	m_startButton->setStyleSheet("background-color: #4CAF50; color: white;");
	m_stopButton->setStyleSheet("background-color: #f44336; color: white;");

	controlLayout->addWidget(m_resetButton);
	controlLayout->addWidget(m_initButton);
	controlLayout->addWidget(m_startButton);
	controlLayout->addWidget(m_stopButton);
	controlLayout->addStretch();
	m_controlGroup->setLayout(controlLayout);

	// 实时数据组
	m_realTimeDataGroup = new QGroupBox(QStringLiteral("实时成像数据配置 (平台姿态每次+0.01)"));
	QFormLayout *realTimeLayout = new QFormLayout;
	realTimeLayout->addRow(QStringLiteral("目标类型:"), m_targetTypeEdit = new QLineEdit("0x11"));
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
    realTimeLayout->addRow(QStringLiteral("发送步长(ms):"), m_timeStep = new QLineEdit("25"));
	

	m_realTimeDataGroup->setLayout(realTimeLayout);

	// 状态组
	m_statusGroup = new QGroupBox(QStringLiteral("运行状态"));
	QVBoxLayout *statusLayout = new QVBoxLayout;
	m_statusLabel = new QLabel(QStringLiteral("● 状态: 就绪 | 未开始发送"));
	m_statusLabel->setStyleSheet("color: #1976D2; font-weight: bold;");
	m_lastSentLabel = new QLabel(QStringLiteral("↑ 最后发送: 无"));
	m_lastReceivedLabel = new QLabel(QStringLiteral("↓ 最后接收: 无"));
	statusLayout->addWidget(m_statusLabel);
	statusLayout->addWidget(m_lastSentLabel);
	statusLayout->addWidget(m_lastReceivedLabel);
	m_statusGroup->setLayout(statusLayout);

	// 主布局
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addWidget(m_configGroup);
	mainLayout->addWidget(m_controlGroup);
	mainLayout->addWidget(m_realTimeDataGroup);
	mainLayout->addWidget(m_statusGroup);
	mainLayout->addStretch();

	QWidget *centralWidget = new QWidget;
	centralWidget->setLayout(mainLayout);
	setCentralWidget(centralWidget);

	// 信号连接
	connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::onResetButtonClicked);
	connect(m_initButton, &QPushButton::clicked, this, &MainWindow::onInitButtonClicked);
	connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
	connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
}

void MainWindow::setupUDP()
{
	m_udpSocket = new QUdpSocket(this);

	// 绑定本地端口（可选，用于接收应答）
	bool bound = m_udpSocket->bind(QHostAddress(m_localIpEdit->text()), m_localPortEdit->text().toUShort());
	if (!bound) {
		QMessageBox::warning(this, QStringLiteral("UDP绑定失败"),
			QString(QStringLiteral("无法绑定到 %1:%2")).arg(m_localIpEdit->text()).arg(m_localPortEdit->text()));
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
					m_lastReceivedLabel->setText(QString(QStringLiteral("↓ 接收: 初始化应答 (0x37) 来自 %1:%2"))
						.arg(sender.toString()).arg(senderPort));
					m_statusLabel->setText(QStringLiteral("● 状态: 初始化完成 | 等待开始指令"));
					m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
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
	cmd.platID = 1;
	cmd.simCommand = command;
	//cmd.roundCut = m_roundCutEdit->text().toInt();
	//cmd.currentRound = m_currentRoundEdit->text().toInt();
	cmd.roundCut = 1;
	cmd.currentRound = 1;
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
	else
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！ | udpSocket错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	

	
}

void MainWindow::sendInitCommand()
{
	BYHWICD::InitP2cObjectTrackingCmd cmd = {};
	cmd.flag = 0x36;
	cmd.JB = 1;
	cmd.platID = 1;
	cmd.sensorID = 1;
	cmd.platNumValid = 1;

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
    cmd.platParam[0].id = 1;
    cmd.platParam[0].type = 0x11;
    cmd.platParam[0].spatial.lat = realTimeData.at(0).platPos.lat;
    cmd.platParam[0].spatial.lon = realTimeData.at(0).platPos.lon;
    cmd.platParam[0].spatial.alt = realTimeData.at(0).platPos.alt;
    cmd.platParam[0].spatial.yaw = realTimeData.at(0).platEul.yaw;
    cmd.platParam[0].spatial.pitch = realTimeData.at(0).platEul.pitch;
    cmd.platParam[0].spatial.roll = realTimeData.at(0).platEul.roll;
    cmd.platParam[0].spatial.speed = realTimeData.at(0).platSpeed;

    // 传感器参数（简化配置）
    cmd.trackingInit.enable = true;
    cmd.trackingInit.envTerrain = 0; // 戈壁
    cmd.trackingInit.envSky = 0;    // 晴
    cmd.trackingInit.envTemp = 25.0;
    cmd.trackingInit.videoFps = 30;
    cmd.trackingInit.trackerSensor[0].index = 0;
    cmd.trackingInit.trackerSensor[0].trackerSensorBand = 2; // 中波红外
    cmd.trackingInit.trackerSensor[0].trackerSensorWidth = 600;
    cmd.trackingInit.trackerSensor[0].trackerSensorHeight = 600;//hml
    cmd.trackingInit.trackerSensor[0].trackerSensorViewMin = 1;
    cmd.trackingInit.trackerSensor[0].trackerSensorViewMax = 550000;
    cmd.trackingInit.trackerSensor[0].trackerSensorPixelAngle = 2.18166;
    cmd.trackingInit.trackerSensor[0].coarseTrackEn = true;
    cmd.trackingInit.trackerSensor[0].preciseTrackEn = true;
    cmd.trackingInit.trackerSensor[0].coarseTrackResolution = m_fovHEdit->text().toDouble();
    cmd.trackingInit.trackerSensor[0].preciseTrackResolution = m_fovVEdit->text().toDouble();


    cmd.MissileMaxCount120 = 5;
    cmd.MissileMaxCount9 = 5;
    cmd.MissileMaxCountMMD = 0;

	if (m_udpSocket)
	{
		QHostAddress remoteIp(m_remoteIpEdit->text());
		quint16 remotePort = m_remotePortEdit->text().toUShort();
		qint64 sent = m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&cmd), sizeof(cmd), remoteIp, remotePort);

		m_lastSentLabel->setText(QString(QStringLiteral("↑ 发送: 初始化命令 (0x36) | %1 bytes")).arg(sent));
		m_statusLabel->setText(QStringLiteral("● 状态: 已发送初始化 | 等待边缘端应答"));
		m_statusLabel->setStyleSheet("color: #FF9800; font-weight: bold;");

		if (sent < 0) {
			m_statusLabel->setText(QStringLiteral("● 状态: 初始化发送失败！"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		}

		qDebug() << "Sent Init Command, Bytes:" << sent;
	}
	else
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 初始化发送失败！ | udpSocket错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	


	initStepSimData();
}

void MainWindow::sendRealTimeData()
{
	BYHWICD::DisplayC2cObjTrackingData data = {};
	data.flag = 0x38;
	data.platID = 1;
	data.sensorID = 1;
	data.time = QDateTime::currentMSecsSinceEpoch();


	// 使用当前累积位置（关键：发送前使用当前值）
	data.platLoc.lat = m_currPlane_pos.x;
	data.platLoc.lon = m_currPlane_pos.y;
	data.platLoc.alt = m_currPlane_pos.z;
	data.platLoc.yaw = m_currPlane_att.yaw;
	data.platLoc.pitch = m_currPlane_att.pitch;
	data.platLoc.roll = m_currPlane_att.roll;
    data.platLoc.speed = realTimeData.at(dataNum-1).platSpeed;

	// Wg信息
    data.weaponState.targetType = 0x22;
	data.weaponState.targetPlatID = 3;
    data.weaponState.targetID = 3;
	data.weaponState.xxOutAng[0] = 0.0;
	data.weaponState.xxOutAng[1] = 0.0;
	data.weaponState.lookatEn = true;
	data.weaponState.illuminatorEn = true;
    if(current_time > 5){
        //5秒后發動機熄火
        data.weaponState.strikeFlag = true;
        data.weaponState.strikePart=2;
    }else{
        data.weaponState.strikeFlag = false;
    }

    data.weaponState.viewValid = realTimeData.at(dataNum-1).viewValid;


	// 目标状态（相对平台偏移）
    data.targetNumValid = 3;
    data.targetState[0].targetType = 0x22;
	data.targetState[0].targetPlatID = 3;
	data.targetState[0].targetID = 3;
    if(current_time > 5){
        //5秒后發動機熄火
        data.targetState[0].engineState = true;
    }else{
        data.targetState[0].engineState = false;
    }

    data.targetState[0].viewValid = realTimeData.at(dataNum-1).viewValid;

	data.targetState[0].targetLoc.lat = m_currMissile_pos.x;
	data.targetState[0].targetLoc.lon = m_currMissile_pos.y;
	data.targetState[0].targetLoc.alt = m_currMissile_pos.z;
	data.targetState[0].targetLoc.yaw = m_currMissile_att.yaw;
	data.targetState[0].targetLoc.pitch = m_currMissile_att.pitch;
	data.targetState[0].targetLoc.roll = m_currMissile_att.roll;
	data.targetState[0].targetState = 0x01;


    data.targetState[1].targetType = 0x22;
    data.targetState[1].targetPlatID = 3;
    data.targetState[1].targetID = 34;
    data.targetState[1].viewValid = realTimeData.at(dataNum-1).viewValid;
    data.targetState[1].targetLoc.lat = 0.0;
    data.targetState[1].targetLoc.lon = 0.0;
    data.targetState[1].targetLoc.alt = 0.0;
    data.targetState[1].targetLoc.yaw = 0.0;
    data.targetState[1].targetLoc.pitch = 0.0;
    data.targetState[1].targetLoc.roll = 0.0;
    data.targetState[1].targetState = 0x01;


    data.targetState[2].targetType = 0x33;
    data.targetState[2].targetPlatID = 3;
    data.targetState[2].targetID = 4;
    data.targetState[2].viewValid = realTimeData.at(dataNum-1).viewValid;
    data.targetState[2].targetLoc.lat = 0.0;
    data.targetState[2].targetLoc.lon = 0.0;
    data.targetState[2].targetLoc.alt = 0.0;
    data.targetState[2].targetLoc.yaw = 0.0;
    data.targetState[2].targetLoc.pitch = 0.0;
    data.targetState[2].targetLoc.roll = 0.0;
    data.targetState[2].targetState = 0x01;


    if(current_time > 5)
    {
        data.targetState[1].targetLoc.lat = m_currMissile_pos.x;
        data.targetState[1].targetLoc.lon = m_currMissile_pos.y;
        data.targetState[1].targetLoc.alt = m_currMissile_pos.z + 1.0;
        data.targetState[1].targetLoc.yaw = m_currMissile_att.yaw;
        data.targetState[1].targetLoc.pitch = m_currMissile_att.pitch;
        data.targetState[1].targetLoc.roll = m_currMissile_att.roll;
    }

    if(current_time > 8)
    {
        data.targetState[2].targetLoc.lat = m_currMissile_pos.x;
        data.targetState[2].targetLoc.lon = m_currMissile_pos.y+0.00002;
        data.targetState[2].targetLoc.alt = m_currMissile_pos.z - 1.0;
        data.targetState[2].targetLoc.yaw = m_currMissile_att.yaw;
        data.targetState[2].targetLoc.pitch = m_currMissile_att.pitch;
        data.targetState[2].targetLoc.roll = m_currMissile_att.roll;
    }
    if(current_time > 12)
    {
         data.weaponState.targetID = 34;
    }

	if (m_udpSocket)
	{
		// 发送
		QHostAddress remoteIp(m_remoteIpEdit->text());
		quint16 remotePort = m_remotePortEdit->text().toUShort();
		qint64 sent = m_udpSocket->writeDatagram(reinterpret_cast<const char*>(&data), sizeof(data), remoteIp, remotePort);

		// 状态更新
		m_lastSentLabel->setText(QString(QStringLiteral("↑ 发送: 实时数据 (0x38) | Lat:%1° | %2 bytes"))
			.arg(m_currentLat, 0, 'f', 4).arg(sent));

		if (sent > 0) {
			m_statusLabel->setText(QString(QStringLiteral("● 状态: 仿真中 | 已发送 %1 帧")).arg(m_realTimeTimer->interval() ? 1 : 0));
			m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
		}
		else {
			m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！检查网络"));
			m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
		}
	}
	else
	{
		m_statusLabel->setText(QStringLiteral("● 状态: 发送失败！ | udpSocket错误"));
		m_statusLabel->setStyleSheet("color: #D32F2F; font-weight: bold;");
	}
	

	// 关键：发送后立即更新位置（为下一次发送准备）
	updatePosition();
}

void MainWindow::updatePosition()
{
	
	if (step(m_currPlane_pos, m_currPlane_att, m_currMissile_pos, m_currMissile_att))
	{
		onStopButtonClicked();
	}

	// 实时更新UI显示（用户可见变化）
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

void MainWindow::onResetButtonClicked()
{
	sendControlCommand(1);
	//initStepSimData();
	

    dataNum = 1;

	m_targetTypeEdit->setText("0x" + QString::number(m_targetType, 16));
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

						   // 启动实时数据发送（100ms/帧）
	if (!m_realTimeTimer->isActive()) {
		m_realTimeTimer->start(time_step);
		m_startButton->setEnabled(false);
		m_stopButton->setEnabled(true);
		m_statusLabel->setText(QStringLiteral("● 状态: 仿真运行中 (10帧/秒)"));
		m_statusLabel->setStyleSheet("color: #388E3C; font-weight: bold;");
	}
}

void MainWindow::onStopButtonClicked()
{
	if (m_realTimeTimer->isActive()) {
		m_realTimeTimer->stop();
		sendControlCommand(3); // 发送停止命令
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

    if (dataNum >= (realTimeData.size()-1)) {
		is_collided = true;
        std::cout << "===== data is over =====" << std::endl;
	}

	// 输出当前数据
	std::cout << "------------------------" << std::endl;
	std::cout << "模拟时间：" << current_time << " 秒" << std::endl;
	std::cout << "【飞机】" << std::endl;
	std::cout << "位置：x=" << std::fixed << std::setprecision(2) << plane_pos.x
		<< " y=" << plane_pos.y << " z=" << plane_pos.z << " 米" << std::endl;
	std::cout << "姿态：yaw=" << plane_att.yaw << " pitch=" << plane_att.pitch
		<< " roll=" << plane_att.roll << " 度" << std::endl;
	std::cout << "【导弹】" << std::endl;
	std::cout << "位置：x=" << missile_pos.x << " y=" << missile_pos.y
		<< " z=" << missile_pos.z << " 米" << std::endl;
	std::cout << "姿态：yaw=" << missile_att.yaw << " pitch=" << missile_att.pitch
		<< " roll=" << missile_att.roll << " 度" << std::endl;

	// 更新时间步
	current_time += double(time_step)/1000;
    dataNum++;

	return is_collided;
}

void MainWindow::initStepSimData()
{
	is_collided = false;
	current_time = 0.0;
	m_targetType = m_targetTypeEdit->text().toInt(nullptr, 16);
	m_fovH = m_fovHEdit->text().toDouble();
	m_fovV = m_fovVEdit->text().toDouble();
    plane_init_pos.x = m_latEdit->text().toDouble();
    plane_init_pos.y = m_lonEdit->text().toDouble();
    plane_init_pos.z = m_altEdit->text().toDouble();
    plane_init_attitude.yaw = m_yawEdit->text().toDouble();
    plane_init_attitude.pitch = m_pitchEdit->text().toDouble();
    plane_init_attitude.roll = m_rollEdit->text().toDouble();
    missile_init_pos.x = m_latEditTarget->text().toDouble();
    missile_init_pos.y = m_lonEditTarget->text().toDouble();
    missile_init_pos.z = m_altEditTarget->text().toDouble();
    missile_init_attitude.yaw = m_yawEditTarget->text().toDouble();
    missile_init_attitude.pitch = m_pitchEditTarget->text().toDouble();
    missile_init_attitude.roll = m_rollEditTarget->text().toDouble();
    plane_speed_y = m_speed->text().toDouble();

//	collision_time = m_collisionTime->text().toDouble();
	time_step = m_timeStep->text().toInt();

	// 抛物线参数：确保t=collision_time时导弹x坐标等于飞机x坐标
//    parabola_k = (missile_init_pos.x - plane_init_pos.x) / (collision_time * collision_time);
}

void MainWindow::readData(QString tmp)
{
    QFile file(tmp);
    QByteArray fileValueTmp;

    realtimeInfo data;
//    QVector<realtimeInfo> coeOpaPgVector;
//    qDebug() << "当前工作目录:" << QDir::currentPath();
    if(file.open(QIODevice::ReadOnly))
    {
        //读取所有数据
        fileValueTmp=file.readAll();
        //将QByteArray转换为QString
        QString QStringTmp(fileValueTmp);
        //将QString的内容用\r\n进行切分，存入QStringList
//        QStringList list = QStringTmp.split("\r\n");
        QStringList list = QStringTmp.split("\n");

        //迭代器代替for循环，不用计算循环的次数
        //QList的迭代器，定义如下，对list进行迭代
        QListIterator<QString> i(list);

        //将QStringList的内容放至coeRis
       //将\r\n获得的数据进行拆分，存入QStringList
        QStringList list1;
        int index=0;

        while (i.hasNext())
        {
            list1 = i.next().split(",");
            if(list1.length()==63 && index != 0)
            {
                data.distance = list1[1].toDouble();
                //redplat
                data.platPos.lat = list1[2].toDouble();
                data.platPos.lon = list1[3].toDouble();
                data.platPos.alt = list1[4].toDouble();
                data.platEul.yaw = list1[5].toDouble();
                data.platEul.pitch = list1[6].toDouble();
                data.platEul.roll = list1[7].toDouble();
                data.platSpeed = list1[8].toDouble();

                //mission
                data.tarPos.lat = list1[10].toDouble();
                data.tarPos.lon = list1[11].toDouble();
                data.tarPos.alt = list1[12].toDouble();
                data.tarEul.yaw = list1[13].toDouble();
                data.tarEul.pitch = list1[14].toDouble();
                data.tarEul.roll = list1[15].toDouble();
                data.tarSpeed = list1[16].toDouble();
//                data.target2plat.azimuth = list1[30].toDouble();
//                data.target2plat.pitch = list1[31].toDouble();
                switch (list1[17].toInt()) {
                case 0:{
                    data.targetType = 0x22;
                    break;
                }
                case 1:{
                    data.targetType = 0x33;
                    break;
                }
                default:{
                    qDebug()<<"unknown target type!";
                    break;
                }
                }
                //data.viewValid = list1[53].toInt();
                data.viewValid = 1;
                data.damageFlag = list1[29].toInt();
                data.strikeFlag = list1[28].toInt();

                realTimeData.push_back(data);
            }
            else
            {
                qDebug()<<"OpaPg data error,the line number is"<<index;
            }
            index++;
        }
    }
    else
    {
        qDebug()<<"open OpaPg file error";
        return;
    }
    qDebug()<<"the data is ready!";
}
