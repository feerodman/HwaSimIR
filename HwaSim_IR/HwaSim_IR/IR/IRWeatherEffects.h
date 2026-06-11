#pragma once

#include "IRTypes.h"

#include <map>
#include <string>
#include <vector>

enum class IRStage7PrecipitationType
{
	None = 0,
	Rain = 1,
	Snow = 2
};

struct IRStage7WeatherProfile
{
	int envSky = 0;
	std::string name = "Clear";
	double skyGrayScale[5] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
	double groundGrayScale[5] = { 1.0, 1.0, 1.0, 1.0, 1.0 };
	bool cloudEnable = false;
	double cloudCoverage = 0.0;
	double cloudOpacity = 0.0;
	double cloudTemperatureK = 255.0;
	std::string cloudTexture = "cloud_few";
	bool fogEnable = false;
	double fogDensity = 0.0;
	double fogColorGray = 0.45;
	bool precipitationEnable = false;
	IRStage7PrecipitationType precipitationType = IRStage7PrecipitationType::None;
	double precipitationDensity = 0.0;
	double precipitationSpeed = 0.0;
	double sunDirectScale = 1.0;
	double skyDiffuseScale = 1.0;
	double targetContrastScale = 1.0;
};

struct IRStage7WeatherRuntimeInput
{
	bool useUdpInput = true;
	int envSky = 0;
	int envTerrain = 0;
	double visibilityM = 23000.0;
	double humidity = 40.0;
	double windV = 0.0;
	double windDir = 0.0;
	double envTempC = 25.0;
	double envRadScaleSky = 1.0;
	double envRadScaleTerrain = 1.0;
	double envMaxHeightRain = 0.0;
	double envTransHeightRain = 0.0;
	double envMaxHeightSnow = 0.0;
	double envTransHeightSnow = 0.0;
	double envRainSnowSpeedScale = 1.0;
};

struct IRStage7WeatherState
{
	bool enabled = false;
	int envSky = 0;
	std::string weatherName = "Clear";
	double visibilityM = 23000.0;
	double humidity = 40.0;
	double windV = 0.0;
	double windDir = 0.0;
	double skyGrayScale = 1.0;
	double groundGrayScale = 1.0;
	bool cloudEnable = false;
	double cloudCoverage = 0.0;
	double cloudOpacity = 0.0;
	double cloudTemperatureK = 255.0;
	double cloudGray = 0.5;
	std::string cloudTextureKey = "cloud_few";
	std::string cloudTexturePath;
	bool cloudTextureFound = false;
	bool fogEnable = false;
	double fogDensity = 0.0;
	double fogGray = 0.45;
	IRStage7PrecipitationType precipitationType = IRStage7PrecipitationType::None;
	double precipitationDensity = 0.0;
	double precipitationSpeed = 0.0;
	double maxHeight = 0.0;
	double transHeight = 0.0;
	double sunDirectScale = 1.0;
	double skyDiffuseScale = 1.0;
	double targetContrastScale = 1.0;
	std::string source = "default";
};

class IRWeatherEffects
{
public:
	IRWeatherEffects();

	bool loadProfilesFromCandidates(const std::vector<std::string>& candidates);
	bool loadTextureConfigFromCandidates(const std::vector<std::string>& candidates);

	const std::string& profilePath() const { return m_profilePath; }
	const std::string& textureConfigPath() const { return m_textureConfigPath; }
	bool profilesLoaded() const { return m_profilesLoaded; }
	bool texturesLoaded() const { return m_texturesLoaded; }

	IRStage7WeatherState evaluate(const IRStage7WeatherRuntimeInput& input,
		IRBand band,
		bool enableWeatherEffects,
		bool enableCloudLayer,
		bool enableFog,
		bool enablePrecipitation) const;

	const IRStage7WeatherProfile& profileForEnvSky(int envSky) const;
	std::string texturePathForKey(const std::string& key) const;

	static const char* precipitationName(IRStage7PrecipitationType type);
	static int precipitationCode(IRStage7PrecipitationType type);

private:
	void resetDefaults();
	bool loadProfileFile(const std::string& path);
	bool loadTextureFile(const std::string& path);

	std::map<int, IRStage7WeatherProfile> m_profiles;
	std::map<std::string, std::string> m_texturePaths;
	bool m_profilesLoaded = false;
	bool m_texturesLoaded = false;
	std::string m_profilePath = "fallback";
	std::string m_textureConfigPath = "fallback";
	std::string m_textureBasePath = "Config/Weather/Textures";
};
