#include "IRWeatherEffects.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <sstream>

namespace {

double Clamp(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

std::string Trim(const std::string& text)
{
	size_t first = 0;
	while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
	{
		++first;
	}
	size_t last = text.size();
	while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
	{
		--last;
	}
	return text.substr(first, last - first);
}

bool FileExists(const std::string& path)
{
	std::ifstream in(path.c_str(), std::ios::in | std::ios::binary);
	return in.good();
}

bool ReadWholeFile(const std::string& path, std::string& text)
{
	std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();
	return true;
}

std::string JoinPath(const std::string& directory, const std::string& fileName)
{
	if (directory.empty())
	{
		return fileName;
	}
	const char last = directory[directory.size() - 1];
	if (last == '/' || last == '\\')
	{
		return directory + fileName;
	}
	return directory + "/" + fileName;
}

bool FindJsonObject(const std::string& text, const std::string& key, std::string& objectText)
{
	const std::string token = "\"" + key + "\"";
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find('{', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	int depth = 0;
	for (size_t i = pos; i < text.size(); ++i)
	{
		if (text[i] == '{')
		{
			++depth;
		}
		else if (text[i] == '}')
		{
			--depth;
			if (depth == 0)
			{
				objectText = text.substr(pos, i - pos + 1);
				return true;
			}
		}
	}
	return false;
}

bool ExtractJsonNumber(const std::string& text, const std::string& key, double& value)
{
	const std::string token = "\"" + key + "\"";
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find_first_of("-+0123456789.", pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = begin;
	while (end < text.size())
	{
		char c = text[end];
		if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
		{
			break;
		}
		++end;
	}
	const std::string numberText = text.substr(begin, end - begin);
	char* parsedEnd = 0;
	const double parsed = std::strtod(numberText.c_str(), &parsedEnd);
	if (parsedEnd == numberText.c_str())
	{
		return false;
	}
	value = parsed;
	return true;
}

bool ExtractJsonBool(const std::string& text, const std::string& key, bool& value)
{
	const std::string token = "\"" + key + "\"";
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find_first_not_of(" \t\r\n", pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	if (text.compare(begin, 4, "true") == 0)
	{
		value = true;
		return true;
	}
	if (text.compare(begin, 5, "false") == 0)
	{
		value = false;
		return true;
	}
	double numeric = value ? 1.0 : 0.0;
	if (ExtractJsonNumber(text, key, numeric))
	{
		value = numeric != 0.0;
		return true;
	}
	return false;
}

bool ExtractJsonString(const std::string& text, const std::string& key, std::string& value)
{
	const std::string token = "\"" + key + "\"";
	size_t pos = text.find(token);
	if (pos == std::string::npos)
	{
		return false;
	}
	pos = text.find(':', pos + token.size());
	if (pos == std::string::npos)
	{
		return false;
	}
	size_t begin = text.find('"', pos + 1);
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = text.find('"', begin + 1);
	if (end == std::string::npos)
	{
		return false;
	}
	value = text.substr(begin + 1, end - begin - 1);
	return true;
}

void AssignNumber(const std::string& text, const std::string& key, double& target)
{
	double value = target;
	if (ExtractJsonNumber(text, key, value))
	{
		target = value;
	}
}

void AssignBool(const std::string& text, const std::string& key, bool& target)
{
	bool value = target;
	if (ExtractJsonBool(text, key, value))
	{
		target = value;
	}
}

void AssignString(const std::string& text, const std::string& key, std::string& target)
{
	std::string value = target;
	if (ExtractJsonString(text, key, value))
	{
		target = value;
	}
}

IRStage7PrecipitationType ParsePrecipitationType(const std::string& text)
{
	if (text == "rain" || text == "Rain")
	{
		return IRStage7PrecipitationType::Rain;
	}
	if (text == "snow" || text == "Snow")
	{
		return IRStage7PrecipitationType::Snow;
	}
	return IRStage7PrecipitationType::None;
}

int BandIndex(IRBand band)
{
	int index = static_cast<int>(band);
	return std::max(0, std::min(4, index));
}

double DefaultCloudGray(IRBand band, double cloudTemperatureK, double skyDiffuseScale)
{
	switch (band)
	{
	case IRBand::Visible:
	case IRBand::NearInfrared:
	case IRBand::ShortWaveInfrared:
		return Clamp(0.55 + 0.18 * skyDiffuseScale, 0.30, 0.86);
	case IRBand::MidWaveInfrared:
		return Clamp(0.34 + (cloudTemperatureK - 245.0) / 220.0, 0.24, 0.58);
	case IRBand::LongWaveInfrared:
		return Clamp(0.30 + (cloudTemperatureK - 230.0) / 120.0, 0.22, 0.74);
	default:
		return 0.45;
	}
}

} // namespace

IRWeatherEffects::IRWeatherEffects()
{
	resetDefaults();
}

void IRWeatherEffects::resetDefaults()
{
	m_profiles.clear();
	m_texturePaths.clear();
	m_textureBasePath = "Config/Weather/Textures";
	m_profilesLoaded = false;
	m_texturesLoaded = false;
	m_profilePath = "fallback";
	m_textureConfigPath = "fallback";

	const char* names[] = { "Clear", "Cloudy", "Rain", "Snow", "Fog", "Overcast" };
	for (int i = 0; i < 6; ++i)
	{
		IRStage7WeatherProfile profile;
		profile.envSky = i;
		profile.name = names[i];
		if (i == 1 || i == 5)
		{
			profile.cloudEnable = true;
			profile.cloudCoverage = i == 5 ? 0.9 : 0.5;
			profile.cloudOpacity = i == 5 ? 0.7 : 0.45;
			profile.sunDirectScale = i == 5 ? 0.18 : 0.55;
			profile.skyDiffuseScale = i == 5 ? 0.68 : 0.90;
			profile.targetContrastScale = i == 5 ? 0.70 : 0.90;
			profile.cloudTexture = i == 5 ? "cloud_overcast" : "cloud_scattered";
		}
		else if (i == 2)
		{
			profile.cloudEnable = true;
			profile.cloudCoverage = 0.8;
			profile.cloudOpacity = 0.65;
			profile.fogEnable = true;
			profile.fogDensity = 0.2;
			profile.precipitationEnable = true;
			profile.precipitationType = IRStage7PrecipitationType::Rain;
			profile.precipitationDensity = 0.55;
			profile.precipitationSpeed = 1.3;
			profile.sunDirectScale = 0.25;
			profile.skyDiffuseScale = 0.62;
			profile.targetContrastScale = 0.72;
			profile.cloudTexture = "cloud_storm";
		}
		else if (i == 3)
		{
			profile.cloudEnable = true;
			profile.cloudCoverage = 0.75;
			profile.cloudOpacity = 0.58;
			profile.fogEnable = true;
			profile.fogDensity = 0.14;
			profile.precipitationEnable = true;
			profile.precipitationType = IRStage7PrecipitationType::Snow;
			profile.precipitationDensity = 0.48;
			profile.precipitationSpeed = 0.55;
			profile.sunDirectScale = 0.38;
			profile.skyDiffuseScale = 0.78;
			profile.targetContrastScale = 0.82;
			profile.cloudTexture = "cloud_overcast";
		}
		else if (i == 4)
		{
			profile.fogEnable = true;
			profile.fogDensity = 0.62;
			profile.fogColorGray = 0.50;
			profile.sunDirectScale = 0.08;
			profile.skyDiffuseScale = 0.50;
			profile.targetContrastScale = 0.45;
		}
		m_profiles[i] = profile;
	}
	m_texturePaths["cloud_cumulus"] = JoinPath(m_textureBasePath, "cloud_cumulus.png");
	m_texturePaths["cloud_scattered"] = JoinPath(m_textureBasePath, "cloud_scattered.png");
	m_texturePaths["cloud_overcast"] = JoinPath(m_textureBasePath, "cloud_overcast.png");
	m_texturePaths["cloud_storm"] = JoinPath(m_textureBasePath, "cloud_storm.png");
	m_texturePaths["cloud_few"] = JoinPath(m_textureBasePath, "cloud_few.png");
	m_texturePaths["rain_shaft"] = JoinPath(m_textureBasePath, "rain_shaft.png");
	m_texturePaths["snow_rgba"] = JoinPath(m_textureBasePath, "snow.rgba");
	m_texturePaths["sun"] = JoinPath(m_textureBasePath, "sun.png");
	m_texturePaths["moon"] = JoinPath(m_textureBasePath, "moon.png");
}

bool IRWeatherEffects::loadProfilesFromCandidates(const std::vector<std::string>& candidates)
{
	resetDefaults();
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (loadProfileFile(candidates[i]))
		{
			m_profilesLoaded = true;
			m_profilePath = candidates[i];
			return true;
		}
	}
	return false;
}

bool IRWeatherEffects::loadTextureConfigFromCandidates(const std::vector<std::string>& candidates)
{
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		if (loadTextureFile(candidates[i]))
		{
			m_texturesLoaded = true;
			m_textureConfigPath = candidates[i];
			return true;
		}
	}
	return false;
}

bool IRWeatherEffects::loadProfileFile(const std::string& path)
{
	std::string text;
	if (!ReadWholeFile(path, text))
	{
		return false;
	}
	std::string profilesText;
	if (!FindJsonObject(text, "profiles", profilesText))
	{
		profilesText = text;
	}

	const char* names[] = { "Clear", "Cloudy", "Rain", "Snow", "Fog", "Overcast" };
	for (int i = 0; i < 6; ++i)
	{
		std::string objectText;
		if (!FindJsonObject(profilesText, names[i], objectText))
		{
			continue;
		}
		IRStage7WeatherProfile profile = m_profiles[i];
		profile.envSky = i;
		profile.name = names[i];
		const char* suffixes[] = { "VIS", "NIR", "SWIR", "MWIR", "LWIR" };
		for (int band = 0; band < 5; ++band)
		{
			AssignNumber(objectText, std::string("skyGrayScale_") + suffixes[band], profile.skyGrayScale[band]);
			AssignNumber(objectText, std::string("groundGrayScale_") + suffixes[band], profile.groundGrayScale[band]);
		}
		AssignBool(objectText, "cloudEnable", profile.cloudEnable);
		AssignNumber(objectText, "cloudCoverage", profile.cloudCoverage);
		AssignNumber(objectText, "cloudOpacity", profile.cloudOpacity);
		AssignNumber(objectText, "cloudTemperatureK", profile.cloudTemperatureK);
		AssignString(objectText, "cloudTexture", profile.cloudTexture);
		AssignBool(objectText, "fogEnable", profile.fogEnable);
		AssignNumber(objectText, "fogDensity", profile.fogDensity);
		AssignNumber(objectText, "fogColorGray", profile.fogColorGray);
		AssignBool(objectText, "precipitationEnable", profile.precipitationEnable);
		std::string precipitationType = precipitationName(profile.precipitationType);
		AssignString(objectText, "precipitationType", precipitationType);
		profile.precipitationType = ParsePrecipitationType(precipitationType);
		AssignNumber(objectText, "precipitationDensity", profile.precipitationDensity);
		AssignNumber(objectText, "precipitationSpeed", profile.precipitationSpeed);
		AssignNumber(objectText, "sunDirectScale", profile.sunDirectScale);
		AssignNumber(objectText, "skyDiffuseScale", profile.skyDiffuseScale);
		AssignNumber(objectText, "targetContrastScale", profile.targetContrastScale);
		m_profiles[i] = profile;
	}
	return true;
}

bool IRWeatherEffects::loadTextureFile(const std::string& path)
{
	std::string text;
	if (!ReadWholeFile(path, text))
	{
		return false;
	}
	AssignString(text, "basePath", m_textureBasePath);
	const char* keys[] = {
		"cloud_cumulus", "cloud_scattered", "cloud_overcast", "cloud_storm", "cloud_few",
		"rain_shaft", "rain_rgba", "snow_rgba", "sun", "moon"
	};
	for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
	{
		std::string fileName;
		if (ExtractJsonString(text, keys[i], fileName))
		{
			m_texturePaths[keys[i]] = JoinPath(m_textureBasePath, fileName);
		}
	}
	return true;
}

const IRStage7WeatherProfile& IRWeatherEffects::profileForEnvSky(int envSky) const
{
	std::map<int, IRStage7WeatherProfile>::const_iterator it = m_profiles.find(envSky);
	if (it != m_profiles.end())
	{
		return it->second;
	}
	return m_profiles.find(0)->second;
}

std::string IRWeatherEffects::texturePathForKey(const std::string& key) const
{
	std::map<std::string, std::string>::const_iterator it = m_texturePaths.find(key);
	if (it != m_texturePaths.end())
	{
		return it->second;
	}
	return std::string();
}

IRStage7WeatherState IRWeatherEffects::evaluate(const IRStage7WeatherRuntimeInput& input,
	IRBand band,
	bool enableWeatherEffects,
	bool enableCloudLayer,
	bool enableFog,
	bool enablePrecipitation) const
{
	const int envSky = (input.useUdpInput && input.envSky >= 0 && input.envSky <= 5) ? input.envSky : 0;
	const IRStage7WeatherProfile& profile = profileForEnvSky(envSky);
	IRStage7WeatherState state;
	state.enabled = enableWeatherEffects;
	state.envSky = envSky;
	state.weatherName = profile.name;
	state.visibilityM = std::isfinite(input.visibilityM) && input.visibilityM > 1.0 ? input.visibilityM : 23000.0;
	state.humidity = Clamp(input.humidity, 0.0, 100.0);
	state.windV = std::isfinite(input.windV) ? Clamp(input.windV, 0.0, 120.0) : 0.0;
	state.windDir = std::isfinite(input.windDir) ? input.windDir : 0.0;
	state.skyGrayScale = profile.skyGrayScale[BandIndex(band)] * Clamp(input.envRadScaleSky, 0.2, 5.0);
	state.groundGrayScale = profile.groundGrayScale[BandIndex(band)] * Clamp(input.envRadScaleTerrain, 0.2, 5.0);
	state.cloudEnable = enableWeatherEffects && enableCloudLayer && profile.cloudEnable;
	state.cloudCoverage = state.cloudEnable ? Clamp(profile.cloudCoverage + std::max(0.0, state.humidity - 70.0) * 0.003, 0.0, 1.0) : 0.0;
	state.cloudOpacity = state.cloudEnable ? Clamp(profile.cloudOpacity, 0.0, 1.0) : 0.0;
	state.cloudTemperatureK = Clamp(profile.cloudTemperatureK, 180.0, 330.0);
	state.cloudGray = DefaultCloudGray(band, state.cloudTemperatureK, profile.skyDiffuseScale);
	state.cloudTextureKey = profile.cloudTexture;
	state.cloudTexturePath = texturePathForKey(profile.cloudTexture);
	state.cloudTextureFound = !state.cloudTexturePath.empty() && FileExists(state.cloudTexturePath);
	state.fogEnable = enableWeatherEffects && enableFog && profile.fogEnable;
	const double visibilityFog = Clamp((12000.0 - state.visibilityM) / 12000.0, 0.0, 0.80);
	state.fogDensity = state.fogEnable ? Clamp(std::max(profile.fogDensity, visibilityFog), 0.0, 0.92) : 0.0;
	state.fogGray = Clamp(profile.fogColorGray, 0.0, 1.0);
	state.precipitationType = (enableWeatherEffects && enablePrecipitation && profile.precipitationEnable)
		? profile.precipitationType : IRStage7PrecipitationType::None;
	state.precipitationDensity = (state.precipitationType != IRStage7PrecipitationType::None)
		? Clamp(profile.precipitationDensity, 0.0, 1.0) : 0.0;
	state.precipitationSpeed = (state.precipitationType != IRStage7PrecipitationType::None)
		? std::max(0.0, profile.precipitationSpeed * Clamp(input.envRainSnowSpeedScale, 0.1, 5.0)) : 0.0;
	if (state.precipitationType == IRStage7PrecipitationType::Rain)
	{
		state.maxHeight = input.envMaxHeightRain;
		state.transHeight = input.envTransHeightRain;
	}
	else if (state.precipitationType == IRStage7PrecipitationType::Snow)
	{
		state.maxHeight = input.envMaxHeightSnow;
		state.transHeight = input.envTransHeightSnow;
	}
	state.sunDirectScale = enableWeatherEffects ? Clamp(profile.sunDirectScale, 0.0, 1.5) : 1.0;
	state.skyDiffuseScale = enableWeatherEffects ? Clamp(profile.skyDiffuseScale, 0.0, 1.5) : 1.0;
	state.targetContrastScale = enableWeatherEffects ? Clamp(profile.targetContrastScale, 0.05, 1.5) : 1.0;
	state.source = m_profilesLoaded ? "UDP+weather_profiles+RuntimeConfig" : "UDP+default+RuntimeConfig";
	return state;
}

const char* IRWeatherEffects::precipitationName(IRStage7PrecipitationType type)
{
	switch (type)
	{
	case IRStage7PrecipitationType::Rain: return "rain";
	case IRStage7PrecipitationType::Snow: return "snow";
	default: return "none";
	}
}

int IRWeatherEffects::precipitationCode(IRStage7PrecipitationType type)
{
	return static_cast<int>(type);
}
