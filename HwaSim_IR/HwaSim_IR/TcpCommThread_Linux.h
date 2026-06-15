#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
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
#include "IR/IRPerfStats.h"

class HwaSimIR;

class TcpCommThread {
public:
	TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort);
	~TcpCommThread();

	bool start();
	void stop();

	void updateFrame(const uchar* data, int width, int height);
	IRFrameEnqueueResult updateFrame(
		const uchar* data,
		int width,
		int height,
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const AnnotationFrameRecord& annotationRecord,
		bool annotationEnabled,
		const IRFrameTelemetry& telemetry);
	void setSyncMode(bool syncMode) { m_syncMode.store(syncMode); }
	void setFlipVertical(bool enabled) { m_flipVertical.store(enabled); }
	void configureOutput(
		int jpegQuality,
		bool jpegGray,
		bool enableH264Experimental,
		bool h264FallbackToJpeg,
		const std::string& codecConfig);
	void setH264Requested(bool enabled);
	void resetFrameCounters();

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
		int tcpHeight,
		const IRFrameTelemetry& telemetry,
		std::uint64_t outputOrdinal,
		std::int64_t tcpSendTimeNs) const;
	void resolveCodecState(
		std::string& requestedCodec,
		std::string& activeCodec,
		std::string& fallbackReason) const;

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

	struct PendingFrame
	{
		std::vector<uchar> pixels;
		int width = 0;
		int height = 0;
		BYHWICD::DisplayC2cObjTrackingData trackingData{};
		AnnotationFrameRecord annotationRecord;
		bool annotationEnabled = false;
		IRFrameTelemetry telemetry;
		double queueWaitMs = 0.0;
		bool overwritten = false;
	};

	std::mutex m_frameMtx;
	std::condition_variable m_frameCv;
	std::condition_variable m_queueSpaceCv;
	std::deque<PendingFrame> m_frameQueue;
	static const std::size_t kMaxFrameQueue = 4;
	std::atomic<bool> m_syncMode{ true };
	std::atomic<bool> m_flipVertical{ true };
	std::atomic<int> m_jpegQuality{ 100 };
	std::atomic<bool> m_jpegGray{ false };
	std::atomic<bool> m_enableH264Experimental{ false };
	std::atomic<bool> m_h264FallbackToJpeg{ true };
	std::atomic<bool> m_h264Requested{ false };
	std::string m_codecConfig = "auto";
	mutable std::mutex m_codecMtx;
	std::atomic<unsigned long long> m_tcpPacketCounter{ 0 };
	std::int64_t m_lastTcpPerfLogNs = 0;
};
