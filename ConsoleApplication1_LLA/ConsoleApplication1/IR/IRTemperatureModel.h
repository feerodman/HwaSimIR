#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

struct IRStage4Vec3
{
	float x;
	float y;
	float z;

	IRStage4Vec3();
	IRStage4Vec3(float xValue, float yValue, float zValue);
};

enum class IRHotspotKind
{
	EngineRear,
	EnginePlume,
	CustomThermal
};

enum class IRHotspotShape
{
	Sphere,
	Ellipsoid,
	Cone,
	Capsule
};

enum class IRBrightSpotPart
{
	None,
	Head,
	MidBody
};

struct IRHotspotState
{
	bool enabled;
	IRHotspotKind kind;
	IRHotspotShape shape;
	IRStage4Vec3 localPos;
	IRStage4Vec3 localDir;
	IRStage4Vec3 size;
	float targetTempK;
	float currentTempK;
	float ambientTempK;
	float heatTauSec;
	float coolTauSec;
	float intensity;

	IRHotspotState();
};

struct IRBrightSpotState
{
	bool enabled;
	IRBrightSpotPart part;
	IRStage4Vec3 localPos;
	float radius;
	float intensity;

	IRBrightSpotState();
};

const char* IRHotspotKindName(IRHotspotKind kind);
const char* IRHotspotShapeName(IRHotspotShape shape);
const char* IRBrightSpotPartName(IRBrightSpotPart part);

class IRTemperatureModel
{
public:
	struct PlatformProfile
	{
		bool engineRearEnabledByEngineState;
		IRHotspotState engineRear;
		IRBrightSpotState headBrightSpot;
		IRBrightSpotState midBrightSpot;

		PlatformProfile();
	};

	IRTemperatureModel();

	bool loadFromFileCandidates(const std::vector<std::string>& filePaths);
	bool load(const std::string& filePath);
	bool loaded() const;
	const std::string& loadedPath() const;

	IRHotspotState updateEngineRear(
		const std::string& platformName,
		const std::string& runtimeKey,
		bool engineState,
		float dtSec,
		float ambientTempK);

	IRBrightSpotState resolveBrightSpot(
		const std::string& platformName,
		bool strikeFlag,
		int strikePart,
		bool* invalidStrikePart) const;

	void resetRuntime();

private:
	PlatformProfile defaultProfileForPlatform(const std::string& platformName) const;
	const PlatformProfile& profileForPlatform(const std::string& platformName) const;
	std::string normalizePlatformName(const std::string& platformName) const;
	void warnOnce(const std::string& key, const std::string& message) const;

	std::map<std::string, PlatformProfile> m_profiles;
	std::map<std::string, IRHotspotState> m_runtimeEngineRear;
	PlatformProfile m_defaultProfile;
	std::string m_loadedPath;
	bool m_loaded;
	mutable std::set<std::string> m_warningKeys;
};
