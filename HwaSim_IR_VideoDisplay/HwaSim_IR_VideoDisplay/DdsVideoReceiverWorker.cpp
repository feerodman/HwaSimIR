#include "DdsVideoReceiverWorker.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QtGlobal>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <map>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include "../../DDS/Protocol/FrameProductV2.h"
#include "../../DDS/Protocol/InputAuditV1.h"
#include <vector>

#include "Video/VideoDecoder.h"
#include "CommonDataDdsAdapter.h"
#include "DdsRuntimeManager.h"
#include "../../DDS/Runtime/RuntimeTelemetryV2.h"
#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1TypeSupport.h"

#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
using namespace DDS;
#endif

namespace
{
qint64 WallTimeNs()
{
	return static_cast<qint64>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count());
}
}

struct DdsVideoReceiverWorker::Impl
{
	std::unique_ptr<H264FfmpegDecoder> h264Decoder;
	std::shared_ptr<DdsRuntimeManager> runtime;
    std::mutex telemetryMutex;
    HwaTelemetryV2::Message boardMetrics;
    HwaTelemetryV2::Message controlResponse;
    HwaTelemetryV2::ClockEstimate clockEstimate;
    std::int64_t metricsArrivalNs=0,lastClockRequestNs=0;
    std::uint64_t requestSequence=0;
    std::map<std::uint64_t,std::int64_t> clockRequests;
    std::string clockClient=HwaFrameV2::newSession();
    std::string requestTopic;
    std::deque<std::int64_t> decodedTimes;
    double outputLatencyMs=-1,clockErrorMs=0;
	std::mutex statusMutex;
	bool statusPending = false;
	QString pendingTopic;
	QString pendingCodec;
	QString pendingPixelFormat;
	int pendingWidth = 0;
	int pendingHeight = 0;
	int pendingFps = 0;
	bool pendingRunning = false;
	int pendingRound = 0;
	int pendingPlatID = -1;
	int pendingSensorID = -1;
	std::mutex syncMutex;
	int syncRound = 0;
	quint64 syncVideo = 0;
	quint64 syncMeta = 0;
	quint64 syncAnnotation = 0;
	quint64 syncMismatch = 0;
	quint32 lastMetaSeq = 0;
	quint32 lastAnnotationSeq = 0;
	struct ReceivedProduct {HwaFrameV2::Product product;QByteArray au;qint64 wallNs=0,steadyNs=0;};
    std::map<qint64, ReceivedProduct> decodeProducts;
    std::string decodeSession;std::uint64_t decodeGeneration=0,decodeRun=0;
    std::deque<std::string> retiredSessions;
    std::uint64_t lastProductSeq=0;
    quint64 diagnosticDropSeq=0;
    int diagnosticAnnotationDelayMs=0;
	qint64 lastMetaPtsMs=-1, lastAnnotationPtsMs=-1;
	QString lastAnnotationBodyHash;
	quint64 identifiedVideos=0, unmatchedVideos=0;
#if defined(HWASIMIR_HAS_ZRDDS)
	DataReader* reader = nullptr;
    DataReader* telemetryReader=nullptr;
    DataWriter* clockWriter=nullptr;
	DataReader* statusReader = nullptr;
	DataReader* controlReader = nullptr;
	DataReader* initReader = nullptr;
	DataReader* realtimeReader = nullptr;
	DataReader* metaReader = nullptr;
	DataReader* annotationReader = nullptr;
	std::vector<DataReader*> retiredVideoReaders;
	std::vector<DataReader*> retiredMetaReaders;
	std::vector<DataReader*> retiredAnnotationReaders;
#endif
};

#if defined(HWASIMIR_HAS_ZRDDS)
class DdsTelemetryListener : public SimpleDataReaderListener<Bytes,BytesSeq,BytesDataReader>
{
    DdsVideoReceiverWorker* owner;
public:
    explicit DdsTelemetryListener(DdsVideoReceiverWorker* p):owner(p){}
    void on_process_sample(DataReader*,const Bytes& data,const SampleInfo&) override {
        const auto t4=HwaTelemetryV2::nowNs();HwaTelemetryV2::Message m;
        if(!HwaTelemetryV2::decode(reinterpret_cast<const char*>(data.value.get_contiguous_buffer()),data.value.length(),m))return;
        std::lock_guard<std::mutex> lock(owner->m_impl->telemetryMutex);
        auto& state=*owner->m_impl;
        if(m.kind==3){
            if(state.boardMetrics.server!=m.server||state.boardMetrics.generation!=m.generation)state.clockEstimate.reset();
            if(state.controlResponse.server!=m.server)state.controlResponse=HwaTelemetryV2::Message();
            state.boardMetrics=m;state.metricsArrivalNs=t4;
        }else if(m.kind==4){
            if(!state.boardMetrics.server.empty() && state.boardMetrics.server!=m.server)return;
            if(m.controlCommand<1||m.controlCommand>3||!m.controlSequence)return;
            if(state.controlResponse.server==m.server && state.controlResponse.controlSequence>=m.controlSequence)return;
            state.controlResponse=m;
            qInfo().noquote()<<QString("[ControlResponseV3] session=%1 sequence=%2 command=%3 round=%4 receiveNs=%5 executeNs=%6 responseMs=%7 equivalentHz=%8 valid=%9")
                .arg(QString::fromStdString(m.server)).arg(m.controlSequence).arg(m.controlCommand).arg(m.controlRound)
                .arg(m.controlReceiveNs).arg(m.controlExecuteNs).arg(HwaTelemetryV2::controlResponseMs(m),0,'f',6)
                .arg(HwaTelemetryV2::controlEquivalentHz(m),0,'f',6).arg(HwaTelemetryV2::controlTimeValid(m));
            const auto csvPath=qgetenv("P9ControlCsv");
            if(!csvPath.isEmpty()){
                QFile csv(QString::fromLocal8Bit(csvPath));const bool empty=!csv.exists()||csv.size()==0;
                if(csv.open(QIODevice::WriteOnly|QIODevice::Append)){
                    if(empty)csv.write("session,sequence,command,round,receiveNs,executeNs,responseMs,equivalentHz,valid,resolutionFloorNs,receiverArrivalNs\n");
                    csv.write(QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11\n")
                        .arg(QString::fromStdString(m.server)).arg(m.controlSequence).arg(m.controlCommand).arg(m.controlRound)
                        .arg(m.controlReceiveNs).arg(m.controlExecuteNs).arg(HwaTelemetryV2::controlResponseMs(m),0,'f',6)
                        .arg(HwaTelemetryV2::controlEquivalentHz(m),0,'f',6).arg(HwaTelemetryV2::controlTimeValid(m))
                        .arg(m.controlResolutionNs).arg(t4).toUtf8());
                }
            }
        }else if(m.kind==2&&m.client==state.clockClient){
            const auto request=state.clockRequests.find(m.sequence);
            if(request==state.clockRequests.end()||request->second!=m.t1)return;
            state.clockRequests.erase(request);
            const bool valid=state.clockEstimate.observe(m,t4);
            qInfo().noquote()<<QString("[ClockEstimateV2] seq=%1 t1=%2 t2=%3 t3=%4 t4=%5 valid=%6 offsetNs=%7 rttMs=%8 uncertaintyMs=%9 definition=server_minus_receiver")
                .arg(m.sequence).arg(m.t1).arg(m.t2).arg(m.t3).arg(t4).arg(valid)
                .arg(state.clockEstimate.offsetNs,0,'f',0).arg(state.clockEstimate.rttMs,0,'f',3).arg(state.clockEstimate.uncertaintyMs,0,'f',3);
        }
    }
};

class DdsBytesListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
	explicit DdsBytesListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader* reader, const Bytes& sample, const SampleInfo&) override
	{
		if (reader != m_owner->m_impl->reader) return;
		const char* data = reinterpret_cast<const char*>(sample.value.get_contiguous_buffer());
		m_owner->processSample(data, static_cast<int>(sample.value.length()));
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsVideoStatusListener : public SimpleDataReaderListener<
	HwaSimIRDds::VideoStatusV1, HwaSimIRDds::VideoStatusV1Seq,
	HwaSimIRDds::VideoStatusV1DataReader>
{
public:
	explicit DdsVideoStatusListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader*, const HwaSimIRDds::VideoStatusV1& sample,
		const SampleInfo&) override
	{
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		if (m_owner->m_config.sensorID >= 0 && sample.sensorID != m_owner->m_config.sensorID) return;
		if (m_owner->m_config.platID < 0 && m_owner->m_config.sensorID < 0 &&
			QString::fromLatin1(sample.channel) != m_owner->m_config.channel) return;
		m_owner->processVideoStatus(sample.platID, sample.sensorID,
			QString::fromLatin1(sample.videoTopic),
			QString::fromLatin1(sample.codec), QString::fromLatin1(sample.pixelFormat),
			sample.width, sample.height, sample.fps, sample.running, sample.currentRound);
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsVideoMetaListener : public SimpleDataReaderListener<
	HwaSimIRDds::VideoFrameMetaV1, HwaSimIRDds::VideoFrameMetaV1Seq,
	HwaSimIRDds::VideoFrameMetaV1DataReader>
{
public:
	explicit DdsVideoMetaListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader* reader, const HwaSimIRDds::VideoFrameMetaV1& sample,
		const SampleInfo&) override
	{
		if (reader != m_owner->m_impl->metaReader) return;
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		if (m_owner->m_config.sensorID >= 0 && sample.sensorID != m_owner->m_config.sensorID) return;
		m_owner->processVideoMeta(sample.currentRound, sample.frameSeq, sample.ptsMs);
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsAnnotationListener : public SimpleDataReaderListener<
	HwaSimIRDds::AnnotationFrameV1, HwaSimIRDds::AnnotationFrameV1Seq,
	HwaSimIRDds::AnnotationFrameV1DataReader>
{
public:
	explicit DdsAnnotationListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader* reader, const HwaSimIRDds::AnnotationFrameV1& sample,
		const SampleInfo&) override
	{
		if (reader != m_owner->m_impl->annotationReader) return;
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		if (m_owner->m_config.sensorID >= 0 && sample.sensorID != m_owner->m_config.sensorID) return;
		m_owner->processAnnotation(sample.currentRound, sample.frameSeq, sample.ptsMs,
			QString::fromUtf8(sample.json));
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsDisplayControlListener : public SimpleDataReaderListener<
	HwaSimIRDds::ControlCommandV1, HwaSimIRDds::ControlCommandV1Seq,
	HwaSimIRDds::ControlCommandV1DataReader>
{
public:
	explicit DdsDisplayControlListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader*, const HwaSimIRDds::ControlCommandV1& sample,
		const SampleInfo&) override
	{
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		qInfo().noquote() << QStringLiteral(
			"[DdsProtocolReceiver] type=control platID=%1 command=%2 round=%3")
			.arg(sample.platID).arg(sample.simCommand).arg(sample.currentRound);
		emit m_owner->controlCmdReceived(HwaSimIRDdsAdapter::FromDds(sample));
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsDisplayInitListener : public SimpleDataReaderListener<
	HwaSimIRDds::InitCommandV1, HwaSimIRDds::InitCommandV1Seq,
	HwaSimIRDds::InitCommandV1DataReader>
{
public:
	explicit DdsDisplayInitListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader*, const HwaSimIRDds::InitCommandV1& sample,
		const SampleInfo&) override
	{
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		if (m_owner->m_config.sensorID >= 0 && sample.sensorID != m_owner->m_config.sensorID &&
			sample.sensorID != 255) return;
		qInfo().noquote() << QStringLiteral(
			"[DdsProtocolReceiver] type=init platID=%1 sensorID=%2 simMode=%3 videoFps=%4")
			.arg(sample.platID).arg(sample.sensorID).arg(sample.trackingInit.simMode)
			.arg(sample.trackingInit.videoFps);
		// A new INIT may reuse currentRound=1. The numeric round alone cannot
		// delimit frame ordinal sets. Preserve lifetime transport counters, but
		// reset the association diagnostics before accepting this run's frames.
		m_owner->logFrameSync(true);
		{
			std::lock_guard<std::mutex> lock(m_owner->m_impl->syncMutex);
			qInfo().noquote() << QStringLiteral("[DdsFrameSyncReset] reason=init previousRound=%1 previousVideo=%2 lifetimeSamples=%3")
				.arg(m_owner->m_impl->syncRound).arg(m_owner->m_impl->syncVideo).arg(m_owner->m_receivedSamples.load());
			m_owner->m_impl->syncVideo = m_owner->m_impl->syncMeta = m_owner->m_impl->syncAnnotation = 0;
			m_owner->m_impl->syncMismatch = 0;
			m_owner->m_impl->lastMetaSeq = m_owner->m_impl->lastAnnotationSeq = 0;



		}
		emit m_owner->initCommandReceived(HwaSimIRDdsAdapter::FromDds(sample));
	}
private:
	DdsVideoReceiverWorker* m_owner;
};

class DdsDisplayRealtimeListener : public SimpleDataReaderListener<
	HwaSimIRDds::RealtimeDataV1, HwaSimIRDds::RealtimeDataV1Seq,
	HwaSimIRDds::RealtimeDataV1DataReader>
{
public:
	explicit DdsDisplayRealtimeListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader*, const HwaSimIRDds::RealtimeDataV1& sample,
		const SampleInfo&) override
	{
		if (m_owner->m_config.platID >= 0 && sample.platID != m_owner->m_config.platID) return;
		if (m_owner->m_config.sensorID >= 0 && sample.sensorID != m_owner->m_config.sensorID &&
			sample.sensorID != 255) return;
		static std::atomic<quint64> count(0);
		const quint64 current = count.fetch_add(1) + 1;
		if (current <= 3 || (current % 120) == 0)
		{
			qInfo().noquote() << QStringLiteral(
				"[DdsProtocolReceiver] type=realtime count=%1 platID=%2 sensorID=%3 time=%4")
				.arg(current).arg(sample.platID).arg(sample.sensorID).arg(sample.time, 0, 'f', 3);
		}
		m_owner->processRealtime(HwaSimIRDdsAdapter::FromDds(sample));
	}
private:
	DdsVideoReceiverWorker* m_owner;
};
#endif

DdsVideoReceiverWorker::DdsVideoReceiverWorker(const DdsVideoReceiverConfig& config, QObject* parent)
	: QObject(parent), m_impl(new Impl()), m_config(config)
{
	m_config.codec = m_config.codec.trimmed().toLower();
	m_config.fps = qMax(1, m_config.fps);
	m_impl->h264Decoder.reset(new H264FfmpegDecoder());
    const QString faultPath=qEnvironmentVariable("P7ReceiverFaultConfig");
    if(!faultPath.isEmpty()){
        QFile input(faultPath);if(!input.open(QIODevice::ReadOnly))qFatal("Cannot open explicit receiver fault configuration");
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(input.readAll(),&error);
        if(error.error!=QJsonParseError::NoError||!doc.isObject())qFatal("Invalid receiver fault configuration");
        m_impl->diagnosticDropSeq=doc.object().value("DropFrameSeq").toVariant().toULongLong();
        m_impl->diagnosticAnnotationDelayMs=qBound(0,doc.object().value("AnnotationDelayMs").toInt(),1000);
        qWarning()<<"[ReceiverFaultTest] explicitly_armed"<<faultPath<<"dropSeq="<<m_impl->diagnosticDropSeq<<"annotationDelayMs="<<m_impl->diagnosticAnnotationDelayMs;
    }
}

DdsVideoReceiverWorker::~DdsVideoReceiverWorker() = default;

void DdsVideoReceiverWorker::doWork()
{
#if !defined(HWASIMIR_HAS_ZRDDS)
	const QString reason = QStringLiteral("[DdsVideoReceiver][FATAL] DDS selected but binary lacks HWASIMIR_HAS_ZRDDS");
	qCritical().noquote() << reason;
	emit fatalError(reason);
	return;
#else
	DdsBytesListener listener(this);
    DdsTelemetryListener telemetryListener(this);
	DdsVideoStatusListener statusListener(this);
	DdsDisplayControlListener controlListener(this);
	DdsDisplayInitListener initListener(this);
	DdsDisplayRealtimeListener realtimeListener(this);
	DdsVideoMetaListener metaListener(this);
	DdsAnnotationListener annotationListener(this);
	const QFileInfo qosInfo(m_config.qosFile);
	const QString resolvedQos = qosInfo.absoluteFilePath();
	const bool qosExists = qosInfo.exists() && qosInfo.isFile();
	qInfo().noquote() << QStringLiteral(
		"[DdsVideoReceiverConfig] requestedQos=%1 resolvedQos=%2 exists=%3")
		.arg(m_config.qosFile).arg(resolvedQos).arg(qosExists ? 1 : 0);
	if (!qosExists)
	{
		const QString reason = QStringLiteral("qos_file_not_found resolvedQos=%1").arg(resolvedQos);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		return;
	}
	DdsRuntimeConfig runtimeConfig;
	runtimeConfig.domainId = m_config.domainId;
	runtimeConfig.qosFile = m_config.qosFile.toLocal8Bit().constData();
	m_impl->runtime.reset(new DdsRuntimeManager());
	std::string runtimeError;
	if (!m_impl->runtime->start(runtimeConfig, runtimeError))
	{
		const QString reason = QString::fromStdString(runtimeError);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		return;
	}
	DomainParticipant* participant = m_impl->runtime->participant();
    if(m_config.platID>=0&&m_config.sensorID>=0){
        const auto base=QString("HwaSimIR.Diagnostics.V2.%1.%2").arg(m_config.platID).arg(m_config.sensorID).toStdString();
        m_impl->requestTopic=base+".ClockRequest";
        m_impl->telemetryReader=DDSIF::SubTopic(participant,(base+".Board").c_str(),BytesTypeSupport::get_instance(),"hwasimir_protocol_reader",&telemetryListener);
        m_impl->clockWriter=DDSIF::PubTopic(participant,m_impl->requestTopic.c_str(),BytesTypeSupport::get_instance(),"hwasimir_protocol_writer",nullptr);
    }
	const bool deferVideoReader = m_config.autoFromVideoStatus &&
		m_config.platID >= 0 && m_config.sensorID >= 0;
	if (!deferVideoReader)
	{
		const QByteArray topic = m_config.topic.toLatin1();
		m_impl->reader = DDSIF::SubTopic(participant, topic.constData(),
			BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &listener);
	}
	if (m_config.receiveFrameProducts && m_config.autoFromVideoStatus &&
		m_config.platID >= 0 && m_config.sensorID >= 0)
	{
		m_config.topicVideoMeta = QStringLiteral("HwaSimIR.VideoMeta.%1.%2")
			.arg(m_config.platID).arg(m_config.sensorID);
		m_config.topicAnnotation = QStringLiteral("HwaSimIR.Annotation.%1.%2")
			.arg(m_config.platID).arg(m_config.sensorID);
	}
	m_impl->statusReader = DDSIF::SubTopic(participant, m_config.topicVideoStatus.toLatin1().constData(),
		HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(), "hwasimir_status_reader", &statusListener);
	m_impl->controlReader = DDSIF::SubTopic(participant, m_config.topicControl.toLatin1().constData(),
		HwaSimIRDds::ControlCommandV1TypeSupport::get_instance(), "hwasimir_protocol_reader", &controlListener);
	m_impl->initReader = DDSIF::SubTopic(participant, m_config.topicInit.toLatin1().constData(),
		HwaSimIRDds::InitCommandV1TypeSupport::get_instance(), "hwasimir_protocol_reader", &initListener);
	m_impl->realtimeReader = DDSIF::SubTopic(participant, m_config.topicRealtime.toLatin1().constData(),
		HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance(), "hwasimir_protocol_reader", &realtimeListener);
	if (m_config.receiveFrameProducts)
	{
		m_impl->metaReader = DDSIF::SubTopic(participant, m_config.topicVideoMeta.toLatin1().constData(),
			HwaSimIRDds::VideoFrameMetaV1TypeSupport::get_instance(), "hwasimir_protocol_reader", &metaListener);
		m_impl->annotationReader = DDSIF::SubTopic(participant, m_config.topicAnnotation.toLatin1().constData(),
			HwaSimIRDds::AnnotationFrameV1TypeSupport::get_instance(), "hwasimir_protocol_reader", &annotationListener);
	}
	if ((!deferVideoReader && !m_impl->reader) || !m_impl->statusReader || !m_impl->controlReader ||
		!m_impl->initReader || !m_impl->realtimeReader ||
		(m_config.receiveFrameProducts && (!m_impl->metaReader || !m_impl->annotationReader)))
	{
		const QString reason = QStringLiteral("DDS full reader creation failed video=%1 status=%2")
			.arg(m_config.topic).arg(m_config.topicVideoStatus);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		if (m_impl->reader) DDSIF::UnSubTopic(m_impl->reader);
		if (m_impl->statusReader) DDSIF::UnSubTopic(m_impl->statusReader);
		if (m_impl->controlReader) DDSIF::UnSubTopic(m_impl->controlReader);
		if (m_impl->initReader) DDSIF::UnSubTopic(m_impl->initReader);
		if (m_impl->realtimeReader) DDSIF::UnSubTopic(m_impl->realtimeReader);
		if (m_impl->metaReader) DDSIF::UnSubTopic(m_impl->metaReader);
		if (m_impl->annotationReader) DDSIF::UnSubTopic(m_impl->annotationReader);
		if(m_impl->telemetryReader){DDSIF::UnSubTopic(m_impl->telemetryReader);m_impl->telemetryReader=nullptr;}
    if(m_impl->clockWriter){DDSIF::UnPubTopic(m_impl->clockWriter);m_impl->clockWriter=nullptr;}
	m_impl->runtime->shutdown();
		return;
	}
	qInfo().noquote() << QStringLiteral(
		"[DdsVideoReceiver] ready=1 initCount=%7 domain=%1 topic=%2 codec=%3 width=%4 height=%5 fps=%6 wireType=DDS::Bytes fullTransport=1")
		.arg(m_config.domainId).arg(m_config.topic).arg(m_config.codec)
		.arg(m_config.width).arg(m_config.height).arg(m_config.fps).arg(m_impl->runtime->initCount());
	while (!m_stop.load())
	{
		QString nextTopic, nextCodec, nextPixelFormat;
		int nextWidth = 0, nextHeight = 0, nextFps = 0, nextRound = 0;
		int nextPlatID = -1, nextSensorID = -1;
		bool nextRunning = false, pending = false;
		{
			std::lock_guard<std::mutex> lock(m_impl->statusMutex);
			pending = m_impl->statusPending;
			if (pending)
			{
				nextTopic = m_impl->pendingTopic;
				nextCodec = m_impl->pendingCodec;
				nextPixelFormat = m_impl->pendingPixelFormat;
				nextWidth = m_impl->pendingWidth;
				nextHeight = m_impl->pendingHeight;
				nextFps = m_impl->pendingFps;
				nextRunning = m_impl->pendingRunning;
				nextRound = m_impl->pendingRound;
				nextPlatID = m_impl->pendingPlatID;
				nextSensorID = m_impl->pendingSensorID;
				m_impl->statusPending = false;
			}
		}
		if (pending)
		{
			const QString resolvedCodec = nextCodec.trimmed().toLower() == QStringLiteral("raw")
				? (nextPixelFormat.trimmed().toLower() == QStringLiteral("bgr24")
					? QStringLiteral("raw_bgr24") : QStringLiteral("raw_gray8"))
				: nextCodec.trimmed().toLower();
			if (m_config.autoFromVideoStatus && !nextTopic.isEmpty() &&
				(nextTopic != m_config.topic || resolvedCodec != m_config.codec ||
				 nextWidth != m_config.width || nextHeight != m_config.height || nextFps != m_config.fps ||
				 !m_impl->reader))
			{
				DataReader* previousReader = m_impl->reader;
				m_impl->h264Decoder.reset(new H264FfmpegDecoder());
				m_config.topic = nextTopic;
				m_config.codec = resolvedCodec;
				m_config.width = qMax(1, nextWidth);
				m_config.height = qMax(1, nextHeight);
				m_config.fps = qMax(1, nextFps);
				DataReader* nextReader = DDSIF::SubTopic(participant, m_config.topic.toLatin1().constData(),
					BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &listener);
				if (!nextReader)
				{
					++m_ddsErrors;
					emit fatalError(QStringLiteral("VideoStatus topic switch failed: %1").arg(m_config.topic));
					break;
				}
				m_impl->reader = nextReader;
				if (previousReader) m_impl->retiredVideoReaders.push_back(previousReader);
				qInfo().noquote() << QStringLiteral(
					"[VideoStatus] applied=1 topic=%1 codec=%2 width=%3 height=%4 fps=%5 round=%6 running=%7")
					.arg(m_config.topic).arg(m_config.codec).arg(m_config.width).arg(m_config.height)
					.arg(m_config.fps).arg(nextRound).arg(nextRunning ? 1 : 0);
			}
			if (m_config.receiveFrameProducts && m_config.autoFromVideoStatus &&
				nextPlatID >= 0 && nextSensorID >= 0)
			{
				const QString nextMeta = QStringLiteral("HwaSimIR.VideoMeta.%1.%2")
					.arg(nextPlatID).arg(nextSensorID);
				const QString nextAnnotation = QStringLiteral("HwaSimIR.Annotation.%1.%2")
					.arg(nextPlatID).arg(nextSensorID);
				if (nextMeta != m_config.topicVideoMeta)
				{
					DataReader* previousReader = m_impl->metaReader;
					m_config.topicVideoMeta = nextMeta;
					m_impl->metaReader = DDSIF::SubTopic(participant,
						m_config.topicVideoMeta.toLatin1().constData(),
						HwaSimIRDds::VideoFrameMetaV1TypeSupport::get_instance(),
						"hwasimir_protocol_reader", &metaListener);
					if (m_impl->metaReader && previousReader)
						m_impl->retiredMetaReaders.push_back(previousReader);
				}
				if (nextAnnotation != m_config.topicAnnotation)
				{
					DataReader* previousReader = m_impl->annotationReader;
					m_config.topicAnnotation = nextAnnotation;
					m_impl->annotationReader = DDSIF::SubTopic(participant,
						m_config.topicAnnotation.toLatin1().constData(),
						HwaSimIRDds::AnnotationFrameV1TypeSupport::get_instance(),
						"hwasimir_protocol_reader", &annotationListener);
					if (m_impl->annotationReader && previousReader)
						m_impl->retiredAnnotationReaders.push_back(previousReader);
				}
				if (!m_impl->metaReader || !m_impl->annotationReader)
				{
					++m_ddsErrors;
					emit fatalError(QStringLiteral("identity Meta/Annotation topic switch failed"));
					break;
				}
			}
			emit videoStatusChanged(nextTopic, nextCodec, nextPixelFormat,
				nextWidth, nextHeight, nextFps, nextRunning, nextRound);
		}
        const auto clockNow=HwaTelemetryV2::nowNs();
        if(m_impl->clockWriter&&clockNow-m_impl->lastClockRequestNs>=1000000000LL){
            HwaTelemetryV2::Message request;request.kind=1;request.client=m_impl->clockClient;
            request.sequence=++m_impl->requestSequence;request.t1=clockNow;
            {std::lock_guard<std::mutex> lock(m_impl->telemetryMutex);m_impl->clockRequests[request.sequence]=clockNow;
             while(m_impl->clockRequests.size()>8)m_impl->clockRequests.erase(m_impl->clockRequests.begin());}
            auto bytes=HwaTelemetryV2::encode(request);
            DDSIF::BytesWrite(m_config.domainId,const_cast<char*>(m_impl->requestTopic.c_str()),reinterpret_cast<const char*>(bytes.data()),static_cast<Long>(bytes.size()));
            m_impl->lastClockRequestNs=clockNow;
        }
        QThread::msleep(20);
	}
	// Acceptance timers and GUI shutdown can race the final running=false
	// status callback. Always emit one owned-state snapshot before teardown.
	logFrameSync(true);
	if (m_impl->reader) DDSIF::UnSubTopic(m_impl->reader);
	for (DataReader* reader : m_impl->retiredVideoReaders) DDSIF::UnSubTopic(reader);
	if (m_impl->statusReader) DDSIF::UnSubTopic(m_impl->statusReader);
	if (m_impl->controlReader) DDSIF::UnSubTopic(m_impl->controlReader);
	if (m_impl->initReader) DDSIF::UnSubTopic(m_impl->initReader);
	if (m_impl->realtimeReader) DDSIF::UnSubTopic(m_impl->realtimeReader);
	if (m_impl->metaReader) DDSIF::UnSubTopic(m_impl->metaReader);
	for (DataReader* reader : m_impl->retiredMetaReaders) DDSIF::UnSubTopic(reader);
	if (m_impl->annotationReader) DDSIF::UnSubTopic(m_impl->annotationReader);
	for (DataReader* reader : m_impl->retiredAnnotationReaders) DDSIF::UnSubTopic(reader);
	m_impl->reader = m_impl->statusReader = m_impl->controlReader =
		m_impl->initReader = m_impl->realtimeReader = m_impl->metaReader =
		m_impl->annotationReader = nullptr;
	if(m_impl->telemetryReader){DDSIF::UnSubTopic(m_impl->telemetryReader);m_impl->telemetryReader=nullptr;}
    if(m_impl->clockWriter){DDSIF::UnPubTopic(m_impl->clockWriter);m_impl->clockWriter=nullptr;}
	m_impl->runtime->shutdown();
	qInfo().noquote() << QStringLiteral(
		"[DdsVideoReceiverPerf] receivedSamples=%1 receivedBytes=%2 ddsErrors=%3 finalizeCode=manager")
		.arg(m_receivedSamples.load()).arg(m_receivedBytes.load())
		.arg(m_ddsErrors.load());
#endif
}

void DdsVideoReceiverWorker::processVideoStatus(int platID, int sensorID,
	const QString& topic, const QString& codec,
	const QString& pixelFormat, int width, int height, int fps, bool running, int currentRound)
{
	{
		std::lock_guard<std::mutex> lock(m_impl->statusMutex);
		m_impl->pendingTopic = topic;
		m_impl->pendingCodec = codec;
		m_impl->pendingPixelFormat = pixelFormat;
		m_impl->pendingWidth = width;
		m_impl->pendingHeight = height;
		m_impl->pendingFps = fps;
		m_impl->pendingRunning = running;
		m_impl->pendingRound = currentRound;
		m_impl->pendingPlatID = platID;
		m_impl->pendingSensorID = sensorID;
		m_impl->statusPending = true;
	}
	if (currentRound > 0)
	{
		std::lock_guard<std::mutex> lock(m_impl->syncMutex);
		if (m_impl->syncRound != currentRound)
		{
			m_impl->syncRound = currentRound;
			m_impl->syncVideo = m_impl->syncMeta = m_impl->syncAnnotation = 0;
			m_impl->syncMismatch = 0;
			m_impl->lastMetaSeq = m_impl->lastAnnotationSeq = 0;



		}
	}
	if (!running) logFrameSync(true);
}

void DdsVideoReceiverWorker::processRealtime(const BYHWICD::DisplayC2cObjTrackingData& data)
{
	emit dataReceived(QImage(), data, QString(), false, true, false, 0, 0, 0,
		false, 0, 0, 0, WallTimeNs(), 0.0, 0, QStringLiteral("dds_realtime"));
}

void DdsVideoReceiverWorker::processVideoMeta(int currentRound, quint32 frameSeq, double ptsMs)
{
    std::lock_guard<std::mutex> lock(m_impl->syncMutex);
    ++m_impl->syncMeta;
    m_impl->lastMetaSeq=frameSeq;
    m_impl->lastMetaPtsMs=static_cast<qint64>(ptsMs);
    // V1 has no INIT generation; this is diagnostic data, never an arrival-order
    // association authority. Late joining need not start at sequence one.
    Q_UNUSED(currentRound);
}

void DdsVideoReceiverWorker::processAnnotation(int currentRound, quint32 frameSeq,
    double ptsMs, const QString& json)
{
    if(m_impl->diagnosticAnnotationDelayMs>0&&frameSeq==1){
        qWarning()<<"[ReceiverFaultTest] delaying_V1_annotation"<<m_impl->diagnosticAnnotationDelayMs;
        QThread::msleep(m_impl->diagnosticAnnotationDelayMs);
    }
    std::lock_guard<std::mutex> lock(m_impl->syncMutex);
    ++m_impl->syncAnnotation;
    m_impl->lastAnnotationSeq=frameSeq;
    m_impl->lastAnnotationPtsMs=static_cast<qint64>(ptsMs);
    QJsonParseError error;
    const auto doc=QJsonDocument::fromJson(json.toUtf8(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||
       doc.object().value("frameSeq").toVariant().toULongLong()!=frameSeq ||
       doc.object().value("ptsMs").toVariant().toLongLong()!=static_cast<qint64>(ptsMs))
        ++m_impl->syncMismatch;
    m_impl->lastAnnotationBodyHash=QString::fromLatin1(
        QCryptographicHash::hash(json.toUtf8(),QCryptographicHash::Sha256).toHex());
    Q_UNUSED(currentRound);
}

void DdsVideoReceiverWorker::logFrameSync(bool force)
{
    std::lock_guard<std::mutex> lock(m_impl->syncMutex);
    if(!force)return;
    qInfo().noquote()<<QStringLiteral("[DdsFrameSync] round=%1 video=%2 meta=%3 annotation=%4 lastFrameSeq=%5 mismatch=%6 identified=%7 unmatched=%8 association=AU_SEI_V2 countsAreNotFileValidation=1")
        .arg(m_impl->syncRound).arg(m_impl->syncVideo).arg(m_impl->syncMeta)
        .arg(m_impl->syncAnnotation).arg(m_impl->lastMetaSeq).arg(m_impl->syncMismatch)
        .arg(m_impl->identifiedVideos).arg(m_impl->unmatchedVideos);
}

void DdsVideoReceiverWorker::processSample(const char* data, int size)
{
    qint64 arrivalNs=WallTimeNs(); // Full DDS sample arrival, before decode.
    qint64 arrivalSteadyNs=std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    const quint64 sampleIndex = m_receivedSamples.fetch_add(1);
    quint64 logicalFrameSeq=0;
    { std::lock_guard<std::mutex> lock(m_impl->syncMutex); ++m_impl->syncVideo; }
	m_receivedBytes.fetch_add(static_cast<quint64>(qMax(0, size)));
	if (!data || size <= 0)
	{
		++m_ddsErrors;
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] empty sample index=%1").arg(sampleIndex);
		return;
	}
	const QByteArray payload(data, size);
    QByteArray matchedAu=payload; // Own the DDS callback buffer immediately.
	QImage image;
	double decodeMs = 0.0;
	int channels = 0;
	QString imageFormat;
	int codecId = 0;
	bool keyFrame = false;
    HwaFrameV2::Product product;
    bool identified=m_config.codec==QStringLiteral("h264") &&
        HwaFrameV2::extract(reinterpret_cast<const std::uint8_t*>(data),size,product);
    qint64 ptsMs=identified?product.ptsMs:-1;
    if(identified) {
        if((m_config.platID>=0&&product.platID!=m_config.platID)||
           (m_config.sensorID>=0&&product.sensorID!=m_config.sensorID)||
           QString::fromStdString(product.channel)!=m_config.channel) {
            ++m_ddsErrors;return;
        }
        if(std::find(m_impl->retiredSessions.begin(),m_impl->retiredSessions.end(),product.session)!=m_impl->retiredSessions.end()||
           (m_impl->decodeSession==product.session&&(product.generation<m_impl->decodeGeneration||
            (product.generation==m_impl->decodeGeneration&&product.run<m_impl->decodeRun)))){
            qWarning()<<"[FrameProductRejected] stale_session_or_generation";return;
        }
        if(m_impl->decodeSession!=product.session||m_impl->decodeGeneration!=product.generation||m_impl->decodeRun!=product.run) {
            if(!m_impl->decodeSession.empty()&&m_impl->decodeSession!=product.session){
                m_impl->retiredSessions.push_back(m_impl->decodeSession);if(m_impl->retiredSessions.size()>16)m_impl->retiredSessions.pop_front();
            }
            m_impl->h264Decoder->reset(QStringLiteral("producer_session_changed"));m_impl->decodeProducts.clear();
            m_impl->decodeSession=product.session;m_impl->decodeGeneration=product.generation;m_impl->decodeRun=product.run;
            m_impl->lastProductSeq=0;
        }
        if(product.frameSeq<=m_impl->lastProductSeq){qWarning()<<"[FrameProductRejected] duplicate_or_stale_frame"<<product.frameSeq;return;}
        m_impl->lastProductSeq=product.frameSeq;
        if(m_impl->diagnosticDropSeq==product.frameSeq){
            qWarning()<<"[ReceiverFaultTest] dropped_one_sample"<<product.frameSeq<<"recovery=next_decodable_IDR";
            m_impl->h264Decoder->reset(QStringLiteral("explicit_missing_sample_test"));m_impl->decodeProducts.clear();return;
        }
        Impl::ReceivedProduct entry;entry.product=product;entry.au=payload;entry.wallNs=arrivalNs;entry.steadyNs=arrivalSteadyNs;
        m_impl->decodeProducts[ptsMs]=std::move(entry);
        // Bounded by decoder latency, not by run length. An expired identity is
        // explicitly unmatched; it is never reused for another picture.
        while(m_impl->decodeProducts.size()>32)m_impl->decodeProducts.erase(m_impl->decodeProducts.begin());
    }
	if (m_config.codec == QStringLiteral("h264"))
	{
		DecodedVideoFrame decoded;
		QString error;
		if (!m_impl->h264Decoder->decode(payload, false, ptsMs, decoded, error))
		{
			if (error != QStringLiteral("waiting_for_decodable_idr"))
			{
				++m_ddsErrors;
				qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] decode sample=%1 reason=%2")
					.arg(sampleIndex).arg(error);
			}
			return;
		}
        const auto matched=m_impl->decodeProducts.find(decoded.ptsMs);
        identified=matched!=m_impl->decodeProducts.end();
        if(identified){product=std::move(matched->second.product);matchedAu=matched->second.au;
            arrivalNs=matched->second.wallNs;arrivalSteadyNs=matched->second.steadyNs;
            m_impl->decodeProducts.erase(matched);ptsMs=product.ptsMs;}
        image = decoded.image;
		decodeMs = decoded.decodeMs;
		channels = decoded.decodedChannels;
		imageFormat = decoded.imageFormat;
		codecId = 2;
		keyFrame = decoded.keyFrame;
	}
	else if (m_config.codec == QStringLiteral("raw_gray8"))
	{
		const qint64 expected = static_cast<qint64>(m_config.width) * m_config.height;
		if (size != expected)
		{
			++m_ddsErrors;
			qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] raw_size sample=%1 expected=%2 actual=%3")
				.arg(sampleIndex).arg(expected).arg(size);
			return;
		}
		image = QImage(reinterpret_cast<const uchar*>(payload.constData()), m_config.width,
			m_config.height, m_config.width, QImage::Format_Grayscale8).copy();
		channels = 1;
		imageFormat = QStringLiteral("grayscale");
	}
	else if (m_config.codec == QStringLiteral("raw_bgr24"))
	{
		const qint64 expected = static_cast<qint64>(m_config.width) * m_config.height * 3;
		if (size != expected)
		{
			++m_ddsErrors;
			qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] raw_size sample=%1 expected=%2 actual=%3")
				.arg(sampleIndex).arg(expected).arg(size);
			return;
		}
		QImage rgb(m_config.width, m_config.height, QImage::Format_RGB888);
		for (int y = 0; y < m_config.height; ++y)
		{
			const uchar* src = reinterpret_cast<const uchar*>(payload.constData()) + y * m_config.width * 3;
			uchar* dst = rgb.scanLine(y);
			for (int x = 0; x < m_config.width; ++x)
			{
				dst[x * 3 + 0] = src[x * 3 + 2];
				dst[x * 3 + 1] = src[x * 3 + 1];
				dst[x * 3 + 2] = src[x * 3 + 0];
			}
		}
		image = rgb;
		channels = 3;
		imageFormat = QStringLiteral("rgb888_from_bgr24");
	}
	else
	{
		++m_ddsErrors;
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] unsupported codec=%1").arg(m_config.codec);
		return;
	}

    const qint64 decodeCompleteSteadyNs=HwaTelemetryV2::nowNs();
	const bool diagnosticsEnabled = !m_config.dumpFirstFramePath.trimmed().isEmpty();
	// Optional bounded receiver-side proof: actual DDS Annex-B bytes, not a
	// renderer-local recording. Decode/receive counters still count every sample.
	const QByteArray p5VideoPath = qgetenv("P5DdsVideoPath");
	const int p5VideoSamples = qEnvironmentVariableIntValue("P5DdsVideoSamples");
	if (!p5VideoPath.isEmpty() && sampleIndex < static_cast<quint64>(qMax(1,p5VideoSamples))) {
		QFile video(QString::fromLocal8Bit(p5VideoPath));
		if (video.open(QIODevice::WriteOnly | (sampleIndex==0 ? QIODevice::Truncate : QIODevice::Append)))
			video.write(payload);
	}
	if (!image.isNull() && diagnosticsEnabled && (sampleIndex < 3 || ((sampleIndex + 1) % 120) == 0 || (!m_dumpAttempted && static_cast<int>(sampleIndex + 1) >= m_config.dumpFrameIndex)))
	{
		const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
		const quint64 pixelCount = static_cast<quint64>(gray.width()) * static_cast<quint64>(gray.height());
		int minimum = 255;
		int maximum = 0;
		quint64 nonZero = 0;
		double sum = 0.0;
		double sumSquares = 0.0;
		for (int y = 0; y < gray.height(); ++y)
		{
			const uchar* row = gray.constScanLine(y);
			for (int x = 0; x < gray.width(); ++x)
			{
				const int value = row[x];
				minimum = qMin(minimum, value);
				maximum = qMax(maximum, value);
				nonZero += value != 0 ? 1 : 0;
				sum += value;
				sumSquares += static_cast<double>(value) * value;
			}
		}
		const double mean = pixelCount ? sum / static_cast<double>(pixelCount) : 0.0;
		const double variance = pixelCount ? qMax(0.0,
			sumSquares / static_cast<double>(pixelCount) - mean * mean) : 0.0;
		if (sampleIndex < 3 || ((sampleIndex + 1) % 120) == 0)
		{
			qInfo().noquote() << QStringLiteral(
				"[DdsFrameDiag] sample=%1 width=%2 height=%3 min=%4 max=%5 mean=%6 stddev=%7 nonZeroRatio=%8")
				.arg(sampleIndex + 1).arg(gray.width()).arg(gray.height())
				.arg(minimum).arg(maximum).arg(mean, 0, 'f', 3)
				.arg(std::sqrt(variance), 0, 'f', 3)
				.arg(pixelCount ? static_cast<double>(nonZero) / pixelCount : 0.0, 0, 'f', 6);
		}
		if (!m_dumpAttempted && !m_config.dumpFirstFramePath.trimmed().isEmpty() &&
			static_cast<int>(sampleIndex + 1) >= m_config.dumpFrameIndex)
		{
			m_dumpAttempted = true;
			const QFileInfo dumpInfo(m_config.dumpFirstFramePath);
			QDir().mkpath(dumpInfo.absolutePath());
			const bool saved = image.save(dumpInfo.absoluteFilePath(), "PNG");
			qInfo().noquote() << QStringLiteral(
				"[DdsFrameDump] sample=%1 path=%2 saved=%3")
				.arg(sampleIndex + 1).arg(dumpInfo.absoluteFilePath()).arg(saved ? 1 : 0);
			if (!saved)
			{
				++m_ddsErrors;
			}
		}
	}

	BYHWICD::DisplayC2cObjTrackingData tracking;
	std::memset(&tracking, 0, sizeof(tracking));
    QString body;
    if(identified) {
        tracking=product.realtime; logicalFrameSeq=product.frameSeq;
        // Compact new-image timing remains available with MP4 recording off.
        // It contains no image bytes and is enabled only by the audit directory.
        static HwaInputAuditV1::Ledger newImageAudit("received");
        newImageAudit.record(product.realtime,product.sourceSeq,arrivalSteadyNs,decodeCompleteSteadyNs,true);
        QJsonParseError parseError;
        auto doc=QJsonDocument::fromJson(QByteArray::fromStdString(product.annotation),&parseError);
        QJsonObject object=doc.object(), meta;
        meta.insert("version",2);meta.insert("session",QString::fromStdString(product.session));
        meta.insert("channel",QString::fromStdString(product.channel));
        meta.insert("generation",QString::number(product.generation));meta.insert("run",QString::number(product.run));
        meta.insert("frameSeq",QString::number(product.frameSeq));meta.insert("sourceSeq",QString::number(product.sourceSeq));
        meta.insert("platID",product.platID);meta.insert("sensorID",product.sensorID);meta.insert("round",product.round);
        meta.insert("ptsMs",QString::number(product.ptsMs));meta.insert("annotationEnabled",product.annotationEnabled);
        meta.insert("saveRequested",product.saveRequested);
        meta.insert("annotationStatus",parseError.error==QJsonParseError::NoError ?
            (product.annotationEnabled?"matched":"disabled") : "invalid_body");
        meta.insert("annotationSha256",QString::fromLatin1(QCryptographicHash::hash(
            QByteArray::fromStdString(product.annotation),QCryptographicHash::Sha256).toHex()));
        meta.insert("acceptedSteadyNs",QString::number(product.acceptedNs));
        meta.insert("executeSteadyNs",QString::number(product.executeNs));
        meta.insert("captureSteadyNs",QString::number(product.captureNs));
        meta.insert("encodeSteadyNs",QString::number(product.encodeNs));
        meta.insert("writerSubmitSteadyNs",QString::number(product.writerSubmitNs));
        meta.insert("receiveSteadyNs",QString::number(arrivalSteadyNs));
        meta.insert("decodeSteadyNs",QString::number(decodeCompleteSteadyNs));
        meta.insert("queueWaitMs",(product.executeNs-product.acceptedNs)/1.e6);
        {
            std::lock_guard<std::mutex> lock(m_impl->telemetryMutex);
            const bool reliable=m_impl->boardMetrics.server==product.session&&
                m_impl->clockEstimate.valid(HwaTelemetryV2::nowNs())&&product.acceptedNs>0;
            const double latency=(double(arrivalSteadyNs)+m_impl->clockEstimate.offsetNs-double(product.acceptedNs))/1.e6;
            const bool latencyValid=reliable&&latency>=0;
            meta.insert("outputLatencyEstimated",latencyValid);
            if(latencyValid)meta.insert("outputLatencyMs",latency);
            meta.insert("clockOffsetNs",QString::number(m_impl->clockEstimate.offsetNs,'f',0));
            meta.insert("clockUncertaintyMs",m_impl->clockEstimate.uncertaintyMs);
            m_impl->outputLatencyMs=latencyValid?latency:-1;m_impl->clockErrorMs=m_impl->clockEstimate.uncertaintyMs;
        }
        object.insert("_frameProduct",meta);
        body=QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
    { std::lock_guard<std::mutex> lock(m_impl->syncMutex);
      if(identified)++m_impl->identifiedVideos;else ++m_impl->unmatchedVideos; }
    {std::lock_guard<std::mutex> lock(m_impl->telemetryMutex);m_impl->decodedTimes.push_back(HwaTelemetryV2::nowNs());
     while(m_impl->decodedTimes.size()>2000)m_impl->decodedTimes.pop_front();}
    // Preserve every product while bounding outstanding Qt image events.
    {std::unique_lock<std::mutex> lock(m_guiMutex);
     m_guiSpace.wait(lock,[this]{return m_guiPending<8||m_stop.load();});
     if(m_stop.load())return;++m_guiPending;}
    ++m_decodedFrames;
    emit dataReceived(image, tracking, body, true, identified,
        identified&&product.annotationEnabled, identified?4:0, 0, codecId,
        keyFrame, logicalFrameSeq, logicalFrameSeq, ptsMs, arrivalNs, decodeMs, channels, imageFormat, m_config.codec==QStringLiteral("h264")?matchedAu:QByteArray());
	logFrameSync(logicalFrameSeq <= 3 || (logicalFrameSeq % 120u) == 0u);
	if (sampleIndex < 3 || ((sampleIndex + 1) % 120) == 0)
		qInfo().noquote() << QStringLiteral("[DdsVideoReceiverSample] sample=%1 bytes=%2 codec=%3 ddsErrors=%4")
			.arg(sampleIndex + 1).arg(size).arg(m_config.codec).arg(m_ddsErrors.load());
}

QJsonObject DdsVideoReceiverWorker::telemetrySnapshot() const
{
    std::lock_guard<std::mutex> lock(m_impl->telemetryMutex);const auto now=HwaTelemetryV2::nowNs();
    while(!m_impl->decodedTimes.empty()&&m_impl->decodedTimes.front()<=now-1000000000LL)m_impl->decodedTimes.pop_front();
    const bool fresh=m_impl->metricsArrivalNs>0&&now-m_impl->metricsArrivalNs<2000000000LL;
    QJsonObject s;s.insert("available",fresh);s.insert("videoFps",static_cast<int>(m_impl->decodedTimes.size()));
    s.insert("receivedSamples",QString::number(m_receivedSamples.load()));s.insert("decodedFrames",QString::number(m_decodedFrames.load()));
    s.insert("acceptedHz",m_impl->boardMetrics.acceptedHz);s.insert("executedHz",m_impl->boardMetrics.executedHz);
    s.insert("accepted",QString::number(m_impl->boardMetrics.accepted));s.insert("executed",QString::number(m_impl->boardMetrics.executed));
    s.insert("queueWaitMs",m_impl->boardMetrics.queueWaitMs);s.insert("generation",QString::number(m_impl->boardMetrics.generation));
    s.insert("server",QString::fromStdString(m_impl->boardMetrics.server));s.insert("run",QString::number(m_impl->boardMetrics.run));
    s.insert("outputSeq",QString::number(m_impl->boardMetrics.outputSeq));s.insert("finished",m_impl->boardMetrics.finished);
    const auto& control=m_impl->controlResponse;
    s.insert("controlSequence",QString::number(control.controlSequence));s.insert("controlCommand",control.controlCommand);
    s.insert("controlTimeValid",HwaTelemetryV2::controlTimeValid(control));
    s.insert("controlResponseMs",HwaTelemetryV2::controlResponseMs(control));
    s.insert("controlEquivalentHz",HwaTelemetryV2::controlEquivalentHz(control));
    s.insert("clockValid",fresh&&m_impl->clockEstimate.valid(now));s.insert("clockErrorMs",m_impl->clockErrorMs);
    s.insert("outputLatencyMs",fresh&&!m_impl->decodedTimes.empty()?m_impl->outputLatencyMs:-1);
    return s;
}
