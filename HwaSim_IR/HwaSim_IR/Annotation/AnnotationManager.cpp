#include "AnnotationManager.h"

#include <chrono>
#include <iostream>
#include <sstream>

namespace
{
double NowMs()
{
	return std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}
}

void AnnotationManager::initialize(const NodePath& overlayRoot)
{
	m_overlay.initialize(overlayRoot);
	m_initialized = !overlayRoot.is_empty();
	clear();
}

bool AnnotationManager::loadProfileFromCandidates(const std::vector<std::string>& filePaths, const std::string& configuredPath, const std::string& source)
{
	return m_config.loadFromCandidates(filePaths, configuredPath, source);
}

void AnnotationManager::applyRuntimeOptions(const AnnotationRuntimeOptions& options)
{
	m_config.applyRuntimeOptions(options);
}

void AnnotationManager::setEnabled(bool enabled)
{
	if (m_enabled == enabled)
	{
		return;
	}

	m_enabled = enabled;
	std::cout << "[Annotation] enabled=" << (m_enabled ? "1" : "0") << std::endl;
	if (!m_enabled)
	{
		clear();
	}
}

bool AnnotationManager::isEnabled() const
{
	return m_enabled;
}

void AnnotationManager::clear()
{
	m_latestRecord = AnnotationFrameRecord();
	if (m_initialized)
	{
		m_overlay.clear();
	}
}

AnnotationFrameRecord AnnotationManager::updateFrame(
	unsigned long long frameIndex,
	double simTimeMs,
	int sensorID,
	int width,
	int height,
	const std::vector<TargetPlatformData>& targets,
	const NodePath& renderRoot,
	const NodePath& cameraNode,
	Lens* cameraLens,
	bool drawOverlay,
	bool enableOcclusion)
{
	if (!m_enabled || !m_initialized)
	{
		clear();
		return m_latestRecord;
	}

	AnnotationFrameRecord record;
	record.frameIndex = frameIndex;
	record.simTimeMs = simTimeMs;
	record.sensorID = sensorID;
	record.width = width;
	record.height = height;

	if (width <= 0 || height <= 0 || renderRoot.is_empty() || cameraNode.is_empty() || cameraLens == nullptr)
	{
		m_latestRecord = record;
		if (drawOverlay)
		{
			m_overlay.clear();
		}
		return m_latestRecord;
	}

	const double totalBeginMs = NowMs();
	m_projector.beginFrame(frameIndex, targets, m_config, renderRoot, enableOcclusion);
	for (size_t i = 0; i < targets.size(); ++i)
	{
		TargetAnnotation targetAnnotation;
		if (m_projector.buildTargetAnnotation(targets[i], targets, m_config, renderRoot, cameraNode, cameraLens, width, height, targetAnnotation))
		{
			record.targets.push_back(targetAnnotation);
		}
	}

	m_latestRecord = record;
	double overlayMs = 0.0;
	if (drawOverlay)
	{
		const double overlayBeginMs = NowMs();
		m_overlay.drawFrame(m_latestRecord, m_config.drawOptions());
		overlayMs = NowMs() - overlayBeginMs;
	}
	else
	{
		m_overlay.clear();
	}
	const double totalMs = NowMs() - totalBeginMs;
	++m_updateCounter;

	if (m_updateCounter <= 3 || (m_updateCounter % 120) == 0)
	{
		std::cout << "[Annotation] frame=" << m_latestRecord.frameIndex
			<< " size=" << m_latestRecord.width << "x" << m_latestRecord.height
			<< " targets=" << m_latestRecord.targets.size()
			<< std::endl;
		for (size_t i = 0; i < m_latestRecord.targets.size(); ++i)
		{
			logTargetRecord(m_latestRecord.targets[i]);
		}
		const AnnotationProjector::PerfStats& perf = m_projector.perfStats();
		std::cout << "[AnnotationPerf]"
			<< " frame=" << m_latestRecord.frameIndex
			<< " targets=" << m_latestRecord.targets.size()
			<< " keypoints=" << perf.keypoints
			<< " drawOverlay=" << (drawOverlay ? "1" : "0")
			<< " occlusionEnabled=" << (enableOcclusion ? "1" : "0")
			<< " bboxMs=" << perf.bboxMs
			<< " collisionBuildMs=" << perf.collisionBuildMs
			<< " surfaceMs=" << perf.surfaceMs
			<< " occlusionMs=" << perf.occlusionMs
			<< " overlayMs=" << overlayMs
			<< " totalMs=" << totalMs
			<< " collisionBuilds=" << perf.collisionBuilds
			<< " collisionReused=" << perf.collisionReused
			<< " collisionTriangles=" << perf.collisionTriangles
			<< std::endl;
	}

	return m_latestRecord;
}

void AnnotationManager::reuseFrameMetadata(
	unsigned long long frameIndex,
	double simTimeMs,
	int sensorID,
	int width,
	int height)
{
	m_latestRecord.frameIndex = frameIndex;
	m_latestRecord.simTimeMs = simTimeMs;
	m_latestRecord.sensorID = sensorID;
	m_latestRecord.width = width;
	m_latestRecord.height = height;
}

const AnnotationFrameRecord& AnnotationManager::latestRecord() const
{
	return m_latestRecord;
}

const AnnotationProjector::PerfStats& AnnotationManager::lastPerfStats() const
{
	return m_projector.perfStats();
}

void AnnotationManager::logTargetRecord(const TargetAnnotation& target) const
{
	std::ostringstream log;
	log << "[Annotation]"
		<< " targetID=" << target.targetID
		<< " targetPlatID=" << target.targetPlatID
		<< " type=" << target.modelLabel
		<< " bbox=(" << target.bbox.x << "," << target.bbox.y << ","
		<< target.bbox.width << "," << target.bbox.height << ")";
	for (size_t i = 0; i < target.keyPoints.size(); ++i)
	{
		const AnnotationPoint2D& point = target.keyPoints[i];
		log << " " << point.name << "=(" << point.x << "," << point.y << ")";
	}
	std::cout << log.str() << std::endl;
}
