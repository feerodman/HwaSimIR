#pragma once

#include "IRTemperatureModel.h"
#include "IRTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

struct IREnginePlumeBandGain
{
	float vis;
	float nir;
	float swir;
	float mwir;
	float lwir;

	IREnginePlumeBandGain();
	float forBand(IRBand band) const;
};

struct IREnginePlumeLayerProfile
{
	bool enabled;
	float temperatureK;
	float opacity;
	float lengthM;
	float radiusRootM;
	float radiusTailM;
	float axialDecay;
	float radialDecay;
	float noiseScale;
	float noiseStrength;
	IREnginePlumeBandGain bandGain;

	IREnginePlumeLayerProfile();
};

struct IREnginePlumeProfile
{
	bool enabled;
	bool enabledByEngineState;
	IRStage4Vec3 localPos;
	IRStage4Vec3 localDir;
	float ambientMixK;
	float heatTauSec;
	float coolTauSec;
	IREnginePlumeLayerProfile core;
	IREnginePlumeLayerProfile halo;

	IREnginePlumeProfile();
};

struct IREnginePlumeRuntimeOptions
{
	bool enableEnginePlume;
	bool useEngineState;
	bool useProceduralNoise;
	bool enablePlumeDebug;
	bool forcePlumeVisible;
	float displayGain;
	float coreDisplayGain;
	float haloDisplayGain;
	float opacityScale;
	float coreOpacityScale;
	float haloOpacityScale;
	int maxPlumeNodes;

	IREnginePlumeRuntimeOptions();
};

struct IREnginePlumeInput
{
	std::string platformName;
	std::string runtimeKey;
	bool engineState;
	float dtSec;
	float ambientTempK;
	IRBand band;
	IREnginePlumeRuntimeOptions options;

	IREnginePlumeInput();
};

struct IREnginePlumeOutput
{
	bool enabled;
	bool profileEnabled;
	bool nodeVisible;
	IRStage4Vec3 localPos;
	IRStage4Vec3 localDir;
	bool coreEnabled;
	bool haloEnabled;
	bool coreNodeVisible;
	bool haloNodeVisible;
	float coreTempK;
	float haloTempK;
	float coreTargetTempK;
	float haloTargetTempK;
	float coreGray;
	float haloGray;
	float coreOpacity;
	float haloOpacity;
	float coreLengthM;
	float haloLengthM;
	float coreRadiusRootM;
	float coreRadiusTailM;
	float haloRadiusRootM;
	float haloRadiusTailM;
	float coreAxialDecay;
	float haloAxialDecay;
	float coreRadialDecay;
	float haloRadialDecay;
	float coreNoiseScale;
	float haloNoiseScale;
	float coreNoiseStrength;
	float haloNoiseStrength;
	float coreBandGain;
	float haloBandGain;
	float coreRadiance;
	float haloRadiance;

	IREnginePlumeOutput();
};

class IREnginePlumeModel
{
public:
	IREnginePlumeModel();

	bool loadFromFileCandidates(const std::vector<std::string>& filePaths);
	bool load(const std::string& filePath);
	bool loaded() const;
	const std::string& loadedPath() const;

	IREnginePlumeOutput update(const IREnginePlumeInput& input);
	const IREnginePlumeProfile& profileForPlatform(const std::string& platformName) const;
	void resetRuntime();
	static IREnginePlumeLayerProfile deriveHaloLayer(const IREnginePlumeLayerProfile& core);

private:
	std::string normalizePlatformName(const std::string& platformName) const;
	void warnOnce(const std::string& key, const std::string& message) const;
	static float clamp(float value, float low, float high);
	static float approachTemperature(float current, float target, float tauSec, float dtSec);
	static float planckRadiance(float wavelengthUm, float temperatureK);
	static float bandCenterUm(IRBand band);
	static float computeLayerGray(const IREnginePlumeLayerProfile& layer, float currentTempK, float targetTempK, float ambientMixK, IRBand band, float displayGain, float opacityScale, float& radianceOut, float& opacityOut, float& bandGainOut);

	std::map<std::string, IREnginePlumeProfile> m_profiles;
	std::map<std::string, float> m_runtimeTemperatureK;
	IREnginePlumeProfile m_defaultProfile;
	std::string m_loadedPath;
	bool m_loaded;
	mutable std::set<std::string> m_warningKeys;
};
