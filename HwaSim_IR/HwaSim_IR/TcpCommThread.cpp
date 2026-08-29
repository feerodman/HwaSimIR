#include "TcpCommThread.h"
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <functional>
#include <sstream>
#include <process.h>
#include "Common/TcpVideoPacketV3.h"
//#include <core.h>

namespace
{
class ScopeExit
{
public:
	explicit ScopeExit(const std::function<void()>& fn) : m_fn(fn) {}
	~ScopeExit() { if (m_fn) m_fn(); }
private:
	std::function<void()> m_fn;
};

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

std::string LowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}
}

TcpCommThread::TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort,
	const std::string& channel, int localPlatID, int localSensorID)
	: m_pHwaSimIR(hwaSimIR), m_channel(channel), m_localPlatID(localPlatID), m_localSensorID(localSensorID),
	m_tcpSocket(INVALID_SOCKET), m_bIsRunning(false), m_bIsConnected(false), m_serverIp(serverIp), m_serverPort(serverPort) {
	m_jpegEncoder.reset(new JpegFrameEncoder());
	m_h264Encoder.reset(new H264FfmpegEncoder());
	m_ddsPublisher.reset(new DdsVideoPublisher());
	m_localRecorder.reset(new LocalMp4Recorder());
	// 初始化服务器地址
	memset(&m_serverAddr, 0, sizeof(m_serverAddr));
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIp.c_str(), &m_serverAddr.sin_addr);
}

TcpCommThread::~TcpCommThread() {
	stop();
	WSACleanup(); // 统一在析构中清理 WSA
}

bool TcpCommThread::start() {
	if (m_bIsRunning) return true;
	if ((m_ddsConfig.enabled && !m_ddsConfig.blockWhenQueueFull) ||
		(m_recordingConfig.enabled && !m_recordingConfig.blockWhenQueueFull))
	{
		std::cerr << "[VideoOutput][FATAL] no-drop output requires BlockWhenQueueFull=true" << std::endl;
		return false;
	}

	// 【修改】只初始化 WSA，不在这里 connect。如果没连上，也不妨碍线程启动
	if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0) {
		std::cerr << "WSAStartup失败" << std::endl;
		return false;
	}

	// 启动发送线程
	m_bIsRunning = true;
	m_bIsConnected = false;
	std::string outputError;
	if (m_ddsRuntime) m_ddsPublisher->setRuntime(m_ddsRuntime);
	if (!m_ddsPublisher->start(m_ddsConfig, outputError))
	{
		std::cerr << "[DdsVideo][FATAL] startup failed reason=" << outputError << std::endl;
		m_bIsRunning = false;
		return false;
	}
	m_sendThread = std::thread(&TcpCommThread::sendFrameThreadFunc, this);

	//std::cout << "TCP通讯线程启动成功，连接服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
	return true;
}

void TcpCommThread::stop() {
	if (!m_bIsRunning) return;
	stopOutputRound("process_stop");
	m_bIsRunning = false;
	m_frameCv.notify_all(); // 唤醒可能阻塞在等新帧的线程
	m_queueSpaceCv.notify_all();

	// 等待线程退出
	if (m_sendThread.joinable()) {
		m_sendThread.join();
	}
	std::string outputError;
	if (m_localRecorder) m_localRecorder->stopAndFlush("process_stop", outputError);
	if (m_localRecorder) m_localRecorder->shutdown();
	if (m_ddsPublisher) m_ddsPublisher->shutdown();
	disconnectFromServer();
	std::cout << "TCP通讯线程已停止" << std::endl;
}

void TcpCommThread::configureDdsVideo(const DdsVideoPublisherConfig& config)
{
	m_ddsConfig = config;
	m_ddsEnabled.store(config.enabled);
}

void TcpCommThread::configureDdsRuntime(const std::shared_ptr<DdsRuntimeManager>& runtime)
{
	m_ddsRuntime = runtime;
}

void TcpCommThread::configureLocalRecording(const LocalMp4RecorderConfig& config)
{
	m_recordingConfig = config;
	if (m_localRecorder) m_localRecorder->configure(config, m_channel);
}

void TcpCommThread::setLocalRecordingProtocolEnabled(bool enabled)
{
	if (m_localRecorder) m_localRecorder->setProtocolEnabled(enabled);
}

bool TcpCommThread::startOutputRound(int round)
{
	{
		std::lock_guard<std::mutex> lock(m_frameMtx);
		m_outputRound.store(round);
		m_roundFrameSequence.store(0);
		m_roundLastCompletedFrame.store(0);
		m_outputRoundActive.store(true);
	}
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_pHwaSimIR) m_pHwaSimIR->ResetDdsFrameProductStats();
#endif
	requestEncoderKeyFrame("round_start");
	if (!m_localRecorder || !m_localRecorder->effectiveEnabled()) return true;
	std::string error;
	if (!m_localRecorder->startPending(round, 0, 0, m_videoFps.load(), error))
	{
		std::cerr << "[LocalRecording][ERROR] start failed reason=" << error << std::endl;
		return false;
	}
	return true;
}

bool TcpCommThread::stopOutputRound(const char* reason)
{
	bool wasActive = false;
	unsigned long long targetFrame = 0;
	{
		std::unique_lock<std::mutex> lock(m_frameMtx);
		wasActive = m_outputRoundActive.exchange(false);
		if (!wasActive) return true;
		targetFrame = m_roundFrameSequence.load();
		m_roundDrainCv.wait(lock, [this, targetFrame] {
			return m_roundLastCompletedFrame.load() >= targetFrame || !m_bIsRunning.load();
		});
	}
	bool ok = true;
	std::string error;
	if (m_localRecorder && !m_localRecorder->stopAndFlush(reason, error))
	{
		std::cerr << "[LocalRecording][ERROR] flush failed reason=" << error << std::endl;
		ok = false;
	}
	if (m_ddsPublisher && !m_ddsPublisher->endRound(error))
	{
		std::cerr << "[DdsVideo][ERROR] round drain failed reason=" << error << std::endl;
		ok = false;
	}
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_pHwaSimIR && m_ddsEnabled.load() && !m_pHwaSimIR->DrainDdsFrameProducts(error))
	{
		std::cerr << "[DdsFrameProducts][ERROR] round drain failed reason=" << error << std::endl;
		ok = false;
	}
#endif
	std::cout << "[OutputRoundDrain] reason=" << (reason ? reason : "unknown")
		<< " round=" << m_outputRound.load() << " targetFrames=" << targetFrame
		<< " completedFrames=" << m_roundLastCompletedFrame.load() << std::endl;
	return ok;
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
		<< " pid=" << _getpid()
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
	std::cout << "[VideoEncoder]"
		<< " channel=" << m_channel
		<< " platID=" << m_localPlatID
		<< " sensorID=" << m_localSensorID
		<< " pid=" << _getpid()
		<< " h264En=" << (enabled ? "1" : "0")
		<< " requestedCodec=" << (enabled ? "h264" : "jpeg")
		<< " ffmpegCompiled=" << (m_h264Encoder && m_h264Encoder->isAvailable() ? "1" : "0")
		<< " platformAutoOrder=ffmpeg,jpeg"
		<< " videoFps=" << m_videoFps.load()
		<< std::endl;
	if (m_ddsPublisher && m_ddsPublisher->enabled())
	{
		const std::string codec = resolvedDdsCodec();
		const std::string topic = resolvedDdsTopic(codec);
		bool changed = false;
		std::string error;
		if (!m_ddsPublisher->configureTopic(topic, &changed, error))
		{
			std::cerr << "[DdsVideo][FATAL] configureTopic failed codec=" << codec
				<< " topic=" << topic << " reason=" << error << std::endl;
		}
		else if (changed && codec == "h264")
		{
			requestEncoderKeyFrame("dds_writer_created");
		}
	}
}

std::string TcpCommThread::resolvedDdsCodec() const
{
	std::string codec = LowerAscii(m_ddsConfig.codec);
	if (codec.empty() || codec == "auto")
	{
		if (m_h264Requested.load()) return "h264";
		const std::string format = LowerAscii(m_ddsConfig.rawPixelFormat);
		return (format == "bgr24" || format == "raw_bgr24") ? "raw_bgr24" : "raw_gray8";
	}
	if (codec == "gray8") return "raw_gray8";
	if (codec == "bgr24") return "raw_bgr24";
	return codec;
}

std::string TcpCommThread::resolvedDdsTopic(const std::string& codec) const
{
	const bool coarse = LowerAscii(m_channel) == "coarse";
	if (codec == "h264") return coarse ? m_ddsConfig.topicH264Coarse : m_ddsConfig.topicH264Precise;
	if (codec == "raw_bgr24") return coarse ? m_ddsConfig.topicRawBgr24Coarse : m_ddsConfig.topicRawBgr24Precise;
	return coarse ? m_ddsConfig.topicRawGray8Coarse : m_ddsConfig.topicRawGray8Precise;
}

bool TcpCommThread::tcpWantsH264() const
{
	std::lock_guard<std::mutex> lock(m_codecMtx);
	return m_h264Requested.load() && m_codecConfig != "jpeg" &&
		m_h264EncoderConfig != "jpeg";
}

void TcpCommThread::requestEncoderKeyFrame(const char* reason)
{
	m_encoderKeyFrameRequested.store(true);
	std::cout << "[VideoEncoder] channel=" << m_channel
		<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
		<< " pid=" << _getpid()
		<< " requestKeyFrame=1 reason=" << (reason ? reason : "unspecified") << std::endl;
}

bool TcpCommThread::encodeFrame(
	const RawVideoFrame& rawFrame,
	EncodedVideoFrame& encodedFrame,
	std::string& requestedCodec,
	std::string& requestedBackend,
	const std::string& forcedCodec,
	bool allowJpegFallback)
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
		m_jpegEncoderConfigured = false;
		m_h264EncoderConfigured = false;
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

	const bool h264Requested = forcedCodec == "h264" ||
		(forcedCodec != "jpeg" && m_h264Requested.load() &&
			codecConfig != "jpeg" && encoderConfig != "jpeg");
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
		else if (encoderConfig == "mpp" || encoderConfig == "rk_mpp")
		{
			fallbackReason = "mpp_backend_unavailable_on_windows";
		}
		else if (!(encoderConfig.empty() || encoderConfig == "auto" ||
			encoderConfig == "ffmpeg" || encoderConfig == "libavcodec"))
		{
			fallbackReason = "requested_encoder_not_integrated:" + encoderConfig;
		}
		else if (!m_h264Encoder->isAvailable())
		{
			fallbackReason = "ffmpeg_sdk_or_h264_encoder_unavailable";
		}
		else
		{
			const bool configChanged = !m_h264EncoderConfigured ||
				m_h264EncoderRuntimeConfig.width != config.width ||
				m_h264EncoderRuntimeConfig.height != config.height ||
				m_h264EncoderRuntimeConfig.fps != config.fps ||
				m_h264EncoderRuntimeConfig.bitrateKbps != config.bitrateKbps ||
				m_h264EncoderRuntimeConfig.gopFrames != config.gopFrames ||
				m_h264EncoderRuntimeConfig.lowLatency != config.lowLatency;
			if (configChanged)
			{
				m_h264EncoderConfigured = m_h264Encoder->configure(config, error);
				m_h264EncoderRuntimeConfig = config;
				if (m_h264EncoderConfigured)
				{
					std::cout << "[VideoEncoder] channel=" << m_channel
						<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
						<< " pid=" << _getpid()
						<< " configured=1 activeCodec=h264_annexb"
						<< " requestedBackend=" << requestedBackend
						<< " activeBackend=ffmpeg"
						<< " encoderName=" << m_h264Encoder->name()
						<< " size=" << config.width << "x" << config.height
						<< " fps=" << config.fps
						<< " bitrateKbps=" << config.bitrateKbps
						<< " gop=" << config.gopFrames
						<< " bFrames=0 pixelFormat=yuv420p annexB=1" << std::endl;
				}
			}
			if (!m_h264EncoderConfigured)
			{
				fallbackReason = error.empty() ? "ffmpeg_h264_configure_failed" : error;
			}
			else
			{
				if (m_encoderKeyFrameRequested.exchange(false))
				{
					m_h264Encoder->requestKeyFrame();
				}
				if (m_h264Encoder->encode(rawFrame, encodedFrame, error))
				{
					return true;
				}
				fallbackReason = error.empty() ? "ffmpeg_h264_encode_failed" : error;
				m_encoderKeyFrameRequested.store(true);
			}
		}
	}

	if (h264Requested && (!allowJpegFallback || !m_h264FallbackToJpeg.load()))
	{
		if (fallbackReason != m_lastCodecFallbackReason)
		{
			std::cerr << "[CodecFallback] channel=" << m_channel
				<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
				<< " pid=" << _getpid()
				<< " allowed=0 action=drop requestedCodec=h264"
				<< " requestedBackend=" << requestedBackend
				<< " activeBackend=none reason=" << fallbackReason << std::endl;
			m_lastCodecFallbackReason = fallbackReason;
		}
		return false;
	}

	const bool jpegConfigChanged = !m_jpegEncoderConfigured ||
		m_jpegEncoderConfig.width != config.width ||
		m_jpegEncoderConfig.height != config.height ||
		m_jpegEncoderConfig.jpegQuality != config.jpegQuality ||
		m_jpegEncoderConfig.jpegGray != config.jpegGray;
	if (jpegConfigChanged)
	{
		m_jpegEncoderConfigured = m_jpegEncoder->configure(config, error);
		m_jpegEncoderConfig = config;
	}
	if (!m_jpegEncoderConfigured || !m_jpegEncoder->encode(rawFrame, encodedFrame, error))
	{
		std::cerr << "[VideoEncoder][ERROR] channel=" << m_channel
			<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
			<< " pid=" << _getpid()
			<< " activeCodec=jpeg reason=" << error << std::endl;
		return false;
	}
	if (h264Requested)
	{
		encodedFrame.fallbackReason = fallbackReason.empty() ? "h264_encoder_unavailable" : fallbackReason;
		if (encodedFrame.fallbackReason != m_lastCodecFallbackReason)
		{
			std::cout << "[CodecFallback] channel=" << m_channel
				<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
				<< " pid=" << _getpid()
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
		// A running DDS/recording round owns every queued frame. Counter resets
		// must not turn into the legacy async latest-frame overwrite path.
		if (!m_outputRoundActive.load())
			m_frameQueue.clear();
		m_tcpPacketCounter.store(0);
		m_videoOutputFrameCounter.store(0);
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

// 建立连接
bool TcpCommThread::connectToServer() {
	disconnectFromServer(); // 确保旧的 socket 被彻底清理

	m_tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_tcpSocket == INVALID_SOCKET) return false;

	u_long nonBlocking = 1;
	ioctlsocket(m_tcpSocket, FIONBIO, &nonBlocking);
	const int connectResult = connect(m_tcpSocket, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr));
	if (connectResult == SOCKET_ERROR) {
		const int connectError = WSAGetLastError();
		if (connectError != WSAEWOULDBLOCK && connectError != WSAEINPROGRESS && connectError != WSAEINVAL)
		{
			closesocket(m_tcpSocket);
			m_tcpSocket = INVALID_SOCKET;
			return false;
		}
		fd_set writeSet;
		FD_ZERO(&writeSet);
		FD_SET(m_tcpSocket, &writeSet);
		timeval timeout = { 0, 20000 };
		if (select(0, nullptr, &writeSet, nullptr, &timeout) <= 0)
		{
			closesocket(m_tcpSocket);
			m_tcpSocket = INVALID_SOCKET;
			return false;
		}
		int socketError = 0;
		int socketErrorLength = sizeof(socketError);
		if (getsockopt(m_tcpSocket, SOL_SOCKET, SO_ERROR,
			reinterpret_cast<char*>(&socketError), &socketErrorLength) == SOCKET_ERROR || socketError != 0)
		{
			closesocket(m_tcpSocket);
			m_tcpSocket = INVALID_SOCKET;
			return false;
		}
	}
	nonBlocking = 0;
	if (ioctlsocket(m_tcpSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
		closesocket(m_tcpSocket);
		m_tcpSocket = INVALID_SOCKET;
		return false;
	}
	return true;
}

// 安全断开连接
void TcpCommThread::disconnectFromServer() {
	std::lock_guard<std::mutex> socketLock(m_socketMtx);
	if (m_tcpSocket != INVALID_SOCKET) {
		closesocket(m_tcpSocket);
		m_tcpSocket = INVALID_SOCKET;
	}
	m_bIsConnected = false;
}

bool TcpCommThread::sendAll(const char* data, int size)
{
	std::lock_guard<std::mutex> socketLock(m_socketMtx);
	if (!m_bIsConnected || m_tcpSocket == INVALID_SOCKET)
	{
		return false;
	}

	int totalSent = 0;
	while (totalSent < size)
	{
		const int sent = send(m_tcpSocket, data + totalSent, size - totalSent, 0);
		if (sent <= 0)
		{
			return false;
		}
		totalSent += sent;
	}
	return true;
}

bool TcpCommThread::sendStruct(const void* structPtr, uint32_t structSize)
{
	if (structPtr == nullptr || structSize == 0)
	{
		return false;
	}

	// 单结构体包格式：[总长度][结构体长度][结构体数据]，用于转发 0x36 初始化和 0x41 控制命令。
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
		std::cerr << "[TcpControlForward][WARN] skipped=1 reason=tcp_not_connected"
			<< " ddsVideoEnabled=" << (m_ddsConfig.enabled ? 1 : 0)
			<< " simCommand=" << cmd.simCommand << std::endl;
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
		<< ",\"frameSeq\":" << outputOrdinal
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
		<< ",\"h264EncoderName\":\"" << JsonEscape(encodedFrame.encoderName) << "\""
		<< ",\"h264BitrateKbps\":" << m_h264BitrateKbps.load()
		<< ",\"h264GopFrames\":" << m_h264GopFrames.load()
		<< ",\"h264LowLatency\":" << (m_h264LowLatency.load() ? "true" : "false")
		<< ",\"keyFrame\":" << (encodedFrame.keyFrame ? "true" : "false")
		<< ",\"ptsMs\":" << encodedFrame.ptsMs
		<< ",\"encodedBytes\":" << encodedFrame.payload.size()
		<< ",\"frameTimeMs\":" << (telemetry.udpReceiveTimeNs > 0 ? telemetry.udpReceiveTimeNs / 1000000LL : static_cast<std::int64_t>(outputOrdinal * 16))
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

void TcpCommThread::sendFrameThreadFunc() {
	std::cout << "TCP发送后台线程已启动..." << std::endl;
	EncodedVideoFrame encodedFrame;
	EncodedVideoFrame h264Frame;
	EncodedVideoFrame jpegFrame;
	auto nextTcpConnectAttempt = std::chrono::steady_clock::now();

	while (m_bIsRunning) {
		const bool independentOutput = m_ddsEnabled.load() ||
			(m_localRecorder && m_localRecorder->effectiveEnabled());
		if (!m_bIsConnected && std::chrono::steady_clock::now() >= nextTcpConnectAttempt) {
			nextTcpConnectAttempt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
			if (connectToServer()) {
				m_bIsConnected = true;
				requestEncoderKeyFrame("tcp_connected");
				std::cout << "TCP成功连接到服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
			}
			else if (!independentOutput) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}
		if (!m_bIsConnected && !independentOutput) continue;

		PendingFrame frame;
		int queueDepth = 0;
		{
			std::unique_lock<std::mutex> lock(m_frameMtx);
			if (m_frameCv.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return !m_frameQueue.empty() || !m_bIsRunning; })) {
				if (!m_bIsRunning) break;
				frame = std::move(m_frameQueue.front());
				m_frameQueue.pop_front();
				if (frame.roundActive) ++m_roundFramesInFlight;
				queueDepth = static_cast<int>(m_frameQueue.size());
				m_queueSpaceCv.notify_one();
			}
			else {
				if (m_bIsConnected && m_tcpSocket != INVALID_SOCKET) {
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(m_tcpSocket, &readSet);
					timeval tv = { 0, 0 };

					if (select(0, &readSet, NULL, NULL, &tv) > 0) {
						char dummy;
						int peekRet = recv(m_tcpSocket, &dummy, 1, MSG_PEEK);
						if (peekRet == 0 || (peekRet == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
							std::cerr << "TCP连接丢失(后台心跳检测)，准备重连..." << std::endl;
							disconnectFromServer();
						}
					}
				}
				continue;
			}
		}
		ScopeExit frameCompletion([this, &frame] {
			if (frame.roundActive)
			{
				m_roundLastCompletedFrame.store(frame.logicalFrameSeq);
				--m_roundFramesInFlight;
				m_roundDrainCv.notify_all();
			}
		});

		const int packetVersion = m_packetVersion.load();
		const bool includeVideo = packetVersion == 2 || m_sendVideo.load();
		const bool includeAnnotation = packetVersion == 2 || m_sendAnnotation.load();
		const bool includeRealtime = packetVersion == 2 || m_sendRealtimeData.load();
		if (!includeVideo && !includeAnnotation && !includeRealtime && !independentOutput)
		{
			if (!m_allPayloadDisabledWarned.exchange(true))
			{
				std::cout << "[TcpPayloadConfig][WARN]"
					<< " channel=" << m_channel
					<< " platID=" << m_localPlatID
					<< " sensorID=" << m_localSensorID
					<< " pid=" << _getpid()
					<< " packetVersion=3 action=no_frame_packet"
					<< " reason=all_frame_sections_disabled"
					<< std::endl;
			}
			continue;
		}
		if ((includeVideo || independentOutput) && frame.pixels.empty())
		{
			continue;
		}

		const std::uint64_t productOrdinal = ++m_videoOutputFrameCounter;
		RawVideoFrame rawFrame;
		rawFrame.data = frame.pixels.data();
		rawFrame.width = frame.width;
		rawFrame.height = frame.height;
		rawFrame.stride = frame.width * 3;
		// Preserve the legacy cv::imencode colour interpretation.
		rawFrame.pixelFormat = RawVideoPixelFormat::Bgr24;
		rawFrame.flipVertical = m_flipVertical.load();
		const std::uint64_t logicalFrameSeq = frame.logicalFrameSeq > 0
			? frame.logicalFrameSeq : productOrdinal;
		rawFrame.ptsMs = frame.logicalFrameSeq > 0
			? static_cast<std::int64_t>((frame.logicalFrameSeq - 1ULL) * 1000ULL /
				std::max(1, m_videoFps.load()))
			: (frame.telemetry.udpReceiveTimeNs > 0
				? frame.telemetry.udpReceiveTimeNs / 1000000LL
				: static_cast<std::int64_t>(productOrdinal * 1000ULL /
					std::max(1, m_videoFps.load())));

		const bool tcpH264 = includeVideo && tcpWantsH264();
		// DDS video belongs to an active simulation round.  Keep the middleware and
		// writer alive between rounds, but STOP/RESET must not produce idle samples.
		const bool ddsRoundActive = m_ddsEnabled.load() && frame.roundActive;
		const std::string ddsCodec = ddsRoundActive ? resolvedDdsCodec() : "none";
		const bool ddsH264 = ddsCodec == "h264";
		const bool ddsRaw = ddsCodec == "raw_gray8" || ddsCodec == "raw_bgr24";
		const bool recording = frame.roundActive && m_localRecorder &&
			m_localRecorder->effectiveEnabled();
		const bool needH264 = tcpH264 || ddsH264 || (recording && m_localRecorder->wantsH264());
		bool needJpeg = includeVideo && !tcpH264;
		if (ddsRoundActive)
		{
			if (!ddsH264 && !ddsRaw)
			{
				std::cerr << "[DdsVideo][FATAL] unsupported codec=" << ddsCodec << std::endl;
				continue;
			}
			bool topicChanged = false;
			std::string topicError;
			const std::string topic = resolvedDdsTopic(ddsCodec);
			if (!m_ddsPublisher->configureTopic(topic, &topicChanged, topicError))
			{
				std::cerr << "[DdsVideo][FATAL] configureTopic failed topic=" << topic
					<< " reason=" << topicError << std::endl;
				continue;
			}
			if (topicChanged && ddsH264) requestEncoderKeyFrame("dds_topic_changed");
		}

		std::string h264RequestedCodec = "none";
		std::string h264RequestedBackend = "none";
		std::string jpegRequestedCodec = "none";
		std::string jpegRequestedBackend = "none";
		h264Frame.clearForReuse();
		jpegFrame.clearForReuse();
		bool h264Ok = false;
		if (needH264)
		{
			h264Frame.ptsMs = rawFrame.ptsMs;
			h264Ok = encodeFrame(rawFrame, h264Frame, h264RequestedCodec,
				h264RequestedBackend, "h264", false);
			if (h264Ok) ++m_h264EncodeCounter;
			else if (ddsH264)
			{
				std::cerr << "[DdsVideo][FATAL] codec=h264 publishSkipped=1 reason=h264_encode_failed"
					<< std::endl;
			}
		}
		if (tcpH264 && !h264Ok && m_h264FallbackToJpeg.load()) needJpeg = true;
		bool jpegOk = false;
		if (needJpeg)
		{
			jpegFrame.ptsMs = rawFrame.ptsMs;
			jpegOk = encodeFrame(rawFrame, jpegFrame, jpegRequestedCodec,
				jpegRequestedBackend, "jpeg", true);
			if (jpegOk) ++m_jpegEncodeCounter;
		}

		std::string ddsError;
		double ddsBackpressureMs = 0.0;
		bool ddsVideoPublished = false;
		if (ddsH264 && h264Ok)
		{
			if (!m_ddsPublisher->publishBytes(h264Frame.payload.data(), h264Frame.payload.size(),
				&ddsBackpressureMs, ddsError))
			{
				std::cerr << "[DdsVideo][FATAL] codec=h264 publishSkipped=1 reason=" << ddsError << std::endl;
			}
			else { ++m_ddsFrameCounter; ddsVideoPublished = true; }
		}
		else if (ddsRaw)
		{
			const auto prepBegin = std::chrono::steady_clock::now();
			const bool gray = ddsCodec == "raw_gray8";
			const std::size_t rowBytes = static_cast<std::size_t>(frame.width) * (gray ? 1u : 3u);
			m_ddsRawBuffer.resize(rowBytes * static_cast<std::size_t>(frame.height));
			for (int y = 0; y < frame.height; ++y)
			{
				const int srcY = rawFrame.flipVertical ? (frame.height - 1 - y) : y;
				const std::uint8_t* src = frame.pixels.data() +
					static_cast<std::size_t>(srcY) * static_cast<std::size_t>(frame.width) * 3u;
				std::uint8_t* dst = m_ddsRawBuffer.data() + static_cast<std::size_t>(y) * rowBytes;
				if (!gray) std::memcpy(dst, src, rowBytes);
				else for (int x = 0; x < frame.width; ++x)
				{
					const unsigned int b = src[x * 3 + 0];
					const unsigned int g = src[x * 3 + 1];
					const unsigned int r = src[x * 3 + 2];
					dst[x] = static_cast<std::uint8_t>((29u * b + 150u * g + 77u * r + 128u) >> 8);
				}
			}
			const double prepMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - prepBegin).count();
			if (productOrdinal <= 3 || (productOrdinal % 120) == 0)
				std::cout << "[DdsRawPrep] format=" << (gray ? "gray8" : "bgr24")
					<< " width=" << frame.width << " height=" << frame.height
					<< " bytes=" << m_ddsRawBuffer.size() << " prepMs=" << prepMs << std::endl;
			if (!m_ddsPublisher->publishBytes(m_ddsRawBuffer.data(), m_ddsRawBuffer.size(),
				&ddsBackpressureMs, ddsError))
				std::cerr << "[DdsVideo][FATAL] codec=" << ddsCodec << " publishSkipped=1 reason="
					<< ddsError << std::endl;
			else { ++m_ddsFrameCounter; ddsVideoPublished = true; }
		}

		if (recording)
		{
			std::string recordError;
			double recordBackpressureMs = 0.0;
			bool recordOk = false;
			if (m_localRecorder->wantsH264() && h264Ok)
				recordOk = m_localRecorder->enqueueH264(h264Frame.payload.data(), h264Frame.payload.size(),
					h264Frame.keyFrame, frame.width, frame.height, &recordBackpressureMs, recordError);
			else if (!m_localRecorder->wantsH264())
				recordOk = m_localRecorder->enqueueRawBgr24(frame.pixels.data(), frame.width, frame.height,
					rawFrame.flipVertical, &recordBackpressureMs, recordError);
			else recordError = "shared H264 encode failed";
			if (!recordOk) std::cerr << "[LocalRecording][ERROR] reason=" << recordError << std::endl;
			else ++m_recordFrameCounter;
		}

		encodedFrame.clearForReuse();
		std::string requestedCodec = "none";
		std::string requestedBackend = "none";
		if (includeVideo)
		{
			if (tcpH264 && h264Ok)
			{
				encodedFrame = h264Frame;
				requestedCodec = h264RequestedCodec;
				requestedBackend = h264RequestedBackend;
			}
			else if (jpegOk)
			{
				encodedFrame = jpegFrame;
				requestedCodec = tcpH264 ? "h264" : jpegRequestedCodec;
				requestedBackend = tcpH264 ? h264RequestedBackend : jpegRequestedBackend;
				if (tcpH264) encodedFrame.fallbackReason = "h264_encode_failed";
			}
			else if (m_bIsConnected) continue;
		}
		if (productOrdinal <= 3 || (productOrdinal % 120) == 0)
		{
			std::cout << "[VideoOutputProducts] frame=" << productOrdinal
				<< " needH264=" << (needH264 ? 1 : 0)
				<< " h264EncodeCount=" << (h264Ok ? 1 : 0)
				<< " needJpeg=" << (needJpeg ? 1 : 0)
				<< " jpegEncodeCount=" << (jpegOk ? 1 : 0)
				<< " tcpH264=" << (tcpH264 && h264Ok ? 1 : 0)
				<< " ddsH264=" << (ddsH264 && h264Ok ? 1 : 0)
				<< " ddsRaw=" << (ddsRaw ? 1 : 0)
				<< " localRecordH264=" << (recording && m_localRecorder->wantsH264() && h264Ok ? 1 : 0)
				<< std::endl;
		}

#if defined(HWASIMIR_HAS_ZRDDS)
		if (ddsVideoPublished && m_pHwaSimIR)
		{
			const EncodedVideoFrame& annotationEncodedFrame = ddsH264 ? h264Frame : encodedFrame;
			const std::string ddsAnnotationJson = frame.annotationEnabled
				? buildAnnotationJson(frame.annotationRecord, true, frame.width, frame.height,
					frame.telemetry, logicalFrameSeq, IRPerfStats::wallTimeNs(), packetVersion,
					ddsCodec, ddsH264 ? h264RequestedBackend : "raw", annotationEncodedFrame)
				: std::string();
			DdsVideoFrameMeta meta;
			meta.platID = m_localPlatID;
			meta.sensorID = m_localSensorID;
			meta.channel = m_channel;
			meta.frameSeq = static_cast<std::uint32_t>(logicalFrameSeq);
			meta.currentRound = frame.currentRound;
			meta.ptsMs = static_cast<double>(rawFrame.ptsMs);
			meta.keyFrame = ddsH264 && h264Frame.keyFrame;
			meta.codec = ddsCodec;
			meta.width = frame.width;
			meta.height = frame.height;
			DdsAnnotationFrame annotation;
			DdsAnnotationFrame* annotationPtr = nullptr;
			if (frame.annotationEnabled)
			{
				annotation.platID = m_localPlatID;
				annotation.sensorID = m_localSensorID;
				annotation.channel = m_channel;
				annotation.frameSeq = meta.frameSeq;
				annotation.currentRound = frame.currentRound;
				annotation.ptsMs = meta.ptsMs;
				annotation.json = ddsAnnotationJson;
				annotationPtr = &annotation;
			}
			std::string auxError;
			if (!m_pHwaSimIR->PublishDdsFrameProducts(meta, annotationPtr, auxError))
			{
				std::cerr << "[DdsFrameProducts][FATAL] round=" << frame.currentRound
					<< " frameSeq=" << logicalFrameSeq << " reason=" << auxError << std::endl;
			}
		}
#endif

		if (!m_bIsConnected) continue;
		const std::uint64_t outputOrdinal = ++m_tcpPacketCounter;
		const std::int64_t tcpSendTimeNs = IRPerfStats::wallTimeNs();
		const std::string annotationJson = includeAnnotation
			? buildAnnotationJson(
				frame.annotationRecord,
				frame.annotationEnabled,
				frame.width,
				frame.height,
				frame.telemetry,
				logicalFrameSeq,
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
				<< " sensorID=" << m_localSensorID << " pid=" << _getpid()
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
			logicalFrameSeq,
			outputOrdinal,
			rawFrame.ptsMs,
			sectionFlags,
			realtimeBytes,
			annotationBytes,
			videoBytes)) {
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
				<< " sensorID=" << m_localSensorID << " pid=" << _getpid()
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
				<< " encoderName=" << encodedFrame.encoderName
				<< " h264EncoderName=" << encodedFrame.encoderName
				<< " realtimeBytes=" << realtimeBytes
				<< " annotationBytes=" << annotationBytes
				<< " videoBytes=" << videoBytes
				<< " frameSeq=" << logicalFrameSeq
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
			const double jpegMs = h264Active ? 0.0 : encodedFrame.encodeMs;
			std::ostringstream perfLine;
			perfLine << std::fixed << std::setprecision(3)
				<< "[TcpPerf]"
				<< " channel=" << m_channel
				<< " platID=" << m_localPlatID
				<< " sensorID=" << m_localSensorID
				<< " pid=" << _getpid()
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
				<< " h264BitrateKbps=" << m_h264BitrateKbps.load()
				<< " h264GopFrames=" << m_h264GopFrames.load()
				<< " h264EncodeMs=" << (h264Active ? encodedFrame.encodeMs : 0.0)
				<< " encodeMs=" << encodedFrame.encodeMs
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
				<< " jpegMs=" << jpegMs
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
					<< " pid=" << _getpid()
					<< " outputOrdinal=" << outputOrdinal
					<< " encodeMs=" << encodedFrame.encodeMs
					<< " payloadBytes=" << encodedFrame.payload.size()
					<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
					<< " queueDepth=" << queueDepth
					<< " tcpSendMs=" << tcpSendMs << std::endl;
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
// 将主线程传递过来的图像数据拷贝到子线程缓冲区
void TcpCommThread::updateFrame(const uchar* data, int width, int height) {
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
	const bool recordingNoDrop = m_localRecorder && m_localRecorder->effectiveEnabled();
	const bool noDropOutputRequired = m_ddsEnabled.load() || recordingNoDrop;
	if (m_syncMode.load() || noDropOutputRequired)
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
	if (result.queueWaitMs > 1.0 && noDropOutputRequired)
	{
		std::cout << "[OutputBackpressure] queueDepth=" << m_frameQueue.size()
			<< " waitMs=" << result.queueWaitMs
			<< " ddsEnabled=" << (m_ddsEnabled.load() ? 1 : 0)
			<< " recordingEnabled=" << (recordingNoDrop ? 1 : 0) << std::endl;
	}

	const auto copyBegin = std::chrono::steady_clock::now();
	PendingFrame frame;
	const bool copyVideo =
		m_packetVersion.load() == 2 ||
		m_sendVideo.load() ||
		m_ddsEnabled.load() ||
		recordingNoDrop;
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
	frame.roundActive = m_outputRoundActive.load();
	frame.currentRound = frame.roundActive ? m_outputRound.load() : 0;
	frame.logicalFrameSeq = frame.roundActive ? ++m_roundFrameSequence : 0;
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
