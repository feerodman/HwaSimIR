#include "AsyncVideoRecorder.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>
#include <chrono>

namespace
{
qint64 steadyTimeNs()
{
    return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

QString targetTypeName(int type)
{
    switch (type) {
    case 0x11: return QStringLiteral("飞机");
    case 0x22: return QStringLiteral("雷达导弹");
    case 0x33: return QStringLiteral("红外弹");
    case 0x44: return QStringLiteral("MMD");
    default: return QStringLiteral("未知");
    }
}

QString targetStateName(int state)
{
    switch (state) {
    case 0x01: return QStringLiteral("打击态");
    case 0x02: return QStringLiteral("爆炸态");
    case 0x03: return QStringLiteral("击毁态");
    default: return QStringLiteral("正常");
    }
}
}

AsyncVideoRecorder::AsyncVideoRecorder(int maxQueueFrames)
    : m_maxQueueFrames(std::max(1, maxQueueFrames))
{
    m_thread = std::thread(&AsyncVideoRecorder::threadMain, this);
}

AsyncVideoRecorder::~AsyncVideoRecorder()
{
    shutdown(10000);
}

void AsyncVideoRecorder::configure(bool enabled, int videoFps, int maxQueueFrames)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_enabled = enabled;
    m_videoFps = std::max(1, videoFps);
    m_maxQueueFrames = std::max(1, maxQueueFrames);
}

bool AsyncVideoRecorder::startPending(int round, const QString& baseDirectory)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || m_shutdownRequested || m_accepting || m_pending ||
        m_initialized || m_flushRequested || m_workerBusy || !m_queue.empty())
    {
        return false;
    }

    m_round = round;
    m_baseDirectory = baseDirectory;
    m_roundDirectory.clear();
    m_outputPath.clear();
    m_accepting = true;
    m_pending = true;
    m_initialized = false;
    m_inputFrames = 0;
    m_writtenFrames = 0;
    m_droppedFrames = 0;
    m_lastWrittenSourceSeq = 0;
    m_sourceSeqContinuousWritten = true;
    m_maxQueueDepthObserved = 0;
    m_writeMsTotal = 0.0;
    m_writeMsMax = 0.0;
    m_perfStartNs = steadyTimeNs();
    m_perfInputFrames = 0;
    m_perfWrittenFrames = 0;
    m_perfWriteMsTotal = 0.0;
    m_perfWriteMsMax = 0.0;
    return true;
}

bool AsyncVideoRecorder::enqueue(const RecordingFrame& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_enabled || !m_accepting || m_shutdownRequested)
    {
        return false;
    }
    if (m_pending && !m_initialized && !hasTargetStateData(frame.trackingData))
    {
        return false;
    }

    ++m_inputFrames;
    ++m_perfInputFrames;
    if (static_cast<int>(m_queue.size()) >= m_maxQueueFrames)
    {
        ++m_droppedFrames;
        return false;
    }

    m_queue.push_back(frame);
    m_maxQueueDepthObserved = std::max(
        m_maxQueueDepthObserved,
        static_cast<int>(m_queue.size()));
    m_wakeCondition.notify_one();
    return true;
}

bool AsyncVideoRecorder::stopAndFlush(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!m_accepting && !m_pending && !m_initialized &&
        !m_flushRequested && !m_workerBusy && m_queue.empty())
    {
        return true;
    }

    m_accepting = false;
    m_flushRequested = true;
    m_wakeCondition.notify_one();
    return m_flushCondition.wait_for(
        lock,
        std::chrono::milliseconds(std::max(1, timeoutMs)),
        [this]() {
            return !m_flushRequested && !m_pending && !m_initialized &&
                !m_workerBusy && m_queue.empty();
        });
}

void AsyncVideoRecorder::shutdown(int timeoutMs)
{
    const bool flushed = stopAndFlush(timeoutMs);
    if (!flushed)
    {
        qWarning().noquote()
            << QStringLiteral("[RecorderPerf][WARN] flushTimeoutMs=%1 during=shutdown")
                .arg(timeoutMs);
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_shutdownRequested)
        {
            return;
        }
        m_shutdownRequested = true;
        m_accepting = false;
        m_wakeCondition.notify_one();
    }
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

RecorderSnapshot AsyncVideoRecorder::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    RecorderSnapshot result;
    result.recordingEnabled = m_enabled && (m_accepting || m_pending || m_initialized);
    result.pending = m_pending;
    result.initialized = m_initialized;
    result.sourceSeqContinuousWritten = m_sourceSeqContinuousWritten;
    result.inputFrames = m_inputFrames;
    result.writtenFrames = m_writtenFrames;
    result.droppedFrames = m_droppedFrames;
    result.sourceSeqWritten = m_lastWrittenSourceSeq;
    result.queueDepth = static_cast<int>(m_queue.size());
    result.maxQueueDepth = m_maxQueueDepthObserved;
    result.writeMsAvg = m_writtenFrames > 0
        ? m_writeMsTotal / static_cast<double>(m_writtenFrames)
        : 0.0;
    result.writeMsMax = m_writeMsMax;
    result.roundDirectory = m_roundDirectory;
    result.outputPath = m_outputPath;
    return result;
}

void AsyncVideoRecorder::threadMain()
{
    for (;;)
    {
        RecordingFrame frame;
        bool haveFrame = false;
        bool shouldClose = false;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wakeCondition.wait(lock, [this]() {
                return m_shutdownRequested || m_flushRequested || !m_queue.empty();
            });

            if (!m_queue.empty())
            {
                frame = m_queue.front();
                m_queue.pop_front();
                m_workerBusy = true;
                haveFrame = true;
            }
            else if (m_flushRequested)
            {
                m_workerBusy = true;
                shouldClose = true;
            }
            else if (m_shutdownRequested)
            {
                break;
            }
        }

        if (haveFrame)
        {
            bool canWrite = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                canWrite = m_initialized;
            }
            if (!canWrite)
            {
                canWrite = initializeSession(frame);
            }
            if (canWrite)
            {
                writeFrame(frame);
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_workerBusy = false;
                shouldClose = m_flushRequested && m_queue.empty();
            }
        }

        if (shouldClose)
        {
            logPerf(true);
            closeSession();
            std::lock_guard<std::mutex> lock(m_mutex);
            m_pending = false;
            m_initialized = false;
            m_flushRequested = false;
            m_workerBusy = false;
            m_flushCondition.notify_all();
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_shutdownRequested && m_queue.empty() && !m_flushRequested)
            {
                break;
            }
        }
    }
    closeSession();
}

bool AsyncVideoRecorder::initializeSession(const RecordingFrame& firstFrame)
{
    int round = 0;
    int videoFps = 25;
    QString baseDirectory;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_pending || !m_enabled)
        {
            return false;
        }
        round = m_round;
        videoFps = m_videoFps;
        baseDirectory = m_baseDirectory;
    }

    if (!QDir().mkpath(baseDirectory))
    {
        qWarning() << "[RecorderPerf][WARN] createBaseDirectoryFailed" << baseDirectory;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        m_pending = false;
        return false;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString roundDirectory = QStringLiteral("%1/round_%2_%3")
        .arg(baseDirectory)
        .arg(round, 3, 10, QChar('0'))
        .arg(timestamp);
    if (!QDir().mkpath(roundDirectory))
    {
        qWarning() << "[RecorderPerf][WARN] createRoundDirectoryFailed" << roundDirectory;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        m_pending = false;
        return false;
    }

    std::unique_ptr<QFile> annotationFile(new QFile(roundDirectory + QStringLiteral("/annotations.txt")));
    if (!annotationFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[RecorderPerf][WARN] openAnnotationsFailed" << annotationFile->fileName();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        m_pending = false;
        return false;
    }
    std::unique_ptr<QTextStream> annotationStream(new QTextStream(annotationFile.get()));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    annotationStream->setCodec("UTF-8");
#endif

    std::unique_ptr<QFile> targetAnnotationFile(new QFile(roundDirectory + QStringLiteral("/target_annotations.txt")));
    if (!targetAnnotationFile->open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qWarning() << "[RecorderPerf][WARN] openTargetAnnotationsFailed" << targetAnnotationFile->fileName();
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        m_pending = false;
        return false;
    }
    std::unique_ptr<QTextStream> targetAnnotationStream(new QTextStream(targetAnnotationFile.get()));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    targetAnnotationStream->setCodec("UTF-8");
#endif

    const QString outputPath = roundDirectory + QStringLiteral("/output.mp4");
    const cv::Size frameSize(firstFrame.image.width(), firstFrame.image.height());
    struct CodecTry { int fourcc; const char* name; };
    const CodecTry codecs[] = {
        { cv::VideoWriter::fourcc('H', '2', '6', '4'), "H264" },
        { cv::VideoWriter::fourcc('X', 'V', 'I', 'D'), "XVID" },
        { cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "MJPG" }
    };

    std::unique_ptr<cv::VideoWriter> writer;
    const char* selectedCodec = "none";
    for (const CodecTry& codec : codecs)
    {
        writer.reset(new cv::VideoWriter(
            outputPath.toStdString(),
            codec.fourcc,
            static_cast<double>(videoFps),
            frameSize,
            true));
        if (writer->isOpened())
        {
            selectedCodec = codec.name;
            break;
        }
        writer.reset();
    }
    if (!writer)
    {
        qWarning() << "[RecorderPerf][WARN] videoWriterOpenFailed" << outputPath;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_accepting = false;
        m_pending = false;
        return false;
    }

    m_videoWriter = std::move(writer);
    m_annotationFile = std::move(annotationFile);
    m_annotationStream = std::move(annotationStream);
    m_targetAnnotationFile = std::move(targetAnnotationFile);
    m_targetAnnotationStream = std::move(targetAnnotationStream);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_roundDirectory = roundDirectory;
        m_outputPath = outputPath;
        m_pending = false;
        m_initialized = true;
    }
    qInfo().noquote()
        << QStringLiteral("[Recorder] started round=%1 fps=%2 size=%3x%4 codec=%5 path=%6")
            .arg(round)
            .arg(videoFps)
            .arg(frameSize.width)
            .arg(frameSize.height)
            .arg(QString::fromLatin1(selectedCodec))
            .arg(QDir::toNativeSeparators(outputPath));
    return true;
}

bool AsyncVideoRecorder::writeFrame(const RecordingFrame& frame)
{
    if (!m_videoWriter || !m_videoWriter->isOpened())
    {
        return false;
    }

    QElapsedTimer writeTimer;
    writeTimer.start();
    const QImage rgbImage = frame.image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgb(
        rgbImage.height(),
        rgbImage.width(),
        CV_8UC3,
        const_cast<uchar*>(rgbImage.constBits()),
        static_cast<size_t>(rgbImage.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    m_videoWriter->write(bgr);
    const double writeMs = static_cast<double>(writeTimer.nsecsElapsed()) / 1.0e6;

    quint64 recordingFrameIndex = 0;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_writtenFrames;
        ++m_perfWrittenFrames;
        recordingFrameIndex = m_writtenFrames;
        m_writeMsTotal += writeMs;
        m_writeMsMax = std::max(m_writeMsMax, writeMs);
        m_perfWriteMsTotal += writeMs;
        m_perfWriteMsMax = std::max(m_perfWriteMsMax, writeMs);
        if (frame.sourceSeq > 0)
        {
            if (m_lastWrittenSourceSeq > 0 && frame.sourceSeq != m_lastWrittenSourceSeq + 1)
            {
                m_sourceSeqContinuousWritten = false;
            }
            m_lastWrittenSourceSeq = frame.sourceSeq;
        }
    }

    if (m_annotationStream)
    {
        *m_annotationStream << trackingJsonLine(recordingFrameIndex, frame) << "\n";
    }
    if (m_targetAnnotationStream)
    {
        *m_targetAnnotationStream << targetAnnotationJsonLine(recordingFrameIndex, frame) << "\n";
    }
    logPerf(false);
    return true;
}

void AsyncVideoRecorder::closeSession()
{
    if (m_videoWriter)
    {
        if (m_videoWriter->isOpened())
        {
            m_videoWriter->release();
        }
        m_videoWriter.reset();
    }
    if (m_annotationStream)
    {
        m_annotationStream->flush();
        m_annotationStream.reset();
    }
    if (m_annotationFile)
    {
        m_annotationFile->close();
        m_annotationFile.reset();
    }
    if (m_targetAnnotationStream)
    {
        m_targetAnnotationStream->flush();
        m_targetAnnotationStream.reset();
    }
    if (m_targetAnnotationFile)
    {
        m_targetAnnotationFile->close();
        m_targetAnnotationFile.reset();
    }
}

void AsyncVideoRecorder::logPerf(bool finalLog)
{
    const qint64 nowNs = steadyTimeNs();
    bool recordingEnabled = false;
    double inputFps = 0.0;
    double writtenFps = 0.0;
    int queueDepth = 0;
    int maxQueueDepth = 0;
    double writeMsAvg = 0.0;
    double writeMsMax = 0.0;
    quint64 droppedFrames = 0;
    quint64 sourceSeqWritten = 0;
    bool sourceSeqContinuous = true;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const qint64 elapsedNs = nowNs - m_perfStartNs;
        if (!finalLog && elapsedNs < 2000000000LL)
        {
            return;
        }
        const double elapsedSec = std::max(0.001, static_cast<double>(elapsedNs) / 1.0e9);
        recordingEnabled = m_enabled && (m_accepting || m_pending || m_initialized);
        inputFps = static_cast<double>(m_perfInputFrames) / elapsedSec;
        writtenFps = static_cast<double>(m_perfWrittenFrames) / elapsedSec;
        queueDepth = static_cast<int>(m_queue.size());
        maxQueueDepth = m_maxQueueDepthObserved;
        writeMsAvg = m_perfWrittenFrames > 0
            ? m_perfWriteMsTotal / static_cast<double>(m_perfWrittenFrames)
            : 0.0;
        writeMsMax = m_perfWriteMsMax;
        droppedFrames = m_droppedFrames;
        sourceSeqWritten = m_lastWrittenSourceSeq;
        sourceSeqContinuous = m_sourceSeqContinuousWritten;
        m_perfStartNs = nowNs;
        m_perfInputFrames = 0;
        m_perfWrittenFrames = 0;
        m_perfWriteMsTotal = 0.0;
        m_perfWriteMsMax = 0.0;
    }

    qInfo().noquote()
        << QStringLiteral("[RecorderPerf] recordingEnabled=%1 inputFps=%2 writtenFps=%3 queueDepth=%4 maxQueueDepth=%5 writeMsAvg=%6 writeMsMax=%7 droppedFrames=%8 sourceSeqWritten=%9 sourceSeqContinuousWritten=%10")
            .arg(recordingEnabled ? 1 : 0)
            .arg(inputFps, 0, 'f', 3)
            .arg(writtenFps, 0, 'f', 3)
            .arg(queueDepth)
            .arg(maxQueueDepth)
            .arg(writeMsAvg, 0, 'f', 3)
            .arg(writeMsMax, 0, 'f', 3)
            .arg(droppedFrames)
            .arg(sourceSeqWritten)
            .arg(sourceSeqContinuous ? 1 : 0);
}

bool AsyncVideoRecorder::hasTargetStateData(const BYHWICD::DisplayC2cObjTrackingData& data)
{
    for (int i = 0; i < 5; ++i)
    {
        if (data.targetState[i].targetType != 0)
        {
            return true;
        }
    }
    return false;
}

QString AsyncVideoRecorder::trackingJsonLine(
    quint64 recordingFrameIndex,
    const RecordingFrame& frame)
{
    QJsonObject root;
    root.insert(QStringLiteral("recordingFrameIndex"), static_cast<double>(recordingFrameIndex));
    root.insert(QStringLiteral("frameIndex"), static_cast<double>(frame.sourceSeq));
    root.insert(QStringLiteral("sourceSeq"), static_cast<double>(frame.sourceSeq));
    root.insert(QStringLiteral("timeMs"), frame.trackingData.time);
    root.insert(QStringLiteral("platID"), frame.trackingData.platID);
    root.insert(QStringLiteral("sensorID"), frame.trackingData.sensorID);
    root.insert(QStringLiteral("receiveTimeNs"), QString::number(frame.receiveTimeNs));
    root.insert(QStringLiteral("displayTimeNs"), QString::number(frame.displayTimeNs));

    QJsonArray targets;
    for (int i = 0; i < 5; ++i)
    {
        const BYHWICD::TargetState& target = frame.trackingData.targetState[i];
        if (target.targetType == 0)
        {
            break;
        }
        QJsonObject item;
        item.insert(QStringLiteral("targetIndex"), i);
        item.insert(QStringLiteral("targetType"), target.targetType);
        item.insert(QStringLiteral("targetTypeName"), targetTypeName(target.targetType));
        item.insert(QStringLiteral("targetPlatID"), target.targetPlatID);
        item.insert(QStringLiteral("targetID"), target.targetID);
        item.insert(QStringLiteral("lat"), target.targetLoc.lat);
        item.insert(QStringLiteral("lon"), target.targetLoc.lon);
        item.insert(QStringLiteral("alt"), target.targetLoc.alt);
        item.insert(QStringLiteral("yaw"), target.targetLoc.yaw);
        item.insert(QStringLiteral("pitch"), target.targetLoc.pitch);
        item.insert(QStringLiteral("roll"), target.targetLoc.roll);
        item.insert(QStringLiteral("viewValid"), target.viewValid);
        item.insert(QStringLiteral("targetState"), target.targetState);
        item.insert(QStringLiteral("targetStateName"), targetStateName(target.targetState));
        targets.append(item);
    }
    root.insert(QStringLiteral("targets"), targets);
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QString AsyncVideoRecorder::targetAnnotationJsonLine(
    quint64 recordingFrameIndex,
    const RecordingFrame& frame)
{
    QJsonObject root;
    if (!frame.annotationJson.trimmed().isEmpty())
    {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(
            frame.annotationJson.toUtf8(),
            &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject())
        {
            root = document.object();
        }
    }
    root.insert(QStringLiteral("recordingFrameIndex"), static_cast<double>(recordingFrameIndex));
    root.insert(QStringLiteral("frameIndex"), static_cast<double>(frame.sourceSeq));
    root.insert(QStringLiteral("sourceSeq"), static_cast<double>(frame.sourceSeq));
    if (!root.contains(QStringLiteral("simTimeMs")))
    {
        root.insert(QStringLiteral("simTimeMs"), frame.trackingData.time);
    }
    if (!root.contains(QStringLiteral("sensorID")))
    {
        root.insert(QStringLiteral("sensorID"), frame.trackingData.sensorID);
    }
    if (!root.contains(QStringLiteral("width")))
    {
        root.insert(QStringLiteral("width"), frame.image.width());
    }
    if (!root.contains(QStringLiteral("height")))
    {
        root.insert(QStringLiteral("height"), frame.image.height());
    }
    if (!root.contains(QStringLiteral("targets")))
    {
        root.insert(QStringLiteral("targets"), QJsonArray());
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
