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
#include <memory>
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
#include "Video/VideoEncoder.h"

class HwaSimIR;

class TcpCommThread {
public:
	TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort,
		const std::string& channel, int localPlatID, int localSensorID);
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
		const std::string& h264Encoder,
		int h264BitrateKbps,
		int h264GopFrames,
		bool h264LowLatency,
		bool h264ForceKeyFrameOnStart,
		const std::string& codecConfig);
	void configurePayload(
		int packetVersion,
		bool sendVideo,
		bool sendAnnotation,
		bool sendRealtimeData,
		bool forwardInitControl);
	void setH264Requested(bool enabled, int videoFps = 60);
	void resetFrameCounters();

	bool sendControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	bool sendInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& initData);
	void resetInitCompleted();

private:
	bool initSocket();
	void destroySocket();

	void sendFrameThreadFunc();
	bool sendFramePacket(
		const BYHWICD::DisplayC2cObjTrackingData& trackingData,
		const std::string& annotationJson,
		const EncodedVideoFrame& encodedFrame,
		std::uint64_t frameSeq,
		std::uint64_t outputOrdinal,
		std::int64_t ptsMs,
		std::uint32_t& sectionFlags,
		std::uint32_t& realtimeBytes,
		std::uint32_t& annotationBytes,
		std::uint32_t& videoBytes);
	bool sendAll(const char* data, int size);
	bool sendStruct(const void* structPtr, uint32_t structSize);
	std::string buildAnnotationJson(
		const AnnotationFrameRecord& record,
		bool annotationEnabled,
		int tcpWidth,
		int tcpHeight,
		const IRFrameTelemetry& telemetry,
		std::uint64_t outputOrdinal,
		std::int64_t tcpSendTimeNs,
		int packetVersion,
		const std::string& requestedCodec,
		const std::string& requestedBackend,
		const EncodedVideoFrame& encodedFrame) const;
	bool encodeFrame(
		const RawVideoFrame& rawFrame,
		EncodedVideoFrame& encodedFrame,
		std::string& requestedCodec,
		std::string& requestedBackend);
	void requestEncoderKeyFrame(const char* reason);

	bool connectToServer();
	void disconnectFromServer();

private:
	HwaSimIR* m_pHwaSimIR;
	std::string m_channel;
	int m_localPlatID = 0;
	int m_localSensorID = 0;

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
	std::atomic<int> m_h264BitrateKbps{ 4000 };
	std::atomic<int> m_h264GopFrames{ 30 };
	std::atomic<bool> m_h264LowLatency{ true };
	std::atomic<bool> m_h264ForceKeyFrameOnStart{ true };
	std::atomic<int> m_videoFps{ 60 };
	std::atomic<int> m_packetVersion{ 3 };
	std::atomic<bool> m_sendVideo{ true };
	std::atomic<bool> m_sendAnnotation{ true };
	std::atomic<bool> m_sendRealtimeData{ true };
	std::atomic<bool> m_forwardInitControl{ true };
	std::atomic<bool> m_allPayloadDisabledWarned{ false };
	std::string m_codecConfig = "auto";
	std::string m_h264EncoderConfig = "auto";
	mutable std::mutex m_codecMtx;
	std::unique_ptr<JpegFrameEncoder> m_jpegEncoder;
	std::unique_ptr<H264FfmpegEncoder> m_h264Encoder;
#if defined(HWASIMIR_HAS_RKMPP)
	std::unique_ptr<H264MppEncoder> m_mppEncoder;
#endif
	VideoEncoderConfig m_jpegEncoderConfig;
	VideoEncoderConfig m_h264EncoderRuntimeConfig;
	VideoEncoderConfig m_mppEncoderRuntimeConfig;
	bool m_jpegEncoderConfigured = false;
	bool m_h264EncoderConfigured = false;
	bool m_mppEncoderConfigured = false;
	std::string m_lastCodecFallbackReason;
	std::atomic<bool> m_encoderResetRequested{ true };
	std::atomic<bool> m_encoderKeyFrameRequested{ true };
	std::atomic<unsigned long long> m_tcpPacketCounter{ 0 };
	std::int64_t m_lastTcpPerfLogNs = 0;
};
