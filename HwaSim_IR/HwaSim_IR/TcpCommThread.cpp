#include "TcpCommThread.h"
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <process.h>
//#include <core.h>

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
}

TcpCommThread::TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort,
	const std::string& channel, int localPlatID, int localSensorID)
	: m_pHwaSimIR(hwaSimIR), m_channel(channel), m_localPlatID(localPlatID), m_localSensorID(localSensorID),
	m_tcpSocket(INVALID_SOCKET), m_bIsRunning(false), m_bIsConnected(false), m_serverIp(serverIp), m_serverPort(serverPort) {
	m_jpegEncoder.reset(new JpegFrameEncoder());
	m_h264Encoder.reset(new H264FfmpegEncoder());
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

	// 【修改】只初始化 WSA，不在这里 connect。如果没连上，也不妨碍线程启动
	if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0) {
		std::cerr << "WSAStartup失败" << std::endl;
		return false;
	}

	// 启动发送线程
	m_bIsRunning = true;
	m_bIsConnected = false;
	m_sendThread = std::thread(&TcpCommThread::sendFrameThreadFunc, this);

	//std::cout << "TCP通讯线程启动成功，连接服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
	return true;
}

void TcpCommThread::stop() {
	if (!m_bIsRunning) return;
	m_bIsRunning = false;
	m_frameCv.notify_all(); // 唤醒可能阻塞在等新帧的线程
	m_queueSpaceCv.notify_all();

	// 等待线程退出
	if (m_sendThread.joinable()) {
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
		<< " encoderBackend=ffmpeg"
		<< " videoFps=" << m_videoFps.load()
		<< std::endl;
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
	std::string& requestedCodec)
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

	const bool h264Requested = m_h264Requested.load() && codecConfig != "jpeg";
	requestedCodec = h264Requested ? "h264" : "jpeg";
	std::string fallbackReason;
	std::string error;

	if (h264Requested)
	{
		if (!m_enableH264Experimental.load())
		{
			fallbackReason = "h264_backend_disabled";
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

	if (h264Requested && !m_h264FallbackToJpeg.load())
	{
		if (fallbackReason != m_lastCodecFallbackReason)
		{
			std::cerr << "[CodecFallback] channel=" << m_channel
				<< " platID=" << m_localPlatID << " sensorID=" << m_localSensorID
				<< " pid=" << _getpid()
				<< " allowed=0 action=drop requestedCodec=h264 reason=" << fallbackReason << std::endl;
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

// 建立连接
bool TcpCommThread::connectToServer() {
	disconnectFromServer(); // 确保旧的 socket 被彻底清理

	m_tcpSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_tcpSocket == INVALID_SOCKET) return false;

	if (connect(m_tcpSocket, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) == SOCKET_ERROR) {
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
	const std::vector<std::uint8_t>& encodedPayload)
{
	const uint32_t trackingLen = static_cast<uint32_t>(sizeof(BYHWICD::DisplayC2cObjTrackingData));
	const uint32_t annotationLen = static_cast<uint32_t>(annotationJson.size());
	const uint32_t payloadLen = static_cast<uint32_t>(encodedPayload.size());
	const uint32_t totalLen = 4 + 4 + trackingLen + 4 + annotationLen + 4 + payloadLen;

	std::vector<char> packet;
	packet.reserve(totalLen);
	AppendUint32BE(packet, totalLen);
	AppendUint32BE(packet, trackingLen);
	packet.insert(packet.end(), reinterpret_cast<const char*>(&trackingData), reinterpret_cast<const char*>(&trackingData) + trackingLen);
	AppendUint32BE(packet, annotationLen);
	packet.insert(packet.end(), annotationJson.begin(), annotationJson.end());
	AppendUint32BE(packet, payloadLen);
	packet.insert(packet.end(), reinterpret_cast<const char*>(encodedPayload.data()), reinterpret_cast<const char*>(encodedPayload.data()) + payloadLen);

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
	const std::string& requestedCodec,
	const EncodedVideoFrame& encodedFrame) const
{
	const int srcWidth = record.width > 0 ? record.width : tcpWidth;
	const int srcHeight = record.height > 0 ? record.height : tcpHeight;
	const unsigned long long frameIndex = record.frameIndex > 0 ? record.frameIndex : outputOrdinal;
	const std::string activeCodec = encodedFrame.payloadCodec.empty() ? "none" : encodedFrame.payloadCodec;
	const std::string fallbackReason = encodedFrame.fallbackReason.empty() ? "none" : encodedFrame.fallbackReason;

	std::ostringstream json;
	json << "{\"version\":1"
		<< ",\"packetVersion\":2"
		<< ",\"enabled\":" << (annotationEnabled ? "true" : "false")
		<< ",\"frameIndex\":" << frameIndex
		<< ",\"frameSeq\":" << telemetry.sourceSeq
		<< ",\"sourceSeq\":" << telemetry.sourceSeq
		<< ",\"outputOrdinal\":" << outputOrdinal
		<< ",\"udpReceiveTimeNs\":\"" << telemetry.udpReceiveTimeNs << "\""
		<< ",\"tcpSendTimeNs\":\"" << tcpSendTimeNs << "\""
		<< ",\"requestedCodec\":\"" << JsonEscape(requestedCodec) << "\""
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

	while (m_bIsRunning) {
		if (!m_bIsConnected) {
			if (connectToServer()) {
				m_bIsConnected = true;
				requestEncoderKeyFrame("tcp_connected");
				std::cout << "TCP成功连接到服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
			}
			else {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		PendingFrame frame;
		int queueDepth = 0;
		{
			std::unique_lock<std::mutex> lock(m_frameMtx);
			if (m_frameCv.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return !m_frameQueue.empty() || !m_bIsRunning; })) {
				if (!m_bIsRunning) break;
				frame = std::move(m_frameQueue.front());
				m_frameQueue.pop_front();
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

		if (frame.pixels.empty()) continue;

		const std::uint64_t nextOutputOrdinal = m_tcpPacketCounter.load() + 1;
		RawVideoFrame rawFrame;
		rawFrame.data = frame.pixels.data();
		rawFrame.width = frame.width;
		rawFrame.height = frame.height;
		rawFrame.stride = frame.width * 3;
		// Preserve the legacy cv::imencode colour interpretation.
		rawFrame.pixelFormat = RawVideoPixelFormat::Bgr24;
		rawFrame.flipVertical = m_flipVertical.load();
		rawFrame.ptsMs = frame.telemetry.udpReceiveTimeNs > 0
			? frame.telemetry.udpReceiveTimeNs / 1000000LL
			: static_cast<std::int64_t>(nextOutputOrdinal * 1000ULL / std::max(1, m_videoFps.load()));

		std::string requestedCodec;
		if (!encodeFrame(rawFrame, encodedFrame, requestedCodec))
		{
			continue;
		}

		const std::uint64_t outputOrdinal = ++m_tcpPacketCounter;
		const std::int64_t tcpSendTimeNs = IRPerfStats::wallTimeNs();
		const std::string annotationJson = buildAnnotationJson(
			frame.annotationRecord,
			frame.annotationEnabled,
			frame.width,
			frame.height,
			frame.telemetry,
			outputOrdinal,
			tcpSendTimeNs,
			requestedCodec,
			encodedFrame);
		if (annotationJson.size() > 1024 * 1024)
		{
			std::cout << "[TcpFramePacket][WARN] annotationJsonTooLarge"
				<< " channel=" << m_channel << " platID=" << m_localPlatID
				<< " sensorID=" << m_localSensorID << " pid=" << _getpid()
				<< " frame=" << outputOrdinal
				<< " annotationBytes=" << annotationJson.size()
				<< std::endl;
		}

		const auto sendBegin = std::chrono::steady_clock::now();
		if (!sendFramePacket(frame.trackingData, annotationJson, encodedFrame.payload)) {
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
				<< " imgBytes=" << encodedFrame.payload.size()
				<< " payloadBytes=" << encodedFrame.payload.size()
				<< " packetVersion=2"
				<< " codec=" << encodedFrame.payloadCodec
				<< " activeCodec=" << encodedFrame.payloadCodec
				<< " keyFrame=" << (encodedFrame.keyFrame ? "1" : "0")
				<< " encoderName=" << encodedFrame.encoderName
				<< " h264EncoderName=" << encodedFrame.encoderName
				<< " annotationBytes=" << annotationJson.size()
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
				<< " codec=" << encodedFrame.payloadCodec
				<< " activeCodec=" << encodedFrame.payloadCodec
				<< " requestedCodec=" << requestedCodec
				<< " h264En=" << (m_h264Requested.load() ? "1" : "0")
				<< " codecFallbackReason=" << (encodedFrame.fallbackReason.empty() ? "none" : encodedFrame.fallbackReason)
				<< " encoderName=" << encodedFrame.encoderName
				<< " h264EncoderName=" << encodedFrame.encoderName
				<< " h264BitrateKbps=" << m_h264BitrateKbps.load()
				<< " h264GopFrames=" << m_h264GopFrames.load()
				<< " h264EncodeMs=" << (h264Active ? encodedFrame.encodeMs : 0.0)
				<< " encodeMs=" << encodedFrame.encodeMs
				<< " encodedBytes=" << encodedFrame.payload.size()
				<< " payloadBytes=" << encodedFrame.payload.size()
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
	const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
	frame.pixels.resize(size);
	memcpy(frame.pixels.data(), data, size);
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
