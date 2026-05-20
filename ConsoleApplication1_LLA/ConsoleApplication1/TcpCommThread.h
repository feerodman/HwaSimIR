#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <condition_variable> // 【新增】用于线程同步的条件变量
#include "CommonData.h"
#pragma comment(lib, "WS2_32.lib")// 链接WS2_32.lib
//#include <core.h>
#include <texture.h>
#include <graphicsOutput.h>
#include <graphicsEngine.h>
#include <pnmImage.h>
#include <opencv2/opencv.hpp>
#include <pta_uchar.h>

class HwaSimIR;
// TCP通信线程类
class TcpCommThread {
public:
	// 构造函数：初始化TCP参数，关联HwaSimIR实例
	TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort);

	// 析构函数：清理资源
	~TcpCommThread();

	// 启动TCP通讯线程
	bool start();

	// 停止TCP通讯线程
	void stop();

	// 供主线程推送最新像素数据的接口
	void updateFrame(const uchar* data, int width, int height);

private:
	// 初始化TCP Socket
	bool initSocket();

	// 销毁Socket和Winsock
	void destroySocket();

	// 发送帧的主函数
	void sendFrameThreadFunc();

	// 负责连接与断开的函数
	bool connectToServer();
	void disconnectFromServer();


private:
	// 关联的HwaSimIR实例
	HwaSimIR* m_pHwaSimIR;

	// TCP Socket相关
	WSADATA m_wsaData;
	SOCKET m_tcpSocket;
	sockaddr_in m_serverAddr;
	std::string m_serverIp;
	uint16_t m_serverPort;

	// 线程相关
	std::thread m_sendThread;
	std::atomic<bool> m_bIsRunning;
	std::atomic<bool> m_bIsConnected; // 连接状态标志
	std::mutex m_mtx; // 互斥锁保护共享数据

	// 线程安全的图像缓冲区结构
	std::mutex m_frameMtx;                 // 帧缓冲互斥锁
	std::condition_variable m_frameCv;     // 条件变量通知新帧
	std::vector<uchar> m_frameBuffer;      // 缓存的 RGB 像素
	int m_frameWidth;
	int m_frameHeight;
	bool m_bNewFrame = false;              // 有新帧的标志
};