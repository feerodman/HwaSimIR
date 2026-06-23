#include "IRPerfStats.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{
double Average(double total, std::uint64_t samples)
{
	return samples > 0 ? total / static_cast<double>(samples) : 0.0;
}
}

IRPerfStats::IRPerfStats()
{
	reset();
}

std::int64_t IRPerfStats::wallTimeNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t IRPerfStats::steadyTimeNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

void IRPerfStats::configure(bool syncMode, double videoFpsTarget)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_syncMode = syncMode;
	m_videoFpsTarget = std::max(0.0, videoFpsTarget);
}

void IRPerfStats::setEnabled(bool enabled)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_enabled = enabled;
}

void IRPerfStats::reset()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_totalUdpFrames = 0;
	m_totalRenderFrames = 0;
	m_totalOutputFrames = 0;
	m_syncOverrunCount = 0;
	m_inputQueueOverflowCount = 0;
	m_lastLoggedOutputFrames = 0;
	resetIntervalLocked(steadyTimeNs());
}

void IRPerfStats::recordUdpFrame()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	++m_totalUdpFrames;
	++m_intervalUdpFrames;
}

void IRPerfStats::recordSceneUpdate(double elapsedMs)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sceneUpdateMsTotal += elapsedMs;
	++m_sceneSamples;
}

void IRPerfStats::recordAnnotation(double elapsedMs)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_annotationMsTotal += elapsedMs;
	++m_annotationSamples;
}

void IRPerfStats::recordIrUpdate(double elapsedMs)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_irUpdateMsTotal += elapsedMs;
	++m_irSamples;
}

void IRPerfStats::recordIrUpdateBreakdown(const IRUpdateBreakdown& breakdown)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (breakdown.timingSample)
	{
		m_irEnvBuildMsTotal += breakdown.irEnvBuildMs;
		m_stage7SkyGroundMsTotal += breakdown.stage7SkyGroundMs;
		m_platformRadianceMsTotal += breakdown.platformRadianceMs;
		m_targetRadianceMsTotal += breakdown.targetRadianceMs;
		m_stage4HotspotMsTotal += breakdown.stage4HotspotMs;
		m_stage5PlumeBreakdownMsTotal += breakdown.stage5PlumeMs;
		m_stage5RadianceComponentMsTotal += breakdown.stage5RadianceComponentMs;
		m_stage5AeroThermalMsTotal += breakdown.stage5AeroThermalMs;
		m_stage5ModtranLookupMsTotal += breakdown.stage5ModtranLookupMs;
		m_shaderInputApplyMsTotal += breakdown.shaderInputApplyMs;
		++m_irBreakdownSamples;
	}
	m_shaderInputSetCountTotal += breakdown.shaderInputSetCount;
	m_shaderInputSkipCountTotal += breakdown.shaderInputSkipCount;
	m_stage5ModtranCacheHitCountTotal += breakdown.stage5ModtranCacheHitCount;
	m_stage5ModtranCacheMissCountTotal += breakdown.stage5ModtranCacheMissCount;
	m_stage7FullUpdateCountTotal += breakdown.stage7FullUpdateCount;
	m_stage7PositionOnlyCountTotal += breakdown.stage7PositionOnlyCount;
	m_stage7SkipCountTotal += breakdown.stage7SkipCount;
	m_stage4UpdateCountTotal += breakdown.stage4UpdateCount;
	m_stage4SkipCountTotal += breakdown.stage4SkipCount;
}

void IRPerfStats::recordPlumeUpdate(double elapsedMs)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_plumeUpdateMsTotal += elapsedMs;
	++m_plumeSamples;
}

void IRPerfStats::recordRender(
	double elapsedMs,
	double stage6MtfBlurMs,
	bool mtfBlurEnabled,
	double mtfBlurSigmaPixels,
	int mtfBlurRadiusPixels,
	double stage6DetectorNoiseMs,
	bool detectorNoiseEnabled,
	double temporalNoiseSigmaGray,
	double fpnSigmaGray,
	double columnNoiseSigmaGray,
	double rowNoiseSigmaGray,
	double badPixelRatio,
	const char* noisePosition)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_renderMsTotal += elapsedMs;
	m_stage6MtfBlurMsTotal += std::max(0.0, stage6MtfBlurMs);
	m_mtfBlurEnabled = mtfBlurEnabled;
	m_mtfBlurSigmaPixels = std::max(0.0, mtfBlurSigmaPixels);
	m_mtfBlurRadiusPixels = std::max(0, mtfBlurRadiusPixels);
	m_stage6DetectorNoiseMsTotal += std::max(0.0, stage6DetectorNoiseMs);
	m_detectorNoiseEnabled = detectorNoiseEnabled;
	m_temporalNoiseSigmaGray = std::max(0.0, temporalNoiseSigmaGray);
	m_fpnSigmaGray = std::max(0.0, fpnSigmaGray);
	m_columnNoiseSigmaGray = std::max(0.0, columnNoiseSigmaGray);
	m_rowNoiseSigmaGray = std::max(0.0, rowNoiseSigmaGray);
	m_badPixelRatio = std::max(0.0, badPixelRatio);
	m_noisePosition = noisePosition != nullptr ? noisePosition : "BeforeAGC";
	++m_renderSamples;
	++m_totalRenderFrames;
	++m_intervalRenderFrames;
}

void IRPerfStats::recordStage6Agc(
	double statsMs,
	double applyMs,
	bool agcEnabled,
	const char* agcMode,
	double agcGain,
	double agcOffset,
	double agcLowInput,
	double agcHighInput,
	int agcSampleCount)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stage6AgcStatsMsTotal += std::max(0.0, statsMs);
	m_stage6AgcApplyMsTotal += std::max(0.0, applyMs);
	++m_stage6AgcSamples;
	m_agcEnabled = agcEnabled;
	m_agcMode = agcMode != nullptr ? agcMode : "Off";
	m_agcGain = agcGain;
	m_agcOffset = agcOffset;
	m_agcLowInput = agcLowInput;
	m_agcHighInput = agcHighInput;
	m_agcSampleCount = std::max(0, agcSampleCount);
}

void IRPerfStats::recordInputQueueDepth(int queueDepth)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_inputQueueDepth = std::max(0, queueDepth);
	m_inputQueueDepthMax = std::max(m_inputQueueDepthMax, m_inputQueueDepth);
}

void IRPerfStats::recordCapture(double readbackMs, double resizeMs, double copyMs, int tcpQueueDepth)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_readbackMsTotal += readbackMs;
	m_resizeMsTotal += resizeMs;
	m_copyMsTotal += copyMs;
	++m_captureSamples;
	m_tcpQueueDepth = std::max(0, tcpQueueDepth);
	m_tcpQueueDepthMax = std::max(m_tcpQueueDepthMax, m_tcpQueueDepth);
}

std::uint64_t IRPerfStats::recordTcpOutput(
	double jpegMs,
	double tcpSendMs,
	double latencyMs,
	int tcpQueueDepth,
	std::uint64_t outputSourceSeq,
	std::uint64_t latestUdpSourceSeq)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_jpegMsTotal += jpegMs;
	m_tcpSendMsTotal += tcpSendMs;
	++m_tcpSamples;
	if (latencyMs >= 0.0)
	{
		m_latencyMsTotal += latencyMs;
		m_latencyMsMax = std::max(m_latencyMsMax, latencyMs);
		++m_latencySamples;
	}
	m_tcpQueueDepth = std::max(0, tcpQueueDepth);
	m_tcpQueueDepthMax = std::max(m_tcpQueueDepthMax, m_tcpQueueDepth);
	m_sourceSeqLag = latestUdpSourceSeq >= outputSourceSeq
		? latestUdpSourceSeq - outputSourceSeq
		: 0;
	++m_totalOutputFrames;
	++m_intervalOutputFrames;
	return m_totalOutputFrames;
}

void IRPerfStats::recordSyncOverrun()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	++m_syncOverrunCount;
}

void IRPerfStats::recordInputQueueOverflow()
{
	std::lock_guard<std::mutex> lock(m_mutex);
	++m_inputQueueOverflowCount;
	++m_syncOverrunCount;
}

void IRPerfStats::maybeLog()
{
	std::string message;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_enabled)
		{
			return;
		}
		const std::int64_t nowNs = steadyTimeNs();
		const double elapsedSec = std::max(0.001, static_cast<double>(nowNs - m_intervalStartNs) / 1.0e9);
		const bool firstOutput = m_totalOutputFrames > 0 && m_lastLoggedOutputFrames == 0;
		const bool frameInterval = m_intervalRenderFrames >= 120 || m_intervalOutputFrames >= 120;
		const bool timeInterval = elapsedSec >= 2.0;
		if (!firstOutput && !frameInterval && !timeInterval)
		{
			return;
		}

		std::ostringstream out;
		const std::uint64_t shaderInputTotal = m_shaderInputSetCountTotal + m_shaderInputSkipCountTotal;
		const double shaderInputHitRate = shaderInputTotal > 0
			? (static_cast<double>(m_shaderInputSkipCountTotal) * 100.0 / static_cast<double>(shaderInputTotal))
			: 0.0;
		const double udpFps = static_cast<double>(m_intervalUdpFrames) / elapsedSec;
		const double renderFps = static_cast<double>(m_intervalRenderFrames) / elapsedSec;
		const double outputFps = static_cast<double>(m_intervalOutputFrames) / elapsedSec;
		const double annotationMs = Average(m_annotationMsTotal, m_annotationSamples);
		const double irUpdateMs = Average(m_irUpdateMsTotal, m_irSamples);
		const double stage7SkyGroundMs = Average(m_stage7SkyGroundMsTotal, m_irBreakdownSamples);
		const double stage4HotspotMs = Average(m_stage4HotspotMsTotal, m_irBreakdownSamples);
		const double stage5RadianceComponentMs = Average(m_stage5RadianceComponentMsTotal, m_irBreakdownSamples);
		const double stage5AeroThermalMs = Average(m_stage5AeroThermalMsTotal, m_irBreakdownSamples);
		const double stage5ModtranLookupMs = Average(m_stage5ModtranLookupMsTotal, m_irBreakdownSamples);
		const double shaderInputApplyMs = Average(m_shaderInputApplyMsTotal, m_irBreakdownSamples);
		const double renderMs = Average(m_renderMsTotal, m_renderSamples);
		const double stage6MtfBlurMs = Average(m_stage6MtfBlurMsTotal, m_renderSamples);
		const double stage6DetectorNoiseMs = Average(m_stage6DetectorNoiseMsTotal, m_renderSamples);
		const double stage6AgcStatsMs = Average(m_stage6AgcStatsMsTotal, m_stage6AgcSamples);
		const double stage6AgcApplyMs = Average(m_stage6AgcApplyMsTotal, m_stage6AgcSamples);
		const double readbackMs = Average(m_readbackMsTotal, m_captureSamples);
		const double jpegMs = Average(m_jpegMsTotal, m_tcpSamples);
		const double tcpSendMs = Average(m_tcpSendMsTotal, m_tcpSamples);
		out << std::fixed << std::setprecision(3)
			<< "[Perf]"
			<< " mode=" << (m_syncMode ? "sync" : "async")
			<< " videoFpsTarget=" << m_videoFpsTarget
			<< " udpFps=" << udpFps
			<< " renderFps=" << renderFps
			<< " outputFps=" << outputFps
			<< " sceneUpdateMs=" << Average(m_sceneUpdateMsTotal, m_sceneSamples)
			<< " annotationMs=" << annotationMs
			<< " irUpdateMs=" << irUpdateMs
			<< " irEnvBuildMs=" << Average(m_irEnvBuildMsTotal, m_irBreakdownSamples)
			<< " stage7SkyGroundMs=" << stage7SkyGroundMs
			<< " platformRadianceMs=" << Average(m_platformRadianceMsTotal, m_irBreakdownSamples)
			<< " targetRadianceMs=" << Average(m_targetRadianceMsTotal, m_irBreakdownSamples)
			<< " stage4HotspotMs=" << stage4HotspotMs
			<< " stage5PlumeMs=" << Average(m_stage5PlumeBreakdownMsTotal, m_irBreakdownSamples)
			<< " stage5RadianceComponentMs=" << stage5RadianceComponentMs
			<< " stage5AeroThermalMs=" << stage5AeroThermalMs
			<< " stage5ModtranLookupMs=" << stage5ModtranLookupMs
			<< " shaderInputApplyMs=" << shaderInputApplyMs
			<< " shaderInputApplyScope=exclusive"
			<< " stage7SkyGroundScope=inclusive"
			<< " stage4HotspotScope=inclusive"
			<< " shaderInputSetCount=" << m_shaderInputSetCountTotal
			<< " shaderInputSkipCount=" << m_shaderInputSkipCountTotal
			<< " shaderInputCacheHitRate=" << shaderInputHitRate
			<< " stage5ModtranCacheHitCount=" << m_stage5ModtranCacheHitCountTotal
			<< " stage5ModtranCacheMissCount=" << m_stage5ModtranCacheMissCountTotal
			<< " stage7FullUpdateCount=" << m_stage7FullUpdateCountTotal
			<< " stage7PositionOnlyCount=" << m_stage7PositionOnlyCountTotal
			<< " stage7SkipCount=" << m_stage7SkipCountTotal
			<< " stage4UpdateCount=" << m_stage4UpdateCountTotal
			<< " stage4SkipCount=" << m_stage4SkipCountTotal
			<< " plumeUpdateMs=" << Average(m_plumeUpdateMsTotal, m_plumeSamples)
			<< " renderMs=" << renderMs
			<< " stage6MtfBlurMs=" << stage6MtfBlurMs
			<< " stage6MtfBlurScope=render_pass_upper_bound"
			<< " mtfBlurEnabled=" << (m_mtfBlurEnabled ? "1" : "0")
			<< " mtfBlurSigmaPixels=" << m_mtfBlurSigmaPixels
			<< " mtfBlurRadiusPixels=" << m_mtfBlurRadiusPixels
			<< " stage6DetectorNoiseMs=" << stage6DetectorNoiseMs
			<< " stage6DetectorNoiseScope=render_pass_upper_bound"
			<< " detectorNoiseEnabled=" << (m_detectorNoiseEnabled ? "1" : "0")
			<< " temporalNoiseSigmaGray=" << m_temporalNoiseSigmaGray
			<< " fpnSigmaGray=" << m_fpnSigmaGray
			<< " columnNoiseSigmaGray=" << m_columnNoiseSigmaGray
			<< " rowNoiseSigmaGray=" << m_rowNoiseSigmaGray
			<< " badPixelRatio=" << m_badPixelRatio
			<< " noisePosition=" << m_noisePosition
			<< " stage6AgcStatsMs=" << stage6AgcStatsMs
			<< " stage6AgcApplyMs=" << stage6AgcApplyMs
			<< " agcEnabled=" << (m_agcEnabled ? "1" : "0")
			<< " agcMode=" << m_agcMode
			<< " agcGain=" << m_agcGain
			<< " agcOffset=" << m_agcOffset
			<< " agcLowInput=" << m_agcLowInput
			<< " agcHighInput=" << m_agcHighInput
			<< " agcSampleCount=" << m_agcSampleCount
			<< " readbackMs=" << readbackMs
			<< " resizeMs=" << Average(m_resizeMsTotal, m_captureSamples)
			<< " frameCopyMs=" << Average(m_copyMsTotal, m_captureSamples)
			<< " jpegMs=" << jpegMs
			<< " tcpSendMs=" << tcpSendMs
			<< " tcpQueueDepth=" << m_tcpQueueDepth
			<< " tcpQueueDepthMax=" << m_tcpQueueDepthMax
			<< " inputQueueDepth=" << m_inputQueueDepth
			<< " inputQueueDepthMax=" << m_inputQueueDepthMax
			<< " sourceSeqLag=" << m_sourceSeqLag
			<< " latencyAvgMs=" << Average(m_latencyMsTotal, m_latencySamples)
			<< " latencyMaxMs=" << m_latencyMsMax
			<< " syncOverrunCount=" << m_syncOverrunCount
			<< " inputQueueOverflowCount=" << m_inputQueueOverflowCount
			<< " udpFrames=" << m_totalUdpFrames
			<< " renderFrames=" << m_totalRenderFrames
			<< " outputFrames=" << m_totalOutputFrames;
		const bool belowTargetFps = m_videoFpsTarget >= 59.5 && outputFps > 0.0 && outputFps < 59.5;
		const bool significantBacklog = m_inputQueueDepth > 2 || m_sourceSeqLag > 2;
		const bool sustainedBelowTarget = outputFps < (m_videoFpsTarget - 1.0);
		const bool inputStillActive =
			udpFps >= (m_videoFpsTarget * 0.8) ||
			significantBacklog ||
			m_intervalOutputFrames >= static_cast<std::uint64_t>(std::max(30.0, m_videoFpsTarget));
		const bool afterWarmup = m_totalOutputFrames >= 120;
		if (belowTargetFps && inputStillActive && afterWarmup && (sustainedBelowTarget || significantBacklog))
		{
			out << "\n[Perf][WARN]"
				<< " outputFps=" << outputFps
				<< " targetFps=" << m_videoFpsTarget
				<< " bottleneckSummary=below_60hz"
				<< " irUpdateMs=" << irUpdateMs
				<< " stage5ModtranLookupMs=" << stage5ModtranLookupMs
				<< " stage5RadianceComponentMs=" << stage5RadianceComponentMs
				<< " stage5AeroThermalMs=" << stage5AeroThermalMs
				<< " stage4HotspotMs=" << stage4HotspotMs
				<< " stage7SkyGroundMs=" << stage7SkyGroundMs
				<< " shaderInputApplyMs=" << shaderInputApplyMs
				<< " annotationMs=" << annotationMs
				<< " renderMs=" << renderMs
				<< " stage6MtfBlurMs=" << stage6MtfBlurMs
				<< " stage6DetectorNoiseMs=" << stage6DetectorNoiseMs
				<< " stage6AgcStatsMs=" << stage6AgcStatsMs
				<< " readbackMs=" << readbackMs
				<< " jpegMs=" << jpegMs
				<< " tcpSendMs=" << tcpSendMs
				<< " inputQueueDepth=" << m_inputQueueDepth
				<< " sourceSeqLag=" << m_sourceSeqLag
				<< " note=recorderWriteMs_is_reported_by_VideoDisplay_RecorderPerf";
		}
		message = out.str();
		m_lastLoggedOutputFrames = m_totalOutputFrames;
		resetIntervalLocked(nowNs);
	}
	std::cout << message << std::endl;
}

double IRPerfStats::videoFpsTarget() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_videoFpsTarget;
}

std::uint64_t IRPerfStats::outputFrames() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_totalOutputFrames;
}

void IRPerfStats::resetIntervalLocked(std::int64_t nowNs)
{
	m_intervalStartNs = nowNs;
	m_intervalUdpFrames = 0;
	m_intervalRenderFrames = 0;
	m_intervalOutputFrames = 0;
	m_sceneSamples = 0;
	m_annotationSamples = 0;
	m_irSamples = 0;
	m_irBreakdownSamples = 0;
	m_plumeSamples = 0;
	m_renderSamples = 0;
	m_captureSamples = 0;
	m_tcpSamples = 0;
	m_latencySamples = 0;
	m_sceneUpdateMsTotal = 0.0;
	m_annotationMsTotal = 0.0;
	m_irUpdateMsTotal = 0.0;
	m_irEnvBuildMsTotal = 0.0;
	m_stage7SkyGroundMsTotal = 0.0;
	m_platformRadianceMsTotal = 0.0;
	m_targetRadianceMsTotal = 0.0;
	m_stage4HotspotMsTotal = 0.0;
	m_stage5PlumeBreakdownMsTotal = 0.0;
	m_stage5RadianceComponentMsTotal = 0.0;
	m_stage5AeroThermalMsTotal = 0.0;
	m_stage5ModtranLookupMsTotal = 0.0;
	m_shaderInputApplyMsTotal = 0.0;
	m_shaderInputSetCountTotal = 0;
	m_shaderInputSkipCountTotal = 0;
	m_stage5ModtranCacheHitCountTotal = 0;
	m_stage5ModtranCacheMissCountTotal = 0;
	m_stage7FullUpdateCountTotal = 0;
	m_stage7PositionOnlyCountTotal = 0;
	m_stage7SkipCountTotal = 0;
	m_stage4UpdateCountTotal = 0;
	m_stage4SkipCountTotal = 0;
	m_plumeUpdateMsTotal = 0.0;
	m_renderMsTotal = 0.0;
	m_stage6MtfBlurMsTotal = 0.0;
	m_stage6DetectorNoiseMsTotal = 0.0;
	m_stage6AgcStatsMsTotal = 0.0;
	m_stage6AgcApplyMsTotal = 0.0;
	m_stage6AgcSamples = 0;
	m_readbackMsTotal = 0.0;
	m_resizeMsTotal = 0.0;
	m_copyMsTotal = 0.0;
	m_jpegMsTotal = 0.0;
	m_tcpSendMsTotal = 0.0;
	m_latencyMsTotal = 0.0;
	m_latencyMsMax = 0.0;
	m_tcpQueueDepth = 0;
	m_tcpQueueDepthMax = 0;
	m_inputQueueDepth = 0;
	m_inputQueueDepthMax = 0;
	m_sourceSeqLag = 0;
}
