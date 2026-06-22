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

struct IRUpdateBreakdown
{
	bool timingSample = false;
	double irEnvBuildMs = 0.0;
	double stage7SkyGroundMs = 0.0;
	double platformRadianceMs = 0.0;
	double targetRadianceMs = 0.0;
	double stage4HotspotMs = 0.0;
	double stage5PlumeMs = 0.0;
	double stage5RadianceComponentMs = 0.0;
	double stage5AeroThermalMs = 0.0;
	double stage5ModtranLookupMs = 0.0;
	double shaderInputApplyMs = 0.0;
	std::uint64_t shaderInputSetCount = 0;
	std::uint64_t shaderInputSkipCount = 0;
	std::uint64_t stage5ModtranCacheHitCount = 0;
	std::uint64_t stage5ModtranCacheMissCount = 0;
	std::uint64_t stage7FullUpdateCount = 0;
	std::uint64_t stage7PositionOnlyCount = 0;
	std::uint64_t stage7SkipCount = 0;
	std::uint64_t stage4UpdateCount = 0;
	std::uint64_t stage4SkipCount = 0;
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
	void recordIrUpdateBreakdown(const IRUpdateBreakdown& breakdown);
	void recordPlumeUpdate(double elapsedMs);
	void recordRender(double elapsedMs, double stage6MtfBlurMs = 0.0, bool mtfBlurEnabled = false, double mtfBlurSigmaPixels = 0.0, int mtfBlurRadiusPixels = 0);
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
	std::uint64_t m_irBreakdownSamples = 0;
	std::uint64_t m_plumeSamples = 0;
	std::uint64_t m_renderSamples = 0;
	std::uint64_t m_captureSamples = 0;
	std::uint64_t m_tcpSamples = 0;
	std::uint64_t m_latencySamples = 0;
	double m_sceneUpdateMsTotal = 0.0;
	double m_annotationMsTotal = 0.0;
	double m_irUpdateMsTotal = 0.0;
	double m_irEnvBuildMsTotal = 0.0;
	double m_stage7SkyGroundMsTotal = 0.0;
	double m_platformRadianceMsTotal = 0.0;
	double m_targetRadianceMsTotal = 0.0;
	double m_stage4HotspotMsTotal = 0.0;
	double m_stage5PlumeBreakdownMsTotal = 0.0;
	double m_stage5RadianceComponentMsTotal = 0.0;
	double m_stage5AeroThermalMsTotal = 0.0;
	double m_stage5ModtranLookupMsTotal = 0.0;
	double m_shaderInputApplyMsTotal = 0.0;
	std::uint64_t m_shaderInputSetCountTotal = 0;
	std::uint64_t m_shaderInputSkipCountTotal = 0;
	std::uint64_t m_stage5ModtranCacheHitCountTotal = 0;
	std::uint64_t m_stage5ModtranCacheMissCountTotal = 0;
	std::uint64_t m_stage7FullUpdateCountTotal = 0;
	std::uint64_t m_stage7PositionOnlyCountTotal = 0;
	std::uint64_t m_stage7SkipCountTotal = 0;
	std::uint64_t m_stage4UpdateCountTotal = 0;
	std::uint64_t m_stage4SkipCountTotal = 0;
	double m_plumeUpdateMsTotal = 0.0;
	double m_renderMsTotal = 0.0;
	double m_stage6MtfBlurMsTotal = 0.0;
	bool m_mtfBlurEnabled = false;
	double m_mtfBlurSigmaPixels = 0.0;
	int m_mtfBlurRadiusPixels = 0;
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
