#include "TcpCommThread_Linux.h" // 根据你实际的文件名包含头文件
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>
#include <errno.h> // 提供 errno

TcpCommThread::TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort)
	: m_pHwaSimIR(hwaSimIR), m_tcpSocket(-1), m_bIsRunning(false), m_bIsConnected(false), m_serverIp(serverIp), m_serverPort(serverPort) {
	// 初始化服务器地址
	memset(&m_serverAddr, 0, sizeof(m_serverAddr));
	m_serverAddr.sin_family = AF_INET;
	m_serverAddr.sin_port = htons(serverPort);
	inet_pton(AF_INET, serverIp.c_str(), &m_serverAddr.sin_addr);
}

TcpCommThread::~TcpCommThread() {
	stop();
	// Linux 下不需要 WSACleanup()
}

bool TcpCommThread::start() {
	if (m_bIsRunning) return true;

	// Linux 下不需要 WSAStartup
	// 启动发送线程
	m_bIsRunning = true;
	m_bIsConnected = false;
	m_sendThread = std::thread(&TcpCommThread::sendFrameThreadFunc, this);

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
	if (m_tcpSocket == -1) return false;

	if (connect(m_tcpSocket, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) == -1) {
		close(m_tcpSocket);
		m_tcpSocket = -1;
		return false;
	}
	return true;
}

// 安全断开连接
void TcpCommThread::disconnectFromServer() {
	if (m_tcpSocket != -1) {
		close(m_tcpSocket);
		m_tcpSocket = -1;
	}
	m_bIsConnected = false;
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
				m_bNewFrame = false;
			}
			else {
				// 主动检测 TCP 连接状态，防止对方断开而本端不知情
				if (m_bIsConnected && m_tcpSocket != -1) {
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(m_tcpSocket, &readSet);
					struct timeval tv = { 0, 0 }; // 非阻塞探测

												  // 注意：Linux下的select第一个参数必须是最大文件描述符 + 1
					if (select(m_tcpSocket + 1, &readSet, NULL, NULL, &tv) > 0) {
						char dummy;
						// 探测性读取 1 字节数据
						int peekRet = recv(m_tcpSocket, &dummy, 1, MSG_PEEK);
						// 如果返回 0 (优雅断开) 或产生非阻塞异常 (强制断开)
						if (peekRet == 0 || (peekRet == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
							std::cerr << "TCP连接丢失(后台心跳检测)，准备重连..." << std::endl;
							disconnectFromServer();
						}
					}
				}
				continue; // 探测完毕，进入下一轮循环继续等待
			}
		}

		if (localBuffer.empty()) continue;

		// --- 3. 图像处理 (翻转与自适应缩放) ---
		// 将底层裸数据映射为 OpenCV Mat
		cv::Mat rawFrame(height, width, CV_8UC3, localBuffer.data());
		cv::Mat flippedFrame;

		// 沿 X 轴翻转
		cv::flip(rawFrame, flippedFrame, 0);

		// 如果拖动边缘导致分辨率轻微变形，强制缩放回服务端期望的 800x800
		if (width != 800 || height != 800) {
			cv::resize(flippedFrame, flippedFrame, cv::Size(800, 800));
		}

		// --- 4. JPEG 压缩编码 ---
		std::vector<uchar> jpegData;
		std::vector<int> params = { cv::IMWRITE_JPEG_QUALITY, 80 };
		if (!cv::imencode(".jpg", flippedFrame, jpegData, params)) {
			continue;
		}

		// --- 5. 网络发送（包含断线检测） ---
		uint32_t frameLength = jpegData.size();
		char header[4];
		header[0] = (frameLength >> 24) & 0xFF;
		header[1] = (frameLength >> 16) & 0xFF;
		header[2] = (frameLength >> 8) & 0xFF;
		header[3] = frameLength & 0xFF;

		// Linux下使用 MSG_NOSIGNAL 防止发送到已断开的socket时触发SIGPIPE信号导致程序崩溃
		int sent = send(m_tcpSocket, header, 4, MSG_NOSIGNAL);
		if (sent <= 0) {
			std::cerr << "TCP连接丢失(发送头报错)，准备重连..." << std::endl;
			disconnectFromServer();
			continue;
		}

		sent = send(m_tcpSocket, reinterpret_cast<const char*>(jpegData.data()), static_cast<int>(jpegData.size()), MSG_NOSIGNAL);
		if (sent <= 0) {
			std::cerr << "TCP连接丢失(发送数据报错)，准备重连..." << std::endl;
			disconnectFromServer();
			continue;
		}
	}
	std::cout << "TCP发送后台线程安全退出" << std::endl;
}

// 将主线程传递过来的图像数据拷贝到子线程缓冲区
void TcpCommThread::updateFrame(const uchar* data, int width, int height) {
	std::lock_guard<std::mutex> lock(m_frameMtx);
	size_t size = width * height * 3;

	if (m_frameBuffer.size() != size) {
		m_frameBuffer.resize(size);
	}

	// 拷贝原始像素
	memcpy(m_frameBuffer.data(), data, size);
	m_frameWidth = width;
	m_frameHeight = height;
	m_bNewFrame = true;

	// 唤醒处于等待状态的 TCP 发送线程
	m_frameCv.notify_one();
}