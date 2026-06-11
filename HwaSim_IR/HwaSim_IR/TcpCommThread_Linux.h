#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "CommonData.h"
#include <graphicsEngine.h>
#include <graphicsOutput.h>
#include <opencv2/opencv.hpp>
#include <pnmImage.h>
#include <pta_uchar.h>
#include <texture.h>
#include "Annotation/AnnotationTypes.h"

class HwaSimIR;

class TcpCommThread {
public:
	TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort);
	~TcpCommThread();

	bool start();
	void stop();

	void updateFrame(const uchar* data, int width, int height);
	void updateFrame(
		const uchar* data,
		int width,
		int height,
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const AnnotationFrameRecord& annotationRecord,
		bool annotationEnabled);

	bool sendControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	bool sendInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& initData);
	void resetInitCompleted() { m_initCompleted = false; }

private:
	bool initSocket();
	void destroySocket();

	void sendFrameThreadFunc();
	bool sendFramePacket(
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const std::string& annotationJson,
		const std::vector<uchar>& jpegData);
	bool sendAll(const char* data, int size);
	bool sendStruct(const void* structPtr, uint32_t structSize);
	std::string buildAnnotationJson(
		const AnnotationFrameRecord& record,
		bool annotationEnabled,
		int tcpWidth,
		int tcpHeight) const;

	bool connectToServer();
	void disconnectFromServer();

private:
	HwaSimIR* m_pHwaSimIR;

	int m_tcpSocket;
	sockaddr_in m_serverAddr;
	std::string m_serverIp;
	uint16_t m_serverPort;

	std::thread m_sendThread;
	std::atomic<bool> m_bIsRunning;
	std::atomic<bool> m_bIsConnected;
	std::atomic<bool> m_initCompleted{ false };
	std::mutex m_mtx;
	std::mutex m_socketMtx;

	std::mutex m_frameMtx;
	std::condition_variable m_frameCv;
	std::vector<uchar> m_frameBuffer;
	int m_frameWidth;
	int m_frameHeight;
	bool m_bNewFrame = false;
	BYHWICD::DisplayC2cObjTrackingData m_trackingData;
	AnnotationFrameRecord m_annotationRecord;
	bool m_hasTrackingData = false;
	bool m_annotationEnabled = false;
	unsigned long long m_tcpPacketCounter = 0;
};
