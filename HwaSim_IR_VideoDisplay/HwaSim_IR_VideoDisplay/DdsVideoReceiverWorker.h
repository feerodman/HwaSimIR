#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <atomic>
#include <memory>
#include <mutex>

#include "CommonData.h"

class DdsBytesListener;

struct DdsVideoReceiverConfig
{
	int domainId = 150;
	QString qosFile = QStringLiteral("Config/DDS/ZRDDS_QOS_PROFILES.xml");
	QString topic = QStringLiteral("HwaSimIR.Video.precise.H264");
	QString codec = QStringLiteral("h264");
	int width = 800;
	int height = 800;
	int fps = 60;
	QString dumpFirstFramePath;
	bool autoFromVideoStatus = true;
	QString topicControl = QStringLiteral("HwaSimIR.Control");
	QString topicInit = QStringLiteral("HwaSimIR.Init");
	QString topicRealtime = QStringLiteral("HwaSimIR.Realtime");
	QString topicInitAck = QStringLiteral("HwaSimIR.InitAck");
	QString topicVideoStatus = QStringLiteral("HwaSimIR.VideoStatus");
};

class DdsVideoReceiverWorker : public QObject
{
	Q_OBJECT
public:
	explicit DdsVideoReceiverWorker(const DdsVideoReceiverConfig& config, QObject* parent = nullptr);
	~DdsVideoReceiverWorker();
	quint64 receivedFrameCount() const { return m_receivedSamples.load(); }

public slots:
	void doWork();
	void stop() { m_stop.store(true); }

signals:
	void dataReceived(
		const QImage& img,
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const QString& annotationJson,
		bool hasVideo,
		bool hasRealtimeData,
		bool hasAnnotation,
		int packetVersion,
		quint32 sectionFlags,
		int codecId,
		bool keyFrame,
		quint64 frameSeq,
		quint64 outputOrdinal,
		qint64 ptsMs,
		qint64 receiveTimeNs,
		double decodeMs,
		int decodedChannels,
		const QString& imageFormat);
	void fatalError(const QString& reason);
	void initCommandReceived(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
	void controlCmdReceived(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	void videoStatusChanged(const QString& topic, const QString& codec,
		const QString& pixelFormat, int width, int height, int fps, bool running,
		int currentRound);

private:
	friend class DdsBytesListener;
	friend class DdsVideoStatusListener;
	friend class DdsDisplayControlListener;
	friend class DdsDisplayInitListener;
	friend class DdsDisplayRealtimeListener;
	void processSample(const char* data, int size);
	void processVideoStatus(const QString& topic, const QString& codec,
		const QString& pixelFormat, int width, int height, int fps,
		bool running, int currentRound);
	void processRealtime(const BYHWICD::DisplayC2cObjTrackingData& data);
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	DdsVideoReceiverConfig m_config;
	std::atomic<bool> m_stop{ false };
	std::atomic<quint64> m_receivedSamples{ 0 };
	std::atomic<quint64> m_receivedBytes{ 0 };
	std::atomic<quint64> m_ddsErrors{ 0 };
	bool m_dumpAttempted = false;
};
