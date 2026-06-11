#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "CommonData.h"

class HwaSimIR;

class UdpCommThread
{
public:
	UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
		const std::string& remoteIp, uint16_t remotePort);
	~UdpCommThread();

	bool start();
	void stop();

	bool sendInitAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ackData);

	void setRemoteAddr(const char* ip, uint16_t port);
	sockaddr_in getRemoteAddr() const { return m_remoteAddr; }
	bool isMutexLockedByAnyThread() {
		if (m_mtx.try_lock()) {
			m_mtx.unlock();
			return false;
		}
		return true;
	}

private:
	bool initSocket();
	void destroySocket();
	void recvThreadFunc();
	void parseUdpData(const char* data, int dataLen);

	void parseControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	void parseInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
	void parseDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data);

private:
	HwaSimIR* m_pHwaSimIR;

	int m_udpSocket;
	sockaddr_in m_localAddr;
	sockaddr_in m_remoteAddr;

	std::thread m_recvThread;
	std::atomic<bool> m_bIsRunning;
	std::mutex m_mtx;

	static const int RECV_BUF_SIZE = 4096;
	char _recvBuf[RECV_BUF_SIZE];
};
