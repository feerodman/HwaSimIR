#pragma once

#include "AnnotationTypes.h"
#include "luse.h"
#include "CommonDefine.h"

#include <map>
#include <string>
#include <vector>

// 模型头部估算使用的局部坐标轴，后续可按模型朝向逐类校准。
enum AnnotationForwardAxis
{
	AnnotationAxisPositiveX,
	AnnotationAxisNegativeX,
	AnnotationAxisPositiveY,
	AnnotationAxisNegativeY,
	AnnotationAxisPositiveZ,
	AnnotationAxisNegativeZ
};

struct AnnotationBBoxConfig
{
	std::string mode = "mesh_body";
	int marginPx = 3;
	int minSizePx = 4;
	std::vector<std::string> excludeNodeNameContains;
};

struct AnnotationKeyPointConfig
{
	bool visible = true;
	bool hasLocalPos = false;
	LPoint3f localPos = LPoint3f(0.0f, 0.0f, 0.0f);
	bool surface = true;
	bool snapToMeshSurface = true;
	std::string surfaceSearchMode = "nearest_mesh_vertex";
};

struct AnnotationOcclusionConfig
{
	bool enabled = true;
	std::string mode = "mesh_collision";
	float epsilonM = 0.25f;
	int collisionMaskBit = 20;
	bool selfTarget = false;
};

struct TargetAnnotationConfig
{
	bool enabled = true;
	std::string label;
	AnnotationBBoxConfig bbox;
	bool keyPointsEnabled = true;
	AnnotationKeyPointConfig headPoint;
	AnnotationKeyPointConfig middlePoint;
	AnnotationForwardAxis forwardAxis = AnnotationAxisPositiveY;
};

struct AnnotationRuntimeOptions
{
	std::string profilePath = "Config/Annotation/annotation_profiles.json";
	std::string profilePathSource = "default";
	AnnotationDrawOptions drawOptions;
	std::string bboxMode = "mesh_body";
	int bboxMarginPx = 3;
	int minBBoxSizePx = 4;
	AnnotationOcclusionConfig occlusion;
	bool surfaceKeyPointEnabled = true;
	bool surfaceSnapEnabled = false;
	std::string surfaceSnapMode = "profile_surface";
};

class AnnotationConfig
{
public:
	AnnotationConfig();

	bool loadFromCandidates(const std::vector<std::string>& filePaths, const std::string& configuredPath, const std::string& source);
	void applyRuntimeOptions(const AnnotationRuntimeOptions& options);

	const TargetAnnotationConfig& configForPlatform(PLATFORM_TYPE type) const;
	std::string labelForTargetType(int targetType) const;
	const AnnotationDrawOptions& drawOptions() const;
	const AnnotationOcclusionConfig& occlusion() const;
	bool surfaceKeyPointEnabled() const;
	bool surfaceSnapEnabled() const;
	const std::string& surfaceSnapMode() const;
	bool loaded() const;
	const std::string& loadedPath() const;
	const std::string& configuredPath() const;

private:
	void resetToFallback();

	std::map<PLATFORM_TYPE, TargetAnnotationConfig> m_configs;
	TargetAnnotationConfig m_defaultConfig;
	AnnotationDrawOptions m_drawOptions;
	AnnotationOcclusionConfig m_occlusion;
	bool m_surfaceKeyPointEnabled = true;
	bool m_surfaceSnapEnabled = false;
	std::string m_surfaceSnapMode = "profile_surface";
	bool m_loaded = false;
	std::string m_loadedPath;
	std::string m_configuredPath = "Config/Annotation/annotation_profiles.json";
};
