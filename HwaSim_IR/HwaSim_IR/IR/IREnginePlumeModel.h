#pragma once

#include "IRTemperatureModel.h"
#include "IRTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

// Effective band emissivity of the unresolved plume gas.  Values are
// dimensionless and physically bounded to [0, 1].  Opacity is kept separate
// so the renderer can apply each factor exactly once.
struct IREnginePlumeBandEmissivity
{
	float vis;
	float nir;
	float swir;
	float mwir;
	float lwir;

	IREnginePlumeBandEmissivity();
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
	IREnginePlumeBandEmissivity bandEmissivity;

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
	// Formal physical outputs.  Source radiance excludes opacity because the
	// transparent plume layer applies opacity during compositing.  Emitted
	// radiance is supplied separately for audit/reference calculations.
	float coreEffectiveEmissivity;
	float haloEffectiveEmissivity;
	float coreSourceRadianceWm2SrUm;
	float haloSourceRadianceWm2SrUm;
	float coreEmittedRadianceWm2SrUm;
	float haloEmittedRadianceWm2SrUm;
	// Compatibility aliases for the pre-P11 application call site.  These now
	// carry bounded emissivity and SI source radiance, never an empirical gain.
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
	static double bandAveragePlanckRadianceWm2SrUm(IRBand band, double temperatureK);
	static double layerSourceRadianceWm2SrUm(IRBand band, double temperatureK,
		double ambientTemperatureK, double effectiveEmissivity);

private:
	std::string normalizePlatformName(const std::string& platformName) const;
	void warnOnce(const std::string& key, const std::string& message) const;
	static float clamp(float value, float low, float high);
	static float approachTemperature(float current, float target, float tauSec, float dtSec);
	static float computeLayerGray(const IREnginePlumeLayerProfile& layer, float currentTempK,
		float targetTempK, float ambientMixK, IRBand band, float displayGain,
		float opacityScale, float& sourceRadianceOut, float& emittedRadianceOut,
		float& opacityOut, float& emissivityOut);

	std::map<std::string, IREnginePlumeProfile> m_profiles;
	std::map<std::string, float> m_runtimeTemperatureK;
	IREnginePlumeProfile m_defaultProfile;
	std::string m_loadedPath;
	bool m_loaded;
	mutable std::set<std::string> m_warningKeys;
};
