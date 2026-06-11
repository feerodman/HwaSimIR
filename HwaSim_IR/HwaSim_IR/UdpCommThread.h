#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include "CommonData.h"

// 链接WS2_32.lib
//#pragma comment(lib, "WS2_32.lib")

// 前置声明，避免循环包含
class HwaSimIR;

/**
* UDP通讯线程类
* 负责接收激励数据软件的指令/数据包，解析后传递给HwaSimIR，并发送应答
*/
class UdpCommThread
{
public:
	// 构造函数：初始化UDP参数，关联HwaSimIR实例
	UdpCommThread(HwaSimIR* hwaSimIR, const std::string& localIp, uint16_t localPort,
		const std::string& remoteIp, uint16_t remotePort);
	// 析构函数：清理资源
	~UdpCommThread();

	// 启动UDP通讯线程
	bool start();
	// 停止UDP通讯线程
	void stop();

	// 发送初始化应答（供HwaSimIR调用）
	bool sendInitAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ackData);

	// 获取/设置远端地址（激励数据软件）
	void setRemoteAddr(const char* ip, uint16_t port);
	sockaddr_in getRemoteAddr() const { return m_remoteAddr; }
	bool isMutexLockedByAnyThread() {
		if (m_mtx.try_lock()) { // 尝试加锁
								// 加锁成功 → 无线程持有，手动解锁后返回false
			m_mtx.unlock();
			return false;
		}
		else {
			// 加锁失败 → 有线程持有，返回true
			return true;
		}
	}

private:
	// 初始化Winsock和UDP Socket
	bool initSocket();
	// 销毁Socket和Winsock
	void destroySocket();
	// 接收线程主函数
	void recvThreadFunc();
	// 解析接收到的UDP数据
	void parseUdpData(const char* data, int dataLen);

	// 解析控制指令（复位/开始/停止）
	void parseControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	// 解析成像初始化指令
	void parseInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd);
	// 解析实时成像数据包
	void parseDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data);

private:
	// 关联的HwaSimIR实例（业务逻辑处理）
	HwaSimIR* m_pHwaSimIR;

	// UDP Socket相关
	WSADATA m_wsaData;
	SOCKET m_udpSocket;
	sockaddr_in m_localAddr;  // 本地绑定地址
	sockaddr_in m_remoteAddr; // 远端（激励数据软件）地址

							 // 线程相关
	std::thread m_recvThread;       // 接收线程
	std::atomic<bool> m_bIsRunning;  // 线程运行标志
	std::mutex m_mtx;               // 互斥锁（保护共享数据）

								   // 接收缓冲区（足够容纳最大数据包）
	static const int RECV_BUF_SIZE = 4096;
	char _recvBuf[RECV_BUF_SIZE];
	std::uint64_t m_receivePacketCount = 0;
	std::uint64_t m_parsePacketCount = 0;
};
