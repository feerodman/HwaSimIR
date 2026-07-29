#include "VideoEncoder.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

#if defined(HWASIM_HAS_FFMPEG)
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}
#endif

namespace
{
#if defined(HWASIM_HAS_FFMPEG)
std::string FfmpegError(int code)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, buffer, sizeof(buffer));
	return buffer;
}

AVPixelFormat ToAvPixelFormat(RawVideoPixelFormat format)
{
	switch (format)
	{
	case RawVideoPixelFormat::Rgb24: return AV_PIX_FMT_RGB24;
	case RawVideoPixelFormat::Gray8: return AV_PIX_FMT_GRAY8;
	case RawVideoPixelFormat::Bgr24:
	default: return AV_PIX_FMT_BGR24;
	}
}

bool AnnexBContainsNalType(const std::vector<std::uint8_t>& payload, std::uint8_t expectedType)
{
	std::size_t offset = 0;
	while (offset + 4 <= payload.size())
	{
		std::size_t nalOffset = payload.size();
		for (std::size_t index = offset; index + 3 < payload.size(); ++index)
		{
			if (payload[index] == 0 && payload[index + 1] == 0 && payload[index + 2] == 1)
			{
				nalOffset = index + 3;
				break;
			}
			if (index + 4 < payload.size() && payload[index] == 0 && payload[index + 1] == 0 &&
				payload[index + 2] == 0 && payload[index + 3] == 1)
			{
				nalOffset = index + 4;
				break;
			}
		}
		if (nalOffset >= payload.size())
		{
			return false;
		}
		if ((payload[nalOffset] & 0x1fU) == expectedType)
		{
			return true;
		}
		offset = nalOffset + 1;
	}
	return false;
}
#endif
}

struct H264FfmpegEncoder::Impl
{
	VideoEncoderConfig config;
	bool configured = false;
	std::int64_t frameIndex = 0;
#if defined(HWASIM_HAS_FFMPEG)
	std::string encoderName = "ffmpeg/libavcodec_h264";
	AVCodecContext* codecContext = nullptr;
	AVFrame* frame = nullptr;
	AVPacket* packet = nullptr;
	SwsContext* swsContext = nullptr;
#else
	std::string encoderName = "ffmpeg_unavailable";
#endif
};

H264FfmpegEncoder::H264FfmpegEncoder()
	: m_impl(new Impl())
{
}

H264FfmpegEncoder::~H264FfmpegEncoder()
{
	reset();
	delete m_impl;
	m_impl = nullptr;
}

bool H264FfmpegEncoder::isAvailable() const
{
#if defined(HWASIM_HAS_FFMPEG)
	return avcodec_find_encoder_by_name("libx264") != nullptr ||
		avcodec_find_encoder(AV_CODEC_ID_H264) != nullptr;
#else
	return false;
#endif
}

const char* H264FfmpegEncoder::name() const
{
	return m_impl ? m_impl->encoderName.c_str() : "ffmpeg_unavailable";
}

void H264FfmpegEncoder::reset()
{
	if (!m_impl)
	{
		return;
	}
#if defined(HWASIM_HAS_FFMPEG)
	if (m_impl->swsContext)
	{
		sws_freeContext(m_impl->swsContext);
		m_impl->swsContext = nullptr;
	}
	if (m_impl->packet)
	{
		av_packet_free(&m_impl->packet);
	}
	if (m_impl->frame)
	{
		av_frame_free(&m_impl->frame);
	}
	if (m_impl->codecContext)
	{
		avcodec_free_context(&m_impl->codecContext);
	}
#endif
	m_impl->configured = false;
	m_impl->frameIndex = 0;
	m_forceKeyFrame.store(true);
}

void H264FfmpegEncoder::requestKeyFrame()
{
	m_forceKeyFrame.store(true);
}

bool H264FfmpegEncoder::configure(const VideoEncoderConfig& config, std::string& error)
{
	reset();
#if !defined(HWASIM_HAS_FFMPEG)
	(void)config;
	error = "ffmpeg_sdk_not_compiled";
	return false;
#else
	if (config.width <= 0 || config.height <= 0 ||
		(config.width & 1) != 0 || (config.height & 1) != 0 || config.fps <= 0)
	{
		error = "invalid_h264_encoder_config";
		return false;
	}

	const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
	if (!codec)
	{
		codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	}
	if (!codec)
	{
		error = "ffmpeg_h264_encoder_not_found";
		return false;
	}

	m_impl->codecContext = avcodec_alloc_context3(codec);
	if (!m_impl->codecContext)
	{
		error = "avcodec_alloc_context_failed";
		return false;
	}
	AVCodecContext* context = m_impl->codecContext;
	context->width = config.width;
	context->height = config.height;
	context->pix_fmt = AV_PIX_FMT_YUV420P;
	context->time_base = AVRational{ 1, config.fps };
	context->framerate = AVRational{ config.fps, 1 };
	context->bit_rate = static_cast<std::int64_t>(std::max(100, config.bitrateKbps)) * 1000LL;
	context->gop_size = std::max(1, config.gopFrames);
	context->max_b_frames = 0;
	context->thread_count = 1;
	context->flags &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

	AVDictionary* options = nullptr;
	av_dict_set(&options, "preset", "ultrafast", 0);
	if (config.lowLatency)
	{
		av_dict_set(&options, "tune", "zerolatency", 0);
	}
	av_dict_set(&options, "profile", "baseline", 0);
	av_dict_set(&options, "x264-params", "annexb=1:repeat-headers=1:scenecut=0:bframes=0", 0);
	const int openResult = avcodec_open2(context, codec, &options);
	av_dict_free(&options);
	if (openResult < 0)
	{
		error = "avcodec_open2_failed:" + FfmpegError(openResult);
		reset();
		return false;
	}

	m_impl->frame = av_frame_alloc();
	m_impl->packet = av_packet_alloc();
	if (!m_impl->frame || !m_impl->packet)
	{
		error = "ffmpeg_frame_or_packet_alloc_failed";
		reset();
		return false;
	}
	m_impl->frame->format = context->pix_fmt;
	m_impl->frame->width = context->width;
	m_impl->frame->height = context->height;
	const int bufferResult = av_frame_get_buffer(m_impl->frame, 32);
	if (bufferResult < 0)
	{
		error = "av_frame_get_buffer_failed:" + FfmpegError(bufferResult);
		reset();
		return false;
	}

	m_impl->config = config;
	m_impl->configured = true;
	m_impl->frameIndex = 0;
	m_impl->encoderName = std::string("ffmpeg/") + codec->name;
	m_forceKeyFrame.store(config.forceKeyFrameOnStart);
	error.clear();
	return true;
#endif
}

bool H264FfmpegEncoder::encode(
	const RawVideoFrame& raw,
	EncodedVideoFrame& encoded,
	std::string& error)
{
	encoded.clearForReuse();
#if !defined(HWASIM_HAS_FFMPEG)
	(void)raw;
	error = "ffmpeg_sdk_not_compiled";
	return false;
#else
	if (!m_impl->configured || !m_impl->codecContext || !m_impl->frame || !m_impl->packet)
	{
		error = "h264_encoder_not_configured";
		return false;
	}
	if (!raw.data || raw.width != m_impl->config.width || raw.height != m_impl->config.height || raw.stride <= 0)
	{
		error = "h264_input_geometry_changed";
		return false;
	}

	const auto encodeBegin = std::chrono::steady_clock::now();
	const int writableResult = av_frame_make_writable(m_impl->frame);
	if (writableResult < 0)
	{
		error = "av_frame_make_writable_failed:" + FfmpegError(writableResult);
		return false;
	}

	const AVPixelFormat inputFormat = ToAvPixelFormat(raw.pixelFormat);
	m_impl->swsContext = sws_getCachedContext(
		m_impl->swsContext,
		raw.width,
		raw.height,
		inputFormat,
		raw.width,
		raw.height,
		AV_PIX_FMT_YUV420P,
		SWS_FAST_BILINEAR,
		nullptr,
		nullptr,
		nullptr);
	if (!m_impl->swsContext)
	{
		error = "sws_get_cached_context_failed";
		return false;
	}

	const std::uint8_t* sourceData = raw.data;
	int sourceStride = raw.stride;
	if (raw.flipVertical)
	{
		sourceData += static_cast<std::size_t>(raw.height - 1) * static_cast<std::size_t>(raw.stride);
		sourceStride = -raw.stride;
	}
	const std::uint8_t* sourcePlanes[4] = { sourceData, nullptr, nullptr, nullptr };
	const int sourceStrides[4] = { sourceStride, 0, 0, 0 };
	const int scaledRows = sws_scale(
		m_impl->swsContext,
		sourcePlanes,
		sourceStrides,
		0,
		raw.height,
		m_impl->frame->data,
		m_impl->frame->linesize);
	if (scaledRows != raw.height)
	{
		error = "sws_scale_failed";
		return false;
	}

	const bool forceKeyFrame = m_forceKeyFrame.exchange(false);
	m_impl->frame->pts = m_impl->frameIndex++;
	m_impl->frame->pict_type = forceKeyFrame ? AV_PICTURE_TYPE_I : AV_PICTURE_TYPE_NONE;
	const int sendResult = avcodec_send_frame(m_impl->codecContext, m_impl->frame);
	if (sendResult < 0)
	{
		error = "avcodec_send_frame_failed:" + FfmpegError(sendResult);
		m_forceKeyFrame.store(true);
		return false;
	}

	bool receivedPacket = false;
	for (;;)
	{
		const int receiveResult = avcodec_receive_packet(m_impl->codecContext, m_impl->packet);
		if (receiveResult == AVERROR(EAGAIN) || receiveResult == AVERROR_EOF)
		{
			break;
		}
		if (receiveResult < 0)
		{
			error = "avcodec_receive_packet_failed:" + FfmpegError(receiveResult);
			m_forceKeyFrame.store(true);
			return false;
		}
		receivedPacket = true;
		encoded.keyFrame = encoded.keyFrame || ((m_impl->packet->flags & AV_PKT_FLAG_KEY) != 0);
		encoded.payload.insert(
			encoded.payload.end(),
			m_impl->packet->data,
			m_impl->packet->data + m_impl->packet->size);
		av_packet_unref(m_impl->packet);
	}
	if (!receivedPacket || encoded.payload.empty())
	{
		error = "h264_encoder_produced_no_access_unit";
		m_forceKeyFrame.store(true);
		return false;
	}

	const bool annexB = encoded.payload.size() >= 4 && encoded.payload[0] == 0 && encoded.payload[1] == 0 &&
		(encoded.payload[2] == 1 || (encoded.payload[2] == 0 && encoded.payload[3] == 1));
	if (!annexB)
	{
		error = "h264_payload_is_not_annexb";
		m_forceKeyFrame.store(true);
		return false;
	}
	if (encoded.keyFrame &&
		(!AnnexBContainsNalType(encoded.payload, 7) ||
			!AnnexBContainsNalType(encoded.payload, 8) ||
			!AnnexBContainsNalType(encoded.payload, 5)))
	{
		error = "h264_key_access_unit_missing_sps_pps_or_idr";
		m_forceKeyFrame.store(true);
		return false;
	}
	encoded.payloadCodec = "h264_annexb";
	encoded.encoderName = name();
	encoded.ptsMs = raw.ptsMs;
	encoded.inputChannels = raw.pixelFormat == RawVideoPixelFormat::Gray8 ? 1 : 3;
	encoded.encodeMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - encodeBegin).count();
	error.clear();
	return true;
#endif
}
