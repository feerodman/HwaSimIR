#pragma once

#include <QtWidgets/QWidget>
#include <QThread>
#include <QTableWidget>
#include <QVector>
#include "AsyncVideoRecorder.h"
#include "TcpServerWorker.h"
#include "ui_HwaSim_IR_VideoDisplay.h"

class HwaSim_IR_VideoDisplay : public QWidget
{
    Q_OBJECT

public:
    HwaSim_IR_VideoDisplay(QWidget *parent = nullptr);
    ~HwaSim_IR_VideoDisplay();

    void InitQss();
    void InitTables();
    void CloseStorage();

public slots:
    void imageReceivedSlot(
        const QImage& img,
        const BYHWICD::DisplayC2cObjTrackingData& data,
        const QString& annotationJson,
        qint64 receiveTimeNs,
        double jpegDecodeMs,
        int decodedChannels,
        const QString& imageFormat);
    void initCommandReceivedSlot(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
    void controlCmdReceivedSlot(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);

private:
    void updatePlatDataTable(int platID, const BYHWICD::SpatialState& platLoc);
    void updateTargetDataTable(const BYHWICD::DisplayC2cObjTrackingData& data);
    void centerVideoLabel();
    void resizeEvent(QResizeEvent* event) override;
    QString targetTypeName(int type);
    QString targetStateName(int state);
    void resetVideoPerfStats();
    bool flushRecorder(const char* reason);

    QThread* m_workerThread = nullptr;
    TcpServerWorker* m_worker = nullptr;
    AsyncVideoRecorder* m_recorder = nullptr;
    bool m_saveMP4Requested = false;
    int m_maxImageWidth = 0;
    int m_maxImageHeight = 0;
    int m_videoFps = 25;
    int m_uiUpdateEveryFrames = 5;
    int m_maxRecordingQueueFrames = 180;
    int m_recorderFlushTimeoutMs = 10000;
    quint64 m_videoPerfFrames = 0;
    quint64 m_videoPerfIntervalFrames = 0;
    quint64 m_lastFrameSeq = 0;
    quint64 m_frameSeqDiscontinuities = 0;
    quint64 m_receivedFrameBaseline = 0;
    quint64 m_lastReceivedFrameCount = 0;
    bool m_intervalSourceSeqContinuous = true;
    qint64 m_videoPerfReceiveStartNs = 0;
    qint64 m_videoPerfDisplayStartNs = 0;
    qint64 m_lastVideoPerfLogNs = 0;
    double m_decodeMsTotal = 0.0;
    double m_displayMsTotal = 0.0;
    double m_recordingEnqueueMsTotal = 0.0;
    double m_recordingEnqueueMsMax = 0.0;
    double m_latencyMsTotal = 0.0;
    double m_latencyMsMax = 0.0;
    quint64 m_latencySamples = 0;
    QVector<double> m_latencyIntervalSamples;
    int m_decodedChannels = 0;
    QString m_imageFormat = QStringLiteral("unknown");
    bool m_h264Requested = false;
    QString m_requestedCodec = QStringLiteral("jpeg");
    QString m_activeCodec = QStringLiteral("jpeg");
    QString m_codecFallbackReason = QStringLiteral("none");

private:
    Ui::HwaSim_IR_VideoDisplayClass ui;
};
