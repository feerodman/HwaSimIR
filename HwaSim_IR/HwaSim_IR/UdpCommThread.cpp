#include "UdpCommThread.h"
#include "HwaSimIR.h"
#include <iostream>
#include <cstring>
#include <sstream>
#include "ProtocolRoute.h"


// 构造函数
UdpCommThread::UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
	const std::string& remoteIp, uint16_t remotePort,
	int localPlatID, int localSensorID,
	bool acceptSensorBroadcast, bool allowDynamicRemote)
	: m_pHwaSimIR(hwaSimIR), m_udpSocket(INVALID_SOCKET),
	  m_localPlatID(localPlatID), m_localSensorID(localSensorID),
	  m_acceptSensorBroadcast(acceptSensorBroadcast), m_allowDynamicRemote(allowDynamicRemote),
	  m_bIsRunning(false), m_mtx()
{
	// 初始化本地地址
	memset(&m_localAddr, 0, sizeof(m_localAddr));
	m_localAddr.sin_family = AF_INET;
	m_localAddr.sin_port = htons(localPort);
	inet_pton(AF_INET, localIp.c_str(), &m_localAddr.sin_addr);

	// 初始化远端地址
	memset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
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
	m_lastStartFailureReason = "none";

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
	std::lock_guard<std::mutex> lock(m_mtx);
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
	std::lock_guard<std::mutex> lock(m_mtx);
	const bool changed = m_remoteAddr.sin_addr.s_addr != tempAddr.sin_addr.s_addr ||
		ntohs(m_remoteAddr.sin_port) != port;
	m_remoteAddr = tempAddr;
	m_remoteAddr.sin_port = htons(port); // 转为网络字节序
	if (changed)
	{
		std::cout << "[INFO] Remote address set to " << ip << ":" << port << "\n";
	}
}

sockaddr_in UdpCommThread::getRemoteAddr() const
{
	std::lock_guard<std::mutex> lock(m_mtx);
	return m_remoteAddr;
}

void UdpCommThread::updateRemoteFromSender(const sockaddr_in& fromAddr)
{
	char fromIpStr[INET_ADDRSTRLEN] = { 0 };
	if (InetNtopA(AF_INET, const_cast<in_addr*>(&fromAddr.sin_addr), fromIpStr, INET_ADDRSTRLEN) == nullptr)
	{
		strcpy_s(fromIpStr, INET_ADDRSTRLEN, "invalid_ip");
		return;
	}
	setRemoteAddr(fromIpStr, ntohs(fromAddr.sin_port));
}

void UdpCommThread::logRoute(
	int flag,
	bool accepted,
	int packetPlatID,
	bool hasSensorID,
	int packetSensorID,
	const char* reason)
{
	std::uint64_t& counter = accepted ? m_routeAcceptCount : m_routeRejectCount;
	++counter;
	const bool shouldLog = flag != 0x38 || counter <= 8 || (counter % 120) == 0;
	if (!shouldLog)
	{
		return;
	}
	std::ostringstream line;
	const char* type = flag == 0x41 ? "control" :
		(flag == 0x36 ? "init" : (flag == 0x38 ? "realtime" : "unknown"));
	line << "[ProtocolRoute] transport=udp"
		<< " type=" << type
		<< " flag=0x" << std::hex << flag << std::dec
		<< " accepted=" << (accepted ? "1" : "0")
		<< " localPlatID=" << m_localPlatID
		<< " localSensorID=" << m_localSensorID
		<< " packetPlatID=" << packetPlatID
		<< " packetSensorID=";
	if (hasSensorID)
	{
		line << packetSensorID;
	}
	else
	{
		line << "na";
	}
	line << " reason=" << reason
		<< " routeCount=" << counter
		<< '\n';
	std::cout << line.str();
	std::cout.flush();
}

// 初始化Socket
bool UdpCommThread::initSocket()
{
	// 初始化Winsock
	if (WSAStartup(MAKEWORD(2, 2), &m_wsaData) != 0)
	{
		m_lastStartFailureReason = "winsock_startup_failed";
		std::cerr << "WSAStartup失败，错误码：" << WSAGetLastError() << std::endl;
		return false;
	}

	// 创建UDP Socket
	m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_udpSocket == INVALID_SOCKET)
	{
		m_lastStartFailureReason = "socket_create_failed";
		std::cerr << "创建UDP Socket失败，错误码：" << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	// 设置Socket为非阻塞模式
	u_long nonBlock = 1;
	if (ioctlsocket(m_udpSocket, FIONBIO, &nonBlock) == SOCKET_ERROR)
	{
		m_lastStartFailureReason = "nonblocking_config_failed";
		std::cerr << "设置Socket非阻塞失败，错误码：" << WSAGetLastError() << std::endl;
		closesocket(m_udpSocket);
		WSACleanup();
		return false;
	}

	// 绑定本地地址
	if (bind(m_udpSocket, (sockaddr*)&m_localAddr, sizeof(m_localAddr)) == SOCKET_ERROR)
	{
		m_lastStartFailureReason = "bind_failed";
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
			// 解析接收到的数据
			char fromIpStr[INET_ADDRSTRLEN] = { 0 };
			if (InetNtopA(AF_INET, &fromAddr.sin_addr, fromIpStr, INET_ADDRSTRLEN) == nullptr) {
				strcpy_s(fromIpStr, INET_ADDRSTRLEN, "invalid_ip");
			}
			++m_receivePacketCount;
			if (m_receivePacketCount <= 3 || (m_receivePacketCount % 120) == 0)
			{
				std::cout << "接收UDP数据，长度：" << recvLen << "字节，来自："
					<< fromIpStr << ":" << ntohs(fromAddr.sin_port)
					<< " packet=" << m_receivePacketCount << std::endl;
			}

			parseUdpData(_recvBuf, recvLen, fromAddr);
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
void UdpCommThread::parseUdpData(const char* data, int dataLen, const sockaddr_in& fromAddr)
{
	if (dataLen < sizeof(int)) // 至少包含flag字段
	{
		logRoute(0, false, -1, false, -1, "length_too_short");
		return;
	}

	// 提取flag字段（所有指令的第一个字段）
	int flag = 0;
	memcpy(&flag, data, sizeof(flag));
	++m_parsePacketCount;
	if (flag != 0x38 || m_parsePacketCount <= 3 || (m_parsePacketCount % 120) == 0)
	{
		std::cout << "解析UDP数据，flag：0x" << std::hex << flag << std::dec
			<< " packet=" << m_parsePacketCount << std::endl;
	}

	// 根据flag解析不同指令
	switch (flag)
	{
	case 0x41: // ControlP2cX1ObjTrackingCmd（控制指令：复位/开始/停止）
		if (dataLen == sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd))
		{
			BYHWICD::ControlP2cX1ObjTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			const ProtocolRouteResult route = EvaluateProtocolRoute(
				m_localPlatID, m_localSensorID, m_acceptSensorBroadcast,
				cmd.platID, -1, false);
			const bool accepted = ProtocolRouteAccepted(route);
			logRoute(flag, accepted, cmd.platID, false, -1,
				ProtocolRouteReason(route));
			if (!accepted)
			{
				return;
			}
			if (m_allowDynamicRemote)
			{
				updateRemoteFromSender(fromAddr);
			}
			parseControlCmd(cmd);
		}
		else
		{
			logRoute(flag, false, -1, false, -1, "length_mismatch");
		}
		break;

	case 0x36: // InitP2cObjectTrackingCmd（成像初始化指令）
		if (dataLen == sizeof(BYHWICD::InitP2cObjectTrackingCmd))
		{
			BYHWICD::InitP2cObjectTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			const ProtocolRouteResult route = EvaluateProtocolRoute(
				m_localPlatID, m_localSensorID, m_acceptSensorBroadcast,
				cmd.platID, cmd.sensorID, true);
			const bool accepted = ProtocolRouteAccepted(route);
			logRoute(flag, accepted, cmd.platID, true, cmd.sensorID, ProtocolRouteReason(route));
			if (!accepted)
			{
				return;
			}
			if (m_allowDynamicRemote)
			{
				updateRemoteFromSender(fromAddr);
			}
			parseInitCmd(cmd);
		}
		else
		{
			logRoute(flag, false, -1, false, -1, "length_mismatch");
		}
		break;

	case 0x38: // DisplayC2cObjTrackingData（实时成像数据包）
		if (dataLen == sizeof(BYHWICD::DisplayC2cObjTrackingData))
		{
			BYHWICD::DisplayC2cObjTrackingData displayData;
			memcpy(&displayData, data, sizeof(displayData));
			const ProtocolRouteResult route = EvaluateProtocolRoute(
				m_localPlatID, m_localSensorID, m_acceptSensorBroadcast,
				displayData.platID, displayData.sensorID, true);
			const bool accepted = ProtocolRouteAccepted(route);
			logRoute(flag, accepted, displayData.platID, true, displayData.sensorID,
				ProtocolRouteReason(route));
			if (!accepted)
			{
				return;
			}
			if (m_allowDynamicRemote)
			{
				updateRemoteFromSender(fromAddr);
			}
			parseDisplayData(displayData);
		}
		else
		{
			logRoute(flag, false, -1, false, -1, "length_mismatch");
		}
		break;

	default:
		logRoute(flag, false, -1, false, -1, "unsupported_flag");
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
