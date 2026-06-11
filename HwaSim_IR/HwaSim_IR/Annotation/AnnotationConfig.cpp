#include "AnnotationConfig.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
bool ReadWholeFile(const std::string& filePath, std::string& text)
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

std::string MakeJsonToken(const std::string& key)
{
	return "\"" + key + "\"";
}

bool MatchJsonObject(const std::string& text, size_t openBrace, std::string& objectText)
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
		const char c = text[i];
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

bool FindJsonObject(const std::string& text, const std::string& key, std::string& objectText)
{
	const std::string token = MakeJsonToken(key);
	size_t pos = text.find(token);
	while (pos != std::string::npos)
	{
		size_t colon = text.find(':', pos + token.size());
		if (colon == std::string::npos)
		{
			return false;
		}
		size_t brace = text.find('{', colon + 1);
		if (brace != std::string::npos && MatchJsonObject(text, brace, objectText))
		{
			return true;
		}
		pos = text.find(token, pos + token.size());
	}
	return false;
}

bool ExtractJsonNumber(const std::string& text, const std::string& key, double& value)
{
	const std::string token = MakeJsonToken(key);
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
		const char c = text[end];
		if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
		{
			break;
		}
		++end;
	}
	char* parsedEnd = nullptr;
	const std::string numberText = text.substr(begin, end - begin);
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
	const std::string token = MakeJsonToken(key);
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
	if (text.compare(begin, 4, "true") == 0 || text.compare(begin, 1, "1") == 0)
	{
		value = true;
		return true;
	}
	if (text.compare(begin, 5, "false") == 0 || text.compare(begin, 1, "0") == 0)
	{
		value = false;
		return true;
	}
	return false;
}

bool ExtractJsonString(const std::string& text, const std::string& key, std::string& value)
{
	const std::string token = MakeJsonToken(key);
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

bool ExtractJsonVec3(const std::string& text, const std::string& key, LPoint3f& value)
{
	const std::string token = MakeJsonToken(key);
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

	std::stringstream stream(text.substr(begin + 1, end - begin - 1));
	std::string item;
	float numbers[3] = { value.get_x(), value.get_y(), value.get_z() };
	int count = 0;
	while (std::getline(stream, item, ',') && count < 3)
	{
		char* parsedEnd = nullptr;
		const double parsed = std::strtod(item.c_str(), &parsedEnd);
		if (parsedEnd == item.c_str())
		{
			return false;
		}
		numbers[count] = static_cast<float>(parsed);
		++count;
	}
	if (count != 3)
	{
		return false;
	}
	value = LPoint3f(numbers[0], numbers[1], numbers[2]);
	return true;
}

bool ExtractJsonStringArray(const std::string& text, const std::string& key, std::vector<std::string>& values)
{
	const std::string token = MakeJsonToken(key);
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

	std::vector<std::string> parsedValues;
	size_t cursor = begin + 1;
	while (cursor < end)
	{
		size_t quoteBegin = text.find('"', cursor);
		if (quoteBegin == std::string::npos || quoteBegin >= end)
		{
			break;
		}
		size_t quoteEnd = text.find('"', quoteBegin + 1);
		if (quoteEnd == std::string::npos || quoteEnd > end)
		{
			return false;
		}
		parsedValues.push_back(text.substr(quoteBegin + 1, quoteEnd - quoteBegin - 1));
		cursor = quoteEnd + 1;
	}
	values = parsedValues;
	return true;
}

AnnotationForwardAxis ParseForwardAxis(const std::string& text, AnnotationForwardAxis fallback)
{
	if (text == "PositiveX") return AnnotationAxisPositiveX;
	if (text == "NegativeX") return AnnotationAxisNegativeX;
	if (text == "PositiveY") return AnnotationAxisPositiveY;
	if (text == "NegativeY") return AnnotationAxisNegativeY;
	if (text == "PositiveZ") return AnnotationAxisPositiveZ;
	if (text == "NegativeZ") return AnnotationAxisNegativeZ;
	return fallback;
}

void ApplyBBoxConfig(const std::string& objectText, AnnotationBBoxConfig& bbox)
{
	ExtractJsonString(objectText, "mode", bbox.mode);
	double margin = static_cast<double>(bbox.marginPx);
	if (ExtractJsonNumber(objectText, "marginPx", margin))
	{
		bbox.marginPx = std::max(0, static_cast<int>(margin));
	}
	double minSize = static_cast<double>(bbox.minSizePx);
	if (ExtractJsonNumber(objectText, "minSizePx", minSize))
	{
		bbox.minSizePx = std::max(1, static_cast<int>(minSize));
	}
	ExtractJsonStringArray(objectText, "excludeNodeNameContains", bbox.excludeNodeNameContains);
}

void ApplyKeyPointConfig(const std::string& objectText, AnnotationKeyPointConfig& point)
{
	ExtractJsonBool(objectText, "visible", point.visible);
	ExtractJsonBool(objectText, "surface", point.surface);
	ExtractJsonBool(objectText, "snapToMeshSurface", point.snapToMeshSurface);
	ExtractJsonString(objectText, "surfaceSearchMode", point.surfaceSearchMode);
	if (ExtractJsonVec3(objectText, "localPos", point.localPos))
	{
		point.hasLocalPos = true;
	}
}

void ApplyTargetConfig(const std::string& objectText, TargetAnnotationConfig& config)
{
	ExtractJsonBool(objectText, "enabled", config.enabled);
	ExtractJsonString(objectText, "label", config.label);

	std::string bboxText;
	if (FindJsonObject(objectText, "bbox", bboxText))
	{
		ApplyBBoxConfig(bboxText, config.bbox);
	}

	std::string keypointsText;
	if (FindJsonObject(objectText, "keypoints", keypointsText))
	{
		ExtractJsonBool(keypointsText, "enabled", config.keyPointsEnabled);
		std::string axisText;
		if (ExtractJsonString(keypointsText, "forwardAxis", axisText))
		{
			config.forwardAxis = ParseForwardAxis(axisText, config.forwardAxis);
		}

		std::string pointsText;
		if (FindJsonObject(keypointsText, "points", pointsText))
		{
			std::string headText;
			if (FindJsonObject(pointsText, "head", headText))
			{
				ApplyKeyPointConfig(headText, config.headPoint);
			}
			std::string middleText;
			if (FindJsonObject(pointsText, "middle", middleText))
			{
				ApplyKeyPointConfig(middleText, config.middlePoint);
			}
		}
	}
}

void SetExplicitPoint(AnnotationKeyPointConfig& point, const LPoint3f& localPos)
{
	point.localPos = localPos;
	point.hasLocalPos = true;
	point.visible = true;
	point.surface = true;
	point.snapToMeshSurface = true;
	point.surfaceSearchMode = "nearest_mesh_vertex";
}

TargetAnnotationConfig MakeFallbackConfig(const std::string& label, const LPoint3f& head, const LPoint3f& middle)
{
	TargetAnnotationConfig config;
	config.label = label;
	config.bbox.mode = "mesh_body";
	config.bbox.marginPx = 3;
	config.bbox.minSizePx = 4;
	config.bbox.excludeNodeNameContains.push_back("EnginePlume");
	config.bbox.excludeNodeNameContains.push_back("Plume");
	config.bbox.excludeNodeNameContains.push_back("Annotation");
	config.bbox.excludeNodeNameContains.push_back("Hotspot");
	config.forwardAxis = AnnotationAxisPositiveY;
	SetExplicitPoint(config.headPoint, head);
	SetExplicitPoint(config.middlePoint, middle);
	return config;
}

PLATFORM_TYPE TargetTypeToPlatform(int targetType)
{
	switch (targetType)
	{
	case 0x11: return F35;
	case 0x22: return AIM120;
	case 0x33: return AIM9;
	case 0x44: return MMD;
	default: return NONE;
	}
}
}

AnnotationConfig::AnnotationConfig()
{
	resetToFallback();
}

void AnnotationConfig::resetToFallback()
{
	m_configs.clear();
	m_loaded = false;
	m_loadedPath.clear();
	m_drawOptions = AnnotationDrawOptions();

	m_defaultConfig = MakeFallbackConfig("UNKNOWN", LPoint3f(0.0f, 1.0f, 0.0f), LPoint3f(0.0f, 0.0f, 0.1f));
	m_configs[F35] = MakeFallbackConfig("F35", LPoint3f(0.0f, 2.8f, 0.2f), LPoint3f(0.0f, 0.0f, 0.5f));
	m_configs[AIM120] = MakeFallbackConfig("AIM120D", LPoint3f(0.0f, 1.1f, 0.0f), LPoint3f(0.0f, 0.0f, 0.08f));
	m_configs[AIM9] = MakeFallbackConfig("AIM9X", LPoint3f(0.0f, 0.9f, 0.0f), LPoint3f(0.0f, 0.0f, 0.08f));
	m_configs[MMD] = MakeFallbackConfig("MMD", LPoint3f(0.0f, 1.0f, 0.0f), LPoint3f(0.0f, 0.0f, 0.1f));
}

bool AnnotationConfig::loadFromCandidates(const std::vector<std::string>& filePaths, const std::string& configuredPath, const std::string& source)
{
	resetToFallback();
	m_configuredPath = configuredPath.empty() ? "Config/Annotation/annotation_profiles.json" : configuredPath;

	std::string text;
	std::string selectedPath;
	for (size_t i = 0; i < filePaths.size(); ++i)
	{
		if (ReadWholeFile(filePaths[i], text))
		{
			selectedPath = filePaths[i];
			break;
		}
	}

	if (selectedPath.empty())
	{
		std::cout << "[AnnotationConfig]"
			<< " profilePath=" << m_configuredPath
			<< " loaded=0"
			<< " source=" << source
			<< " fallback=code"
			<< std::endl;
		return false;
	}

	std::string defaultsText;
	if (FindJsonObject(text, "defaults", defaultsText))
	{
		ApplyTargetConfig(defaultsText, m_defaultConfig);
	}

	std::string platformsText;
	if (FindJsonObject(text, "platforms", platformsText))
	{
		const char* platformNames[] = { "F35", "AIM120D", "AIM120", "AIM9X", "MMD" };
		for (size_t i = 0; i < sizeof(platformNames) / sizeof(platformNames[0]); ++i)
		{
			const std::string name = platformNames[i];
			std::string platformText;
			if (!FindJsonObject(platformsText, name, platformText))
			{
				continue;
			}

			// 平台配置先继承 defaults，再应用平台自己的 label/localPos 覆盖。
			TargetAnnotationConfig config = m_defaultConfig;

			ApplyTargetConfig(platformText, config);
			if (name == "F35")
			{
				m_configs[F35] = config;
			}
			else if (name == "AIM120D" || name == "AIM120")
			{
				m_configs[AIM120] = config;
			}
			else if (name == "AIM9X")
			{
				m_configs[AIM9] = config;
			}
			else if (name == "MMD")
			{
				m_configs[MMD] = config;
			}
		}
	}

	m_loaded = true;
	m_loadedPath = selectedPath;
	std::cout << "[AnnotationConfig]"
		<< " profilePath=" << m_configuredPath
		<< " loaded=1"
		<< " source=" << source
		<< " resolvedPath=" << m_loadedPath
		<< std::endl;
	return true;
}

void AnnotationConfig::applyRuntimeOptions(const AnnotationRuntimeOptions& options)
{
	m_drawOptions = options.drawOptions;
	m_occlusion = options.occlusion;
	if (m_occlusion.mode.empty())
	{
		m_occlusion.mode = "mesh_collision";
	}
	m_occlusion.epsilonM = std::max(0.0f, m_occlusion.epsilonM);
	m_occlusion.collisionMaskBit = std::max(0, std::min(31, m_occlusion.collisionMaskBit));
	m_surfaceKeyPointEnabled = options.surfaceKeyPointEnabled;
	m_surfaceSnapEnabled = options.surfaceSnapEnabled;
	m_surfaceSnapMode = options.surfaceSnapMode.empty() ? "profile_surface" : options.surfaceSnapMode;
	const int marginPx = std::max(0, options.bboxMarginPx);
	const int minSizePx = std::max(1, options.minBBoxSizePx);
	const std::string mode = options.bboxMode.empty() ? "mesh_body" : options.bboxMode;

	m_defaultConfig.bbox.mode = mode;
	m_defaultConfig.bbox.marginPx = marginPx;
	m_defaultConfig.bbox.minSizePx = minSizePx;
	for (std::map<PLATFORM_TYPE, TargetAnnotationConfig>::iterator it = m_configs.begin(); it != m_configs.end(); ++it)
	{
		it->second.bbox.mode = mode;
		it->second.bbox.marginPx = marginPx;
		it->second.bbox.minSizePx = minSizePx;
	}
}

const TargetAnnotationConfig& AnnotationConfig::configForPlatform(PLATFORM_TYPE type) const
{
	std::map<PLATFORM_TYPE, TargetAnnotationConfig>::const_iterator iter = m_configs.find(type);
	if (iter != m_configs.end())
	{
		return iter->second;
	}
	return m_defaultConfig;
}

std::string AnnotationConfig::labelForTargetType(int targetType) const
{
	const PLATFORM_TYPE type = TargetTypeToPlatform(targetType);
	return configForPlatform(type).label;
}

const AnnotationDrawOptions& AnnotationConfig::drawOptions() const
{
	return m_drawOptions;
}

const AnnotationOcclusionConfig& AnnotationConfig::occlusion() const
{
	return m_occlusion;
}

bool AnnotationConfig::surfaceKeyPointEnabled() const
{
	return m_surfaceKeyPointEnabled;
}

bool AnnotationConfig::surfaceSnapEnabled() const
{
	return m_surfaceSnapEnabled;
}

const std::string& AnnotationConfig::surfaceSnapMode() const
{
	return m_surfaceSnapMode;
}

bool AnnotationConfig::loaded() const
{
	return m_loaded;
}

const std::string& AnnotationConfig::loadedPath() const
{
	return m_loadedPath;
}

const std::string& AnnotationConfig::configuredPath() const
{
	return m_configuredPath;
}
