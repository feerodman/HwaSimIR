#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <string>
#include <vector>
#include <condition_variable> // 【新增】用于线程同步的条件变量
#include <memory>
#include "CommonData.h"
#pragma comment(lib, "WS2_32.lib")// 链接WS2_32.lib
//#include <core.h>
#include <texture.h>
#include <graphicsOutput.h>
#include <graphicsEngine.h>
#include <pnmImage.h>
#include <opencv2/opencv.hpp>
#include <pta_uchar.h>
#include "Annotation/AnnotationTypes.h"
#include "IR/IRPerfStats.h"
#include "Video/VideoEncoder.h"
#include "Video/DdsVideoPublisher.h"
#include "Video/LocalMp4Recorder.h"

class HwaSimIR;
class DdsRuntimeManager;
// TCP通信线程类
class TcpCommThread {
public:
	// 构造函数：初始化TCP参数，关联HwaSimIR实例
	TcpCommThread(HwaSimIR* hwaSimIR, const std::string& serverIp, uint16_t serverPort,
		const std::string& channel, int localPlatID, int localSensorID);

	// 析构函数：清理资源
	~TcpCommThread();

	// 启动TCP通讯线程
	bool start();

	// 停止TCP通讯线程
	void stop();

	// 供主线程推送最新像素数据的接口
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
	void configureDdsVideo(const DdsVideoPublisherConfig& config);
	void configureDdsRuntime(const std::shared_ptr<DdsRuntimeManager>& runtime);
	void configureLocalRecording(const LocalMp4RecorderConfig& config);
	void setH264Requested(bool enabled, int videoFps = 60);
	void setLocalRecordingProtocolEnabled(bool enabled);
	bool startOutputRound(int round);
	bool stopOutputRound(const char* reason);
	void resetFrameCounters();
	// 转发 UDP 收到的控制命令，触发接收端开始/停止/复位逻辑。
	bool sendControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd);
	// 转发 UDP 收到的初始化命令，触发接收端初始化界面和回合状态。
	bool sendInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& initData);
	// 新回合或复位时重置初始化状态，允许后续重新转发初始化命令。
	void resetInitCompleted();

private:
	// 初始化TCP Socket
	bool initSocket();

	// 销毁Socket和Winsock
	void destroySocket();

	// 发送帧的主函数
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
		std::string& requestedBackend,
		const std::string& forcedCodec = "auto",
		bool allowJpegFallback = true);
	std::string resolvedDdsCodec() const;
	std::string resolvedDdsTopic(const std::string& codec) const;
	bool tcpWantsH264() const;
	void requestEncoderKeyFrame(const char* reason);

	// 负责连接与断开的函数
	bool connectToServer();
	void disconnectFromServer();


private:
	// 关联的HwaSimIR实例
	HwaSimIR* m_pHwaSimIR;
	std::string m_channel;
	int m_localPlatID = 0;
	int m_localSensorID = 0;

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
	std::atomic<bool> m_initCompleted{ false }; // 初始化命令是否已转发成功
	std::mutex m_mtx; // 互斥锁保护共享数据
	std::mutex m_socketMtx; // 保护控制/初始化包与视频帧包不交叉发送

	struct PendingFrame
	{
		std::vector<uchar> pixels;
		int width = 0;
		int height = 0;
		BYHWICD::DisplayC2cObjTrackingData trackingData{};
		AnnotationFrameRecord annotationRecord;
		bool annotationEnabled = false;
		IRFrameTelemetry telemetry;
		std::uint64_t logicalFrameSeq = 0;
		int currentRound = 0;
		bool roundActive = false;
		double queueWaitMs = 0.0;
		bool overwritten = false;
	};

	std::mutex m_frameMtx;                 // 帧缓冲互斥锁
	std::condition_variable m_frameCv;     // 条件变量通知新帧
	std::condition_variable m_queueSpaceCv;
	std::condition_variable m_roundDrainCv;
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
	VideoEncoderConfig m_jpegEncoderConfig;
	VideoEncoderConfig m_h264EncoderRuntimeConfig;
	bool m_jpegEncoderConfigured = false;
	bool m_h264EncoderConfigured = false;
	std::string m_lastCodecFallbackReason;
	std::atomic<bool> m_encoderResetRequested{ true };
	std::atomic<bool> m_encoderKeyFrameRequested{ true };
	std::atomic<unsigned long long> m_tcpPacketCounter{ 0 };
	std::int64_t m_lastTcpPerfLogNs = 0;
	DdsVideoPublisherConfig m_ddsConfig;
	LocalMp4RecorderConfig m_recordingConfig;
	std::unique_ptr<DdsVideoPublisher> m_ddsPublisher;
	std::shared_ptr<DdsRuntimeManager> m_ddsRuntime;
	std::unique_ptr<LocalMp4Recorder> m_localRecorder;
	std::atomic<bool> m_ddsEnabled{ false };
	std::atomic<bool> m_outputRoundActive{ false };
	std::atomic<unsigned long long> m_roundFrameSequence{ 0 };
	std::atomic<unsigned long long> m_roundLastCompletedFrame{ 0 };
	std::atomic<unsigned long long> m_roundFramesInFlight{ 0 };
	std::atomic<int> m_outputRound{ 0 };
	std::vector<std::uint8_t> m_ddsRawBuffer;
	std::atomic<unsigned long long> m_h264EncodeCounter{ 0 };
	std::atomic<unsigned long long> m_jpegEncodeCounter{ 0 };
	std::atomic<unsigned long long> m_ddsFrameCounter{ 0 };
	std::atomic<unsigned long long> m_recordFrameCounter{ 0 };
	std::atomic<unsigned long long> m_videoOutputFrameCounter{ 0 };
	std::int64_t m_lastVideoOutputPerfLogNs = 0;
};
