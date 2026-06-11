#include "IRTemperatureModel.h"

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
	return false;
}

bool extractJsonString(const std::string& text, const std::string& key, std::string& value)
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
	if (begin == std::string::npos)
	{
		return false;
	}
	size_t end = text.find(']', begin + 1);
	if (end == std::string::npos)
	{
		return false;
	}

	std::string listText = text.substr(begin + 1, end - begin - 1);
	std::stringstream stream(listText);
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

IRHotspotShape parseShape(const std::string& shapeText, IRHotspotShape fallback)
{
	if (shapeText == "Sphere")
	{
		return IRHotspotShape::Sphere;
	}
	if (shapeText == "Ellipsoid")
	{
		return IRHotspotShape::Ellipsoid;
	}
	if (shapeText == "Cone")
	{
		return IRHotspotShape::Cone;
	}
	if (shapeText == "Capsule")
	{
		return IRHotspotShape::Capsule;
	}
	return fallback;
}

float clampPositive(float value, float fallback)
{
	return value > 0.0001f ? value : fallback;
}

float clamp01(float value)
{
	return std::max(0.0f, std::min(1.0f, value));
}

void applyHotspotConfig(const std::string& objectText, IRTemperatureModel::PlatformProfile& profile)
{
	std::string engineRearText;
	if (!findJsonObject(objectText, "engineRear", engineRearText))
	{
		return;
	}

	extractJsonBool(engineRearText, "enabledByEngineState", profile.engineRearEnabledByEngineState);
	std::string shapeText;
	if (extractJsonString(engineRearText, "shape", shapeText))
	{
		profile.engineRear.shape = parseShape(shapeText, profile.engineRear.shape);
	}
	extractJsonVec3(engineRearText, "localPos", profile.engineRear.localPos);
	extractJsonVec3(engineRearText, "localDir", profile.engineRear.localDir);
	extractJsonVec3(engineRearText, "size", profile.engineRear.size);
	extractJsonNumber(engineRearText, "targetTempK", profile.engineRear.targetTempK);
	extractJsonNumber(engineRearText, "heatTauSec", profile.engineRear.heatTauSec);
	extractJsonNumber(engineRearText, "coolTauSec", profile.engineRear.coolTauSec);
	extractJsonNumber(engineRearText, "intensityScale", profile.engineRear.intensity);

	profile.engineRear.heatTauSec = clampPositive(profile.engineRear.heatTauSec, 1.0f);
	profile.engineRear.coolTauSec = clampPositive(profile.engineRear.coolTauSec, 5.0f);
	profile.engineRear.currentTempK = profile.engineRear.ambientTempK;
}

void applyBrightSpotConfig(const std::string& brightspotsText, const std::string& key, IRBrightSpotPart part, IRBrightSpotState& state)
{
	std::string spotText;
	if (!findJsonObject(brightspotsText, key, spotText))
	{
		return;
	}
	state.part = part;
	extractJsonVec3(spotText, "localPos", state.localPos);
	extractJsonNumber(spotText, "radius", state.radius);
	extractJsonNumber(spotText, "intensity", state.intensity);
	state.radius = clampPositive(state.radius, 0.2f);
	state.intensity = std::max(0.0f, state.intensity);
}

void applyBrightSpotsConfig(const std::string& objectText, IRTemperatureModel::PlatformProfile& profile)
{
	std::string brightspotsText;
	if (!findJsonObject(objectText, "brightspots", brightspotsText))
	{
		return;
	}
	applyBrightSpotConfig(brightspotsText, "head", IRBrightSpotPart::Head, profile.headBrightSpot);
	applyBrightSpotConfig(brightspotsText, "mid", IRBrightSpotPart::MidBody, profile.midBrightSpot);
}
}

IRStage4Vec3::IRStage4Vec3()
	: x(0.0f), y(0.0f), z(0.0f)
{
}

IRStage4Vec3::IRStage4Vec3(float xValue, float yValue, float zValue)
	: x(xValue), y(yValue), z(zValue)
{
}

IRHotspotState::IRHotspotState()
	: enabled(false),
	kind(IRHotspotKind::EngineRear),
	shape(IRHotspotShape::Sphere),
	localPos(0.0f, 0.0f, 0.0f),
	localDir(0.0f, -1.0f, 0.0f),
	size(0.5f, 0.5f, 0.5f),
	targetTempK(650.0f),
	currentTempK(300.0f),
	ambientTempK(300.0f),
	heatTauSec(1.0f),
	coolTauSec(6.0f),
	intensity(1.0f)
{
}

IRBrightSpotState::IRBrightSpotState()
	: enabled(false),
	part(IRBrightSpotPart::None),
	localPos(0.0f, 0.0f, 0.0f),
	radius(0.2f),
	intensity(1.0f)
{
}

IRTemperatureModel::PlatformProfile::PlatformProfile()
	: engineRearEnabledByEngineState(true)
{
	engineRear.enabled = true;
	headBrightSpot.part = IRBrightSpotPart::Head;
	headBrightSpot.localPos = IRStage4Vec3(-0.3f, 0.8f, 0.0f);
	headBrightSpot.radius = 0.2f;
	headBrightSpot.intensity = 1.0f;
	midBrightSpot.part = IRBrightSpotPart::MidBody;
	midBrightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
	midBrightSpot.radius = 0.25f;
	midBrightSpot.intensity = 0.8f;
}

const char* IRHotspotKindName(IRHotspotKind kind)
{
	switch (kind)
	{
	case IRHotspotKind::EngineRear: return "EngineRear";
	case IRHotspotKind::EnginePlume: return "EnginePlume";
	case IRHotspotKind::CustomThermal: return "CustomThermal";
	default: return "Unknown";
	}
}

const char* IRHotspotShapeName(IRHotspotShape shape)
{
	switch (shape)
	{
	case IRHotspotShape::Sphere: return "Sphere";
	case IRHotspotShape::Ellipsoid: return "Ellipsoid";
	case IRHotspotShape::Cone: return "Cone";
	case IRHotspotShape::Capsule: return "Capsule";
	default: return "Unknown";
	}
}

const char* IRBrightSpotPartName(IRBrightSpotPart part)
{
	switch (part)
	{
	case IRBrightSpotPart::None: return "None";
	case IRBrightSpotPart::Head: return "Head";
	case IRBrightSpotPart::MidBody: return "MidBody";
	default: return "Unknown";
	}
}

IRTemperatureModel::IRTemperatureModel()
	: m_defaultProfile(defaultProfileForPlatform("default")),
	m_loaded(false)
{
}

bool IRTemperatureModel::loadFromFileCandidates(const std::vector<std::string>& filePaths)
{
	for (size_t i = 0; i < filePaths.size(); ++i)
	{
		if (load(filePaths[i]))
		{
			return true;
		}
	}

	warnOnce("missing-config", "[Stage4] warning: IRHotspots/target_hotspots.json missing; use safe fallback hotspot/brightspot profile.");
	m_loaded = false;
	m_loadedPath.clear();
	return false;
}

bool IRTemperatureModel::load(const std::string& filePath)
{
	std::string text;
	if (!readWholeFile(filePath, text))
	{
		return false;
	}

	std::string platformsText;
	if (!findJsonObject(text, "platforms", platformsText))
	{
		warnOnce("bad-config-" + filePath, "[Stage4] warning: target_hotspots.json missing platforms object; use safe fallback profile.");
		return false;
	}

	m_profiles.clear();
	const char* requiredPlatforms[] = { "F35", "AIM120", "AIM120D", "AIM9X" };
	for (size_t i = 0; i < sizeof(requiredPlatforms) / sizeof(requiredPlatforms[0]); ++i)
	{
		const std::string platformName = requiredPlatforms[i];
		PlatformProfile profile = defaultProfileForPlatform(platformName);
		std::string platformText;
		if (findJsonObject(platformsText, platformName, platformText))
		{
			applyHotspotConfig(platformText, profile);
			applyBrightSpotsConfig(platformText, profile);
		}
		else
		{
			warnOnce("missing-platform-" + platformName, "[Stage4] warning: target_hotspots.json missing " + platformName + " profile; use safe fallback values.");
		}
		m_profiles[platformName] = profile;
	}

	std::string f22Text;
	if (findJsonObject(platformsText, "F22", f22Text))
	{
		PlatformProfile profile = defaultProfileForPlatform("F22");
		applyHotspotConfig(f22Text, profile);
		applyBrightSpotsConfig(f22Text, profile);
		m_profiles["F22"] = profile;
	}

	m_loadedPath = filePath;
	m_loaded = true;
	return true;
}

bool IRTemperatureModel::loaded() const
{
	return m_loaded;
}

const std::string& IRTemperatureModel::loadedPath() const
{
	return m_loadedPath;
}

IRHotspotState IRTemperatureModel::updateEngineRear(
	const std::string& platformName,
	const std::string& runtimeKey,
	bool engineState,
	float dtSec,
	float ambientTempK)
{
	const PlatformProfile& profile = profileForPlatform(platformName);
	IRHotspotState state = profile.engineRear;
	state.ambientTempK = ambientTempK;

	std::map<std::string, IRHotspotState>::const_iterator runtimeIt = m_runtimeEngineRear.find(runtimeKey);
	if (runtimeIt != m_runtimeEngineRear.end())
	{
		state.currentTempK = runtimeIt->second.currentTempK;
	}
	else
	{
		state.currentTempK = ambientTempK;
	}

	const float safeDt = std::max(0.0f, std::min(dtSec, 2.0f));
	const float desiredTempK = engineState ? state.targetTempK : state.ambientTempK;
	const float tauSec = clampPositive(engineState ? state.heatTauSec : state.coolTauSec, 1.0f);
	const float alpha = 1.0f - std::exp(-safeDt / tauSec);

	// Stage4 ThermalHotspot: engine changes only the physical hotspot inertia; off state cools toward ambient.
	state.currentTempK += (desiredTempK - state.currentTempK) * alpha;
	state.enabled = profile.engineRear.enabled && profile.engineRearEnabledByEngineState && engineState;

	m_runtimeEngineRear[runtimeKey] = state;
	return state;
}

IRBrightSpotState IRTemperatureModel::resolveBrightSpot(
	const std::string& platformName,
	bool strikeFlag,
	int strikePart,
	bool* invalidStrikePart) const
{
	if (invalidStrikePart != nullptr)
	{
		*invalidStrikePart = false;
	}

	IRBrightSpotState disabled;
	if (!strikeFlag)
	{
		return disabled;
	}

	const PlatformProfile& profile = profileForPlatform(platformName);
	if (strikePart == 1)
	{
		IRBrightSpotState state = profile.headBrightSpot;
		state.enabled = true;
		state.part = IRBrightSpotPart::Head;
		return state;
	}
	if (strikePart == 2)
	{
		IRBrightSpotState state = profile.midBrightSpot;
		state.enabled = true;
		state.part = IRBrightSpotPart::MidBody;
		return state;
	}

	if (invalidStrikePart != nullptr)
	{
		*invalidStrikePart = true;
	}
	warnOnce("invalid-strike-part-" + std::to_string(strikePart), "[Stage4] warning: strikePart supports only 1=head and 2=mid; BrightSpot disabled for this frame.");
	return disabled;
}

void IRTemperatureModel::resetRuntime()
{
	m_runtimeEngineRear.clear();
}

IRTemperatureModel::PlatformProfile IRTemperatureModel::defaultProfileForPlatform(const std::string& platformName) const
{
	PlatformProfile profile;
	profile.engineRear.kind = IRHotspotKind::EngineRear;
	profile.engineRear.shape = IRHotspotShape::Sphere;

	const std::string name = normalizePlatformName(platformName);
	if (name == "F35")
	{
		profile.engineRear.localPos = IRStage4Vec3(0.0f, -3.8f, 0.0f);
		profile.engineRear.size = IRStage4Vec3(0.85f, 0.85f, 0.85f);
		profile.engineRear.targetTempK = 900.0f;
		profile.engineRear.heatTauSec = 1.2f;
		profile.engineRear.coolTauSec = 8.0f;
		profile.engineRear.intensity = 1.15f;
		profile.headBrightSpot.localPos = IRStage4Vec3(0.0f, 2.8f, 0.0f);
		profile.midBrightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
	}
	else if (name == "AIM120" || name == "AIM120D")
	{
		profile.engineRear.localPos = IRStage4Vec3(0.0f, -1.4f, 0.0f);
		profile.engineRear.size = IRStage4Vec3(0.35f, 0.35f, 0.35f);
		profile.engineRear.targetTempK = 760.0f;
		profile.engineRear.heatTauSec = 0.8f;
		profile.engineRear.coolTauSec = 4.5f;
		profile.engineRear.intensity = 0.9f;
		profile.headBrightSpot.localPos = IRStage4Vec3(0.0f, 1.1f, 0.0f);
		profile.midBrightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		profile.headBrightSpot.radius = 0.18f;
		profile.midBrightSpot.radius = 0.22f;
	}
	else if (name == "AIM9X" || name == "AIM9")
	{
		profile.engineRear.localPos = IRStage4Vec3(0.0f, -1.1f, 0.0f);
		profile.engineRear.size = IRStage4Vec3(0.30f, 0.30f, 0.30f);
		profile.engineRear.targetTempK = 720.0f;
		profile.engineRear.heatTauSec = 0.7f;
		profile.engineRear.coolTauSec = 4.0f;
		profile.engineRear.intensity = 0.85f;
		profile.headBrightSpot.localPos = IRStage4Vec3(0.0f, 0.9f, 0.0f);
		profile.midBrightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		profile.headBrightSpot.radius = 0.16f;
		profile.midBrightSpot.radius = 0.20f;
	}

	profile.engineRear.currentTempK = profile.engineRear.ambientTempK;
	return profile;
}

const IRTemperatureModel::PlatformProfile& IRTemperatureModel::profileForPlatform(const std::string& platformName) const
{
	const std::string normalized = normalizePlatformName(platformName);
	std::map<std::string, PlatformProfile>::const_iterator it = m_profiles.find(normalized);
	if (it != m_profiles.end())
	{
		return it->second;
	}

	if (normalized == "AIM120D")
	{
		it = m_profiles.find("AIM120");
		if (it != m_profiles.end())
		{
			return it->second;
		}
	}
	if (normalized == "AIM9")
	{
		it = m_profiles.find("AIM9X");
		if (it != m_profiles.end())
		{
			return it->second;
		}
	}

	warnOnce("fallback-platform-" + normalized, "[Stage4] warning: platform " + normalized + " has no Hotspot/BrightSpot profile; use safe fallback values.");
	return m_defaultProfile;
}

std::string IRTemperatureModel::normalizePlatformName(const std::string& platformName) const
{
	if (platformName == "AIM120D")
	{
		return "AIM120D";
	}
	if (platformName == "AIM9")
	{
		return "AIM9X";
	}
	return platformName.empty() ? "default" : platformName;
}

void IRTemperatureModel::warnOnce(const std::string& key, const std::string& message) const
{
	if (m_warningKeys.insert(key).second)
	{
		std::cerr << message << std::endl;
	}
}
