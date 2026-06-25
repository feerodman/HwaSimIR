#pragma once

#include "AnnotationConfig.h"
#include "AnnotationOverlay.h"
#include "AnnotationProjector.h"

class AnnotationManager
{
public:
	void initialize(const NodePath& overlayRoot);
	bool loadProfileFromCandidates(const std::vector<std::string>& filePaths, const std::string& configuredPath, const std::string& source);
	void applyRuntimeOptions(const AnnotationRuntimeOptions& options);
	void setEnabled(bool enabled);
	bool isEnabled() const;
	void clear();

	AnnotationFrameRecord updateFrame(
		unsigned long long frameIndex,
		double simTimeMs,
		int sensorID,
		int width,
		int height,
		const std::vector<TargetPlatformData>& targets,
		const NodePath& renderRoot,
		const NodePath& cameraNode,
		Lens* cameraLens,
		bool drawOverlay = true,
		bool enableOcclusion = true);
	void reuseFrameMetadata(
		unsigned long long frameIndex,
		double simTimeMs,
		int sensorID,
		int width,
		int height);

	const AnnotationFrameRecord& latestRecord() const;
	const AnnotationProjector::PerfStats& lastPerfStats() const;

private:
	void logTargetRecord(const TargetAnnotation& target) const;

	bool m_initialized = false;
	bool m_enabled = false;
	unsigned long long m_updateCounter = 0;
	AnnotationConfig m_config;
	AnnotationProjector m_projector;
	AnnotationOverlay m_overlay;
	AnnotationFrameRecord m_latestRecord;
};
