#include "TcpCommThread_Linux.h"
#include "HwaSimIR.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "Common/TcpVideoPacketV3.h"

namespace
{
void AppendUint32BE(std::vector<char>& buffer, uint32_t value)
{
	const uint32_t netValue = htonl(value);
	const char* bytes = reinterpret_cast<const char*>(&netValue);
	buffer.insert(buffer.end(), bytes, bytes + 4);
}

std::string JsonEscape(const std::string& value)
{
	std::ostringstream out;
	for (size_t i = 0; i < value.size(); ++i)
	{
		const unsigned char c = static_cast<unsigned char>(value[i]);
		switch (c)
		{
		case '\\': out << "\\\\"; break;
		case '"': out << "\\\""; break;
		case '\b': out << "\\b"; break;
		case '\f': out << "\\f"; break;
		case '\n': out << "\\n"; break;
		case '\r': out << "\\r"; break;
		case '\t': out << "\\t"; break;
		default:
			if (c < 0x20)
			{
				out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
			}
			else
			{
				out << static_cast<char>(c);
			}
			break;
		}
	}
	return out.str();
}

std::string TargetTypeHex(int targetType)
{
	std::ostringstream out;
	out << "0x" << std::uppercase << std::hex << targetType << std::dec;
	return out.str();
}

int ClampInt(int value, int minValue, int maxValue)
{
	return std::max(minValue, std::min(value, maxValue));
}

int ScaleCoord(int value, int srcSize, int dstSize)
{
	if (srcSize <= 0 || dstSize <= 0 || srcSize == dstSize)
	{
		return value;
	}
	const double scaled = static_cast<double>(value) * static_cast<double>(dstSize) / static_cast<double>(srcSize);
	return static_cast<int>(std::floor(scaled + 0.5));
}

std::uint8_t CodecIdForPayload(const std::string& payloadCodec)
{
	if (payloadCodec == "jpeg")
	{
		return HwaSimTcpVideoV3::CodecJpeg;
	}
	if (payloadCodec == "h264_annexb")
	{
		return HwaSimTcpVideoV3::CodecH264AnnexB;
	}
	return HwaSimTcpVideoV3::CodecNone;
}
}

TcpCommThread::TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort,
	const std::string& channel, int localPlatID, int localSensorID)
	: m_pHwaSimIR(hwaSimIR),
	m_channel(channel),
	m_localPlatID(localPlatID),
	m_localSensorID(localSensorID),
	m_tcpSocket(-1),
	m_serverIp(serverIp),
	m_serverPort(serverPort),
	m_bIsRunning(false),
	m_bIsConnected(false)
{
	m_jpegEncoder.reset(new JpegFrameEncoder());
	m_h264Encoder.reset(new H264FfmpegEncoder());
#if defined(HWASIMIR_HAS_RKMPP)
	m_mppEncoder.reset(new H264MppEncoder());
#endif
	memset(&m_serverAddr, 0, sizeof(m_serverAddr));
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIp.c_str(), &m_serverAddr.sin_addr);
}

TcpCommThread::~TcpCommThread()
{
	stop();
}

bool TcpCommThread::start()
{
	if (m_bIsRunning) return true;

	m_bIsRunning = true;
	m_bIsConnected = false;
	m_sendThread = std::thread(&TcpCommThread::sendFrameThreadFunc, this);

	return true;
}

void TcpCommThread::stop()
{
	if (!m_bIsRunning) return;
	m_bIsRunning = false;
	m_frameCv.notify_all();
	m_queueSpaceCv.notify_all();

	if (m_sendThread.joinable())
	{
		m_sendThread.join();
	}
	disconnectFromServer();
	std::cout << "TCP通讯线程已停止" << std::endl;
}

void TcpCommThread::configureOutput(
	int jpegQuality,
	bool jpegGray,
	bool enableH264Experimental,
	bool h264FallbackToJpeg,
	const std::string& h264Encoder,
	int h264BitrateKbps,
	int h264GopFrames,
	bool h264LowLatency,
	bool h264ForceKeyFrameOnStart,
	const std::string& codecConfig)
{
	m_jpegQuality.store(jpegQuality);
	m_jpegGray.store(jpegGray);
	m_enableH264Experimental.store(enableH264Experimental);
	m_h264FallbackToJpeg.store(h264FallbackToJpeg);
	m_h264BitrateKbps.store(std::max(100, h264BitrateKbps));
	m_h264GopFrames.store(std::max(1, h264GopFrames));
	m_h264LowLatency.store(h264LowLatency);
	m_h264ForceKeyFrameOnStart.store(h264ForceKeyFrameOnStart);
	m_encoderResetRequested.store(true);
	m_encoderKeyFrameRequested.store(true);
	std::lock_guard<std::mutex> lock(m_codecMtx);
	m_codecConfig = codecConfig;
	m_h264EncoderConfig = h264Encoder.empty() ? "auto" : h264Encoder;
}

void TcpCommThread::configurePayload(
	int packetVersion,
	bool sendVideo,
	bool sendAnnotation,
	bool sendRealtimeData,
	bool forwardInitControl)
{
	m_packetVersion.store(packetVersion == 2 ? 2 : 3);
	m_sendVideo.store(sendVideo);
	m_sendAnnotation.store(sendAnnotation);
	m_sendRealtimeData.store(sendRealtimeData);
	m_forwardInitControl.store(forwardInitControl);
	m_allPayloadDisabledWarned.store(false);
	std::cout << "[TcpPayloadConfig]"
		<< " channel=" << m_channel
		<< " platID=" << m_localPlatID
		<< " sensorID=" << m_localSensorID
		<< " pid=" << getpid()
		<< " PacketVersion=" << m_packetVersion.load()
		<< " SendVideo=" << (sendVideo ? "1" : "0")
		<< " SendAnnotation=" << (sendAnnotation ? "1" : "0")
		<< " SendRealtimeData=" << (sendRealtimeData ? "1" : "0")
		<< " ForwardInitControl=" << (forwardInitControl ? "1" : "0")
		<< std::endl;
	if (m_packetVersion.load() == 2 && (!sendVideo || !sendAnnotation || !sendRealtimeData))
	{
		std::cout << "[TcpPayloadConfig][WARN]"
			<< " PacketVersion=2 ignores section switches and preserves legacy realtime+annotation+video layout"
			<< std::endl;
	}
}

void TcpCommThread::setH264Requested(bool enabled, int videoFps)
{
	m_h264Requested.store(enabled);
	m_videoFps.store(std::max(1, videoFps));
	m_encoderResetRequested.store(true);
	m_encoderKeyFrameRequested.store(true);
	std::cout << "[VideoEncoder] channel=" << m_channel
		<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
		<< " pid=" << getpid()
		<< " h264En=" << (enabled ? "1" : "0")
		<< " requestedCodec=" << (enabled ? "h264" : "jpeg")
		<< " mppCompiled="
#if defined(HWASIMIR_HAS_RKMPP)
		<< "1"
#else
		<< "0"
#endif
		<< " ffmpegCompiled=" << (m_h264Encoder && m_h264Encoder->isAvailable() ? "1" : "0")
		<< " platformAutoOrder=mpp,ffmpeg,jpeg"
		<< std::endl;
}

void TcpCommThread::requestEncoderKeyFrame(const char* reason)
{
	m_encoderKeyFrameRequested.store(true);
	std::cout << "[VideoEncoder] channel=" << m_channel
		<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
		<< " pid=" << getpid()
		<< " requestKeyFrame=1 reason=" << (reason ? reason : "unspecified") << std::endl;
}

bool TcpCommThread::encodeFrame(
	const RawVideoFrame& rawFrame,
	EncodedVideoFrame& encodedFrame,
	std::string& requestedCodec,
	std::string& requestedBackend)
{
	std::string codecConfig;
	std::string encoderConfig;
	{
		std::lock_guard<std::mutex> lock(m_codecMtx);
		codecConfig = m_codecConfig;
		encoderConfig = m_h264EncoderConfig;
	}
	if (m_encoderResetRequested.exchange(false))
	{
		m_jpegEncoder->reset();
		m_h264Encoder->reset();
#if defined(HWASIMIR_HAS_RKMPP)
		m_mppEncoder->reset();
#endif
		m_jpegEncoderConfigured = false;
		m_h264EncoderConfigured = false;
		m_mppEncoderConfigured = false;
		m_lastCodecFallbackReason.clear();
	}

	VideoEncoderConfig config;
	config.width = rawFrame.width;
	config.height = rawFrame.height;
	config.fps = std::max(1, m_videoFps.load());
	config.bitrateKbps = m_h264BitrateKbps.load();
	config.gopFrames = m_h264GopFrames.load();
	config.jpegQuality = m_jpegQuality.load();
	config.jpegGray = m_jpegGray.load();
	config.lowLatency = m_h264LowLatency.load();
	config.forceKeyFrameOnStart = m_h264ForceKeyFrameOnStart.load();

	const bool h264Requested =
		m_h264Requested.load() &&
		codecConfig != "jpeg" &&
		encoderConfig != "jpeg";
	requestedCodec = h264Requested ? "h264" : "jpeg";
	requestedBackend = h264Requested
		? (encoderConfig.empty() ? "auto" : encoderConfig)
		: "jpeg";
	std::string fallbackReason;
	std::string error;
	if (h264Requested)
	{
		if (!m_enableH264Experimental.load())
		{
			fallbackReason = "h264_backend_disabled";
		}
		else
		{
			const bool automatic =
				encoderConfig.empty() || encoderConfig == "auto";
			const bool requestMpp =
				automatic || encoderConfig == "mpp" || encoderConfig == "rk_mpp";
			const bool requestFfmpeg =
				automatic || encoderConfig == "ffmpeg" || encoderConfig == "libavcodec";
			if (!requestMpp && !requestFfmpeg)
			{
				fallbackReason = "requested_encoder_not_integrated:" + encoderConfig;
			}
			else
			{
				std::string backendFailures;
				auto tryEncoder = [&](
					IVideoEncoder* encoder,
					const char* backendName,
					bool& configured,
					VideoEncoderConfig& runtimeConfig) -> bool
				{
					if (!encoder || !encoder->isAvailable())
					{
						error = std::string(backendName) + "_sdk_or_encoder_unavailable";
						return false;
					}
					const bool configChanged = !configured ||
						runtimeConfig.width != config.width ||
						runtimeConfig.height != config.height ||
						runtimeConfig.fps != config.fps ||
						runtimeConfig.bitrateKbps != config.bitrateKbps ||
						runtimeConfig.gopFrames != config.gopFrames ||
						runtimeConfig.lowLatency != config.lowLatency;
					if (configChanged)
					{
						configured = encoder->configure(config, error);
						runtimeConfig = config;
						if (configured)
						{
							std::cout << "[VideoEncoder]"
								<< " channel=" << m_channel
								<< " platID=" << m_localPlatID
								<< " sensorID=" << m_localSensorID
								<< " pid=" << getpid()
								<< " configured=1 activeCodec=h264_annexb"
								<< " requestedBackend=" << requestedBackend
								<< " activeBackend=" << backendName
								<< " encoderName=" << encoder->name()
								<< " size=" << config.width << "x" << config.height
								<< " fps=" << config.fps
								<< " bitrateKbps=" << config.bitrateKbps
								<< " gop=" << config.gopFrames
								<< " bFrames=0 annexB=1"
								<< std::endl;
						}
					}
					if (!configured)
					{
						if (error.empty())
						{
							error = std::string(backendName) + "_configure_failed";
						}
						return false;
					}
					if (m_encoderKeyFrameRequested.exchange(false))
					{
						encoder->requestKeyFrame();
					}
					if (encoder->encode(rawFrame, encodedFrame, error))
					{
						return true;
					}
					m_encoderKeyFrameRequested.store(true);
					if (error.empty())
					{
						error = std::string(backendName) + "_encode_failed";
					}
					return false;
				};

				if (requestMpp)
				{
#if defined(HWASIMIR_HAS_RKMPP)
					if (tryEncoder(
						m_mppEncoder.get(),
						"mpp",
						m_mppEncoderConfigured,
						m_mppEncoderRuntimeConfig))
					{
						return true;
					}
					backendFailures = "mpp=" + error;
#else
					backendFailures = "mpp=mpp_not_compiled";
#endif
				}
				if (requestFfmpeg && (automatic || !requestMpp))
				{
					if (tryEncoder(
						m_h264Encoder.get(),
						"ffmpeg",
						m_h264EncoderConfigured,
						m_h264EncoderRuntimeConfig))
					{
						if (!backendFailures.empty())
						{
							encodedFrame.fallbackReason = backendFailures;
							std::cout << "[CodecFallback]"
								<< " channel=" << m_channel
								<< " platID=" << m_localPlatID
								<< " sensorID=" << m_localSensorID
								<< " pid=" << getpid()
								<< " requestedCodec=h264 activeCodec=h264_annexb"
								<< " requestedBackend=" << requestedBackend
								<< " activeBackend=ffmpeg"
								<< " reason=" << backendFailures
								<< std::endl;
						}
						return true;
					}
					if (!backendFailures.empty())
					{
						backendFailures += ";";
					}
					backendFailures += "ffmpeg=" + error;
				}
				fallbackReason = backendFailures.empty()
					? "h264_encoder_unavailable"
					: backendFailures;
			}
		}
	}
	if (h264Requested && !m_h264FallbackToJpeg.load())
	{
		if (fallbackReason != m_lastCodecFallbackReason)
		{
			std::cerr << "[CodecFallback] channel=" << m_channel
				<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
				<< " pid=" << getpid()
				<< " allowed=0 action=drop requestedCodec=h264"
				<< " requestedBackend=" << requestedBackend
				<< " activeBackend=none reason=" << fallbackReason << std::endl;
			m_lastCodecFallbackReason = fallbackReason;
		}
		return false;
	}

	const bool jpegConfigChanged = !m_jpegEncoderConfigured ||
		m_jpegEncoderConfig.width != config.width || m_jpegEncoderConfig.height != config.height ||
		m_jpegEncoderConfig.jpegQuality != config.jpegQuality || m_jpegEncoderConfig.jpegGray != config.jpegGray;
	if (jpegConfigChanged)
	{
		m_jpegEncoderConfigured = m_jpegEncoder->configure(config, error);
		m_jpegEncoderConfig = config;
	}
	if (!m_jpegEncoderConfigured || !m_jpegEncoder->encode(rawFrame, encodedFrame, error)) return false;
	if (h264Requested)
	{
		encodedFrame.fallbackReason = fallbackReason.empty() ? "h264_encoder_unavailable" : fallbackReason;
		if (encodedFrame.fallbackReason != m_lastCodecFallbackReason)
		{
			std::cout << "[CodecFallback] channel=" << m_channel
				<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
				<< " pid=" << getpid()
				<< " allowed=1 requestedCodec=h264 activeCodec=jpeg"
				<< " requestedBackend=" << requestedBackend
				<< " activeBackend=jpeg"
				<< " reason=" << encodedFrame.fallbackReason << std::endl;
			m_lastCodecFallbackReason = encodedFrame.fallbackReason;
		}
	}
	return true;
}

void TcpCommThread::resetFrameCounters()
{
	{
		std::lock_guard<std::mutex> lock(m_frameMtx);
		m_frameQueue.clear();
		m_tcpPacketCounter.store(0);
		m_lastTcpPerfLogNs = 0;
	}
	m_encoderResetRequested.store(true);
	m_encoderKeyFrameRequested.store(true);
	m_queueSpaceCv.notify_all();
}

void TcpCommThread::resetInitCompleted()
{
	m_initCompleted.store(false);
	m_encoderResetRequested.store(true);
	m_encoderKeyFrameRequested.store(true);
}

bool TcpCommThread::connectToServer()
{
	disconnectFromServer();

	const int tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tcpSocket == -1)
	{
		return false;
	}

	if (connect(tcpSocket, reinterpret_cast<sockaddr*>(&m_serverAddr), sizeof(m_serverAddr)) == -1)
	{
		close(tcpSocket);
		return false;
	}

	m_tcpSocket = tcpSocket;
	return true;
}

void TcpCommThread::disconnectFromServer()
{
	std::lock_guard<std::mutex> socketLock(m_socketMtx);
	if (m_tcpSocket != -1)
	{
		close(m_tcpSocket);
		m_tcpSocket = -1;
	}
	m_bIsConnected = false;
}

bool TcpCommThread::sendAll(const char* data, int size)
{
	std::lock_guard<std::mutex> socketLock(m_socketMtx);
	if (!m_bIsConnected || m_tcpSocket == -1)
	{
		return false;
	}

	int totalSent = 0;
	while (totalSent < size)
	{
		const ssize_t sent = send(m_tcpSocket, data + totalSent, size - totalSent, MSG_NOSIGNAL);
		if (sent == -1 && errno == EINTR)
		{
			continue;
		}
		if (sent <= 0)
		{
			return false;
		}
		totalSent += static_cast<int>(sent);
	}
	return true;
}

bool TcpCommThread::sendStruct(const void* structPtr, uint32_t structSize)
{
	if (structPtr == nullptr || structSize == 0)
	{
		return false;
	}

	const uint32_t totalLen = 4 + 4 + structSize;
	std::vector<char> packet;
	packet.reserve(totalLen);
	AppendUint32BE(packet, totalLen);
	AppendUint32BE(packet, structSize);
	packet.insert(packet.end(), reinterpret_cast<const char*>(structPtr), reinterpret_cast<const char*>(structPtr) + structSize);

	return sendAll(packet.data(), static_cast<int>(packet.size()));
}

bool TcpCommThread::sendControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
	if (!m_forwardInitControl.load())
	{
		std::cout << "[TcpControlForward] skipped=1 reason=ForwardInitControl_false"
			<< " simCommand=" << cmd.simCommand << std::endl;
		return true;
	}
	if (!m_bIsConnected)
	{
		std::cerr << "sendControlCmd: TCP未连接，无法转发控制命令" << std::endl;
		return false;
	}

	if (!sendStruct(&cmd, static_cast<uint32_t>(sizeof(cmd))))
	{
		std::cerr << "sendControlCmd: 转发控制命令失败，准备重连" << std::endl;
		disconnectFromServer();
		return false;
	}

	std::cout << "[TcpControlForward] simCommand=" << cmd.simCommand
		<< " round=" << cmd.currentRound << "/" << cmd.roundCut << std::endl;
	requestEncoderKeyFrame("control_or_round_change");
	return true;
}

bool TcpCommThread::sendInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& initData)
{
	if (!m_forwardInitControl.load())
	{
		std::cout << "[TcpInitForward] skipped=1 reason=ForwardInitControl_false"
			<< " platID=" << initData.platID
			<< " sensorID=" << initData.sensorID << std::endl;
		return true;
	}
	if (!m_bIsConnected)
	{
		std::cerr << "sendInitCmd: TCP未连接，无法转发初始化命令" << std::endl;
		return false;
	}

	if (!sendStruct(&initData, static_cast<uint32_t>(sizeof(initData))))
	{
		std::cerr << "sendInitCmd: 转发初始化命令失败，准备重连" << std::endl;
		disconnectFromServer();
		return false;
	}

	m_initCompleted = true;
	requestEncoderKeyFrame("initialization_forwarded");
	std::cout << "[TcpInitForward] sensorID=" << initData.sensorID
		<< " platID=" << initData.platID << std::endl;
	//<< " platNumValid=" << initData.platNumValid << std::endl;
	return true;
}

bool TcpCommThread::sendFramePacket(
	const BYHWICD::DisplayC2cObjTrackingData& trackingData,
	const std::string& annotationJson,
	const EncodedVideoFrame& encodedFrame,
	std::uint64_t frameSeq,
	std::uint64_t outputOrdinal,
	std::int64_t ptsMs,
	std::uint32_t& sectionFlags,
	std::uint32_t& realtimeBytes,
	std::uint32_t& annotationBytes,
	std::uint32_t& videoBytes)
{
	const int packetVersion = m_packetVersion.load();
	const bool includeRealtime = packetVersion == 2 || m_sendRealtimeData.load();
	const bool includeAnnotation = packetVersion == 2 || m_sendAnnotation.load();
	const bool includeVideo = packetVersion == 2 || m_sendVideo.load();
	realtimeBytes = includeRealtime
		? static_cast<std::uint32_t>(sizeof(BYHWICD::DisplayC2cObjTrackingData))
		: 0U;
	annotationBytes = includeAnnotation
		? static_cast<std::uint32_t>(annotationJson.size())
		: 0U;
	videoBytes = includeVideo
		? static_cast<std::uint32_t>(encodedFrame.payload.size())
		: 0U;
	sectionFlags =
		(includeRealtime ? HwaSimTcpVideoV3::HasRealtimeData : 0U) |
		(includeAnnotation ? HwaSimTcpVideoV3::HasAnnotation : 0U) |
		(includeVideo ? HwaSimTcpVideoV3::HasVideo : 0U);

	std::vector<char> packet;
	if (packetVersion == 2)
	{
		const std::uint32_t totalLen =
			4U + 4U + realtimeBytes + 4U + annotationBytes + 4U + videoBytes;
		packet.reserve(totalLen);
		AppendUint32BE(packet, totalLen);
		AppendUint32BE(packet, realtimeBytes);
		packet.insert(
			packet.end(),
			reinterpret_cast<const char*>(&trackingData),
			reinterpret_cast<const char*>(&trackingData) + realtimeBytes);
		AppendUint32BE(packet, annotationBytes);
		packet.insert(packet.end(), annotationJson.begin(), annotationJson.end());
		AppendUint32BE(packet, videoBytes);
		packet.insert(
			packet.end(),
			reinterpret_cast<const char*>(encodedFrame.payload.data()),
			reinterpret_cast<const char*>(encodedFrame.payload.data()) + videoBytes);
	}
	else
	{
		HwaSimTcpVideoV3::Header header;
		header.sectionFlags = sectionFlags;
		header.codecId = includeVideo
			? CodecIdForPayload(encodedFrame.payloadCodec)
			: HwaSimTcpVideoV3::CodecNone;
		header.keyFrame = includeVideo && encodedFrame.keyFrame;
		header.frameSeq = frameSeq;
		header.outputOrdinal = outputOrdinal;
		header.ptsMs = ptsMs;
		header.realtimeBytes = realtimeBytes;
		header.annotationBytes = annotationBytes;
		header.videoBytes = videoBytes;
		const std::array<std::uint8_t, HwaSimTcpVideoV3::kHeaderBytes> headerBytes =
			HwaSimTcpVideoV3::EncodeHeader(header);
		const std::uint64_t totalLen64 =
			4ULL + HwaSimTcpVideoV3::kHeaderBytes +
			HwaSimTcpVideoV3::PayloadBytes(header);
		if (header.codecId == HwaSimTcpVideoV3::CodecNone && includeVideo)
		{
			std::cerr << "[TcpFramePacket][ERROR] packetVersion=3 reason=unknown_video_codec"
				<< " payloadCodec=" << encodedFrame.payloadCodec << std::endl;
			return false;
		}
		if (totalLen64 > 0xffffffffULL)
		{
			std::cerr << "[TcpFramePacket][ERROR] packetVersion=3 reason=packet_too_large"
				<< " totalBytes=" << totalLen64 << std::endl;
			return false;
		}
		const std::uint32_t totalLen = static_cast<std::uint32_t>(totalLen64);
		packet.reserve(totalLen);
		AppendUint32BE(packet, totalLen);
		packet.insert(
			packet.end(),
			reinterpret_cast<const char*>(headerBytes.data()),
			reinterpret_cast<const char*>(headerBytes.data()) + headerBytes.size());
		if (includeRealtime)
		{
			packet.insert(
				packet.end(),
				reinterpret_cast<const char*>(&trackingData),
				reinterpret_cast<const char*>(&trackingData) + realtimeBytes);
		}
		if (includeAnnotation)
		{
			packet.insert(packet.end(), annotationJson.begin(), annotationJson.end());
		}
		if (includeVideo)
		{
			packet.insert(
				packet.end(),
				reinterpret_cast<const char*>(encodedFrame.payload.data()),
				reinterpret_cast<const char*>(encodedFrame.payload.data()) + videoBytes);
		}
	}

	return sendAll(packet.data(), static_cast<int>(packet.size()));
}

std::string TcpCommThread::buildAnnotationJson(
	const AnnotationFrameRecord& record,
	bool annotationEnabled,
	int tcpWidth,
	int tcpHeight,
	const IRFrameTelemetry& telemetry,
	std::uint64_t outputOrdinal,
	std::int64_t tcpSendTimeNs,
	int packetVersion,
	const std::string& requestedCodec,
	const std::string& requestedBackend,
	const EncodedVideoFrame& encodedFrame) const
{
	const int srcWidth = record.width > 0 ? record.width : tcpWidth;
	const int srcHeight = record.height > 0 ? record.height : tcpHeight;
	const unsigned long long frameIndex = record.frameIndex > 0 ? record.frameIndex : outputOrdinal;
	const std::string activeCodec = encodedFrame.payloadCodec.empty() ? "none" : encodedFrame.payloadCodec;
	const std::string fallbackReason = encodedFrame.fallbackReason.empty() ? "none" : encodedFrame.fallbackReason;

	std::ostringstream json;
	json << "{\"version\":1"
		<< ",\"packetVersion\":" << packetVersion
		<< ",\"enabled\":" << (annotationEnabled ? "true" : "false")
		<< ",\"frameIndex\":" << frameIndex
		<< ",\"frameSeq\":" << telemetry.sourceSeq
		<< ",\"sourceSeq\":" << telemetry.sourceSeq
		<< ",\"outputOrdinal\":" << outputOrdinal
		<< ",\"udpReceiveTimeNs\":\"" << telemetry.udpReceiveTimeNs << "\""
		<< ",\"tcpSendTimeNs\":\"" << tcpSendTimeNs << "\""
		<< ",\"requestedCodec\":\"" << JsonEscape(requestedCodec) << "\""
		<< ",\"requestedBackend\":\"" << JsonEscape(requestedBackend) << "\""
		<< ",\"activeCodec\":\"" << JsonEscape(activeCodec) << "\""
		<< ",\"codec\":\"" << JsonEscape(activeCodec) << "\""
		<< ",\"payloadCodec\":\"" << JsonEscape(activeCodec) << "\""
		<< ",\"h264En\":" << (m_h264Requested.load() ? "true" : "false")
		<< ",\"codecFallbackReason\":\"" << JsonEscape(fallbackReason) << "\""
		<< ",\"encoderName\":\"" << JsonEscape(encodedFrame.encoderName) << "\""
		<< ",\"keyFrame\":" << (encodedFrame.keyFrame ? "true" : "false")
		<< ",\"ptsMs\":" << encodedFrame.ptsMs
		<< ",\"encodedBytes\":" << encodedFrame.payload.size()
		<< ",\"simTimeMs\":" << record.simTimeMs
		<< ",\"sensorID\":" << record.sensorID
		<< ",\"width\":" << tcpWidth
		<< ",\"height\":" << tcpHeight
		<< ",\"targets\":[";

	if (annotationEnabled)
	{
		bool firstTarget = true;
		for (size_t i = 0; i < record.targets.size(); ++i)
		{
			const TargetAnnotation& target = record.targets[i];
			if (!target.bbox.visible)
			{
				continue;
			}

			const int left = ClampInt(ScaleCoord(target.bbox.x, srcWidth, tcpWidth), 0, std::max(0, tcpWidth - 1));
			const int top = ClampInt(ScaleCoord(target.bbox.y, srcHeight, tcpHeight), 0, std::max(0, tcpHeight - 1));
			const int rawRight = target.bbox.x + std::max(0, target.bbox.width - 1);
			const int rawBottom = target.bbox.y + std::max(0, target.bbox.height - 1);
			const int right = ClampInt(ScaleCoord(rawRight, srcWidth, tcpWidth), 0, std::max(0, tcpWidth - 1));
			const int bottom = ClampInt(ScaleCoord(rawBottom, srcHeight, tcpHeight), 0, std::max(0, tcpHeight - 1));

			if (!firstTarget)
			{
				json << ",";
			}
			firstTarget = false;

			json << "{\"targetType\":" << target.targetType
				<< ",\"targetTypeHex\":\"" << TargetTypeHex(target.targetType) << "\""
				<< ",\"modelLabel\":\"" << JsonEscape(target.modelLabel) << "\""
				<< ",\"targetPlatID\":" << target.targetPlatID
				<< ",\"targetID\":" << target.targetID
				<< ",\"bboxCorners\":["
				<< "{\"x\":" << left << ",\"y\":" << top << "},"
				<< "{\"x\":" << right << ",\"y\":" << top << "},"
				<< "{\"x\":" << right << ",\"y\":" << bottom << "},"
				<< "{\"x\":" << left << ",\"y\":" << bottom << "}]"
				<< ",\"keyPoints\":[";

			bool firstPoint = true;
			for (size_t p = 0; p < target.keyPoints.size(); ++p)
			{
				const AnnotationPoint2D& point = target.keyPoints[p];
				if (!point.visible)
				{
					continue;
				}
				const int px = ClampInt(ScaleCoord(point.x, srcWidth, tcpWidth), 0, std::max(0, tcpWidth - 1));
				const int py = ClampInt(ScaleCoord(point.y, srcHeight, tcpHeight), 0, std::max(0, tcpHeight - 1));
				if (!firstPoint)
				{
					json << ",";
				}
				firstPoint = false;
				json << "{\"name\":\"" << JsonEscape(point.name) << "\""
					<< ",\"x\":" << px
					<< ",\"y\":" << py
					<< ",\"visible\":true}";
			}
			json << "]}";
		}
	}

	json << "]}";
	return json.str();
}

void TcpCommThread::sendFrameThreadFunc()
{
	EncodedVideoFrame encodedFrame;
	std::cout << "TCP发送后台线程已启动..." << std::endl;

	while (m_bIsRunning)
	{
		if (!m_bIsConnected)
		{
			if (connectToServer())
			{
				m_bIsConnected = true;
				requestEncoderKeyFrame("tcp_connected");
				std::cout << "TCP成功连接到服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		PendingFrame frame;
		int queueDepth = 0;
		{
			std::unique_lock<std::mutex> lock(m_frameMtx);
			if (m_frameCv.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return !m_frameQueue.empty() || !m_bIsRunning; }))
			{
				if (!m_bIsRunning) break;

				frame = std::move(m_frameQueue.front());
				m_frameQueue.pop_front();
				queueDepth = static_cast<int>(m_frameQueue.size());
				m_queueSpaceCv.notify_one();
			}
			else
			{
				if (m_bIsConnected && m_tcpSocket != -1)
				{
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(m_tcpSocket, &readSet);
					timeval tv = { 0, 0 };

					if (select(m_tcpSocket + 1, &readSet, NULL, NULL, &tv) > 0)
					{
						char dummy;
						const int peekRet = recv(m_tcpSocket, &dummy, 1, MSG_PEEK | MSG_DONTWAIT);
						if (peekRet == 0 || (peekRet == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
						{
							std::cerr << "TCP连接丢失(后台心跳检测)，准备重连..." << std::endl;
							disconnectFromServer();
						}
					}
				}
				continue;
			}
		}

		const int packetVersion = m_packetVersion.load();
		const bool includeVideo = packetVersion == 2 || m_sendVideo.load();
		const bool includeAnnotation = packetVersion == 2 || m_sendAnnotation.load();
		const bool includeRealtime = packetVersion == 2 || m_sendRealtimeData.load();
		if (!includeVideo && !includeAnnotation && !includeRealtime)
		{
			if (!m_allPayloadDisabledWarned.exchange(true))
			{
				std::cout << "[TcpPayloadConfig][WARN]"
					<< " channel=" << m_channel
					<< " platID=" << m_localPlatID
					<< " sensorID=" << m_localSensorID
					<< " pid=" << getpid()
					<< " packetVersion=3 action=no_frame_packet"
					<< " reason=all_frame_sections_disabled"
					<< std::endl;
			}
			continue;
		}
		if (includeVideo && frame.pixels.empty())
		{
			continue;
		}

		const std::uint64_t nextOutputOrdinal = m_tcpPacketCounter.load() + 1;
		RawVideoFrame rawFrame;
		rawFrame.data = frame.pixels.data();
		rawFrame.width = frame.width;
		rawFrame.height = frame.height;
		rawFrame.stride = frame.width * 3;
		rawFrame.pixelFormat = RawVideoPixelFormat::Bgr24;
		rawFrame.flipVertical = m_flipVertical.load();
		rawFrame.ptsMs = frame.telemetry.udpReceiveTimeNs > 0
			? frame.telemetry.udpReceiveTimeNs / 1000000LL
			: static_cast<std::int64_t>(nextOutputOrdinal * 1000ULL / std::max(1, m_videoFps.load()));
		std::string requestedCodec = "none";
		std::string requestedBackend = "none";
		encodedFrame.clearForReuse();
		encodedFrame.ptsMs = rawFrame.ptsMs;
		if (includeVideo &&
			!encodeFrame(rawFrame, encodedFrame, requestedCodec, requestedBackend))
		{
			continue;
		}

		const std::uint64_t outputOrdinal = ++m_tcpPacketCounter;
		const std::int64_t tcpSendTimeNs = IRPerfStats::wallTimeNs();
		const std::string annotationJson = includeAnnotation
			? buildAnnotationJson(
				frame.annotationRecord,
				frame.annotationEnabled,
				frame.width,
				frame.height,
				frame.telemetry,
				outputOrdinal,
				tcpSendTimeNs,
				packetVersion,
				requestedCodec,
				requestedBackend,
				encodedFrame)
			: std::string();
		if (annotationJson.size() > 1024 * 1024)
		{
			std::cout << "[TcpFramePacket][WARN] annotationJsonTooLarge"
				<< " channel=" << m_channel << " platID=" << m_localPlatID
				<< " sensorID=" << m_localSensorID << " pid=" << getpid()
				<< " frame=" << outputOrdinal
				<< " annotationBytes=" << annotationJson.size()
				<< std::endl;
		}

		std::uint32_t sectionFlags = 0;
		std::uint32_t realtimeBytes = 0;
		std::uint32_t annotationBytes = 0;
		std::uint32_t videoBytes = 0;
		const auto sendBegin = std::chrono::steady_clock::now();
		if (!sendFramePacket(
			frame.trackingData,
			annotationJson,
			encodedFrame,
			frame.telemetry.sourceSeq,
			outputOrdinal,
			rawFrame.ptsMs,
			sectionFlags,
			realtimeBytes,
			annotationBytes,
			videoBytes))
		{
			std::cerr << "TCP连接丢失(发送帧包失败)，准备重连..." << std::endl;
			disconnectFromServer();
			requestEncoderKeyFrame("tcp_send_failure");
			continue;
		}
		const double tcpSendMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - sendBegin).count();

		if (outputOrdinal <= 3 || (outputOrdinal % 120) == 0)
		{
			std::cout << "[TcpFramePacket]"
				<< " channel=" << m_channel << " platID=" << m_localPlatID
				<< " sensorID=" << m_localSensorID << " pid=" << getpid()
				<< " frame=" << outputOrdinal
				<< " imgBytes=" << videoBytes
				<< " payloadBytes=" << videoBytes
				<< " packetVersion=" << packetVersion
				<< " flags=0x" << std::hex << sectionFlags << std::dec
				<< " codec=" << (includeVideo ? encodedFrame.payloadCodec : "none")
				<< " activeCodec=" << (includeVideo ? encodedFrame.payloadCodec : "none")
				<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
				<< " requestedBackend=" << requestedBackend
				<< " activeBackend=" << (includeVideo ? encodedFrame.encoderName : "none")
				<< " realtimeBytes=" << realtimeBytes
				<< " annotationBytes=" << annotationBytes
				<< " videoBytes=" << videoBytes
				<< " frameSeq=" << frame.telemetry.sourceSeq
				<< " outputOrdinal=" << outputOrdinal
				<< " ptsMs=" << rawFrame.ptsMs
				<< " targets=" << frame.annotationRecord.targets.size()
				<< " width=" << frame.width
				<< " height=" << frame.height
				<< std::endl;
		}

		const std::int64_t perfNowNs = IRPerfStats::steadyTimeNs();
		if (outputOrdinal <= 3 || (outputOrdinal % 120) == 0 ||
			perfNowNs - m_lastTcpPerfLogNs >= 2000000000LL)
		{
			const bool h264Active = encodedFrame.payloadCodec == "h264_annexb";
			std::ostringstream perfLine;
			perfLine << std::fixed << std::setprecision(3)
				<< "[TcpPerf]"
				<< " channel=" << m_channel
				<< " platID=" << m_localPlatID
				<< " sensorID=" << m_localSensorID
				<< " pid=" << getpid()
				<< " sourceSeq=" << frame.telemetry.sourceSeq
				<< " outputOrdinal=" << outputOrdinal
				<< " packetVersion=" << packetVersion
				<< " flags=0x" << std::hex << sectionFlags << std::dec
				<< " codec=" << (includeVideo ? encodedFrame.payloadCodec : "none")
				<< " activeCodec=" << (includeVideo ? encodedFrame.payloadCodec : "none")
				<< " requestedCodec=" << requestedCodec
				<< " requestedBackend=" << requestedBackend
				<< " activeBackend=" << (includeVideo ? encodedFrame.encoderName : "none")
				<< " h264En=" << (m_h264Requested.load() ? "1" : "0")
				<< " codecFallbackReason=" << (encodedFrame.fallbackReason.empty() ? "none" : encodedFrame.fallbackReason)
				<< " encoderName=" << encodedFrame.encoderName
				<< " h264EncoderName=" << encodedFrame.encoderName
				<< " encodeMs=" << encodedFrame.encodeMs
				<< " h264EncodeMs=" << (h264Active ? encodedFrame.encodeMs : 0.0)
				<< " encodedBytes=" << encodedFrame.payload.size()
				<< " payloadBytes=" << videoBytes
				<< " realtimeBytes=" << realtimeBytes
				<< " annotationBytes=" << annotationBytes
				<< " videoBytes=" << videoBytes
				<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
				<< " jpegQuality=" << m_jpegQuality.load()
				<< " jpegMode=" << (m_jpegGray.load() ? "gray" : "rgb")
				<< " jpegBytes=" << (h264Active ? 0 : encodedFrame.payload.size())
				<< " encodeInputChannels=" << encodedFrame.inputChannels
				<< " flipMs=" << encodedFrame.preprocessMs
				<< " resizeMs=0.000"
				<< " jpegMs=" << (h264Active ? 0.0 : encodedFrame.encodeMs)
				<< " tcpSendMs=" << tcpSendMs
				<< " queueDepth=" << queueDepth
				<< " queueWaitMs=" << frame.queueWaitMs
				<< " overwritten=" << (frame.overwritten ? "1" : "0");
			std::cout << perfLine.str() << std::endl;
			if (h264Active)
			{
				std::cout << std::fixed << std::setprecision(3)
					<< "[H264Perf] channel=" << m_channel
					<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
					<< " pid=" << getpid()
					<< " outputOrdinal=" << outputOrdinal
					<< " encodeMs=" << encodedFrame.encodeMs
					<< " payloadBytes=" << encodedFrame.payload.size()
					<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
					<< " queueDepth=" << queueDepth << std::endl;
				if (encodedFrame.encoderName == "rockchip_mpp_avc")
				{
					std::cout << std::fixed << std::setprecision(3)
						<< "[MppPerf] channel=" << m_channel
						<< " platID=" << m_localPlatID
						<< " sensorID=" << m_localSensorID
						<< " pid=" << getpid()
						<< " outputOrdinal=" << outputOrdinal
						<< " colorConvertMs=" << encodedFrame.preprocessMs
						<< " mppEncodeMs=" << encodedFrame.encodeMs
						<< " payloadBytes=" << encodedFrame.payload.size()
						<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
						<< " queueDepth=" << queueDepth
						<< std::endl;
				}
			}
			m_lastTcpPerfLogNs = perfNowNs;
		}
		if (m_pHwaSimIR)
		{
			m_pHwaSimIR->OnTcpFrameSent(
				frame.telemetry,
				outputOrdinal,
				encodedFrame.preprocessMs,
				0.0,
				(encodedFrame.payloadCodec == "jpeg" ? encodedFrame.encodeMs : 0.0),
				tcpSendMs,
				queueDepth,
				frame.queueWaitMs,
				frame.overwritten);
		}
	}
	std::cout << "TCP发送后台线程安全退出" << std::endl;
}

void TcpCommThread::updateFrame(const uchar* data, int width, int height)
{
	BYHWICD::DisplayC2cObjTrackingData trackingData;
	memset(&trackingData, 0, sizeof(trackingData));
	trackingData.flag = 0x38;
	AnnotationFrameRecord annotationRecord;
	IRFrameTelemetry telemetry;
	updateFrame(data, width, height, trackingData, annotationRecord, false, telemetry);
}

IRFrameEnqueueResult TcpCommThread::updateFrame(
	const uchar* data,
	int width,
	int height,
	const BYHWICD::DisplayC2cObjTrackingData& trackingData,
	const AnnotationFrameRecord& annotationRecord,
	bool annotationEnabled,
	const IRFrameTelemetry& telemetry)
{
	IRFrameEnqueueResult result;
	if (data == nullptr || width <= 0 || height <= 0)
	{
		return result;
	}

	const auto waitBegin = std::chrono::steady_clock::now();
	std::unique_lock<std::mutex> lock(m_frameMtx);
	if (m_syncMode.load())
	{
		result.queueWasFull = m_frameQueue.size() >= kMaxFrameQueue;
		m_queueSpaceCv.wait(lock, [this] {
			return m_frameQueue.size() < kMaxFrameQueue || !m_bIsRunning.load();
		});
		if (!m_bIsRunning)
		{
			return result;
		}
	}
	else if (!m_frameQueue.empty())
	{
		m_frameQueue.clear();
		result.overwritten = true;
	}
	result.queueWaitMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - waitBegin).count();

	const auto copyBegin = std::chrono::steady_clock::now();
	PendingFrame frame;
	const bool copyVideo =
		m_packetVersion.load() == 2 ||
		m_sendVideo.load();
	if (copyVideo)
	{
		const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
		frame.pixels.resize(size);
		memcpy(frame.pixels.data(), data, size);
	}
	frame.width = width;
	frame.height = height;
	frame.trackingData = trackingData;
	frame.annotationRecord = annotationRecord;
	frame.annotationEnabled = annotationEnabled;
	frame.telemetry = telemetry;
	frame.queueWaitMs = result.queueWaitMs;
	frame.overwritten = result.overwritten;
	result.copyMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - copyBegin).count();
	m_frameQueue.push_back(std::move(frame));
	result.queueDepth = static_cast<int>(m_frameQueue.size());
	result.accepted = true;
	lock.unlock();
	m_frameCv.notify_one();
	return result;
}
