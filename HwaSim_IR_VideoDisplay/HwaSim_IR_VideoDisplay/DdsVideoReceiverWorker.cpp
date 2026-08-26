#include "DdsVideoReceiverWorker.h"

#include <QByteArray>
#include <QDebug>
#include <QThread>
#include <QtGlobal>
#include <chrono>
#include <cstring>

#include "Video/VideoDecoder.h"

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
#if defined(HWASIMIR_HAS_ZRDDS)
	DomainParticipantFactory* factory = nullptr;
	DomainParticipant* participant = nullptr;
	DataReader* reader = nullptr;
#endif
};

#if defined(HWASIMIR_HAS_ZRDDS)
class DdsBytesListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
	explicit DdsBytesListener(DdsVideoReceiverWorker* owner) : m_owner(owner) {}
	void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) override
	{
		const char* data = reinterpret_cast<const char*>(sample.value.get_contiguous_buffer());
		m_owner->processSample(data, static_cast<int>(sample.value.length()));
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
	const QByteArray qos = m_config.qosFile.toLocal8Bit();
	const QByteArray topic = m_config.topic.toLatin1();
	m_impl->factory = DDSIF::Init(qos.constData(), "hwasimir_factory");
	if (!m_impl->factory)
	{
		const QString reason = QStringLiteral("DDSIF::Init failed qos=%1").arg(m_config.qosFile);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		return;
	}
	m_impl->participant = DDSIF::CreateDP(m_config.domainId, "hwasimir_tcp");
	if (!m_impl->participant)
	{
		const QString reason = QStringLiteral("DDSIF::CreateDP failed domain=%1").arg(m_config.domainId);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		DDSIF::Finalize();
		return;
	}
	m_impl->reader = DDSIF::SubTopic(m_impl->participant, topic.constData(),
		BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &listener);
	if (!m_impl->reader)
	{
		const QString reason = QStringLiteral("DDSIF::SubTopic failed topic=%1").arg(m_config.topic);
		qCritical().noquote() << QStringLiteral("[DdsVideoReceiver][FATAL] %1").arg(reason);
		emit fatalError(reason);
		DDSIF::Finalize();
		return;
	}
	qInfo().noquote() << QStringLiteral(
		"[DdsVideoReceiver] ready=1 initCount=1 domain=%1 topic=%2 codec=%3 width=%4 height=%5 fps=%6 wireType=DDS::Bytes videoOnly=1")
		.arg(m_config.domainId).arg(m_config.topic).arg(m_config.codec)
		.arg(m_config.width).arg(m_config.height).arg(m_config.fps);
	while (!m_stop.load()) QThread::msleep(50);
	const ReturnCode_t result = DDSIF::Finalize();
	qInfo().noquote() << QStringLiteral(
		"[DdsVideoReceiverPerf] receivedSamples=%1 receivedBytes=%2 ddsErrors=%3 finalizeCode=%4")
		.arg(m_receivedSamples.load()).arg(m_receivedBytes.load())
		.arg(m_ddsErrors.load()).arg(static_cast<int>(result));
#endif
}

void DdsVideoReceiverWorker::processSample(const char* data, int size)
{
	const quint64 sampleIndex = m_receivedSamples.fetch_add(1);
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

	BYHWICD::DisplayC2cObjTrackingData tracking;
	std::memset(&tracking, 0, sizeof(tracking));
	emit dataReceived(image, tracking, QString(), true, false, false, 0, 0, codecId,
		keyFrame, sampleIndex + 1, sampleIndex + 1, ptsMs, WallTimeNs(), decodeMs, channels, imageFormat);
	if (sampleIndex < 3 || ((sampleIndex + 1) % 120) == 0)
		qInfo().noquote() << QStringLiteral("[DdsVideoReceiverSample] sample=%1 bytes=%2 codec=%3 ddsErrors=%4")
			.arg(sampleIndex + 1).arg(size).arg(m_config.codec).arg(m_ddsErrors.load());
}
