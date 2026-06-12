#pragma once

#include <QFile>
#include <QImage>
#include <QString>
#include <QTextStream>
#include <QtGlobal>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <opencv2/opencv.hpp>
#include "CommonData.h"

struct RecordingFrame
{
    quint64 sourceSeq = 0;
    QImage image;
    BYHWICD::DisplayC2cObjTrackingData trackingData = {};
    QString annotationJson;
    qint64 receiveTimeNs = 0;
    qint64 displayTimeNs = 0;
};

struct RecorderSnapshot
{
    bool recordingEnabled = false;
    bool pending = false;
    bool initialized = false;
    bool sourceSeqContinuousWritten = true;
    quint64 inputFrames = 0;
    quint64 writtenFrames = 0;
    quint64 droppedFrames = 0;
    quint64 sourceSeqWritten = 0;
    int queueDepth = 0;
    int maxQueueDepth = 0;
    double writeMsAvg = 0.0;
    double writeMsMax = 0.0;
    QString roundDirectory;
    QString outputPath;
};

class AsyncVideoRecorder
{
public:
    explicit AsyncVideoRecorder(int maxQueueFrames = 180);
    ~AsyncVideoRecorder();

    void configure(bool enabled, int videoFps, int maxQueueFrames = 180);
    bool startPending(int round, const QString& baseDirectory);
    bool enqueue(const RecordingFrame& frame);
    bool stopAndFlush(int timeoutMs);
    void shutdown(int timeoutMs);
    RecorderSnapshot snapshot() const;

private:
    void threadMain();
    bool initializeSession(const RecordingFrame& firstFrame);
    bool writeFrame(const RecordingFrame& frame);
    void closeSession();
    void logPerf(bool finalLog);
    static bool hasTargetStateData(const BYHWICD::DisplayC2cObjTrackingData& data);
    static QString trackingJsonLine(quint64 recordingFrameIndex, const RecordingFrame& frame);
    static QString targetAnnotationJsonLine(quint64 recordingFrameIndex, const RecordingFrame& frame);

    mutable std::mutex m_mutex;
    std::condition_variable m_wakeCondition;
    std::condition_variable m_flushCondition;
    std::deque<RecordingFrame> m_queue;
    std::thread m_thread;

    bool m_shutdownRequested = false;
    bool m_enabled = false;
    bool m_accepting = false;
    bool m_pending = false;
    bool m_initialized = false;
    bool m_flushRequested = false;
    bool m_workerBusy = false;
    int m_videoFps = 25;
    int m_maxQueueFrames = 180;
    int m_round = 0;
    QString m_baseDirectory;
    QString m_roundDirectory;
    QString m_outputPath;

    std::unique_ptr<cv::VideoWriter> m_videoWriter;
    std::unique_ptr<QFile> m_annotationFile;
    std::unique_ptr<QTextStream> m_annotationStream;
    std::unique_ptr<QFile> m_targetAnnotationFile;
    std::unique_ptr<QTextStream> m_targetAnnotationStream;

    quint64 m_inputFrames = 0;
    quint64 m_writtenFrames = 0;
    quint64 m_droppedFrames = 0;
    quint64 m_lastWrittenSourceSeq = 0;
    bool m_sourceSeqContinuousWritten = true;
    int m_maxQueueDepthObserved = 0;
    double m_writeMsTotal = 0.0;
    double m_writeMsMax = 0.0;

    qint64 m_perfStartNs = 0;
    quint64 m_perfInputFrames = 0;
    quint64 m_perfWrittenFrames = 0;
    double m_perfWriteMsTotal = 0.0;
    double m_perfWriteMsMax = 0.0;
};
