#include "UdpCommThread_Linux.h"
#include "HwaSimIR.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

UdpCommThread::UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
	const std::string& remoteIp, uint16_t remotePort)
	: m_pHwaSimIR(hwaSimIR), m_udpSocket(-1), m_bIsRunning(false), m_mtx()
{
	memset(&m_localAddr, 0, sizeof(m_localAddr));
	m_localAddr.sin_family = AF_INET;
	m_localAddr.sin_port = htons(localPort);
	inet_pton(AF_INET, localIp.c_str(), &m_localAddr.sin_addr);

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
	if (isMutexLockedByAnyThread())
	{
		int d = 1;
		(void)d;
	}
	else
	{
		int d = 0;
		(void)d;
	}

	m_remoteAddr = tempAddr;
	m_remoteAddr.sin_port = htons(port);
	std::cout << "[INFO] Remote address set to " << ip << ":" << port << "\n";
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
			std::lock_guard<std::mutex> lock(m_mtx);

			char fromIpStr[INET_ADDRSTRLEN] = { 0 };
			if (inet_ntop(AF_INET, &fromAddr.sin_addr, fromIpStr, INET_ADDRSTRLEN) == nullptr)
			{
				strncpy(fromIpStr, "invalid_ip", INET_ADDRSTRLEN);
				fromIpStr[INET_ADDRSTRLEN - 1] = '\0';
			}
			std::cout << "接收UDP数据，长度：" << recvLen << "字节，来自："
				<< fromIpStr << ":" << ntohs(fromAddr.sin_port) << std::endl;

			setRemoteAddr(fromIpStr, ntohs(fromAddr.sin_port));
			parseUdpData(_recvBuf, recvLen);
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

void UdpCommThread::parseUdpData(const char* data, int dataLen)
{
	if (dataLen < static_cast<int>(sizeof(int)))
	{
		std::cerr << "UDP数据长度不足，无法解析" << std::endl;
		return;
	}

	int flag = 0;
	memcpy(&flag, data, sizeof(flag));
	std::cout << "解析UDP数据，flag：0x" << std::hex << flag << std::dec << std::endl;

	switch (flag)
	{
	case 0x41:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)))
		{
			BYHWICD::ControlP2cX1ObjTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			parseControlCmd(cmd);
		}
		else
		{
			std::cerr << "控制指令长度不匹配，期望：" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
				<< "，实际：" << dataLen << std::endl;
		}
		break;

	case 0x36:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::InitP2cObjectTrackingCmd)))
		{
			BYHWICD::InitP2cObjectTrackingCmd cmd;
			memcpy(&cmd, data, sizeof(cmd));
			parseInitCmd(cmd);
		}
		else
		{
			std::cerr << "初始化指令长度不匹配，期望：" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
				<< "，实际：" << dataLen << std::endl;
		}
		break;

	case 0x38:
		if (dataLen == static_cast<int>(sizeof(BYHWICD::DisplayC2cObjTrackingData)))
		{
			BYHWICD::DisplayC2cObjTrackingData displayData;
			memcpy(&displayData, data, sizeof(displayData));
			parseDisplayData(displayData);
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
