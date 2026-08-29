#include "HwaSim_IR_VideoDisplay.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
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
}

HwaSim_IR_VideoDisplay::HwaSim_IR_VideoDisplay(
    const QString& networkConfigPath,
    const QString& channel,
    int platID,
    int sensorID,
	const QString& receiveTransport,
	const QString& ddsTopic,
	const QString& ddsCodec,
	const QString& ddsQos,
	int ddsDomain,
	int ddsWidth,
	int ddsHeight,
	int ddsFps,
	const QString& ddsDumpFirstFrame,
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
			QStringLiteral("tcp")).toString().trimmed().toLower();
	if (m_receiveTransport != QStringLiteral("tcp") && m_receiveTransport != QStringLiteral("dds"))
	{
		qCritical().noquote() << QStringLiteral("[VideoInput][FATAL] invalid Transport=%1").arg(m_receiveTransport);
		m_receiveTransport = QStringLiteral("tcp");
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
    setWindowTitle("红外仿真图像接收器");
    showMaximized();

    // m_Label_Video 居中 + 自适应缩放
    ui.m_Label_Video->setScaledContents(false);
    ui.m_Label_Video->setAlignment(Qt::AlignCenter);

    // 设置 dockWidget
    ui.dockWidget_dataShow->setWindowTitle("数据显示");

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
			QStringLiteral("Config/DDS/ZRDDS_QOS_PROFILES.xml")).toString();
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
		config.topicVideoStatus = instanceSettings.value(QStringLiteral("DdsProtocol/TopicVideoStatus"),
			QStringLiteral("HwaSimIR.VideoStatus")).toString();
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
		connect(m_workerThread, &QThread::finished, m_ddsWorker, &QObject::deleteLater);
		connect(m_workerThread, &QThread::started, m_ddsWorker, &DdsVideoReceiverWorker::doWork);
		ui.dockWidget_dataShow->setWindowTitle(QStringLiteral("数据显示 - DDS full transport"));
		qInfo().noquote() << QStringLiteral("[VideoInput] Transport=dds topic=%1 codec=%2 domain=%3")
			.arg(config.topic).arg(config.codec).arg(config.domainId);
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

void HwaSim_IR_VideoDisplay::InitTables()
{
    // ============ 平台数据表格（ui 已创建，直接配置）============
    ui.tableWidget_platData->setColumnCount(9);
    ui.tableWidget_platData->setHorizontalHeaderLabels({
        "平台ID", "阵营",
        "纬度(°)", "经度(°)", "海拔(m)",
        "航向(°)", "俯仰(°)", "滚转(°)",
        "速度(km/h)"
    });
    // Stretch 模式：列按比例平分占满整行，无空白
    ui.tableWidget_platData->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.tableWidget_platData->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui.tableWidget_platData->horizontalHeader()->setMinimumSectionSize(28);
    ui.tableWidget_platData->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableWidget_platData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableWidget_platData->verticalHeader()->setVisible(false);
    ui.tableWidget_platData->setWordWrap(false);


    // ============ 目标数据表格（ui 已创建，直接配置）============
    ui.tableWidget_targetData->setColumnCount(11);
    ui.tableWidget_targetData->setHorizontalHeaderLabels({
       "目标类型", "目标ID", "挂载平台ID",
        "纬度(°)", "经度(°)", "海拔(m)",
        "航向(°)", "俯仰(°)", "滚转(°)",
        "在视场", "状态"
    });
    // Stretch 模式：列按比例平分占满整行，无空白
    ui.tableWidget_targetData->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui.tableWidget_targetData->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui.tableWidget_targetData->horizontalHeader()->setMinimumSectionSize(28);
    ui.tableWidget_targetData->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableWidget_targetData->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableWidget_targetData->verticalHeader()->setVisible(false);
    ui.tableWidget_targetData->setWordWrap(false);
}

// ==================== 视频标签居中 + 自适应缩放 ====================
void HwaSim_IR_VideoDisplay::centerVideoLabel()
{
    QWidget* parent = ui.widget_video;
    QLabel* label = ui.m_Label_Video;
    int pw = parent->width();
    int ph = parent->height();

    // 计算目标尺寸：填满父控件，但不超过图像最大分辨率，保持宽高比
    int maxW = m_maxImageWidth > 0 ? m_maxImageWidth : pw;
    int maxH = m_maxImageHeight > 0 ? m_maxImageHeight : ph;
    int targetW = qMin(pw, maxW);
    int targetH = qMin(ph, maxH);
    if (maxW > 0 && maxH > 0) {
        double ratio = (double)maxW / maxH;
        if (targetW > targetH * ratio)
            targetW = static_cast<int>(targetH * ratio);
        else
            targetH = static_cast<int>(targetW / ratio);
    }
    if (targetW < 320) targetW = 320;
    if (targetH < 240) targetH = 240;

    label->resize(targetW, targetH);
    int x = (pw - targetW) / 2;
    int y = (ph - targetH) / 2;
    label->move(x, y);
}

void HwaSim_IR_VideoDisplay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    centerVideoLabel();
}

// ==================== 目标类型中文映射 ====================
QString HwaSim_IR_VideoDisplay::targetTypeName(int type)
{
    switch (type) {
    case 0x11: return "飞机";
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
    ui.tableWidget_targetData->setRowCount(0);

    // 遍历 targetState[5]，targetType == 0 时停止
    for (int i = 0; i < 5; ++i) {
        const BYHWICD::TargetState& ts = data.targetState[i];
        if (ts.targetType == 0)
            break;

        int row = ui.tableWidget_targetData->rowCount();
        ui.tableWidget_targetData->insertRow(row);

        ui.tableWidget_targetData->setItem(row, 0, new QTableWidgetItem(targetTypeName(ts.targetType)));
        ui.tableWidget_targetData->setItem(row, 1, new QTableWidgetItem(QString::number(ts.targetID)));
        ui.tableWidget_targetData->setItem(row, 2, new QTableWidgetItem(QString::number(ts.targetPlatID)));
        ui.tableWidget_targetData->setItem(row, 3, new QTableWidgetItem(QString::number(ts.targetLoc.lat, 'f', 6)));
        ui.tableWidget_targetData->setItem(row, 4, new QTableWidgetItem(QString::number(ts.targetLoc.lon, 'f', 6)));
        ui.tableWidget_targetData->setItem(row, 5, new QTableWidgetItem(QString::number(ts.targetLoc.alt, 'f', 2)));
        ui.tableWidget_targetData->setItem(row, 6, new QTableWidgetItem(QString::number(ts.targetLoc.yaw, 'f', 2)));
        ui.tableWidget_targetData->setItem(row, 7, new QTableWidgetItem(QString::number(ts.targetLoc.pitch, 'f', 2)));
        ui.tableWidget_targetData->setItem(row, 8, new QTableWidgetItem(QString::number(ts.targetLoc.roll, 'f', 2)));
        ui.tableWidget_targetData->setItem(row, 9, new QTableWidgetItem(ts.viewValid ? "是" : "否"));
        ui.tableWidget_targetData->setItem(row, 10, new QTableWidgetItem(targetStateName(ts.targetState)));
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
    const QString& imageFormat)
{
    if (!hasVideo)
    {
        // Metadata-only v3 packets must not decode or clear the last displayed frame.
        if (hasRealtimeData)
        {
            updatePlatDataTable(data.platID, data.platLoc);
            updateTargetDataTable(data);
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
		m_maxImageWidth = img.width();
		m_maxImageHeight = img.height();
		centerVideoLabel();
		ui.m_Label_Video->setPixmap(QPixmap::fromImage(img).scaled(
			ui.m_Label_Video->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
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

    const bool updateUiData = m_videoPerfFrames < 3 ||
        (m_videoPerfFrames % static_cast<quint64>(qMax(1, m_uiUpdateEveryFrames))) == 0;
    if (updateUiData && hasRealtimeData)
    {
        updatePlatDataTable(data.platID, data.platLoc);
        updateTargetDataTable(data);
    }

    quint64 frameSeq = packetFrameSeq;
    qint64 udpReceiveTimeNs = 0;
    qint64 tcpSendTimeNs = 0;
    if (packetVersion == 0)
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
        if (ptsMs > 0)
        {
            udpReceiveTimeNs = ptsMs * 1000000LL;
        }
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
    if (hasAnnotation && !annotationJson.isEmpty())
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
            if (packetVersion != 3)
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
    if (udpReceiveTimeNs > 0 && shownTimeNs >= udpReceiveTimeNs)
    {
        endToEndMs = static_cast<double>(shownTimeNs - udpReceiveTimeNs) / 1.0e6;
        if (endToEndMs < 60000.0)
        {
            m_latencyMsTotal += endToEndMs;
            m_latencyMsMax = qMax(m_latencyMsMax, endToEndMs);
            ++m_latencySamples;
            m_latencyIntervalSamples.push_back(endToEndMs);
        }
    }
    const double tcpToReceiveMs = tcpSendTimeNs > 0 && receiveTimeNs >= tcpSendTimeNs
        ? static_cast<double>(receiveTimeNs - tcpSendTimeNs) / 1.0e6
        : -1.0;

    if (m_recorder && m_saveMP4Requested)
    {
        QElapsedTimer enqueueTimer;
        enqueueTimer.start();
        RecordingFrame recordingFrame;
        recordingFrame.sourceSeq = frameSeq;
        recordingFrame.image = img;
        recordingFrame.trackingData = data;
        recordingFrame.annotationJson = annotationJson;
        recordingFrame.hasRealtimeData = hasRealtimeData;
        recordingFrame.hasAnnotation = hasAnnotation;
        recordingFrame.receiveTimeNs = receiveTimeNs;
        recordingFrame.displayTimeNs = shownTimeNs;
        m_recorder->enqueue(recordingFrame);
        const double enqueueMs = static_cast<double>(enqueueTimer.nsecsElapsed()) / 1.0e6;
        m_recordingEnqueueMsTotal += enqueueMs;
        m_recordingEnqueueMsMax = qMax(m_recordingEnqueueMsMax, enqueueMs);
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
        const double displayElapsedSec = qMax(
            0.001,
            static_cast<double>(shownTimeNs - m_videoPerfDisplayStartNs) / 1.0e9);
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
                .arg(static_cast<double>(receivedIntervalFrames) / displayElapsedSec, 0, 'f', 3)
                .arg(static_cast<double>(m_videoPerfIntervalFrames) / displayElapsedSec, 0, 'f', 3)
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

    // ----- 平台数据表格填充 -----
    ui.tableWidget_platData->setRowCount(0);
   // int count = cmd.platNumValid;
   // if (count > 2) count = 2;  // platParam 固定长度 2
   // for (int i = 0; i < count; ++i) {
        const BYHWICD::PlatParamPak& pp = cmd.platParamInit;
        int row = ui.tableWidget_platData->rowCount();
        ui.tableWidget_platData->insertRow(row);

        ui.tableWidget_platData->setItem(row, 0, new QTableWidgetItem(QString::number(pp.id)));
        ui.tableWidget_platData->setItem(row, 1, new QTableWidgetItem(pp.type == 1 ? "红方" : "蓝方"));
        ui.tableWidget_platData->setItem(row, 2, new QTableWidgetItem(QString::number(pp.spatial.lat, 'f', 6)));
        ui.tableWidget_platData->setItem(row, 3, new QTableWidgetItem(QString::number(pp.spatial.lon, 'f', 6)));
        ui.tableWidget_platData->setItem(row, 4, new QTableWidgetItem(QString::number(pp.spatial.alt, 'f', 2)));
        ui.tableWidget_platData->setItem(row, 5, new QTableWidgetItem(QString::number(pp.spatial.yaw, 'f', 2)));
        ui.tableWidget_platData->setItem(row, 6, new QTableWidgetItem(QString::number(pp.spatial.pitch, 'f', 2)));
        ui.tableWidget_platData->setItem(row, 7, new QTableWidgetItem(QString::number(pp.spatial.roll, 'f', 2)));
        ui.tableWidget_platData->setItem(row, 8, new QTableWidgetItem(QString::number(pp.spatial.speed, 'f', 2)));
   // }
}

// ==================== 控制命令接收槽 ====================
void HwaSim_IR_VideoDisplay::controlCmdReceivedSlot(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
    switch (cmd.simCommand)
    {
    case 1: // 复位
    {
        CloseStorage();
        resetVideoPerfStats();
        ui.tableWidget_platData->setRowCount(0);
        ui.tableWidget_targetData->setRowCount(0);
        ui.lineEdit_controlType->clear();
        qDebug() << QStringLiteral("收到复位命令");
        break;
    }
    case 2: // 开始
    {
        CloseStorage();
        resetVideoPerfStats();

        ui.lineEdit_controlType->setText(QString("运行中-第%1/共%2回合")
                                .arg(cmd.currentRound).arg(cmd.roundCut));

        if (!m_saveMP4Requested || !m_recorder) {
            qDebug() << QStringLiteral("收到开始命令，本回合不录制")
                     << QStringLiteral("saveMP4En=") << m_saveMP4Requested;
            break;
        }

        const QString baseDirectory = QApplication::applicationDirPath() + QStringLiteral("/MP4");
        if (!m_recorder->startPending(cmd.currentRound, baseDirectory))
        {
            qWarning().noquote()
                << QStringLiteral("[RecorderPerf][WARN] startPendingFailed round=%1")
                    .arg(cmd.currentRound);
            break;
        }
        qDebug() << QStringLiteral("收到开始命令，异步录像等待有效目标数据，round=") << cmd.currentRound;
        break;
    }
    case 3: // 停止
    {
        ui.lineEdit_controlType->setText("已停止");
        CloseStorage();
        const RecorderSnapshot snapshot = m_recorder
            ? m_recorder->snapshot()
            : RecorderSnapshot();
        qDebug() << QStringLiteral("收到停止命令，异步录像写入") << snapshot.writtenFrames
                 << QStringLiteral("帧，丢弃") << snapshot.droppedFrames
                 << QStringLiteral("路径") << snapshot.outputPath;
        break;
    }
    default:
        break;
    }
}
