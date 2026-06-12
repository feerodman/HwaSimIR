#pragma once

#include <cstdint>
#include <mutex>

struct IRFrameTelemetry
{
	std::uint64_t sourceSeq = 0;
	std::int64_t udpReceiveTimeNs = 0;
	std::int64_t processStartTimeNs = 0;
	int inputQueueDepth = 0;
	double readbackMs = 0.0;
};

struct IRFrameEnqueueResult
{
	bool accepted = false;
	bool queueWasFull = false;
	bool overwritten = false;
	int queueDepth = 0;
	double copyMs = 0.0;
	double queueWaitMs = 0.0;
};

class IRPerfStats
{
public:
	IRPerfStats();

	static std::int64_t wallTimeNs();
	static std::int64_t steadyTimeNs();

	void configure(bool syncMode, double videoFpsTarget);
	void setEnabled(bool enabled);
	void reset();

	void recordUdpFrame();
	void recordSceneUpdate(double elapsedMs);
	void recordAnnotation(double elapsedMs);
	void recordIrUpdate(double elapsedMs);
	void recordPlumeUpdate(double elapsedMs);
	void recordRender(double elapsedMs);
	void recordInputQueueDepth(int queueDepth);
	void recordCapture(double readbackMs, double resizeMs, double copyMs, int tcpQueueDepth);
	std::uint64_t recordTcpOutput(
		double jpegMs,
		double tcpSendMs,
		double latencyMs,
		int tcpQueueDepth,
		std::uint64_t outputSourceSeq,
		std::uint64_t latestUdpSourceSeq);
	void recordSyncOverrun();
	void recordInputQueueOverflow();
	void maybeLog();

	double videoFpsTarget() const;
	std::uint64_t outputFrames() const;

private:
	void resetIntervalLocked(std::int64_t nowNs);

	mutable std::mutex m_mutex;
	bool m_syncMode = true;
	bool m_enabled = true;
	double m_videoFpsTarget = 0.0;
	std::int64_t m_intervalStartNs = 0;
	std::uint64_t m_totalUdpFrames = 0;
	std::uint64_t m_totalRenderFrames = 0;
	std::uint64_t m_totalOutputFrames = 0;
	std::uint64_t m_syncOverrunCount = 0;
	std::uint64_t m_inputQueueOverflowCount = 0;
	std::uint64_t m_intervalUdpFrames = 0;
	std::uint64_t m_intervalRenderFrames = 0;
	std::uint64_t m_intervalOutputFrames = 0;
	std::uint64_t m_sceneSamples = 0;
	std::uint64_t m_annotationSamples = 0;
	std::uint64_t m_irSamples = 0;
	std::uint64_t m_plumeSamples = 0;
	std::uint64_t m_renderSamples = 0;
	std::uint64_t m_captureSamples = 0;
	std::uint64_t m_tcpSamples = 0;
	std::uint64_t m_latencySamples = 0;
	double m_sceneUpdateMsTotal = 0.0;
	double m_annotationMsTotal = 0.0;
	double m_irUpdateMsTotal = 0.0;
	double m_plumeUpdateMsTotal = 0.0;
	double m_renderMsTotal = 0.0;
	double m_readbackMsTotal = 0.0;
	double m_resizeMsTotal = 0.0;
	double m_copyMsTotal = 0.0;
	double m_jpegMsTotal = 0.0;
	double m_tcpSendMsTotal = 0.0;
	double m_latencyMsTotal = 0.0;
	double m_latencyMsMax = 0.0;
	int m_tcpQueueDepth = 0;
	int m_tcpQueueDepthMax = 0;
	int m_inputQueueDepth = 0;
	int m_inputQueueDepthMax = 0;
	std::uint64_t m_sourceSeqLag = 0;
	std::uint64_t m_lastLoggedOutputFrames = 0;
};
