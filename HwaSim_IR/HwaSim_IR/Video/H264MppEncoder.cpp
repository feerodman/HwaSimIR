#include "VideoEncoder.h"

#if !defined(__linux__) || !defined(__aarch64__) || !defined(HWASIMIR_HAS_RKMPP)
#error "H264MppEncoder.cpp requires Linux aarch64 with HWASIMIR_HAS_RKMPP"
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

extern "C"
{
#include <rk_mpi.h>
#include <rk_venc_cfg.h>
#include <mpp_buffer.h>
#include <mpp_frame.h>
#include <mpp_packet.h>
}

namespace
{
int Align16(int value)
{
	return (value + 15) & ~15;
}

std::uint8_t ClampByte(int value)
{
	return static_cast<std::uint8_t>(std::max(0, std::min(255, value)));
}

bool AnnexBContainsNalType(const std::vector<std::uint8_t>& payload, std::uint8_t expectedType)
{
	std::size_t offset = 0;
	while (offset + 3 < payload.size())
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

bool IsAnnexB(const std::vector<std::uint8_t>& payload)
{
	return payload.size() >= 4 && payload[0] == 0 && payload[1] == 0 &&
		(payload[2] == 1 || (payload[2] == 0 && payload[3] == 1));
}

void ReadRgb(
	const RawVideoFrame& raw,
	int x,
	int y,
	int& red,
	int& green,
	int& blue)
{
	const int sourceY = raw.flipVertical ? (raw.height - 1 - y) : y;
	const std::uint8_t* row = raw.data +
		static_cast<std::size_t>(sourceY) * static_cast<std::size_t>(raw.stride);
	if (raw.pixelFormat == RawVideoPixelFormat::Gray8)
	{
		red = green = blue = row[x];
		return;
	}
	const std::uint8_t* pixel = row + static_cast<std::size_t>(x) * 3U;
	if (raw.pixelFormat == RawVideoPixelFormat::Rgb24)
	{
		red = pixel[0];
		green = pixel[1];
		blue = pixel[2];
	}
	else
	{
		blue = pixel[0];
		green = pixel[1];
		red = pixel[2];
	}
}

bool ConvertToNv12(
	const RawVideoFrame& raw,
	std::uint8_t* destination,
	int horizontalStride,
	int verticalStride,
	std::string& error)
{
	if (!raw.data || !destination || raw.width <= 0 || raw.height <= 0 ||
		(raw.width & 1) != 0 || (raw.height & 1) != 0 || raw.stride <= 0)
	{
		error = "mpp_invalid_nv12_input";
		return false;
	}
	const int minimumStride = raw.pixelFormat == RawVideoPixelFormat::Gray8
		? raw.width
		: raw.width * 3;
	if (raw.stride < minimumStride)
	{
		error = "mpp_input_stride_too_small";
		return false;
	}

	std::uint8_t* yPlane = destination;
	std::uint8_t* uvPlane = destination +
		static_cast<std::size_t>(horizontalStride) * static_cast<std::size_t>(verticalStride);
	std::memset(
		yPlane,
		raw.pixelFormat == RawVideoPixelFormat::Gray8 ? 0 : 16,
		static_cast<std::size_t>(horizontalStride) * static_cast<std::size_t>(verticalStride));
	std::memset(
		uvPlane,
		128,
		static_cast<std::size_t>(horizontalStride) * static_cast<std::size_t>(verticalStride / 2));

	for (int y = 0; y < raw.height; ++y)
	{
		std::uint8_t* output = yPlane + static_cast<std::size_t>(y) * static_cast<std::size_t>(horizontalStride);
		for (int x = 0; x < raw.width; ++x)
		{
			int red = 0;
			int green = 0;
			int blue = 0;
			ReadRgb(raw, x, y, red, green, blue);
			output[x] = raw.pixelFormat == RawVideoPixelFormat::Gray8
				? static_cast<std::uint8_t>(red)
				: ClampByte(((66 * red + 129 * green + 25 * blue + 128) >> 8) + 16);
		}
	}

	if (raw.pixelFormat != RawVideoPixelFormat::Gray8)
	{
		for (int y = 0; y < raw.height; y += 2)
		{
			std::uint8_t* output = uvPlane +
				static_cast<std::size_t>(y / 2) * static_cast<std::size_t>(horizontalStride);
			for (int x = 0; x < raw.width; x += 2)
			{
				int uTotal = 0;
				int vTotal = 0;
				for (int dy = 0; dy < 2; ++dy)
				{
					for (int dx = 0; dx < 2; ++dx)
					{
						int red = 0;
						int green = 0;
						int blue = 0;
						ReadRgb(raw, x + dx, y + dy, red, green, blue);
						uTotal += ((-38 * red - 74 * green + 112 * blue + 128) >> 8) + 128;
						vTotal += ((112 * red - 94 * green - 18 * blue + 128) >> 8) + 128;
					}
				}
				output[x] = ClampByte((uTotal + 2) / 4);
				output[x + 1] = ClampByte((vTotal + 2) / 4);
			}
		}
	}
	error.clear();
	return true;
}

std::string MppFailure(const char* operation, MPP_RET result)
{
	std::ostringstream text;
	text << operation << "_failed:" << static_cast<int>(result);
	return text.str();
}

bool SetCfgS32(MppEncCfg cfg, const char* key, RK_S32 value, std::string& error)
{
	const MPP_RET result = mpp_enc_cfg_set_s32(cfg, key, value);
	if (result != MPP_OK)
	{
		error = std::string("mpp_cfg_") + key + "_failed:" + std::to_string(static_cast<int>(result));
		return false;
	}
	return true;
}
}

struct H264MppEncoder::Impl
{
	VideoEncoderConfig config;
	bool configured = false;
	int horizontalStride = 0;
	int verticalStride = 0;
	std::size_t inputBufferBytes = 0;
	MppCtx context = nullptr;
	MppApi* api = nullptr;
	MppEncCfg encoderConfig = nullptr;
	MppBufferGroup bufferGroup = nullptr;
	MppBuffer inputBuffer = nullptr;
	MppFrame inputFrame = nullptr;
	std::vector<std::uint8_t> parameterSets;
};

H264MppEncoder::H264MppEncoder()
	: m_impl(new Impl())
{
}

H264MppEncoder::~H264MppEncoder()
{
	reset();
	delete m_impl;
	m_impl = nullptr;
}

bool H264MppEncoder::isAvailable() const
{
	return true;
}

const char* H264MppEncoder::name() const
{
	return "rockchip_mpp_avc";
}

void H264MppEncoder::reset()
{
	if (!m_impl)
	{
		return;
	}
	if (m_impl->inputFrame)
	{
		mpp_frame_deinit(&m_impl->inputFrame);
	}
	if (m_impl->inputBuffer)
	{
		mpp_buffer_put(m_impl->inputBuffer);
		m_impl->inputBuffer = nullptr;
	}
	if (m_impl->bufferGroup)
	{
		mpp_buffer_group_put(m_impl->bufferGroup);
		m_impl->bufferGroup = nullptr;
	}
	if (m_impl->encoderConfig)
	{
		mpp_enc_cfg_deinit(m_impl->encoderConfig);
		m_impl->encoderConfig = nullptr;
	}
	if (m_impl->context)
	{
		if (m_impl->api)
		{
			m_impl->api->reset(m_impl->context);
		}
		mpp_destroy(m_impl->context);
		m_impl->context = nullptr;
		m_impl->api = nullptr;
	}
	m_impl->configured = false;
	m_impl->horizontalStride = 0;
	m_impl->verticalStride = 0;
	m_impl->inputBufferBytes = 0;
	m_impl->parameterSets.clear();
	m_forceKeyFrame.store(true);
}

void H264MppEncoder::requestKeyFrame()
{
	m_forceKeyFrame.store(true);
}

bool H264MppEncoder::configure(const VideoEncoderConfig& config, std::string& error)
{
	reset();
	if (config.width <= 0 || config.height <= 0 || config.fps <= 0 ||
		(config.width & 1) != 0 || (config.height & 1) != 0)
	{
		error = "invalid_mpp_h264_encoder_config";
		return false;
	}

	MPP_RET result = mpp_create(&m_impl->context, &m_impl->api);
	if (result != MPP_OK || !m_impl->context || !m_impl->api)
	{
		error = MppFailure("mpp_create", result);
		reset();
		return false;
	}
	MppPollType outputTimeout = MPP_POLL_BLOCK;
	result = m_impl->api->control(m_impl->context, MPP_SET_OUTPUT_TIMEOUT, &outputTimeout);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_set_output_timeout", result);
		reset();
		return false;
	}
	result = mpp_init(m_impl->context, MPP_CTX_ENC, MPP_VIDEO_CodingAVC);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_init_avc", result);
		reset();
		return false;
	}
	result = mpp_enc_cfg_init(&m_impl->encoderConfig);
	if (result != MPP_OK || !m_impl->encoderConfig)
	{
		error = MppFailure("mpp_enc_cfg_init", result);
		reset();
		return false;
	}
	result = m_impl->api->control(m_impl->context, MPP_ENC_GET_CFG, m_impl->encoderConfig);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_enc_get_cfg", result);
		reset();
		return false;
	}

	m_impl->horizontalStride = Align16(config.width);
	m_impl->verticalStride = Align16(config.height);
	const int bitrate = std::max(100, config.bitrateKbps) * 1000;
	if (!SetCfgS32(m_impl->encoderConfig, "prep:width", config.width, error) ||
		!SetCfgS32(m_impl->encoderConfig, "prep:height", config.height, error) ||
		!SetCfgS32(m_impl->encoderConfig, "prep:hor_stride", m_impl->horizontalStride, error) ||
		!SetCfgS32(m_impl->encoderConfig, "prep:ver_stride", m_impl->verticalStride, error) ||
		!SetCfgS32(m_impl->encoderConfig, "prep:format", MPP_FMT_YUV420SP, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:mode", MPP_ENC_RC_MODE_CBR, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_in_flex", 0, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_in_num", config.fps, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_in_denom", 1, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_out_flex", 0, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_out_num", config.fps, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:fps_out_denom", 1, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:gop", std::max(1, config.gopFrames), error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:bps_target", bitrate, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:bps_min", bitrate * 15 / 16, error) ||
		!SetCfgS32(m_impl->encoderConfig, "rc:bps_max", bitrate * 17 / 16, error) ||
		!SetCfgS32(m_impl->encoderConfig, "codec:type", MPP_VIDEO_CodingAVC, error) ||
		!SetCfgS32(m_impl->encoderConfig, "h264:profile", 66, error) ||
		!SetCfgS32(m_impl->encoderConfig, "h264:level", 40, error) ||
		!SetCfgS32(m_impl->encoderConfig, "h264:cabac_en", 0, error) ||
		!SetCfgS32(m_impl->encoderConfig, "h264:trans8x8", 0, error))
	{
		reset();
		return false;
	}
	result = m_impl->api->control(m_impl->context, MPP_ENC_SET_CFG, m_impl->encoderConfig);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_enc_set_cfg", result);
		reset();
		return false;
	}
	RK_U32 headerMode = MPP_ENC_HEADER_MODE_EACH_IDR;
	result = m_impl->api->control(
		m_impl->context,
		MPP_ENC_SET_HEADER_MODE,
		&headerMode);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_enc_set_header_mode", result);
		reset();
		return false;
	}

	result = mpp_buffer_group_get_internal(&m_impl->bufferGroup, MPP_BUFFER_TYPE_DRM);
	if (result != MPP_OK || !m_impl->bufferGroup)
	{
		error = MppFailure("mpp_buffer_group_get_internal", result);
		reset();
		return false;
	}
	m_impl->inputBufferBytes =
		static_cast<std::size_t>(m_impl->horizontalStride) *
		static_cast<std::size_t>(m_impl->verticalStride) * 3U / 2U;
	result = mpp_buffer_get(
		m_impl->bufferGroup,
		&m_impl->inputBuffer,
		m_impl->inputBufferBytes);
	if (result != MPP_OK || !m_impl->inputBuffer)
	{
		error = MppFailure("mpp_buffer_get_input", result);
		reset();
		return false;
	}
	result = mpp_frame_init(&m_impl->inputFrame);
	if (result != MPP_OK || !m_impl->inputFrame)
	{
		error = MppFailure("mpp_frame_init", result);
		reset();
		return false;
	}
	mpp_frame_set_width(m_impl->inputFrame, config.width);
	mpp_frame_set_height(m_impl->inputFrame, config.height);
	mpp_frame_set_hor_stride(m_impl->inputFrame, m_impl->horizontalStride);
	mpp_frame_set_ver_stride(m_impl->inputFrame, m_impl->verticalStride);
	mpp_frame_set_fmt(m_impl->inputFrame, MPP_FMT_YUV420SP);
	mpp_frame_set_eos(m_impl->inputFrame, 0);
	mpp_frame_set_buffer(m_impl->inputFrame, m_impl->inputBuffer);

	MppBuffer headerBuffer = nullptr;
	MppPacket headerPacket = nullptr;
	result = mpp_buffer_get(m_impl->bufferGroup, &headerBuffer, 64U * 1024U);
	if (result == MPP_OK && headerBuffer)
	{
		result = mpp_packet_init_with_buffer(&headerPacket, headerBuffer);
	}
	if (result == MPP_OK && headerPacket)
	{
		mpp_packet_set_length(headerPacket, 0);
		result = m_impl->api->control(m_impl->context, MPP_ENC_GET_HDR_SYNC, headerPacket);
	}
	if (result == MPP_OK && headerPacket)
	{
		const std::uint8_t* position =
			static_cast<const std::uint8_t*>(mpp_packet_get_pos(headerPacket));
		const std::size_t length = mpp_packet_get_length(headerPacket);
		if (position && length > 0)
		{
			m_impl->parameterSets.assign(position, position + length);
		}
	}
	if (headerPacket)
	{
		mpp_packet_deinit(&headerPacket);
	}
	if (headerBuffer)
	{
		mpp_buffer_put(headerBuffer);
	}
	if (result != MPP_OK || m_impl->parameterSets.empty() ||
		!AnnexBContainsNalType(m_impl->parameterSets, 7) ||
		!AnnexBContainsNalType(m_impl->parameterSets, 8))
	{
		error = result != MPP_OK
			? MppFailure("mpp_enc_get_header", result)
			: "mpp_avc_header_missing_sps_pps";
		reset();
		return false;
	}

	m_impl->config = config;
	m_impl->configured = true;
	m_forceKeyFrame.store(config.forceKeyFrameOnStart);
	error.clear();
	return true;
}

bool H264MppEncoder::encode(
	const RawVideoFrame& raw,
	EncodedVideoFrame& encoded,
	std::string& error)
{
	encoded.clearForReuse();
	if (!m_impl || !m_impl->configured || !m_impl->api || !m_impl->context ||
		!m_impl->inputBuffer || !m_impl->inputFrame)
	{
		error = "mpp_h264_encoder_not_configured";
		return false;
	}
	if (!raw.data || raw.width != m_impl->config.width || raw.height != m_impl->config.height)
	{
		error = "mpp_h264_input_geometry_changed";
		return false;
	}

	const auto conversionBegin = std::chrono::steady_clock::now();
	std::uint8_t* input =
		static_cast<std::uint8_t*>(mpp_buffer_get_ptr(m_impl->inputBuffer));
	if (!input)
	{
		error = "mpp_input_buffer_map_failed";
		return false;
	}
	mpp_buffer_sync_begin(m_impl->inputBuffer);
	if (!ConvertToNv12(
		raw,
		input,
		m_impl->horizontalStride,
		m_impl->verticalStride,
		error))
	{
		mpp_buffer_sync_end(m_impl->inputBuffer);
		return false;
	}
	mpp_buffer_sync_end(m_impl->inputBuffer);
	encoded.preprocessMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - conversionBegin).count();

	const bool forceKeyFrame = m_forceKeyFrame.exchange(false);
	if (forceKeyFrame)
	{
		const MPP_RET idrResult =
			m_impl->api->control(m_impl->context, MPP_ENC_SET_IDR_FRAME, nullptr);
		if (idrResult != MPP_OK)
		{
			error = MppFailure("mpp_enc_set_idr", idrResult);
			m_forceKeyFrame.store(true);
			return false;
		}
	}
	mpp_frame_set_pts(m_impl->inputFrame, raw.ptsMs);
	mpp_frame_set_eos(m_impl->inputFrame, 0);

	const auto encodeBegin = std::chrono::steady_clock::now();
	MPP_RET result = m_impl->api->encode_put_frame(m_impl->context, m_impl->inputFrame);
	if (result != MPP_OK)
	{
		error = MppFailure("mpp_encode_put_frame", result);
		m_forceKeyFrame.store(true);
		return false;
	}

	bool endOfImage = false;
	bool receivedPacket = false;
	while (!endOfImage)
	{
		MppPacket packet = nullptr;
		result = m_impl->api->encode_get_packet(m_impl->context, &packet);
		if (result != MPP_OK || !packet)
		{
			error = result != MPP_OK
				? MppFailure("mpp_encode_get_packet", result)
				: "mpp_encode_get_packet_returned_null";
			m_forceKeyFrame.store(true);
			return false;
		}
		receivedPacket = true;
		const std::uint8_t* position =
			static_cast<const std::uint8_t*>(mpp_packet_get_pos(packet));
		const std::size_t length = mpp_packet_get_length(packet);
		if (position && length > 0)
		{
			encoded.payload.insert(encoded.payload.end(), position, position + length);
		}
		endOfImage = !mpp_packet_is_partition(packet) || mpp_packet_is_eoi(packet);
		mpp_packet_deinit(&packet);
	}
	if (!receivedPacket || encoded.payload.empty() || !IsAnnexB(encoded.payload))
	{
		error = encoded.payload.empty()
			? "mpp_encoder_produced_no_access_unit"
			: "mpp_h264_payload_is_not_annexb";
		m_forceKeyFrame.store(true);
		return false;
	}

	encoded.keyFrame = AnnexBContainsNalType(encoded.payload, 5);
	if (forceKeyFrame && !encoded.keyFrame)
	{
		error = "mpp_force_idr_not_honored";
		m_forceKeyFrame.store(true);
		return false;
	}
	if (encoded.keyFrame &&
		(!AnnexBContainsNalType(encoded.payload, 7) ||
			!AnnexBContainsNalType(encoded.payload, 8)))
	{
		encoded.payload.insert(
			encoded.payload.begin(),
			m_impl->parameterSets.begin(),
			m_impl->parameterSets.end());
	}
	if (encoded.keyFrame &&
		(!AnnexBContainsNalType(encoded.payload, 7) ||
			!AnnexBContainsNalType(encoded.payload, 8) ||
			!AnnexBContainsNalType(encoded.payload, 5)))
	{
		error = "mpp_key_access_unit_missing_sps_pps_or_idr";
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
}
