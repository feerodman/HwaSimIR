#include "UdpCommThread_Linux.h"
#include "HwaSimIR.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <unistd.h>

UdpCommThread::UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
	const std::string& remoteIp, uint16_t remotePort,
	int localPlatID, int localSensorID,
	bool acceptSensorBroadcast, bool allowDynamicRemote)
	: m_pHwaSimIR(hwaSimIR), m_udpSocket(-1),
	  m_localPlatID(localPlatID), m_localSensorID(localSensorID),
	  m_acceptSensorBroadcast(acceptSensorBroadcast), m_allowDynamicRemote(allowDynamicRemote),
	  m_bIsRunning(false), m_mtx()
{
	memset(&m_localAddr, 0, sizeof(m_localAddr));
	m_localAddr.sin_family = AF_INET;
	m_localAddr.sin_port = htons(localPort);
	inet_pton(AF_INET, localIp.c_str(), &m_localAddr.sin_addr);

	memset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
	setRemoteAddr(remoteIp.c_str(), remotePort);
}

UdpCommThread::~UdpCommThread()
{
	stop();
	destroySocket();
}

bool UdpCommThread::start()
{
	if (m_bIsRunning) return true;

	if (!initSocket())
	{
		std::cerr << "UDP Socket初始化失败！" << std::endl;
		return false;
	}

	m_bIsRunning = true;
	m_recvThread = std::thread(&UdpCommThread::recvThreadFunc, this);

	char localIpStr[INET_ADDRSTRLEN] = { 0 };
	if (inet_ntop(AF_INET, &m_localAddr.sin_addr, localIpStr, INET_ADDRSTRLEN) == nullptr)
	{
		strncpy(localIpStr, "invalid_ip", INET_ADDRSTRLEN);
		localIpStr[INET_ADDRSTRLEN - 1] = '\0';
	}
	std::cout << "UDP通讯线程启动成功，本地地址：" << localIpStr
		<< ":" << ntohs(m_localAddr.sin_port) << std::endl;
	return true;
}

void UdpCommThread::stop()
{
	if (!m_bIsRunning) return;

	m_bIsRunning = false;
	if (m_recvThread.joinable())
	{
		m_recvThread.join();
	}

	std::cout << "UDP通讯线程已停止" << std::endl;
}

bool UdpCommThread::sendInitAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ackData)
{
	std::lock_guard<std::mutex> lock(m_mtx);
	if (m_udpSocket == -1)
	{
		std::cerr << "UDP Socket无效，发送初始化应答失败！" << std::endl;
		return false;
	}

	const int sendLen = sendto(m_udpSocket, reinterpret_cast<const char*>(&ackData), sizeof(ackData), 0,
		reinterpret_cast<sockaddr*>(&m_remoteAddr), sizeof(m_remoteAddr));
	if (sendLen == -1)
	{
		std::cerr << "发送初始化应答失败，错误码：" << strerror(errno) << std::endl;
		return false;
	}

	std::cout << "发送初始化应答成功，长度：" << sendLen << "字节" << std::endl;
	return true;
}

void UdpCommThread::setRemoteAddr(const char* ip, uint16_t port)
{
	if (!ip || port == 0)
	{
		std::cerr << "[ERR] setRemoteAddr: invalid param (ip="
			<< (ip ? ip : "nullptr") << ", port=" << port << ")\n";
		return;
	}

	sockaddr_in tempAddr = {};
	tempAddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, ip, &tempAddr.sin_addr) <= 0)
	{
		std::cerr << "[ERR] setRemoteAddr: invalid IP '" << ip << "'\n";
		return;
	}
	std::lock_guard<std::mutex> lock(m_mtx);
	const bool changed = m_remoteAddr.sin_addr.s_addr != tempAddr.sin_addr.s_addr ||
		ntohs(m_remoteAddr.sin_port) != port;
	m_remoteAddr = tempAddr;
	m_remoteAddr.sin_port = htons(port);
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
	if (inet_ntop(AF_INET, &fromAddr.sin_addr, fromIpStr, INET_ADDRSTRLEN) == nullptr)
	{
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
	line << (accepted ? "[PacketRoute]" : "[PacketRouteReject]")
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

bool UdpCommThread::initSocket()
{
	m_udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_udpSocket == -1)
	{
		std::cerr << "创建UDP Socket失败，错误码：" << strerror(errno) << std::endl;
		return false;
	}

	const int flags = fcntl(m_udpSocket, F_GETFL, 0);
	if (flags == -1)
	{
		std::cerr << "获取Socket标志失败，错误码：" << strerror(errno) << std::endl;
		close(m_udpSocket);
		m_udpSocket = -1;
		return false;
	}
	if (fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		std::cerr << "设置Socket非阻塞失败，错误码：" << strerror(errno) << std::endl;
		close(m_udpSocket);
		m_udpSocket = -1;
		return false;
	}

	if (bind(m_udpSocket, reinterpret_cast<sockaddr*>(&m_localAddr), sizeof(m_localAddr)) == -1)
	{
		std::cerr << "绑定UDP端口失败，错误码：" << strerror(errno) << std::endl;
		close(m_udpSocket);
		m_udpSocket = -1;
		return false;
	}

	return true;
}

void UdpCommThread::destroySocket()
{
	std::lock_guard<std::mutex> lock(m_mtx);
	if (m_udpSocket != -1)
	{
		close(m_udpSocket);
		m_udpSocket = -1;
	}
}

void UdpCommThread::recvThreadFunc()
{
	std::cout << "UDP接收线程开始运行" << std::endl;

	while (m_bIsRunning)
	{
		if (m_udpSocket == -1)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		sockaddr_in fromAddr;
		socklen_t fromAddrLen = sizeof(fromAddr);
		const int recvLen = recvfrom(m_udpSocket, _recvBuf, RECV_BUF_SIZE, 0,
			reinterpret_cast<sockaddr*>(&fromAddr), &fromAddrLen);

		if (recvLen > 0)
		{
			char fromIpStr[INET_ADDRSTRLEN] = { 0 };
			if (inet_ntop(AF_INET, &fromAddr.sin_addr, fromIpStr, INET_ADDRSTRLEN) == nullptr)
			{
				strncpy(fromIpStr, "invalid_ip", INET_ADDRSTRLEN);
				fromIpStr[INET_ADDRSTRLEN - 1] = '\0';
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
			continue;
		}
		else
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				std::cerr << "UDP接收失败，错误码：" << strerror(errno) << std::endl;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	std::cout << "UDP接收线程退出" << std::endl;
}

void UdpCommThread::parseUdpData(const char* data, int dataLen, const sockaddr_in& fromAddr)
{
	if (dataLen < static_cast<int>(sizeof(int)))
	{
		logRoute(0, false, -1, false, -1, "length_too_short");
		return;
	}

	int flag = 0;
	memcpy(&flag, data, sizeof(flag));
	++m_parsePacketCount;
	if (flag != 0x38 || m_parsePacketCount <= 3 || (m_parsePacketCount % 120) == 0)
	{
		std::cout << "解析UDP数据，flag：0x" << std::hex << flag << std::dec
			<< " packet=" << m_parsePacketCount << std::endl;
	}

	switch (flag)
	{
	case 0x41:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)))
		{
			BYHWICD::ControlP2cX1ObjTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			const bool accepted = cmd.platID == m_localPlatID;
			logRoute(flag, accepted, cmd.platID, false, -1,
				accepted ? "plat_match" : "plat_mismatch");
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

	case 0x36:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::InitP2cObjectTrackingCmd)))
		{
			BYHWICD::InitP2cObjectTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			const bool platMatch = cmd.platID == m_localPlatID;
			const bool exactSensor = cmd.sensorID == m_localSensorID;
			const bool sensorBroadcast = m_acceptSensorBroadcast && cmd.sensorID == 255;
			const bool accepted = platMatch && (exactSensor || sensorBroadcast);
			const char* reason = !platMatch ? "plat_mismatch"
				: (exactSensor ? "exact_match" : (sensorBroadcast ? "sensor_broadcast" : "sensor_mismatch"));
			logRoute(flag, accepted, cmd.platID, true, cmd.sensorID, reason);
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

	case 0x38:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::DisplayC2cObjTrackingData)))
		{
			BYHWICD::DisplayC2cObjTrackingData displayData;
			memcpy(&displayData, data, sizeof(displayData));
			const bool platMatch = displayData.platID == m_localPlatID;
			const bool exactSensor = displayData.sensorID == m_localSensorID;
			const bool sensorBroadcast = m_acceptSensorBroadcast && displayData.sensorID == 255;
			const bool accepted = platMatch && (exactSensor || sensorBroadcast);
			const char* reason = !platMatch ? "plat_mismatch"
				: (exactSensor ? "exact_match" : (sensorBroadcast ? "sensor_broadcast" : "sensor_mismatch"));
			logRoute(flag, accepted, displayData.platID, true, displayData.sensorID, reason);
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

void UdpCommThread::parseControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
	if (m_pHwaSimIR)
	{
		m_pHwaSimIR->handleControlCmd(cmd);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理控制指令" << std::endl;
	}
}

void UdpCommThread::parseInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd)
{
	if (m_pHwaSimIR)
	{
		m_pHwaSimIR->handleInitCmd(cmd);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理初始化指令" << std::endl;
	}
}

void UdpCommThread::parseDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data)
{
	if (m_pHwaSimIR)
	{
		m_pHwaSimIR->handleDisplayData(data);
	}
	else
	{
		std::cerr << "HwaSimIR实例为空，无法处理实时成像数据" << std::endl;
	}
}
