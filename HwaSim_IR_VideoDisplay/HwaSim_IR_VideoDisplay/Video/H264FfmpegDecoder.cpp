#include "VideoDecoder.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QVector>
#include <cstring>

#if defined(HWASIM_HAS_FFMPEG)
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}
#endif

namespace
{
bool ContainsIdrNal(const QByteArray& payload)
{
	const unsigned char* data = reinterpret_cast<const unsigned char*>(payload.constData());
	const int size = payload.size();
	for (int i = 0; i + 4 < size; ++i)
	{
		int nalOffset = -1;
		if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
		{
			nalOffset = i + 3;
		}
		else if (i + 4 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
		{
			nalOffset = i + 4;
		}
		if (nalOffset >= 0 && nalOffset < size && (data[nalOffset] & 0x1f) == 5)
		{
			return true;
		}
	}
	return false;
}

QByteArray ExtractParameterSetSignature(const QByteArray& payload)
{
	QByteArray signature;
	const unsigned char* data = reinterpret_cast<const unsigned char*>(payload.constData());
	const int size = payload.size();
	for (int index = 0; index + 3 < size; )
	{
		int nalOffset = -1;
		if (data[index] == 0 && data[index + 1] == 0 && data[index + 2] == 1)
		{
			nalOffset = index + 3;
		}
		else if (index + 4 < size && data[index] == 0 && data[index + 1] == 0 &&
			data[index + 2] == 0 && data[index + 3] == 1)
		{
			nalOffset = index + 4;
		}
		if (nalOffset < 0 || nalOffset >= size)
		{
			++index;
			continue;
		}

		int nextStart = size;
		for (int next = nalOffset + 1; next + 3 < size; ++next)
		{
			if (data[next] == 0 && data[next + 1] == 0 &&
				(data[next + 2] == 1 || (next + 4 < size && data[next + 2] == 0 && data[next + 3] == 1)))
			{
				nextStart = next;
				break;
			}
		}
		const unsigned char nalType = data[nalOffset] & 0x1f;
		if (nalType == 7 || nalType == 8)
		{
			signature.append(static_cast<char>(nalType));
			const quint32 nalSize = static_cast<quint32>(nextStart - nalOffset);
			signature.append(reinterpret_cast<const char*>(&nalSize), sizeof(nalSize));
			signature.append(reinterpret_cast<const char*>(data + nalOffset), nextStart - nalOffset);
		}
		index = nextStart;
	}
	return signature;
}

#if defined(HWASIM_HAS_FFMPEG)
QString FfmpegError(int code)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, buffer, sizeof(buffer));
	return QString::fromLatin1(buffer);
}
#endif
}

struct H264FfmpegDecoder::Impl
{
	bool waitingForIdr = true;
	bool configured = false;
	bool successLogged = false;
	QByteArray packetBuffer;
	QByteArray rgbBuffer;
	QByteArray parameterSetSignature;
#if defined(HWASIM_HAS_FFMPEG)
	QString decoderName = QStringLiteral("ffmpeg/libavcodec_h264");
	AVCodecContext* codecContext = nullptr;
	AVFrame* frame = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* swsContext = nullptr;
#else
	QString decoderName = QStringLiteral("ffmpeg_unavailable");
#endif
};

H264FfmpegDecoder::H264FfmpegDecoder()
	: m_impl(new Impl())
{
}

H264FfmpegDecoder::~H264FfmpegDecoder()
{
	reset(QStringLiteral("destroy"));
	delete m_impl;
	m_impl = nullptr;
}

bool H264FfmpegDecoder::isAvailable() const
{
#if defined(HWASIM_HAS_FFMPEG)
	return avcodec_find_decoder(AV_CODEC_ID_H264) != nullptr;
#else
	return false;
#endif
}

QString H264FfmpegDecoder::name() const
{
	return m_impl ? m_impl->decoderName : QStringLiteral("ffmpeg_unavailable");
}

bool H264FfmpegDecoder::waitingForIdr() const
{
	return !m_impl || m_impl->waitingForIdr;
}

void H264FfmpegDecoder::reset(const QString& reason)
{
	if (!m_impl) return;
#if defined(HWASIM_HAS_FFMPEG)
	if (m_impl->swsContext)
	{
		sws_freeContext(m_impl->swsContext);
		m_impl->swsContext = nullptr;
	}
	if (m_impl->packet) av_packet_free(&m_impl->packet);
	if (m_impl->frame) av_frame_free(&m_impl->frame);
	if (m_impl->codecContext) avcodec_free_context(&m_impl->codecContext);
#endif
	m_impl->configured = false;
	m_impl->waitingForIdr = true;
	m_impl->successLogged = false;
	m_impl->packetBuffer.clear();
	m_impl->parameterSetSignature.clear();
	qDebug().noquote() << QStringLiteral("[VideoDecoder] reset=1 decoder=h264_ffmpeg reason=%1").arg(reason);
}

bool H264FfmpegDecoder::decode(
	const QByteArray& payload,
	bool keyFrame,
	qint64 ptsMs,
	DecodedVideoFrame& decoded,
	QString& error)
{
#if !defined(HWASIM_HAS_FFMPEG)
	Q_UNUSED(payload);
	Q_UNUSED(keyFrame);
	Q_UNUSED(ptsMs);
	Q_UNUSED(decoded);
	error = QStringLiteral("ffmpeg_sdk_not_compiled");
	return false;
#else
	const bool containsIdr = ContainsIdrNal(payload);
	if (m_impl->waitingForIdr && !containsIdr)
	{
		error = QStringLiteral("waiting_for_decodable_idr");
		return false;
	}
	const QByteArray parameterSetSignature = ExtractParameterSetSignature(payload);
	if (!parameterSetSignature.isEmpty() && !m_impl->parameterSetSignature.isEmpty() &&
		parameterSetSignature != m_impl->parameterSetSignature)
	{
		reset(QStringLiteral("sps_pps_changed"));
	}
	if (!parameterSetSignature.isEmpty())
	{
		m_impl->parameterSetSignature = parameterSetSignature;
	}
	if (!m_impl->configured)
	{
		const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		if (!codec)
		{
			error = QStringLiteral("ffmpeg_h264_decoder_not_found");
			return false;
		}
		m_impl->codecContext = avcodec_alloc_context3(codec);
		m_impl->frame = av_frame_alloc();
		m_impl->packet = av_packet_alloc();
		if (!m_impl->codecContext || !m_impl->frame || !m_impl->packet)
		{
			error = QStringLiteral("ffmpeg_decoder_alloc_failed");
			reset(error);
			return false;
		}
		m_impl->codecContext->thread_count = 1;
		m_impl->codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
		const int openResult = avcodec_open2(m_impl->codecContext, codec, nullptr);
		if (openResult < 0)
		{
			error = QStringLiteral("avcodec_open2_failed:%1").arg(FfmpegError(openResult));
			reset(error);
			return false;
		}
		m_impl->decoderName = QStringLiteral("ffmpeg/%1").arg(QString::fromLatin1(codec->name));
		m_impl->configured = true;
		qDebug().noquote() << QStringLiteral("[VideoDecoder] configured=1 codec=h264_annexb decoderName=%1 ffmpeg=%2 persistent=1")
			.arg(m_impl->decoderName, QString::fromLatin1(av_version_info()));
	}

	QElapsedTimer timer;
	timer.start();
	m_impl->packetBuffer.resize(payload.size() + AV_INPUT_BUFFER_PADDING_SIZE);
	std::memcpy(m_impl->packetBuffer.data(), payload.constData(), static_cast<std::size_t>(payload.size()));
	std::memset(m_impl->packetBuffer.data() + payload.size(), 0, AV_INPUT_BUFFER_PADDING_SIZE);
	av_packet_unref(m_impl->packet);
	m_impl->packet->data = reinterpret_cast<std::uint8_t*>(m_impl->packetBuffer.data());
	m_impl->packet->size = payload.size();
	m_impl->packet->pts = ptsMs;
	m_impl->packet->dts = ptsMs;
	const int sendResult = avcodec_send_packet(m_impl->codecContext, m_impl->packet);
	if (sendResult < 0)
	{
		error = QStringLiteral("avcodec_send_packet_failed:%1").arg(FfmpegError(sendResult));
		reset(error);
		return false;
	}

	const int receiveResult = avcodec_receive_frame(m_impl->codecContext, m_impl->frame);
	if (receiveResult == AVERROR(EAGAIN))
	{
		error = QStringLiteral("decoder_needs_more_data");
		return false;
	}
	if (receiveResult < 0)
	{
		error = QStringLiteral("avcodec_receive_frame_failed:%1").arg(FfmpegError(receiveResult));
		reset(error);
		return false;
	}

	const int width = m_impl->frame->width;
	const int height = m_impl->frame->height;
	if (width <= 0 || height <= 0)
	{
		error = QStringLiteral("decoded_frame_invalid_geometry");
		return false;
	}
	m_impl->swsContext = sws_getCachedContext(
		m_impl->swsContext,
		width,
		height,
		static_cast<AVPixelFormat>(m_impl->frame->format),
		width,
		height,
		AV_PIX_FMT_RGB24,
		SWS_FAST_BILINEAR,
		nullptr,
		nullptr,
		nullptr);
	if (!m_impl->swsContext)
	{
		error = QStringLiteral("sws_get_cached_context_failed");
		return false;
	}
	const int rgbStride = width * 3;
	m_impl->rgbBuffer.resize(rgbStride * height);
	std::uint8_t* destinationData[4] = {
		reinterpret_cast<std::uint8_t*>(m_impl->rgbBuffer.data()), nullptr, nullptr, nullptr
	};
	const int destinationStride[4] = { rgbStride, 0, 0, 0 };
	const int rows = sws_scale(
		m_impl->swsContext,
		m_impl->frame->data,
		m_impl->frame->linesize,
		0,
		height,
		destinationData,
		destinationStride);
	if (rows != height)
	{
		error = QStringLiteral("sws_scale_failed");
		return false;
	}
	QImage view(reinterpret_cast<const uchar*>(m_impl->rgbBuffer.constData()), width, height, rgbStride, QImage::Format_RGB888);
	decoded.image = view.copy();
	if (decoded.image.isNull())
	{
		error = QStringLiteral("decoded_image_copy_failed");
		return false;
	}
	decoded.payloadCodec = QStringLiteral("h264_annexb");
	decoded.decoderName = name();
	decoded.keyFrame = keyFrame || containsIdr;
	decoded.ptsMs = ptsMs;
	decoded.decodeMs = static_cast<double>(timer.nsecsElapsed()) / 1.0e6;
	decoded.decodedChannels = 3;
	decoded.imageFormat = QStringLiteral("rgb");
	m_impl->waitingForIdr = false;
	if (!m_impl->successLogged)
	{
		m_impl->successLogged = true;
		qInfo().noquote() << QStringLiteral(
			"[H264DecodeSuccess] backend=ffmpeg codec=h264_annexb resolution=%1x%2 keyFrame=%3 payloadBytes=%4 decodeMs=%5")
			.arg(width)
			.arg(height)
			.arg(decoded.keyFrame ? QStringLiteral("true") : QStringLiteral("false"))
			.arg(payload.size())
			.arg(decoded.decodeMs, 0, 'f', 3);
	}
	error.clear();
	return true;
#endif
}
