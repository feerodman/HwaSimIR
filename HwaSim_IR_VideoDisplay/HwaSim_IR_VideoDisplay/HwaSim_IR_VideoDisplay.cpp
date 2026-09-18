#include "VideoPixelFit.h"
#include "HwaSim_IR_VideoDisplay.h"
#include <QScreen>
#include <QWindow>
#include <QGridLayout>
#include "../../DDS/Protocol/SensorFieldSchema.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QHeaderView>
#include <QResizeEvent>
#include <QSplitter>
#include <QTabWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSettings>
#include <QtGlobal>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace
{
qint64 wallTimeNs()
{
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

qint64 steadyTimeNs()
{
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

HwaSim_IR_VideoDisplay::HwaSim_IR_VideoDisplay(
    const QString& networkConfigPath,
    const QString& channel,
    int platID,
	int sensorID,
	const QString& receiveTransport,
	const QString& streamRole,
	const QString& ddsTopic,
	const QString& ddsCodec,
	const QString& ddsQos,
	int ddsDomain,
	int ddsWidth,
	int ddsHeight,
	int ddsFps,
	const QString& ddsDumpFirstFrame,
	int ddsDumpFrameIndex,
    QWidget *parent)
    : QWidget(parent),
      m_networkConfigPath(networkConfigPath.trimmed()),
      m_channel(channel.trimmed().toLower()),
      m_platID(platID),
      m_sensorID(sensorID),
      m_pid(QCoreApplication::applicationPid())
{
    if (m_networkConfigPath.isEmpty())
    {
        m_networkConfigPath = QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("NetworkConfig.ini"));
    }
    else
    {
        m_networkConfigPath = QFileInfo(m_networkConfigPath).absoluteFilePath();
    }
    QSettings instanceSettings(m_networkConfigPath, QSettings::IniFormat);
	m_receiveTransport = receiveTransport.trimmed().toLower();
	if (m_receiveTransport.isEmpty())
		m_receiveTransport = instanceSettings.value(QStringLiteral("VideoInput/Transport"),
			QStringLiteral("dds")).toString().trimmed().toLower();
	if (m_receiveTransport != QStringLiteral("tcp") && m_receiveTransport != QStringLiteral("dds"))
	{
		qFatal("[VideoInput][FATAL] invalid Transport=%s; expected dds|tcp",
			qPrintable(m_receiveTransport));
	}
	QString resolvedStreamRole = streamRole.trimmed().toLower();
	if (resolvedStreamRole.isEmpty())
		resolvedStreamRole = instanceSettings.value(QStringLiteral("DdsVideo/StreamRole"),
			QStringLiteral("direct")).toString().trimmed().toLower();
	if (resolvedStreamRole != QStringLiteral("direct") &&
		resolvedStreamRole != QStringLiteral("decoded"))
	{
		qCritical().noquote() << QStringLiteral(
			"[VideoInput][FATAL] invalid StreamRole=%1").arg(resolvedStreamRole);
		resolvedStreamRole = QStringLiteral("direct");
	}
    if (m_channel.isEmpty())
    {
        m_channel = instanceSettings.value(
            QStringLiteral("Identity/channel"), QStringLiteral("unknown"))
            .toString().trimmed().toLower();
    }
    if (m_platID < 0)
    {
        m_platID = instanceSettings.value(QStringLiteral("Identity/platID"), 0).toInt();
    }
    if (m_sensorID < 0)
    {
        m_sensorID = instanceSettings.value(QStringLiteral("Identity/sensorID"), 0).toInt();
    }
    const QString tcpIp = instanceSettings.value(
        QStringLiteral("Network/ip"), QStringLiteral("0.0.0.0")).toString();
    const int tcpPort = instanceSettings.value(QStringLiteral("Network/port"), 5555).toInt();
    qInfo().noquote()
        << QStringLiteral("[RuntimeInstance] component=HwaSim_IR_VideoDisplay channel=%1 platID=%2 sensorID=%3 pid=%4 configPath=%5 tcpListen=%6:%7 configSource=%8")
            .arg(m_channel)
            .arg(m_platID)
            .arg(m_sensorID)
            .arg(m_pid)
            .arg(m_networkConfigPath)
            .arg(tcpIp)
            .arg(tcpPort)
            .arg(QFileInfo::exists(m_networkConfigPath) ? QStringLiteral("ini") : QStringLiteral("defaults"));

    ui.setupUi(this);
    setupResponsiveLayout();
    auto* fpsLayout=new QHBoxLayout(ui.Wgt_title);
    fpsLayout->setContentsMargins(8,2,8,2);
    m_liveFpsLabel=new QLabel(ui.Wgt_title);
    m_liveFpsLabel->setObjectName("liveReceivedFps");
    m_liveFpsLabel->setStyleSheet("color:#c7d7e7;font-size:12px;");
    m_liveFpsLabel->setWordWrap(true);
    m_liveFpsLabel->setMinimumWidth(0);
    fpsLayout->addWidget(m_liveFpsLabel,1);
    m_liveFpsClock.start();
    auto* fpsTimer=new QTimer(this);fpsTimer->setInterval(200);
    connect(fpsTimer,&QTimer::timeout,this,&HwaSim_IR_VideoDisplay::updateLiveFps);
    fpsTimer->start();updateLiveFps();
    setWindowTitle(QString::fromUtf8("红外仿真图像接收器"));
    showMaximized();

    // m_Label_Video 居中 + 自适应缩放
    ui.m_Label_Video->setScaledContents(false);
    ui.m_Label_Video->setAlignment(Qt::AlignCenter);
	ui.m_Label_Video->installEventFilter(this);

    // 设置 dockWidget
    ui.dockWidget_dataShow->setWindowTitle(QString::fromUtf8("数据显示"));

    // 动态创建表格
    InitTables();

	InitQss();

    // Network worker always runs outside the GUI thread. TCP and DDS share dataReceived.
    m_workerThread = new QThread(this);
	if (m_receiveTransport == QStringLiteral("dds"))
	{
		DdsVideoReceiverConfig config;
		config.domainId = instanceSettings.value(QStringLiteral("DdsVideo/DomainId"), 150).toInt();
		config.qosFile = instanceSettings.value(QStringLiteral("DdsVideo/QosFile"),
			QStringLiteral("Config/DDS/ZRDDS_PROTOCOL_QOS.xml")).toString();
		config.topic = instanceSettings.value(QStringLiteral("DdsVideo/Topic"),
			QStringLiteral("HwaSimIR.Video.precise.H264")).toString();
		config.codec = instanceSettings.value(QStringLiteral("DdsVideo/Codec"), QStringLiteral("h264")).toString();
		config.width = instanceSettings.value(QStringLiteral("DdsVideo/Width"), 800).toInt();
		config.height = instanceSettings.value(QStringLiteral("DdsVideo/Height"), 800).toInt();
		config.fps = instanceSettings.value(QStringLiteral("DdsVideo/Fps"), 60).toInt();
		if (!ddsTopic.trimmed().isEmpty()) config.topic = ddsTopic.trimmed();
		if (!ddsCodec.trimmed().isEmpty()) config.codec = ddsCodec.trimmed().toLower();
		if (!ddsQos.trimmed().isEmpty()) config.qosFile = ddsQos.trimmed();
		if (ddsDomain >= 0) config.domainId = ddsDomain;
		if (ddsWidth > 0) config.width = ddsWidth;
		if (ddsHeight > 0) config.height = ddsHeight;
		if (ddsFps > 0) config.fps = ddsFps;
		config.dumpFirstFramePath = ddsDumpFirstFrame.trimmed();
		config.dumpFrameIndex = qMax(1, ddsDumpFrameIndex);
		config.autoFromVideoStatus = ddsTopic.trimmed().isEmpty() && ddsCodec.trimmed().isEmpty() &&
			ddsWidth <= 0 && ddsHeight <= 0 && ddsFps <= 0;
		config.topicControl = instanceSettings.value(QStringLiteral("DdsProtocol/TopicControl"),
			QStringLiteral("HwaSimIR.Control")).toString();
		config.topicInit = instanceSettings.value(QStringLiteral("DdsProtocol/TopicInit"),
			QStringLiteral("HwaSimIR.Init")).toString();
		config.topicRealtime = instanceSettings.value(QStringLiteral("DdsProtocol/TopicRealtime"),
			QStringLiteral("HwaSimIR.Realtime")).toString();
		config.topicInitAck = instanceSettings.value(QStringLiteral("DdsProtocol/TopicInitAck"),
			QStringLiteral("HwaSimIR.InitAck")).toString();
		const QString defaultStatusTopic = resolvedStreamRole == QStringLiteral("decoded")
			? QStringLiteral("HwaSimIR.DecodedVideoStatus")
			: QStringLiteral("HwaSimIR.VideoStatus");
		config.topicVideoStatus = resolvedStreamRole == QStringLiteral("decoded")
			? instanceSettings.value(QStringLiteral("DdsGateway/DecodedStatusTopic"),
				defaultStatusTopic).toString()
			: instanceSettings.value(QStringLiteral("DdsVideo/StatusTopic"),
				defaultStatusTopic).toString();
		config.receiveFrameProducts = resolvedStreamRole == QStringLiteral("direct");
		config.channel = instanceSettings.value(QStringLiteral("DdsVideo/Channel"),
			config.topic.contains(QStringLiteral(".coarse."), Qt::CaseInsensitive)
				? QStringLiteral("coarse") : QStringLiteral("precise")).toString().trimmed().toLower();
		config.platID = m_platID;
		config.sensorID = m_sensorID;
		config.topicVideoMeta = instanceSettings.value(QStringLiteral("DdsProtocol/TopicVideoMeta"),
			QStringLiteral("HwaSimIR.VideoMeta.%1").arg(config.channel)).toString();
		config.topicAnnotation = instanceSettings.value(QStringLiteral("DdsProtocol/TopicAnnotation"),
			QStringLiteral("HwaSimIR.Annotation.%1").arg(config.channel)).toString();
		if (QFileInfo(config.qosFile).isRelative())
			config.qosFile = QDir(QCoreApplication::applicationDirPath()).filePath(config.qosFile);
		m_ddsWorker = new DdsVideoReceiverWorker(config);
		m_ddsWorker->moveToThread(m_workerThread);
		connect(m_ddsWorker, &DdsVideoReceiverWorker::dataReceived,
			this, &HwaSim_IR_VideoDisplay::imageReceivedSlot);
		connect(m_ddsWorker, &DdsVideoReceiverWorker::initCommandReceived,
			this, &HwaSim_IR_VideoDisplay::initCommandReceivedSlot);
		connect(m_ddsWorker, &DdsVideoReceiverWorker::controlCmdReceived,
			this, &HwaSim_IR_VideoDisplay::controlCmdReceivedSlot);
		connect(m_ddsWorker, &DdsVideoReceiverWorker::videoStatusChanged,
			this, &HwaSim_IR_VideoDisplay::videoStatusReceivedSlot);
		connect(m_ddsWorker, &DdsVideoReceiverWorker::fatalError, this,
			[this](const QString& reason)
			{
				m_receiverFatalError = reason;
				updateLiveFps();
			});
		connect(m_workerThread, &QThread::finished, m_ddsWorker, &QObject::deleteLater);
		connect(m_workerThread, &QThread::started, m_ddsWorker, &DdsVideoReceiverWorker::doWork);
		ui.dockWidget_dataShow->setWindowTitle(QString::fromUtf8("数据显示 · DDS 全链路"));
		qInfo().noquote() << QStringLiteral(
			"[VideoInput] Transport=dds streamRole=%1 statusTopic=%2 topic=%3 codec=%4 domain=%5")
			.arg(resolvedStreamRole).arg(config.topicVideoStatus).arg(config.topic)
			.arg(config.codec).arg(config.domainId);
	}
	else
	{
		m_worker = new TcpServerWorker(m_networkConfigPath, m_channel, m_platID, m_sensorID);
		m_worker->moveToThread(m_workerThread);
		connect(m_worker, &TcpServerWorker::dataReceived, this, &HwaSim_IR_VideoDisplay::imageReceivedSlot);
		connect(m_worker, &TcpServerWorker::initCommandReceived, this, &HwaSim_IR_VideoDisplay::initCommandReceivedSlot);
		connect(m_worker, &TcpServerWorker::controlCmdReceived, this, &HwaSim_IR_VideoDisplay::controlCmdReceivedSlot);
		connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
		connect(m_workerThread, &QThread::started, m_worker, &TcpServerWorker::doWork);
		qInfo().noquote() << QStringLiteral("[VideoInput] Transport=tcp");
	}
    m_workerThread->start();

    QSettings recorderSettings(m_networkConfigPath, QSettings::IniFormat);
    m_maxRecordingQueueFrames = qBound(
        1,
        recorderSettings.value(QStringLiteral("Recorder/MaxRecordingQueueFrames"), 180).toInt(),
        3600);
    m_recorderFlushTimeoutMs = qBound(
        1000,
        recorderSettings.value(QStringLiteral("Recorder/FlushTimeoutMs"), 10000).toInt(),
        60000);
    m_recorder = new AsyncVideoRecorder(m_maxRecordingQueueFrames);
    const QString recordingDirectory=qEnvironmentVariableIsSet("P7RecordingRoot")?QString::fromLocal8Bit(qgetenv("P7RecordingRoot")):
        recorderSettings.value("Recorder/Directory",QApplication::applicationDirPath()+"/MP4").toString();
    m_recorder->setOutputDirectory(recordingDirectory);
    qInfo().noquote()
        << QStringLiteral("[RecorderConfig] channel=%1 platID=%2 sensorID=%3 pid=%4 MaxRecordingQueueFrames=%5 FlushTimeoutMs=%6")
            .arg(m_channel)
            .arg(m_platID)
            .arg(m_sensorID)
            .arg(m_pid)
            .arg(m_maxRecordingQueueFrames)
            .arg(m_recorderFlushTimeoutMs);
}

HwaSim_IR_VideoDisplay::~HwaSim_IR_VideoDisplay()
{
	logGuiPaintPerf("shutdown");
    // 先停止工作线程，确保不再有信号投递到主线程
	if (m_worker) m_worker->stop();
	if (m_ddsWorker) m_ddsWorker->stop();
    m_workerThread->quit();
    m_workerThread->wait();
    CloseStorage();
    if (m_recorder)
    {
        m_recorder->shutdown(m_recorderFlushTimeoutMs);
        delete m_recorder;
        m_recorder = nullptr;
    }
}

void HwaSim_IR_VideoDisplay::InitQss()
{
    QString strQssPath = QApplication::applicationDirPath();
    strQssPath += "/qss/style.css";

    QFile file(strQssPath);
    if (!file.open(QFile::ReadOnly)) {
        qWarning() << "无法加载QSS样式文件:" << strQssPath;
        return;
    }
    // QSS 可能包含中文字体名，按 UTF-8 读取可避免样式文本乱码。
    QString styleSheet = QString::fromUtf8(file.readAll());
    qApp->setStyleSheet(styleSheet);
    file.close();
}

#include "P7ReceiverLayout.inl"

void HwaSim_IR_VideoDisplay::InitTables()
{
    // ============ 平台数据表格（ui 已创建，直接配置）============
    ui.tableWidget_platData->setColumnCount(9);
    ui.tableWidget_platData->setRowCount(2);
    ui.tableWidget_platData->setHorizontalHeaderLabels({
        "平台ID", "阵营",
        "纬度(°)", "经度(°)", "海拔(m)",
        "航向(°)", "俯仰(°)", "滚转(°)",
        "速度(km/h)"
    });
    // Stretch 模式：列按比例平分占满整行，无空白
    ui.tableWidget_platData->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tableWidget_platData->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui.tableWidget_platData->horizontalHeader()->setMinimumSectionSize(12);
    ui.tableWidget_platData->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableWidget_platData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableWidget_platData->verticalHeader()->setVisible(false);
    ui.tableWidget_platData->setWordWrap(false);


    // ============ 目标数据表格（ui 已创建，直接配置）============
    ui.tableWidget_targetData->setColumnCount(11);
    ui.tableWidget_targetData->setRowCount(7);
    ui.tableWidget_targetData->setHorizontalHeaderLabels({
       "目标类型", "目标ID", "挂载平台ID",
        "纬度(°)", "经度(°)", "海拔(m)",
        "航向(°)", "俯仰(°)", "滚转(°)",
        "在视场", "状态"
    });
    // Stretch 模式：列按比例平分占满整行，无空白
    ui.tableWidget_targetData->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tableWidget_targetData->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui.tableWidget_targetData->horizontalHeader()->setMinimumSectionSize(12);
    ui.tableWidget_targetData->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableWidget_targetData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableWidget_targetData->verticalHeader()->setVisible(false);
    ui.tableWidget_targetData->setWordWrap(false);
    for(auto* table:{ui.tableWidget_platData,ui.tableWidget_targetData}){
        table->setMinimumSize(0,0);
        table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        for(int r=0;r<table->rowCount();++r)for(int c=0;c<table->columnCount();++c)
            table->setItem(r,c,new QTableWidgetItem());
        for(int c=0;c<table->columnCount();++c){auto* h=table->horizontalHeaderItem(c);h->setText(h->text().replace("(","\n(").replace(QString::fromUtf8("挂载平台ID"),QString::fromUtf8("平台\nID")));}
        table->horizontalHeader()->setDefaultSectionSize(60);
        for(int column=0;column<table->columnCount();++column)table->setColumnWidth(column,60);
        table->verticalHeader()->setDefaultSectionSize(32);
        table->setTextElideMode(Qt::ElideNone);
    }
}

// ==================== 视频标签居中 + 自适应缩放 ====================
void HwaSim_IR_VideoDisplay::centerVideoLabel()
{
    QWidget* parent = ui.widget_video;
    QLabel* label = ui.m_Label_Video;
    int pw = parent->width();
    int ph = parent->height();

    const qreal dpr=parent->devicePixelRatioF();
    const int sourceW=m_lastVideoImage.isNull()?m_maxImageWidth:m_lastVideoImage.width();
    const int sourceH=m_lastVideoImage.isNull()?m_maxImageHeight:m_lastVideoImage.height();
    const auto physical=HwaVideoFit::fit(sourceW,sourceH,pw*dpr,ph*dpr);
    if(!physical.width||!physical.height)return;
    // A pixmap retains physical pixel data; DPR describes its logical extent.
    // The label must never scale the pixmap a second time.
    const int targetW=int(std::ceil(physical.width/dpr)),targetH=int(std::ceil(physical.height/dpr));
    label->setScaledContents(false);label->setAlignment(Qt::AlignCenter);
    label->setGeometry((pw-targetW)/2,(ph-targetH)/2,targetW,targetH);
    if(!m_lastVideoImage.isNull()){
        QImage pixels=m_lastVideoImage;
        if(pixels.width()!=physical.width||pixels.height()!=physical.height)
            pixels=pixels.scaled(physical.width,physical.height,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        QPixmap pixmap=QPixmap::fromImage(pixels);pixmap.setDevicePixelRatio(dpr);label->setPixmap(pixmap);
    }
}

bool HwaSim_IR_VideoDisplay::eventFilter(QObject* watched, QEvent* event)
{
	if(watched==ui.m_Label_Video&&event&&event->type()==QEvent::Paint&&
		!m_lastVideoImage.isNull()&&m_pendingPaintFrameSeq>0&&
		m_pendingPaintFrameSeq!=m_lastPaintedFrameSeq){
		const qint64 now=steadyTimeNs();
		if(m_guiPaintFrames==0)m_guiPaintFirstSteadyNs=now;
		if(m_guiPaintLastSteadyNs>0)m_guiPaintMaxIntervalNs=std::max(
			m_guiPaintMaxIntervalNs,now-m_guiPaintLastSteadyNs);
		m_guiPaintLastSteadyNs=now;m_lastPaintedFrameSeq=m_pendingPaintFrameSeq;++m_guiPaintFrames;
		if(m_pendingPaintFrameSeq>180){
			if(m_guiPaintSteadyFrames==0)m_guiPaintSteadyFirstNs=now;
			if(m_guiPaintSteadyLastNs>0)m_guiPaintSteadyMaxIntervalNs=std::max(
				m_guiPaintSteadyMaxIntervalNs,now-m_guiPaintSteadyLastNs);
			m_guiPaintSteadyLastNs=now;++m_guiPaintSteadyFrames;
		}
		if(m_guiPaintFrames<=3||(m_guiPaintFrames%120u)==0u)logGuiPaintPerf("interval");
	}
	return QWidget::eventFilter(watched,event);
}

void HwaSim_IR_VideoDisplay::logGuiPaintPerf(const char* reason) const
{
	const double elapsedSec=m_guiPaintFrames>1&&m_guiPaintLastSteadyNs>m_guiPaintFirstSteadyNs?
		double(m_guiPaintLastSteadyNs-m_guiPaintFirstSteadyNs)/1.0e9:0.0;
	const double fps=elapsedSec>0?double(m_guiPaintFrames-1)/elapsedSec:0.0;
	const double steadyElapsedSec=m_guiPaintSteadyFrames>1&&m_guiPaintSteadyLastNs>m_guiPaintSteadyFirstNs?
		double(m_guiPaintSteadyLastNs-m_guiPaintSteadyFirstNs)/1.0e9:0.0;
	const double steadyFps=steadyElapsedSec>0?double(m_guiPaintSteadyFrames-1)/steadyElapsedSec:0.0;
	qInfo().noquote()<<QStringLiteral(
		"[GuiPaintPerf] reason=%1 semantic=QLabel_Paint_event_after_setPixmap paintedFrames=%2 submittedFrames=%3 paintFps=%4 maxFrameIntervalMs=%5 lastPaintedFrameSeq=%6 pendingFrameSeq=%7 steadyExclusionThroughFrameSeq=180 steadyPaintedFrames=%8 steadyPaintFps=%9 steadyMaxFrameIntervalMs=%10")
		.arg(QString::fromLatin1(reason?reason:"unknown"))
		.arg(m_guiPaintFrames).arg(m_videoPerfFrames).arg(fps,0,'f',3)
		.arg(double(m_guiPaintMaxIntervalNs)/1.0e6,0,'f',3)
		.arg(m_lastPaintedFrameSeq).arg(m_pendingPaintFrameSeq)
		.arg(m_guiPaintSteadyFrames).arg(steadyFps,0,'f',3)
		.arg(double(m_guiPaintSteadyMaxIntervalNs)/1.0e6,0,'f',3);
}

void HwaSim_IR_VideoDisplay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    fitTelemetryTables();
    centerVideoLabel();
}

// ==================== 目标类型中文映射 ====================
QString HwaSim_IR_VideoDisplay::targetTypeName(int type)
{
    switch (type) {
    case 0x11: return "飞机";
    case 0x12: return QString::fromUtf8("飞机");
    case 0x22: return "雷达导弹";
    case 0x33: return "红外弹";
    case 0x44: return "MMD";
    default:   return "未知";
    }
}

// ==================== 目标状态中文映射 ====================
QString HwaSim_IR_VideoDisplay::targetStateName(int state)
{
    switch (state) {
    case 0x01: return "打击态";
    case 0x02: return "爆炸态";
    case 0x03: return "击毁态";
    default:   return "正常";
    }
}

// ==================== 更新平台空间状态列 ====================
void HwaSim_IR_VideoDisplay::updatePlatDataTable(int platID, const BYHWICD::SpatialState& platLoc)
{
    // 用平台ID匹配行（第0列），只更新匹配行的空间状态列
    for (int r = 0; r < ui.tableWidget_platData->rowCount(); ++r) {
        QTableWidgetItem* idItem = ui.tableWidget_platData->item(r, 0);
        if (!idItem)
            continue;
        bool ok = false;
        int rowPlatID = idItem->text().toInt(&ok);
        if (!ok || rowPlatID != platID)
            continue;

        // 保证各列 item 存在（正常情况在 initCommandReceivedSlot 中已创建）
        for (int c = 2; c <= 8; ++c) {
            if (!ui.tableWidget_platData->item(r, c))
                ui.tableWidget_platData->setItem(r, c, new QTableWidgetItem());
        }

        // 第2~8列：纬度 经度 海拔 航向 俯仰 滚转 速度
        ui.tableWidget_platData->item(r, 2)->setText(QString::number(platLoc.lat, 'f', 6));
        ui.tableWidget_platData->item(r, 3)->setText(QString::number(platLoc.lon, 'f', 6));
        ui.tableWidget_platData->item(r, 4)->setText(QString::number(platLoc.alt, 'f', 2));
        ui.tableWidget_platData->item(r, 5)->setText(QString::number(platLoc.yaw, 'f', 2));
        ui.tableWidget_platData->item(r, 6)->setText(QString::number(platLoc.pitch, 'f', 2));
        ui.tableWidget_platData->item(r, 7)->setText(QString::number(platLoc.roll, 'f', 2));
        ui.tableWidget_platData->item(r, 8)->setText(QString::number(platLoc.speed, 'f', 2));
        return;  // 找到匹配行后提前返回
    }
}

// ==================== 更新目标数据表格 ====================
void HwaSim_IR_VideoDisplay::updateTargetDataTable(const BYHWICD::DisplayC2cObjTrackingData& data)
{
    auto* table=ui.tableWidget_targetData;
    const int count=qBound(0,data.targetNumValid,5);
    for(int row=0;row<7;++row){
        QStringList values;
        if(row<count){const auto& t=data.targetState[row];
            values<<targetTypeName(t.targetType)<<QString::number(t.targetID)<<QString::number(t.targetPlatID)
                <<QString::number(t.targetLoc.lat,'f',6)<<QString::number(t.targetLoc.lon,'f',6)
                <<QString::number(t.targetLoc.alt,'f',1)<<QString::number(t.targetLoc.yaw,'f',1)
                <<QString::number(t.targetLoc.pitch,'f',1)<<QString::number(t.targetLoc.roll,'f',1)
                <<(t.viewValid?QString::fromUtf8("是"):QString::fromUtf8("否"))<<targetStateName(t.targetState);
        }
        for(int col=0;col<table->columnCount();++col)table->item(row,col)->setText(col<values.size()?values[col]:QString());
    }
}

void HwaSim_IR_VideoDisplay::CloseStorage()
{
    flushRecorder("close");
}

bool HwaSim_IR_VideoDisplay::flushRecorder(const char* reason)
{
    if (!m_recorder)
    {
        return true;
    }
    const bool flushed = m_recorder->stopAndFlush(m_recorderFlushTimeoutMs);
    const RecorderSnapshot snapshot = m_recorder->snapshot();
    qInfo().noquote()
        << QStringLiteral("[RecorderFlush] completed=%1 reason=%2 queueDepth=%3 inputFrames=%4 writtenFrames=%5 droppedFrames=%6 frameSeqWritten=%7 sourceSeqWritten=%8 frameSeqContinuousWritten=%9 sourceSeqContinuousWritten=%10 outputPath=%11")
            .arg(flushed ? 1 : 0)
            .arg(QString::fromLatin1(reason ? reason : "unknown"))
            .arg(snapshot.queueDepth)
            .arg(snapshot.inputFrames)
            .arg(snapshot.writtenFrames)
            .arg(snapshot.droppedFrames)
            .arg(snapshot.frameSeqWritten)
            .arg(snapshot.sourceSeqWritten)
            .arg(snapshot.frameSeqContinuousWritten ? 1 : 0)
            .arg(snapshot.sourceSeqContinuousWritten ? 1 : 0)
            .arg(snapshot.outputPath);
    if (!flushed)
    {
        qWarning().noquote()
            << QStringLiteral("[RecorderPerf][WARN] flushTimeoutMs=%1 reason=%2 queueDepth=%3 writtenFrames=%4 droppedFrames=%5")
                .arg(m_recorderFlushTimeoutMs)
                .arg(QString::fromLatin1(reason))
                .arg(snapshot.queueDepth)
                .arg(snapshot.writtenFrames)
                .arg(snapshot.droppedFrames);
    }
    return flushed;
}

void HwaSim_IR_VideoDisplay::resetVideoPerfStats()
{
    m_liveFrameTimes.clear();
    updateLiveFps();
    m_videoPerfFrames = 0;
    m_videoPerfIntervalFrames = 0;
    m_lastFrameSeq = 0;
    m_frameSeqDiscontinuities = 0;
    m_receivedFrameBaseline = receivedFrameCount();
    m_lastReceivedFrameCount = m_receivedFrameBaseline;
    m_intervalSourceSeqContinuous = true;
    m_videoPerfReceiveStartNs = 0;
    m_videoPerfDisplayStartNs = 0;
    m_lastVideoPerfLogNs = wallTimeNs();
    m_decodeMsTotal = 0.0;
    m_displayMsTotal = 0.0;
    m_recordingEnqueueMsTotal = 0.0;
    m_recordingEnqueueMsMax = 0.0;
    m_latencyMsTotal = 0.0;
    m_latencyMsMax = 0.0;
    m_latencySamples = 0;
    m_latencyIntervalSamples.clear();
	m_pendingPaintFrameSeq = 0;
	m_lastPaintedFrameSeq = 0;
	m_guiPaintFrames = 0;
	m_guiPaintFirstSteadyNs = 0;
	m_guiPaintLastSteadyNs = 0;
	m_guiPaintMaxIntervalNs = 0;
	m_guiPaintSteadyFrames = 0;
	m_guiPaintSteadyFirstNs = 0;
	m_guiPaintSteadyLastNs = 0;
	m_guiPaintSteadyMaxIntervalNs = 0;
}

void HwaSim_IR_VideoDisplay::updateLiveFps()
{
    static const bool freezeTables=qEnvironmentVariableIntValue("P8FreezeTelemetryTables")==1;
    if(m_uiDataPending&&!freezeTables){updatePlatDataTable(m_pendingUiData.platID,m_pendingUiData.platLoc);updateTargetDataTable(m_pendingUiData);fitTelemetryTables();m_uiDataPending=false;}
    if(!m_liveFpsLabel)return;
    const qint64 now=m_liveFpsClock.elapsed();
    while(!m_liveFrameTimes.empty()&&m_liveFrameTimes.front()<=now-1000)m_liveFrameTimes.pop_front();
    const QJsonObject metrics=m_ddsWorker?m_ddsWorker->telemetrySnapshot():QJsonObject();
    const bool telemetryFresh=metrics.value("available").toBool();
    const QString finishedKey=metrics.value("server").toString()+"/"+metrics.value("generation").toString()+"/"+metrics.value("run").toString();
    if(metrics.value("finished").toBool()&&finishedKey!=m_finishedProductKey&&
       metrics.value("server")==m_lastProductIdentity.value("session")&&
       metrics.value("generation")==m_lastProductIdentity.value("generation")&&
       metrics.value("run")==m_lastProductIdentity.value("run")&&
       m_lastProductIdentity.value("frameSeq").toString().toULongLong()>=metrics.value("outputSeq").toString().toULongLong()) {
        CloseStorage();m_finishedProductKey=finishedKey;
        qInfo().noquote()<<"[RecorderProductEnd]"<<finishedKey<<" lastFrame="<<metrics.value("outputSeq").toString();
    }
    const double fps=m_ddsWorker?metrics.value("videoFps").toDouble():double(m_liveFrameTimes.size());
    if(m_metricLabels[1]){
        m_metricLabels[1]->setText(QString::fromUtf8("数据接收 Hz  ")+(telemetryFresh?QString::number(metrics.value("acceptedHz").toDouble(),'f',1):QString::fromUtf8("—")));
        const bool hasControl=metrics.value("controlSequence").toString().toULongLong()>0;
        const bool timed=hasControl&&metrics.value("controlTimeValid").toBool();
        const int command=metrics.value("controlCommand").toInt();
        const QString name=command==1?QString::fromUtf8("复位"):command==2?QString::fromUtf8("开始"):command==3?QString::fromUtf8("停止"):QString::fromUtf8("—");
        m_metricLabels[2]->setText(QString::fromUtf8("控制指令响应（等效） %1 Hz\n接收→开始执行 %2｜最近指令：%3%4")
            .arg(timed?QString::number(metrics.value("controlEquivalentHz").toDouble(),'f',1):QString::fromUtf8("—"))
            .arg(timed?QString::number(metrics.value("controlResponseMs").toDouble(),'f',3)+" ms":hasControl?QString::fromUtf8("低于计时分辨率"):QString::fromUtf8("—"))
            .arg(name).arg(hasControl&&!telemetryFresh?QString::fromUtf8(" · 已过期"):QString()));
        m_metricLabels[2]->setToolTip(QString::fromUtf8("仅复位、开始、停止；接收至业务开始等待的倒数，不代表命令吞吐能力。"));
        const bool valid=telemetryFresh&&metrics.value("clockValid").toBool()&&metrics.value("outputLatencyMs").toDouble(-1)>=0;
        m_metricLabels[3]->setText(valid?QString::fromUtf8("输出延时 ≈%1 ms · 估计 ±%2 ms").arg(metrics.value("outputLatencyMs").toDouble(),0,'f',1).arg(metrics.value("clockErrorMs").toDouble(),0,'f',1):QString::fromUtf8("输出延时 — · 时间基准未就绪/过期"));
    }
    if(m_metricLabels[0])m_metricLabels[0]->setText(QString::fromUtf8("视频 FPS  %1").arg(fps,0,'f',1));
	const quint64 statusSamples = metrics.value("statusSamples").toString().toULongLong();
	const quint64 statusAccepted = metrics.value("statusAccepted").toString().toULongLong();
	const quint64 statusRejected = metrics.value("statusIdentityRejected").toString().toULongLong();
	const quint64 receivedSamples = metrics.value("receivedSamples").toString().toULongLong();
	const quint64 decodedFrames = metrics.value("decodedFrames").toString().toULongLong();
	const quint64 decodeWaits = metrics.value("decodeWaits").toString().toULongLong();
	const quint64 decodeErrors = metrics.value("decodeErrors").toString().toULongLong();
	QString receiverState;
	if (!m_receiverFatalError.isEmpty())
		receiverState = QString::fromUtf8("接收错误：%1").arg(m_receiverFatalError);
	else if (statusAccepted == 0 && statusRejected > 0)
		receiverState = QString::fromUtf8("身份/配置不匹配：期望 %1/%2，已拒绝 %3 条状态")
			.arg(m_platID).arg(m_sensorID).arg(statusRejected);
	else if (statusAccepted == 0)
		receiverState = QString::fromUtf8("等待 VideoStatus：Identity %1/%2 · 已观察 %3 条")
			.arg(m_platID).arg(m_sensorID).arg(statusSamples);
	else if (!m_statusRunning)
		receiverState = QString::fromUtf8("状态已发现，等待有效 INIT/START · %1 · round %2")
			.arg(m_statusTopic).arg(m_statusRound);
	else if (receivedSamples == 0)
		receiverState = QString::fromUtf8("已绑定 %1，但尚无视频样本").arg(m_statusTopic);
	else if (decodedFrames == 0 && decodeErrors > 0)
		receiverState = QString::fromUtf8("解码错误 %1 次 · 已收样本 %2").arg(decodeErrors).arg(receivedSamples);
	else if (decodedFrames == 0 && decodeWaits > 0)
		receiverState = QString::fromUtf8("已收样本，等待 H.264 关键帧 · %1 次").arg(decodeWaits);
	else if (m_lastGuiFrameMs >= 0 && now - m_lastGuiFrameMs > 2000)
		receiverState = QString::fromUtf8("输入中断/画面已过期 · 最后帧 %1").arg(m_lastFrameSeq);
	else
		receiverState = QString::fromUtf8("正常显示 · 当前帧 %1 · DDS样本 %2 · 解码 %3")
			.arg(m_lastFrameSeq).arg(receivedSamples).arg(decodedFrames);
	m_liveFpsLabel->setText(QString::fromUtf8("%1\n实时接收显示 %2 FPS  |  最近 1 秒新图  |  异步请求 %3 FPS (0=不限)  |  %4 × %5")
		.arg(receiverState).arg(fps,0,'f',1)
		.arg(m_requestedVideoFps>=0?QString::number(m_requestedVideoFps):QString("?" ))
		.arg(m_maxImageWidth).arg(m_maxImageHeight));
    if(m_recorder&&m_recorder->snapshot().fileError)
        m_liveFpsLabel->setText(m_liveFpsLabel->text()+QString::fromUtf8("  |  录像写入失败，请检查日志与存储"));
    if(now-m_lastLiveFpsLogMs>=1000){
        qInfo().noquote()<<QString("[LiveReceivedFps] fps=%1 windowMs=1000 source=receiver_decoded_new_images requestedHz=%2")
            .arg(fps,0,'f',1).arg(m_videoFps);m_lastLiveFpsLogMs=now;
        qInfo().noquote()<<"[RuntimeMetricsV2]"<<QJsonDocument(metrics).toJson(QJsonDocument::Compact);
    }
    // Explicit acceptance capture of the actual widget; never writes on video pixels.
    const QByteArray dump=qgetenv("P6ReceiverUiDump");
    if(!m_uiCaptureSaved&&!dump.isEmpty()&&fps>0&&m_videoPerfFrames>=quint64(qMax(1,m_videoFps)*3)){
        m_uiCaptureSaved=grab().save(QString::fromLocal8Bit(dump));
        qInfo().noquote()<<QString("[ReceiverUiCapture] saved=%1 file=%2 fps=%3 title=%4").arg(m_uiCaptureSaved).arg(QString::fromLocal8Bit(dump)).arg(fps).arg(ui.dockWidget_dataShow->windowTitle());
        if(m_uiCaptureSaved&&qEnvironmentVariableIntValue("P6ReceiverUiResponsiveCapture")==1) captureResponsiveUi(0);
    }
}

quint64 HwaSim_IR_VideoDisplay::receivedFrameCount() const
{
	if (m_ddsWorker) return m_ddsWorker->receivedFrameCount();
	if (m_worker) return m_worker->receivedFrameCount();
	return m_videoPerfFrames;
}

void HwaSim_IR_VideoDisplay::videoStatusReceivedSlot(const QString& topic,
	const QString& codec, const QString& pixelFormat, int width, int height,
	int fps, bool running, int currentRound)
{
	m_statusTopic = topic;
	m_statusCodec = codec;
	m_statusRunning = running;
	m_statusRound = currentRound;
	m_statusWidth = qMax(0, width);
	m_statusHeight = qMax(0, height);
	m_videoFps = qMax(1, fps);
	if (m_statusWidth > 0 && m_statusHeight > 0)
	{
		m_maxImageWidth = m_statusWidth;
		m_maxImageHeight = m_statusHeight;
		centerVideoLabel();
	}
	qInfo().noquote() << QStringLiteral(
		"[VideoStatus] received=1 topic=%1 codec=%2 pixelFormat=%3 width=%4 height=%5 fps=%6 running=%7 round=%8")
		.arg(topic).arg(codec).arg(pixelFormat).arg(width).arg(height).arg(fps)
		.arg(running ? 1 : 0).arg(currentRound);
}

// ==================== 图像帧接收槽 ====================
void HwaSim_IR_VideoDisplay::imageReceivedSlot(
    const QImage& img,
    const BYHWICD::DisplayC2cObjTrackingData& data,
    const QString& annotationJson,
    bool hasVideo,
    bool hasRealtimeData,
    bool hasAnnotation,
    int packetVersion,
    quint32 sectionFlags,
    int codecId,
    bool keyFrame,
    quint64 packetFrameSeq,
    quint64 outputOrdinal,
    qint64 ptsMs,
    qint64 receiveTimeNs,
    double jpegDecodeMs,
    int decodedChannels,
    const QString& imageFormat,
    const QByteArray& encodedAu)
{
    const qint64 guiBeginSteadyNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(hasVideo&&m_ddsWorker)m_ddsWorker->acknowledgeGuiFrame();
    if (!hasVideo)
    {
        // Metadata-only v3 packets must not decode or clear the last displayed frame.
        if (hasRealtimeData)
        {
            m_pendingUiData=data;m_uiDataPending=true;
        }
        return;
    }

    QElapsedTimer displayTimer;
    displayTimer.start();
    if (img.width() > 0 && img.height() > 0)
    {
		if (m_statusWidth > 0 && m_statusHeight > 0 &&
			(img.width() != m_statusWidth || img.height() != m_statusHeight))
		{
			qWarning().noquote() << QStringLiteral(
				"[VideoGeometry][WARN] status=%1x%2 decoded=%3x%4")
				.arg(m_statusWidth).arg(m_statusHeight).arg(img.width()).arg(img.height());
		}
		m_liveFrameTimes.push_back(m_liveFpsClock.elapsed());
		m_lastGuiFrameMs = m_liveFpsClock.elapsed();
		m_maxImageWidth = img.width();
		m_maxImageHeight = img.height();
		m_lastVideoImage=img;
		m_pendingPaintFrameSeq=packetFrameSeq?packetFrameSeq:(m_videoPerfFrames+1);
		centerVideoLabel();
		if (m_videoPerfFrames < 3 || ((m_videoPerfFrames + 1) % 120) == 0)
		{
			qInfo().noquote() << QStringLiteral(
				"[VideoGeometry] statusWidth=%1 statusHeight=%2 decodedWidth=%3 decodedHeight=%4 labelWidth=%5 labelHeight=%6 aspectPreserved=1")
				.arg(m_statusWidth).arg(m_statusHeight).arg(img.width()).arg(img.height())
				.arg(ui.m_Label_Video->width()).arg(ui.m_Label_Video->height());
		}
    }
    const double displayMs = static_cast<double>(displayTimer.nsecsElapsed()) / 1.0e6;
    const qint64 shownTimeNs = wallTimeNs();
    const qint64 guiSubmitSteadyNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

    const bool updateUiData = m_videoPerfFrames < 3 ||
        (m_videoPerfFrames % static_cast<quint64>(qMax(1, m_uiUpdateEveryFrames))) == 0;
    if (updateUiData && hasRealtimeData)
    {
        m_pendingUiData=data;m_uiDataPending=true;
    }

    quint64 frameSeq = packetFrameSeq;
    qint64 udpReceiveTimeNs = 0;
    qint64 tcpSendTimeNs = 0;
    if (packetVersion == 0 || packetVersion == 4)
	{
		m_decodeCodec = codecId == 2 ? QStringLiteral("h264_annexb")
			: (imageFormat == QStringLiteral("grayscale") ? QStringLiteral("raw_gray8")
				: QStringLiteral("raw_bgr24"));
		m_activeCodec = m_decodeCodec;
		m_requestedCodec = m_decodeCodec;
		m_h264Requested = codecId == 2;
		if (m_h264Requested && keyFrame) m_h264KeyFrameSeen = true;
	}
    else if (packetVersion == 3)
    {
        m_decodeCodec = codecId == 2
            ? QStringLiteral("h264_annexb")
            : (codecId == 1 ? QStringLiteral("jpeg") : QStringLiteral("none"));
        m_activeCodec = m_decodeCodec;
        m_requestedCodec = m_decodeCodec == QStringLiteral("h264_annexb")
            ? QStringLiteral("h264")
            : m_decodeCodec;
        m_h264Requested = m_decodeCodec == QStringLiteral("h264_annexb");
        // Media PTS is not a wall-clock timestamp.
        if (m_decodeCodec == QStringLiteral("h264_annexb") && keyFrame)
        {
            m_h264KeyFrameSeen = true;
        }
    }
    else
    {
        m_decodeCodec = imageFormat == QStringLiteral("grayscale")
            ? QStringLiteral("jpeg_gray")
            : QStringLiteral("jpeg");
    }
    if (!annotationJson.isEmpty())
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(annotationJson.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
        {
            const QJsonObject object = document.object();
            if (frameSeq == 0)
            {
                frameSeq = object.value("sourceSeq").toVariant().toULongLong();
                if (frameSeq == 0)
                {
                    frameSeq = object.value("frameSeq").toVariant().toULongLong();
                }
            }
            udpReceiveTimeNs = object.value("udpReceiveTimeNs").toString().toLongLong();
            tcpSendTimeNs = object.value("tcpSendTimeNs").toString().toLongLong();
            m_requestedCodec = object.value("requestedCodec").toString(QStringLiteral("jpeg"));
            if (packetVersion != 3 && packetVersion != 4)
            {
                m_activeCodec = object.value("activeCodec").toString(QStringLiteral("jpeg"));
                m_decodeCodec = object.value("payloadCodec").toString(
                    object.value("codec").toString(m_activeCodec));
            }
            m_h264Requested = object.value("h264En").toBool(false);
            m_codecFallbackReason = object.value("codecFallbackReason").toString(QStringLiteral("none"));
            if (m_decodeCodec == QStringLiteral("h264_annexb") &&
                (keyFrame || object.value("keyFrame").toBool(false)))
            {
                m_h264KeyFrameSeen = true;
            }
            if (m_decodeCodec == QStringLiteral("h264_annexb") && img.isNull())
            {
                ++m_h264DecodeErrors;
            }
        }
    }
    if (m_decodeCodec.isEmpty())
    {
        m_decodeCodec = imageFormat == QStringLiteral("grayscale")
            ? QStringLiteral("jpeg_gray")
            : QStringLiteral("jpeg");
    }
    m_decodedChannels = decodedChannels;
    m_imageFormat = imageFormat;

    ++m_videoPerfFrames;
    ++m_videoPerfIntervalFrames;
    m_decodeMsTotal += jpegDecodeMs;
    m_displayMsTotal += displayMs;
    if (m_videoPerfReceiveStartNs == 0)
    {
        m_videoPerfReceiveStartNs = receiveTimeNs;
        m_videoPerfDisplayStartNs = shownTimeNs;
    }
    if (frameSeq > 0)
    {
        if (m_lastFrameSeq > 0 && frameSeq != m_lastFrameSeq + 1)
        {
            ++m_frameSeqDiscontinuities;
            m_intervalSourceSeqContinuous = false;
        }
        m_lastFrameSeq = frameSeq;
    }

    double endToEndMs = -1.0;
    const QJsonObject annotationMetrics = QJsonDocument::fromJson(annotationJson.toUtf8()).object();
    const QJsonObject productMetrics = annotationMetrics.value("_frameProduct").toObject();
    if (productMetrics.value("outputLatencyEstimated").toBool())
    {
        endToEndMs = productMetrics.value("outputLatencyMs").toDouble(-1);
        if (endToEndMs < 60000.0)
        {
            m_latencyMsTotal += endToEndMs;
            m_latencyMsMax = qMax(m_latencyMsMax, endToEndMs);
            ++m_latencySamples;
            m_latencyIntervalSamples.push_back(endToEndMs);
        }
    }
    const double tcpToReceiveMs = -1.0; // Legacy host clocks have no verified shared epoch.

    const QJsonObject identity=productMetrics;
    if(!identity.isEmpty())m_lastProductIdentity=identity;
    if (m_recorder && (identity.isEmpty()?m_saveMP4Requested:identity.value("saveRequested").toBool()))
    {
        QElapsedTimer enqueueTimer;
        enqueueTimer.start();
        RecordingFrame recordingFrame;
        recordingFrame.product=identity;
        recordingFrame.product.insert("guiBeginSteadyNs",QString::number(guiBeginSteadyNs));
        recordingFrame.product.insert("guiSubmitSteadyNs",QString::number(guiSubmitSteadyNs));
        recordingFrame.frameSeq=frameSeq;
        const auto jsonUInt64 = [](const QJsonValue& value) -> quint64 {
            if (value.isString())
            {
                return value.toString().toULongLong();
            }
            if (value.isDouble())
            {
                const double number = value.toDouble();
                return number > 0.0 ? static_cast<quint64>(number) : 0;
            }
            return 0;
        };
        recordingFrame.sourceSeq=jsonUInt64(productMetrics.value("sourceSeq"));
        if (recordingFrame.sourceSeq == 0)
        {
            // TCP v3 JPEG packets carry the authoritative producer sourceSeq
            // in the annotation root rather than in a DDS FrameProduct.
            recordingFrame.sourceSeq=jsonUInt64(annotationMetrics.value("sourceSeq"));
        }
        if (recordingFrame.sourceSeq > 0)
        {
            recordingFrame.product.insert("sourceSeq",QString::number(recordingFrame.sourceSeq));
        }
        recordingFrame.association=identity.isEmpty()
            ? QStringLiteral("TCP_PACKET_V3_JSON")
            : QStringLiteral("AU_SEI_V2");
        recordingFrame.ptsMs=ptsMs;
        recordingFrame.encodedAu=encodedAu;
        recordingFrame.keyFrame=keyFrame;
        recordingFrame.image = img;
        recordingFrame.trackingData = data;
        recordingFrame.annotationJson = annotationJson;
        recordingFrame.hasRealtimeData = hasRealtimeData;
        recordingFrame.hasAnnotation = hasAnnotation;
        recordingFrame.receiveTimeNs = receiveTimeNs;
        recordingFrame.displayTimeNs = shownTimeNs;
        const bool recorderAccepted = m_recorder->enqueue(recordingFrame);
        const double enqueueMs = static_cast<double>(enqueueTimer.nsecsElapsed()) / 1.0e6;
        m_recordingEnqueueMsTotal += enqueueMs;
        m_recordingEnqueueMsMax = qMax(m_recordingEnqueueMsMax, enqueueMs);
        if (frameSeq <= 3 || (!recorderAccepted && (frameSeq % 120) == 0))
        {
            const RecorderSnapshot recorderState = m_recorder->snapshot();
            qInfo().noquote()
                << QStringLiteral("[RecorderEnqueue] frameSeq=%1 accepted=%2 enabled=%3 pending=%4 initialized=%5 accepting=%6 shutdown=%7 fileError=%8 queueDepth=%9 productIdentity=%10 productSaveRequested=%11")
                    .arg(frameSeq)
                    .arg(recorderAccepted ? 1 : 0)
                    .arg(recorderState.recordingEnabled ? 1 : 0)
                    .arg(recorderState.pending ? 1 : 0)
                    .arg(recorderState.initialized ? 1 : 0)
                    .arg(recorderState.accepting ? 1 : 0)
                    .arg(recorderState.shutdownRequested ? 1 : 0)
                    .arg(recorderState.fileError ? 1 : 0)
                    .arg(recorderState.queueDepth)
                    .arg(identity.isEmpty() ? 0 : 1)
                    .arg(identity.value("saveRequested").toBool() ? 1 : 0);
        }
        if (enqueueMs > 1.0)
        {
            qWarning().noquote()
                << QStringLiteral("[RecorderPerf][WARN] enqueueMs=%1 sourceSeq=%2")
                    .arg(enqueueMs, 0, 'f', 3)
                    .arg(frameSeq);
        }
    }

    const bool shouldLog = shownTimeNs - m_lastVideoPerfLogNs >= 2000000000LL;
    if (shouldLog)
    {
        const double intervalElapsedSec = qMax(
            0.001,
            static_cast<double>(shownTimeNs - m_lastVideoPerfLogNs) / 1.0e9);
        const quint64 receivedFrameCount = this->receivedFrameCount();
        const quint64 receivedIntervalFrames = receivedFrameCount >= m_lastReceivedFrameCount
            ? receivedFrameCount - m_lastReceivedFrameCount
            : 0;
        const quint64 receivedSinceReset = receivedFrameCount >= m_receivedFrameBaseline
            ? receivedFrameCount - m_receivedFrameBaseline
            : 0;
        const quint64 queueDepth = receivedSinceReset > m_videoPerfFrames
            ? receivedSinceReset - m_videoPerfFrames
            : 0;
        const double sampleCount = static_cast<double>(qMax<quint64>(1, m_videoPerfIntervalFrames));
        QVector<double> sortedLatencies = m_latencyIntervalSamples;
        std::sort(sortedLatencies.begin(), sortedLatencies.end());
        double latencyP95Ms = -1.0;
        if (!sortedLatencies.isEmpty())
        {
            const int p95Index = qMin(
                sortedLatencies.size() - 1,
                static_cast<int>(std::ceil(static_cast<double>(sortedLatencies.size()) * 0.95)) - 1);
            latencyP95Ms = sortedLatencies[qMax(0, p95Index)];
        }
        QString videoPerfLine = QStringLiteral(
            "[VideoPerf] channel=%1 platID=%2 sensorID=%3 pid=%4")
                .arg(m_channel)
                .arg(m_platID)
                .arg(m_sensorID)
                .arg(m_pid);
        videoPerfLine += QString(" receiveFps=%1 displayFps=%2 decodeMsAvg=%3 queueDepth=%4 sourceSeqContinuous=%5 latencyAvgMs=%6 latencyP95Ms=%7 displayMsAvg=%8 tcpToReceiveMs=%9 sourceSeq=%10 discontinuities=%11 recordingEnqueueMsAvg=%12 recordingEnqueueMsMax=%13 decodedChannels=%14 imageFormat=%15 requestedCodec=%16 activeCodec=%17 decodeCodec=%18 h264En=%19 codecFallbackReason=%20 h264KeyFrameSeen=%21 h264DecodeErrors=%22 packetVersion=%23 flags=0x%24 codecId=%25 keyFrame=%26 outputOrdinal=%27 ptsMs=%28 hasAnnotation=%29 hasRealtimeData=%30")
                .arg(static_cast<double>(receivedIntervalFrames) / intervalElapsedSec, 0, 'f', 3)
                .arg(static_cast<double>(m_videoPerfIntervalFrames) / intervalElapsedSec, 0, 'f', 3)
                .arg(m_decodeMsTotal / sampleCount, 0, 'f', 3)
                .arg(queueDepth)
                .arg(m_intervalSourceSeqContinuous ? 1 : 0)
                .arg(m_latencySamples > 0 ? m_latencyMsTotal / static_cast<double>(m_latencySamples) : -1.0, 0, 'f', 3)
                .arg(latencyP95Ms, 0, 'f', 3)
                .arg(m_displayMsTotal / sampleCount, 0, 'f', 3)
                .arg(tcpToReceiveMs, 0, 'f', 3)
                .arg(frameSeq)
                .arg(m_frameSeqDiscontinuities)
                .arg(m_recordingEnqueueMsTotal / sampleCount, 0, 'f', 3)
                .arg(m_recordingEnqueueMsMax, 0, 'f', 3)
                .arg(m_decodedChannels)
                .arg(m_imageFormat)
                .arg(m_requestedCodec)
                .arg(m_activeCodec)
                .arg(m_decodeCodec)
                .arg(m_h264Requested ? 1 : 0)
                .arg(m_codecFallbackReason)
                .arg(m_h264KeyFrameSeen ? 1 : 0)
                .arg(m_h264DecodeErrors)
                .arg(packetVersion)
                .arg(sectionFlags, 0, 16)
                .arg(codecId)
                .arg(keyFrame ? 1 : 0)
                .arg(outputOrdinal)
                .arg(ptsMs)
                .arg(hasAnnotation ? 1 : 0)
                .arg(hasRealtimeData ? 1 : 0);
        qInfo().noquote() << videoPerfLine;
        m_videoPerfIntervalFrames = 0;
        m_lastReceivedFrameCount = receivedFrameCount;
        m_videoPerfReceiveStartNs = receiveTimeNs;
        m_videoPerfDisplayStartNs = shownTimeNs;
        m_lastVideoPerfLogNs = shownTimeNs;
        m_intervalSourceSeqContinuous = true;
        m_decodeMsTotal = 0.0;
        m_displayMsTotal = 0.0;
        m_recordingEnqueueMsTotal = 0.0;
        m_recordingEnqueueMsMax = 0.0;
        m_latencyMsTotal = 0.0;
        m_latencyMsMax = 0.0;
        m_latencySamples = 0;
        m_latencyIntervalSamples.clear();
    }
}

// ==================== 初始化命令接收槽 ====================
void HwaSim_IR_VideoDisplay::initCommandReceivedSlot(const BYHWICD::InitP2cObjectTrackingCmd& cmd)
{
    // ----- 传感器参数（原有）-----
    ui.lineEdit_envMaxHeightRain->setText(QString::number(cmd.trackingInit.envMaxHeightRain));
    ui.lineEdit_envTransHeightRain->setText(QString::number(cmd.trackingInit.envTransHeightRain));
    ui.lineEdit_envMaxHeightSnow->setText(QString::number(cmd.trackingInit.envMaxHeightSnow));
    ui.lineEdit_envTransHeightSnow->setText(QString::number(cmd.trackingInit.envTransHeightSnow));
    ui.lineEdit_envRainSnowSpeedScale->setText(QString::number(cmd.trackingInit.envRainSnowSpeedScale));
    ui.lineEdit_envRadScaleTerrain->setText(QString::number(cmd.trackingInit.envRadScaleTerrain));
    ui.lineEdit_envRadScaleSky->setText(QString::number(cmd.trackingInit.envRadScaleSky));
    ui.lineEdit_envTemp->setText(QString::number(cmd.trackingInit.envTemp));
    ui.lineEdit_envHumidity->setText(QString::number(cmd.trackingInit.envHumidity));
    ui.lineEdit_envVisibility->setText(QString::number(cmd.trackingInit.envVisibility));
    ui.lineEdit_envWindV->setText(QString::number(cmd.trackingInit.envWindV));
    ui.lineEdit_envWindDir->setText(QString::number(cmd.trackingInit.envWindDir));
    ui.lineEdit_videoFps->setText(QString::number(cmd.trackingInit.videoFps));

    m_videoFps = cmd.trackingInit.videoFps > 0 ? cmd.trackingInit.videoFps : 25;
    m_uiUpdateEveryFrames = qMax(1, m_videoFps / 5);
    // 保存开关来自 TCP 转发的初始化命令；开始命令只负责进入待录制状态。
    m_saveMP4Requested = cmd.trackingInit.trackerSensor[0].saveMP4En;
    if (m_recorder)
    {
        m_recorder->configure(
            m_saveMP4Requested,
            m_videoFps,
            m_maxRecordingQueueFrames);
    }
    resetVideoPerfStats();

    switch (cmd.trackingInit.envTerrain)
    {
    case 0:
        ui.lineEdit_sceneType->setText("戈壁");
        break;
    case 1:
        ui.lineEdit_sceneType->setText("山区");
        break;
    case 2:
        ui.lineEdit_sceneType->setText("海面");
        break;
    default:
        break;
    }

    switch (cmd.trackingInit.envSky)
    {
    case 0:
        ui.lineEdit_envSky->setText("晴");
        break;
    case 1:
        ui.lineEdit_envSky->setText("云");
        break;
    case 2:
        ui.lineEdit_envSky->setText("雨");
        break;
    case 3:
        ui.lineEdit_envSky->setText("雪");
        break;
    case 4:
        ui.lineEdit_envSky->setText("雾");
        break;
    case 5:
        ui.lineEdit_envSky->setText("阴");
        break;
    default:
        break;
    }

    //ui.lineEdit_sensorIndex->setText(QString::number(cmd.trackingInit.trackerSensor[0].index));
   /* if (cmd.trackingInit.trackerSensor[0].coarseTrackEn)
        ui.lineEdit_coarseTrackEn->setText("是");
    else
        ui.lineEdit_coarseTrackEn->setText("否");*/

   /* if (cmd.trackingInit.trackerSensor[0].preciseTrackEn)
        ui.lineEdit_preciseTrackEn->setText("是");
    else
        ui.lineEdit_preciseTrackEn->setText("否");*/

    if (cmd.trackingInit.trackerSensor[0].h264En)
        ui.lineEdit_h264En->setText("是");
    else
        ui.lineEdit_h264En->setText("否");
    m_h264Requested = cmd.trackingInit.trackerSensor[0].h264En;
    m_requestedCodec = m_h264Requested ? QStringLiteral("h264") : QStringLiteral("jpeg");
    m_activeCodec = QStringLiteral("pending");
    m_codecFallbackReason = QStringLiteral("pending");
    qInfo().noquote()
        << QStringLiteral("[CodecStatus] requestedCodec=%1 activeCodec=%2 h264En=%3 codecFallbackReason=%4")
            .arg(m_requestedCodec)
            .arg(m_activeCodec)
            .arg(m_h264Requested ? 1 : 0)
            .arg(m_codecFallbackReason);

    //ui.lineEdit_coarseTrackResolution->setText(QString::number(cmd.trackingInit.trackerSensor[0].coarseTrackResolution));
    //ui.lineEdit_preciseTrackResolution->setText(QString::number(cmd.trackingInit.trackerSensor[0].preciseTrackResolution));

    if (cmd.trackingInit.trackerSensor[0].noiseEn)
        ui.lineEdit_noiseEn->setText("是");
    else
        ui.lineEdit_noiseEn->setText("否");

    ui.lineEdit_trackerSensorNoise->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorNoise));

    if (cmd.trackingInit.trackerSensor[0].realtimeAnnotation)
        ui.lineEdit_realtimeAnnotation->setText("是");
    else
        ui.lineEdit_realtimeAnnotation->setText("否");

    if (m_saveMP4Requested)
        ui.lineEdit_saveMP4En->setText("是（异步）");
    else
        ui.lineEdit_saveMP4En->setText("否");

    switch (cmd.trackingInit.trackerSensor[0].trackerSensorBand)
    {
    case 0:
        ui.lineEdit_trackerSensorBand->setText("短波红外");
        break;
    case 1:
        ui.lineEdit_trackerSensorBand->setText(QString::fromUtf8("近红外（NIR）"));
        break;
    case 2:
        ui.lineEdit_trackerSensorBand->setText("中波红外");
        break;
    default:
        break;
    }

    ui.lineEdit_trackerSensorWidth->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorWidth));
    ui.lineEdit_trackerSensorHeight->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorHeight));

    // 根据成像分辨率设置视频显示区域
    int imgW = cmd.trackingInit.trackerSensor[0].trackerSensorWidth;
    int imgH = cmd.trackingInit.trackerSensor[0].trackerSensorHeight;
    if (imgW > 0 && imgH > 0) {
        m_maxImageWidth = imgW;
        m_maxImageHeight = imgH;
        centerVideoLabel();
    }
    ui.lineEdit_trackerSensorViewMin->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorViewMin));
    ui.lineEdit_trackerSensorViewMax->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorViewMax));
    ui.lineEdit_trackerSensorPixelAngle->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorPixelAngle));

    const auto& pp=cmd.platParamInit;
    ui.tableWidget_platData->item(0,0)->setText(QString::number(pp.id));
    ui.tableWidget_platData->item(0,1)->setText(pp.type==1?QString::fromUtf8("红方"):QString::fromUtf8("蓝方"));
    updatePlatDataTable(pp.id,pp.spatial);
    for(int i=0;i<HwaSensorFields::count;++i){const auto& field=HwaSensorFields::fields()[i];
        const double value=HwaSensorFields::get(cmd.trackingInit.trackerSensor[0],field);
        m_sensorFields[i]->setText(field.kind==HwaSensorFields::Boolean?(value?QString::fromUtf8("是"):QString::fromUtf8("否")):QString::number(value,'g',12));
        if(field.kind==HwaSensorFields::Band){
            const QString names[]={QString::fromUtf8("短波（SWIR）"),QString::fromUtf8("近红外（NIR）"),QString::fromUtf8("中波（MWIR）"),QString::fromUtf8("长波（LWIR）"),QString::fromUtf8("可见光")};
            const int band=int(value);if(band>=0&&band<5)m_sensorFields[i]->setText(names[band]);
        }
        qInfo().noquote()<<"[P7SensorReadback]"<<field.name<<QString::number(value,'g',17);
    }
    m_requestedVideoFps=cmd.trackingInit.videoFps;
    m_simModeDisplay->setText(cmd.trackingInit.simMode==1?QString::fromUtf8("同步 (1)"):cmd.trackingInit.simMode==2?QString::fromUtf8("异步 (2)"):QString::number(cmd.trackingInit.simMode));
    ui.lineEdit_videoFps->setText(cmd.trackingInit.videoFps==0?QString::fromUtf8("0 · 不限帧"):QString::number(cmd.trackingInit.videoFps));
    fitTelemetryTables();
}

// ==================== 控制命令接收槽 ====================
void HwaSim_IR_VideoDisplay::controlCmdReceivedSlot(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
    switch (cmd.simCommand)
    {
    case 1: // 复位
    {
        if(!m_ddsWorker)CloseStorage();
        resetVideoPerfStats();
        for(auto* table:{ui.tableWidget_platData,ui.tableWidget_targetData})
            for(int r=0;r<table->rowCount();++r)for(int c=0;c<table->columnCount();++c)table->item(r,c)->setText(QString());
        ui.lineEdit_controlType->clear();
        qDebug() << QString::fromUtf8("收到复位命令");
        break;
    }
    case 2: // 开始
    {
        if(!m_ddsWorker)CloseStorage();
        resetVideoPerfStats();

        ui.lineEdit_controlType->setText(QString("运行 %1/%2")
                                .arg(cmd.currentRound).arg(cmd.roundCut));

        if (!m_saveMP4Requested || !m_recorder) {
            qDebug() << QString::fromUtf8("收到开始命令，本回合不录制")
                     << QStringLiteral("saveMP4En=") << m_saveMP4Requested;
            break;
        }

        if(m_ddsWorker)break; // first identified AU starts its own session, including late joining
        const QString baseDirectory = QApplication::applicationDirPath() + QStringLiteral("/MP4");
        if (!m_recorder->startPending(cmd.currentRound, baseDirectory))
        {
            qWarning().noquote()
                << QStringLiteral("[RecorderPerf][WARN] startPendingFailed round=%1")
                    .arg(cmd.currentRound);
            break;
        }
        qDebug() << QString::fromUtf8("收到开始命令，异步录像等待有效目标数据，round=") << cmd.currentRound;
        break;
    }
    case 3: // 停止
    {
        ui.lineEdit_controlType->setText("已停止");
        if(!m_ddsWorker)CloseStorage();
        const RecorderSnapshot snapshot = m_recorder
            ? m_recorder->snapshot()
            : RecorderSnapshot();
        qDebug() << QString::fromUtf8("收到停止命令，异步录像写入") << snapshot.writtenFrames
                 << QString::fromUtf8("帧，丢弃") << snapshot.droppedFrames
                 << QString::fromUtf8("路径") << snapshot.outputPath;
        break;
    }
    default:
        break;
    }
}
