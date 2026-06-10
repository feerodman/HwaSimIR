#pragma once

#include <QtWidgets/QWidget>
#include <QThread>
#include <QTableWidget>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDateTime>
#include <opencv2/opencv.hpp>
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
    void imageReceivedSlot(const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& data, const QString& annotationJson);
    void initCommandReceivedSlot(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
    void controlCmdReceivedSlot(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);

private:
    void updatePlatDataTable(int platID, const BYHWICD::SpatialState& platLoc);
    void updateTargetDataTable(const BYHWICD::DisplayC2cObjTrackingData& data);
    void centerVideoLabel();
    virtual void resizeEvent(QResizeEvent* event) override;
    QString targetTypeName(int type);
    QString targetStateName(int state);
    bool hasTargetStateData(const BYHWICD::DisplayC2cObjTrackingData& data) const;
    bool beginPendingStorage();
    void saveFrameToStorage(const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& data, const QString& annotationJson);
    QString fallbackAnnotationJson(int frameIndex, const QImage& img, const BYHWICD::DisplayC2cObjTrackingData& data) const;

    QThread* m_workerThread;
    TcpServerWorker* m_worker;
    // 存储状态
    bool m_isRecording = false;
    bool m_recordingPending = false;
    bool m_saveMP4Requested = false;
    int m_maxImageWidth = 0;
    int m_maxImageHeight = 0;
    QString m_currentRoundDir;
    cv::VideoWriter* m_videoWriter = nullptr;
    QFile* m_annotFile = nullptr;
    QTextStream* m_annotStream = nullptr;
    QFile* m_targetAnnotFile = nullptr;
    QTextStream* m_targetAnnotStream = nullptr;
    int m_frameCount = 0;
    int m_videoFps = 25;
    int m_pendingRound = 0;

private:
    Ui::HwaSim_IR_VideoDisplayClass ui;
};
