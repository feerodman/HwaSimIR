#include "TcpCommThread.h"
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
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

TcpCommThread::TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort)
	: m_pHwaSimIR(hwaSimIR), m_tcpSocket(INVALID_SOCKET), m_bIsRunning(false), m_bIsConnected(false), m_serverIp(serverIp), m_serverPort(serverPort) {
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

	// 等待线程退出
	if (m_sendThread.joinable()) {
		m_sendThread.join();
	}
	disconnectFromServer();
	std::cout << "TCP通讯线程已停止" << std::endl;
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
	std::cout << "[TcpInitForward] sensorID=" << initData.sensorID
		<< " platNumValid=" << initData.platNumValid << std::endl;
	return true;
}

bool TcpCommThread::sendFramePacket(
	const BYHWICD::DisplayC2cObjTrackingData& trackingData,
	const std::string& annotationJson,
	const std::vector<uchar>& jpegData)
{
	const uint32_t trackingLen = static_cast<uint32_t>(sizeof(BYHWICD::DisplayC2cObjTrackingData));
	const uint32_t annotationLen = static_cast<uint32_t>(annotationJson.size());
	const uint32_t jpegLen = static_cast<uint32_t>(jpegData.size());
	const uint32_t totalLen = 4 + 4 + trackingLen + 4 + annotationLen + 4 + jpegLen;

	std::vector<char> packet;
	packet.reserve(totalLen);
	AppendUint32BE(packet, totalLen);
	AppendUint32BE(packet, trackingLen);
	packet.insert(packet.end(), reinterpret_cast<const char*>(&trackingData), reinterpret_cast<const char*>(&trackingData) + trackingLen);
	AppendUint32BE(packet, annotationLen);
	packet.insert(packet.end(), annotationJson.begin(), annotationJson.end());
	AppendUint32BE(packet, jpegLen);
	packet.insert(packet.end(), reinterpret_cast<const char*>(jpegData.data()), reinterpret_cast<const char*>(jpegData.data()) + jpegLen);

	return sendAll(packet.data(), static_cast<int>(packet.size()));
}

std::string TcpCommThread::buildAnnotationJson(
	const AnnotationFrameRecord& record,
	bool annotationEnabled,
	int tcpWidth,
	int tcpHeight) const
{
	const int srcWidth = record.width > 0 ? record.width : tcpWidth;
	const int srcHeight = record.height > 0 ? record.height : tcpHeight;
	const unsigned long long frameIndex = record.frameIndex > 0 ? record.frameIndex : m_tcpPacketCounter;

	std::ostringstream json;
	json << "{\"version\":1"
		<< ",\"enabled\":" << (annotationEnabled ? "true" : "false")
		<< ",\"frameIndex\":" << frameIndex
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

	while (m_bIsRunning) {
		// --- 1. 断线自动重连逻辑 ---
		if (!m_bIsConnected) {
			if (connectToServer()) {
				m_bIsConnected = true;
				std::cout << "TCP成功连接到服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
			}
			else {
				// 连接失败，睡眠 1 秒后继续尝试
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		// --- 2. 提取新帧与状态检测 ---
		std::vector<uchar> localBuffer;
		BYHWICD::DisplayC2cObjTrackingData localTrackingData;
		AnnotationFrameRecord localAnnotationRecord;
		bool localAnnotationEnabled = false;
		int width = 0, height = 0;
		{
			std::unique_lock<std::mutex> lock(m_frameMtx);
			// 等待 100 毫秒
			if (m_frameCv.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return m_bNewFrame || !m_bIsRunning; })) {

				if (!m_bIsRunning) break;

				localBuffer = m_frameBuffer;
				width = m_frameWidth;
				height = m_frameHeight;
				localTrackingData = m_trackingData;
				localAnnotationRecord = m_annotationRecord;
				localAnnotationEnabled = m_annotationEnabled;
				m_bNewFrame = false;
			}
			else {
				// 【关键修复 1】：超时未收到新帧（例如正在拖动窗口阻塞了主线程）
				// 主动检测 TCP 连接状态，防止对方断开而本端不知情
				if (m_bIsConnected && m_tcpSocket != INVALID_SOCKET) {
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(m_tcpSocket, &readSet);
					timeval tv = { 0, 0 }; // 非阻塞探测

					if (select(0, &readSet, NULL, NULL, &tv) > 0) {
						char dummy;
						// 探测性读取 1 字节数据
						int peekRet = recv(m_tcpSocket, &dummy, 1, MSG_PEEK);
						// 如果返回 0 (优雅断开) 或产生非阻塞异常 (强制断开)
						if (peekRet == 0 || (peekRet == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)) {
							std::cerr << "TCP连接丢失(后台心跳检测)，准备重连..." << std::endl;
							disconnectFromServer();
						}
					}
				}
				continue; // 探测完毕，进入下一轮循环继续等待
			}
		}

		if (localBuffer.empty()) continue; // 去掉 800x800 的严格检查

										   // --- 3. 图像处理 (翻转与自适应缩放) ---
										   // 将底层裸数据映射为 OpenCV Mat
		cv::Mat rawFrame(height, width, CV_8UC3, localBuffer.data());
		cv::Mat flippedFrame;

		// 沿 X 轴翻转 (代替原先易越界的 memcpy)
		cv::flip(rawFrame, flippedFrame, 0);
		// Stage2B：TCP JPEG 仍沿用当前 flip 后图像；标注 JSON 坐标已是最终 JPEG 左上角坐标，不再二次翻转 y。

		// --- 4. JPEG 压缩编码 ---
		std::vector<uchar> jpegData;
		std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
		if (!cv::imencode(".jpg", flippedFrame, jpegData, params)) {
			continue;
		}

		++m_tcpPacketCounter;
		const std::string annotationJson = buildAnnotationJson(localAnnotationRecord, localAnnotationEnabled, width, height);
		if (annotationJson.size() > 1024 * 1024)
		{
			std::cout << "[TcpFramePacket][WARN] annotationJsonTooLarge"
				<< " frame=" << m_tcpPacketCounter
				<< " annotationBytes=" << annotationJson.size()
				<< std::endl;
		}

		// --- 5. 网络发送：trackingData + annotationJson + JPEG 三段式显示帧包 ---
		if (!sendFramePacket(localTrackingData, annotationJson, jpegData)) {
			std::cerr << "TCP连接丢失(发送帧包失败)，准备重连..." << std::endl;
			disconnectFromServer();
			continue;
		}

		if (m_tcpPacketCounter <= 3 || (m_tcpPacketCounter % 120) == 0)
		{
			std::cout << "[TcpFramePacket]"
				<< " frame=" << m_tcpPacketCounter
				<< " imgBytes=" << jpegData.size()
				<< " annotationBytes=" << annotationJson.size()
				<< " targets=" << localAnnotationRecord.targets.size()
				<< " width=" << width
				<< " height=" << height
				<< std::endl;
		}
	}
	std::cout << "TCP发送后台线程安全退出" << std::endl;
}
#if 0

void TcpCommThread::sendFrameThreadFunc() {
	std::cout << "TCP发送后台线程已启动..." << std::endl;

	while (m_bIsRunning) {
		// --- 1. 断线自动重连逻辑 ---
		if (!m_bIsConnected) {
			if (connectToServer()) {
				m_bIsConnected = true;
				std::cout << "TCP成功连接到服务器：" << m_serverIp << ":" << m_serverPort << std::endl;
			}
			else {
				// 连接失败，睡眠 1 秒后继续尝试，防止 CPU 飙升
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
		}

		// --- 2. 获取新帧数据 ---
		std::vector<uchar> localBuffer;
		int width = 0, height = 0;
		{
			std::unique_lock<std::mutex> lock(m_frameMtx);
			if (m_frameCv.wait_for(lock, std::chrono::milliseconds(100),
				[this] { return m_bNewFrame || !m_bIsRunning; })) {
				if (!m_bIsRunning) break;

				localBuffer = m_frameBuffer;
				width = m_frameWidth;
				height = m_frameHeight;
				m_bNewFrame = false;
			}
			else {
				continue; // 超时无数据，继续下一轮循环
			}
		}

		if (localBuffer.empty() || width != 800 || height != 800) continue;

		// --- 3. 翻转与编码 ---
		std::vector<uchar> flipped_data(width * height * 3);
		const uchar* src = localBuffer.data();
		uchar* dst = flipped_data.data();
		for (int y = 0; y < height; ++y) {
			memcpy(dst + (height - 1 - y) * width * 3, src + y * width * 3, width * 3);
		}

		cv::Mat frame(height, width, CV_8UC3, flipped_data.data());
		std::vector<uchar> jpegData;
		std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };

		if (!cv::imencode(".jpg", frame, jpegData, params)) {
			continue;
		}

		// --- 4. 网络发送（加入掉线检测） ---
		uint32_t frameLength = jpegData.size();
		char header[4];
		header[0] = (frameLength >> 24) & 0xFF;
		header[1] = (frameLength >> 16) & 0xFF;
		header[2] = (frameLength >> 8) & 0xFF;
		header[3] = frameLength & 0xFF;

		int sent = send(m_tcpSocket, header, 4, 0);
		if (sent <= 0) { // 发送失败，说明网络断开
			std::cerr << "TCP连接丢失，准备重连..." << std::endl;
			disconnectFromServer(); // 触发重连
			continue;
		}

		sent = send(m_tcpSocket, reinterpret_cast<const char*>(jpegData.data()), static_cast<int>(jpegData.size()), 0);
		if (sent <= 0) {
			std::cerr << "TCP连接丢失，准备重连..." << std::endl;
			disconnectFromServer(); // 触发重连
			continue;
		}
	}
	std::cout << "TCP发送后台线程安全退出" << std::endl;
}

#endif // 0

// 将主线程传递过来的图像数据拷贝到子线程缓冲区
void TcpCommThread::updateFrame(const uchar* data, int width, int height) {
	BYHWICD::DisplayC2cObjTrackingData trackingData;
	memset(&trackingData, 0, sizeof(trackingData));
	trackingData.flag = 0x38;
	AnnotationFrameRecord annotationRecord;
	updateFrame(data, width, height, trackingData, annotationRecord, false);
}

void TcpCommThread::updateFrame(
	const uchar* data,
	int width,
	int height,
	const BYHWICD::DisplayC2cObjTrackingData& trackingData,
	const AnnotationFrameRecord& annotationRecord,
	bool annotationEnabled)
{
	if (data == nullptr || width <= 0 || height <= 0)
	{
		return;
	}

	std::lock_guard<std::mutex> lock(m_frameMtx);
	size_t size = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;

	if (m_frameBuffer.size() != size) {
		m_frameBuffer.resize(size);
	}

	// 同一帧内一次性复制像素、实时数据和标注快照，后台 TCP 线程不访问 Panda3D 或 HwaSimIR 节点。
	memcpy(m_frameBuffer.data(), data, size);
	m_frameWidth = width;
	m_frameHeight = height;
	m_trackingData = trackingData;
	m_hasTrackingData = true;
	m_annotationRecord = annotationRecord;
	m_annotationEnabled = annotationEnabled;
	m_bNewFrame = true;

	// 唤醒处于等待状态的 TCP 发送线程
	m_frameCv.notify_one();
}
