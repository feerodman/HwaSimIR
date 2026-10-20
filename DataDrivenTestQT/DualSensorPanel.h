#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QMap>
#include <QTimer>
#include <QElapsedTimer>
#include <QDateTime>
#include <QVector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include "ReplaySource.h"
#include "DdsStimClient.h"
class QCheckBox; class QComboBox; class QSpinBox; class QDoubleSpinBox;
class QLineEdit; class QLabel; class QTabWidget; class QPushButton;

// 只借用 MainWindow 持有的进程级 DDS 客户端，不再次 Init/Finalize。
class DualSensorPanel : public QWidget {
    Q_OBJECT
public:
    using Coverage = std::function<bool(const BYHWICD::InitP2cObjectTrackingCmd&,
                                      const QVector<ICD::realtimeInfo>&, const QString&, QString&)>;
    DualSensorPanel(DdsStimClient* client, const QString& network, const QString& input,
                    Coverage coverage, QWidget* parent=nullptr);
    ~DualSensorPanel() override;
    void stopReplayWorker();
    void receiveAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ack);
    bool loadConfig(const QString& path);
    bool saveConfig(const QString& path);
    bool exportDeployment(const QString& directory);
    bool validateConfiguration(QString& error) { return collect(error,true); }
    void setSensorFieldAll(const QString& key, double value);
    void setEnvironmentAll(const QString& key, double value);
    void setSource(const QString& path);
    void setStepMs(double value);
    void setEpoch(const QDateTime& value) { epoch=value; }
    void setStatusBarrier(bool value);
    void setRound(int value, int total=1) { round=value; rounds=total; }
    bool busy() const;
    QJsonObject snapshot() const;
public slots:
    void reset();
    void initialize();
    void start();
    void stop();
signals:
    void allInitialized();
    void roundStopped();
    void operationFailed(const QString& error);
private:
    struct Source { QString path; QVector<ICD::realtimeInfo> rows; int cursor=0; };
    struct SensorChannelState {
        QCheckBox *selected=nullptr, *followSource=nullptr, *followModel=nullptr, *platformFromSource=nullptr, *forceVisible=nullptr;
        QComboBox *role=nullptr, *model=nullptr, *pixel=nullptr;
        QSpinBox *plat=nullptr, *sensor=nullptr;
        QLineEdit* source=nullptr;
        QLabel* status=nullptr;
        QWidget* form=nullptr;
        QMap<QString,QDoubleSpinBox*> values;
        BYHWICD::InitP2cObjectTrackingCmd init={};
        std::shared_ptr<Source> replay;
        QString effectiveRole, topic;
        int target=0x22;
        quint64 sent=0;
        bool enabled=false, pending=false, ready=false, eof=false;
    };
    QVector<SensorChannelState> channels;
    DdsStimClient* dds;
    QString network;
    Coverage coverage;
    QLineEdit* commonSource;
    QComboBox* commonModel;
    QDoubleSpinBox* stepMs;
    QSpinBox* domain;
    QSpinBox* videoDomain;
    QSpinBox* customerDomain;
    QLineEdit* qos;
    QCheckBox* observeStatus;
    QLabel* message;
    QTabWidget* tabs;
    QWidget* settings;
    QTimer tick, poll;
    QElapsedTimer pacing, phaseClock;
    QDateTime epoch;
    quint64 ticks=0;
    int round=1, rounds=1;
    enum Phase { Editable, AwaitingAck, Ready, Starting, Running, Stopping, Stopped, Failed } phase=Editable;
    void fail(const QString& error);
    void freeze(bool value);
    bool collect(QString& error, bool loadSources);
    bool control(int command);
    void pump();
    void beginReplayWorker();
    std::thread replayWorker;
    std::atomic<bool> replayCancel{false}, replayDone{false};
    std::atomic<quint64> replaySent[2];
    QString replayError;
    void pollState();
    void status(int index, const QString& state);
    void copyChannel(int from, int to);
};
