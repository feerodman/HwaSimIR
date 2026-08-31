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
#include <set>
#include <vector>

#include "Video/VideoDecoder.h"
#include "CommonDataDdsAdapter.h"
#include "DdsRuntimeManager.h"
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
	std::set<quint32> videoSeqs;
	std::set<quint32> metaSeqs;
	std::set<quint32> annotationSeqs;
#if defined(HWASIMIR_HAS_ZRDDS)
	DataReader* reader = nullptr;
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
			m_impl->videoSeqs.clear();
			m_impl->metaSeqs.clear();
			m_impl->annotationSeqs.clear();
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
	Q_UNUSED(ptsMs);
	{
		std::lock_guard<std::mutex> lock(m_impl->syncMutex);
		++m_impl->syncMeta;
		if (currentRound != m_impl->syncRound || !m_impl->metaSeqs.insert(frameSeq).second ||
			(m_impl->lastMetaSeq != 0 && frameSeq != m_impl->lastMetaSeq + 1) ||
			(m_impl->syncMeta == 1 && frameSeq != 1))
			++m_impl->syncMismatch;
		m_impl->lastMetaSeq = frameSeq;
	}
	logFrameSync(frameSeq <= 3 || (frameSeq % 120u) == 0u);
}

void DdsVideoReceiverWorker::processAnnotation(
	int currentRound, quint32 frameSeq, double ptsMs, const QString& json)
{
	Q_UNUSED(ptsMs);
	Q_UNUSED(json);
	{
		std::lock_guard<std::mutex> lock(m_impl->syncMutex);
		++m_impl->syncAnnotation;
		if (currentRound != m_impl->syncRound || !m_impl->annotationSeqs.insert(frameSeq).second ||
			(m_impl->lastAnnotationSeq != 0 && frameSeq != m_impl->lastAnnotationSeq + 1) ||
			(m_impl->syncAnnotation == 1 && frameSeq != 1))
			++m_impl->syncMismatch;
		m_impl->lastAnnotationSeq = frameSeq;
	}
	logFrameSync(frameSeq <= 3 || (frameSeq % 120u) == 0u);
}

void DdsVideoReceiverWorker::logFrameSync(bool force)
{
	std::lock_guard<std::mutex> lock(m_impl->syncMutex);
	if (!force && m_impl->syncVideo > 3 && (m_impl->syncVideo % 120) != 0) return;
	quint64 pendingMeta = 0;
	quint64 pendingAnnotation = 0;
	if (m_config.receiveFrameProducts)
	{
		for (std::set<quint32>::const_iterator it = m_impl->videoSeqs.begin();
			it != m_impl->videoSeqs.end(); ++it)
		{
			if (m_impl->metaSeqs.count(*it) == 0) ++pendingMeta;
			if (m_impl->syncAnnotation > 0 && m_impl->annotationSeqs.count(*it) == 0)
				++pendingAnnotation;
		}
		for (std::set<quint32>::const_iterator it = m_impl->metaSeqs.begin();
			it != m_impl->metaSeqs.end(); ++it)
			if (m_impl->videoSeqs.count(*it) == 0) ++pendingMeta;
		for (std::set<quint32>::const_iterator it = m_impl->annotationSeqs.begin();
			it != m_impl->annotationSeqs.end(); ++it)
			if (m_impl->videoSeqs.count(*it) == 0) ++pendingAnnotation;
	}
	qInfo().noquote() << QStringLiteral(
		"[DdsFrameSync] round=%1 video=%2 meta=%3 annotation=%4 lastFrameSeq=%5 pendingMeta=%6 pendingAnnotation=%7 mismatch=%8")
		.arg(m_impl->syncRound).arg(m_impl->syncVideo).arg(m_impl->syncMeta)
		.arg(m_impl->syncAnnotation).arg(m_impl->lastMetaSeq).arg(pendingMeta)
		.arg(pendingAnnotation).arg(m_impl->syncMismatch);
}

void DdsVideoReceiverWorker::processSample(const char* data, int size)
{
	const quint64 sampleIndex = m_receivedSamples.fetch_add(1);
	quint64 logicalFrameSeq = 0;
	{
		std::lock_guard<std::mutex> lock(m_impl->syncMutex);
		logicalFrameSeq = ++m_impl->syncVideo;
		m_impl->videoSeqs.insert(static_cast<quint32>(logicalFrameSeq));
	}
	m_receivedBytes.fetch_add(static_cast<quint64>(qMax(0, size)));
	if (!data || size <= 0)
	{
		++m_ddsErrors;
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][ERROR] empty sample index=%1").arg(sampleIndex);
		return;
	}
	const QByteArray payload(data, size); // Own the DDS callback buffer immediately.
	QImage image;
	double decodeMs = 0.0;
	int channels = 0;
	QString imageFormat;
	int codecId = 0;
	bool keyFrame = false;
	const qint64 ptsMs = static_cast<qint64>(sampleIndex * 1000ULL / static_cast<quint64>(m_config.fps));
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

	const bool diagnosticsEnabled = !m_config.dumpFirstFramePath.trimmed().isEmpty();
	if (!image.isNull() && diagnosticsEnabled)
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
		if (!m_dumpAttempted && !m_config.dumpFirstFramePath.trimmed().isEmpty())
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
	emit dataReceived(image, tracking, QString(), true, false, false, 0, 0, codecId,
		keyFrame, logicalFrameSeq, logicalFrameSeq, ptsMs, WallTimeNs(), decodeMs, channels, imageFormat);
	logFrameSync(logicalFrameSeq <= 3 || (logicalFrameSeq % 120u) == 0u);
	if (sampleIndex < 3 || ((sampleIndex + 1) % 120) == 0)
		qInfo().noquote() << QStringLiteral("[DdsVideoReceiverSample] sample=%1 bytes=%2 codec=%3 ddsErrors=%4")
			.arg(sampleIndex + 1).arg(size).arg(m_config.codec).arg(m_ddsErrors.load());
}
