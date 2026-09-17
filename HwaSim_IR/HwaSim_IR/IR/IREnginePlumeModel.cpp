#include "IREnginePlumeModel.h"
#include "IRRadianceModelV2.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
bool readWholeFile(const std::string& filePath, std::string& text)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();
	return true;
}

std::string makeJsonToken(const std::string& key)
{
	return "\"" + key + "\"";
}

bool matchJsonObject(const std::string& text, size_t openBrace, std::string& objectText)
{
	if (openBrace == std::string::npos || openBrace >= text.size() || text[openBrace] != '{')
	{
		return false;
	}
	bool inString = false;
	bool escaping = false;
	int depth = 0;
	for (size_t i = openBrace; i < text.size(); ++i)
	{
		char c = text[i];
		if (inString)
		{
			if (escaping)
			{
				escaping = false;
			}
			else if (c == '\\')
			{
				escaping = true;
			}
			else if (c == '"')
			{
				inString = false;
			}
			continue;
		}
		if (c == '"')
		{
			inString = true;
		}
		else if (c == '{')
		{
			++depth;
		}
		else if (c == '}')
		{
			--depth;
			if (depth == 0)
			{
				objectText = text.substr(openBrace, i - openBrace + 1);
				return true;
			}
		}
	}
	return false;
}

bool findJsonObject(const std::string& text, const std::string& key, std::string& objectText)
{
	const std::string token = makeJsonToken(key);
	size_t pos = text.find(token);
	while (pos != std::string::npos)
	{
		size_t colon = text.find(':', pos + token.size());
		if (colon == std::string::npos)
		{
			return false;
		}
		size_t brace = text.find('{', colon + 1);
		if (brace != std::string::npos && matchJsonObject(text, brace, objectText))
		{
			return true;
		}
		pos = text.find(token, pos + token.size());
	}
	return false;
}

bool extractJsonNumber(const std::string& text, const std::string& key, float& value)
{
	const std::string token = makeJsonToken(key);
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
	try
	{
		value = std::stof(text.substr(begin, end - begin));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool extractJsonBool(const std::string& text, const std::string& key, bool& value)
{
	const std::string token = makeJsonToken(key);
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
	if (text.compare(begin, 4, "true") == 0 || text.compare(begin, 4, "True") == 0)
	{
		value = true;
		return true;
	}
	if (text.compare(begin, 5, "false") == 0 || text.compare(begin, 5, "False") == 0)
	{
		value = false;
		return true;
	}
	float numeric = value ? 1.0f : 0.0f;
	if (extractJsonNumber(text, key, numeric))
	{
		value = numeric != 0.0f;
		return true;
	}
	return false;
}

bool extractJsonVec3(const std::string& text, const std::string& key, IRStage4Vec3& value)
{
	const std::string token = makeJsonToken(key);
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
	size_t begin = text.find('[', pos + 1);
	size_t end = begin == std::string::npos ? std::string::npos : text.find(']', begin + 1);
	if (begin == std::string::npos || end == std::string::npos)
	{
		return false;
	}
	std::stringstream stream(text.substr(begin + 1, end - begin - 1));
	std::string item;
	float numbers[3] = { value.x, value.y, value.z };
	int count = 0;
	while (std::getline(stream, item, ',') && count < 3)
	{
		try
		{
			numbers[count] = std::stof(item);
			++count;
		}
		catch (...)
		{
			return false;
		}
	}
	if (count != 3)
	{
		return false;
	}
	value = IRStage4Vec3(numbers[0], numbers[1], numbers[2]);
	return true;
}

float clampUnit(float value)
{
	return std::isfinite(value) ? std::max(0.0f, std::min(1.0f, value)) : 0.0f;
}

void applyBandEmissivityObject(const std::string& text, IREnginePlumeBandEmissivity& emissivity)
{
	extractJsonNumber(text, "VIS", emissivity.vis);
	extractJsonNumber(text, "NIR", emissivity.nir);
	extractJsonNumber(text, "SWIR", emissivity.swir);
	extractJsonNumber(text, "MWIR", emissivity.mwir);
	extractJsonNumber(text, "LWIR", emissivity.lwir);
	emissivity.vis = clampUnit(emissivity.vis);
	emissivity.nir = clampUnit(emissivity.nir);
	emissivity.swir = clampUnit(emissivity.swir);
	emissivity.mwir = clampUnit(emissivity.mwir);
	emissivity.lwir = clampUnit(emissivity.lwir);
}

void applyLayerObject(const std::string& text, IREnginePlumeLayerProfile& layer)
{
	extractJsonBool(text, "enabled", layer.enabled);
	extractJsonNumber(text, "temperatureK", layer.temperatureK);
	extractJsonNumber(text, "opacity", layer.opacity);
	extractJsonNumber(text, "lengthM", layer.lengthM);
	extractJsonNumber(text, "radiusRootM", layer.radiusRootM);
	extractJsonNumber(text, "radiusTailM", layer.radiusTailM);
	extractJsonNumber(text, "axialDecay", layer.axialDecay);
	extractJsonNumber(text, "radialDecay", layer.radialDecay);
	extractJsonNumber(text, "noiseScale", layer.noiseScale);
	extractJsonNumber(text, "noiseStrength", layer.noiseStrength);

	std::string bandEmissivityText;
	if (findJsonObject(text, "bandEmissivity", bandEmissivityText))
	{
		applyBandEmissivityObject(bandEmissivityText, layer.bandEmissivity);
	}
	else if (findJsonObject(text, "bandGain", bandEmissivityText))
	{
		// Read-only compatibility with archived Stage5 profiles.  Legacy gains
		// are clamped to emissivity bounds and are never multiplied twice.
		applyBandEmissivityObject(bandEmissivityText, layer.bandEmissivity);
	}
}

void applyLegacyLayerFields(const std::string& text, IREnginePlumeLayerProfile& layer)
{
	extractJsonNumber(text, "temperatureK", layer.temperatureK);
	extractJsonNumber(text, "opacity", layer.opacity);
	extractJsonNumber(text, "lengthM", layer.lengthM);
	extractJsonNumber(text, "radiusRootM", layer.radiusRootM);
	extractJsonNumber(text, "radiusTailM", layer.radiusTailM);
	extractJsonNumber(text, "axialDecay", layer.axialDecay);
	extractJsonNumber(text, "radialDecay", layer.radialDecay);
	extractJsonNumber(text, "noiseScale", layer.noiseScale);
	extractJsonNumber(text, "noiseStrength", layer.noiseStrength);

	std::string bandEmissivityText;
	if (findJsonObject(text, "bandEmissivity", bandEmissivityText))
	{
		applyBandEmissivityObject(bandEmissivityText, layer.bandEmissivity);
	}
	else if (findJsonObject(text, "bandGain", bandEmissivityText))
	{
		applyBandEmissivityObject(bandEmissivityText, layer.bandEmissivity);
	}
}

void applyProfileObject(const std::string& text, IREnginePlumeProfile& profile)
{
	extractJsonBool(text, "enabled", profile.enabled);
	extractJsonBool(text, "enabledByEngineState", profile.enabledByEngineState);
	extractJsonVec3(text, "localPos", profile.localPos);
	extractJsonVec3(text, "localDir", profile.localDir);
	extractJsonNumber(text, "ambientMixK", profile.ambientMixK);
	extractJsonNumber(text, "heatTauSec", profile.heatTauSec);
	extractJsonNumber(text, "coolTauSec", profile.coolTauSec);

	std::string coreText;
	std::string haloText;
	const bool hasCore = findJsonObject(text, "core", coreText);
	const bool hasHalo = findJsonObject(text, "halo", haloText);
	if (hasCore)
	{
		applyLayerObject(coreText, profile.core);
	}
	else if (!hasHalo)
	{
		applyLegacyLayerFields(text, profile.core);
	}

	if (hasHalo)
	{
		applyLayerObject(haloText, profile.halo);
	}
	else
	{
		profile.halo = IREnginePlumeModel::deriveHaloLayer(profile.core);
	}
}
}

IREnginePlumeBandEmissivity::IREnginePlumeBandEmissivity()
	: vis(0.0f),
	nir(0.08f),
	swir(0.18f),
	mwir(1.0f),
	lwir(0.45f)
{
}

float IREnginePlumeBandEmissivity::forBand(IRBand band) const
{
	switch (band)
	{
	case IRBand::Visible: return vis;
	case IRBand::NearInfrared: return nir;
	case IRBand::ShortWaveInfrared: return swir;
	case IRBand::MidWaveInfrared: return mwir;
	case IRBand::LongWaveInfrared: return lwir;
	default: return mwir;
	}
}

IREnginePlumeLayerProfile::IREnginePlumeLayerProfile()
	: enabled(true),
	temperatureK(1450.0f),
	opacity(0.68f),
	lengthM(2.6f),
	radiusRootM(0.18f),
	radiusTailM(0.42f),
	axialDecay(3.2f),
	radialDecay(5.0f),
	noiseScale(6.0f),
	noiseStrength(0.10f)
{
}

IREnginePlumeProfile::IREnginePlumeProfile()
	: enabled(true),
	enabledByEngineState(true),
	localPos(0.0f, -1.2f, 0.0f),
	localDir(0.0f, -1.0f, 0.0f),
	ambientMixK(310.0f),
	heatTauSec(0.45f),
	coolTauSec(2.8f)
{
	halo = IREnginePlumeModel::deriveHaloLayer(core);
}

IREnginePlumeRuntimeOptions::IREnginePlumeRuntimeOptions()
	: enableEnginePlume(true),
	useEngineState(true),
	useProceduralNoise(true),
	enablePlumeDebug(false),
	forcePlumeVisible(false),
	displayGain(1.0f),
	coreDisplayGain(1.2f),
	haloDisplayGain(0.8f),
	opacityScale(1.0f),
	coreOpacityScale(1.0f),
	haloOpacityScale(1.0f),
	maxPlumeNodes(16)
{
}

IREnginePlumeInput::IREnginePlumeInput()
	: engineState(false),
	dtSec(0.033f),
	ambientTempK(300.0f),
	band(IRBand::MidWaveInfrared)
{
}

IREnginePlumeOutput::IREnginePlumeOutput()
	: enabled(false),
	profileEnabled(false),
	nodeVisible(false),
	localPos(0.0f, -1.2f, 0.0f),
	localDir(0.0f, -1.0f, 0.0f),
	coreEnabled(false),
	haloEnabled(false),
	coreNodeVisible(false),
	haloNodeVisible(false),
	coreTempK(300.0f),
	haloTempK(300.0f),
	coreTargetTempK(300.0f),
	haloTargetTempK(300.0f),
	coreGray(0.0f),
	haloGray(0.0f),
	coreOpacity(0.0f),
	haloOpacity(0.0f),
	coreLengthM(2.6f),
	haloLengthM(5.0f),
	coreRadiusRootM(0.18f),
	coreRadiusTailM(0.42f),
	haloRadiusRootM(0.32f),
	haloRadiusTailM(1.05f),
	coreAxialDecay(3.2f),
	haloAxialDecay(1.5f),
	coreRadialDecay(5.0f),
	haloRadialDecay(2.0f),
	coreNoiseScale(6.0f),
	haloNoiseScale(3.5f),
	coreNoiseStrength(0.0f),
	haloNoiseStrength(0.0f),
	coreEffectiveEmissivity(1.0f),
	haloEffectiveEmissivity(0.55f),
	coreSourceRadianceWm2SrUm(0.0f),
	haloSourceRadianceWm2SrUm(0.0f),
	coreEmittedRadianceWm2SrUm(0.0f),
	haloEmittedRadianceWm2SrUm(0.0f),
	coreBandGain(1.0f),
	haloBandGain(0.55f),
	coreRadiance(0.0f),
	haloRadiance(0.0f)
{
}

IREnginePlumeModel::IREnginePlumeModel()
	: m_loaded(false)
{
}

bool IREnginePlumeModel::loadFromFileCandidates(const std::vector<std::string>& filePaths)
{
	for (size_t i = 0; i < filePaths.size(); ++i)
	{
		if (load(filePaths[i]))
		{
			return true;
		}
	}
	return false;
}

bool IREnginePlumeModel::load(const std::string& filePath)
{
	std::string text;
	if (!readWholeFile(filePath, text))
	{
		return false;
	}

	m_profiles.clear();
	m_warningKeys.clear();
	m_defaultProfile = IREnginePlumeProfile();

	std::string defaultsText;
	if (findJsonObject(text, "defaults", defaultsText))
	{
		applyProfileObject(defaultsText, m_defaultProfile);
	}

	std::string platformsText;
	if (findJsonObject(text, "platforms", platformsText))
	{
		const char* platformNames[] = {
			"F35", "AIM120D", "AIM120", "AIM9X", "P11-CIVIL-VAN",
			"P11-CONTROLLED-SAMPLES"
		};
		for (size_t i = 0; i < sizeof(platformNames) / sizeof(platformNames[0]); ++i)
		{
			std::string platformText;
			if (findJsonObject(platformsText, platformNames[i], platformText))
			{
				IREnginePlumeProfile profile = m_defaultProfile;
				applyProfileObject(platformText, profile);
				m_profiles[normalizePlatformName(platformNames[i])] = profile;
			}
		}
	}

	if (m_profiles.find("AIM120") == m_profiles.end() && m_profiles.find("AIM120D") != m_profiles.end())
	{
		m_profiles["AIM120"] = m_profiles["AIM120D"];
	}

	m_loadedPath = filePath;
	m_loaded = true;
	return true;
}

bool IREnginePlumeModel::loaded() const
{
	return m_loaded;
}

const std::string& IREnginePlumeModel::loadedPath() const
{
	return m_loadedPath;
}

const IREnginePlumeProfile& IREnginePlumeModel::profileForPlatform(const std::string& platformName) const
{
	std::map<std::string, IREnginePlumeProfile>::const_iterator it = m_profiles.find(normalizePlatformName(platformName));
	if (it != m_profiles.end())
	{
		return it->second;
	}
	warnOnce("fallback-" + normalizePlatformName(platformName),
		"[Stage5 Plume][WARN] platform has no EnginePlume profile; using defaults. platform=" + platformName);
	return m_defaultProfile;
}

IREnginePlumeOutput IREnginePlumeModel::update(const IREnginePlumeInput& input)
{
	IREnginePlumeOutput output;
	const IREnginePlumeProfile& profile = profileForPlatform(input.platformName);
	output.profileEnabled = profile.enabled;
	output.localPos = profile.localPos;
	output.localDir = profile.localDir;

	const float ambientTempK = std::max(1.0f, input.ambientTempK);
	const float ambientMixK = std::max(ambientTempK, profile.ambientMixK);
	const bool strictCivilEngineGate =
		normalizePlatformName(input.platformName) == "P11CIVILVAN" &&
		profile.enabledByEngineState;
	// The civil reference exhaust is never synthesized while its protocol
	// engineState is false.  Diagnostic force/useEngineState bypasses remain
	// available to legacy military profiles only.
	const bool engineInput = strictCivilEngineGate
		? input.engineState
		: (input.options.useEngineState ? input.engineState : true);
	const bool forceVisible = input.options.forcePlumeVisible && !strictCivilEngineGate;
	const bool heating = forceVisible || engineInput || !profile.enabledByEngineState;
	const float dtSec = std::max(0.0f, input.dtSec);
	const float tau = heating ? std::max(0.01f, profile.heatTauSec) : std::max(0.01f, profile.coolTauSec);

	output.coreTargetTempK = heating ? std::max(ambientMixK, profile.core.temperatureK) : ambientMixK;
	output.haloTargetTempK = heating ? std::max(ambientMixK, profile.halo.temperatureK) : ambientMixK;

	const std::string runtimeBase = input.runtimeKey.empty() ? normalizePlatformName(input.platformName) : input.runtimeKey;
	float coreCurrent = ambientMixK;
	float haloCurrent = ambientMixK;
	std::map<std::string, float>::iterator coreIt = m_runtimeTemperatureK.find(runtimeBase + "#core");
	if (coreIt != m_runtimeTemperatureK.end())
	{
		coreCurrent = coreIt->second;
	}
	std::map<std::string, float>::iterator haloIt = m_runtimeTemperatureK.find(runtimeBase + "#halo");
	if (haloIt != m_runtimeTemperatureK.end())
	{
		haloCurrent = haloIt->second;
	}
	coreCurrent = approachTemperature(coreCurrent, output.coreTargetTempK, tau, dtSec);
	haloCurrent = approachTemperature(haloCurrent, output.haloTargetTempK, tau, dtSec);
	m_runtimeTemperatureK[runtimeBase + "#core"] = coreCurrent;
	m_runtimeTemperatureK[runtimeBase + "#halo"] = haloCurrent;
	output.coreTempK = coreCurrent;
	output.haloTempK = haloCurrent;

	output.coreLengthM = std::max(0.05f, profile.core.lengthM);
	output.haloLengthM = std::max(0.05f, profile.halo.lengthM);
	output.coreRadiusRootM = std::max(0.01f, profile.core.radiusRootM);
	output.coreRadiusTailM = std::max(0.01f, profile.core.radiusTailM);
	output.haloRadiusRootM = std::max(0.01f, profile.halo.radiusRootM);
	output.haloRadiusTailM = std::max(0.01f, profile.halo.radiusTailM);
	output.coreAxialDecay = std::max(0.01f, profile.core.axialDecay);
	output.haloAxialDecay = std::max(0.01f, profile.halo.axialDecay);
	output.coreRadialDecay = std::max(0.01f, profile.core.radialDecay);
	output.haloRadialDecay = std::max(0.01f, profile.halo.radialDecay);
	output.coreNoiseScale = std::max(0.0f, profile.core.noiseScale);
	output.haloNoiseScale = std::max(0.0f, profile.halo.noiseScale);
	output.coreNoiseStrength = input.options.useProceduralNoise ? clamp(profile.core.noiseStrength, 0.0f, 1.0f) : 0.0f;
	output.haloNoiseStrength = input.options.useProceduralNoise ? clamp(profile.halo.noiseStrength, 0.0f, 1.0f) : 0.0f;

	output.coreGray = computeLayerGray(profile.core, coreCurrent, output.coreTargetTempK, ambientMixK, input.band,
		input.options.displayGain * input.options.coreDisplayGain,
		input.options.opacityScale * input.options.coreOpacityScale,
		output.coreSourceRadianceWm2SrUm,
		output.coreEmittedRadianceWm2SrUm,
		output.coreOpacity,
		output.coreEffectiveEmissivity);
	output.haloGray = computeLayerGray(profile.halo, haloCurrent, output.haloTargetTempK, ambientMixK, input.band,
		input.options.displayGain * input.options.haloDisplayGain,
		input.options.opacityScale * input.options.haloOpacityScale,
		output.haloSourceRadianceWm2SrUm,
		output.haloEmittedRadianceWm2SrUm,
		output.haloOpacity,
		output.haloEffectiveEmissivity);
	// Keep source compatibility until HwaSimIR.cpp switches to the explicit P11
	// names.  No extra scaling is introduced by these aliases.
	output.coreBandGain = output.coreEffectiveEmissivity;
	output.haloBandGain = output.haloEffectiveEmissivity;
	output.coreRadiance = output.coreSourceRadianceWm2SrUm;
	output.haloRadiance = output.haloSourceRadianceWm2SrUm;

	const bool baseEnabled = input.options.enableEnginePlume && profile.enabled;
	output.coreEnabled = baseEnabled &&
		profile.core.enabled &&
		output.coreOpacity > 0.001f &&
		(forceVisible || (engineInput && output.coreEffectiveEmissivity > 0.001f));
	output.haloEnabled = baseEnabled &&
		profile.halo.enabled &&
		output.haloOpacity > 0.001f &&
		(forceVisible || (engineInput && output.haloEffectiveEmissivity > 0.001f));
	output.coreNodeVisible = output.coreEnabled && output.coreEmittedRadianceWm2SrUm > 1.0e-9f;
	output.haloNodeVisible = output.haloEnabled && output.haloEmittedRadianceWm2SrUm > 1.0e-9f;
	output.enabled = output.coreEnabled || output.haloEnabled;
	output.nodeVisible = output.coreNodeVisible || output.haloNodeVisible;
	return output;
}

void IREnginePlumeModel::resetRuntime()
{
	m_runtimeTemperatureK.clear();
}

std::string IREnginePlumeModel::normalizePlatformName(const std::string& platformName) const
{
	std::string normalized;
	for (size_t i = 0; i < platformName.size(); ++i)
	{
		char c = platformName[i];
		if (c >= 'a' && c <= 'z')
		{
			c = static_cast<char>(c - 'a' + 'A');
		}
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
		{
			normalized.push_back(c);
		}
	}
	return normalized.empty() ? "DEFAULT" : normalized;
}

void IREnginePlumeModel::warnOnce(const std::string& key, const std::string& message) const
{
	if (m_warningKeys.insert(key).second)
	{
		std::cout << message << std::endl;
	}
}

float IREnginePlumeModel::clamp(float value, float low, float high)
{
	return std::max(low, std::min(high, value));
}

float IREnginePlumeModel::approachTemperature(float current, float target, float tauSec, float dtSec)
{
	if (tauSec <= 0.0f)
	{
		return target;
	}
	const float alpha = 1.0f - std::exp(-std::max(0.0f, dtSec) / tauSec);
	return current + (target - current) * clamp(alpha, 0.0f, 1.0f);
}

double IREnginePlumeModel::bandAveragePlanckRadianceWm2SrUm(IRBand band, double temperatureK)
{
	// A single implementation owns the formal response integration so plume,
	// target body and hotspot terms cannot drift onto different wavelength
	// grids.  SWIR is 1.10--2.50 um (10 Simpson intervals); MWIR is 3--5 um
	// (4 intervals).  Returned units are W/(m^2 sr um).
	return IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(band, temperatureK);
}

double IREnginePlumeModel::layerSourceRadianceWm2SrUm(IRBand band, double temperatureK,
	double ambientTemperatureK, double effectiveEmissivity)
{
	const double emissivity = std::max(0.0, std::min(1.0, effectiveEmissivity));
	const double hot = bandAveragePlanckRadianceWm2SrUm(band, temperatureK);
	const double ambient = bandAveragePlanckRadianceWm2SrUm(band, ambientTemperatureK);
	return emissivity * std::max(0.0, hot - ambient);
}

float IREnginePlumeModel::computeLayerGray(const IREnginePlumeLayerProfile& layer,
	float currentTempK, float targetTempK, float ambientMixK, IRBand band,
	float displayGain, float opacityScale, float& sourceRadianceOut,
	float& emittedRadianceOut, float& opacityOut, float& emissivityOut)
{
	sourceRadianceOut = 0.0f;
	emittedRadianceOut = 0.0f;
	opacityOut = layer.enabled ? clamp(layer.opacity * opacityScale, 0.0f, 1.0f) : 0.0f;
	emissivityOut = layer.enabled ? clamp(layer.bandEmissivity.forBand(band), 0.0f, 1.0f) : 0.0f;
	if (!layer.enabled || opacityOut <= 0.001f || emissivityOut <= 0.001f)
	{
		return 0.0f;
	}

	const double sourceRadiance = layerSourceRadianceWm2SrUm(
		band, currentTempK, ambientMixK, emissivityOut);
	const double blackbodyTargetRadiance = std::max(1.0e-12,
		layerSourceRadianceWm2SrUm(
			band, std::max(targetTempK, ambientMixK + 1.0f), ambientMixK, 1.0));
	sourceRadianceOut = static_cast<float>(sourceRadiance);
	// Opacity is deliberately applied exactly once here for the emitted audit
	// value.  The renderer must use sourceRadianceOut as RGB and opacityOut as
	// alpha; multiplying RGB by opacity again would be a duplicate attenuation.
	emittedRadianceOut = static_cast<float>(sourceRadiance * opacityOut);
	if (displayGain <= 0.0f)
	{
		return 0.0f;
	}
	const float normalizedThermal = clamp(
		static_cast<float>(sourceRadiance / blackbodyTargetRadiance), 0.0f, 1.0f);
	return clamp(normalizedThermal * displayGain, 0.0f, 1.0f);
}

IREnginePlumeLayerProfile IREnginePlumeModel::deriveHaloLayer(const IREnginePlumeLayerProfile& core)
{
	IREnginePlumeLayerProfile halo = core;
	halo.enabled = core.enabled;
	halo.temperatureK = std::max(320.0f, core.temperatureK * 0.78f);
	halo.opacity = clamp(core.opacity * 0.46f, 0.05f, 0.65f);
	halo.lengthM = std::max(core.lengthM * 1.8f, core.lengthM + 0.6f);
	halo.radiusRootM = std::max(core.radiusRootM * 1.45f, core.radiusRootM + 0.04f);
	halo.radiusTailM = std::max(core.radiusTailM * 2.25f, core.radiusTailM + 0.25f);
	halo.axialDecay = std::max(0.25f, core.axialDecay * 0.46f);
	halo.radialDecay = std::max(0.25f, core.radialDecay * 0.42f);
	halo.noiseScale = std::max(0.5f, core.noiseScale * 0.58f);
	halo.noiseStrength = clamp(core.noiseStrength + 0.10f, 0.0f, 0.55f);
	halo.bandEmissivity.vis = clamp(core.bandEmissivity.vis * 0.25f, 0.0f, 1.0f);
	halo.bandEmissivity.nir = clamp(core.bandEmissivity.nir * 0.45f, 0.0f, 1.0f);
	halo.bandEmissivity.swir = clamp(core.bandEmissivity.swir * 0.55f, 0.0f, 1.0f);
	halo.bandEmissivity.mwir = clamp(core.bandEmissivity.mwir * 0.52f, 0.0f, 1.0f);
	halo.bandEmissivity.lwir = clamp(core.bandEmissivity.lwir * 0.62f, 0.0f, 1.0f);
	return halo;
}
