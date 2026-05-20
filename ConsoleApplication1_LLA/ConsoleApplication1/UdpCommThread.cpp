#include "UdpCommThread.h"
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>


// 构造函数
UdpCommThread::UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
	const std::string& remoteIp, uint16_t remotePort)
	: m_pHwaSimIR(hwaSimIR), m_udpSocket(INVALID_SOCKET), m_bIsRunning(false), m_mtx()
{
	// 初始化本地地址
	memset(&m_localAddr, 0, sizeof(m_localAddr));
	m_localAddr.sin_family = AF_INET;
	m_localAddr.sin_port = htons(localPort);
	inet_pton(AF_INET, localIp.c_str(), &m_localAddr.sin_addr);

	// 初始化远端地址
	setRemoteAddr(remoteIp.c_str(), remotePort);
}

// 析构函数
UdpCommThread::~UdpCommThread()
{
	stop();
	destroySocket();
}

// 启动UDP线程
bool UdpCommThread::start()
{
	if (m_bIsRunning) return true;

	// 初始化Socket
	if (!initSocket())
	{
		std::cerr << "UDP Socket初始化失败！" << std::endl;
		return false;
	}

	// 启动接收线程
	m_bIsRunning = true;
	m_recvThread = std::thread(&UdpCommThread::recvThreadFunc, this);

	char localIpStr[INET_ADDRSTRLEN] = { 0 };
	if (InetNtopA(AF_INET, &m_localAddr.sin_addr, localIpStr, INET_ADDRSTRLEN) == nullptr) {
		strcpy_s(localIpStr, INET_ADDRSTRLEN, "invalid_ip");
	}
	std::cout << "UDP通讯线程启动成功，本地地址：" << localIpStr
		<< ":" << ntohs(m_localAddr.sin_port) << std::endl;
	return true;
}

// 停止UDP线程
void UdpCommThread::stop()
{
	if (!m_bIsRunning) return;

	m_bIsRunning = false;
	// 等待线程退出
	if (m_recvThread.joinable())
	{
		m_recvThread.join();
	}

	std::cout << "UDP通讯线程已停止" << std::endl;
}

// 发送初始化应答
bool UdpCommThread::sendInitAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ackData)
{
	if (m_udpSocket == INVALID_SOCKET)
	{
		std::cerr << "UDP Socket无效，发送初始化应答失败！" << std::endl;
		return false;
	}

	// 发送应答（结构体直接序列化，注意内存对齐）
	int sendLen = sendto(m_udpSocket, (const char*)&ackData, sizeof(ackData), 0,
		(sockaddr*)&m_remoteAddr, sizeof(m_remoteAddr));
	if (sendLen == SOCKET_ERROR)
	{
		std::cerr << "发送初始化应答失败，错误码：" << WSAGetLastError() << std::endl;
		return false;
	}

	std::cout << "发送初始化应答成功，长度：" << sendLen << "字节" << std::endl;
	return true;
}

// 设置远端地址
void UdpCommThread::setRemoteAddr(const char* ip, uint16_t port)
{
	// 空指针/端口校验
	if (!ip || port == 0) {
		std::cerr << "[ERR] setRemoteAddr: invalid param (ip="
			<< (ip ? ip : "nullptr") << ", port=" << port << ")\n";
		return;
	}

	// IP格式验证
	sockaddr_in tempAddr = {};
	tempAddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, ip, &tempAddr.sin_addr) <= 0) {
		std::cerr << "[ERR] setRemoteAddr: invalid IP '" << ip << "'\n";
		return;
	}
	if (isMutexLockedByAnyThread())
	{
		int d = 1;
	}
	else
	{
		int d = 0;
	}

	m_remoteAddr = tempAddr;
	m_remoteAddr.sin_port = htons(port); // 转为网络字节序
	std::cout << "[INFO] Remote address set to " << ip << ":" << port << "\n";
}

// 初始化Socket
bool UdpCommThread::initSocket()
{
	// 初始化Winsock
	if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
	{
		std::cerr << "WSAStartup失败，错误码：" << WSAGetLastError() << std::endl;
		return false;
	}

	// 创建UDP Socket
	m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_udpSocket == INVALID_SOCKET)
	{
		std::cerr << "创建UDP Socket失败，错误码：" << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	// 设置Socket为非阻塞模式
	u_long nonBlock = 1;
	if (ioctlsocket(m_udpSocket, FIONBIO, &nonBlock) == SOCKET_ERROR)
	{
		std::cerr << "设置Socket非阻塞失败，错误码：" << WSAGetLastError() << std::endl;
		closesocket(m_udpSocket);
		WSACleanup();
		return false;
	}

	// 绑定本地地址
	if (bind(m_udpSocket, (sockaddr*)&m_localAddr, sizeof(m_localAddr)) == SOCKET_ERROR)
	{
		std::cerr << "绑定UDP端口失败，错误码：" << WSAGetLastError() << std::endl;
		closesocket(m_udpSocket);
		WSACleanup();
		return false;
	}

	return true;
}

// 销毁Socket
void UdpCommThread::destroySocket()
{
	std::lock_guard<std::mutex> lock(m_mtx);
	if (m_udpSocket != INVALID_SOCKET)
	{
		closesocket(m_udpSocket);
		m_udpSocket = INVALID_SOCKET;
	}
	WSACleanup();
}

// 接收线程主函数
void UdpCommThread::recvThreadFunc()
{
	std::cout << "UDP接收线程开始运行" << std::endl;

	while (m_bIsRunning)
	{
		//std::lock_guard<std::mutex> lock(m_mtx);
		// 
		//std::unique_lock<std::mutex> lock(m_mtx);
		//lock.unlock(); //允许手动解锁
		if (m_udpSocket == INVALID_SOCKET)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		// 非阻塞接收数据
		sockaddr_in fromAddr;
		int fromAddrLen = sizeof(fromAddr);
		int recvLen = recvfrom(m_udpSocket, _recvBuf, RECV_BUF_SIZE, 0,
			(sockaddr*)&fromAddr, &fromAddrLen);

		if (recvLen > 0)
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			// 解析接收到的数据
			char fromIpStr[INET_ADDRSTRLEN] = { 0 };
			if (InetNtopA(AF_INET, &fromAddr.sin_addr, fromIpStr, INET_ADDRSTRLEN) == nullptr) {
				strcpy_s(fromIpStr, INET_ADDRSTRLEN, "invalid_ip");
			}
			std::cout << "接收UDP数据，长度：" << recvLen << "字节，来自："
				<< fromIpStr << ":" << ntohs(fromAddr.sin_port) << std::endl;

			// 更新远端地址（如果来自新的地址）
			setRemoteAddr(fromIpStr, ntohs(fromAddr.sin_port)); // 无需再次转换！
			parseUdpData(_recvBuf, recvLen);
		}
		else if (recvLen == 0)
		{
			// UDP无连接，recvLen=0无意义
			continue;
		}
		else
		{
			//constexpr
			// 非阻塞模式下，WSAEWOULDBLOCK表示无数据，无需处理
			int err = WSAGetLastError();
			if (err != WSAEWOULDBLOCK)
			{
				std::cerr << "UDP接收失败，错误码：" << err << std::endl;
			}
		}

		// 释放锁，避免长时间占用
		//lock.unlock();
		// 降低CPU占用
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::cout << "UDP接收线程退出" << std::endl;
}

// 解析UDP数据
void UdpCommThread::parseUdpData(const char* data, int dataLen)
{
	if (dataLen < sizeof(int)) // 至少包含flag字段
	{
		std::cerr << "UDP数据长度不足，无法解析" << std::endl;
		return;
	}

	// 提取flag字段（所有指令的第一个字段）
	int flag = *(const int*)data;
	std::cout << "解析UDP数据，flag：0x" << std::hex << flag << std::dec << std::endl;

	// 根据flag解析不同指令
	switch (flag)
	{
	case 0x41: // ControlP2cX1ObjTrackingCmd（控制指令：复位/开始/停止）
		if (dataLen == sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd))
		{
			parseControlCmd(*(const BYHWICD::ControlP2cX1ObjTrackingCmd*)data);
		}
		else
		{
			std::cerr << "控制指令长度不匹配，期望：" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
				<< "，实际：" << dataLen << std::endl;
		}
		break;

	case 0x36: // InitP2cObjectTrackingCmd（成像初始化指令）
		if (dataLen == sizeof(BYHWICD::InitP2cObjectTrackingCmd))
		{
			parseInitCmd(*(const BYHWICD::InitP2cObjectTrackingCmd*)data);
		}
		else
		{
			std::cerr << "初始化指令长度不匹配，期望：" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
				<< "，实际：" << dataLen << std::endl;
		}
		break;

	case 0x38: // DisplayC2cObjTrackingData（实时成像数据包）
		if (dataLen == sizeof(BYHWICD::DisplayC2cObjTrackingData))
		{
			parseDisplayData(*(const BYHWICD::DisplayC2cObjTrackingData*)data);
		}
		else
		{
			std::cerr << "实时成像数据长度不匹配，期望：" << sizeof(BYHWICD::DisplayC2cObjTrackingData)
				<< "，实际：" << dataLen << std::endl;
		}
		break;

	default:
		std::cerr << "未知的flag值：0x" << std::hex << flag << std::dec << std::endl;
		break;
	}
}

// 解析控制指令
void UdpCommThread::parseControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
	if (m_pHwaSimIR)
	{
		// 传递给HwaSimIR处理业务逻辑
		m_pHwaSimIR->handleControlCmd(cmd);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理控制指令" << std::endl;
	}
}

// 解析初始化指令
void UdpCommThread::parseInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd)
{
	if (m_pHwaSimIR)
	{
		// 传递给HwaSimIR处理初始化逻辑，并触发应答
		m_pHwaSimIR->handleInitCmd(cmd);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理初始化指令" << std::endl;
	}
}

// 解析实时成像数据
void UdpCommThread::parseDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data)
{
	if (m_pHwaSimIR)
	{
		// 传递给HwaSimIR处理实时数据
		m_pHwaSimIR->handleDisplayData(data);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理实时成像数据" << std::endl;
	}
}