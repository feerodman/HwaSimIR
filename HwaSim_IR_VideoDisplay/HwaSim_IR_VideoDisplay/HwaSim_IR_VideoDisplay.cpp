#include "HwaSim_IR_VideoDisplay.h"

#include <QVBoxLayout>
#include <QHeaderView>
#include <QResizeEvent>
#include <QDebug>
#include <QFile>
#include <QApplication>
#include <QDir>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
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

HwaSim_IR_VideoDisplay::HwaSim_IR_VideoDisplay(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    setWindowTitle("红外仿真图像接收器");
    showMaximized();

    // m_Label_Video 居中 + 自适应缩放
    ui.m_Label_Video->setScaledContents(true);

    // 设置 dockWidget
    ui.dockWidget_dataShow->setWindowTitle("数据显示");

    // 动态创建表格
    InitTables();

	InitQss();

    // worker 线程...
    m_workerThread = new QThread(this);
    m_worker = new TcpServerWorker();
    m_worker->moveToThread(m_workerThread);

    // 收到图像信号 → 更新显示
    connect(m_worker, &TcpServerWorker::dataReceived, this, &HwaSim_IR_VideoDisplay::imageReceivedSlot);
    // 收到初始化
    connect(m_worker, &TcpServerWorker::initCommandReceived, this, &HwaSim_IR_VideoDisplay::initCommandReceivedSlot);
    // 收到控制命令
    connect(m_worker, &TcpServerWorker::controlCmdReceived, this, &HwaSim_IR_VideoDisplay::controlCmdReceivedSlot);
    // 线程结束时自动删除 worker
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    // 线程启动时执行 doWork
    connect(m_workerThread, &QThread::started, m_worker, &TcpServerWorker::doWork);
    m_workerThread->start();
}

HwaSim_IR_VideoDisplay::~HwaSim_IR_VideoDisplay()
{
    // 先停止工作线程，确保不再有信号投递到主线程
    m_worker->stop();
    m_workerThread->quit();
    m_workerThread->wait();
    // 线程完全停止后再释放录制资源
    CloseStorage();
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

bool HwaSim_IR_VideoDisplay::hasTargetStateData(const BYHWICD::DisplayC2cObjTrackingData& data) const
{
    // 空实时帧的 targetState 通常为清零状态；以 targetType 是否非 0 判断是否已有有效目标数据。
    for (int i = 0; i < 5; ++i) {
        if (data.targetState[i].targetType != 0)
            return true;
    }
    return false;
}

bool HwaSim_IR_VideoDisplay::beginPendingStorage()
{
    if (m_isRecording)
        return true;

    if (!m_recordingPending || !m_saveMP4Requested)
        return false;

    // 收到开始命令后不立刻落盘，等第一帧 targetState 有数据时再创建本回合保存目录。
    QString baseDir = QApplication::applicationDirPath() + "/MP4";
    if (!QDir().mkpath(baseDir)) {
        qWarning() << "创建MP4基础目录失败:" << baseDir;
        return false;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_currentRoundDir = QString("%1/round_%2_%3")
                        .arg(baseDir)
                        .arg(m_pendingRound, 3, 10, QChar('0'))
                        .arg(timestamp);
    if (!QDir().mkpath(m_currentRoundDir)) {
        qWarning() << "创建回合保存目录失败:" << m_currentRoundDir;
        return false;
    }

    QString annotPath = m_currentRoundDir + "/annotations.txt";
    m_annotFile = new QFile(annotPath);
    if (m_annotFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_annotStream = new QTextStream(m_annotFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // 实时数据文本固定为 UTF-8，避免中文状态字段乱码。
        m_annotStream->setCodec("UTF-8");
#endif
        *m_annotStream << "frame_id,time_ms,platID,sensorID,targetIdx,targetType,targetPlatID,targetID,lat,lon,alt,yaw,pitch,roll,viewValid,targetState\n";
    }
    else {
        qWarning() << "打开实时数据保存文件失败:" << annotPath;
        delete m_annotFile;
        m_annotFile = nullptr;
    }

    QString targetAnnotPath = m_currentRoundDir + "/target_annotations.txt";
    m_targetAnnotFile = new QFile(targetAnnotPath);
    if (m_targetAnnotFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_targetAnnotStream = new QTextStream(m_targetAnnotFile);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_targetAnnotStream->setCodec("UTF-8");
#endif
    }
    else {
        qWarning() << "打开目标标注保存文件失败:" << targetAnnotPath;
        delete m_targetAnnotFile;
        m_targetAnnotFile = nullptr;
    }

    m_frameCount = 0;
    m_recordingPending = false;
    m_isRecording = true;
    qDebug() << "检测到有效目标数据，开始保存:" << m_currentRoundDir;
    return true;
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

// ==================== 帧存储：MP4 + 标注文件 ====================
QString HwaSim_IR_VideoDisplay::fallbackAnnotationJson(int frameIndex, const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& data) const
{
	return  QString("{\"version\":1,\"enabled\":false,\"frameIndex\":%1,\"simTimeMs\":%2,\"sensorID\":%3,\"width\":%4,\"height\":%5,\"targets\":[]}")
		.arg(frameIndex)
		.arg(data.time, 0, 'f', 3)
		.arg(data.sensorID)
		.arg(img.width())
		.arg(img.height());
}

void HwaSim_IR_VideoDisplay::saveFrameToStorage(const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& data, const QString& annotationJson)
{
    if (!m_isRecording) {
        if (!m_recordingPending)
            return;

        if (!hasTargetStateData(data)) {
            // 已收到开始命令但实时目标数据仍为空，此帧只显示不保存，避免视频/标注与有效数据错位。
            return;
        }

        if (!beginPendingStorage())
            return;
    }

    // 第一帧时延迟创建 VideoWriter（此时才知晓图像尺寸）
    if (!m_videoWriter) {
        QString videoPath = m_currentRoundDir + "/output.mp4";
        cv::Size frameSize(img.width(), img.height());

        // 尝试多种编码器：H264 → XVID → MJPG（MJPG 在 Windows 上最可靠）
        struct CodecTry { int fourcc; const char* name; };
        const CodecTry codecs[] = {
            { cv::VideoWriter::fourcc('H','2','6','4'), "H264" },
            { cv::VideoWriter::fourcc('X','V','I','D'), "XVID" },
            { cv::VideoWriter::fourcc('M','J','P','G'), "MJPG" },
        };
        for (const auto& c : codecs) {
            m_videoWriter = new cv::VideoWriter(videoPath.toStdString(), c.fourcc,
                                                m_videoFps, frameSize, true);
            if (m_videoWriter->isOpened()) {
                qDebug() << "VideoWriter 已创建:" << videoPath
                         << frameSize.width << "x" << frameSize.height
                         << "编码:" << c.name;
                break;
            }
            delete m_videoWriter;
            m_videoWriter = nullptr;
        }
        if (!m_videoWriter) {
            qWarning() << "VideoWriter 打开失败（尝试了 H264/XVID/MJPG）:" << videoPath;
            return;
        }
    }

    // 防御性检查：VideoWriter 可能在上方创建失败或中途被释放
    if (!m_videoWriter || !m_videoWriter->isOpened())
        return;

    // 写入视频帧：QImage → cv::Mat (BGR)
    QImage rgbImg = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat src(rgbImg.height(), rgbImg.width(), CV_8UC3,
                const_cast<uchar*>(rgbImg.bits()),
                static_cast<size_t>(rgbImg.bytesPerLine()));
    cv::Mat dst;
    cv::cvtColor(src, dst, cv::COLOR_RGB2BGR);
    m_videoWriter->write(dst);

    ++m_frameCount;

    // 写入标注行
    if (m_annotStream) {
        for (int i = 0; i < 5; ++i) {
            const BYHWICD::TargetState& ts = data.targetState[i];
            if (ts.targetType == 0)
                break;

            *m_annotStream << m_frameCount << ","
                           << data.time << ","
                           << data.platID << ","
                           << data.sensorID << ","
                           << i << ","
                           << targetTypeName(ts.targetType) << ","
                           << ts.targetPlatID << ","
                           << ts.targetID << ","
                           << ts.targetLoc.lat << ","
                           << ts.targetLoc.lon << ","
                           << ts.targetLoc.alt << ","
                           << ts.targetLoc.yaw << ","
                           << ts.targetLoc.pitch << ","
                           << ts.targetLoc.roll << ","
                           << (ts.viewValid ? "是" : "否") << ","
                           << targetStateName(ts.targetState) << "\n";
        }
    }

    // Stage2B：标注 JSON 单独保存，一帧图像对应 target_annotations.txt 中一行。
    if (m_targetAnnotStream) {
        QString line = annotationJson.trimmed();
        if (line.isEmpty()) {
            line = fallbackAnnotationJson(m_frameCount, img, data);
        }
        *m_targetAnnotStream << line << "\n";
    }
}

// ==================== 关闭存储文件 ====================
void HwaSim_IR_VideoDisplay::CloseStorage()
{
    m_recordingPending = false;
    m_isRecording = false;

    if (m_videoWriter) {
        if (m_videoWriter->isOpened())
            m_videoWriter->release();
        delete m_videoWriter;
        m_videoWriter = nullptr;
    }

    if (m_annotStream) {
        m_annotStream->flush();
        delete m_annotStream;
        m_annotStream = nullptr;
    }
    if (m_annotFile) {
        if (m_annotFile->isOpen())
            m_annotFile->close();
        delete m_annotFile;
        m_annotFile = nullptr;
    }

    if (m_targetAnnotStream) {
        m_targetAnnotStream->flush();
        delete m_targetAnnotStream;
        m_targetAnnotStream = nullptr;
    }
    if (m_targetAnnotFile) {
        if (m_targetAnnotFile->isOpen())
            m_targetAnnotFile->close();
        delete m_targetAnnotFile;
        m_targetAnnotFile = nullptr;
    }

    m_frameCount = 0;
    m_currentRoundDir.clear();
}

void HwaSim_IR_VideoDisplay::resetVideoPerfStats()
{
    m_videoPerfFrames = 0;
    m_videoPerfIntervalFrames = 0;
    m_lastFrameSeq = 0;
    m_frameSeqDiscontinuities = 0;
    m_receivedFrameBaseline = m_worker ? m_worker->receivedFrameCount() : 0;
    m_lastReceivedFrameCount = m_receivedFrameBaseline;
    m_intervalSourceSeqContinuous = true;
    m_videoPerfReceiveStartNs = 0;
    m_videoPerfDisplayStartNs = 0;
    m_lastVideoPerfLogNs = wallTimeNs();
    m_decodeMsTotal = 0.0;
    m_displayMsTotal = 0.0;
    m_latencyMsTotal = 0.0;
    m_latencyMsMax = 0.0;
    m_latencySamples = 0;
    m_latencyIntervalSamples.clear();
}

// ==================== 图像帧接收槽 ====================
void HwaSim_IR_VideoDisplay::imageReceivedSlot(
    const QImage& img,
    const BYHWICD::DisplayC2cObjTrackingData& data,
    const QString& annotationJson,
    qint64 receiveTimeNs,
    double jpegDecodeMs)
{
    QElapsedTimer displayTimer;
    displayTimer.start();
    ui.m_Label_Video->setPixmap(QPixmap::fromImage(img));
    const double displayMs = static_cast<double>(displayTimer.nsecsElapsed()) / 1.0e6;
    const qint64 shownTimeNs = wallTimeNs();

    const bool updateUiData = m_videoPerfFrames < 3 ||
        (m_videoPerfFrames % static_cast<quint64>(qMax(1, m_uiUpdateEveryFrames))) == 0;
    if (updateUiData)
    {
        updatePlatDataTable(data.platID, data.platLoc);
        updateTargetDataTable(data);
    }

    // 如正在录制，保存帧
    if (!m_storageSuppressedForRealtime)
    {
        saveFrameToStorage(img, data, annotationJson);
    }

    quint64 frameSeq = 0;
    qint64 udpReceiveTimeNs = 0;
    qint64 tcpSendTimeNs = 0;
    if (!annotationJson.isEmpty())
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(annotationJson.toUtf8(), &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
        {
            const QJsonObject object = document.object();
            frameSeq = object.value("sourceSeq").toVariant().toULongLong();
            if (frameSeq == 0)
            {
                frameSeq = object.value("frameSeq").toVariant().toULongLong();
            }
            udpReceiveTimeNs = object.value("udpReceiveTimeNs").toString().toLongLong();
            tcpSendTimeNs = object.value("tcpSendTimeNs").toString().toLongLong();
        }
    }

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
    const bool shouldLog = shownTimeNs - m_lastVideoPerfLogNs >= 2000000000LL;
    if (shouldLog)
    {
        const double displayElapsedSec = qMax(
            0.001,
            static_cast<double>(shownTimeNs - m_videoPerfDisplayStartNs) / 1.0e9);
        const quint64 receivedFrameCount = m_worker ? m_worker->receivedFrameCount() : m_videoPerfFrames;
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
        qInfo().noquote()
            << QString("[VideoPerf] receiveFps=%1 displayFps=%2 decodeMsAvg=%3 queueDepth=%4 sourceSeqContinuous=%5 latencyAvgMs=%6 latencyP95Ms=%7 displayMsAvg=%8 tcpToReceiveMs=%9 sourceSeq=%10 discontinuities=%11 storageSuppressed=%12")
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
                .arg(m_storageSuppressedForRealtime ? 1 : 0);
        m_videoPerfIntervalFrames = 0;
        m_lastReceivedFrameCount = receivedFrameCount;
        m_videoPerfReceiveStartNs = receiveTimeNs;
        m_videoPerfDisplayStartNs = shownTimeNs;
        m_lastVideoPerfLogNs = shownTimeNs;
        m_intervalSourceSeqContinuous = true;
        m_decodeMsTotal = 0.0;
        m_displayMsTotal = 0.0;
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
    m_storageSuppressedForRealtime = m_saveMP4Requested && m_videoFps >= 60;
    resetVideoPerfStats();
    if (m_storageSuppressedForRealtime)
    {
        qWarning().noquote()
            << "[VideoPerf][WARN] storageSuppressed=1 reason=videoFps_60_realtime";
    }

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

    ui.lineEdit_sensorIndex->setText(QString::number(cmd.trackingInit.trackerSensor[0].index));
    if (cmd.trackingInit.trackerSensor[0].coarseTrackEn)
        ui.lineEdit_coarseTrackEn->setText("是");
    else
        ui.lineEdit_coarseTrackEn->setText("否");

    if (cmd.trackingInit.trackerSensor[0].preciseTrackEn)
        ui.lineEdit_preciseTrackEn->setText("是");
    else
        ui.lineEdit_preciseTrackEn->setText("否");

    if (cmd.trackingInit.trackerSensor[0].h264En)
        ui.lineEdit_h264En->setText("是");
    else
        ui.lineEdit_h264En->setText("否");

    ui.lineEdit_coarseTrackResolution->setText(QString::number(cmd.trackingInit.trackerSensor[0].coarseTrackResolution));
    ui.lineEdit_preciseTrackResolution->setText(QString::number(cmd.trackingInit.trackerSensor[0].preciseTrackResolution));

    if (cmd.trackingInit.trackerSensor[0].noiseEn)
        ui.lineEdit_noiseEn->setText("是");
    else
        ui.lineEdit_noiseEn->setText("否");

    ui.lineEdit_trackerSensorNoise->setText(QString::number(cmd.trackingInit.trackerSensor[0].trackerSensorNoise));

    if (cmd.trackingInit.trackerSensor[0].realtimeAnnotation)
        ui.lineEdit_realtimeAnnotation->setText("是");
    else
        ui.lineEdit_realtimeAnnotation->setText("否");

    if (m_storageSuppressedForRealtime)
        ui.lineEdit_saveMP4En->setText("是（60 FPS性能模式暂停）");
    else if (m_saveMP4Requested)
        ui.lineEdit_saveMP4En->setText("是");
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
    int count = cmd.platNumValid;
    if (count > 2) count = 2;  // platParam 固定长度 2
    for (int i = 0; i < count; ++i) {
        const BYHWICD::PlatParamPak& pp = cmd.platParam[i];
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
    }
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
        qDebug() << "收到复位命令";
        break;
    }
    case 2: // 开始
    {
        CloseStorage();
        resetVideoPerfStats();

        ui.lineEdit_controlType->setText(QString("运行中-第%1/共%2回合")
                                .arg(cmd.currentRound).arg(cmd.roundCut));

        if (!m_saveMP4Requested || m_storageSuppressedForRealtime) {
            qDebug() << "收到开始命令，本回合不执行同步录制"
                     << "saveMP4En=" << m_saveMP4Requested
                     << "storageSuppressed=" << m_storageSuppressedForRealtime;
            break;
        }

        // 开始命令只进入待录制状态，等第一帧 targetState 有数据后再创建目录和文件。
        m_pendingRound = cmd.currentRound;
        m_frameCount = 0;
        m_recordingPending = true;
        qDebug() << "收到开始命令，等待有效目标数据后开始保存，round=" << m_pendingRound;
        break;
    }
    case 3: // 停止
    {
        ui.lineEdit_controlType->setText("已停止");
        qDebug() << "收到停止命令，共保存" << m_frameCount << "帧";
        CloseStorage();
        break;
    }
    default:
        break;
    }
}
