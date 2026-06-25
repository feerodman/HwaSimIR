// HwaSim_IR.cpp : 定义控制台应用程序的入口点。
//
//#include "stdafx.h"

#include "HwaSimIR.h"
#include "lvecBase4.h"
#include "pta_LVecBase4.h"
#include "pta_float.h"
#include "graphicsEngine.h"
#include "graphicsPipe.h"
#include "graphicsStateGuardian.h"
#include "geom.h"
#include "geomNode.h"
#include "geomTriangles.h"
#include "geomVertexData.h"
#include "geomVertexFormat.h"
#include "geomVertexWriter.h"
#include "internalName.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
bool FileExists(const std::string& path)
{
	std::ifstream file(path.c_str());
	return file.good();
}

std::vector<std::string> BuildRuntimeConfigCandidatePaths()
{
	std::vector<std::string> runtimeConfigPaths;
	runtimeConfigPaths.push_back("Config/HwaSimIRRuntime.ini");
	runtimeConfigPaths.push_back("../Bin/Config/HwaSimIRRuntime.ini");
	runtimeConfigPaths.push_back("HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini");
	runtimeConfigPaths.push_back("../HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini");
	return runtimeConfigPaths;
}

double ClampStage5Double(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

LVecBase3f Stage5SunDirectionLocal(double sunAzimuthDeg, double sunElevationDeg)
{
	const double pi = 3.14159265358979323846;
	const double az = sunAzimuthDeg * pi / 180.0;
	const double el = ClampStage5Double(sunElevationDeg, -90.0, 90.0) * pi / 180.0;
	const double cosEl = std::cos(el);
	return LVecBase3f(
		static_cast<float>(std::sin(az) * cosEl),
		static_cast<float>(std::cos(az) * cosEl),
		static_cast<float>(std::max(0.0, std::sin(el))));
}

bool ReadWholeFile(const std::string& path, std::string& text)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
	{
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();
	return true;
}

bool IsReasonableAltitudeMeters(double altitudeMeters)
{
	return std::isfinite(altitudeMeters) && altitudeMeters > -500.0 && altitudeMeters < 100000.0;
}

double NormalizeAeroAltitudeMeters(double altitudeMeters)
{
	if (!IsReasonableAltitudeMeters(altitudeMeters))
	{
		return 0.0;
	}
	return std::abs(altitudeMeters) < 1.0e-6 ? 0.0 : altitudeMeters;
}

std::string Stage5AeroSpeedSourceForTarget(const BYHWICD::TargetState& targetState)
{
	if (!std::isfinite(targetState.targetLoc.speed))
	{
		return "invalid_udp";
	}
	if (targetState.targetLoc.speed > 0.0)
	{
		return "udp_targetLoc_speed";
	}
	return "zero_udp";
}

std::string TargetTypeHexString(int targetType)
{
	std::ostringstream out;
	out << "0x" << std::hex << std::uppercase << targetType;
	return out.str();
}

double QuantizeForCache(double value, double step)
{
	if (!std::isfinite(value) || step <= 0.0)
	{
		return value;
	}
	return std::floor(value / step + 0.5) * step;
}

std::string Stage5ModtranCacheDouble(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

PT(PandaNode) CreateStage7SkyDomeNode()
{
	const int slices = 48;
	const int stacks = 12;
	const double pi = 3.14159265358979323846;
	PT(GeomVertexData) data = new GeomVertexData(
		"Stage7SkyDomeData",
		GeomVertexFormat::get_v3n3t2(),
		Geom::UH_static);
	GeomVertexWriter vertex(data, "vertex");
	GeomVertexWriter normal(data, "normal");
	GeomVertexWriter texcoord(data, "texcoord");

	for (int stack = 0; stack <= stacks; ++stack)
	{
		const double phi = (pi * 0.5) * static_cast<double>(stack) / static_cast<double>(stacks);
		const double z = std::cos(phi);
		const double r = std::sin(phi);
		for (int slice = 0; slice <= slices; ++slice)
		{
			const double theta = 2.0 * pi * static_cast<double>(slice) / static_cast<double>(slices);
			const float x = static_cast<float>(std::cos(theta) * r);
			const float y = static_cast<float>(std::sin(theta) * r);
			const float zz = static_cast<float>(z);
			vertex.add_data3f(x, y, zz);
			normal.add_data3f(-x, -y, -zz);
			texcoord.add_data2f(
				static_cast<float>(slice) / static_cast<float>(slices),
				static_cast<float>(stack) / static_cast<float>(stacks));
		}
	}

	PT(GeomTriangles) triangles = new GeomTriangles(Geom::UH_static);
	const int rowSize = slices + 1;
	for (int stack = 0; stack < stacks; ++stack)
	{
		for (int slice = 0; slice < slices; ++slice)
		{
			const int a = stack * rowSize + slice;
			const int b = a + 1;
			const int c = (stack + 1) * rowSize + slice;
			const int d = c + 1;
			triangles->add_vertices(a, c, b);
			triangles->add_vertices(b, c, d);
		}
	}
	triangles->close_primitive();

	PT(Geom) geom = new Geom(data);
	geom->add_primitive(triangles);
	PT(GeomNode) node = new GeomNode("Stage7_SkyDome");
	node->add_geom(geom);
	return node;
}

PT(PandaNode) CreateStage7LowerHemisphereShellNode()
{
	const int slices = 48;
	const int stacks = 12;
	const double pi = 3.14159265358979323846;
	PT(GeomVertexData) data = new GeomVertexData(
		"Stage7LowerHemisphereShellData",
		GeomVertexFormat::get_v3n3t2(),
		Geom::UH_static);
	GeomVertexWriter vertex(data, "vertex");
	GeomVertexWriter normal(data, "normal");
	GeomVertexWriter texcoord(data, "texcoord");

	for (int stack = 0; stack <= stacks; ++stack)
	{
		const double phi = (pi * 0.5) + (pi * 0.5) * static_cast<double>(stack) / static_cast<double>(stacks);
		const double z = std::cos(phi);
		const double r = std::sin(phi);
		for (int slice = 0; slice <= slices; ++slice)
		{
			const double theta = 2.0 * pi * static_cast<double>(slice) / static_cast<double>(slices);
			const float x = static_cast<float>(std::cos(theta) * r);
			const float y = static_cast<float>(std::sin(theta) * r);
			const float zz = static_cast<float>(z);
			vertex.add_data3f(x, y, zz);
			normal.add_data3f(-x, -y, -zz);
			texcoord.add_data2f(
				static_cast<float>(slice) / static_cast<float>(slices),
				static_cast<float>(stack) / static_cast<float>(stacks));
		}
	}

	PT(GeomTriangles) triangles = new GeomTriangles(Geom::UH_static);
	const int rowSize = slices + 1;
	for (int stack = 0; stack < stacks; ++stack)
	{
		for (int slice = 0; slice < slices; ++slice)
		{
			const int a = stack * rowSize + slice;
			const int b = a + 1;
			const int c = (stack + 1) * rowSize + slice;
			const int d = c + 1;
			triangles->add_vertices(a, c, b);
			triangles->add_vertices(b, c, d);
		}
	}
	triangles->close_primitive();

	PT(Geom) geom = new Geom(data);
	geom->add_primitive(triangles);
	PT(GeomNode) node = new GeomNode("Stage7_LowerHemisphereShell");
	node->add_geom(geom);
	return node;
}

PT(PandaNode) CreateStage5EnginePlumeBillboardNode()
{
	PT(GeomVertexData) data = new GeomVertexData(
		"Stage5EnginePlumeData",
		GeomVertexFormat::get_v3n3t2(),
		Geom::UH_static);
	GeomVertexWriter vertex(data, "vertex");
	GeomVertexWriter normal(data, "normal");
	GeomVertexWriter texcoord(data, "texcoord");

	const LVecBase3f verts[8] = {
		LVecBase3f(-1.0f, 0.0f, 0.0f),
		LVecBase3f( 1.0f, 0.0f, 0.0f),
		LVecBase3f( 1.0f,-1.0f, 0.0f),
		LVecBase3f(-1.0f,-1.0f, 0.0f),
		LVecBase3f( 0.0f, 0.0f,-1.0f),
		LVecBase3f( 0.0f, 0.0f, 1.0f),
		LVecBase3f( 0.0f,-1.0f, 1.0f),
		LVecBase3f( 0.0f,-1.0f,-1.0f)
	};
	const LVecBase3f norms[8] = {
		LVecBase3f(0.0f, 0.0f, 1.0f),
		LVecBase3f(0.0f, 0.0f, 1.0f),
		LVecBase3f(0.0f, 0.0f, 1.0f),
		LVecBase3f(0.0f, 0.0f, 1.0f),
		LVecBase3f(1.0f, 0.0f, 0.0f),
		LVecBase3f(1.0f, 0.0f, 0.0f),
		LVecBase3f(1.0f, 0.0f, 0.0f),
		LVecBase3f(1.0f, 0.0f, 0.0f)
	};
	const LVecBase2f uvs[8] = {
		LVecBase2f(0.0f, 0.0f),
		LVecBase2f(1.0f, 0.0f),
		LVecBase2f(1.0f, 1.0f),
		LVecBase2f(0.0f, 1.0f),
		LVecBase2f(0.0f, 0.0f),
		LVecBase2f(1.0f, 0.0f),
		LVecBase2f(1.0f, 1.0f),
		LVecBase2f(0.0f, 1.0f)
	};
	for (int i = 0; i < 8; ++i)
	{
		vertex.add_data3f(verts[i]);
		normal.add_data3f(norms[i]);
		texcoord.add_data2f(uvs[i]);
	}

	PT(GeomTriangles) triangles = new GeomTriangles(Geom::UH_static);
	triangles->add_vertices(0, 2, 1);
	triangles->add_vertices(0, 3, 2);
	triangles->add_vertices(4, 6, 5);
	triangles->add_vertices(4, 7, 6);
	triangles->close_primitive();

	PT(Geom) geom = new Geom(data);
	geom->add_primitive(triangles);
	PT(GeomNode) node = new GeomNode("Stage5_EnginePlume");
	node->add_geom(geom);
	return node;
}

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return value;
}

int ParseStage5DebugViewMode(const std::string& value)
{
	std::string lower = ToLowerAscii(value);
	if (lower == "off" || lower == "none" || lower == "disabled" || lower == "0")
	{
		return 0;
	}
	if (lower == "bodyonly" || lower == "body")
	{
		return 1;
	}
	if (lower == "reflected" || lower == "reflection" || lower == "reflectedonly")
	{
		return 2;
	}
	if (lower == "rearhotspot" || lower == "rear" || lower == "hotspotonly" || lower == "hotspot")
	{
		return 3;
	}
	if (lower == "plume" || lower == "engineplume")
	{
		return 4;
	}
	if (lower == "brightspotonly" || lower == "brightspot")
	{
		return 5;
	}
	if (lower == "atmosphere" || lower == "path" || lower == "pathradiance")
	{
		return 6;
	}
	if (lower == "sensorinput" || lower == "sensor" || lower == "composite")
	{
		return 7;
	}
	return 0;
}

std::string NormalizeStage5SensorInputDisplayMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "legacymatch" || lower == "matchlegacy" || lower == "calib" || lower == "calibration")
	{
		return "LegacyMatch";
	}
	return "Manual";
}

std::string NormalizeStage6MtfBlurMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "gaussianseparable" || lower == "gaussian" || lower == "separable")
	{
		return "GaussianSeparable";
	}
	return "GaussianSeparable";
}

std::string NormalizeStage6AgcMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "off" || lower == "none" || lower == "disabled" || lower == "0")
	{
		return "Off";
	}
	if (lower == "meanstd" || lower == "mean_std" || lower == "mean-std")
	{
		return "MeanStd";
	}
	if (lower == "manual")
	{
		return "Manual";
	}
	return "Percentile";
}

int Stage6AgcModeCode(const std::string& value)
{
	if (value == "Percentile")
	{
		return 1;
	}
	if (value == "MeanStd")
	{
		return 2;
	}
	if (value == "Manual")
	{
		return 3;
	}
	return 0;
}

std::string NormalizeStage6NoisePosition(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "afteragc" || lower == "after_agc" || lower == "after-agc")
	{
		return "AfterAGC";
	}
	return "BeforeAGC";
}

int Stage6NoisePositionCode(const std::string& value)
{
	return value == "AfterAGC" ? 2 : 1;
}

std::string NormalizeStage5ModtranPathRuntimeMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "compareonly" || lower == "compare" || lower == "logonly" || lower == "log")
	{
		return "CompareOnly";
	}
	if (lower == "replacelegacy" || lower == "replace" || lower == "runtime")
	{
		return "ReplaceLegacy";
	}
	if (lower == "blendlegacy" || lower == "blend")
	{
		return "BlendLegacy";
	}
	return "Off";
}

IRBand ParseStage5ModtranPathRuntimeBand(const std::string& value, std::string& normalizedName)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "mwir" || lower == "midwave" || lower == "midwaveinfrared" || lower == "2" || lower == "3")
	{
		normalizedName = "MWIR";
		return IRBand::MidWaveInfrared;
	}
	if (lower == "vis" || lower == "visible" || lower == "4" || lower == "0")
	{
		normalizedName = "VIS";
		return IRBand::Visible;
	}
	if (lower == "nir" || lower == "nearinfrared" || lower == "1")
	{
		normalizedName = "NIR";
		return IRBand::NearInfrared;
	}
	if (lower == "swir" || lower == "shortwave" || lower == "shortwaveinfrared")
	{
		normalizedName = "SWIR";
		return IRBand::ShortWaveInfrared;
	}
	if (lower == "lwir" || lower == "longwave" || lower == "longwaveinfrared")
	{
		normalizedName = "LWIR";
		return IRBand::LongWaveInfrared;
	}
	normalizedName = "MWIR";
	return IRBand::MidWaveInfrared;
}

const char* Stage5DebugViewModeName(int viewMode)
{
	switch (viewMode)
	{
	case 1: return "Body";
	case 2: return "Reflected";
	case 3: return "RearHotspot";
	case 4: return "Plume";
	case 5: return "BrightSpot";
	case 6: return "Atmosphere";
	case 7: return "SensorInput";
	default:
		return "Off";
	}
}

int ParseStage7DebugMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "skyonly" || lower == "sky")
	{
		return 1;
	}
	if (lower == "groundonly" || lower == "ground")
	{
		return 2;
	}
	if (lower == "skygroundcolor" || lower == "color")
	{
		return 3;
	}
	return 0;
}

const char* Stage7DebugModeName(int debugMode)
{
	switch (debugMode)
	{
	case 1: return "SkyOnly";
	case 2: return "GroundOnly";
	case 3: return "SkyGroundColor";
	case 0:
	default:
		return "Off";
	}
}

int ParseStage7PrecipitationMode(const std::string& value)
{
	const std::string lower = ToLowerAscii(value);
	if (lower == "none" || lower == "off" || lower == "0")
	{
		return 0;
	}
	if (lower == "cards" || lower == "card" || lower == "3d" || lower == "2")
	{
		return 2;
	}
	return 1;
}

const char* Stage7PrecipitationModeName(int mode)
{
	switch (mode)
	{
	case 0: return "None";
	case 2: return "Cards";
	case 1:
	default:
		return "ScreenOverlay";
	}
}

IRStage5ToneMap ParseStage5ToneMap(const std::string& value)
{
	std::string lower = ToLowerAscii(value);
	if (lower == "linear")
	{
		return IRStage5ToneMap::Linear;
	}
	if (lower == "log")
	{
		return IRStage5ToneMap::Log;
	}
	return IRStage5ToneMap::Asinh;
}

const char* Stage5ToneMapName(IRStage5ToneMap toneMap)
{
	switch (toneMap)
	{
	case IRStage5ToneMap::Linear: return "linear";
	case IRStage5ToneMap::Log: return "log";
	case IRStage5ToneMap::Asinh:
	default:
		return "asinh";
	}
}

int Stage5BandIndex(IRBand band)
{
	const int index = static_cast<int>(band);
	return (index >= 0 && index < 5) ? index : static_cast<int>(IRBand::MidWaveInfrared);
}

double DefaultStage5SolarReflectanceWeight(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 1.0;
	case IRBand::NearInfrared: return 0.8;
	case IRBand::ShortWaveInfrared: return 0.7;
	case IRBand::MidWaveInfrared: return 0.15;
	case IRBand::LongWaveInfrared: return 0.0;
	default:
		return 0.0;
	}
}

IRBand Stage5BandFromIndex(int index)
{
	switch (index)
	{
	case 0: return IRBand::Visible;
	case 1: return IRBand::NearInfrared;
	case 2: return IRBand::ShortWaveInfrared;
	case 3: return IRBand::MidWaveInfrared;
	case 4: return IRBand::LongWaveInfrared;
	default:
		return IRBand::MidWaveInfrared;
	}
}

int IRBandClassForShader(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible:
	case IRBand::NearInfrared:
	case IRBand::ShortWaveInfrared:
		return 0; // reflective: VIS/NIR/SWIR
	case IRBand::MidWaveInfrared:
		return 1; // mixed: MWIR
	case IRBand::LongWaveInfrared:
		return 2; // thermal: LWIR
	default:
		return 1;
	}
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
		char c = text[end];
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

void ApplyStage5DebugConfigObject(const std::string& objectText, IRRadianceModelV2DebugConfig& config, bool& useBaseTextureModulation)
{
	std::string toneMapText;
	if (ExtractJsonString(objectText, "Stage5DebugToneMap", toneMapText))
	{
		config.toneMap = ParseStage5ToneMap(toneMapText);
	}
	ExtractJsonNumber(objectText, "Stage5BodyRadianceScale", config.bodyRadianceScale);
	ExtractJsonNumber(objectText, "Stage5HotspotRadianceScale", config.hotspotRadianceScale);
	ExtractJsonNumber(objectText, "Stage5BrightspotRadianceScale", config.brightspotRadianceScale);
	ExtractJsonNumber(objectText, "Stage5DebugMinBodyGray", config.minBodyGray);
	ExtractJsonNumber(objectText, "Stage5SolarReflectanceWeight", config.solarReflectanceWeight);
	ExtractJsonNumber(objectText, "Stage5BodyDisplayGain", config.bodyDisplayGain);
	ExtractJsonNumber(objectText, "Stage5ReflectedDisplayGain", config.reflectedDisplayGain);
	ExtractJsonNumber(objectText, "Stage5HotspotDisplayGain", config.hotspotDisplayGain);
	ExtractJsonNumber(objectText, "Stage5BrightspotDisplayGain", config.brightspotDisplayGain);
	ExtractJsonNumber(objectText, "Stage5CompositeMinGray", config.compositeMinGray);
	ExtractJsonNumber(objectText, "Stage5CompositeMaxGray", config.compositeMaxGray);
	ExtractJsonBool(objectText, "Stage5UseBaseTextureModulation", useBaseTextureModulation);
}

bool LoadStage5DebugDisplayConfig(
	const std::string& path,
	IRRadianceModelV2DebugConfig configs[5],
	bool useBaseTextureModulationByBand[5])
{
	for (int i = 0; i < 5; ++i)
	{
		configs[i] = IRRadianceModelV2DebugConfig();
		configs[i].solarReflectanceWeight = DefaultStage5SolarReflectanceWeight(Stage5BandFromIndex(i));
		useBaseTextureModulationByBand[i] = false;
	}

	std::string text;
	if (!ReadWholeFile(path, text))
	{
		return false;
	}

	IRRadianceModelV2DebugConfig defaultConfig;
	bool defaultUseBaseTextureModulation = false;
	std::string defaultsText;
	if (FindJsonObject(text, "defaults", defaultsText))
	{
		ApplyStage5DebugConfigObject(defaultsText, defaultConfig, defaultUseBaseTextureModulation);
	}
	for (int i = 0; i < 5; ++i)
	{
		configs[i] = defaultConfig;
		configs[i].solarReflectanceWeight = DefaultStage5SolarReflectanceWeight(Stage5BandFromIndex(i));
		useBaseTextureModulationByBand[i] = defaultUseBaseTextureModulation;
	}

	std::string bandsText;
	if (!FindJsonObject(text, "bands", bandsText))
	{
		return true;
	}

	const IRBand bands[5] = {
		IRBand::Visible,
		IRBand::NearInfrared,
		IRBand::ShortWaveInfrared,
		IRBand::MidWaveInfrared,
		IRBand::LongWaveInfrared
	};
	for (int i = 0; i < 5; ++i)
	{
		std::string bandText;
		if (FindJsonObject(bandsText, IRBandName(bands[i]), bandText))
		{
			ApplyStage5DebugConfigObject(bandText, configs[Stage5BandIndex(bands[i])], useBaseTextureModulationByBand[Stage5BandIndex(bands[i])]);
		}
	}
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_VIS", configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_NIR", configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_SWIR", configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_MWIR", configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight);
	ExtractJsonNumber(text, "Stage5SolarReflectanceWeight_LWIR", configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight);
	return true;
}

void ApplyStage5RuntimeOverrides(IRRuntimeConfig& runtimeConfig, IRRadianceModelV2DebugConfig configs[5], bool useBaseTextureModulationByBand[5])
{
	for (int i = 0; i < 5; ++i)
	{
		configs[i].toneMap = ParseStage5ToneMap(runtimeConfig.getString("Stage5", "DebugToneMap", "Stage5DebugToneMap", Stage5ToneMapName(configs[i].toneMap)));
		configs[i].bodyRadianceScale = runtimeConfig.getDouble("Stage5", "BodyRadianceScale", "Stage5BodyRadianceScale", configs[i].bodyRadianceScale);
		configs[i].hotspotRadianceScale = runtimeConfig.getDouble("Stage5", "HotspotRadianceScale", "Stage5HotspotRadianceScale", configs[i].hotspotRadianceScale);
		configs[i].brightspotRadianceScale = runtimeConfig.getDouble("Stage5", "BrightspotRadianceScale", "Stage5BrightspotRadianceScale", configs[i].brightspotRadianceScale);
		configs[i].minBodyGray = runtimeConfig.getDouble("Stage5", "DebugMinBodyGray", "Stage5DebugMinBodyGray", configs[i].minBodyGray);
		configs[i].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight", "Stage5SolarReflectanceWeight", configs[i].solarReflectanceWeight);
		configs[i].bodyDisplayGain = runtimeConfig.getDouble("Stage5", "BodyDisplayGain", "Stage5BodyDisplayGain", configs[i].bodyDisplayGain);
		configs[i].reflectedDisplayGain = runtimeConfig.getDouble("Stage5", "ReflectedDisplayGain", "Stage5ReflectedDisplayGain", configs[i].reflectedDisplayGain);
		configs[i].hotspotDisplayGain = runtimeConfig.getDouble("Stage5", "HotspotDisplayGain", "Stage5HotspotDisplayGain", configs[i].hotspotDisplayGain);
		configs[i].brightspotDisplayGain = runtimeConfig.getDouble("Stage5", "BrightspotDisplayGain", "Stage5BrightspotDisplayGain", configs[i].brightspotDisplayGain);
		configs[i].compositeMinGray = runtimeConfig.getDouble("Stage5", "CompositeMinGray", "Stage5CompositeMinGray", configs[i].compositeMinGray);
		configs[i].compositeMaxGray = runtimeConfig.getDouble("Stage5", "CompositeMaxGray", "Stage5CompositeMaxGray", configs[i].compositeMaxGray);
		useBaseTextureModulationByBand[i] = runtimeConfig.getBool("Stage5", "UseBaseTextureModulation", "Stage5UseBaseTextureModulation", useBaseTextureModulationByBand[i]);
	}
	configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight_VIS", "Stage5SolarReflectanceWeight_VIS", configs[Stage5BandIndex(IRBand::Visible)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight_NIR", "Stage5SolarReflectanceWeight_NIR", configs[Stage5BandIndex(IRBand::NearInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight_SWIR", "Stage5SolarReflectanceWeight_SWIR", configs[Stage5BandIndex(IRBand::ShortWaveInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight_MWIR", "Stage5SolarReflectanceWeight_MWIR", configs[Stage5BandIndex(IRBand::MidWaveInfrared)].solarReflectanceWeight);
	configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight = runtimeConfig.getDouble("Stage5", "SolarReflectanceWeight_LWIR", "Stage5SolarReflectanceWeight_LWIR", configs[Stage5BandIndex(IRBand::LongWaveInfrared)].solarReflectanceWeight);
}

std::string FirstExistingPath(const std::vector<std::string>& paths)
{
	for (size_t i = 0; i < paths.size(); ++i)
	{
		if (FileExists(paths[i]))
		{
			return paths[i];
		}
	}
	return paths.empty() ? std::string() : paths[0];
}

std::vector<std::string> BuildRuntimeConfigPathCandidates(const std::string& configuredPath)
{
	std::vector<std::string> paths;
	if (!configuredPath.empty())
	{
		paths.push_back(configuredPath);
		paths.push_back("../Bin/" + configuredPath);
		paths.push_back("HwaSim_IR/Bin/" + configuredPath);
		paths.push_back("../HwaSim_IR/Bin/" + configuredPath);
	}
	return paths;
}

double WeatherSunStrength(int weatherCode, double sunElevationDeg)
{
	if (sunElevationDeg <= 0.0)
	{
		return 0.0;
	}
	switch (weatherCode)
	{
	case 0: return 1.0;   // 晴天：太阳直射完整保留
	case 1: return 0.65;  // 云：削弱直射，保留部分散射
	case 2: return 0.45;  // 雨：直射和能见度同时下降
	case 3: return 0.55;  // 雪：直射下降，但地表反照可后续增强
	case 4: return 0.30;  // 雾：太阳项大幅衰减，程辐射增强
	case 5: return 0.42;  // 阴：无明显直射，以漫射近似
	default: return 0.75;
	}
}
}


// ========== HwaSimIRMainApp 类实现 ==========
HwaSimIR::HwaSimIR(int argc, char** argv)
	: m_pFramework(new PandaFramework()), m_pMainWindow(nullptr)
	,m_isAddPlatform(false), m_isSimRunning(false), m_currentRound(0), m_isCameraAttached(false), m_isInitTargetPlatID(false), m_stage0DisplayFrameCount(0){
	// 关闭垂直同步，突破帧率上限
	load_prc_file_data("", "sync-video false");
	// 初始化HwaSimIR框架（解析命令行参数）
	m_pFramework->open_framework(argc, argv);
	m_runtimeConfig.loadFromCandidates(BuildRuntimeConfigCandidatePaths());
	LoadRenderBackendConfig();

	if (IsVisibleWindowMode())
	{
		//load_prc_file_data("", "win-size 800 800");
		// 设置初始窗口属性（800x800、标题、置顶）
		WindowProperties init_props;
		init_props.set_size(800, 800);
		init_props.set_title("HwaSimIR");
		init_props.set_foreground(true);

		// 打开主窗口（使用默认GraphicsPipe）
		m_pMainWindow = m_pFramework->open_window(
			init_props,
			0,
			m_pFramework->get_default_pipe()
		);
		if (m_pMainWindow == nullptr)
		{
			m_renderBackendReady = false;
			LogRenderBackendConfig("open_window_failed");
			std::cerr << "[Fatal][RenderBackend]"
				<< " mode=" << m_renderPresentationModeName
				<< " failure=open_window_failed"
				<< " message=VisibleWindow requires a valid Panda3D window"
				<< std::endl;
			return;
		}

		//m_pGraphicsOutput = m_pMainWindow->get_graphics_window();
		m_pGraphicsWindow = m_pMainWindow->get_graphics_window();
		if (m_pGraphicsWindow == nullptr)
		{
			m_renderBackendReady = false;
			LogRenderBackendConfig("graphics_window_unavailable");
			std::cerr << "[Fatal][RenderBackend]"
				<< " mode=" << m_renderPresentationModeName
				<< " failure=graphics_window_unavailable"
				<< " message=VisibleWindow created without GraphicsWindow"
				<< std::endl;
			return;
		}
		m_pGraphicsWindow->request_properties(init_props);
		m_renderRoot = m_pMainWindow->get_render();
		m_renderBackendReady = true;
		InitVisibleWindowUi();
		LogGraphicsBackend();
	}
	else
	{
		m_renderRoot = NodePath("HeadlessOffscreenRenderRoot");
		m_camera = nullptr;
		m_cameraLens = nullptr;
		m_renderBackendReady = true;
		std::cout << "[RenderBackend][TODO]"
			<< " mode=" << m_renderPresentationModeName
			<< " headless_final_pipeline_not_ready=1"
			<< " stage=H1"
			<< std::endl;
	}

	SetRenderMode(true, 0);
	//SetRenderMode(false, 0);

	LogRenderBackendConfig("startup");

	// 公共初始化：以渲染后端基础状态为门闩，不再依赖 m_pMainWindow。
	if (IsRenderBackendReady()) {
		std::cout << "[BuildStamp] stage6b4_stage7a2_source_active=1"
			<< " compile_date=" << __DATE__
			<< " compile_time=" << __TIME__
			<< std::endl;
		InitCommonCaptureTask();
		//register_custom_functions();
	// 初始化通讯线程
		LoadNetworkConfig();
		InitUdpThread();

		InitTcpThread();
	// 初始化平台模型路径
		InitPlatformModels();

		// 编译红外着色器
		InitInfraredShader();
		SetupStage6FinalPipeline(800, 800, "startup");
		InitInfraredSimulation();
		InitSkyAndCloudScene();

		// 向全局任务管理器添加红外动态更新任务
		PT(GenericAsyncTask) ir_task = new GenericAsyncTask("IR_UpdateTask", &HwaSimIR::shader_update_task, this);
		AsyncTaskManager::get_global_ptr()->add(ir_task);

	}
	else {
		std::cerr << "[Fatal][RenderBackend] render backend is not ready; startup aborted safely." << std::endl;
	}
}

HwaSimIR::~HwaSimIR() {

	m_annotationManager.clear();

	// 删除所有平台
	m_isAddPlatform = false;
	ProcessAddRemovePakPlatform();
	ProcessAddRemoveWeaponPlatform();
	ProcessAddRemoveTargetPlatform();

	// 停止UDP线程
	if (m_pUdpThread) {
		m_pUdpThread->stop();
		delete m_pUdpThread;
		m_pUdpThread = nullptr;
	}
	if (m_pTcpThread) {
		m_pTcpThread->stop();
		delete m_pTcpThread;
		m_pTcpThread = nullptr;
	}
	// 清理HwaSimIR框架和窗口资源
	if (m_pFramework) {
		m_pFramework->close_all_windows();
		m_pFramework->close_framework();
		delete m_pFramework;
		m_pFramework = nullptr;
	}
	m_pMainWindow = nullptr;
}

//void HwaSimIR::run() {
//	// 启动HwaSimIR主循环（对应Qt的exec()）
//	if (m_pMainWindow && m_pFramework) {
//		std::cout << "应用程序已启动。按ESC键退出。" << std::endl;
//		m_pFramework->main_loop();
//	}
//}

void HwaSimIR::run() {
	if (!m_pFramework || !IsRenderBackendReady()) return;

	std::cout << "应用程序已启动。按ESC键退出。" << std::endl;

	Thread* current_thread = Thread::get_current_thread();

	// 接管主循环
	while (true) {
		if (m_requestExit.load()) {
			break;
		}

		ProcessPendingNetworkCommands();

		PendingDisplayFrame pendingFrame;
		bool hasDisplayFrame = false;
		if (m_bSyncRenderMode.load() && m_isSimRunning.load()) {
			std::unique_lock<std::mutex> lock(m_mtx);
			m_cvNewData.wait_for(lock, std::chrono::milliseconds(100), [this] {
				return !m_pendingDisplayFrames.empty()
					|| !m_pendingNetworkCommands.empty()
					|| m_requestExit.load();
			});
			// 解锁：如果不解锁，接下来的 do_frame 内部任务将无法获取锁，导致死锁
			lock.unlock();
		}

		ProcessPendingNetworkCommands();

		{
			std::unique_lock<std::mutex> lock(m_mtx);
			if (!m_pendingDisplayFrames.empty()) {
				if (m_bSyncRenderMode.load())
				{
					pendingFrame = m_pendingDisplayFrames.front();
				}
				else
				{
					pendingFrame = m_pendingDisplayFrames.back();
				}
				m_pendingDisplayFrames.pop_front();
				if (!m_bSyncRenderMode.load())
				{
					m_pendingDisplayFrames.clear();
				}
				pendingFrame.telemetry.processStartTimeNs = IRPerfStats::steadyTimeNs();
				m_currentFrameTelemetry = pendingFrame.telemetry;
				m_realTimeSceneData = pendingFrame.data;
				hasDisplayFrame = true;
				m_perfStats.recordInputQueueDepth(static_cast<int>(m_pendingDisplayFrames.size()));
			}
		}
		if (hasDisplayFrame) {
			m_cvDisplayQueueSpace.notify_one();
			const auto sceneBegin = std::chrono::steady_clock::now();
			ProcessRealSimSceneDrivenData();
			m_perfStats.recordSceneUpdate(std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - sceneBegin).count());
		}

		m_syncFrameActive.store(!m_bSyncRenderMode.load() || hasDisplayFrame);
		const auto renderBegin = std::chrono::steady_clock::now();
		if (!m_pFramework->do_frame(current_thread)) {
			break;
		}
		const double renderMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - renderBegin).count();
		if (!m_bSyncRenderMode.load() || hasDisplayFrame)
		{
			const bool mtfEffective = m_stage6MtfBlurEnabled &&
				m_stage6MtfApplyTo == "final_display" &&
				m_stage6MtfBlurRadiusPixels > 0 &&
				m_stage6MtfBlurSigmaPixels > 0.001;
			const double mtfBlurMs = mtfEffective ? renderMs : 0.0;
			const bool detectorNoiseHasSource =
				(m_stage6TemporalNoiseEnabled && m_stage6TemporalNoiseSigmaGray > 0.0) ||
				(m_stage6FpnEnabled && m_stage6FpnSigmaGray > 0.0) ||
				(m_stage6ColumnNoiseEnabled && m_stage6ColumnNoiseSigmaGray > 0.0) ||
				(m_stage6RowNoiseEnabled && m_stage6RowNoiseSigmaGray > 0.0) ||
				(m_stage6BadPixelsEnabled && m_stage6BadPixelRatio > 0.0);
			const bool detectorNoiseEffective =
				m_stage6DetectorNoiseEnabled &&
				m_stage6DetectorNoiseApplyTo == "final_display" &&
				detectorNoiseHasSource;
			const double detectorNoiseMs = detectorNoiseEffective ? renderMs : 0.0;
			m_perfStats.recordRender(
				renderMs,
				mtfBlurMs,
				mtfEffective,
				m_stage6MtfBlurSigmaPixels,
				m_stage6MtfBlurRadiusPixels,
				detectorNoiseMs,
				detectorNoiseEffective,
				m_stage6TemporalNoiseSigmaGray,
				m_stage6FpnSigmaGray,
				m_stage6ColumnNoiseSigmaGray,
				m_stage6RowNoiseSigmaGray,
				m_stage6BadPixelRatio,
				m_stage6DetectorNoisePosition.c_str());
			LogStage6MtfBlur(m_currentFrameTelemetry.sourceSeq, renderMs);
			LogStage6DetectorNoise(m_currentFrameTelemetry.sourceSeq, renderMs);
			LogStage6Agc(m_currentFrameTelemetry.sourceSeq);
		}
		m_syncFrameActive.store(false);
		m_perfStats.maybeLog();
		if (m_requestExit.load()) {
			break;
		}
	}
}

WindowFramework* HwaSimIR::get_main_window() const {
	return m_pMainWindow;
}

void HwaSimIR::LogGraphicsBackend() const
{
	if (m_pGraphicsWindow == nullptr)
	{
		std::cout << "[GPU][WARN] graphics window unavailable" << std::endl;
		return;
	}

	GraphicsPipe* pipe = m_pGraphicsWindow->get_pipe();
	GraphicsStateGuardian* gsg = m_pGraphicsWindow->get_gsg();
	const std::string pipeName = pipe != nullptr ? pipe->get_interface_name() : "unknown";
	const std::string vendor = gsg != nullptr ? gsg->get_driver_vendor() : "unknown";
	const std::string renderer = gsg != nullptr ? gsg->get_driver_renderer() : "unknown";
	const std::string driverVersion = gsg != nullptr ? gsg->get_driver_version() : "unknown";
	std::cout << "[GPU]"
		<< " graphicsPipe=" << pipeName
		<< " vendor=" << vendor
		<< " renderer=" << renderer
		<< " driverVersion=" << driverVersion
		<< " apiVersion=" << driverVersion
		<< std::endl;

	std::string rendererLower = renderer;
	std::transform(rendererLower.begin(), rendererLower.end(), rendererLower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (rendererLower.find("llvmpipe") != std::string::npos ||
		rendererLower.find("softpipe") != std::string::npos ||
		rendererLower.find("software") != std::string::npos)
	{
		std::cout << "[GPU][WARN] software renderer detected"
			<< " renderer=" << renderer << std::endl;
	}
}

void HwaSimIR::LoadRenderBackendConfig()
{
	std::string modeSource;
	std::string previewSource;
	std::string frameRateSource;
	std::string widthSource;
	std::string heightSource;
	const std::string requestedMode = m_runtimeConfig.getString(
		"RenderBackend",
		"PresentationMode",
		"RenderPresentationMode",
		"VisibleWindow",
		&modeSource);
	const std::string modeLower = ToLowerAscii(requestedMode);
	if (modeLower == "visiblewindow" || modeLower == "visible_window" || modeLower == "window")
	{
		m_renderPresentationMode = RenderPresentationMode::VisibleWindow;
		m_renderPresentationModeName = "VisibleWindow";
	}
	else if (modeLower == "headlessoffscreen" || modeLower == "headless_offscreen" || modeLower == "headless")
	{
		m_renderPresentationMode = RenderPresentationMode::HeadlessOffscreen;
		m_renderPresentationModeName = "HeadlessOffscreen";
	}
	else
	{
		std::cout << "[RenderBackend][WARN]"
			<< " invalid PresentationMode=" << requestedMode
			<< " fallback=VisibleWindow"
			<< std::endl;
		m_renderPresentationMode = RenderPresentationMode::VisibleWindow;
		m_renderPresentationModeName = "VisibleWindow";
	}

	m_renderWindowPreview = m_runtimeConfig.getBool(
		"RenderBackend",
		"WindowPreview",
		"RenderWindowPreview",
		true,
		&previewSource);
	m_renderEnableFrameRateMeter = m_runtimeConfig.getBool(
		"RenderBackend",
		"EnableFrameRateMeter",
		"RenderEnableFrameRateMeter",
		true,
		&frameRateSource);
	m_headlessWidth = m_runtimeConfig.getInt(
		"RenderBackend",
		"HeadlessWidth",
		"RenderHeadlessWidth",
		800,
		&widthSource);
	m_headlessHeight = m_runtimeConfig.getInt(
		"RenderBackend",
		"HeadlessHeight",
		"RenderHeadlessHeight",
		800,
		&heightSource);
	if (m_headlessWidth <= 0)
	{
		std::cout << "[RenderBackend][WARN]"
			<< " invalid HeadlessWidth=" << m_headlessWidth
			<< " fallback=800"
			<< std::endl;
		m_headlessWidth = 800;
	}
	if (m_headlessHeight <= 0)
	{
		std::cout << "[RenderBackend][WARN]"
			<< " invalid HeadlessHeight=" << m_headlessHeight
			<< " fallback=800"
			<< std::endl;
		m_headlessHeight = 800;
	}
	m_headlessWidth = std::max(1, std::min(8192, m_headlessWidth));
	m_headlessHeight = std::max(1, std::min(8192, m_headlessHeight));

	std::cout << "[RenderBackendConfig]"
		<< " PresentationMode=" << m_renderPresentationModeName
		<< " WindowPreview=" << (m_renderWindowPreview ? "1" : "0")
		<< " EnableFrameRateMeter=" << (m_renderEnableFrameRateMeter ? "1" : "0")
		<< " HeadlessSize=" << m_headlessWidth << "x" << m_headlessHeight
		<< " source=" << modeSource << "/" << previewSource << "/" << frameRateSource
		<< "/" << widthSource << "/" << heightSource
		<< std::endl;
}

bool HwaSimIR::IsVisibleWindowMode() const
{
	return m_renderPresentationMode == RenderPresentationMode::VisibleWindow;
}

bool HwaSimIR::IsHeadlessOffscreenMode() const
{
	return m_renderPresentationMode == RenderPresentationMode::HeadlessOffscreen;
}

bool HwaSimIR::IsRenderBackendReady() const
{
	return m_renderBackendReady;
}

void HwaSimIR::LogRenderBackendConfig(const char* reason) const
{
	std::cout << "[RenderBackend]"
		<< " mode=" << m_renderPresentationModeName
		<< " windowCreated=" << (m_pMainWindow != nullptr ? "1" : "0")
		<< " renderBackendReady=" << (m_renderBackendReady ? "1" : "0")
		<< " reason=" << (reason != nullptr ? reason : "unknown")
		<< " graphicsWindow=" << (m_pGraphicsWindow != nullptr ? "1" : "0")
		<< " windowPreview=" << (m_renderWindowPreview ? "1" : "0")
		<< " enableFrameRateMeter=" << (m_renderEnableFrameRateMeter ? "1" : "0")
		<< " headlessSize=" << m_headlessWidth << "x" << m_headlessHeight
		<< std::endl;
}

// 修改窗口分辨率
void HwaSimIR::resize_window(int new_width, int new_height) {
	if (IsHeadlessOffscreenMode()) {
		const int safeWidth = std::max(1, new_width);
		const int safeHeight = std::max(1, new_height);
		if (m_renderTex != nullptr) {
			m_renderTex->setup_2d_texture(safeWidth, safeHeight, Texture::T_unsigned_byte, Texture::F_rgb);
		}
		std::cout << "[Stage6 Resize][TODO]"
			<< " renderMode=" << m_renderPresentationModeName
			<< " renderTexture=" << safeWidth << "x" << safeHeight
			<< " headless_final_pipeline_not_ready=1"
			<< std::endl;
		return;
	}

	if (!m_pMainWindow) return;

	GraphicsWindow* graphics_win = m_pMainWindow->get_graphics_window();
	if (!graphics_win) return;

	const WindowProperties old_props = graphics_win->get_properties();
	const int old_width = old_props.get_x_size();
	const int old_height = old_props.get_y_size();

	// 复制当前窗口属性，仅修改尺寸
	WindowProperties new_props = graphics_win->get_properties();
	new_props.set_size(new_width, new_height);

	graphics_win->request_properties(new_props);
	m_pMainWindow->adjust_dimensions();

	if (m_renderTex != nullptr) {
		m_renderTex->setup_2d_texture(new_width, new_height, Texture::T_unsigned_byte, Texture::F_rgb);
	}

	std::cout << "[Stage6 Resize]"
		<< " oldWindow=" << old_width << "x" << old_height
		<< " newWindow=" << new_width << "x" << new_height
		<< " renderTexture=" << new_width << "x" << new_height
		<< " resizeRequest=1"
		<< std::endl;
}

void HwaSimIR::ApplySensorOutputConfig(const IRSensorDisplayConfig& config, const char* reason)
{
	m_sensorDisplayConfig = config;
	m_sensorDisplayConfigReady = true;
	m_stage6FarClipWarningLogged = false;
	m_stage7NearFarClipWarningLogged = false;
	m_stage6CaptureLogCounter = 0;

	if (m_cameraLens != nullptr) {
		m_cameraLens->set_fov(config.horizontalFovDeg, config.verticalFovDeg);
		m_cameraLens->set_near_far(config.nearClipM, config.farClipM);
	}

	resize_window(config.width, config.height);
	SetupStage6FinalPipeline(config.width, config.height, reason);
	LogStage6SensorGeometry(config, reason);
}

void HwaSimIR::LogStage6SensorGeometry(const IRSensorDisplayConfig& config, const char* reason) const
{
	const char* safeReason = (reason != nullptr) ? reason : "unknown";
	std::cout << "[Stage6 SensorGeometry]"
		<< " reason=" << safeReason
		<< " width=" << config.width
		<< " height=" << config.height
		<< " viewMin=" << config.viewMinM
		<< " viewMax=" << config.viewMaxM
		<< " pixelAngleUrad=" << config.pixelAngleUrad
		<< " horizontalFovDeg=" << config.horizontalFovDeg
		<< " verticalFovDeg=" << config.verticalFovDeg
		<< " nearClipM=" << config.nearClipM
		<< " farClipM=" << config.farClipM
		<< " fov_source=" << config.fovSource
		<< std::endl;

	if (config.widthFallback || config.heightFallback || config.nearFallback ||
		config.farFallback || config.pixelAngleFallback || config.pixelAngleRangeWarning) {
		std::cout << "[Stage6 SensorGeometry][WARN]"
			<< " requestedWidth=" << config.requestedWidth
			<< " requestedHeight=" << config.requestedHeight
			<< " requestedViewMin=" << config.requestedViewMinM
			<< " requestedViewMax=" << config.requestedViewMaxM
			<< " requestedPixelAngleUrad=" << config.requestedPixelAngleUrad
			<< " widthFallback=" << (config.widthFallback ? "1" : "0")
			<< " heightFallback=" << (config.heightFallback ? "1" : "0")
			<< " nearFallback=" << (config.nearFallback ? "1" : "0")
			<< " farFallback=" << (config.farFallback ? "1" : "0")
			<< " pixelAngleFallback=" << (config.pixelAngleFallback ? "1" : "0")
			<< " pixelAngleRangeWarning=" << (config.pixelAngleRangeWarning ? "1" : "0")
			<< " pixelAngleClamped=" << (config.pixelAngleClamped ? "1" : "0")
			<< std::endl;
	}
}

void HwaSimIR::ApplyStage6DisplayConfig(const BYHWICD::trackerSensorParam& sensor, const char* reason)
{
	IRSensorPostProcessConfig config;
	std::string whiteHotSource;
	std::string gainSource;
	std::string offsetSource;
	std::string applyToWindowSource;
	std::string backgroundSource;
	std::string noiseOverrideSource;
	std::string noiseEnableSource;
	std::string noiseSigmaSource;
	const bool protocolNoisePresent = sensor.noiseEn || sensor.trackerSensorNoise > 0.0;

	config.whiteHot = m_runtimeConfig.getBool("Stage6Display", "WhiteHot", "Stage6WhiteHot", true, &whiteHotSource);
	config.displayGain = m_runtimeConfig.getDouble("Stage6Display", "DisplayGain", "Stage6DisplayGain", 1.0, &gainSource);
	config.displayOffset = m_runtimeConfig.getDouble("Stage6Display", "DisplayOffset", "Stage6DisplayOffset", 0.0, &offsetSource);
	config.applyToWindow = m_runtimeConfig.getBool("Stage6Display", "ApplyToWindow", "Stage6DisplayApplyToWindow", true, &applyToWindowSource);
	config.applyToCapture = false;
	config.backgroundDisplayEnable = m_runtimeConfig.getBool("Stage6Display", "BackgroundDisplayEnable", "Stage6BackgroundDisplayEnable", true, &backgroundSource);
	config.noiseOverrideEnable = m_runtimeConfig.getBool("Stage6Display", "NoiseOverrideEnable", "Stage6NoiseOverrideEnable", true, &noiseOverrideSource);

	const bool configuredNoiseEnable = m_runtimeConfig.getBool("Stage6Display", "NoiseEnable", "Stage6NoiseEnable", false, &noiseEnableSource);
	const double configuredNoiseSigma = std::max(0.0, m_runtimeConfig.getDouble("Stage6Display", "NoiseSigmaGray", "Stage6NoiseSigmaGray", 0.0, &noiseSigmaSource));
	if (config.noiseOverrideEnable) {
		config.noiseEnable = configuredNoiseEnable;
		config.noiseSigmaGray = configuredNoiseSigma;
		config.noiseSource = (noiseEnableSource == "env" || noiseSigmaSource == "env") ? "env" :
			((noiseEnableSource == "ini" || noiseSigmaSource == "ini") ? "ini" : "default");
	}
	else if (protocolNoisePresent) {
		config.noiseEnable = sensor.noiseEn;
		config.noiseSigmaGray = std::max(0.0, sensor.trackerSensorNoise);
		config.noiseSource = "protocol";
	}
	else {
		config.noiseEnable = configuredNoiseEnable;
		config.noiseSigmaGray = configuredNoiseSigma;
		config.noiseSource = (noiseEnableSource == "env" || noiseSigmaSource == "env") ? "env" :
			((noiseEnableSource == "ini" || noiseSigmaSource == "ini") ? "ini" : "default");
	}
	const bool anyEnvSource = whiteHotSource == "env" || gainSource == "env" || offsetSource == "env" ||
		applyToWindowSource == "env" || backgroundSource == "env" || noiseOverrideSource == "env" ||
		config.noiseSource == "env";
	const bool anyIniSource = whiteHotSource == "ini" || gainSource == "ini" || offsetSource == "ini" ||
		applyToWindowSource == "ini" || backgroundSource == "ini" || noiseOverrideSource == "ini" ||
		config.noiseSource == "ini";
	if (config.noiseSource == "protocol")
	{
		config.source = anyEnvSource ? "env+protocol" : (anyIniSource ? "ini+protocol" : "protocol");
	}
	else
	{
		config.source = anyEnvSource ? "env" : (anyIniSource ? "ini" : "default");
	}

	m_stage6DisplayConfig = config;
	m_stage6DisplayConfigReady = true;
	m_stage6DisplayLogCounter = 0;
	LogStage6DisplayConfig(config, reason);
	LogStage6DisplayRoute(config, reason);
	RefreshStage6DisplayShaderInputs();
	ApplyStage6FinalPostprocessInputs();
	LogStage6FinalPipeline(reason);
}

void HwaSimIR::LogStage6DisplayConfig(const IRSensorPostProcessConfig& config, const char* reason) const
{
	const char* safeReason = (reason != nullptr) ? reason : "unknown";
	std::cout << "[Stage6 Display]"
		<< " reason=" << safeReason
		<< " whiteHot=" << (config.whiteHot ? "1" : "0")
		<< " displayGain=" << config.displayGain
		<< " displayOffset=" << config.displayOffset
		<< " noiseEnable=" << (config.noiseEnable ? "1" : "0")
		<< " noiseSigmaGray=" << config.noiseSigmaGray
		<< " noiseOverrideEnable=" << (config.noiseOverrideEnable ? "1" : "0")
		<< " noiseSource=" << config.noiseSource
		<< " applyToWindow=" << (config.applyToWindow ? "1" : "0")
		<< " applyToCapture=" << (config.applyToCapture ? "1" : "0")
		<< " backgroundDisplay=" << (config.backgroundDisplayEnable ? "1" : "0")
		<< " configSource=" << config.source
		<< " effectiveWhiteHot=" << (config.whiteHot ? "1" : "0")
		<< " effectiveNoiseEnable=" << (config.noiseEnable ? "1" : "0")
		<< " effectiveNoiseSigmaGray=" << config.noiseSigmaGray
		<< " source=" << config.source
		<< std::endl;
}

void HwaSimIR::LogStage6DisplayRoute(const IRSensorPostProcessConfig& config, const char* reason) const
{
	const char* safeReason = (reason != nullptr) ? reason : "unknown";
	std::cout << "[Stage6 DisplayRoute]"
		<< " reason=" << safeReason
		<< " route=final_sensor"
		<< " deprecatedRoute=1"
		<< " finalRoute=final_sensor"
		<< " applyToWindow=" << (config.applyToWindow ? "1" : "0")
		<< " applyToCapture=" << (config.applyToCapture ? "1" : "0")
		<< " backgroundDisplay=" << (config.backgroundDisplayEnable ? "1" : "0")
		<< std::endl;
}

std::uintptr_t HwaSimIR::ShaderInputCacheKey(const NodePath& node) const
{
	if (node.is_empty())
	{
		return 0;
	}
	return reinterpret_cast<std::uintptr_t>(node.node());
}

bool HwaSimIR::SetShaderInputCached(NodePath& node, const char* name, const LVecBase2f& value, bool force)
{
	if (node.is_empty() || name == nullptr)
	{
		return false;
	}
	const auto applyStart = m_measureShaderInputApplyTime ? std::chrono::high_resolution_clock::now() : std::chrono::high_resolution_clock::time_point();
	const std::uintptr_t key = ShaderInputCacheKey(node);
	ShaderInputCachedValue& cached = m_shaderInputCache[key][name];
	const bool same = cached.kind == 1 &&
		cached.count == 2 &&
		std::fabs(cached.floats[0] - value[0]) <= m_shaderInputFloatEpsilon &&
		std::fabs(cached.floats[1] - value[1]) <= m_shaderInputFloatEpsilon;
	if (!force && same)
	{
		++m_shaderInputStats.skipCount;
		if (m_measureShaderInputApplyTime)
		{
			m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
				std::chrono::high_resolution_clock::now() - applyStart).count();
		}
		return false;
	}
	cached.kind = 1;
	cached.count = 2;
	cached.floats[0] = value[0];
	cached.floats[1] = value[1];
	node.set_shader_input(InternalName::make(name), value);
	++m_shaderInputStats.setCount;
	if (m_measureShaderInputApplyTime)
	{
		m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - applyStart).count();
	}
	return true;
}

bool HwaSimIR::SetShaderInputCached(NodePath& node, const char* name, const LVecBase3f& value, bool force)
{
	if (node.is_empty() || name == nullptr)
	{
		return false;
	}
	const auto applyStart = m_measureShaderInputApplyTime ? std::chrono::high_resolution_clock::now() : std::chrono::high_resolution_clock::time_point();
	const std::uintptr_t key = ShaderInputCacheKey(node);
	ShaderInputCachedValue& cached = m_shaderInputCache[key][name];
	const bool same = cached.kind == 1 &&
		cached.count == 3 &&
		std::fabs(cached.floats[0] - value[0]) <= m_shaderInputFloatEpsilon &&
		std::fabs(cached.floats[1] - value[1]) <= m_shaderInputFloatEpsilon &&
		std::fabs(cached.floats[2] - value[2]) <= m_shaderInputFloatEpsilon;
	if (!force && same)
	{
		++m_shaderInputStats.skipCount;
		if (m_measureShaderInputApplyTime)
		{
			m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
				std::chrono::high_resolution_clock::now() - applyStart).count();
		}
		return false;
	}
	cached.kind = 1;
	cached.count = 3;
	cached.floats[0] = value[0];
	cached.floats[1] = value[1];
	cached.floats[2] = value[2];
	node.set_shader_input(InternalName::make(name), value);
	++m_shaderInputStats.setCount;
	if (m_measureShaderInputApplyTime)
	{
		m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - applyStart).count();
	}
	return true;
}

bool HwaSimIR::SetShaderInputCached(NodePath& node, const char* name, const LVecBase2i& value, bool force)
{
	if (node.is_empty() || name == nullptr)
	{
		return false;
	}
	const auto applyStart = m_measureShaderInputApplyTime ? std::chrono::high_resolution_clock::now() : std::chrono::high_resolution_clock::time_point();
	const std::uintptr_t key = ShaderInputCacheKey(node);
	ShaderInputCachedValue& cached = m_shaderInputCache[key][name];
	const bool same = cached.kind == 2 &&
		cached.count == 2 &&
		cached.ints[0] == value[0] &&
		cached.ints[1] == value[1];
	if (!force && same)
	{
		++m_shaderInputStats.skipCount;
		if (m_measureShaderInputApplyTime)
		{
			m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
				std::chrono::high_resolution_clock::now() - applyStart).count();
		}
		return false;
	}
	cached.kind = 2;
	cached.count = 2;
	cached.ints[0] = value[0];
	cached.ints[1] = value[1];
	node.set_shader_input(InternalName::make(name), value);
	++m_shaderInputStats.setCount;
	if (m_measureShaderInputApplyTime)
	{
		m_shaderInputStats.applyMs += std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - applyStart).count();
	}
	return true;
}

void HwaSimIR::InvalidateShaderInputCache(const NodePath& node)
{
	const std::uintptr_t key = ShaderInputCacheKey(node);
	if (key != 0)
	{
		m_shaderInputCache.erase(key);
	}
}

void HwaSimIR::ResetShaderInputCounters()
{
	m_shaderInputStats.setCount = 0;
	m_shaderInputStats.skipCount = 0;
	m_shaderInputStats.applyMs = 0.0;
}

HwaSimIR::ShaderInputCacheStats HwaSimIR::SnapshotShaderInputCounters() const
{
	return m_shaderInputStats;
}

void HwaSimIR::ApplyStage6DisplayShaderInputs(NodePath& node)
{
	if (node.is_empty())
	{
		return;
	}

	const IRSensorPostProcessConfig config = m_stage6DisplayConfigReady ? m_stage6DisplayConfig : IRSensorPostProcessConfig();
	const bool shaderDisplayEnabled = false;
	const double offsetNorm = config.displayOffset / 255.0;
	const double noiseSigmaNorm = config.noiseSigmaGray / 255.0;

	SetShaderInputCached(node, "u_stage6_display_en", LVecBase2i(shaderDisplayEnabled ? 1 : 0, 0));
	SetShaderInputCached(node, "u_stage6_white_hot", LVecBase2i(config.whiteHot ? 1 : 0, 0));
	SetShaderInputCached(node, "u_stage6_display_gain", LVecBase2f(static_cast<float>(config.displayGain), 0.0f));
	SetShaderInputCached(node, "u_stage6_display_offset", LVecBase2f(static_cast<float>(offsetNorm), 0.0f));
	SetShaderInputCached(node, "u_stage6_noise_enable", LVecBase2i(config.noiseEnable ? 1 : 0, 0));
	SetShaderInputCached(node, "u_stage6_noise_sigma_norm", LVecBase2f(static_cast<float>(noiseSigmaNorm), 0.0f));
	SetShaderInputCached(node, "u_stage6_background_display_en", LVecBase2i(config.backgroundDisplayEnable ? 1 : 0, 0));
}

void HwaSimIR::RefreshStage6DisplayShaderInputs()
{
	if (!m_skyNode.is_empty())
	{
		ApplyStage6DisplayShaderInputs(m_skyNode);
	}
	if (!m_stage7LowerShellNode.is_empty())
	{
		ApplyStage6DisplayShaderInputs(m_stage7LowerShellNode);
	}
	for (size_t i = 0; i < m_cloudNodes.size(); ++i)
	{
		ApplyStage6DisplayShaderInputs(m_cloudNodes[i]);
	}
	for (auto& pakPlat : m_pakPlatformList)
	{
		ApplyStage6DisplayShaderInputs(pakPlat.nodePath);
	}
	for (auto& weaponPlat : m_weaponPlatformList)
	{
		ApplyStage6DisplayShaderInputs(weaponPlat.nodePath);
	}
	for (auto& targetPlat : m_targetPlatformList)
	{
		ApplyStage6DisplayShaderInputs(targetPlat.nodePath);
	}
}

void HwaSimIR::InitStage6FinalPostShader()
{
	std::string vertexShader = R"(
    #version 100
    uniform mat4 p3d_ModelViewProjectionMatrix;
    attribute vec4 p3d_Vertex;
    attribute vec2 p3d_MultiTexCoord0;
    varying vec2 texcoord;

    void main() {
        gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;
        texcoord = p3d_MultiTexCoord0;
    }
    )";

	std::string fragmentShader = R"(
    #version 100
    precision mediump float;

    uniform sampler2D p3d_Texture0;
    uniform int u_stage6_final_white_hot;
    uniform float u_stage6_final_display_gain;
    uniform float u_stage6_final_display_offset;
    uniform int u_stage6_final_noise_enable;
    uniform float u_stage6_final_noise_sigma_norm;
    uniform vec2 u_stage6_final_uv_scale;
    uniform int u_stage6_mtf_blur_enable;
    uniform float u_stage6_mtf_sigma_pixels;
    uniform int u_stage6_mtf_radius_pixels;
    uniform vec2 u_stage6_mtf_texel_size;
    uniform vec2 u_stage6_mtf_uv_max;
    uniform int u_stage6_agc_enable;
    uniform float u_stage6_agc_gain;
    uniform float u_stage6_agc_offset;
    uniform int u_stage6_agc_mode;
    uniform int u_stage6_detector_noise_enable;
    uniform int u_stage6_detector_noise_position; // 1 before AGC, 2 after AGC
    uniform int u_stage6_temporal_noise_enable;
    uniform float u_stage6_temporal_noise_sigma_gray;
    uniform int u_stage6_fpn_enable;
    uniform float u_stage6_fpn_sigma_gray;
    uniform int u_stage6_column_noise_enable;
    uniform float u_stage6_column_noise_sigma_gray;
    uniform int u_stage6_row_noise_enable;
    uniform float u_stage6_row_noise_sigma_gray;
    uniform int u_stage6_bad_pixels_enable;
    uniform float u_stage6_bad_pixel_ratio;
    uniform float u_stage6_bad_pixel_hot_gray;
    uniform float u_stage6_bad_pixel_dead_gray;
    uniform float u_stage6_noise_clamp_min;
    uniform float u_stage6_noise_clamp_max;
    uniform float u_stage6_noise_seed;
    uniform float u_stage6_noise_frame_index;
    uniform int u_stage7_final_precipitation_mode; // 0 none, 1 screen overlay
    uniform int u_stage7_final_precipitation_type; // 0 none, 1 rain, 2 snow
    uniform float u_stage7_final_precipitation_density;
    uniform float u_stage7_final_precipitation_speed;
    uniform float u_stage7_final_precipitation_wind_dir_deg;
    uniform float u_stage7_final_time;
    uniform float u_stage7_final_sensor_fov_deg;

    varying vec2 texcoord;

    float Stage6FinalNoise(vec2 pixel)
    {
        vec2 p = fract(pixel * vec2(0.1031, 0.11369));
        p += dot(p, p.yx + vec2(19.19, 19.19));
        return fract((p.x + p.y) * p.x);
    }

    float Stage6FinalSampleDisplayGray(vec2 uv)
    {
        vec2 safeUv = clamp(uv, vec2(0.0, 0.0), u_stage6_mtf_uv_max);
        vec4 rawColor = texture2D(p3d_Texture0, safeUv);
        float gray = dot(rawColor.rgb, vec3(0.299, 0.587, 0.114));
        gray = gray * u_stage6_final_display_gain + u_stage6_final_display_offset;
        return clamp(gray, 0.0, 1.0);
    }

    float Stage6FinalMtfBlurGray(vec2 sampleUv)
    {
        int radius = u_stage6_mtf_radius_pixels;
        if (radius < 0) {
            radius = 0;
        }
        if (radius > 4) {
            radius = 4;
        }
        float sigma = max(u_stage6_mtf_sigma_pixels, 0.001);
        if (u_stage6_mtf_blur_enable != 1 || radius <= 0 || sigma <= 0.001) {
            return Stage6FinalSampleDisplayGray(sampleUv);
        }

        float sum = 0.0;
        float weightSum = 0.0;
        for (int y = -4; y <= 4; ++y) {
            for (int x = -4; x <= 4; ++x) {
                if (x >= -radius && x <= radius && y >= -radius && y <= radius) {
                    vec2 offset = vec2(float(x), float(y)) * u_stage6_mtf_texel_size;
                    float d2 = float(x * x + y * y);
                    float weight = exp(-0.5 * d2 / (sigma * sigma));
                    sum += Stage6FinalSampleDisplayGray(sampleUv + offset) * weight;
                    weightSum += weight;
                }
            }
        }
        if (weightSum <= 0.0) {
            return Stage6FinalSampleDisplayGray(sampleUv);
        }
        return clamp(sum / weightSum, 0.0, 1.0);
    }

    float Stage6FinalApplyAgc(float gray)
    {
        if (u_stage6_agc_enable != 1 || u_stage6_agc_mode == 0) {
            return gray;
        }
        return clamp(gray * u_stage6_agc_gain + u_stage6_agc_offset, 0.0, 1.0);
    }

    float Stage6Hash2(vec2 p)
    {
        vec2 q = fract(p * vec2(0.1031, 0.11369));
        q += dot(q, q.yx + vec2(19.19, 19.19));
        return fract((q.x + q.y) * q.x);
    }

    float Stage6Hash3(vec3 p)
    {
        vec3 q = fract(p * vec3(0.1031, 0.11369, 0.13787));
        q += dot(q, q.yzx + vec3(19.19, 19.19, 19.19));
        return fract((q.x + q.y) * q.z);
    }

    float Stage6SignedHash3(vec3 p)
    {
        return Stage6Hash3(p) * 2.0 - 1.0;
    }

    float Stage6FinalApplyDetectorNoise(float gray, vec2 pixel)
    {
        if (u_stage6_detector_noise_enable != 1) {
            return gray;
        }
        vec2 ip = floor(pixel);
        float noisy = gray;
        float seed = u_stage6_noise_seed;
        if (u_stage6_temporal_noise_enable == 1 && u_stage6_temporal_noise_sigma_gray > 0.0) {
            noisy += Stage6SignedHash3(vec3(ip + vec2(seed, seed * 0.37), u_stage6_noise_frame_index)) *
                u_stage6_temporal_noise_sigma_gray;
        }
        if (u_stage6_fpn_enable == 1 && u_stage6_fpn_sigma_gray > 0.0) {
            noisy += (Stage6Hash2(ip + vec2(seed * 1.31, seed * 0.73)) * 2.0 - 1.0) *
                u_stage6_fpn_sigma_gray;
        }
        if (u_stage6_column_noise_enable == 1 && u_stage6_column_noise_sigma_gray > 0.0) {
            noisy += (Stage6Hash2(vec2(ip.x + seed * 2.17, seed * 0.19)) * 2.0 - 1.0) *
                u_stage6_column_noise_sigma_gray;
        }
        if (u_stage6_row_noise_enable == 1 && u_stage6_row_noise_sigma_gray > 0.0) {
            noisy += (Stage6Hash2(vec2(seed * 0.29, ip.y + seed * 1.97)) * 2.0 - 1.0) *
                u_stage6_row_noise_sigma_gray;
        }
        if (u_stage6_bad_pixels_enable == 1 && u_stage6_bad_pixel_ratio > 0.0) {
            float bad = Stage6Hash2(ip + vec2(seed * 3.11, seed * 5.07));
            if (bad < u_stage6_bad_pixel_ratio) {
                float hot = Stage6Hash2(ip + vec2(seed * 7.13, seed * 11.17));
                noisy = hot > 0.5 ? u_stage6_bad_pixel_hot_gray : u_stage6_bad_pixel_dead_gray;
            }
        }
        return clamp(noisy, u_stage6_noise_clamp_min, u_stage6_noise_clamp_max);
    }

    float Stage7FinalHash(vec2 p)
    {
        return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
    }

    float Stage7RainOverlay(vec2 uv)
    {
        float density = clamp(u_stage7_final_precipitation_density, 0.0, 1.0);
        if (density <= 0.001) {
            return 0.0;
        }
        float wind = sin(u_stage7_final_precipitation_wind_dir_deg * 0.01745329252);
        vec2 grid = vec2(96.0, 72.0);
        vec2 p = uv * grid + vec2(wind * u_stage7_final_time * 0.35, u_stage7_final_time * max(0.1, u_stage7_final_precipitation_speed) * 1.35);
        vec2 cell = floor(p);
        vec2 local = fract(p);
        float rnd = Stage7FinalHash(cell);
        float spawn = step(1.0 - density * 0.42, rnd);
        float slant = 0.10 + wind * 0.22;
        float center = 0.5 + slant * (local.y - 0.5);
        float fovNorm = clamp(u_stage7_final_sensor_fov_deg / 9.0, 0.01, 1.0);
        float width = mix(0.026, 0.014, fovNorm);
        float line = 1.0 - smoothstep(width, width * 2.8, abs(local.x - center));
        float segment = 1.0 - smoothstep(0.18, 0.42, abs(local.y - 0.50));
        return clamp(line * segment * spawn, 0.0, 1.0);
    }

    float Stage7SnowOverlay(vec2 uv)
    {
        float density = clamp(u_stage7_final_precipitation_density, 0.0, 1.0);
        if (density <= 0.001) {
            return 0.0;
        }
        float wind = sin(u_stage7_final_precipitation_wind_dir_deg * 0.01745329252);
        vec2 grid = vec2(52.0, 48.0);
        vec2 p = uv * grid + vec2(wind * u_stage7_final_time * 0.12, u_stage7_final_time * max(0.08, u_stage7_final_precipitation_speed) * 0.28);
        vec2 cell = floor(p);
        vec2 local = fract(p);
        float rnd = Stage7FinalHash(cell);
        float spawn = step(1.0 - density * 0.34, rnd);
        float radius = 0.16 + rnd * 0.06;
        float dotMask = 1.0 - smoothstep(radius * 0.45, radius, length(local - vec2(0.5, 0.5)));
        return clamp(dotMask * spawn, 0.0, 1.0);
    }

    float ApplyStage7FinalPrecipitationOverlay(float gray, vec2 uv)
    {
        if (u_stage7_final_precipitation_mode != 1 || u_stage7_final_precipitation_type == 0) {
            return gray;
        }
        float density = clamp(u_stage7_final_precipitation_density, 0.0, 1.0);
        float mask = 0.0;
        float strength = 0.0;
        if (u_stage7_final_precipitation_type == 1) {
            mask = Stage7RainOverlay(uv);
            strength = 0.10 * density;
        } else if (u_stage7_final_precipitation_type == 2) {
            mask = Stage7SnowOverlay(uv);
            strength = 0.08 * density;
        }
        return clamp(gray + mask * strength, 0.0, 1.0);
    }

    void main() {
        vec2 sampleUv = texcoord * u_stage6_final_uv_scale;
        vec4 rawColor = texture2D(p3d_Texture0, sampleUv);
        float gray = Stage6FinalMtfBlurGray(sampleUv);
        if (u_stage6_final_noise_enable == 1 && u_stage6_final_noise_sigma_norm > 0.0) {
            gray += (Stage6FinalNoise(gl_FragCoord.xy) * 2.0 - 1.0) * u_stage6_final_noise_sigma_norm;
        }
        gray = clamp(gray, 0.0, 1.0);
        if (u_stage6_detector_noise_position == 1) {
            gray = Stage6FinalApplyDetectorNoise(gray, gl_FragCoord.xy);
        }
        gray = Stage6FinalApplyAgc(gray);
        if (u_stage6_detector_noise_position == 2) {
            gray = Stage6FinalApplyDetectorNoise(gray, gl_FragCoord.xy);
        }
        if (u_stage6_final_white_hot == 0) {
            gray = 1.0 - gray;
        }
        gray = ApplyStage7FinalPrecipitationOverlay(gray, texcoord);
        gl_FragColor = vec4(gray, gray, gray, rawColor.a);
    }
    )";

	m_stage6FinalPostShader = Shader::make(Shader::SL_GLSL, vertexShader, fragmentShader);
	if (!m_stage6FinalPostShader)
	{
		std::cerr << "Stage6 final sensor postprocess shader compile failed." << std::endl;
	}
}

void HwaSimIR::SetupStage6FinalPipeline(int width, int height, const char* reason)
{
	if (IsHeadlessOffscreenMode())
	{
		m_stage6FinalPipelineReady = false;
		std::cout << "[Stage6 FinalPipeline][TODO]"
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< " renderMode=" << m_renderPresentationModeName
			<< " windowPreview=" << (m_renderWindowPreview ? "1" : "0")
			<< " tcpSource=final_sensor"
			<< " finalSensorTex=Stage6FinalSensorTex"
			<< " headless_final_pipeline_not_ready=1"
			<< " stage=H1"
			<< std::endl;
		return;
	}

	if (!m_pMainWindow || !m_pGraphicsWindow || m_cameraNode.is_empty())
	{
		std::cout << "[Stage6 FinalPipeline][WARN]"
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< " renderMode=" << m_renderPresentationModeName
			<< " windowPreview=" << (m_renderWindowPreview ? "1" : "0")
			<< " tcpSource=final_sensor"
			<< " finalSensorTex=Stage6FinalSensorTex"
			<< " failure=visible_window_pipeline_unavailable"
			<< std::endl;
		return;
	}
	if (!m_stage6FinalPostShader)
	{
		InitStage6FinalPostShader();
	}
	if (m_stage6FinalRoot.is_empty())
	{
		m_stage6FinalRoot = NodePath("Stage6FinalSensorRoot");
	}
	if (m_stage6FinalCameraNode.is_empty())
	{
		PT(Camera) finalCamera = new Camera("Stage6FinalSensorCamera");
		PT(OrthographicLens) finalLens = new OrthographicLens();
		finalLens->set_film_size(2.0f, 2.0f);
		finalLens->set_near_far(-10.0f, 10.0f);
		finalCamera->set_lens(finalLens);
		m_stage6FinalCameraNode = m_stage6FinalRoot.attach_new_node(finalCamera);
		m_stage6FinalCameraNode.set_pos(0.0f, -1.0f, 0.0f);
		m_stage6FinalCameraNode.set_hpr(0.0f, 0.0f, 0.0f);
	}

	const int safeWidth = std::max(1, width);
	const int safeHeight = std::max(1, height);
	const bool sameSize = m_stage6FinalPipelineReady &&
		m_stage6FinalWidth == safeWidth &&
		m_stage6FinalHeight == safeHeight &&
		m_stage6RawSceneBuffer != nullptr &&
		m_stage6FinalRegion != nullptr &&
		!m_stage6FinalCard.is_empty();
	if (sameSize)
	{
		if (m_stage6RawSceneRegion != nullptr)
		{
			m_stage6RawSceneRegion->set_dimensions(0.0f, 1.0f, 0.0f, 1.0f);
			m_stage6RawSceneRegion->set_active(true);
		}
		if (m_stage6FinalRegion != nullptr)
		{
			m_stage6FinalRegion->set_dimensions(0.0f, 1.0f, 0.0f, 1.0f);
			m_stage6FinalRegion->set_camera(m_stage6FinalCameraNode);
			m_stage6FinalRegion->set_active(true);
		}
		DisplayRegion* sourceRegion = m_pMainWindow->get_display_region_3d();
		if (sourceRegion != nullptr)
		{
			sourceRegion->set_active(false);
		}
		ApplyStage6FinalPostprocessInputs();
		LogStage6FinalPipeline(reason);
		SetupAnnotationOverlayRegion(reason);
		return;
	}

	if (m_stage6RawSceneBuffer != nullptr)
	{
		GraphicsEngine::get_global_ptr()->remove_window(m_stage6RawSceneBuffer);
		m_stage6RawSceneBuffer = nullptr;
		m_stage6RawSceneRegion = nullptr;
	}
	if (!m_stage6FinalCard.is_empty())
	{
		m_stage6FinalCard.remove_node();
	}

	m_stage6RawSceneTex = new Texture("Stage6RawSceneTex");
	m_stage6RawSceneTex->setup_2d_texture(safeWidth, safeHeight, Texture::T_unsigned_byte, Texture::F_rgb);
	m_stage6RawSceneBuffer = m_pGraphicsWindow->make_texture_buffer("Stage6RawSceneBuffer", safeWidth, safeHeight, m_stage6RawSceneTex, false);
	if (m_stage6RawSceneBuffer == nullptr)
	{
		m_stage6FinalPipelineReady = false;
		std::cout << "[Stage6 FinalPipeline][WARN]"
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< " rawSceneTex=Stage6RawSceneTex"
			<< " finalSensorTex=Stage6FinalSensorTex"
			<< " windowSource=raw_scene"
			<< " tcpSource=final_sensor"
			<< " sameOutput=0"
			<< " failure=raw_buffer_unavailable"
			<< std::endl;
		return;
	}

	m_stage6RawSceneBuffer->remove_all_display_regions();
	m_stage6RawSceneBuffer->set_sort(m_pGraphicsWindow->get_sort() - 10);
	m_stage6RawSceneRegion = m_stage6RawSceneBuffer->make_display_region(0.0f, 1.0f, 0.0f, 1.0f);
	DisplayRegion* sourceRegion = m_pMainWindow->get_display_region_3d();
	if (sourceRegion != nullptr)
	{
		m_stage6RawSceneRegion->set_camera(sourceRegion->get_camera());
		sourceRegion->set_active(false);
	}
	m_stage6RawSceneRegion->set_dimensions(0.0f, 1.0f, 0.0f, 1.0f);
	m_stage6RawSceneRegion->set_sort(0);
	m_stage6RawSceneRegion->set_clear_color_active(true);
	m_stage6RawSceneRegion->set_clear_color(LColor(0.0f, 0.0f, 0.0f, 1.0f));
	m_stage6RawSceneRegion->set_clear_depth_active(true);
	m_stage6RawSceneRegion->set_active(true);

	if (m_stage6FinalRegion == nullptr)
	{
		m_stage6FinalRegion = m_pGraphicsWindow->make_display_region(0.0f, 1.0f, 0.0f, 1.0f);
	}
	m_stage6FinalRegion->set_dimensions(0.0f, 1.0f, 0.0f, 1.0f);
	m_stage6FinalRegion->set_camera(m_stage6FinalCameraNode);
	m_stage6FinalRegion->set_sort(100);
	m_stage6FinalRegion->set_clear_color_active(true);
	m_stage6FinalRegion->set_clear_color(LColor(0.0f, 0.0f, 0.0f, 1.0f));
	m_stage6FinalRegion->set_clear_depth_active(true);
	m_stage6FinalRegion->set_active(true);

	CardMaker finalCardMaker("Stage6_FinalSensor_Card");
	finalCardMaker.set_frame(-1.0f, 1.0f, -1.0f, 1.0f);
	m_stage6FinalCard = m_stage6FinalRoot.attach_new_node(finalCardMaker.generate());
	m_stage6FinalCard.set_texture(m_stage6RawSceneTex);
	if (m_stage6FinalPostShader)
	{
		m_stage6FinalCard.set_shader(m_stage6FinalPostShader, 1);
	}
	m_stage6FinalCard.set_depth_write(false);
	m_stage6FinalCard.set_depth_test(false);
	m_stage6FinalCard.set_bin("fixed", 0);

	if (m_renderTex != nullptr)
	{
		m_renderTex->setup_2d_texture(safeWidth, safeHeight, Texture::T_unsigned_byte, Texture::F_rgb);
	}

	m_stage6FinalWidth = safeWidth;
	m_stage6FinalHeight = safeHeight;
	m_stage6FinalPipelineReady = true;
	ApplyStage6FinalPostprocessInputs();
	LogStage6FinalPipeline(reason);
	SetupAnnotationOverlayRegion(reason);
}

void HwaSimIR::SetupAnnotationOverlayRegion(const char* reason)
{
	if (!m_pGraphicsWindow)
	{
		return;
	}

	if (m_annotationRoot.is_empty())
	{
		m_annotationRoot = NodePath("AnnotationFinalOverlayRoot");
	}
	if (m_annotationCameraNode.is_empty())
	{
		PT(Camera) annotationCamera = new Camera("AnnotationFinalOverlayCamera");
		PT(OrthographicLens) annotationLens = new OrthographicLens();
		annotationLens->set_film_size(2.0f, 2.0f);
		annotationLens->set_near_far(-10.0f, 10.0f);
		annotationCamera->set_lens(annotationLens);
		m_annotationCameraNode = m_annotationRoot.attach_new_node(annotationCamera);
		m_annotationCameraNode.set_pos(0.0f, -1.0f, 0.0f);
		m_annotationCameraNode.set_hpr(0.0f, 0.0f, 0.0f);
	}

	if (m_annotationRegion == nullptr)
	{
		m_annotationRegion = m_pGraphicsWindow->make_display_region(0.0f, 1.0f, 0.0f, 1.0f);
	}

	const int overlaySort = 200;
	m_annotationRegion->set_dimensions(0.0f, 1.0f, 0.0f, 1.0f);
	m_annotationRegion->set_camera(m_annotationCameraNode);
	m_annotationRegion->set_sort(overlaySort);
	m_annotationRegion->set_clear_color_active(false);
	m_annotationRegion->set_clear_depth_active(false);
	m_annotationRegion->set_active(true);

	// Stage1.1 标注必须画在 Stage6 final sensor card 之后，但不进入灰度/噪声/天气后处理。
	m_annotationManager.initialize(m_annotationRoot);

	++m_annotationOverlayLogCounter;
	if (m_annotationOverlayLogCounter <= 3 || (m_annotationOverlayLogCounter % 120) == 0)
	{
		std::cout << "[AnnotationOverlay]"
			<< " mode=final_region"
			<< " regionReady=" << (m_annotationRegion != nullptr ? "1" : "0")
			<< " rootEmpty=" << (m_annotationRoot.is_empty() ? "1" : "0")
			<< " sort=" << overlaySort
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< std::endl;
	}
}

void HwaSimIR::ApplyStage6FinalPostprocessInputs()
{
	if (m_stage6FinalCard.is_empty())
	{
		return;
	}
	const IRSensorPostProcessConfig config = m_stage6DisplayConfigReady ? m_stage6DisplayConfig : IRSensorPostProcessConfig();
	const double offsetNorm = config.displayOffset / 255.0;
	const double noiseSigmaNorm = config.noiseSigmaGray / 255.0;
	const int rawW = m_stage6RawSceneTex != nullptr ? m_stage6RawSceneTex->get_x_size() : 0;
	const int rawH = m_stage6RawSceneTex != nullptr ? m_stage6RawSceneTex->get_y_size() : 0;
	const float uvScaleU = rawW > 0 ? std::min(1.0f, static_cast<float>(m_stage6FinalWidth) / static_cast<float>(rawW)) : 1.0f;
	const float uvScaleV = rawH > 0 ? std::min(1.0f, static_cast<float>(m_stage6FinalHeight) / static_cast<float>(rawH)) : 1.0f;
	const float texelU = rawW > 0 ? 1.0f / static_cast<float>(rawW) : 0.0f;
	const float texelV = rawH > 0 ? 1.0f / static_cast<float>(rawH) : 0.0f;
	const bool mtfBlurEffective =
		m_stage6MtfBlurEnabled &&
		m_stage6MtfApplyTo == "final_display" &&
		m_stage6MtfBlurRadiusPixels > 0 &&
		m_stage6MtfBlurSigmaPixels > 0.001;
	const bool agcEffective =
		m_stage6AgcEnabled &&
		m_stage6AgcApplyTo == "final_display" &&
		m_stage6AgcMode != "Off";
	const bool detectorNoiseHasSource =
		(m_stage6TemporalNoiseEnabled && m_stage6TemporalNoiseSigmaGray > 0.0) ||
		(m_stage6FpnEnabled && m_stage6FpnSigmaGray > 0.0) ||
		(m_stage6ColumnNoiseEnabled && m_stage6ColumnNoiseSigmaGray > 0.0) ||
		(m_stage6RowNoiseEnabled && m_stage6RowNoiseSigmaGray > 0.0) ||
		(m_stage6BadPixelsEnabled && m_stage6BadPixelRatio > 0.0);
	const bool detectorNoiseEffective =
		m_stage6DetectorNoiseEnabled &&
		m_stage6DetectorNoiseApplyTo == "final_display" &&
		detectorNoiseHasSource;
	const int precipitationType = IRWeatherEffects::precipitationCode(m_stage7WeatherState.precipitationType);
	const bool screenOverlayActive = m_stage7WeatherEnabled &&
		m_stage7PrecipitationEnabled &&
		m_stage7PrecipitationMode == 1 &&
		precipitationType != 0 &&
		m_stage7WeatherState.precipitationDensity > 0.001;
	const double sensorFovDeg = std::max(m_sensorDisplayConfig.horizontalFovDeg, m_sensorDisplayConfig.verticalFovDeg);
	const double safeSensorFovDeg = std::isfinite(sensorFovDeg) && sensorFovDeg > 0.0 ? sensorFovDeg : 1.0;
	const double currentTime = ClockObject::get_global_clock() != nullptr ? ClockObject::get_global_clock()->get_frame_time() : 0.0;
	m_stage6FinalCard.set_shader_input("u_stage6_final_white_hot", LVecBase2i(config.whiteHot ? 1 : 0, 0));
	m_stage6FinalCard.set_shader_input("u_stage6_final_display_gain", LVecBase2f(static_cast<float>(config.displayGain), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage6_final_display_offset", LVecBase2f(static_cast<float>(offsetNorm), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage6_final_noise_enable", LVecBase2i(config.noiseEnable ? 1 : 0, 0));
	m_stage6FinalCard.set_shader_input("u_stage6_final_noise_sigma_norm", LVecBase2f(static_cast<float>(noiseSigmaNorm), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage6_final_uv_scale", LVecBase2f(uvScaleU, uvScaleV));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_mtf_blur_enable", LVecBase2i(mtfBlurEffective ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_mtf_sigma_pixels", LVecBase2f(static_cast<float>(m_stage6MtfBlurSigmaPixels), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_mtf_radius_pixels", LVecBase2i(m_stage6MtfBlurRadiusPixels, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_mtf_texel_size", LVecBase2f(texelU, texelV));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_mtf_uv_max", LVecBase2f(uvScaleU, uvScaleV));
	const auto agcApplyBegin = std::chrono::steady_clock::now();
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_agc_enable", LVecBase2i(agcEffective ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_agc_gain", LVecBase2f(static_cast<float>(agcEffective ? m_stage6AgcGain : 1.0), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_agc_offset", LVecBase2f(static_cast<float>(agcEffective ? m_stage6AgcOffset : 0.0), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_agc_mode", LVecBase2i(agcEffective ? m_stage6AgcModeCode : 0, 0));
	m_stage6AgcApplyMsCurrent = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - agcApplyBegin).count();
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_detector_noise_enable", LVecBase2i(detectorNoiseEffective ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_detector_noise_position", LVecBase2i(detectorNoiseEffective ? m_stage6DetectorNoisePositionCode : 1, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_temporal_noise_enable", LVecBase2i((detectorNoiseEffective && m_stage6TemporalNoiseEnabled) ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_temporal_noise_sigma_gray", LVecBase2f(static_cast<float>(m_stage6TemporalNoiseSigmaGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_fpn_enable", LVecBase2i((detectorNoiseEffective && m_stage6FpnEnabled) ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_fpn_sigma_gray", LVecBase2f(static_cast<float>(m_stage6FpnSigmaGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_column_noise_enable", LVecBase2i((detectorNoiseEffective && m_stage6ColumnNoiseEnabled) ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_column_noise_sigma_gray", LVecBase2f(static_cast<float>(m_stage6ColumnNoiseSigmaGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_row_noise_enable", LVecBase2i((detectorNoiseEffective && m_stage6RowNoiseEnabled) ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_row_noise_sigma_gray", LVecBase2f(static_cast<float>(m_stage6RowNoiseSigmaGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_bad_pixels_enable", LVecBase2i((detectorNoiseEffective && m_stage6BadPixelsEnabled) ? 1 : 0, 0));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_bad_pixel_ratio", LVecBase2f(static_cast<float>(m_stage6BadPixelRatio), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_bad_pixel_hot_gray", LVecBase2f(static_cast<float>(m_stage6BadPixelHotGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_bad_pixel_dead_gray", LVecBase2f(static_cast<float>(m_stage6BadPixelDeadGray), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_noise_clamp_min", LVecBase2f(static_cast<float>(m_stage6NoiseClampMin), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_noise_clamp_max", LVecBase2f(static_cast<float>(m_stage6NoiseClampMax), 0.0f));
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_noise_seed", LVecBase2f(static_cast<float>(m_stage6FpnSeed), 0.0f));
	const float noiseFrameIndex = static_cast<float>(m_currentFrameTelemetry.sourceSeq % 100000ULL);
	SetShaderInputCached(m_stage6FinalCard, "u_stage6_noise_frame_index", LVecBase2f(noiseFrameIndex, 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage7_final_precipitation_mode", LVecBase2i(screenOverlayActive ? 1 : 0, 0));
	m_stage6FinalCard.set_shader_input("u_stage7_final_precipitation_type", LVecBase2i(screenOverlayActive ? precipitationType : 0, 0));
	m_stage6FinalCard.set_shader_input("u_stage7_final_precipitation_density", LVecBase2f(static_cast<float>(screenOverlayActive ? m_stage7WeatherState.precipitationDensity : 0.0), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage7_final_precipitation_speed", LVecBase2f(static_cast<float>(m_stage7WeatherState.precipitationSpeed), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage7_final_precipitation_wind_dir_deg", LVecBase2f(static_cast<float>(m_stage7WeatherState.windDir), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage7_final_time", LVecBase2f(static_cast<float>(currentTime), 0.0f));
	m_stage6FinalCard.set_shader_input("u_stage7_final_sensor_fov_deg", LVecBase2f(static_cast<float>(safeSensorFovDeg), 0.0f));
}

void HwaSimIR::LogStage6FinalPipeline(const char* reason)
{
	if (!m_stage6FinalPipelineReady)
	{
		return;
	}
	++m_stage6FinalPipelineLogCounter;
	if (m_stage6FinalPipelineLogCounter > 3 && (m_stage6FinalPipelineLogCounter % 120) != 0)
	{
		return;
	}
	std::cout << "[Stage6 FinalPipeline]"
		<< " reason=" << (reason != nullptr ? reason : "unknown")
		<< " renderMode=" << m_renderPresentationModeName
		<< " windowPreview=" << (m_renderWindowPreview ? "1" : "0")
		<< " rawSceneTex=Stage6RawSceneTex"
		<< " finalSensorTex=Stage6FinalSensorTex"
		<< " rawSceneSize=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
		<< " finalSensorSize=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
		<< " windowSource=final_sensor"
		<< " tcpSource=final_sensor"
		<< " windowRegion=fullscreen"
		<< " mtfBlurEnabled=" << (m_stage6MtfBlurEnabled ? "1" : "0")
		<< " mtfBlurMode=" << m_stage6MtfBlurMode
		<< " mtfBlurEffectiveMode=" << m_stage6MtfBlurEffectiveMode
		<< " mtfBlurSigmaPixels=" << m_stage6MtfBlurSigmaPixels
		<< " mtfBlurRadiusPixels=" << m_stage6MtfBlurRadiusPixels
		<< " agcEnabled=" << (m_stage6AgcEnabled ? "1" : "0")
		<< " agcMode=" << m_stage6AgcMode
		<< " agcStatsSource=" << m_stage6AgcStatsSource
		<< " agcGain=" << m_stage6AgcGain
		<< " agcOffset=" << m_stage6AgcOffset
		<< " detectorNoiseEnabled=" << (m_stage6DetectorNoiseEnabled ? "1" : "0")
		<< " detectorNoisePosition=" << m_stage6DetectorNoisePosition
		<< " temporalNoiseSigmaGray=" << m_stage6TemporalNoiseSigmaGray
		<< " fpnSigmaGray=" << m_stage6FpnSigmaGray
		<< " sameOutput=1"
		<< std::endl;
	LogStage6ViewportDiag(reason);
}

void HwaSimIR::LogStage6MtfBlur(std::uint64_t sourceSeq, double renderMs)
{
	std::ostringstream state;
	state << (m_stage6MtfBlurEnabled ? 1 : 0)
		<< ":" << m_stage6MtfBlurMode
		<< ":" << m_stage6MtfBlurEffectiveMode
		<< ":" << m_stage6MtfBlurSigmaPixels
		<< ":" << m_stage6MtfBlurRadiusPixels
		<< ":" << m_stage6MtfBlurPasses
		<< ":" << m_stage6MtfApplyTo
		<< ":" << m_stage6FinalWidth
		<< "x" << m_stage6FinalHeight;
	const std::string stateKey = state.str();
	const bool stateChanged = stateKey != m_lastStage6MtfLogState;
	const bool sampleDue =
		sourceSeq <= 3 ||
		(m_stage6MtfLogEveryFrames > 0 && sourceSeq > 0 &&
			(sourceSeq % static_cast<std::uint64_t>(m_stage6MtfLogEveryFrames)) == 0);
	if (!m_stage6MtfDebugLog && !m_enableIRVerboseLog && !stateChanged && !sampleDue)
	{
		return;
	}
	m_lastStage6MtfLogState = stateKey;
	++m_stage6MtfLogCounter;
	const bool mtfEffective =
		m_stage6MtfBlurEnabled &&
		m_stage6MtfApplyTo == "final_display" &&
		m_stage6MtfBlurRadiusPixels > 0 &&
		m_stage6MtfBlurSigmaPixels > 0.001;
	std::cout << "[Stage6 MTF]"
		<< " sourceSeq=" << sourceSeq
		<< " enabled=" << (m_stage6MtfBlurEnabled ? "1" : "0")
		<< " effective=" << (mtfEffective ? "1" : "0")
		<< " mode=" << m_stage6MtfBlurMode
		<< " effectiveMode=" << m_stage6MtfBlurEffectiveMode
		<< " sigmaPixels=" << m_stage6MtfBlurSigmaPixels
		<< " radiusPixels=" << m_stage6MtfBlurRadiusPixels
		<< " passes=" << m_stage6MtfBlurPasses
		<< " applyTo=" << m_stage6MtfApplyTo
		<< " inputSize=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
		<< " outputSize=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
		<< " stage6MtfBlurMs=" << (mtfEffective ? std::max(0.0, renderMs) : 0.0)
		<< " stage6MtfBlurScope=render_pass_upper_bound"
		<< " implementation=single_pass_gpu_separable_weights"
		<< " todo=true_two_pass_separable_if_needed"
		<< std::endl;
}

void HwaSimIR::LogStage6DetectorNoise(std::uint64_t sourceSeq, double renderMs)
{
	std::ostringstream state;
	state << (m_stage6DetectorNoiseEnabled ? 1 : 0)
		<< ":" << m_stage6DetectorNoiseApplyTo
		<< ":" << m_stage6DetectorNoisePosition
		<< ":" << (m_stage6TemporalNoiseEnabled ? 1 : 0)
		<< ":" << m_stage6TemporalNoiseSigmaGray
		<< ":" << (m_stage6FpnEnabled ? 1 : 0)
		<< ":" << m_stage6FpnSigmaGray
		<< ":" << (m_stage6ColumnNoiseEnabled ? 1 : 0)
		<< ":" << m_stage6ColumnNoiseSigmaGray
		<< ":" << (m_stage6RowNoiseEnabled ? 1 : 0)
		<< ":" << m_stage6RowNoiseSigmaGray
		<< ":" << (m_stage6BadPixelsEnabled ? 1 : 0)
		<< ":" << m_stage6BadPixelRatio
		<< ":" << m_stage6FpnSeed;
	const std::string stateKey = state.str();
	const bool stateChanged = stateKey != m_lastStage6NoiseLogState;
	const bool sampleDue =
		sourceSeq <= 3 ||
		(m_stage6NoiseLogEveryFrames > 0 && sourceSeq > 0 &&
			(sourceSeq % static_cast<std::uint64_t>(m_stage6NoiseLogEveryFrames)) == 0);
	if (!m_stage6NoiseDebugLog && !m_enableIRVerboseLog && !stateChanged && !sampleDue)
	{
		return;
	}
	m_lastStage6NoiseLogState = stateKey;
	++m_stage6NoiseLogCounter;
	const bool detectorNoiseHasSource =
		(m_stage6TemporalNoiseEnabled && m_stage6TemporalNoiseSigmaGray > 0.0) ||
		(m_stage6FpnEnabled && m_stage6FpnSigmaGray > 0.0) ||
		(m_stage6ColumnNoiseEnabled && m_stage6ColumnNoiseSigmaGray > 0.0) ||
		(m_stage6RowNoiseEnabled && m_stage6RowNoiseSigmaGray > 0.0) ||
		(m_stage6BadPixelsEnabled && m_stage6BadPixelRatio > 0.0);
	const bool detectorNoiseEffective =
		m_stage6DetectorNoiseEnabled &&
		m_stage6DetectorNoiseApplyTo == "final_display" &&
		detectorNoiseHasSource;
	std::cout << "[Stage6 Noise]"
		<< " sourceSeq=" << sourceSeq
		<< " enabled=" << (m_stage6DetectorNoiseEnabled ? "1" : "0")
		<< " effective=" << (detectorNoiseEffective ? "1" : "0")
		<< " applyTo=" << m_stage6DetectorNoiseApplyTo
		<< " position=" << m_stage6DetectorNoisePosition
		<< " temporalEnabled=" << (m_stage6TemporalNoiseEnabled ? "1" : "0")
		<< " temporalSigma=" << m_stage6TemporalNoiseSigmaGray
		<< " fpnEnabled=" << (m_stage6FpnEnabled ? "1" : "0")
		<< " fpnSigma=" << m_stage6FpnSigmaGray
		<< " columnEnabled=" << (m_stage6ColumnNoiseEnabled ? "1" : "0")
		<< " columnSigma=" << m_stage6ColumnNoiseSigmaGray
		<< " rowEnabled=" << (m_stage6RowNoiseEnabled ? "1" : "0")
		<< " rowSigma=" << m_stage6RowNoiseSigmaGray
		<< " badPixelEnabled=" << (m_stage6BadPixelsEnabled ? "1" : "0")
		<< " badPixelRatio=" << m_stage6BadPixelRatio
		<< " seed=" << m_stage6FpnSeed
		<< " stage6DetectorNoiseMs=" << (detectorNoiseEffective ? std::max(0.0, renderMs) : 0.0)
		<< " stage6DetectorNoiseScope=render_pass_upper_bound"
		<< " implementation=single_pass_gpu_hash_noise"
		<< std::endl;
}

void HwaSimIR::UpdateStage6AgcFromFrame(const unsigned char* frameData, int frameWidth, int frameHeight, std::uint64_t sourceSeq)
{
	const bool agcEffective =
		m_stage6AgcEnabled &&
		m_stage6AgcApplyTo == "final_display" &&
		m_stage6AgcMode != "Off";
	if (!agcEffective || frameData == nullptr || frameWidth <= 0 || frameHeight <= 0)
	{
		m_stage6AgcStatsMsCurrent = 0.0;
		if (!agcEffective)
		{
			m_stage6AgcGain = 1.0;
			m_stage6AgcOffset = 0.0;
			m_stage6AgcTargetGain = 1.0;
			m_stage6AgcTargetOffset = 0.0;
			m_stage6AgcLowInput = 0.0;
			m_stage6AgcHighInput = 1.0;
			m_stage6AgcSampleCount = 0;
			m_stage6AgcValid = false;
			m_stage6AgcFallbackReason = "disabled";
		}
		else
		{
			m_stage6AgcValid = false;
			m_stage6AgcFallbackReason = "invalid_frame";
		}
		m_perfStats.recordStage6Agc(
			0.0,
			m_stage6AgcApplyMsCurrent,
			agcEffective,
			m_stage6AgcMode.c_str(),
			m_stage6AgcGain,
			m_stage6AgcOffset,
			m_stage6AgcLowInput,
			m_stage6AgcHighInput,
			m_stage6AgcSampleCount);
		return;
	}

	const std::int64_t nowNs = IRPerfStats::steadyTimeNs();
	const double minUpdateIntervalSec = m_stage6AgcUpdateHz > 0.0 ? (1.0 / m_stage6AgcUpdateHz) : 0.0;
	const bool updateDue =
		!m_stage6AgcInitialized ||
		m_stage6AgcLastUpdateNs == 0 ||
		minUpdateIntervalSec <= 0.0 ||
		(static_cast<double>(nowNs - m_stage6AgcLastUpdateNs) / 1.0e9) >= minUpdateIntervalSec;
	if (!updateDue)
	{
		m_stage6AgcStatsMsCurrent = 0.0;
		m_perfStats.recordStage6Agc(
			0.0,
			m_stage6AgcApplyMsCurrent,
			true,
			m_stage6AgcMode.c_str(),
			m_stage6AgcGain,
			m_stage6AgcOffset,
			m_stage6AgcLowInput,
			m_stage6AgcHighInput,
			m_stage6AgcSampleCount);
		return;
	}

	const auto statsBegin = std::chrono::steady_clock::now();
	const IRSensorPostProcessConfig config = m_stage6DisplayConfigReady ? m_stage6DisplayConfig : IRSensorPostProcessConfig();
	double targetGain = m_stage6AgcGain;
	double targetOffset = m_stage6AgcOffset;
	double lowInput = m_stage6AgcLowInput;
	double highInput = m_stage6AgcHighInput;
	int sampleCount = 0;
	bool valid = false;
	std::string fallbackReason = "none";

	if (m_stage6AgcMode == "Manual")
	{
		targetGain = ClampStage5Double(config.displayGain, m_stage6AgcMinGain, m_stage6AgcMaxGain);
		targetOffset = ClampStage5Double(config.displayOffset / 255.0, m_stage6AgcMinOffset, m_stage6AgcMaxOffset);
		lowInput = 0.0;
		highInput = 1.0;
		valid = true;
		fallbackReason = "manual_display_gain_offset";
	}
	else
	{
		std::array<int, 256> histogram;
		histogram.fill(0);
		double sum = 0.0;
		double sumSq = 0.0;
		const int stride = std::max(1, m_stage6AgcStride);
		const double inverseGain = std::fabs(m_stage6AgcGain) > 1.0e-6 ? (1.0 / m_stage6AgcGain) : 1.0;
		for (int y = 0; y < frameHeight; y += stride)
		{
			const unsigned char* row = frameData + static_cast<size_t>(y) * static_cast<size_t>(frameWidth) * 3u;
			for (int x = 0; x < frameWidth; x += stride)
			{
				const unsigned char* px = row + static_cast<size_t>(x) * 3u;
				const int r = px[0];
				const int g = px[1];
				const int b = px[2];
				if (m_stage6AgcExcludeAnnotationOverlay &&
					g > 150 && r < 120 && b < 120 && g > r + 45 && g > b + 45)
				{
					continue;
				}
				double gray = (0.299 * static_cast<double>(r) +
					0.587 * static_cast<double>(g) +
					0.114 * static_cast<double>(b)) / 255.0;
				if (!config.whiteHot)
				{
					gray = 1.0 - gray;
				}
				gray = ClampStage5Double((gray - m_stage6AgcOffset) * inverseGain, 0.0, 1.0);
				const int bin = std::max(0, std::min(255, static_cast<int>(gray * 255.0 + 0.5)));
				++histogram[bin];
				sum += gray;
				sumSq += gray * gray;
				++sampleCount;
			}
		}

		if (sampleCount < 16)
		{
			fallbackReason = "insufficient_samples";
		}
		else if (m_stage6AgcMode == "MeanStd")
		{
			const double mean = sum / static_cast<double>(sampleCount);
			const double variance = std::max(0.0, sumSq / static_cast<double>(sampleCount) - mean * mean);
			const double stddev = std::sqrt(variance);
			lowInput = ClampStage5Double(mean - m_stage6AgcMeanStdK * stddev, 0.0, 1.0);
			highInput = ClampStage5Double(mean + m_stage6AgcMeanStdK * stddev, 0.0, 1.0);
			valid = highInput > lowInput + 1.0e-5;
			if (!valid)
			{
				fallbackReason = "mean_std_flat_range";
			}
		}
		else
		{
			auto percentileValue = [&](double percentile) -> double {
				const double p = ClampStage5Double(percentile, 0.0, 100.0);
				const int targetIndex = std::max(0, static_cast<int>(
					std::floor((p / 100.0) * static_cast<double>(sampleCount - 1) + 0.5)));
				int cumulative = 0;
				for (int i = 0; i < 256; ++i)
				{
					cumulative += histogram[i];
					if (cumulative > targetIndex)
					{
						return static_cast<double>(i) / 255.0;
					}
				}
				return 1.0;
			};
			lowInput = percentileValue(m_stage6AgcLowPercentile);
			highInput = percentileValue(m_stage6AgcHighPercentile);
			valid = highInput > lowInput + 1.0e-5;
			if (!valid)
			{
				fallbackReason = "percentile_flat_range";
			}
		}

		if (valid)
		{
			targetGain = (m_stage6AgcTargetHighGray - m_stage6AgcTargetLowGray) / std::max(1.0e-5, highInput - lowInput);
			targetOffset = m_stage6AgcTargetLowGray - targetGain * lowInput;
			targetGain = ClampStage5Double(targetGain, m_stage6AgcMinGain, m_stage6AgcMaxGain);
			targetOffset = ClampStage5Double(targetOffset, m_stage6AgcMinOffset, m_stage6AgcMaxOffset);
			fallbackReason = "none";
		}
		else
		{
			targetGain = m_stage6AgcInitialized ? m_stage6AgcGain : 1.0;
			targetOffset = m_stage6AgcInitialized ? m_stage6AgcOffset : 0.0;
			lowInput = m_stage6AgcInitialized ? m_stage6AgcLowInput : 0.0;
			highInput = m_stage6AgcInitialized ? m_stage6AgcHighInput : 1.0;
		}
	}

	const double alpha = ClampStage5Double(m_stage6AgcSmoothingAlpha, 0.0, 1.0);
	if (!m_stage6AgcInitialized)
	{
		m_stage6AgcGain = targetGain;
		m_stage6AgcOffset = targetOffset;
	}
	else
	{
		m_stage6AgcGain = m_stage6AgcGain + (targetGain - m_stage6AgcGain) * alpha;
		m_stage6AgcOffset = m_stage6AgcOffset + (targetOffset - m_stage6AgcOffset) * alpha;
	}
	m_stage6AgcGain = ClampStage5Double(m_stage6AgcGain, m_stage6AgcMinGain, m_stage6AgcMaxGain);
	m_stage6AgcOffset = ClampStage5Double(m_stage6AgcOffset, m_stage6AgcMinOffset, m_stage6AgcMaxOffset);
	m_stage6AgcTargetGain = targetGain;
	m_stage6AgcTargetOffset = targetOffset;
	m_stage6AgcLowInput = lowInput;
	m_stage6AgcHighInput = highInput;
	m_stage6AgcSampleCount = sampleCount;
	m_stage6AgcValid = valid;
	m_stage6AgcFallbackReason = fallbackReason;
	m_stage6AgcInitialized = true;
	m_stage6AgcLastUpdateNs = nowNs;
	m_stage6AgcLastUpdateSourceSeq = sourceSeq;
	m_stage6AgcStatsMsCurrent = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - statsBegin).count();

	ApplyStage6FinalPostprocessInputs();
	m_perfStats.recordStage6Agc(
		m_stage6AgcStatsMsCurrent,
		m_stage6AgcApplyMsCurrent,
		true,
		m_stage6AgcMode.c_str(),
		m_stage6AgcGain,
		m_stage6AgcOffset,
		m_stage6AgcLowInput,
		m_stage6AgcHighInput,
		m_stage6AgcSampleCount);
	LogStage6Agc(sourceSeq, false);
}

void HwaSimIR::LogStage6Agc(std::uint64_t sourceSeq, bool forceLog)
{
	std::ostringstream state;
	state << (m_stage6AgcEnabled ? 1 : 0)
		<< ":" << m_stage6AgcMode
		<< ":" << m_stage6AgcStatsSource
		<< ":" << m_stage6AgcUpdateHz
		<< ":" << m_stage6AgcLowPercentile
		<< ":" << m_stage6AgcHighPercentile
		<< ":" << m_stage6AgcGain
		<< ":" << m_stage6AgcOffset
		<< ":" << m_stage6AgcFallbackReason;
	const std::string stateKey = state.str();
	const bool stateChanged = stateKey != m_lastStage6AgcLogState;
	const bool sampleDue =
		sourceSeq <= 3 ||
		(m_stage6AgcLogEveryFrames > 0 && sourceSeq > 0 &&
			(sourceSeq % static_cast<std::uint64_t>(m_stage6AgcLogEveryFrames)) == 0);
	if (!forceLog && !m_stage6AgcDebugLog && !m_enableIRVerboseLog && !stateChanged && !sampleDue)
	{
		return;
	}
	m_lastStage6AgcLogState = stateKey;
	++m_stage6AgcLogCounter;
	const bool agcEffective =
		m_stage6AgcEnabled &&
		m_stage6AgcApplyTo == "final_display" &&
		m_stage6AgcMode != "Off";
	std::cout << "[Stage6 AGC]"
		<< " sourceSeq=" << sourceSeq
		<< " enabled=" << (m_stage6AgcEnabled ? "1" : "0")
		<< " effective=" << (agcEffective ? "1" : "0")
		<< " mode=" << m_stage6AgcMode
		<< " statsSource=" << m_stage6AgcStatsSource
		<< " updateHz=" << m_stage6AgcUpdateHz
		<< " sampleCount=" << m_stage6AgcSampleCount
		<< " lowPercentile=" << m_stage6AgcLowPercentile
		<< " highPercentile=" << m_stage6AgcHighPercentile
		<< " lowInput=" << m_stage6AgcLowInput
		<< " highInput=" << m_stage6AgcHighInput
		<< " gain=" << m_stage6AgcTargetGain
		<< " offset=" << m_stage6AgcTargetOffset
		<< " gainSmoothed=" << m_stage6AgcGain
		<< " offsetSmoothed=" << m_stage6AgcOffset
		<< " valid=" << (m_stage6AgcValid ? "1" : "0")
		<< " fallbackReason=" << m_stage6AgcFallbackReason
		<< " stage6AgcStatsMs=" << m_stage6AgcStatsMsCurrent
		<< std::endl;
}

void HwaSimIR::LogStage6ViewportDiag(const char* reason) const
{
	const WindowProperties props = m_pGraphicsWindow ? m_pGraphicsWindow->get_properties() : WindowProperties();
	const int windowW = props.get_x_size();
	const int windowH = props.get_y_size();
	const int rawW = m_stage6RawSceneTex != nullptr ? m_stage6RawSceneTex->get_x_size() : 0;
	const int rawH = m_stage6RawSceneTex != nullptr ? m_stage6RawSceneTex->get_y_size() : 0;
	const int renderW = m_renderTex != nullptr ? m_renderTex->get_x_size() : 0;
	const int renderH = m_renderTex != nullptr ? m_renderTex->get_y_size() : 0;
	const float uvScaleU = rawW > 0 ? std::min(1.0f, static_cast<float>(m_stage6FinalWidth) / static_cast<float>(rawW)) : 0.0f;
	const float uvScaleV = rawH > 0 ? std::min(1.0f, static_cast<float>(m_stage6FinalHeight) / static_cast<float>(rawH)) : 0.0f;
	LVecBase4 dims(0.0f, 0.0f, 0.0f, 0.0f);
	if (m_stage6FinalRegion != nullptr)
	{
		dims = m_stage6FinalRegion->get_dimensions();
	}
	const bool finalFullscreen =
		std::fabs(dims[0] - 0.0f) < 0.0001f &&
		std::fabs(dims[1] - 1.0f) < 0.0001f &&
		std::fabs(dims[2] - 0.0f) < 0.0001f &&
		std::fabs(dims[3] - 1.0f) < 0.0001f &&
		!m_stage6FinalCard.is_empty() &&
		m_stage6FinalWidth == renderW &&
		m_stage6FinalHeight == renderH &&
		rawW >= m_stage6FinalWidth &&
		rawH >= m_stage6FinalHeight &&
		uvScaleU > 0.0f &&
		uvScaleV > 0.0f;

	std::cout << "[Stage6 ViewportDiag]"
		<< " reason=" << (reason != nullptr ? reason : "unknown")
		<< " windowSize=" << windowW << "x" << windowH
		<< " rawSceneTexSize=" << rawW << "x" << rawH
		<< " rawSceneRequested=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
		<< " finalUvScale=" << uvScaleU << "," << uvScaleV
		<< " finalRegionDimensions=" << dims[0] << "," << dims[1] << "," << dims[2] << "," << dims[3]
		<< " finalCardBounds=-1,1,-1,1"
		<< " renderTexSize=" << renderW << "x" << renderH
		<< " fullscreen=" << (finalFullscreen ? "1" : "0")
		<< std::endl;
	if (!finalFullscreen)
	{
		std::cout << "STAGE6_FINAL_NOT_FULLSCREEN"
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< " finalRegionDimensions=" << dims[0] << "," << dims[1] << "," << dims[2] << "," << dims[3]
			<< " rawSceneTexSize=" << rawW << "x" << rawH
			<< " rawSceneRequested=" << m_stage6FinalWidth << "x" << m_stage6FinalHeight
			<< " finalUvScale=" << uvScaleU << "," << uvScaleV
			<< " renderTexSize=" << renderW << "x" << renderH
			<< std::endl;
	}
}

IRStage7WeatherRuntimeInput HwaSimIR::BuildStage7WeatherInput() const
{
	IRStage7WeatherRuntimeInput input;
	input.useUdpInput = m_stage7UseWeatherUdpInput && m_isAddPlatform;
	if (m_isAddPlatform)
	{
		input.envSky = m_initSceneData.trackingInit.envSky;
		input.envTerrain = m_initSceneData.trackingInit.envTerrain;
		input.visibilityM = m_initSceneData.trackingInit.envVisibility;
		input.humidity = m_initSceneData.trackingInit.envHumidity;
		input.windV = m_initSceneData.trackingInit.envWindV;
		input.windDir = m_initSceneData.trackingInit.envWindDir;
		input.envTempC = m_initSceneData.trackingInit.envTemp;
		input.envRadScaleSky = m_initSceneData.trackingInit.envRadScaleSky;
		input.envRadScaleTerrain = m_initSceneData.trackingInit.envRadScaleTerrain;
		input.envMaxHeightRain = m_initSceneData.trackingInit.envMaxHeightRain;
		input.envTransHeightRain = m_initSceneData.trackingInit.envTransHeightRain;
		input.envMaxHeightSnow = m_initSceneData.trackingInit.envMaxHeightSnow;
		input.envTransHeightSnow = m_initSceneData.trackingInit.envTransHeightSnow;
		input.envRainSnowSpeedScale = m_initSceneData.trackingInit.envRainSnowSpeedScale;
	}
	return input;
}

IRStage7WeatherState HwaSimIR::EvaluateStage7WeatherState(const IRRuntimeEnvironment& environment) const
{
	return m_stage7WeatherEffects.evaluate(
		BuildStage7WeatherInput(),
		environment.band,
		m_stage7WeatherEnabled,
		m_stage7CloudLayerEnabled,
		m_stage7FogEnabled,
		m_stage7PrecipitationEnabled);
}

void HwaSimIR::ApplyStage7WeatherInputs(NodePath& node, const IRStage7WeatherState& weatherState)
{
	if (node.is_empty())
	{
		return;
	}
	SetShaderInputCached(node, "u_stage7_weather_type", LVecBase2i(weatherState.envSky, 0));
	SetShaderInputCached(node, "u_stage7_cloud_coverage", LVecBase2f(static_cast<float>(weatherState.cloudCoverage), 0.0f));
	SetShaderInputCached(node, "u_stage7_cloud_opacity", LVecBase2f(static_cast<float>(weatherState.cloudOpacity), 0.0f));
	SetShaderInputCached(node, "u_stage7_cloud_temperature_K", LVecBase2f(static_cast<float>(weatherState.cloudTemperatureK), 0.0f));
	SetShaderInputCached(node, "u_stage7_cloud_gray", LVecBase2f(static_cast<float>(weatherState.cloudGray), 0.0f));
	SetShaderInputCached(node, "u_stage7_fog_density", LVecBase2f(static_cast<float>(weatherState.fogDensity), 0.0f));
	SetShaderInputCached(node, "u_stage7_fog_gray", LVecBase2f(static_cast<float>(weatherState.fogGray), 0.0f));
	SetShaderInputCached(node, "u_stage7_precipitation_type", LVecBase2i(IRWeatherEffects::precipitationCode(weatherState.precipitationType), 0));
	SetShaderInputCached(node, "u_stage7_precipitation_density", LVecBase2f(static_cast<float>(weatherState.precipitationDensity), 0.0f));
	SetShaderInputCached(node, "u_stage7_precipitation_speed", LVecBase2f(static_cast<float>(weatherState.precipitationSpeed), 0.0f));
	SetShaderInputCached(node, "u_stage7_sun_direct_scale", LVecBase2f(static_cast<float>(weatherState.sunDirectScale), 0.0f));
	SetShaderInputCached(node, "u_stage7_sky_diffuse_scale", LVecBase2f(static_cast<float>(weatherState.skyDiffuseScale), 0.0f));
	SetShaderInputCached(node, "u_stage7_target_contrast_scale", LVecBase2f(static_cast<float>(weatherState.targetContrastScale), 0.0f));
}

void HwaSimIR::InitStage7WeatherScene()
{
	for (size_t i = 0; i < m_cloudNodes.size(); ++i)
	{
		if (!m_cloudNodes[i].is_empty())
		{
			m_cloudNodes[i].remove_node();
		}
	}
	for (size_t i = 0; i < m_stage7PrecipitationNodes.size(); ++i)
	{
		if (!m_stage7PrecipitationNodes[i].is_empty())
		{
			m_stage7PrecipitationNodes[i].remove_node();
		}
	}
	m_cloudNodes.clear();
	m_stage7PrecipitationNodes.clear();
	m_stage7WeatherTextureCacheKey.clear();
	m_stage7CachedCloudTexturePath.clear();
	m_stage7CachedRainTexturePath.clear();
	m_stage7CachedSnowTexturePath.clear();
	m_stage7CloudTexture = nullptr;
	m_stage7RainTexture = nullptr;
	m_stage7SnowTexture = nullptr;

	if (!m_stage7WeatherEnabled || m_cameraNode.is_empty())
	{
		return;
	}

	// Stage7C.1: default cloud rendering is a sky-dome shader perturbation.
	// Camera-attached cloud cards are intentionally not created on the main path.
	const int cloudCardCount = 0;
	for (int i = 0; i < cloudCardCount; ++i)
	{
		CardMaker cloudMaker("Stage7_CloudLayer_Card");
		cloudMaker.set_frame(-1.0f, 1.0f, -0.35f, 0.35f);
		NodePath cloud = m_cameraNode.attach_new_node(cloudMaker.generate());
		const float x = -900.0f + static_cast<float>((i * 173) % 1800);
		const float y = 1800.0f + static_cast<float>((i % 5) * 180);
		const float z = 240.0f + static_cast<float>((i * 97) % 520);
		const float scale = 180.0f + static_cast<float>((i % 4) * 55);
		cloud.set_pos(x, y, z);
		cloud.set_scale(scale * 2.1f, 1.0f, scale);
		cloud.set_transparency(TransparencyAttrib::M_alpha);
		cloud.set_depth_write(false);
		cloud.set_bin("transparent", 10);
		ApplyInfraredShader(cloud, false);
		cloud.set_shader_input("u_object_kind", LVecBase2i(2, 0));
		cloud.hide();
		m_cloudNodes.push_back(cloud);
	}

	const int precipitationCount =
		(m_stage7PrecipitationEnabled && m_stage7PrecipitationMode == 2)
		? std::max(0, std::min(128, m_stage7PrecipitationMaxParticles))
		: 0;
	for (int i = 0; i < precipitationCount; ++i)
	{
		CardMaker particleMaker("Stage7_Precipitation_Card");
		particleMaker.set_frame(-0.035f, 0.035f, -0.55f, 0.55f);
		NodePath particle = m_cameraNode.attach_new_node(particleMaker.generate());
		const float x = -700.0f + static_cast<float>((i * 61) % 1400);
		const float y = 240.0f + static_cast<float>((i % 16) * 28);
		const float z = -320.0f + static_cast<float>((i * 43) % 760);
		particle.set_pos(x, y, z);
		particle.set_scale(30.0f, 1.0f, 120.0f);
		particle.set_transparency(TransparencyAttrib::M_alpha);
		particle.set_depth_write(false);
		particle.set_bin("transparent", 20);
		ApplyInfraredShader(particle, false);
		particle.set_shader_input("u_object_kind", LVecBase2i(3, 0));
		particle.hide();
		m_stage7PrecipitationNodes.push_back(particle);
	}
}

int HwaSimIR::RefreshStage7WeatherTextureCache(const IRStage7WeatherState& weatherState)
{
	std::ostringstream key;
	key << weatherState.cloudTexturePath
		<< "|" << m_cloudNodes.size()
		<< "|" << m_stage7PrecipitationNodes.size()
		<< "|" << m_stage7PrecipitationMode
		<< "|" << IRWeatherEffects::precipitationCode(weatherState.precipitationType);
	const std::string cacheKey = key.str();
	if (cacheKey == m_stage7WeatherTextureCacheKey)
	{
		return 0;
	}
	m_stage7WeatherTextureCacheKey = cacheKey;

	int loadCount = 0;
	if (!m_cloudNodes.empty())
	{
		const std::string resolvedCloudTexturePath = weatherState.cloudTexturePath.empty()
			? std::string()
			: FirstExistingPath(BuildRuntimeConfigPathCandidates(weatherState.cloudTexturePath));
		if (resolvedCloudTexturePath != m_stage7CachedCloudTexturePath)
		{
			m_stage7CachedCloudTexturePath = resolvedCloudTexturePath;
			m_stage7CloudTexture = nullptr;
			if (!resolvedCloudTexturePath.empty() && FileExists(resolvedCloudTexturePath))
			{
				m_stage7CloudTexture = TexturePool::load_texture(resolvedCloudTexturePath);
				++loadCount;
			}
		}
		if (m_stage7CloudTexture)
		{
			for (size_t i = 0; i < m_cloudNodes.size(); ++i)
			{
				if (!m_cloudNodes[i].is_empty())
				{
					m_cloudNodes[i].set_texture(m_stage7CloudTexture, 1);
				}
			}
		}
	}

	if (m_stage7PrecipitationMode == 2 && !m_stage7PrecipitationNodes.empty())
	{
		const std::string rainPath = m_stage7WeatherEffects.texturePathForKey("rain_shaft");
		const std::string snowPath = m_stage7WeatherEffects.texturePathForKey("snow_rgba");
		const std::string resolvedRainPath = rainPath.empty() ? std::string() : FirstExistingPath(BuildRuntimeConfigPathCandidates(rainPath));
		const std::string resolvedSnowPath = snowPath.empty() ? std::string() : FirstExistingPath(BuildRuntimeConfigPathCandidates(snowPath));

		if (resolvedRainPath != m_stage7CachedRainTexturePath)
		{
			m_stage7CachedRainTexturePath = resolvedRainPath;
			m_stage7RainTexture = nullptr;
			if (!resolvedRainPath.empty() && FileExists(resolvedRainPath))
			{
				m_stage7RainTexture = TexturePool::load_texture(resolvedRainPath);
				++loadCount;
			}
		}
		if (resolvedSnowPath != m_stage7CachedSnowTexturePath)
		{
			m_stage7CachedSnowTexturePath = resolvedSnowPath;
			m_stage7SnowTexture = nullptr;
			if (!resolvedSnowPath.empty() && FileExists(resolvedSnowPath))
			{
				m_stage7SnowTexture = TexturePool::load_texture(resolvedSnowPath);
				++loadCount;
			}
		}
		PT(Texture) activeTexture = weatherState.precipitationType == IRStage7PrecipitationType::Snow
			? m_stage7SnowTexture
			: m_stage7RainTexture;
		if (activeTexture)
		{
			for (size_t i = 0; i < m_stage7PrecipitationNodes.size(); ++i)
			{
				if (!m_stage7PrecipitationNodes[i].is_empty())
				{
					m_stage7PrecipitationNodes[i].set_texture(activeTexture, 1);
				}
			}
		}
	}
	return loadCount;
}

void HwaSimIR::UpdateStage7WeatherNodes(const IRStage7WeatherState& weatherState, double currentTime)
{
	const auto totalStart = std::chrono::high_resolution_clock::now();
	const int textureLoadCountThisFrame = RefreshStage7WeatherTextureCache(weatherState);
	const auto updateStart = std::chrono::high_resolution_clock::now();

	for (size_t i = 0; i < m_cloudNodes.size(); ++i)
	{
		if (m_cloudNodes[i].is_empty())
		{
			continue;
		}
		if (weatherState.cloudEnable && weatherState.cloudCoverage > 0.01)
		{
			m_cloudNodes[i].show();
		}
		else
		{
			m_cloudNodes[i].hide();
		}
		ApplyStage7WeatherInputs(m_cloudNodes[i], weatherState);
		SetShaderInputCached(m_cloudNodes[i], "u_object_kind", LVecBase2i(2, 0));
		SetShaderInputCached(m_cloudNodes[i], "u_cloud_density", LVecBase2f(static_cast<float>(weatherState.cloudCoverage), 0.0f));
		SetShaderInputCached(m_cloudNodes[i], "u_time", LVecBase2f(static_cast<float>(currentTime), 0.0f));
	}

	const bool precipitationVisible = weatherState.precipitationType != IRStage7PrecipitationType::None &&
		weatherState.precipitationDensity > 0.01 &&
		m_stage7PrecipitationMode == 2;
	for (size_t i = 0; i < m_stage7PrecipitationNodes.size(); ++i)
	{
		if (m_stage7PrecipitationNodes[i].is_empty())
		{
			continue;
		}
		if (precipitationVisible)
		{
			m_stage7PrecipitationNodes[i].show();
		}
		else
		{
			m_stage7PrecipitationNodes[i].hide();
		}
		const double drift = weatherState.windV * 0.35 * std::sin((weatherState.windDir + 90.0) * 3.14159265358979323846 / 180.0);
		const float x = -700.0f + static_cast<float>(((static_cast<int>(i) * 61) % 1400)) + static_cast<float>(std::fmod(currentTime * drift * 20.0, 1400.0));
		const float y = 240.0f + static_cast<float>((i % 16) * 28);
		const float fall = static_cast<float>(std::fmod(currentTime * weatherState.precipitationSpeed * 180.0 + i * 43.0, 760.0));
		const float z = 380.0f - fall;
		m_stage7PrecipitationNodes[i].set_pos(x, y, z);
		m_stage7PrecipitationNodes[i].set_scale(weatherState.precipitationType == IRStage7PrecipitationType::Snow ? 38.0f : 28.0f,
			1.0f,
			weatherState.precipitationType == IRStage7PrecipitationType::Snow ? 52.0f : 125.0f);
		ApplyStage7WeatherInputs(m_stage7PrecipitationNodes[i], weatherState);
		SetShaderInputCached(m_stage7PrecipitationNodes[i], "u_object_kind", LVecBase2i(3, 0));
		SetShaderInputCached(m_stage7PrecipitationNodes[i], "u_time", LVecBase2f(static_cast<float>(currentTime), 0.0f));
	}
	ApplyStage6FinalPostprocessInputs();
	const auto updateEnd = std::chrono::high_resolution_clock::now();
	const double updateWeatherNodesMs = std::chrono::duration<double, std::milli>(updateEnd - updateStart).count();
	const double totalWeatherMs = std::chrono::duration<double, std::milli>(updateEnd - totalStart).count();
	const int cloudNodeCount = static_cast<int>(m_cloudNodes.size());
	const int precipitationNodeCount = static_cast<int>(m_stage7PrecipitationNodes.size());
	LogStage7Perf(weatherState, cloudNodeCount + precipitationNodeCount, cloudNodeCount, precipitationNodeCount, textureLoadCountThisFrame, updateWeatherNodesMs, totalWeatherMs);
}

void HwaSimIR::LogStage7Perf(const IRStage7WeatherState& weatherState, int weatherNodeCount, int cloudNodeCount, int precipitationNodeCount, int textureLoadCountThisFrame, double updateWeatherNodesMs, double totalWeatherMs)
{
	std::ostringstream state;
	state << weatherNodeCount
		<< ":" << cloudNodeCount
		<< ":" << precipitationNodeCount
		<< ":" << textureLoadCountThisFrame
		<< ":" << m_stage7PrecipitationMode
		<< ":" << IRWeatherEffects::precipitationCode(weatherState.precipitationType)
		<< ":" << static_cast<int>(weatherState.precipitationDensity * 1000.0);
	const std::string stateKey = state.str();
	++m_stage7PerfLogCounter;
	const bool shouldLog = textureLoadCountThisFrame > 0 ||
		m_stage7PerfLogCounter <= 3 ||
		m_stage7LastPerfState != stateKey ||
		(m_stage7PerfLogCounter % 120) == 0;
	if (!shouldLog)
	{
		return;
	}
	std::cout << "[Stage7 Perf]"
		<< " weatherNodeCount=" << weatherNodeCount
		<< " cloudNodeCount=" << cloudNodeCount
		<< " precipitationNodeCount=" << precipitationNodeCount
		<< " textureLoadCountThisFrame=" << textureLoadCountThisFrame
		<< " updateWeatherNodesMs=" << updateWeatherNodesMs
		<< " totalWeatherMs=" << totalWeatherMs
		<< " precipitationMode=" << m_stage7PrecipitationModeName
		<< std::endl;
	if (textureLoadCountThisFrame > 0)
	{
		std::cout << "[Stage7 Perf][WARN] STAGE7_TEXTURE_LOAD_IN_FRAME"
			<< " textureLoadCountThisFrame=" << textureLoadCountThisFrame
			<< " note=allowed_on_weather_state_change_only"
			<< std::endl;
	}
	m_stage7LastPerfState = stateKey;
}

void HwaSimIR::CreateEnginePlumeForTarget(TargetPlatformData& targetPlat)
{
	if (!m_stage5PlumeOptions.enableEnginePlume ||
		m_stage5PlumeOptions.maxPlumeNodes <= 0 ||
		targetPlat.nodePath.is_empty() ||
		(!targetPlat.enginePlumeCoreNodePath.is_empty() && !targetPlat.enginePlumeHaloNodePath.is_empty()))
	{
		return;
	}

	int plumeNodeCount = 0;
	for (size_t i = 0; i < m_targetPlatformList.size(); ++i)
	{
		if (!m_targetPlatformList[i].enginePlumeCoreNodePath.is_empty())
		{
			++plumeNodeCount;
		}
		if (!m_targetPlatformList[i].enginePlumeHaloNodePath.is_empty())
		{
			++plumeNodeCount;
		}
	}
	if (plumeNodeCount + 2 > m_stage5PlumeOptions.maxPlumeNodes)
	{
		return;
	}

	targetPlat.enginePlumeHaloNodePath = targetPlat.nodePath.attach_new_node(CreateStage5EnginePlumeBillboardNode());
	targetPlat.enginePlumeHaloNodePath.set_name("Stage5_EnginePlume_Halo_Target");
	targetPlat.enginePlumeHaloNodePath.set_transparency(TransparencyAttrib::M_alpha);
	targetPlat.enginePlumeHaloNodePath.set_depth_write(false);
	targetPlat.enginePlumeHaloNodePath.set_depth_test(true);
	targetPlat.enginePlumeHaloNodePath.set_two_sided(true);
	targetPlat.enginePlumeHaloNodePath.set_bin("transparent", 5);
	ApplyInfraredShader(targetPlat.enginePlumeHaloNodePath, false);
	targetPlat.enginePlumeHaloNodePath.set_shader_input("u_object_kind", LVecBase2i(4, 0));
	targetPlat.enginePlumeHaloNodePath.set_shader_input("u_plume_layer", LVecBase2i(2, 0));
	targetPlat.enginePlumeHaloNodePath.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
	targetPlat.enginePlumeHaloNodePath.hide();

	targetPlat.enginePlumeCoreNodePath = targetPlat.nodePath.attach_new_node(CreateStage5EnginePlumeBillboardNode());
	targetPlat.enginePlumeCoreNodePath.set_name("Stage5_EnginePlume_Core_Target");
	targetPlat.enginePlumeCoreNodePath.set_transparency(TransparencyAttrib::M_alpha);
	targetPlat.enginePlumeCoreNodePath.set_depth_write(false);
	targetPlat.enginePlumeCoreNodePath.set_depth_test(true);
	targetPlat.enginePlumeCoreNodePath.set_two_sided(true);
	targetPlat.enginePlumeCoreNodePath.set_bin("transparent", 6);
	ApplyInfraredShader(targetPlat.enginePlumeCoreNodePath, false);
	targetPlat.enginePlumeCoreNodePath.set_shader_input("u_object_kind", LVecBase2i(4, 0));
	targetPlat.enginePlumeCoreNodePath.set_shader_input("u_plume_layer", LVecBase2i(1, 0));
	targetPlat.enginePlumeCoreNodePath.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
	targetPlat.enginePlumeCoreNodePath.hide();
}

void HwaSimIR::HideEnginePlume(TargetPlatformData& targetPlat)
{
	if (!targetPlat.enginePlumeCoreNodePath.is_empty() && !targetPlat.enginePlumeCoreNodePath.is_hidden())
	{
		targetPlat.enginePlumeCoreNodePath.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
		targetPlat.enginePlumeCoreNodePath.hide();
	}
	if (!targetPlat.enginePlumeHaloNodePath.is_empty() && !targetPlat.enginePlumeHaloNodePath.is_hidden())
	{
		targetPlat.enginePlumeHaloNodePath.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
		targetPlat.enginePlumeHaloNodePath.hide();
	}
}

IREnginePlumeOutput HwaSimIR::UpdateEnginePlumeForTarget(TargetPlatformData& targetPlat, float dtSec, float ambientTempK, IRBand band, bool targetRenderable, double currentTime, bool* modelUpdated)
{
	IREnginePlumeOutput output;
	if (modelUpdated)
	{
		*modelUpdated = false;
	}
	if (targetPlat.nodePath.is_empty())
	{
		return output;
	}
	if (!targetRenderable)
	{
		HideEnginePlume(targetPlat);
		return output;
	}
	if (targetPlat.enginePlumeCoreNodePath.is_empty() && targetPlat.enginePlumeHaloNodePath.is_empty())
	{
		HideEnginePlume(targetPlat);
		return output;
	}

	const std::string platformName = Stage4PlatformName(targetPlat.type);
	const std::string runtimeKey = platformName + "#plat" + std::to_string(targetPlat.targetState.targetPlatID)
		+ "#target" + std::to_string(targetPlat.targetState.targetID);
	Stage5PlumeRuntimeCache& cache = m_stage5PlumeRuntimeCache[runtimeKey];
	const bool bypassEngineState = m_stage5PlumeOptions.forcePlumeVisible || m_stage5PlumeOptions.enablePlumeDebug;
	if (m_stage5PlumeOptions.useEngineState && !targetPlat.targetState.engineState && !bypassEngineState)
	{
		HideEnginePlume(targetPlat);
		cache.hasOutput = false;
		cache.hasAppliedOutput = false;
		cache.lastEngineState = false;
		cache.lastBand = band;
		return output;
	}
	const double updateIntervalSec = 1.0 / std::max(1.0, m_stage5PlumeUpdateHz);
	const bool stateChanged = !cache.hasOutput ||
		cache.lastEngineState != targetPlat.targetState.engineState ||
		cache.lastBand != band;
	const bool updateDue = cache.lastUpdateTime < 0.0 ||
		(currentTime - cache.lastUpdateTime) >= updateIntervalSec;
	const bool refreshModel = stateChanged || updateDue;
	if (!refreshModel)
	{
		return cache.output;
	}

	IREnginePlumeInput input;
	input.platformName = platformName;
	input.runtimeKey = runtimeKey;
	input.engineState = targetPlat.targetState.engineState;
	input.dtSec = cache.lastUpdateTime >= 0.0
		? static_cast<float>(std::max(0.0, currentTime - cache.lastUpdateTime))
		: dtSec;
	input.ambientTempK = ambientTempK;
	input.band = band;
	input.options = m_stage5PlumeOptions;
	output = m_irEnginePlumeModel.update(input);
	cache.output = output;
	cache.hasOutput = true;
	cache.lastUpdateTime = currentTime;
	cache.lastEngineState = targetPlat.targetState.engineState;
	cache.lastBand = band;
	if (modelUpdated)
	{
		*modelUpdated = true;
	}

	const bool validTarget = targetPlat.targetState.targetID >= 0 && targetPlat.targetState.viewValid;
	const bool coreVisible = targetRenderable && validTarget && output.coreNodeVisible;
	const bool haloVisible = targetRenderable && validTarget && output.haloNodeVisible;
	auto applyPlumeLayer = [&](NodePath& node, int layer, bool visible, float tempK, float gray, float opacity, float lengthM,
		float radiusRootM, float radiusTailM, float axialDecay, float radialDecay, float noiseScale, float noiseStrength, float bandGain,
		float previousTempK, float previousGray, float previousOpacity)
	{
		if (node.is_empty())
		{
			return;
		}
		if (!visible)
		{
			if (!node.is_hidden())
			{
				node.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
				node.hide();
			}
			return;
		}
		const bool becomingVisible = node.is_hidden();
		if (becomingVisible)
		{
			node.set_pos(output.localPos.x, output.localPos.y, output.localPos.z);
			node.set_hpr(0.0f, 0.0f, 0.0f);
			node.set_scale(
				std::max(0.01f, radiusRootM),
				std::max(0.05f, lengthM),
				std::max(0.01f, radiusRootM));
			node.set_shader_input("u_wave_band", LVecBase2i(static_cast<int>(band), 0));
			node.set_shader_input("u_ir_band_index", LVecBase2i(static_cast<int>(band), 0));
			node.set_shader_input("u_ir_band_class", LVecBase2i(IRBandClassForShader(band), 0));
			ApplyStage7WeatherInputs(node, m_stage7WeatherState);
			node.set_shader_input("u_object_kind", LVecBase2i(4, 0));
			node.set_shader_input("u_plume_layer", LVecBase2i(layer, 0));
			node.set_shader_input("u_plume_enabled", LVecBase2i(1, 0));
			node.set_shader_input("u_plume_length", LVecBase2f(lengthM, 0.0f));
			node.set_shader_input("u_plume_radius_root", LVecBase2f(radiusRootM, 0.0f));
			node.set_shader_input("u_plume_radius_tail", LVecBase2f(radiusTailM, 0.0f));
			node.set_shader_input("u_plume_axial_decay", LVecBase2f(axialDecay, 0.0f));
			node.set_shader_input("u_plume_radial_decay", LVecBase2f(radialDecay, 0.0f));
			node.set_shader_input("u_plume_noise_scale", LVecBase2f(noiseScale, 0.0f));
			node.set_shader_input("u_plume_noise_strength", LVecBase2f(noiseStrength, 0.0f));
			node.set_shader_input("u_plume_band_gain", LVecBase2f(bandGain, 0.0f));
			node.show();
		}
		const bool dynamicChanged = !cache.hasAppliedOutput ||
			std::fabs(tempK - previousTempK) > 0.25f ||
			std::fabs(gray - previousGray) > 0.001f ||
			std::fabs(opacity - previousOpacity) > 0.001f;
		if (becomingVisible || dynamicChanged)
		{
			node.set_shader_input("u_plume_temperature_K", LVecBase2f(tempK, 0.0f));
			node.set_shader_input("u_plume_gray", LVecBase2f(gray, 0.0f));
			node.set_shader_input("u_plume_opacity", LVecBase2f(opacity, 0.0f));
		}
		node.set_shader_input("u_time", LVecBase2f(static_cast<float>(currentTime), 0.0f));
	};
	applyPlumeLayer(targetPlat.enginePlumeHaloNodePath, 2, haloVisible, output.haloTempK, output.haloGray, output.haloOpacity,
		output.haloLengthM, output.haloRadiusRootM, output.haloRadiusTailM, output.haloAxialDecay, output.haloRadialDecay,
		output.haloNoiseScale, output.haloNoiseStrength, output.haloBandGain,
		cache.lastAppliedOutput.haloTempK, cache.lastAppliedOutput.haloGray, cache.lastAppliedOutput.haloOpacity);
	applyPlumeLayer(targetPlat.enginePlumeCoreNodePath, 1, coreVisible, output.coreTempK, output.coreGray, output.coreOpacity,
		output.coreLengthM, output.coreRadiusRootM, output.coreRadiusTailM, output.coreAxialDecay, output.coreRadialDecay,
		output.coreNoiseScale, output.coreNoiseStrength, output.coreBandGain,
		cache.lastAppliedOutput.coreTempK, cache.lastAppliedOutput.coreGray, cache.lastAppliedOutput.coreOpacity);
	cache.lastAppliedOutput = output;
	cache.hasAppliedOutput = true;

	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const std::string plumeLogKey = runtimeKey + "#plume";
	const std::string plumeLogState =
		std::to_string(targetPlat.targetState.engineState ? 1 : 0) + ":" +
		std::to_string(output.coreEnabled ? 1 : 0) + ":" +
		std::to_string(output.haloEnabled ? 1 : 0) + ":" +
		std::to_string(coreVisible ? 1 : 0) + ":" +
		std::to_string(haloVisible ? 1 : 0) + ":" +
		std::to_string(static_cast<int>(band));
	const bool plumeStateChanged = m_lastStage4TargetLogState[plumeLogKey] != plumeLogState;
	m_lastStage4TargetLogState[plumeLogKey] = plumeLogState;
	const bool shouldLog = m_enableIRVerboseLog ||
		m_stage5PlumeOptions.enablePlumeDebug ||
		m_stage5PlumeOptions.forcePlumeVisible ||
		frameSeq <= 3 ||
		(frameSeq % 120) == 0 ||
		plumeStateChanged;
	if (shouldLog)
	{
		std::cout << "[Stage5 Plume]"
			<< " targetType=0x" << std::hex << targetPlat.targetState.targetType << std::dec
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " platform=" << platformName
			<< " engineState=" << (targetPlat.targetState.engineState ? "1" : "0")
			<< " band=" << IRBandName(band)
			<< " coreEnabled=" << (output.coreEnabled ? "1" : "0")
			<< " haloEnabled=" << (output.haloEnabled ? "1" : "0")
			<< " coreTempK=" << output.coreTempK
			<< " haloTempK=" << output.haloTempK
			<< " coreGray=" << output.coreGray
			<< " haloGray=" << output.haloGray
			<< " coreOpacity=" << output.coreOpacity
			<< " haloOpacity=" << output.haloOpacity
			<< " coreVisible=" << (coreVisible ? "1" : "0")
			<< " haloVisible=" << (haloVisible ? "1" : "0")
			<< std::endl;
	}
	return output;
}

void HwaSimIR::LogStage5PlumePerf(int plumeNodeCount, int visiblePlumeCount, double updatePlumeMs)
{
	m_stage5PlumePerfMsTotal += updatePlumeMs;
	m_stage5PlumePerfMsMax = std::max(m_stage5PlumePerfMsMax, updatePlumeMs);
	++m_stage5PlumePerfSamples;
	std::ostringstream state;
	state << plumeNodeCount << ":" << visiblePlumeCount;
	const std::string stateKey = state.str();
	++m_stage5PlumePerfLogCounter;
	const bool shouldLog = m_enableIRVerboseLog ||
		m_stage5PlumePerfLogCounter <= 3 ||
		m_stage5PlumeLastPerfState != stateKey ||
		(m_stage5PlumePerfLogCounter % 120) == 0;
	if (!shouldLog)
	{
		return;
	}
	const double updatePlumeMsAvg = m_stage5PlumePerfSamples > 0
		? m_stage5PlumePerfMsTotal / static_cast<double>(m_stage5PlumePerfSamples)
		: 0.0;
	std::cout << "[Stage5 PlumePerf]"
		<< " plumeNodeCount=" << plumeNodeCount
		<< " visiblePlumeCount=" << visiblePlumeCount
		<< " textureLoadCountThisFrame=0"
		<< " updateHz=" << m_stage5PlumeUpdateHz
		<< " updatePlumeMs=" << updatePlumeMsAvg
		<< " updatePlumeMsMax=" << m_stage5PlumePerfMsMax
		<< " perfBudgetMs=" << m_stage5PlumePerfBudgetMs
		<< std::endl;
	if (updatePlumeMsAvg > m_stage5PlumePerfBudgetMs)
	{
		std::cout << "[Stage5 PlumePerf][WARN]"
			<< " updatePlumeMs=" << updatePlumeMsAvg
			<< " perfBudgetMs=" << m_stage5PlumePerfBudgetMs
			<< std::endl;
	}
	m_stage5PlumePerfMsTotal = 0.0;
	m_stage5PlumePerfMsMax = 0.0;
	m_stage5PlumePerfSamples = 0;
	m_stage5PlumeLastPerfState = stateKey;
}

void HwaSimIR::LogStage7Weather(const IRStage7WeatherState& weatherState, const char* reason, bool forceLog)
{
	std::ostringstream state;
	state << weatherState.envSky
		<< ":" << weatherState.weatherName
		<< ":" << static_cast<int>(weatherState.cloudCoverage * 1000.0)
		<< ":" << static_cast<int>(weatherState.fogDensity * 1000.0)
		<< ":" << IRWeatherEffects::precipitationCode(weatherState.precipitationType)
		<< ":" << static_cast<int>(weatherState.precipitationDensity * 1000.0);
	const std::string stateKey = state.str();
	++m_stage7WeatherLogCounter;
	if (!forceLog && m_stage7LastWeatherState == stateKey && m_stage7WeatherLogCounter > 3 && (m_stage7WeatherLogCounter % 120) != 0)
	{
		return;
	}

	std::cout << "[Stage7 Weather]"
		<< " reason=" << (reason != nullptr ? reason : "unknown")
		<< " envSky=" << weatherState.envSky
		<< " weatherName=" << weatherState.weatherName
		<< " visibilityM=" << weatherState.visibilityM
		<< " humidity=" << weatherState.humidity
		<< " windV=" << weatherState.windV
		<< " windDir=" << weatherState.windDir
		<< " cloudCoverage=" << weatherState.cloudCoverage
		<< " fogDensity=" << weatherState.fogDensity
		<< " precipitationType=" << IRWeatherEffects::precipitationName(weatherState.precipitationType)
		<< " sunDirectScale=" << weatherState.sunDirectScale
		<< " skyDiffuseScale=" << weatherState.skyDiffuseScale
		<< " targetContrastScale=" << weatherState.targetContrastScale
		<< " source=" << weatherState.source
		<< std::endl;
	std::cout << "[Stage7 CloudLayer]"
		<< " enabled=" << (weatherState.cloudEnable ? "1" : "0")
		<< " texture=" << (weatherState.cloudTexturePath.empty() ? "procedural" : weatherState.cloudTexturePath)
		<< " textureFound=" << (weatherState.cloudTextureFound ? "1" : "0")
		<< " coverage=" << weatherState.cloudCoverage
		<< " opacity=" << weatherState.cloudOpacity
		<< " temperatureK=" << weatherState.cloudTemperatureK
		<< " band=" << IRBandName(BuildRuntimeEnvironment().band)
		<< " cloudGray=" << weatherState.cloudGray
		<< std::endl;
	if (weatherState.cloudEnable && !weatherState.cloudTextureFound)
	{
		std::cout << "[Stage7 CloudLayer][WARN] STAGE7_WEATHER_TEXTURE_FALLBACK"
			<< " textureKey=" << weatherState.cloudTextureKey
			<< " path=" << weatherState.cloudTexturePath
			<< std::endl;
	}
	std::cout << "[Stage7 Fog]"
		<< " enabled=" << (weatherState.fogEnable ? "1" : "0")
		<< " visibilityM=" << weatherState.visibilityM
		<< " fogDensity=" << weatherState.fogDensity
		<< " fogGray=" << weatherState.fogGray
		<< " targetContrastScale=" << weatherState.targetContrastScale
		<< std::endl;
	std::cout << "[Stage7 Precipitation]"
		<< " enabled=" << (weatherState.precipitationType != IRStage7PrecipitationType::None ? "1" : "0")
		<< " type=" << IRWeatherEffects::precipitationName(weatherState.precipitationType)
		<< " density=" << weatherState.precipitationDensity
		<< " speed=" << weatherState.precipitationSpeed
		<< " windV=" << weatherState.windV
		<< " windDir=" << weatherState.windDir
		<< " maxHeight=" << weatherState.maxHeight
		<< " transHeight=" << weatherState.transHeight
		<< std::endl;
	const double sensorFovDeg = std::max(m_sensorDisplayConfig.horizontalFovDeg, m_sensorDisplayConfig.verticalFovDeg);
	const bool overlayActive = m_stage7WeatherEnabled &&
		m_stage7PrecipitationEnabled &&
		m_stage7PrecipitationMode == 1 &&
		weatherState.precipitationType != IRStage7PrecipitationType::None &&
		weatherState.precipitationDensity > 0.001;
	std::cout << "[Stage7 PrecipitationOverlay]"
		<< " mode=" << m_stage7PrecipitationModeName
		<< " active=" << (overlayActive ? "1" : "0")
		<< " type=" << IRWeatherEffects::precipitationName(weatherState.precipitationType)
		<< " density=" << weatherState.precipitationDensity
		<< " sensorFovDeg=" << sensorFovDeg
		<< " pattern=screen_sparse_low_contrast"
		<< " giantVerticalBars=0"
		<< " sameOutput=1"
		<< std::endl;
	m_stage7LastWeatherState = stateKey;
}

std::string HwaSimIR::BuildStage7UpdateKey(const IRRuntimeEnvironment& environment) const
{
	const int envTerrain = m_isAddPlatform ? m_initSceneData.trackingInit.envTerrain : 0;
	const int envSky = m_isAddPlatform ? m_initSceneData.trackingInit.envSky : environment.weatherCode;
	double farClipM = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.farClipM : 50000.0;
	if (!std::isfinite(farClipM) || farClipM <= 1.0)
	{
		farClipM = 50000.0;
	}
	const auto q100 = [](double value) -> int {
		return static_cast<int>(std::floor(value * 100.0 + 0.5));
	};
	std::ostringstream key;
	key << "band=" << static_cast<int>(environment.band)
		<< ":terrain=" << envTerrain
		<< ":sky=" << envSky
		<< ":airT=" << q100(environment.airTemperatureC)
		<< ":humidity=" << q100(environment.humidityPercent)
		<< ":visibility=" << q100(environment.visibilityMeters)
		<< ":wind=" << q100(environment.windSpeedMps)
		<< ":windDir=" << q100(environment.windDirectionDeg)
		<< ":sunAz=" << q100(environment.sunAzimuthDeg)
		<< ":sunEl=" << q100(environment.sunElevationDeg)
		<< ":sunStrength=" << q100(environment.sunStrength)
		<< ":far=" << q100(farClipM)
		<< ":enable=" << (m_enableStage7SkyHorizon ? 1 : 0)
		<< ":debug=" << m_stage7DebugMode
		<< ":real3d=" << (m_stage7UseReal3DBackground ? 1 : 0)
		<< ":groundZ=" << q100(m_stage7GroundZOffset)
		<< ":weather=" << (m_stage7WeatherEnabled ? 1 : 0)
		<< ":cloud=" << (m_stage7CloudLayerEnabled ? 1 : 0)
		<< ":fog=" << (m_stage7FogEnabled ? 1 : 0)
		<< ":precip=" << (m_stage7PrecipitationEnabled ? 1 : 0)
		<< ":precipMode=" << m_stage7PrecipitationMode
		<< ":udpWeather=" << (m_stage7UseWeatherUdpInput ? 1 : 0);
	if (m_isAddPlatform)
	{
		key << ":terrainScale=" << q100(m_initSceneData.trackingInit.envRadScaleTerrain)
			<< ":skyScale=" << q100(m_initSceneData.trackingInit.envRadScaleSky)
			<< ":envTemp=" << q100(m_initSceneData.trackingInit.envTemp)
			<< ":envVis=" << q100(m_initSceneData.trackingInit.envVisibility)
			<< ":envWindV=" << q100(m_initSceneData.trackingInit.envWindV)
			<< ":envWindDir=" << q100(m_initSceneData.trackingInit.envWindDir);
	}
	return key.str();
}

void HwaSimIR::UpdateStage7SkyHorizonPositionOnly()
{
	if (!m_enableStage7SkyHorizon || (m_skyNode.is_empty() && m_stage7LowerShellNode.is_empty()))
	{
		return;
	}
	const LPoint3f cameraPos = m_cameraNode.is_empty()
		? LPoint3f(0.0f, 0.0f, 0.0f)
		: m_cameraNode.get_pos(m_renderRoot);
	if (!m_skyNode.is_empty())
	{
		m_skyNode.set_pos(m_renderRoot, cameraPos);
	}
	if (!m_stage7LowerShellNode.is_empty())
	{
		m_stage7LowerShellNode.set_pos(m_renderRoot, cameraPos);
	}
}

void HwaSimIR::UpdateStage7SkyHorizon(const IRRuntimeEnvironment& environment, const char* reason, bool forceLog)
{
	if (!m_enableStage7SkyHorizon || m_skyNode.is_empty())
	{
		return;
	}

	const int envTerrain = m_isAddPlatform ? m_initSceneData.trackingInit.envTerrain : 0;
	const int envSky = m_isAddPlatform ? m_initSceneData.trackingInit.envSky : environment.weatherCode;
	double terrainScale = m_isAddPlatform ? m_initSceneData.trackingInit.envRadScaleTerrain : 1.0;
	double skyScale = m_isAddPlatform ? m_initSceneData.trackingInit.envRadScaleSky : 1.0;
	if (!std::isfinite(terrainScale) || terrainScale <= 0.0 || terrainScale > 5.0)
	{
		terrainScale = 1.0;
	}
	if (!std::isfinite(skyScale) || skyScale <= 0.0 || skyScale > 5.0)
	{
		skyScale = 1.0;
	}
	m_stage7WeatherState = EvaluateStage7WeatherState(environment);

	double skyGrayBase = 0.62;
	double groundGrayBase = 0.38;
	switch (environment.band)
	{
	case IRBand::Visible:
	case IRBand::NearInfrared:
	case IRBand::ShortWaveInfrared:
		skyGrayBase = 0.70;
		groundGrayBase = 0.40;
		break;
	case IRBand::MidWaveInfrared:
		skyGrayBase = 0.65;
		groundGrayBase = 0.35;
		break;
	case IRBand::LongWaveInfrared:
		skyGrayBase = 0.55;
		groundGrayBase = 0.38;
		break;
	default:
		break;
	}

	double skyWeatherFactor = 1.0;
	switch (envSky)
	{
	case 1: skyWeatherFactor = 0.85; break;
	case 2: skyWeatherFactor = 0.75; break;
	case 3: skyWeatherFactor = 0.95; break;
	case 4: skyWeatherFactor = 0.70; break;
	case 5: skyWeatherFactor = 0.80; break;
	default: skyWeatherFactor = 1.0; break;
	}

	double terrainFactor = 1.0;
	switch (envTerrain)
	{
	case 1: terrainFactor = 0.85; break;
	case 2: terrainFactor = 0.75; break;
	default: terrainFactor = 1.0; break;
	}

	const double skyGrayRaw = skyGrayBase * skyWeatherFactor * skyScale *
		m_stage7WeatherState.skyGrayScale * m_stage7WeatherState.skyDiffuseScale;
	const double groundGrayRaw = groundGrayBase * terrainFactor * terrainScale *
		m_stage7WeatherState.groundGrayScale;
	double skyGray = ClampStage5Double(skyGrayRaw, 0.12, 0.92);
	double groundGray = ClampStage5Double(groundGrayRaw, 0.18, 0.88);
	if (m_stage7WeatherState.fogEnable && m_stage7WeatherState.fogDensity > 0.0)
	{
		const double fogMix = ClampStage5Double(m_stage7WeatherState.fogDensity * 0.35, 0.0, 0.55);
		skyGray = skyGray * (1.0 - fogMix) + m_stage7WeatherState.fogGray * fogMix;
		groundGray = groundGray * (1.0 - fogMix) + m_stage7WeatherState.fogGray * fogMix;
	}
	if (std::fabs(skyGray - groundGray) < 0.03)
	{
		groundGray = ClampStage5Double(groundGray - 0.04, 0.18, 0.88);
	}
	if (m_stage7DebugMode == 3)
	{
		skyGray = 0.65;
		groundGray = 0.35;
	}

	double farClipM = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.farClipM : 50000.0;
	if (!std::isfinite(farClipM) || farClipM <= 1.0)
	{
		farClipM = 50000.0;
	}
	m_stage7SkyDomeRadius = ClampStage5Double(farClipM * 0.85, 1000.0, 200000.0);
	if (m_stage7SkyDomeRadius >= farClipM)
	{
		m_stage7SkyDomeRadius = std::max(1.0, farClipM * 0.85);
	}
	if (m_stage7SkyDomeRadius >= farClipM)
	{
		m_stage7SkyDomeRadius = std::max(0.5, farClipM - 1.0);
	}
	m_stage7LowerShellRadius = m_stage7SkyDomeRadius;

	const LPoint3f cameraPos = m_cameraNode.is_empty()
		? LPoint3f(0.0f, 0.0f, 0.0f)
		: m_cameraNode.get_pos(m_renderRoot);
	m_stage7GroundReferenceZ = m_stage7GroundZOffset;

	const bool skyVisible = m_stage7DebugMode != 2;
	const bool lowerShellVisible = m_stage7DebugMode != 1;

	if (!m_skyNode.is_empty())
	{
		if (skyVisible)
		{
			m_skyNode.show();
		}
		else
		{
			m_skyNode.hide();
		}
		m_skyNode.set_pos(m_renderRoot, cameraPos);
		m_skyNode.set_hpr(m_renderRoot, 0.0f, 0.0f, 0.0f);
		m_skyNode.set_scale(static_cast<float>(m_stage7SkyDomeRadius));
		SetShaderInputCached(m_skyNode, "u_stage7_sky_horizon_en", LVecBase2i(m_enableStage7SkyHorizon ? 1 : 0, 0));
		SetShaderInputCached(m_skyNode, "u_stage7_background_kind", LVecBase2i(1, 0));
		SetShaderInputCached(m_skyNode, "u_stage7_sky_gray", LVecBase2f(static_cast<float>(skyGray), 0.0f));
		SetShaderInputCached(m_skyNode, "u_stage7_ground_gray", LVecBase2f(static_cast<float>(groundGray), 0.0f));
		ApplyStage7WeatherInputs(m_skyNode, m_stage7WeatherState);
	}
	if (!m_stage7LowerShellNode.is_empty())
	{
		if (lowerShellVisible)
		{
			m_stage7LowerShellNode.show();
		}
		else
		{
			m_stage7LowerShellNode.hide();
		}
		m_stage7LowerShellNode.set_pos(m_renderRoot, cameraPos);
		m_stage7LowerShellNode.set_hpr(m_renderRoot, 0.0f, 0.0f, 0.0f);
		m_stage7LowerShellNode.set_scale(static_cast<float>(m_stage7LowerShellRadius));
		SetShaderInputCached(m_stage7LowerShellNode, "u_stage7_sky_horizon_en", LVecBase2i(m_enableStage7SkyHorizon ? 1 : 0, 0));
		SetShaderInputCached(m_stage7LowerShellNode, "u_stage7_background_kind", LVecBase2i(2, 0));
		SetShaderInputCached(m_stage7LowerShellNode, "u_stage7_sky_gray", LVecBase2f(static_cast<float>(skyGray), 0.0f));
		SetShaderInputCached(m_stage7LowerShellNode, "u_stage7_ground_gray", LVecBase2f(static_cast<float>(groundGray), 0.0f));
		ApplyStage7WeatherInputs(m_stage7LowerShellNode, m_stage7WeatherState);
	}

	LogStage7SkyGround(environment, envTerrain, envSky, skyGrayRaw, groundGrayRaw, skyGray, groundGray, farClipM, m_stage7GroundReferenceZ, forceLog, reason);
	LogStage7Weather(m_stage7WeatherState, reason, forceLog);
}

void HwaSimIR::LogStage7SkyGround(const IRRuntimeEnvironment& environment, int envTerrain, int envSky, double skyGrayRaw, double groundGrayRaw, double skyGrayFinal, double groundGrayFinal, double farClipM, double groundReferenceZ, bool forceLog, const char* reason)
{
	const bool skyVisible = !m_skyNode.is_empty() && !m_skyNode.is_hidden();
	const bool lowerShellVisible = !m_stage7LowerShellNode.is_empty() && !m_stage7LowerShellNode.is_hidden();
	const bool radiusLessThanFarClip = m_stage7SkyDomeRadius < farClipM;
	std::ostringstream state;
	state << (m_enableStage7SkyHorizon ? 1 : 0)
		<< ":" << IRBandName(environment.band)
		<< ":" << envTerrain
		<< ":" << envSky
		<< ":" << m_stage7DebugModeName
		<< ":" << static_cast<int>(skyGrayFinal * 1000.0)
		<< ":" << static_cast<int>(groundGrayFinal * 1000.0)
		<< ":" << static_cast<int>(m_stage7SkyDomeRadius)
		<< ":" << static_cast<int>(m_stage7LowerShellRadius)
		<< ":" << (skyVisible ? 1 : 0)
		<< ":" << (lowerShellVisible ? 1 : 0);
	const std::string stateKey = state.str();
	++m_stage7SkyHorizonLogCounter;
	if (forceLog || m_stage7LastSkyHorizonState != stateKey || m_stage7SkyHorizonLogCounter <= 3 || (m_stage7SkyHorizonLogCounter % 120) == 0)
	{
		std::cout << "[Stage7 3DSkyGround]"
			<< " reason=" << (reason != nullptr ? reason : "unknown")
			<< " enable=" << (m_enableStage7SkyHorizon ? "1" : "0")
			<< " band=" << IRBandName(environment.band)
			<< " envTerrain=" << envTerrain
			<< " envSky=" << envSky
			<< " skyGrayRaw=" << skyGrayRaw
			<< " groundGrayRaw=" << groundGrayRaw
			<< " skyGrayFinal=" << skyGrayFinal
			<< " groundGrayFinal=" << groundGrayFinal
			<< " skyGray=" << skyGrayFinal
			<< " groundGray=" << groundGrayFinal
			<< " skyDome=" << (!m_skyNode.is_empty() ? "1" : "0")
			<< " lowerShell=" << (!m_stage7LowerShellNode.is_empty() ? "1" : "0")
			<< " groundPlane=0"
			<< " source=env+band_default"
			<< std::endl;
		std::cout << "[Stage7 Debug]"
			<< " mode=" << m_stage7DebugModeName
			<< " skyVisible=" << (skyVisible ? "1" : "0")
			<< " groundVisible=" << (lowerShellVisible ? "1" : "0")
			<< " forcedSkyGray=" << (m_stage7DebugMode == 3 ? "0.65" : "NA")
			<< " forcedGroundGray=" << (m_stage7DebugMode == 3 ? "0.35" : "NA")
			<< std::endl;
		std::cout << "[Stage7 Real3DBackground]"
			<< " skyDome=" << (!m_skyNode.is_empty() ? "1" : "0")
			<< " lowerShell=" << (!m_stage7LowerShellNode.is_empty() ? "1" : "0")
			<< " flatGroundPlane=0"
			<< " cameraPos=" << (m_cameraNode.is_empty() ? 0.0f : m_cameraNode.get_pos(m_renderRoot)[0])
			<< "," << (m_cameraNode.is_empty() ? 0.0f : m_cameraNode.get_pos(m_renderRoot)[1])
			<< "," << (m_cameraNode.is_empty() ? 0.0f : m_cameraNode.get_pos(m_renderRoot)[2])
			<< " backgroundMode=real_3d"
			<< std::endl;
		std::cout << "[Stage7 GroundReference]"
			<< " mode=reference_zero"
			<< " groundZOffset=" << m_stage7GroundZOffset
			<< " finalGroundReferenceZ=" << groundReferenceZ
			<< std::endl;
		const LPoint3f cameraPos = m_cameraNode.is_empty()
			? LPoint3f(0.0f, 0.0f, 0.0f)
			: m_cameraNode.get_pos(m_renderRoot);
		const LPoint3f skyDomePos = m_skyNode.is_empty()
			? LPoint3f(0.0f, 0.0f, 0.0f)
			: m_skyNode.get_pos(m_renderRoot);
		std::cout << "[Stage7 SkyDomeDiag]"
			<< " cameraPos=" << cameraPos[0] << "," << cameraPos[1] << "," << cameraPos[2]
			<< " skyDomePos=" << skyDomePos[0] << "," << skyDomePos[1] << "," << skyDomePos[2]
			<< " farClipM=" << farClipM
			<< " skyRadius=" << m_stage7SkyDomeRadius
			<< " radius=" << m_stage7SkyDomeRadius
			<< " radiusSource=farClip_scaled"
			<< " radiusLessThanFarClip=" << (radiusLessThanFarClip ? "1" : "0")
			<< " twoSided=1"
			<< " depthWrite=0"
			<< " depthTest=0"
			<< std::endl;
		const LPoint3f groundPos = m_stage7LowerShellNode.is_empty()
			? LPoint3f(0.0f, 0.0f, static_cast<float>(groundReferenceZ))
			: m_stage7LowerShellNode.get_pos(m_renderRoot);
		std::cout << "[Stage7 GroundDiag]"
			<< " groundPos=" << groundPos[0] << "," << groundPos[1] << "," << groundPos[2]
			<< " farClipM=" << farClipM
			<< " groundSize=" << m_stage7LowerShellRadius
			<< " lowerShellRadius=" << m_stage7LowerShellRadius
			<< " groundZOffset=" << m_stage7GroundZOffset
			<< " envTerrain=" << envTerrain
			<< " groundGray=" << groundGrayFinal
			<< std::endl;
		if (m_stage7DebugMode == 3 && (!skyVisible || !radiusLessThanFarClip))
		{
			std::cout << "STAGE7_SKY_DOME_NOT_VISIBLE_AFTER_RADIUS_FIX"
				<< " skyVisible=" << (skyVisible ? "1" : "0")
				<< " farClipM=" << farClipM
				<< " skyRadius=" << m_stage7SkyDomeRadius
				<< std::endl;
		}
		m_stage7LastSkyHorizonState = stateKey;
	}
}

void HwaSimIR::LogStage6FrameDiag(const BYHWICD::DisplayC2cObjTrackingData& currentData, int targetMappedCount, int targetVisibleCount, int hiddenByTargetNum, int hiddenByTargetViewValid, int hiddenByWeaponViewValid, int beyondFarClipCount)
{
	++m_stage6FrameDiagLogCounter;
	if (targetVisibleCount <= 0)
	{
		++m_stage6NoVisibleTargetFrames;
	}
	else
	{
		m_stage6NoVisibleTargetFrames = 0;
	}

	const IRSensorPostProcessConfig displayConfig = m_stage6DisplayConfigReady ? m_stage6DisplayConfig : IRSensorPostProcessConfig();
	const char* route = "final_sensor";
	const bool skyVisible = !m_skyNode.is_empty();
	LVecBase3f cameraHpr = m_cameraNode.is_empty() ? LVecBase3f(0.0f, 0.0f, 0.0f) : m_cameraNode.get_hpr();
	const double sensorFovH = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.horizontalFovDeg : 0.0;
	const double sensorFovV = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.verticalFovDeg : 0.0;
	const double nearClip = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.nearClipM : 0.0;
	const double farClip = m_sensorDisplayConfigReady ? m_sensorDisplayConfig.farClipM : 0.0;
	const int protocolBand = (m_sensorParam.trackerSensorBand >= 0 && m_sensorParam.trackerSensorBand <= 4)
		? m_sensorParam.trackerSensorBand : 2;
	const IRBand band = IRBandFromProtocol(protocolBand);

	std::ostringstream state;
	state << targetMappedCount
		<< ":" << targetVisibleCount
		<< ":" << hiddenByTargetNum
		<< ":" << hiddenByTargetViewValid
		<< ":" << hiddenByWeaponViewValid
		<< ":" << beyondFarClipCount
		<< ":" << (skyVisible ? 1 : 0)
		<< ":" << route
		<< ":" << (displayConfig.whiteHot ? 1 : 0)
		<< ":" << static_cast<int>(displayConfig.displayGain * 100.0)
		<< ":" << static_cast<int>(displayConfig.displayOffset)
		<< ":" << (displayConfig.noiseEnable ? 1 : 0)
		<< ":" << static_cast<int>(displayConfig.noiseSigmaGray * 100.0);
	const std::string stateKey = state.str();
	const bool shouldLog = m_enableIRVerboseLog ||
		(m_stage6FrameDiagLogCounter <= 3) ||
		((m_stage6FrameDiagLogCounter % 120) == 0) ||
		(m_stage6LastFrameDiagState != stateKey);
	if (shouldLog)
	{
		std::cout << "[Stage6 FrameDiag]"
			<< " frameIndex=" << currentData.time
			<< " packet=" << m_stage0DisplayFrameCount
			<< " band=" << IRBandName(band)
			<< " whiteHot=" << (displayConfig.whiteHot ? "1" : "0")
			<< " displayGain=" << displayConfig.displayGain
			<< " displayOffset=" << displayConfig.displayOffset
			<< " noiseEnable=" << (displayConfig.noiseEnable ? "1" : "0")
			<< " noiseSigmaGray=" << displayConfig.noiseSigmaGray
			<< " route=" << route
			<< " targetVisibleCount=" << targetVisibleCount
			<< " targetMappedCount=" << targetMappedCount
			<< " hiddenByTargetNum=" << hiddenByTargetNum
			<< " hiddenByTargetViewValid=" << hiddenByTargetViewValid
			<< " hiddenByWeaponViewValid=" << hiddenByWeaponViewValid
			<< " beyondFarClip=" << beyondFarClipCount
			<< " skyVisible=" << (skyVisible ? "1" : "0")
			<< " cameraHpr=" << cameraHpr[0] << "," << cameraHpr[1] << "," << cameraHpr[2]
			<< " sensorFovH=" << sensorFovH
			<< " sensorFovV=" << sensorFovV
			<< " near=" << nearClip
			<< " far=" << farClip
			<< std::endl;
		m_stage6LastFrameDiagState = stateKey;
	}

	if (m_stage6NoVisibleTargetFrames >= 5 &&
		(m_stage6NoVisibleTargetFrames == 5 || (m_stage6NoVisibleTargetFrames % 60) == 0))
	{
		std::cout << "[Stage6 FrameDiag][WARN] NO_VISIBLE_TARGETS"
			<< " consecutiveFrames=" << m_stage6NoVisibleTargetFrames
			<< " targetMappedCount=" << targetMappedCount
			<< " hiddenByTargetNum=" << hiddenByTargetNum
			<< " hiddenByTargetViewValid=" << hiddenByTargetViewValid
			<< " hiddenByWeaponViewValid=" << hiddenByWeaponViewValid
			<< " beyondFarClip=" << beyondFarClipCount
			<< std::endl;
		if (skyVisible)
		{
			std::cout << "[Stage6 FrameDiag][WARN] BACKGROUND_ONLY_FRAME"
				<< " consecutiveFrames=" << m_stage6NoVisibleTargetFrames
				<< " skyVisible=1"
				<< " targetMappedCount=" << targetMappedCount
				<< std::endl;
		}
	}
}

bool HwaSimIR::ResolveAnnotationOutputSize(int& width, int& height) const
{
	width = 0;
	height = 0;

	if (m_stage6FinalWidth > 0 && m_stage6FinalHeight > 0)
	{
		width = m_stage6FinalWidth;
		height = m_stage6FinalHeight;
		return true;
	}

	if (m_sensorDisplayConfigReady && m_sensorDisplayConfig.width > 0 && m_sensorDisplayConfig.height > 0)
	{
		width = m_sensorDisplayConfig.width;
		height = m_sensorDisplayConfig.height;
		return true;
	}

	if (m_renderTex != nullptr && m_renderTex->get_x_size() > 0 && m_renderTex->get_y_size() > 0)
	{
		width = m_renderTex->get_x_size();
		height = m_renderTex->get_y_size();
		return true;
	}

	if (m_pGraphicsWindow != nullptr)
	{
		const WindowProperties props = m_pGraphicsWindow->get_properties();
		if (props.get_x_size() > 0 && props.get_y_size() > 0)
		{
			width = props.get_x_size();
			height = props.get_y_size();
			return true;
		}
	}

	return false;
}

void HwaSimIR::RefreshAnnotationOverlay(const BYHWICD::DisplayC2cObjTrackingData& currentData)
{
	if (!m_sensorParam.realtimeAnnotation)
	{
		if (m_annotationManager.isEnabled())
		{
			m_annotationManager.setEnabled(false);
		}
		return;
	}

	if (!m_annotationManager.isEnabled())
	{
		m_annotationManager.setEnabled(true);
	}

	int outputWidth = 0;
	int outputHeight = 0;
	if (!ResolveAnnotationOutputSize(outputWidth, outputHeight))
	{
		m_annotationManager.clear();
		return;
	}

	// 标注像素坐标以最终显示/输出图像左上角为原点；TCP 内部 flip 不在 Stage1 修改。
	const std::uint64_t sourceSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq
		: m_stage0DisplayFrameCount;
	const double targetFps = std::max(1.0, static_cast<double>(m_targetVideoFps.load()));
	const std::uint64_t updateStride = static_cast<std::uint64_t>(
		std::max(1.0, std::floor(targetFps / std::max(1.0, m_annotationUpdateHz) + 0.5)));
	const bool updateProjection = sourceSeq <= 3 ||
		m_annotationLastProjectionSourceSeq == 0 ||
		sourceSeq >= m_annotationLastProjectionSourceSeq + updateStride;
	if (updateProjection)
	{
		m_annotationManager.updateFrame(
			sourceSeq,
			currentData.time,
			currentData.sensorID,
			outputWidth,
			outputHeight,
			m_targetPlatformList,
			m_renderRoot,
			m_cameraNode,
			m_cameraLens);
		m_annotationLastProjectionSourceSeq = sourceSeq;
	}
	else
	{
		m_annotationManager.reuseFrameMetadata(
			sourceSeq,
			currentData.time,
			currentData.sensorID,
			outputWidth,
			outputHeight);
	}
}

// 窗口UI初始化（VisibleWindow专用）
void HwaSimIR::InitVisibleWindowUi() {
	if (!m_pMainWindow || !m_pGraphicsWindow)
	{
		return;
	}
	m_pMainWindow->enable_keyboard();       // 启用键盘输入
	//m_pMainWindow->setup_trackball();       // 启用鼠标轨迹球（视角操控）
	m_pMainWindow->set_background_type(WindowFramework::BT_gray); // 灰色背景
	//m_pFramework->enable_default_keys();     // 启用默认按键（ESC退出）
	//m_pMainWindow->enable_keyboard();





	//帧率显示器
	if (m_renderEnableFrameRateMeter)
	{
		PT(FrameRateMeter) meter;
		meter = new FrameRateMeter("frame_rate_meter");
		meter->setup_window(m_pGraphicsWindow);
	}


	// 获取全局事件处理器（根据 eventHandler.h）
	//EventHandler* eh = EventHandler::get_global_event_handler();
	//eh->add_hook("keystroke_escape", &HwaSimIR::on_key_event, this);
	//eh->add_hook("keystroke_space", &HwaSimIR::on_key_event, this);

	m_pFramework->define_key("w", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("s", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("a", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("d", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_up", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_down", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_left", "Move forward", &HwaSimIR::on_key_event, this);
	m_pFramework->define_key("arrow_right", "Move forward", &HwaSimIR::on_key_event, this);


}

void HwaSimIR::InitCommonCaptureTask() {
	m_renderTex = new Texture("Stage6FinalSensorTex");
	if (IsVisibleWindowMode())
	{
		if (m_pGraphicsWindow != nullptr)
		{
			m_pGraphicsWindow->add_render_texture(m_renderTex, GraphicsOutput::RTM_copy_ram);
		}
		else
		{
			std::cout << "[RenderBackend][WARN]"
				<< " mode=" << m_renderPresentationModeName
				<< " captureTask=Stage6FinalSensorTex"
				<< " failure=graphics_window_unavailable"
				<< std::endl;
		}
	}
	else
	{
		m_renderTex->setup_2d_texture(
			std::max(1, m_headlessWidth),
			std::max(1, m_headlessHeight),
			Texture::T_unsigned_byte,
			Texture::F_rgb);
		std::cout << "[RenderBackend][TODO]"
			<< " mode=" << m_renderPresentationModeName
			<< " captureTask=Stage6FinalSensorTex"
			<< " headless_final_pipeline_not_ready=1"
			<< std::endl;
	}

	// 向引擎全局任务管理器添加一个捕获任务，保证在主线程安全运行
	PT(GenericAsyncTask) cap_task = new GenericAsyncTask("CaptureTask", &HwaSimIR::capture_task, this);
	AsyncTaskManager::get_global_ptr()->add(cap_task);

}

void HwaSimIR::InitPlatformModels()
{
	// 清空原有路径映射
	m_platformResMap.clear();

	// 初始化各平台的模型路径和纹理路径
	// 飞机：协议 0x11 当前仍默认绑定 F35；F22 已入库但暂未接协议枚举，避免改变现有测试语义。
	m_platformResMap[F35] = {
		"Config/TargetLib/models/f35/F35C.obj",
		"Config/TargetLib/models/f35/f35c.jpg",
		"Config/TargetLib/models/f35/f35c_mat.tif",
		"Config/TargetLib/models/f35/f35c_mat.tif.xml",
		"Config/TargetLib/models/f35",
		"F35",
		"BM_METAL-ALUMINIUM"
	};

	// 导弹：资源目录使用新加入的 AIM9X/AIM120D 资产，保持协议类型 0x33/0x22 不变。
	m_platformResMap[AIM9] = {
		"Config/TargetLib/models/aim9x/aim9x.obj",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse.png",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse_mat.tif",
		"Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse_mat.tif.xml",
		"Config/TargetLib/models/aim9x",
		"AIM9X",
		"IR_CERAMIC"
	};
	m_platformResMap[AIM120] = {
		"Config/TargetLib/models/aim120/AIM120.obj",
		"Config/TargetLib/models/aim120/aim120.jpg",
		"Config/TargetLib/models/aim120/aim120_mat.tif",
		"Config/TargetLib/models/aim120/aim120_mat.tif.xml",
		"Config/TargetLib/models/aim120",
		"AIM120D",
		"BM_METAL-STEEL"
	};

	std::cout << "平台模型路径初始化完成，共加载" << m_platformResMap.size() << "种平台资源" << std::endl;

	if (IsHeadlessOffscreenMode())
	{
		std::cout << "[RenderBackend][TODO]"
			<< " mode=" << m_renderPresentationModeName
			<< " platformModelPaths=" << m_platformResMap.size()
			<< " modelRenderHost=headless_final_pipeline_not_ready"
			<< std::endl;
		m_camera = nullptr;
		m_cameraLens = nullptr;
		return;
	}

	if (!m_pMainWindow || m_renderRoot.is_empty())
	{
		std::cout << "[RenderBackend][WARN]"
			<< " mode=" << m_renderPresentationModeName
			<< " platformModelPaths=" << m_platformResMap.size()
			<< " failure=visible_model_host_unavailable"
			<< std::endl;
		return;
	}


	// 旧的单模型调试代码保留为参考，路径已迁移到新的 AIM9X 资产目录。
#if 0

	Filename aim9_path = Filename::from_os_specific("Config/TargetLib/models/aim9x/aim9x.obj");
	Filename aim9_texture_path = Filename::from_os_specific("Config/TargetLib/models/aim9x/TX_AIM9X_Diffuse.png");

	aim9 = m_pMainWindow->load_model(m_renderRoot, aim9_path);
	PT(Texture) textureaim9 = TexturePool::load_texture(aim9_texture_path);
	if (textureaim9 != nullptr)
	{
		aim9.set_texture(textureaim9);
	}
	aim9.set_pos(1000, 0, 0);
	aim9.set_hpr(0, 0, 0);

#endif // 0

	//aim9.set_scale(0.1f, 0.1f, 0.1f);


	//m_camera = m_pMainWindow->get_camera(1);J20 textureJ20
	//m_cameraNode.set_pos(0, -20, 10);      // (x, y, z)
	//m_cameraNode.look_at(LPoint3f(0, 0, 0)); // 朝向原点
	//m_cameraNode.set_hpr(0, 0, 0);         // 可选：重置旋转


	// 获取主窗口的相机组节点
	m_cameraNode = m_pMainWindow->get_camera_group();
	//m_cameraNode.reparent_to(m_pMainWindow->get_render());
	m_cameraNode.reparent_to(m_renderRoot);
	//LPoint3 x = (0, 0, 0);
	//int num = m_pMainWindow->get_num_cameras();
	//std::cout << "get_num_cameras" << num << std::endl;
	m_cameraNode.set_pos(0,0,0); // 往后15单位，往上8单位
	m_cameraNode.set_hpr(0,0,0); 			 // 让相机始终看向平台模型中心
	//m_cameraNode.look_at(aim9);

	m_camera = m_pMainWindow->get_camera(0);
	m_cameraLens = m_camera != nullptr ? m_camera->get_lens() : nullptr;
	if (m_cameraLens != nullptr)
	{
		m_cameraLens->set_fov(0.1, 0.1);
		m_cameraLens->set_near_far(1.0, 100000.0);
	}


}

void HwaSimIR::ProcessRealSimSceneInitData()
{
	// 确保平台模型路径已初始化
	if (m_platformResMap.empty())
	{
		InitPlatformModels();
	}

	// 缓存传感器参数
	m_sensorParam = m_initSceneData.trackingInit.trackerSensor[0];
	std::cout << "传感器参数初始化完成，传感器ID=" << m_initSceneData.sensorID << std::endl;
	const IRSensorProfile& sensorProfile = m_irSensorProfiles.profileForProtocolBand(m_sensorParam.trackerSensorBand);
	int sensorWidth = m_sensorParam.trackerSensorWidth;
	int sensorHeight = m_sensorParam.trackerSensorHeight;
	double sensorPixelAngleUrad = m_sensorParam.trackerSensorPixelAngle;
	bool sensorWaveWidthFallback = false;
	bool sensorWaveHeightFallback = false;
	bool sensorWavePixelAngleFallback = false;
	if (sensorWidth <= 0 && sensorProfile.width > 0)
	{
		sensorWidth = sensorProfile.width;
		sensorWaveWidthFallback = true;
	}
	if (sensorHeight <= 0 && sensorProfile.height > 0)
	{
		sensorHeight = sensorProfile.height;
		sensorWaveHeightFallback = true;
	}
	if (sensorPixelAngleUrad <= 0.0 && sensorProfile.fovHDeg > 0.0 && sensorWidth > 0)
	{
		const double pi = 3.14159265358979323846;
		const double fovRad = sensorProfile.fovHDeg * pi / 180.0;
		const double pixelAngleRad = 2.0 * std::atan(std::tan(fovRad * 0.5) / static_cast<double>(sensorWidth));
		sensorPixelAngleUrad = pixelAngleRad * 1.0e6;
		sensorWavePixelAngleFallback = true;
	}
	std::string sensorWidthFallbackSource;
	std::string sensorHeightFallbackSource;
	std::string sensorPixelAngleFallbackSource;
	if (sensorWidth <= 0)
	{
		sensorWidth = m_runtimeConfig.getInt("SensorFallback", "Width", "Stage6FallbackSensorWidth", 800, &sensorWidthFallbackSource);
	}
	if (sensorHeight <= 0)
	{
		sensorHeight = m_runtimeConfig.getInt("SensorFallback", "Height", "Stage6FallbackSensorHeight", 800, &sensorHeightFallbackSource);
	}
	if (sensorPixelAngleUrad <= 0.0)
	{
		sensorPixelAngleUrad = m_runtimeConfig.getDouble("SensorFallback", "PixelAngleUrad", "Stage6FallbackPixelAngleUrad", 20.0, &sensorPixelAngleFallbackSource);
	}
	if (sensorWaveWidthFallback || sensorWaveHeightFallback || sensorWavePixelAngleFallback)
	{
		std::cout << "[SensorWave Usage]"
			<< " band=" << IRBandName(IRBandFromProtocol(m_sensorParam.trackerSensorBand))
			<< " file=" << sensorProfile.sourcePath
			<< " fallbackApplied=1"
			<< " widthFromSensorWave=" << (sensorWaveWidthFallback ? "1" : "0")
			<< " heightFromSensorWave=" << (sensorWaveHeightFallback ? "1" : "0")
			<< " pixelAngleFromFOVH=" << (sensorWavePixelAngleFallback ? "1" : "0")
			<< " priority=UDP_init>SensorWave>RuntimeConfig>default"
			<< std::endl;
	}
	if (!sensorWidthFallbackSource.empty() || !sensorHeightFallbackSource.empty() || !sensorPixelAngleFallbackSource.empty())
	{
		std::cout << "[RuntimeConfig] SensorFallback"
			<< " widthSource=" << (sensorWidthFallbackSource.empty() ? "not_used" : sensorWidthFallbackSource)
			<< " heightSource=" << (sensorHeightFallbackSource.empty() ? "not_used" : sensorHeightFallbackSource)
			<< " pixelAngleSource=" << (sensorPixelAngleFallbackSource.empty() ? "not_used" : sensorPixelAngleFallbackSource)
			<< " priority=UDP_init>SensorWave>RuntimeConfig>default"
			<< std::endl;
	}
	IRSensorDisplayConfig sensorDisplayConfig = m_irSensorModel.BuildSensorDisplayConfig(
		sensorWidth,
		sensorHeight,
		m_sensorParam.trackerSensorViewMin,
		m_sensorParam.trackerSensorViewMax,
		sensorPixelAngleUrad);
	std::cout << "[Stage0] Init render setup begin"
		<< " sensorSize=" << sensorDisplayConfig.width << "x" << sensorDisplayConfig.height
		<< std::endl;
	ApplySensorOutputConfig(sensorDisplayConfig, "init-command");
	ApplyStage6DisplayConfig(m_sensorParam, "init-command");
	std::cout << "[Stage0] Init render setup complete" << std::endl;
	// Stage1 标注跟随初始化开关；重新初始化时先清空上一回合 overlay 和内存快照。
	m_annotationManager.clear();
	m_annotationManager.setEnabled(m_sensorParam.realtimeAnnotation);

	// 设置增删标记为"增加"
	m_isAddPlatform = true;
	m_irTemperatureModel.resetRuntime();
	m_irEnginePlumeModel.resetRuntime();
	m_stage5PlumeRuntimeCache.clear();
	m_stage5PlumePerfMsTotal = 0.0;
	m_stage5PlumePerfMsMax = 0.0;
	m_stage5PlumePerfSamples = 0;
	m_lastStage4UpdateTime = -1.0;

	// 调用增删逻辑生成平台
	ProcessAddRemovePakPlatform();
	ProcessAddRemoveWeaponPlatform();
	ProcessAddRemoveTargetPlatform();
	m_stage6FrameDiagLogCounter = 0;
	m_stage6NoVisibleTargetFrames = 0;
	m_stage6LastFrameDiagState.clear();
	m_stage6MtfLogCounter = 0;
	m_lastStage6MtfLogState.clear();
	m_stage7NearFarClipWarningLogged = false;
	m_stage7SkyHorizonLogCounter = 0;
	m_stage7LastSkyHorizonState.clear();
	UpdateStage7SkyHorizon(BuildRuntimeEnvironment(), "init-command", true);
	UpdateStage7WeatherNodes(m_stage7WeatherState, ClockObject::get_global_clock()->get_frame_time());
	BYHWICD::SpatialState spatial;
	spatial = m_initSceneData.platParam[0].spatial;
	m_geoTrans.InitReferencePoint(spatial.lat, spatial.lon, spatial.alt);

	std::cout << "初始化仿真中心原点：ID=" << m_initSceneData.platParam[0].id
		<< " 位置(" << spatial.lat << "," << spatial.lon << "," << spatial.alt << ")" << std::endl;

	std::cout << "成像初始化完成：军别=" << m_initSceneData.JB
		<< " 挂载平台ID=" << m_initSceneData.platID
		<< " 仿真回合=" << m_currentRound << std::endl;
}

void HwaSimIR::ProcessRealSimSceneDrivenData()
{
	// 仿真未运行时不处理
	if (!m_isSimRunning.load())
	{
		return;
	}

	// ================= 新增：局部拷贝当前帧数据，防止在计算过程中被 UDP 线程覆盖 =================
	BYHWICD::DisplayC2cObjTrackingData currentData;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		currentData = m_realTimeSceneData;
	}
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;

	// 更新PlatParamPak平台（核心：platLoc映射到飞机平台）
	for (auto& pakPlat : m_pakPlatformList)
	{
		if (!pakPlat.isExist || pakPlat.platID != currentData.platID) continue;

		// 更新平台位置/姿态（从实时数据的platLoc读取）
		const BYHWICD::SpatialState& platSpatial = currentData.platLoc;


		/*double px, py, pz;
		m_geoTrans.Wgs84ToPandaXYZ(platSpatial, px, py, pz);
		pakPlat.nodePath.set_pos(px, py, pz);*/

		LMatrix4f exactTransform = m_geoTrans.GetPandaMatrix(platSpatial);

		pakPlat.nodePath.set_mat(LMatrix4(exactTransform));


		//pakPlat.nodePath.set_pos(platSpatial.lat, platSpatial.lon, platSpatial.alt);
		//pakPlat.nodePath.set_hpr(-platSpatial.yaw, platSpatial.pitch, platSpatial.roll);


		// 更新协议参数缓存
		pakPlat.platParam.spatial = platSpatial;
		/*std::cout << "更新PlatParamPak平台位置：ID=" << pakPlat.platID
			<< " 位置(" << platSpatial.lat << "," << platSpatial.lon << "," << platSpatial.alt << ")" 
			<< " 姿态(" << -platSpatial.yaw << "," << platSpatial.pitch << "," << platSpatial.roll << ")" << std::endl;*/
		/*std::cout << "更新PlatParamPak平台转换后位置：ID=" << pakPlat.platID
			<< " 位置(" << px << "," << py << "," << pz << ")"
			<< " 姿态(" << -platSpatial.yaw << "," << platSpatial.pitch << "," << platSpatial.roll << ")" << std::endl;*/
	}

	// 更新WeaponState平台
	for (auto& weaponPlat : m_weaponPlatformList)
	{
		if (!weaponPlat.isExist || weaponPlat.platID != currentData.platID) continue;

		// 更新武器状态
		weaponPlat.weaponState = currentData.weaponState;
		const bool weaponStateChanged = m_lastWeaponDamageFlag != weaponPlat.weaponState.damageFlag;
		if (frameSeq <= 3 || (frameSeq % 120) == 0 || weaponStateChanged)
		{
			std::cout << "更新WeaponState平台：目标ID=" << weaponPlat.platID
				<< " 毁伤状态=" << weaponPlat.weaponState.damageFlag << std::endl;
		}
		m_lastWeaponDamageFlag = weaponPlat.weaponState.damageFlag;
	}

	// TargetState[5] 最多携带5组目标数据；targetNumValid只控制前N个是否参与显示，不再限制状态更新。
	const int visibleTargetNum = std::max(0, std::min(currentData.targetNumValid, 5));
	int targetMappedCount = 0;
	int targetVisibleCount = 0;
	int hiddenByTargetNum = 0;
	int hiddenByTargetViewValid = 0;
	int hiddenByWeaponViewValid = 0;
	int beyondFarClipCount = 0;
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (targetPlat.isExist)
		{
			targetPlat.nodePath.hide();
		}
	}

	TargetPlatformData* lookAtTarget = nullptr;
	for (int i = 0; i < 5; ++i)
	{
		const BYHWICD::TargetState& targetState = currentData.targetState[i];
		if (!IsValidTargetStateKey(targetState))
		{
			continue;
		}

		TargetPlatformData* targetPlat = FindOrMapTargetPlatform(targetState, i);
		if (targetPlat == nullptr)
		{
			continue;
		}
		++targetMappedCount;

		// 通过 targetType + targetPlatID + targetID 唯一键更新同一个目标，避免不同挂载平台目标ID冲突。
		targetPlat->targetState = targetState;
		targetPlat->platID = targetState.targetID;

		const BYHWICD::SpatialState& spatial = targetState.targetLoc;
		LMatrix4f exactTransform = m_geoTrans.GetPandaMatrix(spatial);
		targetPlat->nodePath.set_mat(LMatrix4(exactTransform));
		const float targetRangeM = EstimateRangeToCamera(targetPlat->nodePath);
		const bool beyondFarClip = m_sensorDisplayConfigReady &&
			targetRangeM > static_cast<float>(m_sensorDisplayConfig.farClipM);
		if (beyondFarClip)
		{
			++beyondFarClipCount;
		}
		if (beyondFarClip && !m_stage6FarClipWarningLogged)
		{
			std::cout << "STAGE6_TARGET_BEYOND_FAR_CLIP"
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< " rangeM=" << targetRangeM
				<< " farClipM=" << m_sensorDisplayConfig.farClipM
				<< std::endl;
			m_stage6FarClipWarningLogged = true;
		}

		const bool visibleByTargetNum = i < visibleTargetNum;
		const bool hiddenByTargetNumNow = !visibleByTargetNum;
		const bool hiddenByTargetViewValidNow = !targetState.viewValid;
		const bool weaponMatchesTarget = WeaponTargetKeyMatches(currentData.weaponState, *targetPlat);
		const bool hiddenByWeaponViewValidNow = weaponMatchesTarget && !currentData.weaponState.viewValid;
		if (hiddenByTargetNumNow)
		{
			++hiddenByTargetNum;
		}
		if (hiddenByTargetViewValidNow)
		{
			++hiddenByTargetViewValid;
		}
		if (hiddenByWeaponViewValidNow)
		{
			++hiddenByWeaponViewValid;
		}

		bool renderVisible = visibleByTargetNum && targetState.viewValid;
		if (hiddenByWeaponViewValidNow)
		{
			renderVisible = false;
		}
		if (renderVisible && !beyondFarClip)
		{
			++targetVisibleCount;
		}
		LogAeroSpeedState(*targetPlat, renderVisible && !beyondFarClip);
		const bool nearFarClip = renderVisible &&
			!beyondFarClip &&
			m_sensorDisplayConfigReady &&
			targetRangeM > static_cast<float>(m_sensorDisplayConfig.farClipM * 0.9);
		if (nearFarClip && !m_stage7NearFarClipWarningLogged)
		{
			std::cout << "STAGE7_TARGET_NEAR_FAR_CLIP"
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< " rangeM=" << targetRangeM
				<< " farClipM=" << m_sensorDisplayConfig.farClipM
				<< " ratio=" << (targetRangeM / static_cast<float>(m_sensorDisplayConfig.farClipM))
				<< std::endl;
			m_stage7NearFarClipWarningLogged = true;
		}
		if (renderVisible)
		{
			targetPlat->nodePath.show();
		}
		else
		{
			targetPlat->nodePath.hide();
			HideEnginePlume(*targetPlat);
			if ((!targetPlat->enginePlumeCoreNodePath.is_empty() || !targetPlat->enginePlumeHaloNodePath.is_empty()) &&
				(m_stage5PlumeOptions.enablePlumeDebug || m_stage5PlumeOptions.forcePlumeVisible ||
					frameSeq <= 3 || (frameSeq % 120) == 0))
			{
				const IRBand plumeBand = BuildRuntimeEnvironment().band;
				std::cout << "[Stage5 Plume]"
					<< " targetType=0x" << std::hex << targetState.targetType << std::dec
					<< " targetPlatID=" << targetState.targetPlatID
					<< " targetID=" << targetState.targetID
					<< " platform=" << Stage4PlatformName(targetPlat->type)
					<< " engineState=" << (targetState.engineState ? "1" : "0")
					<< " band=" << IRBandName(plumeBand)
					<< " coreEnabled=0"
					<< " haloEnabled=0"
					<< " coreTempK=0"
					<< " haloTempK=0"
					<< " coreGray=0"
					<< " haloGray=0"
					<< " coreOpacity=0"
					<< " haloOpacity=0"
					<< " coreVisible=0"
					<< " haloVisible=0"
					<< " reason=target_not_renderable"
					<< std::endl;
			}
		}

		if (weaponMatchesTarget)
		{
			lookAtTarget = targetPlat;
		}

		if (frameSeq <= 3 || (frameSeq % 120) == 0)
		{
			std::cout << "[TargetMapping]"
				<< " packet=" << frameSeq
				<< " index=" << i
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< " visibleByTargetNum=" << (visibleByTargetNum ? "1" : "0")
				<< " targetViewValid=" << (targetState.viewValid ? "1" : "0")
				<< " weaponViewValid=" << (currentData.weaponState.viewValid ? "1" : "0")
				<< " beyondFarClip=" << (beyondFarClip ? "1" : "0")
				<< " renderVisible=" << (renderVisible ? "1" : "0")
				<< std::endl;
		}
	}
	m_isInitTargetPlatID = true;

	ApplyWeaponCameraControl(currentData, lookAtTarget);
	LogStage6FrameDiag(currentData, targetMappedCount, targetVisibleCount, hiddenByTargetNum, hiddenByTargetViewValid, hiddenByWeaponViewValid, beyondFarClipCount);
	const auto annotationBegin = std::chrono::steady_clock::now();
	RefreshAnnotationOverlay(currentData);
	m_perfStats.recordAnnotation(std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - annotationBegin).count());

	//for (int i = 0; i < validTargetNum; ++i)
	//{
	//	const BYHWICD::TargetState& targetState = currentData.targetState[i];
	//	for (auto& targetPlat : m_targetPlatformList)
	//	{
	//		PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
	//		if (!targetPlat.isExist || targetPlat.platID != targetState.targetID || targetPlat.type != platType) continue;

	//		// 更新目标状态和位置
	//		targetPlat.targetState = targetState;
	//		const BYHWICD::SpatialState& spatial = targetState.targetLoc;


	//		double px, py, pz;
	//		m_geoTrans.Wgs84ToPandaXYZ(spatial, px, py, pz);
	//		targetPlat.nodePath.set_pos(px, py, pz);


	//		//targetPlat.nodePath.set_pos(spatial.lat, spatial.lon, spatial.alt);
	//		targetPlat.nodePath.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);
	//		m_cameraNode.look_at(targetPlat.nodePath);

	//		
	//		LVecBase3 hpr = m_cameraNode.get_hpr();
	//		std::cout << " 获取传感器姿态(" << hpr[0] << "," << hpr[1] << "," << hpr[2] << ")" << std::endl;


	//		std::cout << "更新TargetState平台：目标ID=" << targetState.targetID
	//			<< " 位置(" << spatial.lat << "," << spatial.lon << "," << spatial.alt << ")"  
	//			<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;
	//		std::cout << "更新TargetState平台转换后位置：ID=" << targetState.targetID
	//			<< " 位置(" << px << "," << py << "," << pz << ")"
	//			<< " 姿态(" << -spatial.yaw << "," << spatial.pitch << "," << spatial.roll << ")" << std::endl;
	//	}
	//}

	if (m_enableIRVerboseLog ||
		m_stage0DisplayFrameCount <= 3 ||
		(m_stage0DisplayFrameCount % 120) == 0)
	{
		std::cout << "实时数据更新完成，时间戳=" << currentData.time << "ms" << std::endl;
	}
}


PLATFORM_TYPE HwaSimIR::TargetTypeToPlatformType(int targetType) const
{
	switch (targetType)
	{
	case 0x00: return NONE;
	case 0x11: return F35; // 飞机类型暂时默认F35
	case 0x22: return AIM120;
	case 0x33: return AIM9;
	case 0x44: return MMD;
	default: return NONE;
	}
}

NodePath HwaSimIR::LoadPlatformAssetNode(PLATFORM_TYPE type, const PlatformResPath& res)
{
	if (!m_pMainWindow || m_renderRoot.is_empty())
	{
		std::cout << "[RenderBackend][TODO]"
			<< " mode=" << m_renderPresentationModeName
			<< " platformType=" << type
			<< " modelPath=" << res.modelPath
			<< " modelRenderHost=headless_final_pipeline_not_ready"
			<< std::endl;
		return NodePath();
	}

	Filename modelPath = Filename::from_os_specific(res.modelPath);
	NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, modelPath);
	if (modelNode.is_empty())
	{
		std::cerr << "模型加载失败：" << res.modelPath << " 类型=" << type << std::endl;
		return modelNode;
	}

	// OBJ/MTL 可能带有旧机器的绝对贴图路径，这里显式绑定基础纹理保证 Windows 和 RK3588 路径一致。
	if (!res.texturePath.empty())
	{
		Filename texturePath = Filename::from_os_specific(res.texturePath);
		PT(Texture) texture = TexturePool::load_texture(texturePath);
		if (texture != nullptr)
		{
			modelNode.set_texture(texture);
		}
		else
		{
			std::cerr << "基础纹理加载失败：" << res.texturePath << " 类型=" << type << std::endl;
		}
	}

	// 先挂红外 shader，再绑定材质 ID 纹理和材质参数数组，确保 shader input 已声明可用。
	ApplyInfraredShader(modelNode, false);
	m_irSceneMaterialMapper.bindPlatformNode(modelNode, res, m_irMaterialDatabase);
	return modelNode;
}

// 处理PlatParamPak平台增删（飞机平台）
void HwaSimIR::ProcessAddRemovePakPlatform()
{
	if (m_isAddPlatform)
	{
		// 增加PlatParamPak平台（飞机）
		std::cout << "开始生成PlatParamPak平台，有效数：" << m_initSceneData.platNumValid << std::endl;

		for (int i = 0; i < m_initSceneData.platNumValid; ++i)
		{
			if (i >= 2) break; // 协议中platParam最多2个

			const BYHWICD::PlatParamPak& platParam = m_initSceneData.platParam[i];
			// 根据飞机编号映射平台类型
			PLATFORM_TYPE platType = TargetTypeToPlatformType(platParam.type);
			if (platType == NONE)
			{
				std::cerr << "无效的飞机编号：" << platParam.id << "，跳过生成" << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end())
			{
				std::cerr << "未找到平台类型" << platType << "的资源路径，跳过" << std::endl;
				continue;
			}
			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty())
			{
				continue;
			}

			// 初始化PakPlatformData
			PakPlatformData newPakPlat;
			newPakPlat.type = platType;
			newPakPlat.platID = platParam.id;
			newPakPlat.platParam = platParam; // 直接拷贝协议参数
			newPakPlat.isExist = true;
			newPakPlat.nodePath = modelNode;

			// 设置初始位置/姿态（从协议SpatialState读取）
			const BYHWICD::SpatialState& spatial = platParam.spatial;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_pakPlatformList.push_back(newPakPlat);
			modelNode.show();

			std::cout << "PlatParamPak平台生成成功：类型=" << platType << " ID=" << platParam.id << std::endl;
			// ========== 绑定相机到第一个平台 ==========
			if (i==0 && !m_isCameraAttached) {
				//m_cameraNode = m_pMainWindow->get_camera_group();
				//m_camera = m_pMainWindow->get_camera();
				m_cameraNode.reparent_to(modelNode);
				m_cameraNode.set_pos(0, 0, 0); // 往后15单位，往上8单位
				//m_cameraNode.look_at(modelNode);
												 // 标记相机已绑定
				m_isCameraAttached = true;
				std::cout << "相机已绑定到第一个PlatParamPak平台（ID=" << platParam.id << "），偏移：(0, 0, -8)" << std::endl;
				if (m_sensorDisplayConfigReady) {
					std::cout << "相机视场角(Stage6 SensorGeometry)："
						<< m_sensorDisplayConfig.horizontalFovDeg << ","
						<< m_sensorDisplayConfig.verticalFovDeg << std::endl;
				}
			}
		}
	}
	else
	{
		// 删除PlatParamPak平台
		for (auto& pakPlat : m_pakPlatformList)
		{
			if (pakPlat.isExist)
			{
				pakPlat.nodePath.remove_node();
				pakPlat.isExist = false;
				std::cout << "PlatParamPak平台已删除：ID=" << pakPlat.platID << std::endl;
			}
		}
		// ========== 新增：解绑相机，恢复默认视角 ==========
		if (m_isCameraAttached && !m_cameraNode.is_empty()) {
			// 将相机重新父节点设为渲染根节点
			m_cameraNode.reparent_to(m_renderRoot);
			// 重置相机位置和姿态（恢复初始视角，可选）
			m_cameraNode.set_pos(0, 0, 0);
			m_cameraNode.set_hpr(0, 0, 0);
			// 标记相机未绑定
			m_isCameraAttached = false;
			std::cout << "相机已解绑，恢复默认视角" << std::endl;
		}
		m_pakPlatformList.clear();
	}
}

// 处理WeaponState平台增删（武器状态）
void HwaSimIR::ProcessAddRemoveWeaponPlatform()
{
#if 0

	if (m_isAddPlatform)
	{
		// 增加WeaponState平台
		const BYHWICD::WeaponState& weaponState = m_initSceneData.weaponState;
		if (weaponState.targetType == 0x00)
		{
			std::cout << "WeaponState无有效目标，跳过生成" << std::endl;
			return;
		}

		PLATFORM_TYPE platType = TargetTypeToPlatformType(weaponState.targetType);
		if (platType == NONE)
		{
			std::cerr << "无效的WeaponState目标类型：0x" << std::hex << weaponState.targetType << std::endl;
			return;
		}

		auto resIter = m_platformResMap.find(platType);
		if (resIter == m_platformResMap.end()) return;


		// 加载模型和纹理
		NodePath modelNode = m_pMainWindow->load_model(m_renderRoot, resIter->second.modelPath);
		if (modelNode.is_empty()) return;

		PT(Texture) texture = TexturePool::load_texture(resIter->second.texturePath);
		if (texture != nullptr) modelNode.set_texture(texture);

		// 初始化WeaponPlatformData
		WeaponPlatformData newWeaponPlat;
		newWeaponPlat.type = platType;
		newWeaponPlat.platID = weaponState.targetID;
		newWeaponPlat.weaponState = weaponState; // 拷贝协议参数
		newWeaponPlat.isExist = true;
		newWeaponPlat.nodePath = modelNode;

		// 添加到列表并显示
		m_weaponPlatformList.push_back(newWeaponPlat);
		modelNode.show();
		std::cout << "WeaponState平台生成成功：类型=" << platType << " 目标ID=" << weaponState.targetID << std::endl;
	}
	else
	{
		// 删除WeaponState平台
		for (auto& weaponPlat : m_weaponPlatformList)
		{
			if (weaponPlat.isExist)
			{
				weaponPlat.nodePath.remove_node();
				weaponPlat.isExist = false;
			}
		}
		m_weaponPlatformList.clear();
	}

#endif // 0
}

// 处理TargetState平台增删（目标状态）
void HwaSimIR::ProcessAddRemoveTargetPlatform()
{

	if (m_isAddPlatform)
	{
		// 增加TargetState平台
		std::cout << "开始生成TargetState平台，有效数--120：" << m_initSceneData.MissileMaxCount120 <<"--9："<< m_initSceneData.MissileMaxCount9 << "--MMD：" << m_initSceneData.MissileMaxCountMMD << std::endl;
		//int 

		for (int i = 0; i < m_initSceneData.MissileMaxCount120; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x22);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x22 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x22;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;
			CreateEnginePlumeForTarget(newTargetPlat);

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.hide(); // 未收到有效TargetState前不渲染

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
			/*if (newTargetPlat.platID == 3)
			{
				m_cameraNode.look_at(modelNode);
			}*/
		}
		for (int i = 0; i < m_initSceneData.MissileMaxCount9; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x33);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x33 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 阶段2统一入口：加载模型、基础纹理，并绑定材质ID纹理/材质参数。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x33;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;
			CreateEnginePlumeForTarget(newTargetPlat);

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.hide(); // 未收到有效TargetState前不渲染

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
		}
		for (int i = 0; i < m_initSceneData.MissileMaxCountMMD; ++i)
		{
			//if (i >= 5) break; // 协议中targetState最多5个

			//const BYHWICD::TargetState& targetState = m_initSceneData.targetState[i];
			PLATFORM_TYPE platType = TargetTypeToPlatformType(0x44);
			if (platType == NONE)
			{
				std::cerr << "无效的TargetState目标类型：0x" << std::hex << 0x44 << std::endl;
				continue;
			}

			auto resIter = m_platformResMap.find(platType);
			if (resIter == m_platformResMap.end()) continue;

			// 阶段2统一入口：MMD 暂无资产时不会进入加载；后续补模型后沿用同一材质绑定流程。
			NodePath modelNode = LoadPlatformAssetNode(platType, resIter->second);
			if (modelNode.is_empty()) continue;

			// 初始化TargetPlatformData
			TargetPlatformData newTargetPlat;
			newTargetPlat.type = platType;
			newTargetPlat.platID = -1; // 初始化时尚未绑定协议目标ID，Display包到达后按三元组映射
			newTargetPlat.targetState.targetType = 0x44;
			newTargetPlat.targetState.targetPlatID = -1;
			newTargetPlat.targetState.targetID = -1;
			newTargetPlat.targetState.engineState = false;
			newTargetPlat.targetState.viewValid = false;
			newTargetPlat.targetState.targetLoc.lat = 0.0;
			newTargetPlat.targetState.targetLoc.lon = 0.0;
			newTargetPlat.targetState.targetLoc.alt = 0.0;
			newTargetPlat.targetState.targetLoc.yaw = 0.0;
			newTargetPlat.targetState.targetLoc.pitch = 0.0;
			newTargetPlat.targetState.targetLoc.roll = 0.0;
			newTargetPlat.targetState.targetState = 0x01;
			newTargetPlat.isExist = true;
			newTargetPlat.nodePath = modelNode;
			CreateEnginePlumeForTarget(newTargetPlat);

			// 设置初始位置（从目标空间状态读取）
			const BYHWICD::SpatialState& spatial = newTargetPlat.targetState.targetLoc;
			modelNode.set_pos(spatial.lat, spatial.lon, spatial.alt);
			modelNode.set_hpr(-spatial.yaw, spatial.pitch, spatial.roll);

			// 添加到列表并显示
			m_targetPlatformList.push_back(newTargetPlat);
			modelNode.hide(); // 未收到有效TargetState前不渲染

			std::cout << "TargetState平台生成成功：类型=" << platType << " 目标ID=" << newTargetPlat.platID << std::endl;
		}
	}
	else
	{
		// 删除TargetState平台
		m_annotationManager.clear();
		for (auto& targetPlat : m_targetPlatformList)
		{
			if (targetPlat.isExist)
			{
				if (!targetPlat.enginePlumeCoreNodePath.is_empty())
				{
					targetPlat.enginePlumeCoreNodePath.remove_node();
				}
				if (!targetPlat.enginePlumeHaloNodePath.is_empty())
				{
					targetPlat.enginePlumeHaloNodePath.remove_node();
				}
				targetPlat.nodePath.remove_node();
				targetPlat.isExist = false;
			}
		}
		m_targetPlatformList.clear();
	}
}


void HwaSimIR::on_key_event(const Event * event, void * user_data)
{
	HwaSimIR* self = static_cast<HwaSimIR*>(user_data);
	
	LVecBase3 hpr = self->m_cameraNode.get_hpr();
	LVecBase3 pos = self->m_cameraNode.get_pos();
	double insu = 30.0;

	const std::string& name = event->get_name();


	if (name == "w") {
		std::cout << "[INPUT] w" << std::endl;
		hpr[1] = hpr[1] + insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;
		
	}
	else if (name == "s") {
		std::cout << "[INPUT] s" << std::endl;
		hpr[1] = hpr[1] - insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;
		
	}
	else if (name == "a") {
		std::cout << "[INPUT] a" << std::endl;
		hpr[0] = hpr[0] - insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;

	}
	else if (name == "d") {
		std::cout << "[INPUT] d" << std::endl;
		hpr[0] = hpr[0] + insu;
		self->m_cameraNode.set_hpr(hpr);
		std::cout << "Camera HPR: H=" << hpr[0]
			<< ", P=" << hpr[1]
			<< ", R=" << hpr[2] << std::endl;

	}
	else if (name == "arrow_up") {
		std::cout << "[INPUT] arrow_up" << std::endl;
		pos[1] = pos[1] + insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else if (name == "arrow_down") {
		std::cout << "[INPUT] arrow_down" << std::endl;
		pos[1] = pos[1] - insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;
	}
	else if (name == "arrow_left") {
		std::cout << "[INPUT] arrow_left" << std::endl;
		pos[0] = pos[0] - insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else if (name == "arrow_right") {
		std::cout << "[INPUT] arrow_right" << std::endl;
		pos[0] = pos[0] + insu;
		self->m_cameraNode.set_pos(pos);
		std::cout << "Camera XYZ: X=" << pos[0]
			<< ", Y=" << pos[1]
			<< ", Z=" << pos[2] << std::endl;

	}
	else
	{
		std::cout << "[INPUT] null" << std::endl;
	}
}

void HwaSimIR::LoadNetworkConfig()
{
	IRRuntimeConfig networkConfig;
	std::vector<std::string> configPaths;
	configPaths.push_back("Config/NetworkConfig.ini");
	configPaths.push_back("../Bin/Config/NetworkConfig.ini");
	configPaths.push_back("HwaSim_IR/Bin/Config/NetworkConfig.ini");
	configPaths.push_back("../HwaSim_IR/Bin/Config/NetworkConfig.ini");
	networkConfig.loadFromCandidates(configPaths);

	m_udpLocalIp = networkConfig.getString("UDP", "localIp", "", m_udpLocalIp);
	m_udpRemoteIp = networkConfig.getString("UDP", "remoteIp", "", m_udpRemoteIp);
	m_tcpServerIp = networkConfig.getString("TCP", "serverIp", "", m_tcpServerIp);

	const int udpLocalPort = networkConfig.getInt("UDP", "localPort", "", m_udpLocalPort);
	const int udpRemotePort = networkConfig.getInt("UDP", "remotePort", "", m_udpRemotePort);
	const int tcpServerPort = networkConfig.getInt("TCP", "serverPort", "", m_tcpServerPort);
	if (udpLocalPort > 0 && udpLocalPort <= 65535)
	{
		m_udpLocalPort = static_cast<uint16_t>(udpLocalPort);
	}
	if (udpRemotePort > 0 && udpRemotePort <= 65535)
	{
		m_udpRemotePort = static_cast<uint16_t>(udpRemotePort);
	}
	if (tcpServerPort > 0 && tcpServerPort <= 65535)
	{
		m_tcpServerPort = static_cast<uint16_t>(tcpServerPort);
	}

	std::cout << "[NetworkConfig]"
		<< " path=" << networkConfig.loadedPath()
		<< " loaded=" << (networkConfig.loaded() ? "1" : "0")
		<< " udpLocal=" << m_udpLocalIp << ":" << m_udpLocalPort
		<< " udpRemote=" << m_udpRemoteIp << ":" << m_udpRemotePort
		<< " tcpServer=" << m_tcpServerIp << ":" << m_tcpServerPort
		<< std::endl;
	if (!networkConfig.loaded())
	{
		std::cerr << "[NetworkConfig][WARN] NetworkConfig.ini not found; using built-in defaults."
			<< std::endl;
	}
}

// 初始化UDP线程
bool HwaSimIR::InitUdpThread() {
	std::cout << "[Stage0] UDP baseline local=" << m_udpLocalIp << ":" << m_udpLocalPort
		<< " remote=" << m_udpRemoteIp << ":" << m_udpRemotePort << std::endl;

	m_pUdpThread = new UdpCommThread(
		this,
		m_udpLocalIp,
		m_udpLocalPort,
		m_udpRemoteIp,
		m_udpRemotePort);
	if (!m_pUdpThread->start()) {
		std::cerr << "UDP线程启动失败！" << std::endl;
		delete m_pUdpThread;
		m_pUdpThread = nullptr;
		return false;
	}

	std::cout << "UDP线程初始化成功" << std::endl;
	return true;
}

bool HwaSimIR::InitTcpThread()
{
	std::cout << "[Stage0] TCP video baseline server=" << m_tcpServerIp << ":" << m_tcpServerPort
		<< " format=length-prefixed JPEG" << std::endl;

	m_pTcpThread = new TcpCommThread(this, m_tcpServerIp, m_tcpServerPort);
	m_pTcpThread->setSyncMode(m_bSyncRenderMode.load());
	m_pTcpThread->setFlipVertical(m_stage6FlipInTcpThread);
	m_pTcpThread->configureOutput(
		m_tcpJpegQuality,
		m_tcpJpegGray,
		m_enableH264Experimental,
		m_h264FallbackToJpeg,
		m_h264Encoder,
		m_h264BitrateKbps,
		m_h264GopFrames,
		m_h264LowLatency,
		m_h264ForceKeyFrameOnStart,
		m_tcpCodecConfig);
	if (!m_pTcpThread->start()) {
		std::cerr << "TCP线程启动失败！" << std::endl;
		delete m_pTcpThread;
		m_pTcpThread = nullptr;
		return false;
	}
	std::cout << "TCP线程初始化成功" << std::endl;
	return true;
}

void HwaSimIR::ProcessPendingNetworkCommands()
{
	std::deque<PendingNetworkCommand> pendingCommands;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		pendingCommands.swap(m_pendingNetworkCommands);
	}

	for (std::deque<PendingNetworkCommand>::const_iterator it = pendingCommands.begin();
		it != pendingCommands.end(); ++it)
	{
		if (it->type == PendingNetworkCommandType::Control)
		{
			std::cout << "[Stage0] Processing queued control command on render thread"
				<< " command=" << it->controlCmd.simCommand
				<< " round=" << it->controlCmd.currentRound << "/" << it->controlCmd.roundCut
				<< std::endl;
			ProcessControlCmdOnMainThread(it->controlCmd);
		}
		else
		{
			std::cout << "[Stage0] Processing queued init command on render thread"
				<< " sensorID=" << it->initCmd.sensorID
				<< " platNumValid=" << it->initCmd.platNumValid
				<< std::endl;
			ProcessInitCmdOnMainThread(it->initCmd);
		}
	}
}

// 处理控制指令（复位/开始/停止）
void HwaSimIR::handleControlCmd(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd)
{
	PendingNetworkCommand pending;
	pending.type = PendingNetworkCommandType::Control;
	pending.controlCmd = cmd;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_pendingNetworkCommands.push_back(pending);
	}
	std::cout << "[Stage0] Control command queued for main thread: command=" << cmd.simCommand
		<< ", round=" << cmd.currentRound << "/" << cmd.roundCut << std::endl;
	m_cvNewData.notify_one();
}

void HwaSimIR::ProcessControlCmdOnMainThread(const BYHWICD::ControlP2cX1ObjTrackingCmd& cmd) {
	std::lock_guard<std::mutex> lock(m_mtx);
	// 更新当前回合数
	m_currentRound = cmd.currentRound;

	std::cout << "收到控制指令：" << std::endl;
	std::cout << "  军别：" << cmd.JB << std::endl;
	std::cout << "  挂载平台ID：" << cmd.platID << std::endl;
	std::cout << "  仿真指令：" << (cmd.simCommand == 1 ? "复位" : (cmd.simCommand == 2 ? "开始" : "停止")) << std::endl;
	std::cout << "  总回合数：" << cmd.roundCut << std::endl;
	std::cout << "  当前回合：" << cmd.currentRound << std::endl;
	std::cout << "[Stage0] Control command received: command=" << cmd.simCommand
		<< ", round=" << cmd.currentRound << "/" << cmd.roundCut << std::endl;

	// ========== 业务逻辑（后续填充） ==========
	switch (cmd.simCommand) {
	case 1: // 复位
		std::cout << "执行复位逻辑..." << std::endl;
		m_stage0DisplayFrameCount = 0;
		m_udpSequence = 0;
		m_inputQueueBackpressureLogCount = 0;
		m_annotationLastProjectionSourceSeq = 0;
		m_lastIrUpdateSourceSeq = 0;
		m_lastIrUpdateState.clear();
		m_pendingDisplayFrames.clear();
		m_currentFrameTelemetry = IRFrameTelemetry();
		m_latestUdpSourceSeq.store(0);
		m_lastCapturedSourceSeq = 0;
		m_lastOutputSourceSeq.store(0);
		m_lastSourceSeqContinuous.store(true);
		m_perfStats.reset();
		m_perfStats.configure(m_bSyncRenderMode.load(), static_cast<double>(m_targetVideoFps.load()));
		m_annotationManager.clear();
		m_annotationManager.setEnabled(false);
		// TODO: 实现复位逻辑（清空状态、重置传感器等）

		// 设置增删标记为"删除"
		m_isAddPlatform = false;
		// 设置TargetState平台初始化ID映射标记
		m_isInitTargetPlatID = false;
		// 删除所有三类平台
		ProcessAddRemovePakPlatform();
		ProcessAddRemoveWeaponPlatform();
		ProcessAddRemoveTargetPlatform();

		////测试坐标转换
		//m_geoTrans.InitReferencePoint(12.77632, 45.04385, 6.0);
		//double px, py, pz;
		//BYHWICD::SpatialState spatial;
		//spatial.lat = 12.77627;
		//spatial.lon = 45.04369;
		//spatial.alt = 6.0;
		//spatial.yaw = 239.0;
		//spatial.pitch = 0.0;
		//spatial.roll = 0.0;
		//m_geoTrans.Wgs84ToPandaXYZ(spatial, px, py, pz);
		//std::cout << "px=   " << px << "-----py=   " << py << "-----pz=   " << pz << std::endl;

		// 清空协议数据缓存
		memset(&m_initSceneData, 0, sizeof(BYHWICD::InitP2cObjectTrackingCmd));
		memset(&m_realTimeSceneData, 0, sizeof(BYHWICD::DisplayC2cObjTrackingData));
		memset(&m_sensorParam, 0, sizeof(BYHWICD::trackerSensorParam));
		m_lastLoggedSensorProtocolBand = -999;
		m_sensorDisplayConfigReady = false;
		m_stage6DisplayConfigReady = false;
		m_stage6FarClipWarningLogged = false;
		m_stage7NearFarClipWarningLogged = false;
		m_stage6CaptureLogCounter = 0;
		m_stage6DisplayLogCounter = 0;
		m_irEnginePlumeModel.resetRuntime();
		m_stage5PlumeRuntimeCache.clear();
		m_stage5PlumePerfMsTotal = 0.0;
		m_stage5PlumePerfMsMax = 0.0;
		m_stage5PlumePerfSamples = 0;
		m_stage5PlumeLastPerfState.clear();
		m_stage5PlumePerfLogCounter = 0;
		m_lastStage4TargetLogState.clear();
		m_lastStage5RadianceComponentLogState.clear();
		m_lastStage5ModtranRadianceCompareLogState.clear();
		m_lastStage5ModtranPathABLogState.clear();
		m_stage5ModtranRadianceCache.clear();
		m_stage5ModtranPathRuntimeBandWarned.clear();
		m_stage5AeroThermalStateByTarget.clear();
		m_lastStage5AeroThermalLogState.clear();
		m_lastAeroSpeedStateLogState.clear();
		m_lastStage4InputState.clear();
		RefreshStage6DisplayShaderInputs();

		// 重置仿真状态
		m_isSimRunning.store(false);
		// Stage2B：同步转发复位控制命令，驱动显示端关闭/重置本回合保存状态。
		if (m_pTcpThread) {
			m_pTcpThread->resetFrameCounters();
			m_pTcpThread->sendControlCmd(cmd);
			m_pTcpThread->resetInitCompleted();
			std::cout << "TCP线程初始化标志已重置，下一回合可重新发送初始化命令" << std::endl;
		}
		std::cout << "复位完成：所有平台已删除，数据已清空" << std::endl;
		break;
	case 2: // 开始
		std::cout << "执行开始仿真逻辑..." << std::endl;
		m_stage0DisplayFrameCount = 0;
		m_udpSequence = 0;
		m_inputQueueBackpressureLogCount = 0;
		m_annotationLastProjectionSourceSeq = 0;
		m_lastIrUpdateSourceSeq = 0;
		m_lastIrUpdateState.clear();
		m_latestUdpSourceSeq.store(0);
		m_pendingDisplayFrames.clear();
		m_currentFrameTelemetry = IRFrameTelemetry();
		m_lastCapturedSourceSeq = 0;
		m_lastOutputSourceSeq.store(0);
		m_lastSourceSeqContinuous.store(true);
		m_perfStats.reset();
		m_perfStats.configure(m_bSyncRenderMode.load(), static_cast<double>(m_targetVideoFps.load()));
		// TODO: 实现开始仿真逻辑（启动渲染、数据采集等）
		m_isSimRunning.store(true);
		// Stage2B：同步转发开始控制命令，触发显示端开始录制和创建保存目录。
		if (m_pTcpThread) {
			m_pTcpThread->resetFrameCounters();
			m_pTcpThread->sendControlCmd(cmd);
			m_pTcpThread->resetInitCompleted();
			std::cout << "TCP线程初始化标志已重置，回合重新开始" << std::endl;
		}
		
		std::cout << "仿真开始：当前回合=" << m_currentRound << std::endl;
		break;
	case 3: // 停止
		std::cout << "执行停止仿真逻辑..." << std::endl;
		// TODO: 实现停止仿真逻辑（停止渲染、保存数据等）
		m_isSimRunning.store(false);
		// Stage2B：同步转发停止控制命令，触发显示端 flush 并关闭视频/数据/标注文件。
		if (m_pTcpThread) {
			m_pTcpThread->sendControlCmd(cmd);
		}
	
		std::cout << "仿真停止：当前回合=" << m_currentRound << std::endl;
		{
			std::string exitOnStopSource;
			const bool exitOnStop = m_runtimeConfig.getBool("Debug", "ExitOnStop", "HwaSimIRExitOnStop", false, &exitOnStopSource);
			if (exitOnStop) {
				std::cout << "[Stage0] ExitOnStop requested by " << exitOnStopSource << "; closing main loop." << std::endl;
				m_requestExit.store(true);
				m_cvNewData.notify_one();
			}
		}
		break;
	default:
		std::cerr << "未知的仿真指令：" << cmd.simCommand << std::endl;
		break;
	}
}

// 处理初始化指令并发送应答
void HwaSimIR::handleInitCmd(const BYHWICD::InitP2cObjectTrackingCmd& cmd)
{
	PendingNetworkCommand pending;
	pending.type = PendingNetworkCommandType::Init;
	pending.initCmd = cmd;
	{
		std::lock_guard<std::mutex> lock(m_mtx);
		m_pendingNetworkCommands.push_back(pending);
	}
	std::cout << "[Stage0] Init command queued for main thread: sensorID=" << cmd.sensorID
		<< ", platNumValid=" << cmd.platNumValid << std::endl;
	m_cvNewData.notify_one();
}

void HwaSimIR::ProcessInitCmdOnMainThread(const BYHWICD::InitP2cObjectTrackingCmd& cmd) {
	std::lock_guard<std::mutex> lock(m_mtx);

	std::cout << "收到成像初始化指令：" << std::endl;
	std::cout << "  军别：" << cmd.JB << std::endl;
	std::cout << "  挂载平台ID：" << cmd.platID << std::endl;
	std::cout << "  传感器ID：" << cmd.sensorID << std::endl;
	std::cout << "  有效平台数：" << cmd.platNumValid << std::endl;
	const BYHWICD::trackerSensorParam& sensor = cmd.trackingInit.trackerSensor[0];
	std::cout << "[Stage0] Init baseline: sensorBand=" << sensor.trackerSensorBand
		<< ", sensorSize=" << sensor.trackerSensorWidth << "x" << sensor.trackerSensorHeight
		<< ", viewMinMax=" << sensor.trackerSensorViewMin << "/" << sensor.trackerSensorViewMax
		<< ", pixelAngleUrad=" << sensor.trackerSensorPixelAngle
		<< ", legacyResolution=" << sensor.coarseTrackResolution << "x" << sensor.preciseTrackResolution
		<< ", videoFps=" << cmd.trackingInit.videoFps
		<< ", missileMax(AIM120/AIM9/MMD)=" << cmd.MissileMaxCount120 << "/"
		<< cmd.MissileMaxCount9 << "/" << cmd.MissileMaxCountMMD << std::endl;
	const int targetVideoFps = cmd.trackingInit.videoFps > 0 ? cmd.trackingInit.videoFps : 25;
	m_targetVideoFps.store(targetVideoFps);
	m_perfStats.reset();
	m_perfStats.configure(m_bSyncRenderMode.load(), static_cast<double>(targetVideoFps));
	m_pendingDisplayFrames.clear();
	m_udpSequence = 0;
	m_inputQueueBackpressureLogCount = 0;
	m_annotationLastProjectionSourceSeq = 0;
	m_lastIrUpdateSourceSeq = 0;
	m_irBreakdownUpdateCounter = 0;
	m_lastIrUpdateState.clear();
	m_currentFrameTelemetry = IRFrameTelemetry();
	m_latestUdpSourceSeq.store(0);
	m_lastCapturedSourceSeq = 0;
	m_lastOutputSourceSeq.store(0);
	m_lastSourceSeqContinuous.store(true);
	m_lastStage4TargetLogState.clear();
	m_lastStage5RadianceComponentLogState.clear();
	m_lastStage5ModtranRadianceCompareLogState.clear();
	m_lastStage5ModtranPathABLogState.clear();
	m_stage5ModtranRadianceCache.clear();
	m_stage5ModtranPathRuntimeBandWarned.clear();
	m_stage5AeroThermalStateByTarget.clear();
	m_lastStage5AeroThermalLogState.clear();
	m_lastAeroSpeedStateLogState.clear();
	m_lastStage4InputState.clear();
	m_stage6AgcGain = 1.0;
	m_stage6AgcOffset = 0.0;
	m_stage6AgcLowInput = 0.0;
	m_stage6AgcHighInput = 1.0;
	m_stage6AgcTargetGain = 1.0;
	m_stage6AgcTargetOffset = 0.0;
	m_stage6AgcSampleCount = 0;
	m_stage6AgcValid = false;
	m_stage6AgcInitialized = false;
	m_stage6AgcFallbackReason = m_stage6AgcEnabled ? "reset" : "disabled";
	m_stage6AgcStatsMsCurrent = 0.0;
	m_stage6AgcApplyMsCurrent = 0.0;
	m_stage6AgcLastUpdateNs = 0;
	m_stage6AgcLastUpdateSourceSeq = 0;
	m_stage6AgcLogCounter = 0;
	m_lastStage6AgcLogState.clear();
	ApplyStage6FinalPostprocessInputs();
	if (m_pTcpThread)
	{
		m_pTcpThread->setSyncMode(m_bSyncRenderMode.load());
		m_h264EnFromInit = sensor.h264En;
		m_pTcpThread->setH264Requested(sensor.h264En);
		m_pTcpThread->resetFrameCounters();
	}
	LogEffectiveRuntimeConfig(
		"after_init",
		targetVideoFps,
		0,
		sensor.saveMP4En,
		"cmd",
		"pending_realtime_packet",
		"cmd");
	if (!m_bSyncRenderMode.load())
	{
		SetRenderMode(false, static_cast<double>(targetVideoFps));
	}
	LogActiveIRSensorProfile(sensor.trackerSensorBand, "init-command", true);

	// ========== 初始化业务逻辑（后续填充） ==========
	std::cout << "执行成像初始化逻辑..." << std::endl;
	// TODO: 解析传感器参数、初始化场景、加载模型等
	//缓存初始化数据
	m_initSceneData = cmd;
	m_currentRound = 0; // 重置回合数
	m_stage0DisplayFrameCount = 0;

	//处理成像初始化数据，生成平台
	ProcessRealSimSceneInitData();

	// 阶段3：初始化后立即合成一次环境状态，保证 UDP 环境参数优先级生效。
	IRRuntimeEnvironment initEnvironment = BuildRuntimeEnvironment();
	m_irRadianceModel.setEnvironment(initEnvironment);
	LogActiveIREnvironment(initEnvironment, "init-command", true);




	// 构造初始化应答
	BYHWICD::InitAckC2pObjectTrackingCmd ack;
	ack.flag = 0x37;
	ack.JB = cmd.JB;
	ack.platID = cmd.platID;
	ack.sensorID = cmd.sensorID;
	ack.trackingReady = true; // 标记为已准备好（根据实际初始化结果修改）

							  // 发送应答
	if (m_pUdpThread) {
		m_pUdpThread->sendInitAck(ack);
	}
	std::cout << "发送初始化应答：准备状态=" << (ack.trackingReady ? "就绪" : "未就绪") << std::endl;

	// Stage2B：同步转发初始化命令，驱动显示端初始化界面和传感器/平台状态。
	if (m_pTcpThread) {
		m_pTcpThread->sendInitCmd(cmd);
	}
}

// 处理实时成像数据包
void HwaSimIR::handleDisplayData(const BYHWICD::DisplayC2cObjTrackingData& data) {
	const std::int64_t receiveTimeNs = IRPerfStats::wallTimeNs();
	std::unique_lock<std::mutex> lock(m_mtx);
	++m_stage0DisplayFrameCount;
	const std::uint64_t udpSeq = ++m_udpSequence;
	m_perfStats.recordUdpFrame();
	if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 120) == 0)
	{
		std::cout << "[Stage0] Display packet #" << m_stage0DisplayFrameCount
			<< ": platID=" << data.platID
			<< ", sensorID=" << data.sensorID
			<< ", targetNumValid=" << data.targetNumValid
			<< ", timeMs=" << data.time << std::endl;
	}
	if (m_stage0DisplayFrameCount == 1)
	{
		LogEffectiveRuntimeConfig(
			"first_display",
			m_targetVideoFps.load(),
			data.targetNumValid,
			m_sensorParam.saveMP4En,
			"cmd",
			"udp_display",
			"cmd");
	}
	const int visibleTargetNumForLog = std::max(0, std::min(data.targetNumValid, 5));
	int stage4LogTargetCount = 0;
	bool anyStage4InputSignal = data.weaponState.strikeFlag || data.weaponState.strikePart != 0;
	std::ostringstream stage4State;
	stage4State << data.weaponState.strikeFlag << ":" << data.weaponState.strikePart
		<< ":" << data.weaponState.targetType << ":" << data.weaponState.targetPlatID
		<< ":" << data.weaponState.targetID << ":" << data.weaponState.viewValid;
	for (int i = 0; i < 5; ++i)
	{
		const BYHWICD::TargetState& target = data.targetState[i];
		stage4State << "|" << target.targetType << ":" << target.targetPlatID << ":" << target.targetID
			<< ":" << target.engineState << ":" << target.viewValid;
	}
	const std::string stage4StateKey = stage4State.str();
	const bool logStage4Input =
		m_enableIRVerboseLog ||
		m_stage0DisplayFrameCount <= 3 ||
		(m_stage0DisplayFrameCount % 120) == 0 ||
		stage4StateKey != m_lastStage4InputState;
	m_lastStage4InputState = stage4StateKey;
	for (int i = 0; i < 5; ++i)
	{
		const auto& target = data.targetState[i];
		if (i >= visibleTargetNumForLog && TargetTypeToPlatformType(target.targetType) == NONE)
		{
			continue;
		}
		++stage4LogTargetCount;
		anyStage4InputSignal = anyStage4InputSignal || target.engineState;
		if (logStage4Input)
		{
			std::cout << "[Stage4 Input]"
				<< " packet=" << m_stage0DisplayFrameCount
				<< " targetIndex=" << i
				<< " targetID=" << target.targetID
				<< " targetPlatID=" << target.targetPlatID
				<< " platform=" << Stage4PlatformName(TargetTypeToPlatformType(target.targetType))
				<< " engineState=" << (target.engineState ? "1" : "0")
				<< " targetViewValid=" << (target.viewValid ? "1" : "0")
				<< " visibleByTargetNum=" << (i < visibleTargetNumForLog ? "1" : "0")
				<< " strikeFlag=" << (data.weaponState.strikeFlag ? "1" : "0")
				<< " strikePart=" << data.weaponState.strikePart
				<< " weaponTargetType=0x" << std::hex << data.weaponState.targetType << std::dec
				<< " weaponTargetID=" << data.weaponState.targetID
				<< " weaponTargetPlatID=" << data.weaponState.targetPlatID
				<< " weaponViewValid=" << (data.weaponState.viewValid ? "1" : "0")
				<< " sourcePacketTimeMs=" << data.time
				<< std::endl;
		}
	}
	if (logStage4Input &&
		(stage4LogTargetCount == 0 || (!anyStage4InputSignal && (m_stage0DisplayFrameCount % 120) == 0)))
	{
		std::cout << "[Stage4 Input][WARN] STAGE4_INPUT_NOT_UPDATED"
			<< " packet=" << m_stage0DisplayFrameCount
			<< " targetNumValid=" << data.targetNumValid
			<< " strikeFlag=" << (data.weaponState.strikeFlag ? "1" : "0")
			<< " strikePart=" << data.weaponState.strikePart
			<< " reason=" << (stage4LogTargetCount == 0 ? "no_valid_targets" : "no_engine_or_strike_signal")
			<< std::endl;
	}
	const bool logAeroSpeedRecv =
		m_enableIRVerboseLog ||
		m_stage0DisplayFrameCount <= 3 ||
		(m_stage0DisplayFrameCount % 600) == 0;
	if (logAeroSpeedRecv)
	{
		const int aeroTargetCount = std::max(0, std::min(data.targetNumValid, 5));
		for (int i = 0; i < 5; ++i)
		{
			const auto& target = data.targetState[i];
			if (i >= aeroTargetCount && !IsValidTargetStateKey(target))
			{
				continue;
			}
			std::cout << "[AeroSpeedRecv]"
				<< " sourceSeq=" << udpSeq
				<< " targetIndex=" << i
				<< " targetID=" << target.targetID
				<< " targetType=" << TargetTypeHexString(target.targetType)
				<< " platform=" << Stage4PlatformName(TargetTypeToPlatformType(target.targetType))
				<< " receivedSpeedRaw=" << target.targetLoc.speed
				<< " receivedAltitudeM=" << target.targetLoc.alt
				<< " targetLoc.speed=" << target.targetLoc.speed
				<< " speedUnit=km/h"
				<< std::endl;
		}
	}

	/*std::cout << "收到实时成像数据：" << std::endl;
	std::cout << "  挂载平台ID：" << data.platID << std::endl;
	std::cout << "  传感器ID：" << data.sensorID << std::endl;
	std::cout << "  当前时间：" << data.time << "ms" << std::endl;
	std::cout << "  有效目标数：" << data.targetNumValid << std::endl;*/
#if 0
	std::cout << "==================== 收到实时成像数据 ====================" << std::endl;
	std::cout << "  数据标志位：0x" << std::hex << data.flag << std::dec << std::endl; // 十六进制打印标志位
	std::cout << "  挂载平台ID：" << data.platID << std::endl;
	std::cout << "  传感器ID：" << data.sensorID << std::endl;
	std::cout << "  当前时间：" << data.time << "ms" << std::endl;
	std::cout << "  有效目标数：" << data.targetNumValid << std::endl;

	// 打印传感器挂载平台姿态信息
	std::cout << "  挂载平台姿态信息：" << std::endl;
	std::cout << "    位置/姿态：x=" << data.platLoc.lon << ", y=" << data.platLoc.lat << ", z=" << data.platLoc.alt
		<< " | 俯仰=" << data.platLoc.pitch << "°, 偏航=" << data.platLoc.yaw << "°, 滚转=" << data.platLoc.roll << "°" << std::endl;

	// 打印实时武器状态信息
	std::cout << "  实时武器状态（Wg）：" << std::endl;
	std::cout << "    目标类型：0x" << std::hex << data.weaponState.targetType << std::dec
		<< " (" << (data.weaponState.targetType == 0x00 ? "无" :
			data.weaponState.targetType == 0x11 ? "飞机" :
			data.weaponState.targetType == 0x22 ? "雷达导弹" :
			data.weaponState.targetType == 0x33 ? "红外弹" : "MMD") << ")" << std::endl;
	std::cout << "    被打击目标平台ID：" << data.weaponState.targetPlatID << std::endl;
	std::cout << "    被打击目标ID：" << data.weaponState.targetID << std::endl;
	std::cout << "    目标与机轴角度：方位角=" << data.weaponState.xxOutAng[0] << "°, 俯仰角=" << data.weaponState.xxOutAng[1] << "°" << std::endl;
	std::cout << "    自动对准使能：" << (data.weaponState.lookatEn ? "是" : "否") << std::endl;
	std::cout << "    照明器开启状态：" << (data.weaponState.illuminatorEn ? "开启" : "关闭") << std::endl;
	std::cout << "    角度偏移量：Pitch=" << data.weaponState.offsetAng[0] << "°, Yaw=" << data.weaponState.offsetAng[1] << "°" << std::endl;
	std::cout << "    目标是否在视场：" << (data.weaponState.viewValid ? "是" : "否") << std::endl;
	std::cout << "    目标毁伤标志：" << (data.weaponState.damageFlag == 0 ? "未毁伤" : "已毁伤") << std::endl;
	std::cout << "    武器打击标志：" << (data.weaponState.strikeFlag ? "已打击" : "未打击") << std::endl;
	std::cout << "    目标打击部位：" << (data.weaponState.strikePart == 1 ? "头部" : (data.weaponState.strikePart == 2 ? "舱体" : "未知")) << std::endl;

	// 打印多目标状态信息（遍历所有有效目标）
	std::cout << "  有效目标状态列表（共" << data.targetNumValid << "个）：" << std::endl;
	for (int i = 0; i < data.targetNumValid; ++i) {
		const auto& target = data.targetState[i];
		std::cout << "    【目标" << (i + 1) << "】" << std::endl;
		std::cout << "      目标类型：0x" << std::hex << target.targetType << std::dec
			<< " (" << (target.targetType == 0x11 ? "飞机" :
				target.targetType == 0x22 ? "雷达导弹" :
				target.targetType == 0x33 ? "红外弹" : "MMD") << ")" << std::endl;
		std::cout << "      目标挂载平台ID：" << target.targetPlatID << std::endl;
		std::cout << "      目标ID：" << target.targetID << std::endl;
		std::cout << "      发动机状态：" << (target.engineState ? "开机" : "关机") << std::endl;
		std::cout << "      目标是否在视场：" << (target.viewValid ? "是" : "否") << std::endl;
		std::cout << "      目标位置/姿态：x=" << target.targetLoc.lon << ", y=" << target.targetLoc.lat << ", z=" << target.targetLoc.alt
			<< " | 俯仰=" << target.targetLoc.pitch << "°, 偏航=" << target.targetLoc.yaw << "°, 滚转=" << target.targetLoc.roll << "°" << std::endl;
		std::cout << "      目标状态：0x" << std::hex << target.targetState << std::dec
			<< " (" << (target.targetState == 0x01 ? "打击态" :
				target.targetState == 0x02 ? "爆炸态" : "击毁态") << ")" << std::endl;
	}

	// ========== 实时数据处理逻辑（后续填充） ==========
	std::cout << "============================================================" << std::endl;
#endif // 0
	// ========== 实时数据处理逻辑（后续填充） ==========
	if (m_stage0DisplayFrameCount <= 3 || (m_stage0DisplayFrameCount % 120) == 0)
	{
		std::cout << "处理实时成像数据..." << std::endl;
	}
	// TODO: 更新传感器姿态、目标位置、渲染红外图像等

	PendingDisplayFrame pending;
	pending.data = data;
	m_latestUdpSourceSeq.store(udpSeq);
	pending.telemetry.sourceSeq = udpSeq;
	pending.telemetry.udpReceiveTimeNs = receiveTimeNs;
	if (m_bSyncRenderMode.load())
	{
		if (m_pendingDisplayFrames.size() >= kMaxPendingDisplayFrames)
		{
			m_perfStats.recordSyncOverrun();
			++m_inputQueueBackpressureLogCount;
			if (m_enableIRVerboseLog ||
				m_inputQueueBackpressureLogCount <= 3 ||
				(m_inputQueueBackpressureLogCount % 120) == 0)
			{
				std::cout << "[SyncFrame][WARN] inputQueueBackpressure"
					<< " sourceSeq=" << udpSeq
					<< " backpressureCount=" << m_inputQueueBackpressureLogCount
					<< " queueDepth=" << m_pendingDisplayFrames.size()
					<< " queueCapacity=" << kMaxPendingDisplayFrames
					<< " action=wait_for_space"
					<< std::endl;
			}
			m_cvNewData.notify_one();
			m_cvDisplayQueueSpace.wait(lock, [this] {
				return m_pendingDisplayFrames.size() < kMaxPendingDisplayFrames ||
					m_requestExit.load();
			});
			if (m_requestExit.load())
			{
				return;
			}
		}
	}
	else
	{
		m_pendingDisplayFrames.clear();
	}
	pending.telemetry.inputQueueDepth = static_cast<int>(m_pendingDisplayFrames.size() + 1);
	m_pendingDisplayFrames.push_back(pending);
	m_perfStats.recordInputQueueDepth(static_cast<int>(m_pendingDisplayFrames.size()));

	// 通知主线程，有新数据到达
	m_cvNewData.notify_one();
}

void HwaSimIR::InitInfraredSimulation()
{
	std::string materialPath = FirstExistingPath({
		"materials/MaterialDatabase.csv",
		"../materials/MaterialDatabase.csv",
		"../../materials/MaterialDatabase.csv"
	});
	std::string transmittancePath = FirstExistingPath({
		"transmittance/transmittance_0.3_15.txt",
		"../transmittance/transmittance_0.3_15.txt",
		"../../transmittance/transmittance_0.3_15.txt"
	});
	std::string modtranBandLutPath = FirstExistingPath({
		"Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"../Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv",
		"../HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv"
	});
	std::string weatherPath = FirstExistingPath({
		"temperatures/Temperatures_Yemen_Summer.csv",
		"../temperatures/Temperatures_Yemen_Summer.csv",
		"../../temperatures/Temperatures_Yemen_Summer.csv"
	});
	std::vector<std::string> sensorWaveDirs;
	sensorWaveDirs.push_back("Config/SensorWave");
	sensorWaveDirs.push_back("../Bin/Config/SensorWave");
	sensorWaveDirs.push_back("HwaSim_IR/Bin/Config/SensorWave");
	sensorWaveDirs.push_back("../HwaSim_IR/Bin/Config/SensorWave");
	std::vector<std::string> hotspotConfigPaths;
	hotspotConfigPaths.push_back("Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("../Bin/Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("HwaSim_IR/Bin/Config/IRHotspots/target_hotspots.json");
	hotspotConfigPaths.push_back("../HwaSim_IR/Bin/Config/IRHotspots/target_hotspots.json");
	std::vector<std::string> plumeConfigPaths;
	plumeConfigPaths.push_back("Config/IRPlume/engine_plume_profiles.json");
	plumeConfigPaths.push_back("../Bin/Config/IRPlume/engine_plume_profiles.json");
	plumeConfigPaths.push_back("HwaSim_IR/Bin/Config/IRPlume/engine_plume_profiles.json");
	plumeConfigPaths.push_back("../HwaSim_IR/Bin/Config/IRPlume/engine_plume_profiles.json");
	std::vector<std::string> stage5DebugConfigPaths;
	stage5DebugConfigPaths.push_back("Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("../Bin/Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("HwaSim_IR/Bin/Config/IRRadiance/stage5_debug_display.json");
	stage5DebugConfigPaths.push_back("../HwaSim_IR/Bin/Config/IRRadiance/stage5_debug_display.json");
	if (!m_runtimeConfig.loaded())
	{
		m_runtimeConfig.loadFromCandidates(BuildRuntimeConfigCandidatePaths());
	}
	m_irMaterialReady = m_irMaterialDatabase.load(materialPath);
	m_irAtmosphereReady = m_irAtmosphereModel.loadTransmissionTable(transmittancePath);
	bool modtranTauLutReady = m_irAtmosphereModel.loadModtranBandLut(modtranBandLutPath);
	std::string stage3TauDebugSource;
	std::string stage3UseTauSource;
	std::string stage4VisualSource;
	std::string stage4BrightSource;
	std::string stage4RearSource;
	std::string stage4LegacyEngineBodyHeatingSource;
	std::string perfLogSource;
	std::string verboseLogSource;
	std::string irUpdateHzSource;
	std::string stage4UpdateHzSource;
	std::string stage6FlipShaderSource;
	std::string stage6FlipTcpSource;
	std::string stage6MtfEnableSource;
	std::string stage6MtfModeSource;
	std::string stage6MtfSigmaSource;
	std::string stage6MtfRadiusSource;
	std::string stage6MtfPassesSource;
	std::string stage6MtfApplyToSource;
	std::string stage6MtfDebugSource;
	std::string stage6MtfLogEverySource;
	std::string stage6NoiseEnableSource;
	std::string stage6NoiseApplyToSource;
	std::string stage6NoisePositionSource;
	std::string stage6TemporalEnableSource;
	std::string stage6TemporalSigmaSource;
	std::string stage6FpnEnableSource;
	std::string stage6FpnSigmaSource;
	std::string stage6FpnSeedSource;
	std::string stage6ColumnEnableSource;
	std::string stage6ColumnSigmaSource;
	std::string stage6RowEnableSource;
	std::string stage6RowSigmaSource;
	std::string stage6BadPixelsEnableSource;
	std::string stage6BadPixelRatioSource;
	std::string stage6BadPixelHotSource;
	std::string stage6BadPixelDeadSource;
	std::string stage6NoiseClampMinSource;
	std::string stage6NoiseClampMaxSource;
	std::string stage6NoiseDebugSource;
	std::string stage6NoiseLogEverySource;
	std::string stage6AgcEnableSource;
	std::string stage6AgcModeSource;
	std::string stage6AgcApplyToSource;
	std::string stage6AgcStatsSource;
	std::string stage6AgcUpdateHzSource;
	std::string stage6AgcLogEverySource;
	std::string stage6AgcLowPercentileSource;
	std::string stage6AgcHighPercentileSource;
	std::string stage6AgcMeanStdKSource;
	std::string stage6AgcMinGainSource;
	std::string stage6AgcMaxGainSource;
	std::string stage6AgcMinOffsetSource;
	std::string stage6AgcMaxOffsetSource;
	std::string stage6AgcSmoothingSource;
	std::string stage6AgcTargetLowSource;
	std::string stage6AgcTargetHighSource;
	std::string stage6AgcStrideSource;
	std::string stage6AgcExcludeAnnotationSource;
	std::string stage6AgcDebugSource;
	std::string tcpCodecSource;
	std::string jpegQualitySource;
	std::string jpegModeSource;
	std::string jpegGrayMethodSource;
	std::string h264ExperimentalSource;
	std::string h264FallbackSource;
	std::string h264EncoderSource;
	std::string h264BitrateSource;
	std::string h264GopSource;
	std::string h264LowLatencySource;
	std::string h264ForceKeySource;
	std::string jpegAbTestSource;
	std::string annotationProfileSource;
	std::string annotationDebugSource;
	std::string annotationBBoxModeSource;
	std::string annotationBBoxMarginSource;
	std::string annotationMinBBoxSource;
	std::string annotationDrawKeyPointsSource;
	std::string annotationDrawModelLabelSource;
	std::string annotationDrawBBoxSource;
	std::string annotationOcclusionEnableSource;
	std::string annotationOcclusionModeSource;
	std::string annotationOcclusionEpsilonSource;
	std::string annotationOcclusionMaskSource;
	std::string annotationOcclusionSelfSource;
	std::string annotationSurfaceKeyPointSource;
	std::string annotationSurfaceSnapSource;
	std::string annotationSurfaceSnapModeSource;
	std::string annotationUpdateHzSource;
	bool enableModtranTauDebug = m_runtimeConfig.getBool("Stage3", "EnableModtranTauDebug", "EnableModtranTauDebug", false, &stage3TauDebugSource);
	bool useModtranTauForAtmosphere = m_runtimeConfig.getBool("Stage3", "UseModtranTauForAtmosphere", "UseModtranTauForAtmosphere", false, &stage3UseTauSource);
	m_enablePerfLog = m_runtimeConfig.getBool("Performance", "EnablePerfLog", "EnablePerfLog", true, &perfLogSource);
	m_enableIRVerboseLog = m_runtimeConfig.getBool("Performance", "EnableIRVerboseLog", "EnableIRVerboseLog", false, &verboseLogSource);
	m_irUpdateHz = m_runtimeConfig.getDouble("Performance", "IRUpdateHz", "IRUpdateHz", 30.0, &irUpdateHzSource);
	if (!std::isfinite(m_irUpdateHz) || m_irUpdateHz <= 0.0)
	{
		m_irUpdateHz = 30.0;
	}
	m_irUpdateHz = std::max(1.0, std::min(240.0, m_irUpdateHz));
	m_perfStats.setEnabled(m_enablePerfLog);
	std::cout << "[PerfConfig]"
		<< " EnablePerfLog=" << (m_enablePerfLog ? "1" : "0")
		<< " EnableIRVerboseLog=" << (m_enableIRVerboseLog ? "1" : "0")
		<< " IRUpdateHz=" << m_irUpdateHz
		<< " source=" << perfLogSource << "/" << verboseLogSource << "/" << irUpdateHzSource
		<< std::endl;
	const bool stage6FlipInShaderRequested = m_runtimeConfig.getBool(
		"Stage6Capture",
		"FlipInShader",
		"Stage6FlipInShader",
		false,
		&stage6FlipShaderSource);
	m_stage6FlipInTcpThread = m_runtimeConfig.getBool(
		"Stage6Capture",
		"FlipInTcpThread",
		"Stage6FlipInTcpThread",
		true,
		&stage6FlipTcpSource);
	// Shader-side flipping remains opt-in only after window/TCP/annotation direction validation.
	m_stage6FlipInShader = false;
	if (stage6FlipInShaderRequested)
	{
		std::cout << "[Stage6 CaptureConfig][WARN]"
			<< " FlipInShaderRequested=1"
			<< " FlipInShaderEffective=0"
			<< " reason=direction_annotation_validation_required"
			<< std::endl;
	}
	if (!m_stage6FlipInTcpThread)
	{
		std::cout << "[Stage6 CaptureConfig][WARN]"
			<< " FlipInTcpThreadRequested=0"
			<< " FlipInTcpThreadEffective=1"
			<< " reason=FlipInShader_not_enabled"
			<< std::endl;
		m_stage6FlipInTcpThread = true;
	}
	if (m_pTcpThread)
	{
		m_pTcpThread->setFlipVertical(m_stage6FlipInTcpThread);
	}
	std::cout << "[Stage6 CaptureConfig]"
		<< " FlipInShader=" << (m_stage6FlipInShader ? "1" : "0")
		<< " FlipInTcpThread=" << (m_stage6FlipInTcpThread ? "1" : "0")
		<< " source=" << stage6FlipShaderSource << "/" << stage6FlipTcpSource
		<< std::endl;
	m_stage6MtfBlurEnabled = m_runtimeConfig.getBool(
		"Stage6MTF",
		"EnableMTFBlur",
		"EnableMTFBlur",
		false,
		&stage6MtfEnableSource);
	m_stage6MtfBlurMode = NormalizeStage6MtfBlurMode(m_runtimeConfig.getString(
		"Stage6MTF",
		"MTFBlurMode",
		"MTFBlurMode",
		"GaussianSeparable",
		&stage6MtfModeSource));
	m_stage6MtfBlurEffectiveMode = "GaussianSinglePassSeparableWeights";
	m_stage6MtfBlurSigmaPixels = m_runtimeConfig.getDouble(
		"Stage6MTF",
		"MTFBlurSigmaPixels",
		"MTFBlurSigmaPixels",
		0.65,
		&stage6MtfSigmaSource);
	if (!std::isfinite(m_stage6MtfBlurSigmaPixels) || m_stage6MtfBlurSigmaPixels < 0.0)
	{
		std::cout << "[Stage6 MTFConfig][WARN]"
			<< " invalid MTFBlurSigmaPixels=" << m_stage6MtfBlurSigmaPixels
			<< " fallback=0.65"
			<< std::endl;
		m_stage6MtfBlurSigmaPixels = 0.65;
	}
	m_stage6MtfBlurSigmaPixels = ClampStage5Double(m_stage6MtfBlurSigmaPixels, 0.0, 4.0);
	m_stage6MtfBlurRadiusPixels = m_runtimeConfig.getInt(
		"Stage6MTF",
		"MTFBlurRadiusPixels",
		"MTFBlurRadiusPixels",
		2,
		&stage6MtfRadiusSource);
	if (m_stage6MtfBlurRadiusPixels < 0)
	{
		m_stage6MtfBlurRadiusPixels = 0;
	}
	if (m_stage6MtfBlurRadiusPixels > 4)
	{
		std::cout << "[Stage6 MTFConfig][WARN]"
			<< " MTFBlurRadiusPixels=" << m_stage6MtfBlurRadiusPixels
			<< " clamp=4"
			<< " reason=shader_single_pass_max_radius"
			<< std::endl;
		m_stage6MtfBlurRadiusPixels = 4;
	}
	m_stage6MtfBlurPasses = m_runtimeConfig.getInt(
		"Stage6MTF",
		"MTFBlurPasses",
		"MTFBlurPasses",
		1,
		&stage6MtfPassesSource);
	if (m_stage6MtfBlurPasses != 1)
	{
		std::cout << "[Stage6 MTFConfig][WARN]"
			<< " MTFBlurPasses=" << m_stage6MtfBlurPasses
			<< " effectivePasses=1"
			<< " reason=stage6a_single_pass_gpu_approx"
			<< std::endl;
		m_stage6MtfBlurPasses = 1;
	}
	m_stage6MtfApplyTo = ToLowerAscii(m_runtimeConfig.getString(
		"Stage6MTF",
		"MTFApplyTo",
		"MTFApplyTo",
		"final_display",
		&stage6MtfApplyToSource));
	if (m_stage6MtfApplyTo != "final_display")
	{
		std::cout << "[Stage6 MTFConfig][WARN]"
			<< " MTFApplyTo=" << m_stage6MtfApplyTo
			<< " fallback=final_display"
			<< std::endl;
		m_stage6MtfApplyTo = "final_display";
	}
	m_stage6MtfDebugLog = m_runtimeConfig.getBool(
		"Stage6MTF",
		"MTFDebugLog",
		"MTFDebugLog",
		false,
		&stage6MtfDebugSource);
	m_stage6MtfLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage6MTF",
		"MTFLogEveryFrames",
		"MTFLogEveryFrames",
		120,
		&stage6MtfLogEverySource));
	std::cout << "[Stage6 MTFConfig]"
		<< " EnableMTFBlur=" << (m_stage6MtfBlurEnabled ? "1" : "0")
		<< " MTFBlurMode=" << m_stage6MtfBlurMode
		<< " effectiveMode=" << m_stage6MtfBlurEffectiveMode
		<< " MTFBlurSigmaPixels=" << m_stage6MtfBlurSigmaPixels
		<< " MTFBlurRadiusPixels=" << m_stage6MtfBlurRadiusPixels
		<< " MTFBlurPasses=" << m_stage6MtfBlurPasses
		<< " MTFApplyTo=" << m_stage6MtfApplyTo
		<< " MTFDebugLog=" << (m_stage6MtfDebugLog ? "1" : "0")
		<< " MTFLogEveryFrames=" << m_stage6MtfLogEveryFrames
		<< " implementation=single_pass_gpu_separable_weights"
		<< " todo=true_two_pass_separable_if_needed"
		<< " source=" << stage6MtfEnableSource << "/" << stage6MtfModeSource
		<< "/" << stage6MtfSigmaSource << "/" << stage6MtfRadiusSource
		<< "/" << stage6MtfPassesSource << "/" << stage6MtfApplyToSource
		<< "/" << stage6MtfDebugSource << "/" << stage6MtfLogEverySource
		<< std::endl;
	m_stage6DetectorNoiseEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableDetectorNoise",
		"EnableDetectorNoise",
		false,
		&stage6NoiseEnableSource);
	m_stage6DetectorNoiseApplyTo = ToLowerAscii(m_runtimeConfig.getString(
		"Stage6DetectorNoise",
		"NoiseApplyTo",
		"NoiseApplyTo",
		"final_display",
		&stage6NoiseApplyToSource));
	if (m_stage6DetectorNoiseApplyTo != "final_display")
	{
		std::cout << "[Stage6 NoiseConfig][WARN]"
			<< " NoiseApplyTo=" << m_stage6DetectorNoiseApplyTo
			<< " fallback=final_display"
			<< std::endl;
		m_stage6DetectorNoiseApplyTo = "final_display";
	}
	m_stage6DetectorNoisePosition = NormalizeStage6NoisePosition(m_runtimeConfig.getString(
		"Stage6DetectorNoise",
		"NoisePosition",
		"NoisePosition",
		"BeforeAGC",
		&stage6NoisePositionSource));
	m_stage6DetectorNoisePositionCode = Stage6NoisePositionCode(m_stage6DetectorNoisePosition);
	m_stage6TemporalNoiseEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableTemporalNoise",
		"EnableTemporalNoise",
		true,
		&stage6TemporalEnableSource);
	m_stage6TemporalNoiseSigmaGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"TemporalNoiseSigmaGray",
		"TemporalNoiseSigmaGray",
		0.005,
		&stage6TemporalSigmaSource), 0.0, 0.25);
	m_stage6FpnEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableFPN",
		"EnableFPN",
		false,
		&stage6FpnEnableSource);
	m_stage6FpnSigmaGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"FPNSigmaGray",
		"FPNSigmaGray",
		0.003,
		&stage6FpnSigmaSource), 0.0, 0.25);
	m_stage6FpnSeed = m_runtimeConfig.getInt(
		"Stage6DetectorNoise",
		"FPNSeed",
		"FPNSeed",
		12345,
		&stage6FpnSeedSource);
	m_stage6ColumnNoiseEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableColumnNoise",
		"EnableColumnNoise",
		false,
		&stage6ColumnEnableSource);
	m_stage6ColumnNoiseSigmaGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"ColumnNoiseSigmaGray",
		"ColumnNoiseSigmaGray",
		0.002,
		&stage6ColumnSigmaSource), 0.0, 0.25);
	m_stage6RowNoiseEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableRowNoise",
		"EnableRowNoise",
		false,
		&stage6RowEnableSource);
	m_stage6RowNoiseSigmaGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"RowNoiseSigmaGray",
		"RowNoiseSigmaGray",
		0.001,
		&stage6RowSigmaSource), 0.0, 0.25);
	m_stage6BadPixelsEnabled = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"EnableBadPixels",
		"EnableBadPixels",
		false,
		&stage6BadPixelsEnableSource);
	m_stage6BadPixelRatio = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"BadPixelRatio",
		"BadPixelRatio",
		0.0001,
		&stage6BadPixelRatioSource), 0.0, 0.1);
	m_stage6BadPixelHotGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"BadPixelHotGray",
		"BadPixelHotGray",
		1.0,
		&stage6BadPixelHotSource), 0.0, 1.0);
	m_stage6BadPixelDeadGray = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"BadPixelDeadGray",
		"BadPixelDeadGray",
		0.0,
		&stage6BadPixelDeadSource), 0.0, 1.0);
	m_stage6NoiseClampMin = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"NoiseClampMin",
		"NoiseClampMin",
		0.0,
		&stage6NoiseClampMinSource), 0.0, 1.0);
	m_stage6NoiseClampMax = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage6DetectorNoise",
		"NoiseClampMax",
		"NoiseClampMax",
		1.0,
		&stage6NoiseClampMaxSource), 0.0, 1.0);
	if (m_stage6NoiseClampMax <= m_stage6NoiseClampMin)
	{
		std::cout << "[Stage6 NoiseConfig][WARN]"
			<< " NoiseClampMin=" << m_stage6NoiseClampMin
			<< " NoiseClampMax=" << m_stage6NoiseClampMax
			<< " fallback=0/1"
			<< std::endl;
		m_stage6NoiseClampMin = 0.0;
		m_stage6NoiseClampMax = 1.0;
	}
	m_stage6NoiseDebugLog = m_runtimeConfig.getBool(
		"Stage6DetectorNoise",
		"NoiseDebugLog",
		"NoiseDebugLog",
		false,
		&stage6NoiseDebugSource);
	m_stage6NoiseLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage6DetectorNoise",
		"NoiseLogEveryFrames",
		"NoiseLogEveryFrames",
		120,
		&stage6NoiseLogEverySource));
	m_stage6NoiseLogCounter = 0;
	m_lastStage6NoiseLogState.clear();
	std::cout << "[Stage6 NoiseConfig]"
		<< " EnableDetectorNoise=" << (m_stage6DetectorNoiseEnabled ? "1" : "0")
		<< " NoiseApplyTo=" << m_stage6DetectorNoiseApplyTo
		<< " NoisePosition=" << m_stage6DetectorNoisePosition
		<< " EnableTemporalNoise=" << (m_stage6TemporalNoiseEnabled ? "1" : "0")
		<< " TemporalNoiseSigmaGray=" << m_stage6TemporalNoiseSigmaGray
		<< " EnableFPN=" << (m_stage6FpnEnabled ? "1" : "0")
		<< " FPNSigmaGray=" << m_stage6FpnSigmaGray
		<< " FPNSeed=" << m_stage6FpnSeed
		<< " EnableColumnNoise=" << (m_stage6ColumnNoiseEnabled ? "1" : "0")
		<< " ColumnNoiseSigmaGray=" << m_stage6ColumnNoiseSigmaGray
		<< " EnableRowNoise=" << (m_stage6RowNoiseEnabled ? "1" : "0")
		<< " RowNoiseSigmaGray=" << m_stage6RowNoiseSigmaGray
		<< " EnableBadPixels=" << (m_stage6BadPixelsEnabled ? "1" : "0")
		<< " BadPixelRatio=" << m_stage6BadPixelRatio
		<< " NoiseClampMin=" << m_stage6NoiseClampMin
		<< " NoiseClampMax=" << m_stage6NoiseClampMax
		<< " NoiseDebugLog=" << (m_stage6NoiseDebugLog ? "1" : "0")
		<< " NoiseLogEveryFrames=" << m_stage6NoiseLogEveryFrames
		<< " implementation=single_pass_gpu_hash_noise"
		<< " source=" << stage6NoiseEnableSource << "/" << stage6NoiseApplyToSource
		<< "/" << stage6NoisePositionSource << "/" << stage6TemporalEnableSource
		<< "/" << stage6TemporalSigmaSource << "/" << stage6FpnEnableSource
		<< "/" << stage6FpnSigmaSource << "/" << stage6FpnSeedSource
		<< "/" << stage6ColumnEnableSource << "/" << stage6ColumnSigmaSource
		<< "/" << stage6RowEnableSource << "/" << stage6RowSigmaSource
		<< "/" << stage6BadPixelsEnableSource << "/" << stage6BadPixelRatioSource
		<< "/" << stage6BadPixelHotSource << "/" << stage6BadPixelDeadSource
		<< "/" << stage6NoiseClampMinSource << "/" << stage6NoiseClampMaxSource
		<< "/" << stage6NoiseDebugSource << "/" << stage6NoiseLogEverySource
		<< std::endl;
	m_stage6AgcEnabled = m_runtimeConfig.getBool(
		"Stage6AGC",
		"EnableAGC",
		"EnableAGC",
		false,
		&stage6AgcEnableSource);
	m_stage6AgcMode = NormalizeStage6AgcMode(m_runtimeConfig.getString(
		"Stage6AGC",
		"AGCMode",
		"AGCMode",
		"Percentile",
		&stage6AgcModeSource));
	m_stage6AgcModeCode = Stage6AgcModeCode(m_stage6AgcMode);
	m_stage6AgcApplyTo = ToLowerAscii(m_runtimeConfig.getString(
		"Stage6AGC",
		"AGCApplyTo",
		"AGCApplyTo",
		"final_display",
		&stage6AgcApplyToSource));
	if (m_stage6AgcApplyTo != "final_display")
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " AGCApplyTo=" << m_stage6AgcApplyTo
			<< " fallback=final_display"
			<< std::endl;
		m_stage6AgcApplyTo = "final_display";
	}
	m_stage6AgcStatsSource = ToLowerAscii(m_runtimeConfig.getString(
		"Stage6AGC",
		"AGCStatsSource",
		"AGCStatsSource",
		"previous_readback",
		&stage6AgcStatsSource));
	if (m_stage6AgcStatsSource != "previous_readback")
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " AGCStatsSource=" << m_stage6AgcStatsSource
			<< " fallback=previous_readback"
			<< std::endl;
		m_stage6AgcStatsSource = "previous_readback";
	}
	m_stage6AgcUpdateHz = m_runtimeConfig.getDouble(
		"Stage6AGC",
		"AGCUpdateHz",
		"AGCUpdateHz",
		30.0,
		&stage6AgcUpdateHzSource);
	if (!std::isfinite(m_stage6AgcUpdateHz) || m_stage6AgcUpdateHz <= 0.0)
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " invalid AGCUpdateHz=" << m_stage6AgcUpdateHz
			<< " fallback=30"
			<< std::endl;
		m_stage6AgcUpdateHz = 30.0;
	}
	m_stage6AgcUpdateHz = ClampStage5Double(m_stage6AgcUpdateHz, 1.0, 120.0);
	m_stage6AgcLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage6AGC",
		"AGCLogEveryFrames",
		"AGCLogEveryFrames",
		120,
		&stage6AgcLogEverySource));
	m_stage6AgcLowPercentile = m_runtimeConfig.getDouble(
		"Stage6AGC",
		"AGCLowPercentile",
		"AGCLowPercentile",
		2.0,
		&stage6AgcLowPercentileSource);
	m_stage6AgcHighPercentile = m_runtimeConfig.getDouble(
		"Stage6AGC",
		"AGCHighPercentile",
		"AGCHighPercentile",
		98.0,
		&stage6AgcHighPercentileSource);
	if (!std::isfinite(m_stage6AgcLowPercentile) || !std::isfinite(m_stage6AgcHighPercentile))
	{
		m_stage6AgcLowPercentile = 2.0;
		m_stage6AgcHighPercentile = 98.0;
	}
	m_stage6AgcLowPercentile = ClampStage5Double(m_stage6AgcLowPercentile, 0.0, 99.0);
	m_stage6AgcHighPercentile = ClampStage5Double(m_stage6AgcHighPercentile, 1.0, 100.0);
	if (m_stage6AgcHighPercentile <= m_stage6AgcLowPercentile)
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " AGCLowPercentile=" << m_stage6AgcLowPercentile
			<< " AGCHighPercentile=" << m_stage6AgcHighPercentile
			<< " fallback=2/98"
			<< std::endl;
		m_stage6AgcLowPercentile = 2.0;
		m_stage6AgcHighPercentile = 98.0;
	}
	m_stage6AgcMeanStdK = m_runtimeConfig.getDouble(
		"Stage6AGC",
		"AGCMeanStdK",
		"AGCMeanStdK",
		2.5,
		&stage6AgcMeanStdKSource);
	if (!std::isfinite(m_stage6AgcMeanStdK))
	{
		m_stage6AgcMeanStdK = 2.5;
	}
	m_stage6AgcMeanStdK = ClampStage5Double(m_stage6AgcMeanStdK, 0.1, 8.0);
	m_stage6AgcMinGain = m_runtimeConfig.getDouble("Stage6AGC", "AGCMinGain", "AGCMinGain", 0.25, &stage6AgcMinGainSource);
	m_stage6AgcMaxGain = m_runtimeConfig.getDouble("Stage6AGC", "AGCMaxGain", "AGCMaxGain", 8.0, &stage6AgcMaxGainSource);
	if (!std::isfinite(m_stage6AgcMinGain) || !std::isfinite(m_stage6AgcMaxGain) || m_stage6AgcMaxGain <= m_stage6AgcMinGain)
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " invalidGainRange min=" << m_stage6AgcMinGain
			<< " max=" << m_stage6AgcMaxGain
			<< " fallback=0.25/8"
			<< std::endl;
		m_stage6AgcMinGain = 0.25;
		m_stage6AgcMaxGain = 8.0;
	}
	m_stage6AgcMinOffset = m_runtimeConfig.getDouble("Stage6AGC", "AGCMinOffset", "AGCMinOffset", -1.0, &stage6AgcMinOffsetSource);
	m_stage6AgcMaxOffset = m_runtimeConfig.getDouble("Stage6AGC", "AGCMaxOffset", "AGCMaxOffset", 1.0, &stage6AgcMaxOffsetSource);
	if (!std::isfinite(m_stage6AgcMinOffset) || !std::isfinite(m_stage6AgcMaxOffset) || m_stage6AgcMaxOffset <= m_stage6AgcMinOffset)
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " invalidOffsetRange min=" << m_stage6AgcMinOffset
			<< " max=" << m_stage6AgcMaxOffset
			<< " fallback=-1/1"
			<< std::endl;
		m_stage6AgcMinOffset = -1.0;
		m_stage6AgcMaxOffset = 1.0;
	}
	m_stage6AgcSmoothingAlpha = m_runtimeConfig.getDouble(
		"Stage6AGC",
		"AGCSmoothingAlpha",
		"AGCSmoothingAlpha",
		0.15,
		&stage6AgcSmoothingSource);
	if (!std::isfinite(m_stage6AgcSmoothingAlpha))
	{
		m_stage6AgcSmoothingAlpha = 0.15;
	}
	m_stage6AgcSmoothingAlpha = ClampStage5Double(m_stage6AgcSmoothingAlpha, 0.0, 1.0);
	m_stage6AgcTargetLowGray = m_runtimeConfig.getDouble("Stage6AGC", "AGCTargetLowGray", "AGCTargetLowGray", 0.05, &stage6AgcTargetLowSource);
	m_stage6AgcTargetHighGray = m_runtimeConfig.getDouble("Stage6AGC", "AGCTargetHighGray", "AGCTargetHighGray", 0.95, &stage6AgcTargetHighSource);
	if (!std::isfinite(m_stage6AgcTargetLowGray) || !std::isfinite(m_stage6AgcTargetHighGray))
	{
		m_stage6AgcTargetLowGray = 0.05;
		m_stage6AgcTargetHighGray = 0.95;
	}
	m_stage6AgcTargetLowGray = ClampStage5Double(m_stage6AgcTargetLowGray, 0.0, 1.0);
	m_stage6AgcTargetHighGray = ClampStage5Double(m_stage6AgcTargetHighGray, 0.0, 1.0);
	if (m_stage6AgcTargetHighGray <= m_stage6AgcTargetLowGray)
	{
		std::cout << "[Stage6 AGCConfig][WARN]"
			<< " AGCTargetLowGray=" << m_stage6AgcTargetLowGray
			<< " AGCTargetHighGray=" << m_stage6AgcTargetHighGray
			<< " fallback=0.05/0.95"
			<< std::endl;
		m_stage6AgcTargetLowGray = 0.05;
		m_stage6AgcTargetHighGray = 0.95;
	}
	m_stage6AgcStride = m_runtimeConfig.getInt(
		"Stage6AGC",
		"AGCStride",
		"AGCStride",
		8,
		&stage6AgcStrideSource);
	m_stage6AgcStride = std::max(1, std::min(64, m_stage6AgcStride));
	m_stage6AgcExcludeAnnotationOverlay = m_runtimeConfig.getBool(
		"Stage6AGC",
		"AGCExcludeAnnotationOverlay",
		"AGCExcludeAnnotationOverlay",
		true,
		&stage6AgcExcludeAnnotationSource);
	m_stage6AgcDebugLog = m_runtimeConfig.getBool(
		"Stage6AGC",
		"AGCDebugLog",
		"AGCDebugLog",
		false,
		&stage6AgcDebugSource);
	m_stage6AgcGain = 1.0;
	m_stage6AgcOffset = 0.0;
	m_stage6AgcLowInput = 0.0;
	m_stage6AgcHighInput = 1.0;
	m_stage6AgcTargetGain = 1.0;
	m_stage6AgcTargetOffset = 0.0;
	m_stage6AgcSampleCount = 0;
	m_stage6AgcValid = false;
	m_stage6AgcInitialized = false;
	m_stage6AgcFallbackReason = m_stage6AgcEnabled ? "startup" : "disabled";
	m_stage6AgcStatsMsCurrent = 0.0;
	m_stage6AgcApplyMsCurrent = 0.0;
	m_stage6AgcLastUpdateNs = 0;
	m_stage6AgcLastUpdateSourceSeq = 0;
	m_stage6AgcLogCounter = 0;
	m_lastStage6AgcLogState.clear();
	std::cout << "[Stage6 AGCConfig]"
		<< " EnableAGC=" << (m_stage6AgcEnabled ? "1" : "0")
		<< " AGCMode=" << m_stage6AgcMode
		<< " AGCApplyTo=" << m_stage6AgcApplyTo
		<< " AGCStatsSource=" << m_stage6AgcStatsSource
		<< " AGCUpdateHz=" << m_stage6AgcUpdateHz
		<< " AGCLogEveryFrames=" << m_stage6AgcLogEveryFrames
		<< " AGCLowPercentile=" << m_stage6AgcLowPercentile
		<< " AGCHighPercentile=" << m_stage6AgcHighPercentile
		<< " AGCMeanStdK=" << m_stage6AgcMeanStdK
		<< " AGCMinGain=" << m_stage6AgcMinGain
		<< " AGCMaxGain=" << m_stage6AgcMaxGain
		<< " AGCMinOffset=" << m_stage6AgcMinOffset
		<< " AGCMaxOffset=" << m_stage6AgcMaxOffset
		<< " AGCSmoothingAlpha=" << m_stage6AgcSmoothingAlpha
		<< " AGCTargetLowGray=" << m_stage6AgcTargetLowGray
		<< " AGCTargetHighGray=" << m_stage6AgcTargetHighGray
		<< " AGCStride=" << m_stage6AgcStride
		<< " AGCExcludeAnnotationOverlay=" << (m_stage6AgcExcludeAnnotationOverlay ? "1" : "0")
		<< " AGCDebugLog=" << (m_stage6AgcDebugLog ? "1" : "0")
		<< " implementation=previous_readback_histogram"
		<< " source=" << stage6AgcEnableSource << "/" << stage6AgcModeSource
		<< "/" << stage6AgcApplyToSource << "/" << stage6AgcStatsSource
		<< "/" << stage6AgcUpdateHzSource << "/" << stage6AgcLogEverySource
		<< "/" << stage6AgcLowPercentileSource << "/" << stage6AgcHighPercentileSource
		<< "/" << stage6AgcMeanStdKSource
		<< "/" << stage6AgcMinGainSource << "/" << stage6AgcMaxGainSource
		<< "/" << stage6AgcMinOffsetSource << "/" << stage6AgcMaxOffsetSource
		<< "/" << stage6AgcSmoothingSource
		<< "/" << stage6AgcTargetLowSource << "/" << stage6AgcTargetHighSource
		<< "/" << stage6AgcStrideSource << "/" << stage6AgcExcludeAnnotationSource
		<< "/" << stage6AgcDebugSource
		<< std::endl;
	m_tcpCodecConfig = ToLowerAscii(m_runtimeConfig.getString(
		"TcpOutput", "Codec", "TcpOutputCodec", "auto", &tcpCodecSource));
	if (m_tcpCodecConfig != "auto" && m_tcpCodecConfig != "jpeg" && m_tcpCodecConfig != "h264")
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid Codec=" << m_tcpCodecConfig
			<< " fallback=auto"
			<< std::endl;
		m_tcpCodecConfig = "auto";
	}
	m_tcpJpegQuality = m_runtimeConfig.getInt(
		"TcpOutput", "JpegQuality", "JpegQuality", 100, &jpegQualitySource);
	if (m_tcpJpegQuality < 40 || m_tcpJpegQuality > 100)
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid JpegQuality=" << m_tcpJpegQuality
			<< " validRange=40..100 fallback=100"
			<< std::endl;
		m_tcpJpegQuality = 100;
	}
	const std::string jpegMode = ToLowerAscii(m_runtimeConfig.getString(
		"TcpOutput", "JpegEncodeMode", "JpegEncodeMode", "rgb", &jpegModeSource));
	if (jpegMode != "rgb" && jpegMode != "gray")
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid JpegEncodeMode=" << jpegMode
			<< " fallback=rgb"
			<< std::endl;
		m_tcpJpegGray = false;
	}
	else
	{
		m_tcpJpegGray = jpegMode == "gray";
	}
	m_tcpJpegGrayConvertMethod = ToLowerAscii(m_runtimeConfig.getString(
		"TcpOutput",
		"JpegGrayConvertMethod",
		"JpegGrayConvertMethod",
		"luma",
		&jpegGrayMethodSource));
	if (m_tcpJpegGrayConvertMethod != "luma")
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid JpegGrayConvertMethod=" << m_tcpJpegGrayConvertMethod
			<< " fallback=luma"
			<< std::endl;
		m_tcpJpegGrayConvertMethod = "luma";
	}
	m_enableH264Experimental = m_runtimeConfig.getBool(
		"TcpOutput",
		"EnableH264Experimental",
		"EnableH264Experimental",
		false,
		&h264ExperimentalSource);
	m_h264FallbackToJpeg = m_runtimeConfig.getBool(
		"TcpOutput",
		"H264FallbackToJpeg",
		"H264FallbackToJpeg",
		true,
		&h264FallbackSource);
	m_h264Encoder = ToLowerAscii(m_runtimeConfig.getString(
		"TcpOutput",
		"H264Encoder",
		"H264Encoder",
		"auto",
		&h264EncoderSource));
	if (m_h264Encoder != "auto" &&
		m_h264Encoder != "opencv" &&
		m_h264Encoder != "opencv_videowriter" &&
		m_h264Encoder != "mediafoundation" &&
		m_h264Encoder != "mf" &&
		m_h264Encoder != "mpp" &&
		m_h264Encoder != "rk_mpp")
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid H264Encoder=" << m_h264Encoder
			<< " fallback=auto"
			<< std::endl;
		m_h264Encoder = "auto";
	}
	m_h264BitrateKbps = m_runtimeConfig.getInt(
		"TcpOutput",
		"H264BitrateKbps",
		"H264BitrateKbps",
		4000,
		&h264BitrateSource);
	if (m_h264BitrateKbps < 256 || m_h264BitrateKbps > 50000)
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid H264BitrateKbps=" << m_h264BitrateKbps
			<< " validRange=256..50000 fallback=4000"
			<< std::endl;
		m_h264BitrateKbps = 4000;
	}
	m_h264GopFrames = m_runtimeConfig.getInt(
		"TcpOutput",
		"H264GopFrames",
		"H264GopFrames",
		30,
		&h264GopSource);
	if (m_h264GopFrames < 1 || m_h264GopFrames > 300)
	{
		std::cout << "[TcpOutputConfig][WARN]"
			<< " invalid H264GopFrames=" << m_h264GopFrames
			<< " validRange=1..300 fallback=30"
			<< std::endl;
		m_h264GopFrames = 30;
	}
	m_h264LowLatency = m_runtimeConfig.getBool(
		"TcpOutput",
		"H264LowLatency",
		"H264LowLatency",
		true,
		&h264LowLatencySource);
	m_h264ForceKeyFrameOnStart = m_runtimeConfig.getBool(
		"TcpOutput",
		"H264ForceKeyFrameOnStart",
		"H264ForceKeyFrameOnStart",
		true,
		&h264ForceKeySource);
	m_jpegPerfABTest = m_runtimeConfig.getBool(
		"TcpOutput",
		"JpegPerfABTest",
		"JpegPerfABTest",
		false,
		&jpegAbTestSource);
	if (m_pTcpThread)
	{
		m_pTcpThread->configureOutput(
			m_tcpJpegQuality,
			m_tcpJpegGray,
			m_enableH264Experimental,
			m_h264FallbackToJpeg,
			m_h264Encoder,
			m_h264BitrateKbps,
			m_h264GopFrames,
			m_h264LowLatency,
			m_h264ForceKeyFrameOnStart,
			m_tcpCodecConfig);
	}
	std::cout << "[TcpOutputConfig]"
		<< " Codec=" << m_tcpCodecConfig
		<< " JpegQuality=" << m_tcpJpegQuality
		<< " JpegEncodeMode=" << (m_tcpJpegGray ? "gray" : "rgb")
		<< " JpegGrayConvertMethod=" << m_tcpJpegGrayConvertMethod
		<< " EnableH264Experimental=" << (m_enableH264Experimental ? "1" : "0")
		<< " H264FallbackToJpeg=" << (m_h264FallbackToJpeg ? "1" : "0")
		<< " H264Encoder=" << m_h264Encoder
		<< " H264BitrateKbps=" << m_h264BitrateKbps
		<< " H264GopFrames=" << m_h264GopFrames
		<< " H264LowLatency=" << (m_h264LowLatency ? "1" : "0")
		<< " H264ForceKeyFrameOnStart=" << (m_h264ForceKeyFrameOnStart ? "1" : "0")
		<< " JpegPerfABTest=" << (m_jpegPerfABTest ? "1" : "0")
		<< " source=" << tcpCodecSource << "/" << jpegQualitySource << "/" << jpegModeSource
		<< "/" << jpegGrayMethodSource << "/" << h264ExperimentalSource
		<< "/" << h264FallbackSource << "/" << h264EncoderSource
		<< "/" << h264BitrateSource << "/" << h264GopSource
		<< "/" << h264LowLatencySource << "/" << h264ForceKeySource
		<< "/" << jpegAbTestSource
		<< std::endl;
	m_enableStage4HotspotVisualDebug = m_runtimeConfig.getBool("Stage4", "EnableHotspotVisualDebug", "EnableStage4HotspotVisualDebug", false, &stage4VisualSource);
	m_forceStage4BrightSpotVisible = m_runtimeConfig.getBool("Stage4", "ForceBrightSpotVisible", "ForceStage4BrightSpotVisible", false, &stage4BrightSource);
	m_forceStage4RearHotspotVisible = m_runtimeConfig.getBool("Stage4", "ForceRearHotspotVisible", "ForceStage4RearHotspotVisible", false, &stage4RearSource);
	m_stage4LegacyEngineBodyHeating = m_runtimeConfig.getBool("Stage4", "LegacyEngineBodyHeating", "Stage4LegacyEngineBodyHeating", false, &stage4LegacyEngineBodyHeatingSource);
	m_stage4UpdateHz = m_runtimeConfig.getDouble("Stage4", "Stage4UpdateHz", "Stage4UpdateHz", 30.0, &stage4UpdateHzSource);
	if (!std::isfinite(m_stage4UpdateHz) || m_stage4UpdateHz <= 0.0)
	{
		m_stage4UpdateHz = 30.0;
	}
	m_stage4UpdateHz = std::max(1.0, std::min(240.0, m_stage4UpdateHz));
	if (m_stage4LegacyEngineBodyHeating)
	{
		std::cout << "[Stage4][WARN]"
			<< " LegacyEngineBodyHeating=1"
			<< " reason=legacy_debug_mode_engineState_heats_whole_body"
			<< std::endl;
	}
	AnnotationRuntimeOptions annotationOptions;
	annotationOptions.profilePath = m_runtimeConfig.getString("Annotation", "ProfilePath", "AnnotationProfilePath", "Config/Annotation/annotation_profiles.json", &annotationProfileSource);
	annotationOptions.profilePathSource = annotationProfileSource;
	annotationOptions.drawOptions.debugOverlay = m_runtimeConfig.getBool("Annotation", "DebugOverlay", "AnnotationDebugOverlay", false, &annotationDebugSource);
	annotationOptions.bboxMode = m_runtimeConfig.getString("Annotation", "BBoxMode", "AnnotationBBoxMode", "mesh_body", &annotationBBoxModeSource);
	annotationOptions.bboxMarginPx = std::max(0, m_runtimeConfig.getInt("Annotation", "BBoxMarginPx", "AnnotationBBoxMarginPx", 3, &annotationBBoxMarginSource));
	annotationOptions.minBBoxSizePx = std::max(1, m_runtimeConfig.getInt("Annotation", "MinBBoxSizePx", "AnnotationMinBBoxSizePx", 4, &annotationMinBBoxSource));
	annotationOptions.drawOptions.drawKeyPoints = m_runtimeConfig.getBool("Annotation", "DrawKeyPoints", "AnnotationDrawKeyPoints", true, &annotationDrawKeyPointsSource);
	annotationOptions.drawOptions.drawModelLabel = m_runtimeConfig.getBool("Annotation", "DrawModelLabel", "AnnotationDrawModelLabel", true, &annotationDrawModelLabelSource);
	annotationOptions.drawOptions.drawBBox = m_runtimeConfig.getBool("Annotation", "DrawBBox", "AnnotationDrawBBox", true, &annotationDrawBBoxSource);
	annotationOptions.occlusion.enabled = m_runtimeConfig.getBool("Annotation", "OcclusionEnable", "AnnotationOcclusionEnable", true, &annotationOcclusionEnableSource);
	annotationOptions.occlusion.mode = m_runtimeConfig.getString("Annotation", "OcclusionMode", "AnnotationOcclusionMode", "mesh_collision", &annotationOcclusionModeSource);
	annotationOptions.occlusion.epsilonM = static_cast<float>(std::max(0.0, m_runtimeConfig.getDouble("Annotation", "OcclusionEpsilonM", "AnnotationOcclusionEpsilonM", 0.25, &annotationOcclusionEpsilonSource)));
	annotationOptions.occlusion.collisionMaskBit = std::max(0, std::min(31, m_runtimeConfig.getInt("Annotation", "OcclusionCollisionMaskBit", "AnnotationOcclusionCollisionMaskBit", 20, &annotationOcclusionMaskSource)));
	annotationOptions.occlusion.selfTarget = m_runtimeConfig.getBool("Annotation", "OcclusionSelfTarget", "AnnotationOcclusionSelfTarget", false, &annotationOcclusionSelfSource);
	annotationOptions.surfaceKeyPointEnabled = m_runtimeConfig.getBool("Annotation", "SurfaceKeyPointEnable", "AnnotationSurfaceKeyPointEnable", true, &annotationSurfaceKeyPointSource);
	annotationOptions.surfaceSnapEnabled = m_runtimeConfig.getBool("Annotation", "SurfaceSnapEnable", "AnnotationSurfaceSnapEnable", false, &annotationSurfaceSnapSource);
	annotationOptions.surfaceSnapMode = m_runtimeConfig.getString("Annotation", "SurfaceSnapMode", "AnnotationSurfaceSnapMode", "profile_surface", &annotationSurfaceSnapModeSource);
	m_annotationUpdateHz = m_runtimeConfig.getDouble("Annotation", "UpdateHz", "AnnotationUpdateHz", 15.0, &annotationUpdateHzSource);
	if (!std::isfinite(m_annotationUpdateHz) || m_annotationUpdateHz <= 0.0)
	{
		m_annotationUpdateHz = 15.0;
	}
	m_annotationUpdateHz = std::max(1.0, std::min(240.0, m_annotationUpdateHz));
	std::cout << "[AnnotationConfig]"
		<< " UpdateHz=" << m_annotationUpdateHz
		<< " source=" << annotationUpdateHzSource
		<< std::endl;
	m_annotationManager.loadProfileFromCandidates(BuildRuntimeConfigPathCandidates(annotationOptions.profilePath), annotationOptions.profilePath, annotationProfileSource);
	m_annotationManager.applyRuntimeOptions(annotationOptions);
	m_stage5DebugDisplayConfigPath = FirstExistingPath(stage5DebugConfigPaths);
	m_stage5DebugDisplayConfigReady = LoadStage5DebugDisplayConfig(m_stage5DebugDisplayConfigPath, m_stage5DebugConfigs, m_stage5UseBaseTextureModulationByBand);
	ApplyStage5RuntimeOverrides(m_runtimeConfig, m_stage5DebugConfigs, m_stage5UseBaseTextureModulationByBand);
	std::string stage5DebugSource;
	std::string stage5ViewModeSource;
	std::string stage5PhysicalSource;
	std::string stage5ComponentLogSource;
	std::string stage5ComponentLogEverySource;
	std::string stage5SensorInputDisplaySource;
	std::string stage5SensorInputDisplayModeSource;
	std::string stage5SensorInputDisplayScaleSource;
	std::string stage5SensorInputDisplayOffsetSource;
	std::string stage5SensorInputDisplayClampMinSource;
	std::string stage5SensorInputDisplayClampMaxSource;
	std::string stage5SensorInputDisplayGammaSource;
	std::string stage5SensorInputDisplayBandSource;
	std::string stage5AeroEnableSource;
	std::string stage5AeroApplySource;
	std::string stage5AeroDebugLogSource;
	std::string stage5AeroLogEverySource;
	std::string stage5AeroRecoverySource;
	std::string stage5AeroGammaSource;
	std::string stage5AeroBodyCoeffSource;
	std::string stage5AeroNoseCoeffSource;
	std::string stage5AeroEdgeCoeffSource;
	std::string stage5AeroRearCoeffSource;
	std::string stage5AeroHeatTauSource;
	std::string stage5AeroCoolTauSource;
	std::string stage5AeroMachMinSource;
	std::string stage5AeroMachMaxSource;
	std::string stage5AeroDeltaMaxSource;
	std::string stage5AeroApplyScaleSource;
	std::string stage5AeroApplyClampBodyDeltaSource;
	std::string stage5AeroApplyOnlyBandSource;
	std::string stage5ModtranDebugSource;
	std::string stage5ModtranPathRuntimeSource;
	std::string stage5ModtranSkyRuntimeSource;
	std::string stage5ModtranSolarRuntimeSource;
	std::string stage5ModtranPreferredSourceSource;
	std::string stage5ModtranLogEverySource;
	std::string stage5ModtranCompareLegacySource;
	std::string stage5ModtranPathRuntimeBandSource;
	std::string stage5ModtranPathRuntimeModeSource;
	std::string stage5ModtranPathUnitModeSource;
	std::string stage5ModtranPathScaleSource;
	std::string stage5ModtranPathOffsetSource;
	std::string stage5ModtranPathClampMinSource;
	std::string stage5ModtranPathClampMaxSource;
	std::string stage5ModtranPathBlendSource;
	std::string stage5ModtranPathABLogSource;
	std::string stage5DumpSource;
	std::string stage5DumpPathSource;
	std::string stage5DumpEverySource;
	m_enableStage5PhysicalPipeline = m_runtimeConfig.getBool(
		"Stage5Radiance",
		"EnableIRPhysicalPipeline",
		"EnableIRPhysicalPipeline",
		true,
		&stage5PhysicalSource);
	const bool legacyStage5Debug = m_runtimeConfig.getBool("Stage5", "EnableRadianceDebug", "EnableStage5RadianceDebug", false, &stage5DebugSource);
	std::string stage5DebugViewText = m_runtimeConfig.getString(
		"Stage5Radiance",
		"DebugView",
		"Stage5RadianceDebugView",
		"Off",
		&stage5ViewModeSource);
	if (legacyStage5Debug)
	{
		stage5DebugViewText = m_runtimeConfig.getString(
			"Stage5",
			"DebugViewMode",
			"Stage5DebugViewMode",
			"SensorInput",
			&stage5ViewModeSource);
	}
	m_stage5DebugViewMode = ParseStage5DebugViewMode(stage5DebugViewText);
	m_stage5DebugViewModeName = Stage5DebugViewModeName(m_stage5DebugViewMode);
	m_enableStage5RadianceDebug = m_stage5DebugViewMode != 0;
	m_stage5LogComponents = m_runtimeConfig.getBool(
		"Stage5Radiance",
		"LogComponents",
		"Stage5RadianceLogComponents",
		false,
		&stage5ComponentLogSource);
	m_stage5ComponentLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage5Radiance",
		"ComponentLogEveryFrames",
		"Stage5RadianceComponentLogEveryFrames",
		120,
		&stage5ComponentLogEverySource));
	m_stage5UseSensorInputForDisplay = m_runtimeConfig.getBool(
		"Stage5Radiance",
		"UseSensorInputForDisplay",
		"UseSensorInputForDisplay",
		false,
		&stage5SensorInputDisplaySource);
	m_stage5SensorInputDisplayMode = NormalizeStage5SensorInputDisplayMode(m_runtimeConfig.getString(
		"Stage5Radiance",
		"SensorInputDisplayMode",
		"SensorInputDisplayMode",
		"Manual",
		&stage5SensorInputDisplayModeSource));
	m_stage5SensorInputDisplayScale = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5Radiance",
		"SensorInputDisplayScale",
		"SensorInputDisplayScale",
		1.0,
		&stage5SensorInputDisplayScaleSource));
	m_stage5SensorInputDisplayOffset = m_runtimeConfig.getDouble(
		"Stage5Radiance",
		"SensorInputDisplayOffset",
		"SensorInputDisplayOffset",
		0.0,
		&stage5SensorInputDisplayOffsetSource);
	m_stage5SensorInputDisplayClampMin = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5Radiance",
		"SensorInputDisplayClampMin",
		"SensorInputDisplayClampMin",
		0.0,
		&stage5SensorInputDisplayClampMinSource), 0.0, 1.0);
	m_stage5SensorInputDisplayClampMax = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5Radiance",
		"SensorInputDisplayClampMax",
		"SensorInputDisplayClampMax",
		1.0,
		&stage5SensorInputDisplayClampMaxSource), 0.0, 1.0);
	if (m_stage5SensorInputDisplayClampMax < m_stage5SensorInputDisplayClampMin)
	{
		m_stage5SensorInputDisplayClampMax = m_stage5SensorInputDisplayClampMin;
	}
	m_stage5SensorInputDisplayGamma = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5Radiance",
		"SensorInputDisplayGamma",
		"SensorInputDisplayGamma",
		1.0,
		&stage5SensorInputDisplayGammaSource), 0.1, 5.0);
	{
		const std::string displayBandText = m_runtimeConfig.getString(
			"Stage5Radiance",
			"SensorInputDisplayBand",
			"SensorInputDisplayBand",
			"MWIR",
			&stage5SensorInputDisplayBandSource);
		m_stage5SensorInputDisplayBand = ParseStage5ModtranPathRuntimeBand(displayBandText, m_stage5SensorInputDisplayBandName);
	}
	m_stage5AeroThermalEnabled = m_runtimeConfig.getBool(
		"Stage5AeroThermal",
		"EnableAeroThermalModel",
		"EnableAeroThermalModel",
		true,
		&stage5AeroEnableSource);
	m_stage5ApplyAeroToRadiance = m_runtimeConfig.getBool(
		"Stage5AeroThermal",
		"ApplyAeroToRadiance",
		"ApplyAeroToRadiance",
		false,
		&stage5AeroApplySource);
	m_stage5AeroDebugLog = m_runtimeConfig.getBool(
		"Stage5AeroThermal",
		"AeroDebugLog",
		"AeroDebugLog",
		false,
		&stage5AeroDebugLogSource);
	m_stage5AeroLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage5AeroThermal",
		"AeroLogEveryFrames",
		"Stage5AeroLogEveryFrames",
		120,
		&stage5AeroLogEverySource));
	m_stage5AeroThermalOptions.enabled = m_stage5AeroThermalEnabled;
	m_stage5AeroThermalOptions.recoveryFactor = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "RecoveryFactor", "Stage5AeroRecoveryFactor", 0.85, &stage5AeroRecoverySource), 0.0, 1.0);
	m_stage5AeroThermalOptions.gamma = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "Gamma", "Stage5AeroGamma", 1.4, &stage5AeroGammaSource), 1.01, 2.0);
	m_stage5AeroThermalOptions.bodyCoeff = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "BodyCoeff", "Stage5AeroBodyCoeff", 0.20, &stage5AeroBodyCoeffSource));
	m_stage5AeroThermalOptions.noseCoeff = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "NoseCoeff", "Stage5AeroNoseCoeff", 0.60, &stage5AeroNoseCoeffSource));
	m_stage5AeroThermalOptions.edgeCoeff = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "EdgeCoeff", "Stage5AeroEdgeCoeff", 0.45, &stage5AeroEdgeCoeffSource));
	m_stage5AeroThermalOptions.rearCoeff = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "RearCoeff", "Stage5AeroRearCoeff", 0.10, &stage5AeroRearCoeffSource));
	m_stage5AeroThermalOptions.heatTauSec = std::max(0.001, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "HeatTauSec", "Stage5AeroHeatTauSec", 2.0, &stage5AeroHeatTauSource));
	m_stage5AeroThermalOptions.coolTauSec = std::max(0.001, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "CoolTauSec", "Stage5AeroCoolTauSec", 5.0, &stage5AeroCoolTauSource));
	m_stage5AeroThermalOptions.clampMachMin = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "ClampMachMin", "Stage5AeroClampMachMin", 0.0, &stage5AeroMachMinSource));
	m_stage5AeroThermalOptions.clampMachMax = std::max(
		m_stage5AeroThermalOptions.clampMachMin,
		m_runtimeConfig.getDouble("Stage5AeroThermal", "ClampMachMax", "Stage5AeroClampMachMax", 4.0, &stage5AeroMachMaxSource));
	m_stage5AeroThermalOptions.clampDeltaKMax = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "ClampDeltaKMax", "Stage5AeroClampDeltaKMax", 250.0, &stage5AeroDeltaMaxSource));
	m_stage5AeroApplyScale = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "AeroApplyScale", "Stage5AeroApplyScale", 0.25, &stage5AeroApplyScaleSource), 0.0, 1.0);
	m_stage5AeroApplyClampBodyDeltaK = std::max(0.0, m_runtimeConfig.getDouble(
		"Stage5AeroThermal", "AeroApplyClampBodyDeltaK", "Stage5AeroApplyClampBodyDeltaK", 40.0, &stage5AeroApplyClampBodyDeltaSource));
	{
		const std::string aeroBandText = m_runtimeConfig.getString(
			"Stage5AeroThermal",
			"AeroApplyOnlyBand",
			"Stage5AeroApplyOnlyBand",
			"MWIR",
			&stage5AeroApplyOnlyBandSource);
		m_stage5AeroApplyOnlyBand = ParseStage5ModtranPathRuntimeBand(aeroBandText, m_stage5AeroApplyOnlyBandName);
	}
	m_enableStage5ModtranRadianceDebug = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"EnableModtranRadianceDebug",
		"EnableStage5ModtranRadianceDebug",
		false,
		&stage5ModtranDebugSource);
	m_stage5UseModtranPathRuntime = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"UseModtranPathRuntime",
		"UseModtranPathRuntime",
		false,
		&stage5ModtranPathRuntimeSource);
	m_stage5UseModtranSkyRuntime = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"UseModtranSkyRuntime",
		"UseModtranSkyRuntime",
		false,
		&stage5ModtranSkyRuntimeSource);
	m_stage5UseModtranSolarRuntime = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"UseModtranSolarRuntime",
		"UseModtranSolarRuntime",
		false,
		&stage5ModtranSolarRuntimeSource);
	m_stage5ModtranPreferredSource = ToLowerAscii(m_runtimeConfig.getString(
		"Stage5ModtranRadiance",
		"PreferredSource",
		"Stage5ModtranRadiancePreferredSource",
		"band_lut",
		&stage5ModtranPreferredSourceSource));
	if (m_stage5ModtranPreferredSource != "band_lut")
	{
		std::cout << "[Stage5 ModtranRadianceConfig][WARN]"
			<< " PreferredSource=" << m_stage5ModtranPreferredSource
			<< " fallback=band_lut"
			<< std::endl;
		m_stage5ModtranPreferredSource = "band_lut";
	}
	m_stage5ModtranLogEveryFrames = std::max(1, m_runtimeConfig.getInt(
		"Stage5ModtranRadiance",
		"LogEveryFrames",
		"Stage5ModtranRadianceLogEveryFrames",
		120,
		&stage5ModtranLogEverySource));
	m_stage5ModtranCompareLegacy = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"CompareLegacy",
		"Stage5ModtranRadianceCompareLegacy",
		false,
		&stage5ModtranCompareLegacySource);
	{
		const std::string bandText = m_runtimeConfig.getString(
			"Stage5ModtranRadiance",
			"ModtranPathRuntimeBand",
			"ModtranPathRuntimeBand",
			"MWIR",
			&stage5ModtranPathRuntimeBandSource);
		m_stage5ModtranPathRuntimeBand = ParseStage5ModtranPathRuntimeBand(bandText, m_stage5ModtranPathRuntimeBandName);
	}
	m_stage5ModtranPathRuntimeMode = NormalizeStage5ModtranPathRuntimeMode(m_runtimeConfig.getString(
		"Stage5ModtranRadiance",
		"ModtranPathRuntimeMode",
		"ModtranPathRuntimeMode",
		"Off",
		&stage5ModtranPathRuntimeModeSource));
	m_stage5ModtranPathUnitMode = m_runtimeConfig.getString(
		"Stage5ModtranRadiance",
		"ModtranPathUnitMode",
		"ModtranPathUnitMode",
		"Native",
		&stage5ModtranPathUnitModeSource);
	m_stage5ModtranPathScale = m_runtimeConfig.getDouble(
		"Stage5ModtranRadiance",
		"ModtranPathScale",
		"ModtranPathScale",
		1.0,
		&stage5ModtranPathScaleSource);
	m_stage5ModtranPathOffset = m_runtimeConfig.getDouble(
		"Stage5ModtranRadiance",
		"ModtranPathOffset",
		"ModtranPathOffset",
		0.0,
		&stage5ModtranPathOffsetSource);
	m_stage5ModtranPathClampMin = m_runtimeConfig.getDouble(
		"Stage5ModtranRadiance",
		"ModtranPathClampMin",
		"ModtranPathClampMin",
		0.0,
		&stage5ModtranPathClampMinSource);
	m_stage5ModtranPathClampMax = m_runtimeConfig.getDouble(
		"Stage5ModtranRadiance",
		"ModtranPathClampMax",
		"ModtranPathClampMax",
		10.0,
		&stage5ModtranPathClampMaxSource);
	if (m_stage5ModtranPathClampMax < m_stage5ModtranPathClampMin)
	{
		std::swap(m_stage5ModtranPathClampMin, m_stage5ModtranPathClampMax);
	}
	m_stage5ModtranPathBlend = ClampStage5Double(m_runtimeConfig.getDouble(
		"Stage5ModtranRadiance",
		"ModtranPathBlend",
		"ModtranPathBlend",
		1.0,
		&stage5ModtranPathBlendSource), 0.0, 1.0);
	m_stage5ModtranPathABLog = m_runtimeConfig.getBool(
		"Stage5ModtranRadiance",
		"ModtranPathABLog",
		"ModtranPathABLog",
		true,
		&stage5ModtranPathABLogSource);
	if (m_stage5ModtranPathRuntimeBand != IRBand::MidWaveInfrared &&
		(m_stage5ModtranPathRuntimeMode == "ReplaceLegacy" || m_stage5ModtranPathRuntimeMode == "BlendLegacy"))
	{
		std::cout << "[Stage5 ModtranPathRuntime][WARN]"
			<< " ModtranPathRuntimeBand=" << m_stage5ModtranPathRuntimeBandName
			<< " runtimeMode=" << m_stage5ModtranPathRuntimeMode
			<< " fallback=legacy"
			<< " reason=only_MWIR_path_runtime_allowed_in_stage3C"
			<< std::endl;
	}
	m_stage5ModtranRadiancePath = modtranBandLutPath;
	m_stage5ModtranRadianceReady = !m_stage5ModtranRadiancePath.empty() &&
		m_stage5ModtranRadianceLut.load(m_stage5ModtranRadiancePath);
	m_stage5ModtranRadianceCache.clear();
	m_lastStage5ModtranPathABLogState.clear();
	m_stage5ModtranPathRuntimeBandWarned.clear();
	if (!m_stage5ModtranRadianceReady)
	{
		std::cout << "[Stage5 ModtranRadianceConfig][WARN]"
			<< " source=missing"
			<< " preferredSource=" << m_stage5ModtranPreferredSource
			<< " path=" << (m_stage5ModtranRadiancePath.empty() ? "NA" : m_stage5ModtranRadiancePath)
			<< " reason=band_lut_missing_or_missing_path_sky_solar_columns"
			<< std::endl;
	}
	if (m_stage5UseModtranSkyRuntime || m_stage5UseModtranSolarRuntime)
	{
		std::cout << "[Stage5 ModtranRadianceConfig][WARN]"
			<< " UseModtranSkyRuntime=" << (m_stage5UseModtranSkyRuntime ? "1" : "0")
			<< " UseModtranSolarRuntime=" << (m_stage5UseModtranSolarRuntime ? "1" : "0")
			<< " effective=log_only"
			<< " reason=Stage3B_does_not_apply_sky_or_solar_to_final_image"
			<< std::endl;
	}
	m_stage5DebugConfig = m_stage5DebugConfigs[Stage5BandIndex(IRBand::MidWaveInfrared)];
	m_stage5DebugToneMapName = Stage5ToneMapName(m_stage5DebugConfig.toneMap);
	m_stage5UseBaseTextureModulation = m_stage5UseBaseTextureModulationByBand[Stage5BandIndex(IRBand::MidWaveInfrared)];
	m_stage5OutputFrameDumpEnabled = m_runtimeConfig.getBool("Stage5", "OutputFrameDump", "Stage5OutputFrameDump", false, &stage5DumpSource);
	m_stage5OutputFrameDumpPath = m_runtimeConfig.getString("Stage5", "OutputFrameDumpPath", "Stage5OutputFrameDumpPath", "", &stage5DumpPathSource);
	m_stage5OutputFrameDumpEvery = std::max(1, m_runtimeConfig.getInt("Stage5", "OutputFrameDumpEvery", "Stage5OutputFrameDumpEvery", 5, &stage5DumpEverySource));
	m_stage5OutputFrameCounter = 0;
	m_stage5OutputFrameDumpWrites = 0;
	m_stage5OutputFrameDumpFailureLogged = false;
	std::string stage7EnableSource;
	std::string stage7DebugSource;
	std::string stage7GroundSource;
	std::string stage7Real3DSource;
	std::string stage7UpdateHzSource;
	std::string stage7WeatherEnableSource;
	std::string stage7WeatherProfileSource;
	std::string stage7WeatherTextureSource;
	std::string stage7CloudSource;
	std::string stage7FogSource;
	std::string stage7PrecipSource;
	std::string stage7PrecipModeSource;
	std::string stage7CloudCardsSource;
	std::string stage7PrecipParticlesSource;
	std::string stage7UdpSource;
	std::string plumeEnableSource;
	std::string plumePathSource;
	std::string plumeMaxSource;
	std::string plumeUseEngineSource;
	std::string plumeNoiseSource;
	std::string plumeDebugSource;
	std::string plumeForceSource;
	std::string plumeGainSource;
	std::string plumeCoreGainSource;
	std::string plumeHaloGainSource;
	std::string plumeOpacitySource;
	std::string plumeCoreOpacitySource;
	std::string plumeHaloOpacitySource;
	std::string plumeUpdateHzSource;
	std::string plumePerfBudgetSource;
	m_enableStage7SkyHorizon = m_runtimeConfig.getBool("Stage7Background", "EnableSkyHorizon", "EnableStage7SkyHorizon", true, &stage7EnableSource);
	m_stage7DebugMode = ParseStage7DebugMode(m_runtimeConfig.getString("Stage7Background", "DebugMode", "Stage7DebugMode", "Off", &stage7DebugSource));
	m_stage7DebugModeName = Stage7DebugModeName(m_stage7DebugMode);
	m_stage7GroundZOffset = m_runtimeConfig.getDouble("Stage7Background", "GroundZOffset", "Stage7GroundZOffset", 0.0, &stage7GroundSource);
	m_stage7UseReal3DBackground = m_runtimeConfig.getBool("Stage7Background", "UseReal3DBackground", "UseReal3DBackground", true, &stage7Real3DSource);
	m_stage7UpdateHz = m_runtimeConfig.getDouble("Stage7Background", "Stage7UpdateHz", "Stage7UpdateHz", 10.0, &stage7UpdateHzSource);
	if (!std::isfinite(m_stage7UpdateHz) || m_stage7UpdateHz <= 0.0)
	{
		m_stage7UpdateHz = 10.0;
	}
	m_stage7UpdateHz = std::max(1.0, std::min(240.0, m_stage7UpdateHz));
	m_stage7WeatherEnabled = m_runtimeConfig.getBool("Stage7Weather", "EnableWeatherEffects", "EnableStage7WeatherEffects", true, &stage7WeatherEnableSource);
	m_stage7WeatherProfilePath = m_runtimeConfig.getString("Stage7Weather", "WeatherProfilePath", "Stage7WeatherProfilePath", "Config/Weather/weather_profiles.json", &stage7WeatherProfileSource);
	m_stage7WeatherTextureConfigPath = m_runtimeConfig.getString("Stage7Weather", "WeatherTextureConfig", "Stage7WeatherTextureConfig", "Config/Weather/weather_textures.json", &stage7WeatherTextureSource);
	m_stage7CloudLayerEnabled = m_runtimeConfig.getBool("Stage7Weather", "EnableCloudLayer", "Stage7EnableCloudLayer", false, &stage7CloudSource);
	m_stage7FogEnabled = m_runtimeConfig.getBool("Stage7Weather", "EnableFog", "Stage7EnableFog", true, &stage7FogSource);
	m_stage7PrecipitationEnabled = m_runtimeConfig.getBool("Stage7Weather", "EnablePrecipitation", "Stage7EnablePrecipitation", false, &stage7PrecipSource);
	m_stage7PrecipitationMode = ParseStage7PrecipitationMode(m_runtimeConfig.getString("Stage7Weather", "Stage7PrecipitationMode", "Stage7PrecipitationMode", "ScreenOverlay", &stage7PrecipModeSource));
	m_stage7PrecipitationModeName = Stage7PrecipitationModeName(m_stage7PrecipitationMode);
	m_stage7CloudLayerMaxCards = std::max(0, std::min(64, m_runtimeConfig.getInt("Stage7Weather", "CloudLayerMaxCards", "Stage7CloudLayerMaxCards", 0, &stage7CloudCardsSource)));
	m_stage7PrecipitationMaxParticles = std::max(0, std::min(512, m_runtimeConfig.getInt("Stage7Weather", "PrecipitationMaxParticles", "Stage7PrecipitationMaxParticles", 0, &stage7PrecipParticlesSource)));
	m_stage7UseWeatherUdpInput = m_runtimeConfig.getBool("Stage7Weather", "UseWeatherUdpInput", "Stage7UseWeatherUdpInput", true, &stage7UdpSource);
	m_stage5PlumeOptions.enableEnginePlume = m_runtimeConfig.getBool("Stage5Plume", "EnableEnginePlume", "EnableEnginePlume", true, &plumeEnableSource);
	m_stage5PlumeProfilePath = m_runtimeConfig.getString("Stage5Plume", "EnginePlumeProfilePath", "EnginePlumeProfilePath", "Config/IRPlume/engine_plume_profiles.json", &plumePathSource);
	m_stage5PlumeOptions.maxPlumeNodes = std::max(0, std::min(128, m_runtimeConfig.getInt("Stage5Plume", "MaxPlumeNodes", "MaxPlumeNodes", 16, &plumeMaxSource)));
	m_stage5PlumeOptions.useEngineState = m_runtimeConfig.getBool("Stage5Plume", "UseEngineState", "UseEngineState", true, &plumeUseEngineSource);
	m_stage5PlumeOptions.useProceduralNoise = m_runtimeConfig.getBool("Stage5Plume", "UseProceduralNoise", "UseProceduralNoise", true, &plumeNoiseSource);
	m_stage5PlumeOptions.enablePlumeDebug = m_runtimeConfig.getBool("Stage5Plume", "EnablePlumeDebug", "EnablePlumeDebug", false, &plumeDebugSource);
	m_stage5PlumeOptions.forcePlumeVisible = m_runtimeConfig.getBool("Stage5Plume", "ForcePlumeVisible", "ForcePlumeVisible", false, &plumeForceSource);
	m_stage5PlumeOptions.displayGain = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeDisplayGain", "PlumeDisplayGain", 1.0, &plumeGainSource));
	m_stage5PlumeOptions.coreDisplayGain = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeCoreDisplayGain", "PlumeCoreDisplayGain", 1.2, &plumeCoreGainSource));
	m_stage5PlumeOptions.haloDisplayGain = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeHaloDisplayGain", "PlumeHaloDisplayGain", 0.8, &plumeHaloGainSource));
	m_stage5PlumeOptions.opacityScale = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeOpacityScale", "PlumeOpacityScale", 1.0, &plumeOpacitySource));
	m_stage5PlumeOptions.coreOpacityScale = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeCoreOpacityScale", "PlumeCoreOpacityScale", 1.0, &plumeCoreOpacitySource));
	m_stage5PlumeOptions.haloOpacityScale = static_cast<float>(m_runtimeConfig.getDouble("Stage5Plume", "PlumeHaloOpacityScale", "PlumeHaloOpacityScale", 1.0, &plumeHaloOpacitySource));
	m_stage5PlumeUpdateHz = m_runtimeConfig.getDouble("Stage5Plume", "UpdateHz", "Stage5PlumeUpdateHz", 30.0, &plumeUpdateHzSource);
	m_stage5PlumePerfBudgetMs = m_runtimeConfig.getDouble("Stage5Plume", "PerfBudgetMs", "Stage5PlumePerfBudgetMs", 1.0, &plumePerfBudgetSource);
	if (!std::isfinite(m_stage5PlumeUpdateHz) || m_stage5PlumeUpdateHz <= 0.0)
	{
		m_stage5PlumeUpdateHz = 30.0;
	}
	m_stage5PlumeUpdateHz = std::max(1.0, std::min(240.0, m_stage5PlumeUpdateHz));
	if (!std::isfinite(m_stage5PlumePerfBudgetMs) || m_stage5PlumePerfBudgetMs <= 0.0)
	{
		m_stage5PlumePerfBudgetMs = 1.0;
	}
	if (!std::isfinite(m_stage5PlumeOptions.displayGain) || m_stage5PlumeOptions.displayGain < 0.0f)
	{
		m_stage5PlumeOptions.displayGain = 1.0f;
	}
	if (!std::isfinite(m_stage5PlumeOptions.coreDisplayGain) || m_stage5PlumeOptions.coreDisplayGain < 0.0f)
	{
		m_stage5PlumeOptions.coreDisplayGain = 1.2f;
	}
	if (!std::isfinite(m_stage5PlumeOptions.haloDisplayGain) || m_stage5PlumeOptions.haloDisplayGain < 0.0f)
	{
		m_stage5PlumeOptions.haloDisplayGain = 0.8f;
	}
	if (!std::isfinite(m_stage5PlumeOptions.opacityScale) || m_stage5PlumeOptions.opacityScale < 0.0f)
	{
		m_stage5PlumeOptions.opacityScale = 1.0f;
	}
	if (!std::isfinite(m_stage5PlumeOptions.coreOpacityScale) || m_stage5PlumeOptions.coreOpacityScale < 0.0f)
	{
		m_stage5PlumeOptions.coreOpacityScale = 1.0f;
	}
	if (!std::isfinite(m_stage5PlumeOptions.haloOpacityScale) || m_stage5PlumeOptions.haloOpacityScale < 0.0f)
	{
		m_stage5PlumeOptions.haloOpacityScale = 1.0f;
	}
	plumeConfigPaths = BuildRuntimeConfigPathCandidates(m_stage5PlumeProfilePath);
	if (!std::isfinite(m_stage7GroundZOffset))
	{
		m_stage7GroundZOffset = 0.0;
	}
	m_stage7NearFarClipWarningLogged = false;
	m_stage7SkyHorizonLogCounter = 0;
	m_stage7LastSkyHorizonState.clear();
	m_stage6FrameDiagLogCounter = 0;
	m_stage6NoVisibleTargetFrames = 0;
	m_stage6LastFrameDiagState.clear();
	m_irAtmosphereModel.setModtranTauDebugEnabled(enableModtranTauDebug);
	m_irAtmosphereModel.setUseModtranTauForAtmosphere(useModtranTauForAtmosphere);
	m_irSensorProfilesReady = m_irSensorProfiles.loadFromDirectoryCandidates(sensorWaveDirs);
	m_irWeatherReady = m_irWeatherProfile.load(weatherPath);
	const bool stage7WeatherProfilesReady = m_stage7WeatherEffects.loadProfilesFromCandidates(BuildRuntimeConfigPathCandidates(m_stage7WeatherProfilePath));
	const bool stage7WeatherTexturesReady = m_stage7WeatherEffects.loadTextureConfigFromCandidates(BuildRuntimeConfigPathCandidates(m_stage7WeatherTextureConfigPath));
	m_irTemperatureReady = m_irTemperatureModel.loadFromFileCandidates(hotspotConfigPaths);
	m_irEnginePlumeReady = m_irEnginePlumeModel.loadFromFileCandidates(plumeConfigPaths);
	m_irRadianceModel.setMaterialDatabase(&m_irMaterialDatabase);
	m_irRadianceModel.setAtmosphereModel(&m_irAtmosphereModel);

	IRRuntimeEnvironment environment;
	environment.band = IRBandFromProtocol(2);
	if (m_irWeatherReady)
	{
		// 阶段3默认使用正午 profile，真实运行时由实时数据时间换算仿真小时。
		IRWeatherSample sample = m_irWeatherProfile.sampleForHour(12.0);
		environment.airTemperatureC = sample.airTemperatureC;
		environment.sunAzimuthDeg = sample.sunAzimuthDeg;
		environment.sunElevationDeg = sample.sunElevationDeg;
		environment.simulationHour = sample.hour;
	}
	else
	{
		environment.airTemperatureC = 25.0;
	}
	environment.visibilityMeters = 23000.0;
	environment.sunStrength = WeatherSunStrength(environment.weatherCode, environment.sunElevationDeg);
	m_irRadianceModel.setEnvironment(environment);

	std::cout << "[RuntimeConfig]"
		<< " path=" << m_runtimeConfig.loadedPath()
		<< " loaded=" << (m_runtimeConfig.loaded() ? "1" : "0")
		<< " envOverrideCount=" << m_runtimeConfig.envOverrideCount()
		<< " iniValueCount=" << m_runtimeConfig.iniValueCount()
		<< " sourcePriority=" << m_runtimeConfig.sourcePriority()
		<< std::endl;
	if (!m_runtimeConfig.loaded())
	{
		std::cout << "[RuntimeConfig][WARN] HwaSimIRRuntime.ini not found; using safe runtime defaults."
			<< " path=" << m_runtimeConfig.loadedPath()
			<< std::endl;
	}

	std::cout << "红外全链路CPU模型初始化：材质库="
		<< (m_irMaterialReady ? "OK" : "未加载，使用默认材质")
		<< " 路径=" << materialPath
		<< "，MODTRAN透过率="
		<< (m_irAtmosphereReady ? "OK" : "未加载，使用经验透过率")
		<< " 路径=" << transmittancePath << std::endl;
	std::cout << "[Stage3] MODTRAN tau-only debug LUT="
		<< (modtranTauLutReady ? "OK" : "未加载，仅使用旧透过率表")
		<< " 路径=" << modtranBandLutPath
		<< " EnableModtranTauDebug=" << (enableModtranTauDebug ? "1" : "0")
		<< " UseModtranTauForAtmosphere=" << (useModtranTauForAtmosphere ? "1" : "0")
		<< " source=" << stage3TauDebugSource << "/" << stage3UseTauSource
		<< "（默认仅日志对比；active=1 才返回 MODTRAN tau）" << std::endl;
	std::cout << "[Stage1] IR配置输入：SensorWave="
		<< (m_irSensorProfilesReady ? "OK" : "未加载，使用内置传感器默认值")
		<< " 路径=" << (m_irSensorProfilesReady ? m_irSensorProfiles.loadedDirectory() : "fallback")
		<< "，MaterialDatabase=" << (m_irMaterialReady ? materialPath : "fallback")
		<< "，Transmittance=" << (m_irAtmosphereReady ? transmittancePath : "fallback")
		<< "，WeatherProfile=" << (m_irWeatherReady ? weatherPath : "fallback") << std::endl;
	std::cout << "[Stage4] IRHotspots配置："
		<< (m_irTemperatureReady ? "OK" : "未加载，使用内置安全默认值")
		<< " 路径=" << (m_irTemperatureReady ? m_irTemperatureModel.loadedPath() : "fallback")
		<< "（ThermalHotspot与BrightSpot分离；本阶段只处理发动机热源与特殊亮斑）" << std::endl;
	std::cout << "[Stage4] Visual debug flags:"
		<< " EnableStage4HotspotVisualDebug=" << (m_enableStage4HotspotVisualDebug ? "1" : "0")
		<< " ForceStage4BrightSpotVisible=" << (m_forceStage4BrightSpotVisible ? "1" : "0")
		<< " ForceStage4RearHotspotVisible=" << (m_forceStage4RearHotspotVisible ? "1" : "0")
		<< " LegacyEngineBodyHeating=" << (m_stage4LegacyEngineBodyHeating ? "1" : "0")
		<< " Stage4UpdateHz=" << m_stage4UpdateHz
		<< " source=" << stage4VisualSource << "/" << stage4BrightSource << "/" << stage4RearSource
		<< "/" << stage4LegacyEngineBodyHeatingSource << "/" << stage4UpdateHzSource
		<< "（默认全为0；仅用于可见性/legacy诊断）" << std::endl;
	std::cout << "[Stage5 Radiance] Stage5DebugDisplayConfig="
		<< (m_stage5DebugDisplayConfigReady ? "OK" : "fallback")
		<< " path=" << m_stage5DebugDisplayConfigPath
		<< "（只配置DebugView显示映射，不决定Stage5正式分量链路是否计算）" << std::endl;
	std::cout << "[Stage5 RadianceConfig]"
		<< " EnableIRPhysicalPipeline=" << (m_enableStage5PhysicalPipeline ? "1" : "0")
		<< " DebugView=" << m_stage5DebugViewModeName
		<< " EnableStage5RadianceDebug=" << (m_enableStage5RadianceDebug ? "1" : "0")
		<< " LogComponents=" << (m_stage5LogComponents ? "1" : "0")
		<< " ComponentLogEveryFrames=" << m_stage5ComponentLogEveryFrames
		<< " UseSensorInputForDisplay=" << (m_stage5UseSensorInputForDisplay ? "1" : "0")
		<< " SensorInputDisplayMode=" << m_stage5SensorInputDisplayMode
		<< " SensorInputDisplayScale=" << m_stage5SensorInputDisplayScale
		<< " SensorInputDisplayOffset=" << m_stage5SensorInputDisplayOffset
		<< " SensorInputDisplayClampMin=" << m_stage5SensorInputDisplayClampMin
		<< " SensorInputDisplayClampMax=" << m_stage5SensorInputDisplayClampMax
		<< " SensorInputDisplayGamma=" << m_stage5SensorInputDisplayGamma
		<< " SensorInputDisplayBand=" << m_stage5SensorInputDisplayBandName
		<< " pathRadianceSource=legacy_empirical"
		<< " Stage5DebugToneMap=" << m_stage5DebugToneMapName
		<< " Stage5BodyRadianceScale=" << m_stage5DebugConfig.bodyRadianceScale
		<< " Stage5HotspotRadianceScale=" << m_stage5DebugConfig.hotspotRadianceScale
		<< " Stage5BrightspotRadianceScale=" << m_stage5DebugConfig.brightspotRadianceScale
		<< " Stage5SolarReflectanceWeight=" << m_stage5DebugConfig.solarReflectanceWeight
		<< " Stage5DebugMinBodyGray=" << m_stage5DebugConfig.minBodyGray
		<< " Stage5UseBaseTextureModulation=" << (m_stage5UseBaseTextureModulation ? "1" : "0")
		<< " Stage5BodyDisplayGain=" << m_stage5DebugConfig.bodyDisplayGain
		<< " Stage5ReflectedDisplayGain=" << m_stage5DebugConfig.reflectedDisplayGain
		<< " Stage5HotspotDisplayGain=" << m_stage5DebugConfig.hotspotDisplayGain
		<< " Stage5BrightspotDisplayGain=" << m_stage5DebugConfig.brightspotDisplayGain
		<< " Stage5CompositeMinGray=" << m_stage5DebugConfig.compositeMinGray
		<< " Stage5CompositeMaxGray=" << m_stage5DebugConfig.compositeMaxGray
		<< " source=" << stage5PhysicalSource << "/" << stage5ViewModeSource << "/"
		<< stage5DebugSource << "/" << stage5ComponentLogSource << "/" << stage5ComponentLogEverySource
		<< "/" << stage5SensorInputDisplaySource << "/" << stage5SensorInputDisplayModeSource
		<< "/" << stage5SensorInputDisplayScaleSource
		<< "/" << stage5SensorInputDisplayOffsetSource << "/" << stage5SensorInputDisplayClampMinSource
		<< "/" << stage5SensorInputDisplayClampMaxSource << "/" << stage5SensorInputDisplayGammaSource
		<< "/" << stage5SensorInputDisplayBandSource
		<< "（DebugView Off 时主画面保持legacy输出；path/sky/solar MODTRAN runtime disabled）"
		<< std::endl;
	std::cout << "[Stage5 AeroThermalConfig]"
		<< " EnableAeroThermalModel=" << (m_stage5AeroThermalEnabled ? "1" : "0")
		<< " ApplyAeroToRadiance=" << (m_stage5ApplyAeroToRadiance ? "1" : "0")
		<< " AeroDebugLog=" << (m_stage5AeroDebugLog ? "1" : "0")
		<< " AeroLogEveryFrames=" << m_stage5AeroLogEveryFrames
		<< " AeroApplyScale=" << m_stage5AeroApplyScale
		<< " AeroApplyClampBodyDeltaK=" << m_stage5AeroApplyClampBodyDeltaK
		<< " AeroApplyOnlyBand=" << m_stage5AeroApplyOnlyBandName
		<< " RecoveryFactor=" << m_stage5AeroThermalOptions.recoveryFactor
		<< " Gamma=" << m_stage5AeroThermalOptions.gamma
		<< " BodyCoeff=" << m_stage5AeroThermalOptions.bodyCoeff
		<< " NoseCoeff=" << m_stage5AeroThermalOptions.noseCoeff
		<< " EdgeCoeff=" << m_stage5AeroThermalOptions.edgeCoeff
		<< " RearCoeff=" << m_stage5AeroThermalOptions.rearCoeff
		<< " HeatTauSec=" << m_stage5AeroThermalOptions.heatTauSec
		<< " CoolTauSec=" << m_stage5AeroThermalOptions.coolTauSec
		<< " ClampMachMin=" << m_stage5AeroThermalOptions.clampMachMin
		<< " ClampMachMax=" << m_stage5AeroThermalOptions.clampMachMax
		<< " ClampDeltaKMax=" << m_stage5AeroThermalOptions.clampDeltaKMax
		<< " speedUnit=km/h"
		<< " source=" << stage5AeroEnableSource << "/" << stage5AeroApplySource
		<< "/" << stage5AeroDebugLogSource << "/" << stage5AeroLogEverySource
		<< "/" << stage5AeroRecoverySource << "/" << stage5AeroGammaSource
		<< "/" << stage5AeroBodyCoeffSource << "/" << stage5AeroNoseCoeffSource
		<< "/" << stage5AeroEdgeCoeffSource << "/" << stage5AeroRearCoeffSource
		<< "/" << stage5AeroHeatTauSource << "/" << stage5AeroCoolTauSource
		<< "/" << stage5AeroMachMinSource << "/" << stage5AeroMachMaxSource
		<< "/" << stage5AeroDeltaMaxSource
		<< "/" << stage5AeroApplyScaleSource << "/" << stage5AeroApplyClampBodyDeltaSource
		<< "/" << stage5AeroApplyOnlyBandSource
		<< "（Stage4B: default computes components only; ApplyAeroToRadiance=false keeps production image unchanged）"
		<< std::endl;
	std::cout << "[Stage5 ModtranRadianceConfig]"
		<< " EnableModtranRadianceDebug=" << (m_enableStage5ModtranRadianceDebug ? "1" : "0")
		<< " UseModtranPathRuntime=" << (m_stage5UseModtranPathRuntime ? "1" : "0")
		<< " UseModtranSkyRuntime=" << (m_stage5UseModtranSkyRuntime ? "1" : "0")
		<< " UseModtranSolarRuntime=" << (m_stage5UseModtranSolarRuntime ? "1" : "0")
		<< " PreferredSource=" << m_stage5ModtranPreferredSource
		<< " LogEveryFrames=" << m_stage5ModtranLogEveryFrames
		<< " CompareLegacy=" << (m_stage5ModtranCompareLegacy ? "1" : "0")
		<< " ModtranPathRuntimeBand=" << m_stage5ModtranPathRuntimeBandName
		<< " ModtranPathRuntimeMode=" << m_stage5ModtranPathRuntimeMode
		<< " ModtranPathUnitMode=" << m_stage5ModtranPathUnitMode
		<< " ModtranPathScale=" << m_stage5ModtranPathScale
		<< " ModtranPathOffset=" << m_stage5ModtranPathOffset
		<< " ModtranPathClampMin=" << m_stage5ModtranPathClampMin
		<< " ModtranPathClampMax=" << m_stage5ModtranPathClampMax
		<< " ModtranPathBlend=" << m_stage5ModtranPathBlend
		<< " ModtranPathABLog=" << (m_stage5ModtranPathABLog ? "1" : "0")
		<< " loaded=" << (m_stage5ModtranRadianceReady ? "1" : "0")
		<< " sourceFile=" << (m_stage5ModtranRadianceReady ? m_stage5ModtranRadianceLut.loadedPath() : "missing")
		<< " defaultPathRadianceSource=legacy_empirical"
		<< " units=MODOUT2_native"
		<< " Stage5ModtranCompareEffective=" << (Stage5ModtranRadianceCompareEnabled() ? "1" : "0")
		<< " source=" << stage5ModtranDebugSource << "/" << stage5ModtranPathRuntimeSource
		<< "/" << stage5ModtranSkyRuntimeSource << "/" << stage5ModtranSolarRuntimeSource
		<< "/" << stage5ModtranPreferredSourceSource << "/" << stage5ModtranLogEverySource
		<< "/" << stage5ModtranCompareLegacySource
		<< "/" << stage5ModtranPathRuntimeBandSource << "/" << stage5ModtranPathRuntimeModeSource
		<< "/" << stage5ModtranPathScaleSource << "/" << stage5ModtranPathBlendSource
		<< "（Stage3C: production default Off; MWIR path runtime A/B must opt in）"
		<< std::endl;
	std::cout << "[Stage5 OutputCapture] Stage5OutputFrameDump="
		<< ((m_stage5OutputFrameDumpEnabled && m_enableStage5RadianceDebug) ? "1" : "0")
		<< " path=" << (m_stage5OutputFrameDumpPath.empty() ? "NA" : m_stage5OutputFrameDumpPath)
		<< " every=" << m_stage5OutputFrameDumpEvery
		<< "（smoke-only render texture dump; TCP/JPEG protocol unchanged）" << std::endl;
	std::cout << "[Stage5 PlumeConfig]"
		<< " EnableEnginePlume=" << (m_stage5PlumeOptions.enableEnginePlume ? "1" : "0")
		<< " profile=" << (m_irEnginePlumeReady ? "OK" : "fallback")
		<< " path=" << (m_irEnginePlumeReady ? m_irEnginePlumeModel.loadedPath() : m_stage5PlumeProfilePath)
		<< " MaxPlumeNodes=" << m_stage5PlumeOptions.maxPlumeNodes
		<< " UseEngineState=" << (m_stage5PlumeOptions.useEngineState ? "1" : "0")
		<< " UseProceduralNoise=" << (m_stage5PlumeOptions.useProceduralNoise ? "1" : "0")
		<< " EnablePlumeDebug=" << (m_stage5PlumeOptions.enablePlumeDebug ? "1" : "0")
		<< " ForcePlumeVisible=" << (m_stage5PlumeOptions.forcePlumeVisible ? "1" : "0")
		<< " PlumeDisplayGain=" << m_stage5PlumeOptions.displayGain
		<< " PlumeCoreDisplayGain=" << m_stage5PlumeOptions.coreDisplayGain
		<< " PlumeHaloDisplayGain=" << m_stage5PlumeOptions.haloDisplayGain
		<< " PlumeOpacityScale=" << m_stage5PlumeOptions.opacityScale
		<< " PlumeCoreOpacityScale=" << m_stage5PlumeOptions.coreOpacityScale
		<< " PlumeHaloOpacityScale=" << m_stage5PlumeOptions.haloOpacityScale
		<< " UpdateHz=" << m_stage5PlumeUpdateHz
		<< " PerfBudgetMs=" << m_stage5PlumePerfBudgetMs
		<< " source=" << plumeEnableSource << "/" << plumePathSource << "/" << plumeMaxSource
		<< "/" << plumeUseEngineSource << "/" << plumeNoiseSource << "/" << plumeDebugSource
		<< "/" << plumeForceSource << "/" << plumeGainSource << "/" << plumeCoreGainSource << "/" << plumeHaloGainSource
		<< "/" << plumeOpacitySource << "/" << plumeCoreOpacitySource << "/" << plumeHaloOpacitySource
		<< "/" << plumeUpdateHzSource << "/" << plumePerfBudgetSource
		<< "（EnginePlume 独立于 rear ThermalHotspot 与 BrightSpot；path/sky/solar disabled）"
		<< std::endl;
	std::cout << "[Stage7 Config]"
		<< " enableSkyHorizon=" << (m_enableStage7SkyHorizon ? "1" : "0")
		<< " debugMode=" << m_stage7DebugModeName
		<< " groundZOffset=" << m_stage7GroundZOffset
		<< " useReal3DBackground=" << (m_stage7UseReal3DBackground ? "1" : "0")
		<< " Stage7UpdateHz=" << m_stage7UpdateHz
		<< " source=" << stage7EnableSource << "/" << stage7DebugSource << "/" << stage7GroundSource << "/" << stage7Real3DSource
		<< "/" << stage7UpdateHzSource
		<< std::endl;
	std::cout << "[Stage7 WeatherConfig]"
		<< " enableWeatherEffects=" << (m_stage7WeatherEnabled ? "1" : "0")
		<< " weatherProfile=" << (stage7WeatherProfilesReady ? "OK" : "fallback")
		<< " profilePath=" << m_stage7WeatherEffects.profilePath()
		<< " weatherTextures=" << (stage7WeatherTexturesReady ? "OK" : "fallback")
		<< " textureConfigPath=" << m_stage7WeatherEffects.textureConfigPath()
		<< " enableCloudLayer=" << (m_stage7CloudLayerEnabled ? "1" : "0")
		<< " enableFog=" << (m_stage7FogEnabled ? "1" : "0")
		<< " enablePrecipitation=" << (m_stage7PrecipitationEnabled ? "1" : "0")
		<< " precipitationMode=" << m_stage7PrecipitationModeName
		<< " cloudLayerMaxCards=" << m_stage7CloudLayerMaxCards
		<< " precipitationMaxParticles=" << m_stage7PrecipitationMaxParticles
		<< " useWeatherUdpInput=" << (m_stage7UseWeatherUdpInput ? "1" : "0")
		<< " source=" << stage7WeatherEnableSource << "/" << stage7WeatherProfileSource << "/" << stage7WeatherTextureSource
		<< "/" << stage7CloudSource << "/" << stage7FogSource << "/" << stage7PrecipSource
		<< "/" << stage7PrecipModeSource << "/" << stage7CloudCardsSource << "/" << stage7PrecipParticlesSource << "/" << stage7UdpSource
		<< std::endl;
	{
		std::ostringstream sourceSummary;
		sourceSummary
			<< "EnablePerfLog:" << perfLogSource
			<< ",EnableIRVerboseLog:" << verboseLogSource
			<< ",IRUpdateHz:" << irUpdateHzSource
			<< ",EnableMTFBlur:" << stage6MtfEnableSource
			<< ",MTFBlurMode:" << stage6MtfModeSource
			<< ",MTFBlurSigmaPixels:" << stage6MtfSigmaSource
			<< ",MTFBlurRadiusPixels:" << stage6MtfRadiusSource
			<< ",MTFBlurPasses:" << stage6MtfPassesSource
			<< ",MTFApplyTo:" << stage6MtfApplyToSource
			<< ",MTFDebugLog:" << stage6MtfDebugSource
			<< ",MTFLogEveryFrames:" << stage6MtfLogEverySource
			<< ",EnableDetectorNoise:" << stage6NoiseEnableSource
			<< ",NoiseApplyTo:" << stage6NoiseApplyToSource
			<< ",NoisePosition:" << stage6NoisePositionSource
			<< ",EnableTemporalNoise:" << stage6TemporalEnableSource
			<< ",TemporalNoiseSigmaGray:" << stage6TemporalSigmaSource
			<< ",EnableFPN:" << stage6FpnEnableSource
			<< ",FPNSigmaGray:" << stage6FpnSigmaSource
			<< ",FPNSeed:" << stage6FpnSeedSource
			<< ",EnableColumnNoise:" << stage6ColumnEnableSource
			<< ",ColumnNoiseSigmaGray:" << stage6ColumnSigmaSource
			<< ",EnableRowNoise:" << stage6RowEnableSource
			<< ",RowNoiseSigmaGray:" << stage6RowSigmaSource
			<< ",EnableBadPixels:" << stage6BadPixelsEnableSource
			<< ",BadPixelRatio:" << stage6BadPixelRatioSource
			<< ",BadPixelHotGray:" << stage6BadPixelHotSource
			<< ",BadPixelDeadGray:" << stage6BadPixelDeadSource
			<< ",NoiseClampMin:" << stage6NoiseClampMinSource
			<< ",NoiseClampMax:" << stage6NoiseClampMaxSource
			<< ",NoiseDebugLog:" << stage6NoiseDebugSource
			<< ",NoiseLogEveryFrames:" << stage6NoiseLogEverySource
			<< ",EnableAGC:" << stage6AgcEnableSource
			<< ",AGCMode:" << stage6AgcModeSource
			<< ",AGCApplyTo:" << stage6AgcApplyToSource
			<< ",AGCStatsSource:" << stage6AgcStatsSource
			<< ",AGCUpdateHz:" << stage6AgcUpdateHzSource
			<< ",AGCLogEveryFrames:" << stage6AgcLogEverySource
			<< ",AGCLowPercentile:" << stage6AgcLowPercentileSource
			<< ",AGCHighPercentile:" << stage6AgcHighPercentileSource
			<< ",AGCMeanStdK:" << stage6AgcMeanStdKSource
			<< ",AGCMinGain:" << stage6AgcMinGainSource
			<< ",AGCMaxGain:" << stage6AgcMaxGainSource
			<< ",AGCMinOffset:" << stage6AgcMinOffsetSource
			<< ",AGCMaxOffset:" << stage6AgcMaxOffsetSource
			<< ",AGCSmoothingAlpha:" << stage6AgcSmoothingSource
			<< ",AGCTargetLowGray:" << stage6AgcTargetLowSource
			<< ",AGCTargetHighGray:" << stage6AgcTargetHighSource
			<< ",AGCStride:" << stage6AgcStrideSource
			<< ",AGCExcludeAnnotationOverlay:" << stage6AgcExcludeAnnotationSource
			<< ",AGCDebugLog:" << stage6AgcDebugSource
			<< ",AnnotationUpdateHz:" << annotationUpdateHzSource
			<< ",EnableIRPhysicalPipeline:" << stage5PhysicalSource
			<< ",DebugView:" << stage5ViewModeSource
			<< ",LogComponents:" << stage5ComponentLogSource
			<< ",UseSensorInputForDisplay:" << stage5SensorInputDisplaySource
			<< ",SensorInputDisplayMode:" << stage5SensorInputDisplayModeSource
			<< ",SensorInputDisplayScale:" << stage5SensorInputDisplayScaleSource
			<< ",SensorInputDisplayOffset:" << stage5SensorInputDisplayOffsetSource
			<< ",SensorInputDisplayClampMin:" << stage5SensorInputDisplayClampMinSource
			<< ",SensorInputDisplayClampMax:" << stage5SensorInputDisplayClampMaxSource
			<< ",SensorInputDisplayGamma:" << stage5SensorInputDisplayGammaSource
			<< ",SensorInputDisplayBand:" << stage5SensorInputDisplayBandSource
			<< ",EnableAeroThermalModel:" << stage5AeroEnableSource
			<< ",ApplyAeroToRadiance:" << stage5AeroApplySource
			<< ",AeroDebugLog:" << stage5AeroDebugLogSource
			<< ",AeroLogEveryFrames:" << stage5AeroLogEverySource
			<< ",AeroApplyScale:" << stage5AeroApplyScaleSource
			<< ",AeroApplyClampBodyDeltaK:" << stage5AeroApplyClampBodyDeltaSource
			<< ",AeroApplyOnlyBand:" << stage5AeroApplyOnlyBandSource
			<< ",RecoveryFactor:" << stage5AeroRecoverySource
			<< ",Gamma:" << stage5AeroGammaSource
			<< ",BodyCoeff:" << stage5AeroBodyCoeffSource
			<< ",NoseCoeff:" << stage5AeroNoseCoeffSource
			<< ",EdgeCoeff:" << stage5AeroEdgeCoeffSource
			<< ",RearCoeff:" << stage5AeroRearCoeffSource
			<< ",HeatTauSec:" << stage5AeroHeatTauSource
			<< ",CoolTauSec:" << stage5AeroCoolTauSource
			<< ",ClampMachMin:" << stage5AeroMachMinSource
			<< ",ClampMachMax:" << stage5AeroMachMaxSource
			<< ",ClampDeltaKMax:" << stage5AeroDeltaMaxSource
			<< ",EnableModtranRadianceDebug:" << stage5ModtranDebugSource
			<< ",UseModtranPathRuntime:" << stage5ModtranPathRuntimeSource
			<< ",UseModtranSkyRuntime:" << stage5ModtranSkyRuntimeSource
			<< ",UseModtranSolarRuntime:" << stage5ModtranSolarRuntimeSource
			<< ",CompareLegacy:" << stage5ModtranCompareLegacySource
			<< ",ModtranPathRuntimeBand:" << stage5ModtranPathRuntimeBandSource
			<< ",ModtranPathRuntimeMode:" << stage5ModtranPathRuntimeModeSource
			<< ",ModtranPathUnitMode:" << stage5ModtranPathUnitModeSource
			<< ",ModtranPathScale:" << stage5ModtranPathScaleSource
			<< ",ModtranPathOffset:" << stage5ModtranPathOffsetSource
			<< ",ModtranPathClampMin:" << stage5ModtranPathClampMinSource
			<< ",ModtranPathClampMax:" << stage5ModtranPathClampMaxSource
			<< ",ModtranPathBlend:" << stage5ModtranPathBlendSource
			<< ",ModtranPathABLog:" << stage5ModtranPathABLogSource
			<< ",Stage7UpdateHz:" << stage7UpdateHzSource
			<< ",Stage4UpdateHz:" << stage4UpdateHzSource
			<< ",Stage5PlumeUpdateHz:" << plumeUpdateHzSource
			<< ",Codec:" << tcpCodecSource
			<< ",JpegEncodeMode:" << jpegModeSource
			<< ",JpegQuality:" << jpegQualitySource
			<< ",EnableH264Experimental:" << h264ExperimentalSource
			<< ",H264FallbackToJpeg:" << h264FallbackSource
			<< ",H264Encoder:" << h264EncoderSource
			<< ",H264BitrateKbps:" << h264BitrateSource
			<< ",H264GopFrames:" << h264GopSource
			<< ",H264LowLatency:" << h264LowLatencySource
			<< ",H264ForceKeyFrameOnStart:" << h264ForceKeySource
			<< ",JpegPerfABTest:" << jpegAbTestSource;
		m_effectiveRuntimeConfigSources = sourceSummary.str();
	}
	LogEffectiveRuntimeConfig("startup", 0, 0, false, "not_initialized", "not_initialized", "not_initialized");
	LogActiveIRSensorProfile(2, "startup-default", true);
	LogActiveIREnvironment(environment, "startup-default", true);
}

void HwaSimIR::InitSkyAndCloudScene()
{
	if (!m_pMainWindow || m_cameraNode.is_empty())
	{
		if (IsHeadlessOffscreenMode())
		{
			std::cout << "[RenderBackend][TODO]"
				<< " mode=" << m_renderPresentationModeName
				<< " skyAndCloudScene=headless_final_pipeline_not_ready"
				<< std::endl;
		}
		return;
	}

	m_cloudNodes.clear();
	if (m_enableStage7SkyHorizon && m_stage7UseReal3DBackground)
	{
		m_skyNode = m_renderRoot.attach_new_node(CreateStage7SkyDomeNode());
		m_skyNode.set_scale(1.0f);
		m_skyNode.set_depth_write(false);
		m_skyNode.set_depth_test(false);
		m_skyNode.set_two_sided(true);
		m_skyNode.set_bin("background", 0);
		ApplyInfraredShader(m_skyNode, true);
		m_skyNode.set_shader_input("u_stage7_background_kind", LVecBase2i(1, 0));

		m_stage7LowerShellNode = m_renderRoot.attach_new_node(CreateStage7LowerHemisphereShellNode());
		m_stage7LowerShellNode.set_depth_write(false);
		m_stage7LowerShellNode.set_depth_test(false);
		m_stage7LowerShellNode.set_two_sided(true);
		m_stage7LowerShellNode.set_bin("background", 1);
		ApplyInfraredShader(m_stage7LowerShellNode, true);
		m_stage7LowerShellNode.set_shader_input("u_stage7_background_kind", LVecBase2i(2, 0));

		IRRuntimeEnvironment environment = BuildRuntimeEnvironment();
		InitStage7WeatherScene();
		UpdateStage7SkyHorizon(environment, "init-sky", true);
		UpdateStage7WeatherNodes(m_stage7WeatherState, ClockObject::get_global_clock()->get_frame_time());
		std::cout << "Stage7B/7C真实3D背景初始化完成：skyDome=1 lowerShell=1 flatGroundPlane=0 cloud cards="
			<< m_cloudNodes.size()
			<< " precipitationCards=" << m_stage7PrecipitationNodes.size()
			<< std::endl;
		return;
	}

	CardMaker skyMaker("IR_Sky_Background");
	skyMaker.set_frame(-1.0f, 1.0f, -1.0f, 1.0f);
	m_skyNode = m_cameraNode.attach_new_node(skyMaker.generate());
	m_skyNode.set_pos(0.0f, 2500.0f, 0.0f);
	m_skyNode.set_scale(2600.0f, 1.0f, 1600.0f);
	m_skyNode.set_depth_write(false);
	m_skyNode.set_depth_test(false);
	m_skyNode.set_bin("background", 0);
	ApplyInfraredShader(m_skyNode, true);

	for (int i = 0; i < 18; ++i)
	{
		CardMaker cloudMaker("IR_Cloud_Particle");
		cloudMaker.set_frame(-1.0f, 1.0f, -0.45f, 0.45f);
		NodePath cloud = m_cameraNode.attach_new_node(cloudMaker.generate());
		float x = -900.0f + static_cast<float>((i * 137) % 1800);
		float z = 180.0f + static_cast<float>((i * 79) % 620);
		float y = 1200.0f + static_cast<float>((i % 5) * 120);
		float scale = 160.0f + static_cast<float>((i % 4) * 45);
		cloud.set_pos(x, y, z);
		cloud.set_scale(scale * 1.9f, 1.0f, scale);
		cloud.set_transparency(TransparencyAttrib::M_alpha);
		cloud.set_depth_write(false);
		cloud.set_bin("transparent", 10);
		ApplyInfraredShader(cloud, false);
		m_cloudNodes.push_back(cloud);
	}

	std::cout << "天空背景与粒子云初始化完成：cloud cards=" << m_cloudNodes.size() << std::endl;
}

// 初始化红外仿真着色器
void HwaSimIR::InitInfraredShader() {
	// 顶点着色器：计算世界坐标并传递局部坐标给片段着色器计算亮斑距离
	std::string vertex_shader = R"(
    #version 100
    uniform mat4 p3d_ModelViewProjectionMatrix;
    attribute vec4 p3d_Vertex;
    attribute vec3 p3d_Normal;
    attribute vec2 p3d_MultiTexCoord0;
    
    varying vec2 texcoord;
    varying vec3 v_local_pos; // 传递模型局部坐标系下的三维坐标
    varying vec3 v_stage5_normal;

    void main() {
        gl_Position = p3d_ModelViewProjectionMatrix * p3d_Vertex;
        texcoord = p3d_MultiTexCoord0;
        v_local_pos = p3d_Vertex.xyz; // 提取局部坐标
        v_stage5_normal = p3d_Normal;
    }
    )";

	// 片段着色器：实现白热模式、中/长波切换、一头一尾独立热源亮斑
	std::string fragment_shader = R"(
    #version 100
    precision mediump float;

    uniform sampler2D p3d_Texture0;
    
    uniform int u_is_background;
    uniform int u_object_kind;     // 0:目标 1:天空 2:粒子云 3:雨雪card 4:EnginePlume
    uniform int u_wave_band;       // deprecated compatibility uniform; prefer u_ir_band_index/class
    uniform int u_ir_band_index;   // 0 VIS, 1 NIR, 2 SWIR, 3 MWIR, 4 LWIR
    uniform int u_ir_band_class;   // 0 reflective, 1 mixed, 2 thermal
    uniform float u_base_temperature;
    uniform float u_time;
    uniform float u_ir_radiance;
    uniform float u_emissivity;
    uniform float u_reflectance;
    uniform float u_tau_up;
    uniform float u_path_radiance;
    uniform float u_sky_radiance;
    uniform float u_display_gain;
    uniform float u_display_offset;
    uniform float u_cloud_density;
    uniform sampler2D p3d_Texture1;
    uniform int u_material_id_ready;
    uniform int u_debug_material_id;
    uniform int u_material_param_count;
    uniform float u_material_ids[8];
    uniform vec4 u_material_params[8];
    uniform int u_stage4_visual_debug; // 阶段4可视化诊断：默认0，只在排查Hotspot/BrightSpot接线时打开
    uniform int u_stage5_radiance_debug_en; // Stage5A minimal radiance debug switch, default 0
    uniform int u_stage5_debug_view_mode;   // 0 Off, 1 Body, 2 Reflected, 3 RearHotspot, 4 Plume, 5 BrightSpot, 6 Atmosphere, 7 SensorInput
    uniform int u_stage5_use_base_texture_modulation;
    uniform float u_material_temp_K;
    uniform float u_material_emissivity;
    uniform float u_stage5_body_radiance;
    uniform float u_stage5_rear_hotspot_radiance;
    uniform float u_stage5_plume_radiance;
    uniform float u_stage5_brightspot_radiance;
    uniform float u_stage5_path_radiance;
    uniform float u_stage5_sensor_input_radiance;
    uniform float u_stage5_sensor_input_display_gray;
    uniform int u_stage5_use_sensor_input_for_display;
    uniform float u_body_radiance_scale;
    uniform float u_stage5_body_gray;
    uniform float u_stage5_reflected_radiance;
    uniform float u_stage5_reflected_gray;
    uniform float u_stage5_reflectance_band;
    uniform float u_stage5_solar_weight;
    uniform vec3 u_stage5_sun_dir_local;
    uniform float u_stage5_hotspot_gray;
    uniform float u_stage5_brightspot_gray;
    uniform float u_stage5_final_gray_debug;
    uniform float u_stage5_body_display_gray;
    uniform float u_stage5_reflected_display_gray;
    uniform float u_stage5_hotspot_display_gray;
    uniform float u_stage5_brightspot_display_gray;
    uniform float u_stage5_plume_display_gray;
    uniform float u_stage5_atmosphere_display_gray;
    uniform float u_stage5_composite_min_gray;
    uniform float u_stage5_composite_max_gray;
    uniform int u_stage5_display_fallback_applied;
    uniform int u_stage6_display_en;
    uniform int u_stage6_white_hot;
    uniform float u_stage6_display_gain;
    uniform float u_stage6_display_offset;
    uniform int u_stage6_noise_enable;
    uniform float u_stage6_noise_sigma_norm;
    uniform int u_stage6_background_display_en;
    uniform int u_stage7_sky_horizon_en;
    uniform int u_stage7_background_kind; // 0 legacy, 1 3D sky dome, 2 lower ground/sea shell
    uniform float u_stage7_sky_gray;
    uniform float u_stage7_ground_gray;
    uniform float u_stage7_horizon_y;
    uniform float u_stage7_camera_roll;
    uniform int u_stage7_weather_type;
    uniform float u_stage7_cloud_coverage;
    uniform float u_stage7_cloud_opacity;
    uniform float u_stage7_cloud_temperature_K;
    uniform float u_stage7_cloud_gray;
    uniform float u_stage7_fog_density;
    uniform float u_stage7_fog_gray;
    uniform int u_stage7_precipitation_type; // 0 none, 1 rain, 2 snow
    uniform float u_stage7_precipitation_density;
    uniform float u_stage7_precipitation_speed;
    uniform float u_stage7_sun_direct_scale;
    uniform float u_stage7_sky_diffuse_scale;
    uniform float u_stage7_target_contrast_scale;
    uniform int u_plume_enabled;
    uniform int u_plume_layer; // 1 core, 2 halo
    uniform float u_plume_temperature_K;
    uniform float u_plume_gray;
    uniform float u_plume_opacity;
    uniform float u_plume_length;
    uniform float u_plume_radius_root;
    uniform float u_plume_radius_tail;
    uniform float u_plume_axial_decay;
    uniform float u_plume_radial_decay;
    uniform float u_plume_noise_scale;
    uniform float u_plume_noise_strength;
    uniform float u_plume_band_gain;

    // 头部热源参数
    uniform int u_hotspot_front_en;
    uniform vec3 u_hotspot_front_pos;
    uniform float u_hotspot_front_radius;
    uniform float u_hotspot_front_temp;

    // 尾部热源参数
    uniform int u_hotspot_rear_en;
    uniform vec3 u_hotspot_rear_pos;
    uniform float u_hotspot_rear_radius;
    uniform float u_hotspot_rear_temp;

	// ================= 表面亮斑参数 =================
    uniform int u_brightspot_en;       // 亮斑开关：0-关，1-开
    uniform vec3 u_brightspot_pos;     // 亮斑中心位置(局部坐标)
    uniform float u_brightspot_radius; // 亮斑基础半径
    uniform float u_brightspot_temp;   // legacy命名：阶段4按亮斑intensity使用，不代表Kelvin温度
    // ======================================================

    varying vec2 texcoord;
    varying vec3 v_local_pos;
    varying vec3 v_stage5_normal;

    float Stage6Noise(vec2 pixel)
    {
        return fract(sin(dot(pixel + vec2(u_time * 17.13, u_time * 3.71), vec2(12.9898, 78.233))) * 43758.5453);
    }

    float ApplyStage6Display(float gray)
    {
        gray = clamp(gray, 0.0, 1.0);
        if (u_stage6_display_en != 1) {
            return gray;
        }
        gray = gray * u_stage6_display_gain + u_stage6_display_offset;
        if (u_stage6_noise_enable == 1 && u_stage6_noise_sigma_norm > 0.0) {
            gray += (Stage6Noise(gl_FragCoord.xy) * 2.0 - 1.0) * u_stage6_noise_sigma_norm;
        }
        gray = clamp(gray, 0.0, 1.0);
        if (u_stage6_white_hot == 0) {
            gray = 1.0 - gray;
        }
        return clamp(gray, 0.0, 1.0);
    }

    vec4 Stage6DisplayColor(float gray, float alpha)
    {
        float display_gray = ApplyStage6Display(gray);
        return vec4(display_gray, display_gray, display_gray, alpha);
    }

    vec4 Stage6BackgroundDisplayColor(float gray, float alpha)
    {
        float safe_gray = clamp(gray, 0.0, 1.0);
        if (u_stage6_background_display_en != 1) {
            return vec4(safe_gray, safe_gray, safe_gray, alpha);
        }
        return Stage6DisplayColor(safe_gray, alpha);
    }

    float ApplyStage7WeatherDisplay(float gray)
    {
        float safe_gray = clamp(gray, 0.0, 1.0);
        float contrast = clamp(u_stage7_target_contrast_scale, 0.05, 1.5);
        safe_gray = clamp(u_stage7_fog_gray + (safe_gray - u_stage7_fog_gray) * contrast, 0.0, 1.0);
        float fog_mix = clamp(u_stage7_fog_density, 0.0, 0.78);
        safe_gray = mix(safe_gray, clamp(u_stage7_fog_gray, 0.0, 1.0), fog_mix);
        return clamp(safe_gray, 0.0, 1.0);
    }

    void main() {
        vec4 texColor = texture2D(p3d_Texture0, texcoord);

        if (u_object_kind == 1 || u_is_background == 1) {
            if (u_stage7_sky_horizon_en == 1) {
                float stage7_intensity = u_stage7_sky_gray;
                if (u_stage7_background_kind == 2) {
                    stage7_intensity = u_stage7_ground_gray;
                } else if (u_stage7_background_kind == 1) {
                    float vertical = clamp(v_local_pos.z * 0.5 + 0.5, 0.0, 1.0);
                    stage7_intensity = clamp(u_stage7_sky_gray + vertical * 0.035, 0.0, 1.0);
                    if (u_stage7_cloud_coverage > 0.01 && u_stage7_cloud_opacity > 0.01) {
                        float cloud_noise = 0.5 + 0.25 * sin(v_local_pos.x * 5.7 + u_time * 0.04)
                                                  + 0.25 * cos(v_local_pos.y * 4.3 - u_time * 0.03);
                        float cloud_mask = smoothstep(0.42, 0.78, cloud_noise) *
                                           clamp(u_stage7_cloud_coverage * u_stage7_cloud_opacity, 0.0, 1.0) * 0.55;
                        stage7_intensity = mix(stage7_intensity, clamp(u_stage7_cloud_gray, 0.0, 1.0), cloud_mask);
                    }
                }
                stage7_intensity = ApplyStage7WeatherDisplay(stage7_intensity);
                gl_FragColor = vec4(stage7_intensity, stage7_intensity, stage7_intensity, 1.0);
                return;
            }
            float sky_tint = (u_ir_band_class == 0) ? 0.18 : 0.055;
            float bg_intensity = clamp(u_ir_radiance * u_display_gain + sky_tint, 0.0, 1.0);
            bg_intensity = ApplyStage7WeatherDisplay(bg_intensity);
            gl_FragColor = vec4(bg_intensity, bg_intensity, bg_intensity, 1.0);
            return;
        }

        if (u_object_kind == 2) {
            float edge = 1.0 - smoothstep(0.45, 1.0, length(v_local_pos.xz));
            float texture_density = clamp(max(texColor.a, dot(texColor.rgb, vec3(0.299, 0.587, 0.114))), 0.0, 1.0);
            edge *= mix(0.55, 1.0, texture_density);
            float cloud_noise = 0.65 + 0.20 * sin(v_local_pos.x * 8.0 + u_time * 0.2)
                                      + 0.15 * cos(v_local_pos.y * 11.0);
            float cloud_base = clamp((u_ir_radiance + u_sky_radiance + 0.12) * cloud_noise, 0.0, 1.0);
            float cloud_intensity = mix(cloud_base, clamp(u_stage7_cloud_gray, 0.0, 1.0), 0.70);
            cloud_intensity = ApplyStage7WeatherDisplay(cloud_intensity);
            float cloud_alpha = edge * clamp(max(u_cloud_density, u_stage7_cloud_coverage) * u_stage7_cloud_opacity, 0.0, 0.90);
            gl_FragColor = vec4(cloud_intensity, cloud_intensity, cloud_intensity, cloud_alpha);
            return;
        }

        if (u_object_kind == 3) {
            float density = clamp(u_stage7_precipitation_density, 0.0, 1.0);
            float streak = 1.0 - smoothstep(0.0, 0.52, abs(v_local_pos.x));
            float particle_mask = clamp(max(texColor.a, dot(texColor.rgb, vec3(0.299, 0.587, 0.114))), 0.0, 1.0);
            float fall_noise = 0.55 + 0.45 * fract(sin(dot(v_local_pos.xy + vec2(u_time * u_stage7_precipitation_speed, 0.0), vec2(17.1, 91.7))) * 43758.5453);
            float precip_gray = (u_stage7_precipitation_type == 2) ? 0.82 : 0.55;
            precip_gray = ApplyStage7WeatherDisplay(precip_gray);
            float alpha = streak * density * fall_noise * mix(0.55, 1.0, particle_mask);
            if (u_stage7_precipitation_type == 2) {
                alpha *= 0.65 + 0.35 * (1.0 - smoothstep(0.0, 0.50, length(v_local_pos.xy)));
            }
            gl_FragColor = vec4(precip_gray, precip_gray, precip_gray, clamp(alpha, 0.0, 0.82));
            return;
        }

        if (u_object_kind == 4) {
            if (u_plume_enabled != 1) {
                gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
                return;
            }
            bool is_core = (u_plume_layer == 1);
            float axial = clamp(-v_local_pos.y, 0.0, 1.0);
            float root_radius = max(0.001, u_plume_radius_root);
            float tail_radius = max(0.001, u_plume_radius_tail);
            float radius_norm = mix(1.0, tail_radius / root_radius, axial);
            float radial = length(v_local_pos.xz) / max(0.001, radius_norm);
            float axial_mask = exp(-axial * max(0.001, u_plume_axial_decay));
            float radial_mask = exp(-radial * radial * max(0.001, u_plume_radial_decay));
            if (is_core) {
                axial_mask *= mix(1.28, 0.58, axial);
                radial_mask = pow(radial_mask, 0.82);
            } else {
                axial_mask *= smoothstep(0.0, 0.10, axial) * mix(0.92, 0.50, axial);
                radial_mask = pow(radial_mask, 0.56);
            }
            float flicker = 1.0;
            if (u_plume_noise_strength > 0.0) {
                float n = sin((v_local_pos.x + axial * 2.0) * u_plume_noise_scale + u_time * 9.0)
                        * cos((v_local_pos.z - axial) * (u_plume_noise_scale * 0.73) - u_time * 6.0);
                flicker = clamp(1.0 + n * u_plume_noise_strength, 0.55, 1.35);
            }
            float plume_mask = clamp(axial_mask * radial_mask, 0.0, 1.0);
            float layer_gain = is_core ? 1.12 : 0.86;
            float plume_intensity = clamp(u_plume_gray * layer_gain * flicker * plume_mask, 0.0, 1.0);
            plume_intensity = ApplyStage7WeatherDisplay(plume_intensity);
            float layer_alpha_limit = is_core ? 0.82 : 0.52;
            float alpha_shape = is_core ? (0.32 + plume_intensity * 0.68) : (0.14 + plume_intensity * 0.44);
            float plume_alpha = clamp(u_plume_opacity * plume_mask * alpha_shape, 0.0, layer_alpha_limit);
            gl_FragColor = vec4(plume_intensity, plume_intensity, plume_intensity, plume_alpha);
            return;
        }

    )";
	fragment_shader += R"(
        vec4 surface_param = vec4(u_emissivity, u_reflectance, 0.0, 0.5);
        float material_id = texture2D(p3d_Texture1, texcoord).r;
        if (u_material_id_ready == 1) {
            if (u_debug_material_id == 1) {
                gl_FragColor = vec4(material_id, material_id, material_id, texColor.a);
                return;
            }
            for (int i = 0; i < 8; ++i) {
                if (i < u_material_param_count) {
                    float hit = 1.0 - step(0.006, abs(material_id - u_material_ids[i]));
                    surface_param = mix(surface_param, u_material_params[i], hit);
                }
            }
        }

        float surface_emissivity = clamp(surface_param.x, 0.01, 1.0);
        float surface_reflectance = clamp(surface_param.y, 0.02, 0.95);
		// 计算基础热辐射与范围热源
        float current_temp = u_base_temperature;
        float stage4_debug_mask = 0.0;
        float stage5_rear_mask = 0.0;
        float stage5_bright_mask = 0.0;

        if (u_hotspot_front_en == 1) {
            float dist_front = distance(v_local_pos, u_hotspot_front_pos);
            float factor_front = 1.0 - smoothstep(0.0, u_hotspot_front_radius, dist_front);
            current_temp += factor_front * u_hotspot_front_temp;
        }

        if (u_hotspot_rear_en == 1) {
            float dist_rear = distance(v_local_pos, u_hotspot_rear_pos);
            float flicker = 0.85 + 0.15 * sin(u_time * 15.0);
            float factor_rear = 1.0 - smoothstep(0.0, u_hotspot_rear_radius, dist_rear);
            stage4_debug_mask = max(stage4_debug_mask, factor_rear);
            stage5_rear_mask = max(stage5_rear_mask, factor_rear);
            current_temp += factor_rear * (u_hotspot_rear_temp * flicker);
        }

        float luminance = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));

        // Band class semantics: reflective VIS/NIR/SWIR keep texture detail, MWIR is mixed, LWIR is thermal dominant.
        float detail_weight = (u_ir_band_class == 0) ? 0.7 : ((u_ir_band_class == 1) ? 0.2 : 0.05);
        float temp_weight   = (u_ir_band_class == 0) ? 0.3 : 1.0;

        float final_intensity = current_temp * temp_weight * (0.75 + 0.25 * surface_emissivity)
                              + luminance * detail_weight * (0.70 + 0.30 * surface_reflectance);
        float chain_intensity = u_ir_radiance * surface_emissivity * u_display_gain + u_path_radiance + u_display_offset;
        final_intensity = mix(chain_intensity, final_intensity, 0.28);

		// 计算表面不规则亮斑 (仅在局部生效)
        float brightspot_intensity = 0.0;
        if (u_brightspot_en == 1) {
            float dist_bs = distance(v_local_pos, u_brightspot_pos);
            
            // 使用三角函数对局部空间坐标进行高频采样，制造“不规则”的边缘效果
            float irregularity = (sin(v_local_pos.x * 15.0) + cos(v_local_pos.y * 15.0 + v_local_pos.z * 5.0)) * 0.15 * u_brightspot_radius;
            float current_radius = u_brightspot_radius + irregularity;
            
            // 边缘过渡：从 current_radius 的 40% 处开始向外羽化衰减
            float factor_bs = 1.0 - smoothstep(current_radius * 0.4, current_radius, dist_bs);
            stage4_debug_mask = max(stage4_debug_mask, factor_bs);
            stage5_bright_mask = max(stage5_bright_mask, factor_bs);
            brightspot_intensity = factor_bs * u_brightspot_temp;
        }

    )";
	fragment_shader += R"(
        if (u_stage5_radiance_debug_en == 1) {
            // Stage5 debug uses normalized body/reflection/hotspot/brightspot components only.
            // Stage5B reflection is a direct empirical term; it intentionally does not use MODTRAN solar irradiance tables.
            float body_debug = clamp(u_stage5_body_display_gray, 0.0, 1.0);
            if (u_stage5_use_base_texture_modulation == 1) {
                body_debug *= clamp(0.65 + 0.35 * luminance, 0.0, 1.0);
            }
            vec3 stage5_normal_raw = v_stage5_normal;
            vec3 stage5_normal = (dot(stage5_normal_raw, stage5_normal_raw) < 0.001)
                ? vec3(0.0, 0.0, 1.0)
                : normalize(stage5_normal_raw);
            vec3 stage5_sun_raw = u_stage5_sun_dir_local;
            vec3 stage5_sun_dir = (dot(stage5_sun_raw, stage5_sun_raw) < 0.001)
                ? vec3(0.0, 0.0, 1.0)
                : normalize(stage5_sun_raw);
            float ndotl_shader = clamp(dot(stage5_normal, stage5_sun_dir), 0.0, 1.0);
            float texture_luma_shader = clamp(luminance, 0.0, 1.0);
            float reflected_debug = clamp(u_stage5_reflected_display_gray, 0.0, 1.0) * ndotl_shader * texture_luma_shader;
            float rear_hotspot_debug = stage5_rear_mask * clamp(u_stage5_hotspot_display_gray, 0.0, 1.0);
            float plume_debug = stage5_rear_mask * clamp(u_stage5_plume_display_gray, 0.0, 1.0);
            float brightspot_debug = stage5_bright_mask * clamp(u_stage5_brightspot_display_gray, 0.0, 1.0);
            float atmosphere_debug = clamp(u_stage5_atmosphere_display_gray, 0.0, 1.0);
            float sensor_input_debug = clamp(u_stage5_sensor_input_display_gray, 0.0, 1.0);
            float stage5_intensity = 0.0;
            if (u_stage5_use_sensor_input_for_display == 1) {
                stage5_intensity = sensor_input_debug;
            } else if (u_stage5_debug_view_mode == 1) {
                stage5_intensity = body_debug;
            } else if (u_stage5_debug_view_mode == 2) {
                stage5_intensity = reflected_debug;
            } else if (u_stage5_debug_view_mode == 3) {
                stage5_intensity = rear_hotspot_debug;
            } else if (u_stage5_debug_view_mode == 4) {
                stage5_intensity = plume_debug;
            } else if (u_stage5_debug_view_mode == 5) {
                stage5_intensity = brightspot_debug;
            } else if (u_stage5_debug_view_mode == 6) {
                stage5_intensity = atmosphere_debug;
            } else if (u_stage5_debug_view_mode == 7) {
                stage5_intensity = sensor_input_debug;
            } else {
                float composite_min = clamp(u_stage5_composite_min_gray, 0.0, 1.0);
                float composite_max = max(composite_min, clamp(u_stage5_composite_max_gray, 0.0, 1.0));
                stage5_intensity = clamp(body_debug + reflected_debug + rear_hotspot_debug + plume_debug + brightspot_debug + atmosphere_debug, composite_min, composite_max);
            }
            if (u_stage4_visual_debug == 1 && stage4_debug_mask > 0.0) {
                stage5_intensity = max(stage5_intensity, clamp(0.25 + stage4_debug_mask * 0.75, 0.0, 1.0));
            }
            stage5_intensity = ApplyStage7WeatherDisplay(stage5_intensity);
            gl_FragColor = vec4(stage5_intensity, stage5_intensity, stage5_intensity, texColor.a);
            return;
        }

    )";
	fragment_shader += R"(
        // 合成（限制在纯白 1.0 以内）
        final_intensity = clamp(final_intensity + brightspot_intensity, 0.0, 1.0);
        if (u_stage4_visual_debug == 1 && stage4_debug_mask > 0.0) {
            // 只在显式打开诊断开关时抬亮mask区域，用于确认uniform已经进入可见像素链路。
            final_intensity = max(final_intensity, clamp(0.25 + stage4_debug_mask * 0.75, 0.0, 1.0));
        }
        //float final_intensity = clamp(current_temp * temp_weight + luminance * detail_weight, 0.0, 1.0);

        final_intensity = ApplyStage7WeatherDisplay(final_intensity);
        gl_FragColor = vec4(final_intensity, final_intensity, final_intensity, texColor.a);
    }
    )";

	m_irShader = Shader::make(Shader::SL_GLSL, vertex_shader, fragment_shader);
	if (!m_irShader) {
		std::cerr << "红外仿真着色器编译失败！" << std::endl;
	}
}

// 挂载红外着色器及默认参数
void HwaSimIR::ApplyInfraredShader(NodePath& node, bool isBackground) {
	if (!m_irShader || node.is_empty()) return;

	// 优先级设为 1，强制覆盖底层自带材质
	node.set_shader(m_irShader, 1);

	// 为所有声明过的 uniform 赋初值，防止 HwaSimIR 渲染器报 not present 错误！

	// 全局与环境参数
	node.set_shader_input("u_is_background", LVecBase2i(isBackground ? 1 : 0, 0));
	node.set_shader_input("u_object_kind", LVecBase2i(isBackground ? 1 : 0, 0));
	node.set_shader_input("u_wave_band", LVecBase2i(3, 0));       // deprecated compatibility: internal IRBand index
	node.set_shader_input("u_ir_band_index", LVecBase2i(3, 0));   // 默认 MWIR
	node.set_shader_input("u_ir_band_class", LVecBase2i(1, 0));   // 默认 mixed
	node.set_shader_input("u_time", LVecBase2f(0.0f, 0.0f));      // 初始化时间，解决报错
	node.set_shader_input("u_base_temperature", LVecBase2f(0.4f, 0.0f));
	node.set_shader_input("u_ir_radiance", LVecBase2f(0.25f, 0.0f));
	node.set_shader_input("u_emissivity", LVecBase2f(0.85f, 0.0f));
	node.set_shader_input("u_reflectance", LVecBase2f(0.15f, 0.0f));
	node.set_shader_input("u_tau_up", LVecBase2f(0.85f, 0.0f));
	node.set_shader_input("u_path_radiance", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_sky_radiance", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_display_gain", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_display_offset", LVecBase2f(0.02f, 0.0f));
	node.set_shader_input("u_cloud_density", LVecBase2f(0.35f, 0.0f));

	// 阶段2材质映射默认值：没有材质ID纹理时仍按整目标默认材质稳定渲染。
	PTA_float defaultMaterialIds;
	PTA_LVecBase4f defaultMaterialParams;
	for (int i = 0; i < 8; ++i)
	{
		defaultMaterialIds.push_back(0.0f);
		defaultMaterialParams.push_back(LVecBase4f(0.85f, 0.15f, 0.40f, 0.50f));
	}
	node.set_shader_input("u_material_id_ready", LVecBase2i(0, 0));
	node.set_shader_input("u_debug_material_id", LVecBase2i(0, 0));
	node.set_shader_input("u_material_param_count", LVecBase2i(0, 0));
	node.set_shader_input("u_material_ids", defaultMaterialIds);
	node.set_shader_input("u_material_params", defaultMaterialParams);
	node.set_shader_input("u_stage4_visual_debug", LVecBase2i(0, 0)); // 默认关闭阶段4可视化诊断
	node.set_shader_input("u_stage5_radiance_debug_en", LVecBase2i(0, 0)); // 默认关闭Stage5A最小辐射debug
	node.set_shader_input("u_stage5_debug_view_mode", LVecBase2i(0, 0));
	node.set_shader_input("u_stage5_use_base_texture_modulation", LVecBase2i(0, 0));
	node.set_shader_input("u_material_temp_K", LVecBase2f(300.0f, 0.0f));
	node.set_shader_input("u_material_emissivity", LVecBase2f(0.85f, 0.0f));
	SetShaderInputCached(node, "u_stage5_body_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_rear_hotspot_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_plume_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_brightspot_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_path_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_sensor_input_radiance", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_sensor_input_display_gray", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_use_sensor_input_for_display", LVecBase2i(0, 0));
	node.set_shader_input("u_body_radiance_scale", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_radiance", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflectance_band", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_solar_weight", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_sun_dir_local", LVecBase3f(0.0f, 0.0f, 1.0f));
	node.set_shader_input("u_stage5_hotspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_final_gray_debug", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_body_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_reflected_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_hotspot_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_brightspot_display_gray", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_plume_display_gray", LVecBase2f(0.0f, 0.0f));
	SetShaderInputCached(node, "u_stage5_atmosphere_display_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_min_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage5_composite_max_gray", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_stage5_display_fallback_applied", LVecBase2i(0, 0));
	node.set_shader_input("u_stage6_background_display_en", LVecBase2i(1, 0));
	node.set_shader_input("u_stage7_sky_horizon_en", LVecBase2i(0, 0));
	node.set_shader_input("u_stage7_background_kind", LVecBase2i(0, 0));
	node.set_shader_input("u_stage7_sky_gray", LVecBase2f(0.12f, 0.0f));
	node.set_shader_input("u_stage7_ground_gray", LVecBase2f(0.30f, 0.0f));
	node.set_shader_input("u_stage7_horizon_y", LVecBase2f(0.50f, 0.0f));
	node.set_shader_input("u_stage7_camera_roll", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_weather_type", LVecBase2i(0, 0));
	node.set_shader_input("u_stage7_cloud_coverage", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_cloud_opacity", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_cloud_temperature_K", LVecBase2f(255.0f, 0.0f));
	node.set_shader_input("u_stage7_cloud_gray", LVecBase2f(0.5f, 0.0f));
	node.set_shader_input("u_stage7_fog_density", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_fog_gray", LVecBase2f(0.45f, 0.0f));
	node.set_shader_input("u_stage7_precipitation_type", LVecBase2i(0, 0));
	node.set_shader_input("u_stage7_precipitation_density", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_precipitation_speed", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_stage7_sun_direct_scale", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_stage7_sky_diffuse_scale", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_stage7_target_contrast_scale", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_plume_enabled", LVecBase2i(0, 0));
	node.set_shader_input("u_plume_layer", LVecBase2i(0, 0));
	node.set_shader_input("u_plume_temperature_K", LVecBase2f(300.0f, 0.0f));
	node.set_shader_input("u_plume_gray", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_plume_opacity", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_plume_length", LVecBase2f(1.0f, 0.0f));
	node.set_shader_input("u_plume_radius_root", LVecBase2f(0.1f, 0.0f));
	node.set_shader_input("u_plume_radius_tail", LVecBase2f(0.3f, 0.0f));
	node.set_shader_input("u_plume_axial_decay", LVecBase2f(2.4f, 0.0f));
	node.set_shader_input("u_plume_radial_decay", LVecBase2f(3.8f, 0.0f));
	node.set_shader_input("u_plume_noise_scale", LVecBase2f(5.0f, 0.0f));
	node.set_shader_input("u_plume_noise_strength", LVecBase2f(0.0f, 0.0f));
	node.set_shader_input("u_plume_band_gain", LVecBase2f(1.0f, 0.0f));
	ApplyStage7WeatherInputs(node, m_stage7WeatherState);
	ApplyStage6DisplayShaderInputs(node);

	// 头部亮斑默认配置
	node.set_shader_input("u_hotspot_front_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_front_pos", LVecBase3f(-0.3f, 0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_radius", LVecBase2f(0.5f, 0.0f));
	node.set_shader_input("u_hotspot_front_temp", LVecBase2f(1.0f, 0.0f));

	// 阶段4 ThermalHotspot 默认关闭；运行时仅由 TargetState.engineState 控制发动机/尾喷热源。
	node.set_shader_input("u_hotspot_rear_en", LVecBase2i(0, 0));
	node.set_shader_input("u_hotspot_rear_pos", LVecBase3f(0.0f, 0.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_radius", LVecBase2f(2.0f, 0.0f));
	node.set_shader_input("u_hotspot_rear_temp", LVecBase2f(1.2f, 0.0f));

	// 阶段4 BrightSpot 默认关闭；运行时仅由 WeaponState.strikeFlag/strikePart 控制。
	node.set_shader_input("u_brightspot_en", LVecBase2i(0, 0)); // 默认关闭
	// 初始位置
	node.set_shader_input("u_brightspot_pos", LVecBase3f(0.0f, 0.0f, 2.0f));
	node.set_shader_input("u_brightspot_radius", LVecBase2f(1.0f, 0.0f)); // 亮斑大小
	node.set_shader_input("u_brightspot_temp", LVecBase2f(0.0f, 0.0f));   // legacy命名：传入intensity，不是Kelvin温度
	InvalidateShaderInputCache(node);
}

void HwaSimIR::ApplyRadianceInputs(NodePath& node, const IRObjectRadianceOutput& radiance, int objectKind)
{
	if (node.is_empty())
	{
		return;
	}

	SetShaderInputCached(node, "u_object_kind", LVecBase2i(objectKind, 0));
	const IRBand shaderBand = static_cast<IRBand>(static_cast<int>(radiance.bandIndex));
	SetShaderInputCached(node, "u_wave_band", LVecBase2i(static_cast<int>(radiance.bandIndex), 0)); // deprecated compatibility
	SetShaderInputCached(node, "u_ir_band_index", LVecBase2i(static_cast<int>(shaderBand), 0));
	SetShaderInputCached(node, "u_ir_band_class", LVecBase2i(IRBandClassForShader(shaderBand), 0));
	SetShaderInputCached(node, "u_ir_radiance", LVecBase2f(radiance.baseRadiance, 0.0f));
	SetShaderInputCached(node, "u_emissivity", LVecBase2f(radiance.emissivity, 0.0f));
	SetShaderInputCached(node, "u_reflectance", LVecBase2f(radiance.reflectance, 0.0f));
	SetShaderInputCached(node, "u_tau_up", LVecBase2f(radiance.tauUp, 0.0f));
	SetShaderInputCached(node, "u_path_radiance", LVecBase2f(radiance.pathRadiance, 0.0f));
	SetShaderInputCached(node, "u_sky_radiance", LVecBase2f(radiance.skyRadiance, 0.0f));
	SetShaderInputCached(node, "u_display_gain", LVecBase2f(radiance.displayGain, 0.0f));
	SetShaderInputCached(node, "u_display_offset", LVecBase2f(radiance.displayOffset, 0.0f));
	SetShaderInputCached(node, "u_base_temperature", LVecBase2f(radiance.baseRadiance, 0.0f));
	ApplyStage7WeatherInputs(node, m_stage7WeatherState);
	const int stage5BandIndex = Stage5BandIndex(shaderBand);
	SetShaderInputCached(node, "u_stage5_radiance_debug_en", LVecBase2i(m_enableStage5RadianceDebug ? 1 : 0, 0));
	SetShaderInputCached(node, "u_stage5_debug_view_mode", LVecBase2i(m_stage5DebugViewMode, 0));
	SetShaderInputCached(node, "u_stage5_use_base_texture_modulation", LVecBase2i(m_stage5UseBaseTextureModulationByBand[stage5BandIndex] ? 1 : 0, 0));
	if (objectKind != 0)
	{
		SetShaderInputCached(node, "u_material_temp_K", LVecBase2f(radiance.temperatureK, 0.0f));
		SetShaderInputCached(node, "u_material_emissivity", LVecBase2f(radiance.emissivity, 0.0f));
		SetShaderInputCached(node, "u_stage5_body_radiance", LVecBase2f(radiance.baseRadiance, 0.0f));
		SetShaderInputCached(node, "u_stage5_rear_hotspot_radiance", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_plume_radiance", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_brightspot_radiance", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_path_radiance", LVecBase2f(radiance.pathRadiance, 0.0f));
		SetShaderInputCached(node, "u_stage5_sensor_input_radiance", LVecBase2f(radiance.baseRadiance, 0.0f));
		SetShaderInputCached(node, "u_stage5_sensor_input_display_gray", LVecBase2f(radiance.baseRadiance, 0.0f));
		SetShaderInputCached(node, "u_stage5_use_sensor_input_for_display", LVecBase2i(0, 0));
		SetShaderInputCached(node, "u_body_radiance_scale", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_body_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_reflected_radiance", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_reflected_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_reflectance_band", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_solar_weight", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_sun_dir_local", LVecBase3f(0.0f, 0.0f, 1.0f));
		SetShaderInputCached(node, "u_stage5_hotspot_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_brightspot_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_final_gray_debug", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_body_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_reflected_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_hotspot_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_brightspot_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_plume_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_atmosphere_display_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_composite_min_gray", LVecBase2f(0.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_composite_max_gray", LVecBase2f(1.0f, 0.0f));
		SetShaderInputCached(node, "u_stage5_display_fallback_applied", LVecBase2i(0, 0));
	}
	ApplyStage6DisplayShaderInputs(node);
}

IRObjectRadianceOutput HwaSimIR::EvaluateNodeRadiance(const std::string& materialName, const NodePath& node, bool engineOn, bool damaged, bool isSky, bool isCloud, double cloudDensity, double targetAltitudeMeters)
{
	IRObjectRadianceInput input;
	input.materialName = materialName;
	input.rangeMeters = isSky ? 5000.0 : static_cast<double>(EstimateRangeToCamera(node));
	// Stage2A: engineState is local to EngineRear/EnginePlume.  The CPU body
	// radiance model still has a legacy whole-body heating branch, but production
	// input keeps it disabled unless the explicit legacy/debug switch is enabled.
	input.engineOn = m_stage4LegacyEngineBodyHeating && engineOn;
	input.damaged = damaged;
	input.isSky = isSky;
	input.isCloud = isCloud;
	input.cloudDensity = cloudDensity;

	if (m_stage0DisplayFrameCount > 0 && IsReasonableAltitudeMeters(m_realTimeSceneData.platLoc.alt))
	{
		input.observerAltitudeMeters = m_realTimeSceneData.platLoc.alt;
		input.hasObserverAltitude = true;
	}
	else if (m_isAddPlatform && IsReasonableAltitudeMeters(m_initSceneData.platParam[0].spatial.alt))
	{
		input.observerAltitudeMeters = m_initSceneData.platParam[0].spatial.alt;
		input.hasObserverAltitude = true;
	}

	if (IsReasonableAltitudeMeters(targetAltitudeMeters))
	{
		input.targetAltitudeMeters = targetAltitudeMeters;
		input.hasTargetAltitude = true;
	}
	else if (!isSky && !node.is_empty() && !m_renderRoot.is_empty() && input.hasObserverAltitude)
	{
		LPoint3f nodePos = node.get_pos(m_renderRoot);
		double estimatedTargetAltitude = input.observerAltitudeMeters + static_cast<double>(nodePos.get_z());
		if (IsReasonableAltitudeMeters(estimatedTargetAltitude))
		{
			input.targetAltitudeMeters = estimatedTargetAltitude;
			input.hasTargetAltitude = true;
		}
	}

	return m_irRadianceModel.evaluate(input);
}

std::string HwaSimIR::MaterialNameForPlatform(PLATFORM_TYPE type) const
{
	// CPU 辐亮度模型当前仍按平台默认材质计算；像素级差异由阶段2 shader 材质ID纹理补充。
	std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(type);
	if (resIter != m_platformResMap.end() && !resIter->second.defaultMaterialName.empty())
	{
		return resIter->second.defaultMaterialName;
	}

	switch (type)
	{
	case F35:
	case J20:
		return "BM_METAL-ALUMINIUM";
	case AIM9:
		return "IR_CERAMIC";
	case AIM120:
		return "BM_METAL-STEEL";
	case MMD:
		return "BM_METAL";
	default:
		return "BM_METAL-ALUMINIUM";
	}
}

float HwaSimIR::EstimateRangeToCamera(const NodePath& node) const
{
	if (node.is_empty() || m_cameraNode.is_empty())
	{
		return 500.0f;
	}

	LPoint3f nodePos = node.get_pos(m_renderRoot);
	LPoint3f cameraPos = m_cameraNode.get_pos(m_renderRoot);
	LVector3f delta = nodePos - cameraPos;
	float range = delta.length();
	return std::max(1.0f, range);
}

bool HwaSimIR::IsValidTargetStateKey(const BYHWICD::TargetState& targetState) const
{
	return TargetTypeToPlatformType(targetState.targetType) != NONE;
}

bool HwaSimIR::TargetKeyMatches(const BYHWICD::TargetState& targetState, const TargetPlatformData& targetPlat) const
{
	if (!targetPlat.isExist || !IsValidTargetStateKey(targetState))
	{
		return false;
	}
	// TargetState 的唯一目标键为 targetType + targetPlatID + targetID，不能只看 targetID。
	return targetPlat.type == TargetTypeToPlatformType(targetState.targetType)
		&& targetPlat.targetState.targetType == targetState.targetType
		&& targetPlat.targetState.targetPlatID == targetState.targetPlatID
		&& targetPlat.targetState.targetID == targetState.targetID;
}

bool HwaSimIR::WeaponTargetKeyMatches(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const
{
	if (!targetPlat.isExist || weaponState.targetType == 0x00)
	{
		return false;
	}
	// WeaponState 同样使用三元组定位目标；strike/lookat/viewValid 都不能只按 targetID 匹配。
	return targetPlat.type == TargetTypeToPlatformType(weaponState.targetType)
		&& targetPlat.targetState.targetType == weaponState.targetType
		&& targetPlat.targetState.targetPlatID == weaponState.targetPlatID
		&& targetPlat.targetState.targetID == weaponState.targetID;
}

TargetPlatformData* HwaSimIR::FindTargetPlatformByTargetState(const BYHWICD::TargetState& targetState)
{
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (TargetKeyMatches(targetState, targetPlat))
		{
			return &targetPlat;
		}
	}
	return nullptr;
}

TargetPlatformData* HwaSimIR::FindTargetPlatformByWeaponState(const BYHWICD::WeaponState& weaponState)
{
	for (auto& targetPlat : m_targetPlatformList)
	{
		if (WeaponTargetKeyMatches(weaponState, targetPlat))
		{
			return &targetPlat;
		}
	}
	return nullptr;
}

TargetPlatformData* HwaSimIR::FindOrMapTargetPlatform(const BYHWICD::TargetState& targetState, int targetStateIndex)
{
	TargetPlatformData* mappedTarget = FindTargetPlatformByTargetState(targetState);
	if (mappedTarget != nullptr)
	{
		return mappedTarget;
	}

	const PLATFORM_TYPE platType = TargetTypeToPlatformType(targetState.targetType);
	for (size_t platIdx = 0; platIdx < m_targetPlatformList.size(); ++platIdx)
	{
		TargetPlatformData& targetPlat = m_targetPlatformList[platIdx];
		const bool unmapped = targetPlat.targetState.targetID < 0;
		if (targetPlat.isExist && targetPlat.type == platType && unmapped)
		{
			targetPlat.platID = targetState.targetID;
			targetPlat.targetState.targetType = targetState.targetType;
			targetPlat.targetState.targetPlatID = targetState.targetPlatID;
			targetPlat.targetState.targetID = targetState.targetID;
			targetPlat.nodePath.hide();
			std::cout << "[TargetMapping] bind"
				<< " index=" << targetStateIndex
				<< " platformIndex=" << platIdx
				<< " targetType=0x" << std::hex << targetState.targetType << std::dec
				<< " targetPlatID=" << targetState.targetPlatID
				<< " targetID=" << targetState.targetID
				<< std::endl;
			return &targetPlat;
		}
	}

	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	if (frameSeq <= 3 || (frameSeq % 120) == 0)
	{
		std::cout << "[TargetMapping][WARN] no_free_target_platform"
			<< " index=" << targetStateIndex
			<< " targetType=0x" << std::hex << targetState.targetType << std::dec
			<< " targetPlatID=" << targetState.targetPlatID
			<< " targetID=" << targetState.targetID
			<< std::endl;
	}
	return nullptr;
}

void HwaSimIR::ApplyWeaponCameraControl(BYHWICD::DisplayC2cObjTrackingData& currentData, TargetPlatformData* lookAtTarget)
{
	if (m_cameraNode.is_empty())
	{
		return;
	}

	BYHWICD::WeaponState& weaponState = currentData.weaponState;
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	if (weaponState.lookatEn)
	{
		if (lookAtTarget == nullptr)
		{
			std::cout << "[CameraControl][WARN] lookat_target_not_found"
				<< " targetType=0x" << std::hex << weaponState.targetType << std::dec
				<< " targetPlatID=" << weaponState.targetPlatID
				<< " targetID=" << weaponState.targetID
				<< std::endl;
			return;
		}

		const BYHWICD::SpatialState& platSpatial = currentData.platLoc;
		const BYHWICD::SpatialState& targetSpatial = lookAtTarget->targetState.targetLoc;
		double range = 0.0;
		double relPitch = 0.0;
		double relYaw = 0.0;
		m_attitudeTrans.computeRelativePosition(
			platSpatial.lat, platSpatial.lon, platSpatial.alt,
			platSpatial.roll, platSpatial.pitch, platSpatial.yaw,
			targetSpatial.lat, targetSpatial.lon, targetSpatial.alt,
			range, relPitch, relYaw);

		m_cameraNode.set_hpr(-relYaw, relPitch, 0.0);
		weaponState.offsetAng[0] = relPitch;
		weaponState.offsetAng[1] = relYaw;
		if (frameSeq <= 3 || (frameSeq % 120) == 0)
		{
			std::cout << "[CameraControl]"
				<< " mode=lookat"
				<< " targetPlatID=" << weaponState.targetPlatID
				<< " targetID=" << weaponState.targetID
				<< " relPitch=" << relPitch
				<< " relYaw=" << relYaw
				<< " range=" << range
				<< std::endl;
		}
		return;
	}

	// 手动角模式：xxOutAng[0] 是方位角，xxOutAng[1] 是俯仰角；同步写入 offsetAng[pitch,yaw]。
	const double yaw = weaponState.xxOutAng[0];
	const double pitch = weaponState.xxOutAng[1];
	weaponState.offsetAng[0] = pitch;
	weaponState.offsetAng[1] = yaw;
	m_cameraNode.set_hpr(-yaw, pitch, 0.0);
	if (frameSeq <= 3 || (frameSeq % 120) == 0)
	{
		std::cout << "[CameraControl]"
			<< " mode=angle"
			<< " xxYaw=" << yaw
			<< " xxPitch=" << pitch
			<< " offsetPitch=" << weaponState.offsetAng[0]
			<< " offsetYaw=" << weaponState.offsetAng[1]
			<< std::endl;
	}
}

std::string HwaSimIR::Stage4PlatformName(PLATFORM_TYPE type) const
{
	std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(type);
	if (resIter != m_platformResMap.end() && !resIter->second.displayName.empty())
	{
		return resIter->second.displayName;
	}

	switch (type)
	{
	case F35: return "F35";
	case AIM120: return "AIM120";
	case AIM9: return "AIM9X";
	case MMD: return "MMD";
	default: return "default";
	}
}

bool HwaSimIR::Stage4WeaponAppliesToTarget(const BYHWICD::WeaponState& weaponState, const TargetPlatformData& targetPlat) const
{
	if (!weaponState.strikeFlag)
	{
		return false;
	}
	return WeaponTargetKeyMatches(weaponState, targetPlat);
}

bool HwaSimIR::ApplyStage4TargetState(TargetPlatformData& targetPlat, const BYHWICD::WeaponState& weaponState, float dtSec, float ambientTempK, const IRObjectRadianceOutput& radiance, bool applyNodeInputs)
{
	if (targetPlat.nodePath.is_empty())
	{
		return false;
	}

	const std::string platformName = Stage4PlatformName(targetPlat.type);
	const std::string runtimeKey = platformName + "#plat" + std::to_string(targetPlat.targetState.targetPlatID)
		+ "#target" + std::to_string(targetPlat.targetState.targetID);
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const bool engineState = targetPlat.targetState.engineState;
	IRHotspotState rearHotspot = m_irTemperatureModel.updateEngineRear(platformName, runtimeKey, engineState, dtSec, ambientTempK);
	if (!applyNodeInputs)
	{
		return false;
	}
	float tempSpan = std::max(1.0f, rearHotspot.targetTempK - rearHotspot.ambientTempK);
	float normalizedTemp = std::max(0.0f, std::min(1.0f, (rearHotspot.currentTempK - rearHotspot.ambientTempK) / tempSpan));
	float rearIntensity = normalizedTemp * rearHotspot.intensity;
	float rearRadius = std::max(rearHotspot.size.x, std::max(rearHotspot.size.y, rearHotspot.size.z));
	bool rearEnabledForShader = rearHotspot.enabled;
	IRStage4Vec3 rearPosForShader = rearHotspot.localPos;
	float rearRadiusForShader = rearRadius;
	float rearIntensityForShader = rearIntensity;

	if (m_enableStage4HotspotVisualDebug && m_forceStage4RearHotspotVisible)
	{
		// 诊断模式：覆盖到大半径、强强度，专门确认尾部热源uniform是否能进入像素输出。
		rearEnabledForShader = true;
		rearPosForShader = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		rearRadiusForShader = std::max(rearRadiusForShader, 1000.0f);
		rearIntensityForShader = std::max(rearIntensityForShader, 1.0f);
	}

	// 阶段4 ThermalHotspot：只由 engineState 控制发动机/尾喷热源，不读取 strikeFlag。
	bool wroteStage4Inputs = false;
	if (applyNodeInputs)
	{
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_stage4_visual_debug", LVecBase2i(m_enableStage4HotspotVisualDebug ? 1 : 0, 0));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_hotspot_rear_en", LVecBase2i(rearEnabledForShader ? 1 : 0, 0));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_hotspot_rear_pos", LVecBase3f(rearPosForShader.x, rearPosForShader.y, rearPosForShader.z));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_hotspot_rear_radius", LVecBase2f(rearRadiusForShader, 0.0f));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_hotspot_rear_temp", LVecBase2f(rearIntensityForShader, 0.0f));
	}
	const std::string thermalLogKey = runtimeKey + "#thermal";
	const std::string thermalLogState =
		std::to_string(engineState ? 1 : 0) + ":" + std::to_string(rearEnabledForShader ? 1 : 0);
	const bool logThermal =
		m_enableIRVerboseLog ||
		frameSeq <= 3 ||
		(frameSeq % 120) == 0 ||
		m_enableStage4HotspotVisualDebug ||
		m_lastStage4TargetLogState[thermalLogKey] != thermalLogState;
	m_lastStage4TargetLogState[thermalLogKey] = thermalLogState;
	if (logThermal)
	{
		std::cout << "[Stage4 ThermalHotspot]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " platform=" << platformName
			<< " engineState=" << (engineState ? "1" : "0")
			<< " currentTempK=" << rearHotspot.currentTempK
			<< " targetTempK=" << rearHotspot.targetTempK
			<< " enabled=" << (rearEnabledForShader ? "1" : "0")
			<< std::endl;
	}

	const bool brightSpotApplies = Stage4WeaponAppliesToTarget(weaponState, targetPlat);
	bool invalidStrikePart = false;
	IRBrightSpotState brightSpot = m_irTemperatureModel.resolveBrightSpot(
		platformName,
		brightSpotApplies,
		weaponState.strikePart,
		&invalidStrikePart);
	if (m_enableStage4HotspotVisualDebug && m_forceStage4BrightSpotVisible)
	{
		// 诊断模式：与协议解耦，强制生成一个覆盖面足够大的亮斑，排查shader/坐标/输出映射。
		brightSpot.enabled = true;
		brightSpot.part = IRBrightSpotPart::Head;
		brightSpot.localPos = IRStage4Vec3(0.0f, 0.0f, 0.0f);
		brightSpot.radius = std::max(brightSpot.radius, 1000.0f);
		brightSpot.intensity = std::max(brightSpot.intensity, 1.0f);
	}

	// 阶段4 BrightSpot：只由 WeaponState.strikeFlag/strikePart 控制；u_brightspot_temp 传 intensity，不是温度。
	if (applyNodeInputs)
	{
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_brightspot_en", LVecBase2i(brightSpot.enabled ? 1 : 0, 0));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_brightspot_pos", LVecBase3f(brightSpot.localPos.x, brightSpot.localPos.y, brightSpot.localPos.z));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_brightspot_radius", LVecBase2f(brightSpot.radius, 0.0f));
		wroteStage4Inputs |= SetShaderInputCached(targetPlat.nodePath, "u_brightspot_temp", LVecBase2f(brightSpot.intensity, 0.0f));
	}
	const std::string brightLogKey = runtimeKey + "#bright";
	const std::string brightLogState =
		std::to_string(weaponState.strikeFlag ? 1 : 0) + ":" +
		std::to_string(weaponState.strikePart) + ":" +
		std::to_string(brightSpot.enabled ? 1 : 0);
	const bool logBright =
		m_enableIRVerboseLog ||
		frameSeq <= 3 ||
		(frameSeq % 120) == 0 ||
		m_enableStage4HotspotVisualDebug ||
		m_lastStage4TargetLogState[brightLogKey] != brightLogState;
	m_lastStage4TargetLogState[brightLogKey] = brightLogState;
	if (logBright)
	{
		std::cout << "[Stage4 BrightSpot]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " strikeFlag=" << (weaponState.strikeFlag ? "1" : "0")
			<< " strikePart=" << weaponState.strikePart
			<< " part=" << IRBrightSpotPartName(brightSpot.part)
			<< " localPos=(" << brightSpot.localPos.x << "," << brightSpot.localPos.y << "," << brightSpot.localPos.z << ")"
			<< " radius=" << brightSpot.radius
			<< " intensity=" << brightSpot.intensity
			<< " enabled=" << (brightSpot.enabled ? "1" : "0")
			<< std::endl;
	}

	const bool plumeBypassEngineState =
		m_stage5PlumeOptions.forcePlumeVisible || m_stage5PlumeOptions.enablePlumeDebug;
	const bool plumeEnabledForDiag =
		m_stage5PlumeOptions.enableEnginePlume &&
		applyNodeInputs &&
		(!targetPlat.enginePlumeCoreNodePath.is_empty() || !targetPlat.enginePlumeHaloNodePath.is_empty()) &&
		(!m_stage5PlumeOptions.useEngineState || engineState || plumeBypassEngineState);
	const std::string heatLogKey = runtimeKey + "#heat-source";
	const std::string heatLogState =
		std::to_string(engineState ? 1 : 0) + ":" +
		std::to_string(rearEnabledForShader ? 1 : 0) + ":" +
		std::to_string(plumeEnabledForDiag ? 1 : 0) + ":" +
		std::to_string(weaponState.strikeFlag ? 1 : 0) + ":" +
		std::to_string(weaponState.strikePart) + ":" +
		std::to_string(brightSpot.enabled ? 1 : 0) + ":" +
		std::to_string(static_cast<int>(brightSpot.part)) + ":" +
		std::to_string(m_stage4LegacyEngineBodyHeating ? 1 : 0);
	const bool logHeatSource =
		m_enableIRVerboseLog ||
		frameSeq <= 3 ||
		(frameSeq % 120) == 0 ||
		m_enableStage4HotspotVisualDebug ||
		m_lastStage4TargetLogState[heatLogKey] != heatLogState;
	m_lastStage4TargetLogState[heatLogKey] = heatLogState;
	if (logHeatSource)
	{
		std::cout << "[Stage4 HeatSourceDiag]"
			<< " sourceSeq=" << frameSeq
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " engineState=" << (engineState ? "1" : "0")
			<< " legacyEngineBodyHeating=" << (m_stage4LegacyEngineBodyHeating ? "1" : "0")
			<< " bodyTempK=" << radiance.temperatureK
			<< " bodyRadiance=" << radiance.baseRadiance
			<< " rearHotspotEnabled=" << (rearEnabledForShader ? "1" : "0")
			<< " rearHotspotTempK=" << rearHotspot.currentTempK
			<< " rearHotspotIntensity=" << rearIntensityForShader
			<< " plumeEnabled=" << (plumeEnabledForDiag ? "1" : "0")
			<< " strikeFlag=" << (weaponState.strikeFlag ? "1" : "0")
			<< " strikePart=" << weaponState.strikePart
			<< " brightspotPart=" << IRBrightSpotPartName(brightSpot.part)
			<< " brightspotEnabled=" << (brightSpot.enabled ? "1" : "0")
			<< " brightspotIntensity=" << brightSpot.intensity
			<< std::endl;
	}

	if (applyNodeInputs)
	{
		ApplyStage5RadianceDebug(targetPlat, radiance, rearHotspot, brightSpot, rearEnabledForShader, rearIntensityForShader, runtimeKey, dtSec);
	}

	const bool hasShader = (targetPlat.nodePath.get_shader() != nullptr);
	const std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(targetPlat.type);
	bool baseTextureAvailable = false;
	bool materialIdTexBound = false;
	if (resIter != m_platformResMap.end())
	{
		baseTextureAvailable = !resIter->second.texturePath.empty() && FileExists(resIter->second.texturePath);
		materialIdTexBound = !resIter->second.materialIdTexturePath.empty() && FileExists(resIter->second.materialIdTexturePath);
	}
	const bool logUniform = applyNodeInputs && (logThermal || logBright);
	if (logUniform)
	{
		std::cout << "[Stage4 Uniform]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " nodeName=" << targetPlat.nodePath.get_name()
			<< " hasShader=" << (hasShader ? "1" : "0")
			<< " baseTextureBound=" << (baseTextureAvailable ? "1" : "0")
			<< " materialIdTexBound=" << (materialIdTexBound ? "1" : "0")
			<< " hotspotRearEn=" << (rearEnabledForShader ? "1" : "0")
			<< " brightspotEn=" << (brightSpot.enabled ? "1" : "0")
			<< " brightspotPos=(" << brightSpot.localPos.x << "," << brightSpot.localPos.y << "," << brightSpot.localPos.z << ")"
			<< " brightspotRadius=" << brightSpot.radius
			<< " brightspotIntensity=" << brightSpot.intensity
			<< std::endl;
		if (!hasShader)
		{
			std::cout << "[Stage4 Uniform][WARN] STAGE4_SHADER_NOT_BOUND"
				<< " targetPlatID=" << targetPlat.targetState.targetPlatID
				<< " targetID=" << targetPlat.targetState.targetID
				<< " nodeName=" << targetPlat.nodePath.get_name()
				<< std::endl;
		}
		if (!baseTextureAvailable || !materialIdTexBound)
		{
			std::cout << "[Stage4 Uniform][WARN] STAGE4_TEXTURE_MISSING"
				<< " targetPlatID=" << targetPlat.targetState.targetPlatID
				<< " targetID=" << targetPlat.targetState.targetID
				<< " baseTextureBound=" << (baseTextureAvailable ? "1" : "0")
				<< " materialIdTexBound=" << (materialIdTexBound ? "1" : "0")
				<< std::endl;
		}
	}
	if (m_enableStage4HotspotVisualDebug && logUniform)
	{
		std::cout << "[Stage4 VisualDebug]"
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " enabled=1"
			<< " forceRear=" << (m_forceStage4RearHotspotVisible ? "1" : "0")
			<< " forceBright=" << (m_forceStage4BrightSpotVisible ? "1" : "0")
			<< " rearRadius=" << rearRadiusForShader
			<< " rearIntensity=" << rearIntensityForShader
			<< " brightRadius=" << brightSpot.radius
			<< " brightIntensity=" << brightSpot.intensity
			<< std::endl;
	}
	return wroteStage4Inputs;
}

IRModtranRadianceResult HwaSimIR::QueryStage5ModtranRadiance(const TargetPlatformData& targetPlat, const IRRuntimeEnvironment& environment, const IRObjectRadianceOutput& radiance, const std::string& targetKey)
{
	IRModtranRadianceResult disabledResult;
	if (!Stage5ModtranRadianceCompareEnabled())
	{
		disabledResult.fallbackReason = "debug_disabled";
		return disabledResult;
	}
	if (!m_stage5ModtranRadianceReady)
	{
		disabledResult.fallbackReason = "modtran_lut_missing";
		return disabledResult;
	}

	double observerAltKm = 10.0;
	if (m_stage0DisplayFrameCount > 0 && IsReasonableAltitudeMeters(m_realTimeSceneData.platLoc.alt))
	{
		observerAltKm = m_realTimeSceneData.platLoc.alt / 1000.0;
	}
	else if (m_isAddPlatform && IsReasonableAltitudeMeters(m_initSceneData.platParam[0].spatial.alt))
	{
		observerAltKm = m_initSceneData.platParam[0].spatial.alt / 1000.0;
	}
	double targetAltKm = IsReasonableAltitudeMeters(targetPlat.targetState.targetLoc.alt)
		? targetPlat.targetState.targetLoc.alt / 1000.0
		: observerAltKm;
	const double rangeKmRaw = std::max(0.001, static_cast<double>(EstimateRangeToCamera(targetPlat.nodePath)) / 1000.0);
	const double visibilityKmRaw = std::max(0.001, environment.visibilityMeters / 1000.0);
	const double humidity = std::max(0.0, std::min(100.0, environment.humidityPercent));
	const double solarZenithDeg = std::max(0.0, std::min(180.0, 90.0 - environment.sunElevationDeg));

	IRModtranRadianceQuery query;
	query.band = static_cast<IRBand>(static_cast<int>(radiance.bandIndex));
	query.observerAltKm = observerAltKm;
	query.targetAltKm = targetAltKm;
	query.rangeKm = rangeKmRaw;
	query.visibilityKm = visibilityKmRaw;
	query.humidityPercent = humidity;
	query.solarZenithDeg = solarZenithDeg;

	const double rangeBucket = QuantizeForCache(rangeKmRaw, 0.5);
	const double observerBucket = QuantizeForCache(observerAltKm, 1.0);
	const double targetBucket = QuantizeForCache(targetAltKm, 1.0);
	const double visibilityBucket = QuantizeForCache(visibilityKmRaw, 5.0);
	const double humidityBucket = QuantizeForCache(humidity, 10.0);
	const double solarZenithBucket = QuantizeForCache(solarZenithDeg, 5.0);
	std::ostringstream key;
	key << targetKey
		<< ":band=" << static_cast<int>(query.band)
		<< ":range=" << Stage5ModtranCacheDouble(rangeBucket)
		<< ":obs=" << Stage5ModtranCacheDouble(observerBucket)
		<< ":tgt=" << Stage5ModtranCacheDouble(targetBucket)
		<< ":vis=" << Stage5ModtranCacheDouble(visibilityBucket)
		<< ":hum=" << Stage5ModtranCacheDouble(humidityBucket)
		<< ":sza=" << Stage5ModtranCacheDouble(solarZenithBucket);
	const std::string cacheKey = key.str();
	const auto lookupStart = std::chrono::steady_clock::now();
	std::map<std::string, Stage5ModtranRadianceCacheEntry>::const_iterator cacheIt =
		m_stage5ModtranRadianceCache.find(cacheKey);
	if (cacheIt != m_stage5ModtranRadianceCache.end())
	{
		++m_stage5ModtranCacheHitCurrent;
		m_stage5ModtranLookupMsCurrent += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - lookupStart).count();
		return cacheIt->second.result;
	}

	IRModtranRadianceQuery bucketedQuery = query;
	bucketedQuery.rangeKm = rangeBucket;
	bucketedQuery.observerAltKm = observerBucket;
	bucketedQuery.targetAltKm = targetBucket;
	bucketedQuery.visibilityKm = visibilityBucket;
	bucketedQuery.humidityPercent = humidityBucket;
	bucketedQuery.solarZenithDeg = solarZenithBucket;
	IRModtranRadianceResult result = m_stage5ModtranRadianceLut.query(bucketedQuery);
	result.humidityPercent = humidityBucket;
	m_stage5ModtranRadianceCache[cacheKey].result = result;
	++m_stage5ModtranCacheMissCurrent;
	m_stage5ModtranLookupMsCurrent += std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - lookupStart).count();
	return result;
}

bool HwaSimIR::Stage5ModtranRadianceCompareEnabled() const
{
	return m_enableStage5ModtranRadianceDebug ||
		m_stage5ModtranCompareLegacy ||
		m_stage5ModtranPathRuntimeMode != "Off" ||
		Stage5ModtranPathRuntimeAffectsImage() ||
		m_stage5UseModtranSkyRuntime ||
		m_stage5UseModtranSolarRuntime;
}

bool HwaSimIR::Stage5ModtranPathRuntimeAffectsImage() const
{
	return m_stage5UseModtranPathRuntime &&
		(m_stage5ModtranPathRuntimeMode == "ReplaceLegacy" ||
			m_stage5ModtranPathRuntimeMode == "BlendLegacy");
}

double HwaSimIR::MapSensorInputToDisplayGray(double sensorInputRadiance) const
{
	const double finiteSensorInput = std::isfinite(sensorInputRadiance) ? sensorInputRadiance : 0.0;
	double mapped = finiteSensorInput * m_stage5SensorInputDisplayScale + m_stage5SensorInputDisplayOffset;
	mapped = ClampStage5Double(mapped, m_stage5SensorInputDisplayClampMin, m_stage5SensorInputDisplayClampMax);
	mapped = ClampStage5Double(mapped, 0.0, 1.0);
	if (mapped <= 0.0)
	{
		return 0.0;
	}
	const double gamma = ClampStage5Double(m_stage5SensorInputDisplayGamma, 0.1, 5.0);
	return ClampStage5Double(std::pow(mapped, 1.0 / gamma), 0.0, 1.0);
}

void HwaSimIR::LogEffectiveRuntimeConfig(
	const char* reason,
	int videoFps,
	int targetNumValid,
	bool saveMP4En,
	const char* videoFpsSource,
	const char* targetNumSource,
	const char* saveMP4Source) const
{
	const bool modtranCompareEffective = Stage5ModtranRadianceCompareEnabled();
	std::cout << "[EffectiveRuntimeConfig]"
		<< " reason=" << (reason ? reason : "unknown")
		<< " runtimeConfigPath=" << (m_runtimeConfig.loaded() ? m_runtimeConfig.loadedPath() : "missing")
		<< " runtimeConfigLoaded=" << (m_runtimeConfig.loaded() ? "1" : "0")
		<< " videoFps=" << videoFps
		<< " syncMode=" << (m_bSyncRenderMode.load() ? "1" : "0")
		<< " targetNumValid=" << targetNumValid
		<< " IRUpdateHz=" << m_irUpdateHz
		<< " AnnotationUpdateHz=" << m_annotationUpdateHz
		<< " EnablePerfLog=" << (m_enablePerfLog ? "1" : "0")
		<< " EnableIRVerboseLog=" << (m_enableIRVerboseLog ? "1" : "0")
		<< " EnableMTFBlur=" << (m_stage6MtfBlurEnabled ? "1" : "0")
		<< " MTFBlurMode=" << m_stage6MtfBlurMode
		<< " MTFBlurEffectiveMode=" << m_stage6MtfBlurEffectiveMode
		<< " MTFBlurSigmaPixels=" << m_stage6MtfBlurSigmaPixels
		<< " MTFBlurRadiusPixels=" << m_stage6MtfBlurRadiusPixels
		<< " MTFBlurPasses=" << m_stage6MtfBlurPasses
		<< " MTFApplyTo=" << m_stage6MtfApplyTo
		<< " EnableDetectorNoise=" << (m_stage6DetectorNoiseEnabled ? "1" : "0")
		<< " NoiseApplyTo=" << m_stage6DetectorNoiseApplyTo
		<< " NoisePosition=" << m_stage6DetectorNoisePosition
		<< " EnableTemporalNoise=" << (m_stage6TemporalNoiseEnabled ? "1" : "0")
		<< " TemporalNoiseSigmaGray=" << m_stage6TemporalNoiseSigmaGray
		<< " EnableFPN=" << (m_stage6FpnEnabled ? "1" : "0")
		<< " FPNSigmaGray=" << m_stage6FpnSigmaGray
		<< " EnableColumnNoise=" << (m_stage6ColumnNoiseEnabled ? "1" : "0")
		<< " ColumnNoiseSigmaGray=" << m_stage6ColumnNoiseSigmaGray
		<< " EnableRowNoise=" << (m_stage6RowNoiseEnabled ? "1" : "0")
		<< " RowNoiseSigmaGray=" << m_stage6RowNoiseSigmaGray
		<< " EnableBadPixels=" << (m_stage6BadPixelsEnabled ? "1" : "0")
		<< " BadPixelRatio=" << m_stage6BadPixelRatio
		<< " EnableAGC=" << (m_stage6AgcEnabled ? "1" : "0")
		<< " AGCMode=" << m_stage6AgcMode
		<< " AGCApplyTo=" << m_stage6AgcApplyTo
		<< " AGCStatsSource=" << m_stage6AgcStatsSource
		<< " AGCUpdateHz=" << m_stage6AgcUpdateHz
		<< " AGCStride=" << m_stage6AgcStride
		<< " AGCGain=" << m_stage6AgcGain
		<< " AGCOffset=" << m_stage6AgcOffset
		<< " EnableIRPhysicalPipeline=" << (m_enableStage5PhysicalPipeline ? "1" : "0")
		<< " DebugView=" << m_stage5DebugViewModeName
		<< " LogComponents=" << (m_stage5LogComponents ? "1" : "0")
		<< " UseSensorInputForDisplay=" << (m_stage5UseSensorInputForDisplay ? "1" : "0")
		<< " SensorInputDisplayMode=" << m_stage5SensorInputDisplayMode
		<< " SensorInputDisplayScale=" << m_stage5SensorInputDisplayScale
		<< " SensorInputDisplayOffset=" << m_stage5SensorInputDisplayOffset
		<< " SensorInputDisplayClampMin=" << m_stage5SensorInputDisplayClampMin
		<< " SensorInputDisplayClampMax=" << m_stage5SensorInputDisplayClampMax
		<< " SensorInputDisplayGamma=" << m_stage5SensorInputDisplayGamma
		<< " SensorInputDisplayBand=" << m_stage5SensorInputDisplayBandName
		<< " EnableAeroThermalModel=" << (m_stage5AeroThermalEnabled ? "1" : "0")
		<< " ApplyAeroToRadiance=" << (m_stage5ApplyAeroToRadiance ? "1" : "0")
		<< " AeroDebugLog=" << (m_stage5AeroDebugLog ? "1" : "0")
		<< " AeroApplyScale=" << m_stage5AeroApplyScale
		<< " AeroApplyClampBodyDeltaK=" << m_stage5AeroApplyClampBodyDeltaK
		<< " AeroApplyOnlyBand=" << m_stage5AeroApplyOnlyBandName
		<< " RecoveryFactor=" << m_stage5AeroThermalOptions.recoveryFactor
		<< " ClampDeltaKMax=" << m_stage5AeroThermalOptions.clampDeltaKMax
		<< " EnableModtranRadianceDebug=" << (m_enableStage5ModtranRadianceDebug ? "1" : "0")
		<< " UseModtranPathRuntime=" << (m_stage5UseModtranPathRuntime ? "1" : "0")
		<< " UseModtranSkyRuntime=" << (m_stage5UseModtranSkyRuntime ? "1" : "0")
		<< " UseModtranSolarRuntime=" << (m_stage5UseModtranSolarRuntime ? "1" : "0")
		<< " CompareLegacy=" << (m_stage5ModtranCompareLegacy ? "1" : "0")
		<< " ModtranPathRuntimeBand=" << m_stage5ModtranPathRuntimeBandName
		<< " ModtranPathRuntimeMode=" << m_stage5ModtranPathRuntimeMode
		<< " ModtranPathUnitMode=" << m_stage5ModtranPathUnitMode
		<< " ModtranPathScale=" << m_stage5ModtranPathScale
		<< " ModtranPathBlend=" << m_stage5ModtranPathBlend
		<< " Stage5ModtranCompareEffective=" << (modtranCompareEffective ? "1" : "0")
		<< " Stage7UpdateHz=" << m_stage7UpdateHz
		<< " Stage4UpdateHz=" << m_stage4UpdateHz
		<< " Stage5PlumeUpdateHz=" << m_stage5PlumeUpdateHz
		<< " Codec=" << m_tcpCodecConfig
		<< " JpegEncodeMode=" << (m_tcpJpegGray ? "gray" : "rgb")
		<< " JpegQuality=" << m_tcpJpegQuality
		<< " h264EnFromInit=" << (m_h264EnFromInit ? "1" : "0")
		<< " EnableH264Experimental=" << (m_enableH264Experimental ? "1" : "0")
		<< " H264FallbackToJpeg=" << (m_h264FallbackToJpeg ? "1" : "0")
		<< " H264Encoder=" << m_h264Encoder
		<< " H264BitrateKbps=" << m_h264BitrateKbps
		<< " H264GopFrames=" << m_h264GopFrames
		<< " H264LowLatency=" << (m_h264LowLatency ? "1" : "0")
		<< " H264ForceKeyFrameOnStart=" << (m_h264ForceKeyFrameOnStart ? "1" : "0")
		<< " JpegPerfABTest=" << (m_jpegPerfABTest ? "1" : "0")
		<< " saveMP4En=" << (saveMP4En ? "1" : "0")
		<< " source=videoFps:" << (videoFpsSource ? videoFpsSource : "unknown")
		<< ",targetNumValid:" << (targetNumSource ? targetNumSource : "unknown")
		<< ",saveMP4En:" << (saveMP4Source ? saveMP4Source : "unknown")
		<< "," << m_effectiveRuntimeConfigSources
		<< std::endl;
	if (m_stage5DebugViewModeName != "Off")
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] DebugView=" << m_stage5DebugViewModeName
			<< " reason=production_default_should_be_Off" << std::endl;
	}
	if (m_stage5LogComponents)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] LogComponents=1 reason=production_default_should_be_false" << std::endl;
	}
	if (m_stage5UseSensorInputForDisplay)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] UseSensorInputForDisplay=1"
			<< " reason=stage5A_AB_or_debug_mode_not_production_default"
			<< std::endl;
	}
	if (m_enableIRVerboseLog)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] EnableIRVerboseLog=1 reason=60Hz_console_logging_risk" << std::endl;
	}
	if (m_stage6MtfBlurEnabled)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] EnableMTFBlur=1"
			<< " sigmaPixels=" << m_stage6MtfBlurSigmaPixels
			<< " radiusPixels=" << m_stage6MtfBlurRadiusPixels
			<< " reason=stage6a_AB_or_candidate_mode_not_production_default"
			<< std::endl;
	}
	if (m_stage6DetectorNoiseEnabled)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] EnableDetectorNoise=1"
			<< " position=" << m_stage6DetectorNoisePosition
			<< " temporalSigma=" << m_stage6TemporalNoiseSigmaGray
			<< " fpnSigma=" << m_stage6FpnSigmaGray
			<< " reason=stage6c_AB_or_candidate_mode_not_production_default"
			<< std::endl;
	}
	if (m_stage6AgcEnabled)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] EnableAGC=1"
			<< " mode=" << m_stage6AgcMode
			<< " statsSource=" << m_stage6AgcStatsSource
			<< " reason=stage6b_AB_or_candidate_mode_not_production_default"
			<< std::endl;
	}
	if (m_stage5ApplyAeroToRadiance)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] ApplyAeroToRadiance=1"
			<< " reason=stage4A_AB_or_debug_mode_not_production_default"
			<< std::endl;
	}
	if (m_stage5AeroDebugLog)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] AeroDebugLog=1"
			<< " reason=stage4A_debug_log_enabled"
			<< std::endl;
	}
	if (m_stage5UseModtranPathRuntime || m_stage5UseModtranSkyRuntime || m_stage5UseModtranSolarRuntime)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN]"
			<< " UseModtranPathRuntime=" << (m_stage5UseModtranPathRuntime ? "1" : "0")
			<< " UseModtranSkyRuntime=" << (m_stage5UseModtranSkyRuntime ? "1" : "0")
			<< " UseModtranSolarRuntime=" << (m_stage5UseModtranSolarRuntime ? "1" : "0")
			<< " reason=stage3b_fix_production_must_not_apply_modtran_path_sky_solar"
			<< std::endl;
	}
	if (m_stage5ModtranPathRuntimeMode != "Off")
	{
		std::cout << "[EffectiveRuntimeConfig][WARN]"
			<< " ModtranPathRuntimeMode=" << m_stage5ModtranPathRuntimeMode
			<< " ModtranPathScale=" << m_stage5ModtranPathScale
			<< " ModtranPathBlend=" << m_stage5ModtranPathBlend
			<< " reason=stage3C_AB_or_debug_mode_not_production_default"
			<< std::endl;
	}
	if (m_jpegPerfABTest)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN] JpegPerfABTest=1 reason=production_default_should_be_false" << std::endl;
	}
	if (m_enableH264Experimental)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN]"
			<< " EnableH264Experimental=1"
			<< " H264Encoder=" << m_h264Encoder
			<< " reason=stage7a_experimental_transport_not_production_default"
			<< std::endl;
	}
	if (modtranCompareEffective && !m_stage5UseModtranPathRuntime)
	{
		std::cout << "[EffectiveRuntimeConfig][WARN]"
			<< " Stage5ModtranCompareEffective=1"
			<< " reason=debug_or_compare_query_enabled; production_default_should_disable_lookup"
			<< std::endl;
	}
}

void HwaSimIR::LogStage5ModtranRadianceCompare(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRModtranRadianceResult& modtranResult, double rangeKm, double observerAltKm, double targetAltKm)
{
	if (!Stage5ModtranRadianceCompareEnabled())
	{
		return;
	}
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const bool sampleDue = frameSeq <= 3 ||
		(m_stage5ModtranLogEveryFrames > 0 && (frameSeq % static_cast<std::uint64_t>(m_stage5ModtranLogEveryFrames)) == 0);
	if (!sampleDue && !m_enableIRVerboseLog)
	{
		return;
	}
	const double visibilityKm = m_irRadianceModel.environment().visibilityMeters / 1000.0;
	const double legacyPath = components.legacyPathRadiance;
	const double modtranPath = components.modtranPathRadiance;
	const double ratio = std::abs(legacyPath) > 1.0e-12 ? modtranPath / legacyPath : 0.0;
	const double diff = modtranPath - legacyPath;
	std::ostringstream state;
	state << static_cast<int>(components.band)
		<< ":" << (modtranResult.valid ? 1 : 0)
		<< ":" << modtranResult.fallbackReason
		<< ":" << Stage5ModtranCacheDouble(rangeKm)
		<< ":" << Stage5ModtranCacheDouble(observerAltKm)
		<< ":" << Stage5ModtranCacheDouble(targetAltKm)
		<< ":" << Stage5ModtranCacheDouble(visibilityKm);
	const std::string logKey =
		std::to_string(targetPlat.targetState.targetType) + ":" +
		std::to_string(targetPlat.targetState.targetPlatID) + ":" +
		std::to_string(targetPlat.targetState.targetID) + "#modtran-radiance";
	const bool stateChanged = m_lastStage5ModtranRadianceCompareLogState[logKey] != state.str();
	m_lastStage5ModtranRadianceCompareLogState[logKey] = state.str();
	const bool logDue = sampleDue || stateChanged;
	if (!logDue && !m_enableIRVerboseLog)
	{
		return;
	}
	std::cout << "[Stage5 ModtranRadianceCompare]"
		<< " sourceSeq=" << frameSeq
		<< " targetID=" << targetPlat.targetState.targetID
		<< " band=" << IRBandName(components.band)
		<< " rangeKm=" << rangeKm
		<< " observerAltKm=" << observerAltKm
		<< " targetAltKm=" << targetAltKm
		<< " visibilityKm=" << visibilityKm
		<< " legacyPath=" << legacyPath
		<< " modtranPath=" << modtranPath
		<< " modtranPathRaw=" << components.modtranPathRaw
		<< " modtranPathScaled=" << components.modtranPathScaled
		<< " modtranSky=" << components.modtranSkyRadiance
		<< " modtranSolar=" << components.modtranSolarIrradiance
		<< " tauUp=" << components.tauUp
		<< " valid=" << (modtranResult.valid ? "1" : "0")
		<< " fallbackReason=" << components.modtranFallbackReason
		<< " interpolationMode=" << components.modtranInterpolationMode
		<< " pathRadianceSource=" << components.pathRadianceSource
		<< " ratio=" << ratio
		<< " diff=" << diff
		<< " unitRadiance=" << modtranResult.unitRadiance
		<< " unitIrradiance=" << modtranResult.unitIrradiance
		<< " sourceFile=" << modtranResult.sourceFile
		<< std::endl;
	if (Stage5ModtranPathRuntimeAffectsImage() &&
		components.pathRadianceSource != "modtran_runtime_scaled" &&
		components.pathRadianceSource != "modtran_runtime_blend")
	{
		std::cout << "[Stage5 ModtranRadianceCompare][WARN]"
			<< " requestedRuntimePath=1"
			<< " activePathRadianceSource=" << components.pathRadianceSource
			<< " fallbackReason=" << components.modtranFallbackReason
			<< std::endl;
	}
}

void HwaSimIR::LogStage5ModtranPathAB(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRModtranRadianceResult& modtranResult, double rangeKm, double observerAltKm, double targetAltKm)
{
	if (!m_stage5ModtranPathABLog || !Stage5ModtranRadianceCompareEnabled())
	{
		return;
	}
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const bool sampleDue = frameSeq <= 3 ||
		(m_stage5ModtranLogEveryFrames > 0 && (frameSeq % static_cast<std::uint64_t>(m_stage5ModtranLogEveryFrames)) == 0);
	if (!sampleDue && !m_enableIRVerboseLog)
	{
		return;
	}
	const double visibilityKm = m_irRadianceModel.environment().visibilityMeters / 1000.0;
	const double surfaceRadiance =
		components.bodyRadiance +
		components.reflectedRadiance +
		components.rearHotspotRadiance +
		components.plumeRadiance +
		components.brightspotRadiance;
	std::ostringstream state;
	state << static_cast<int>(components.band)
		<< ":" << components.pathRadianceSource
		<< ":" << components.modtranPathRuntimeMode
		<< ":" << (modtranResult.valid ? 1 : 0)
		<< ":" << components.modtranFallbackReason
		<< ":" << Stage5ModtranCacheDouble(rangeKm)
		<< ":" << Stage5ModtranCacheDouble(observerAltKm)
		<< ":" << Stage5ModtranCacheDouble(targetAltKm)
		<< ":" << Stage5ModtranCacheDouble(visibilityKm)
		<< ":" << Stage5ModtranCacheDouble(components.modtranPathScale)
		<< ":" << Stage5ModtranCacheDouble(components.modtranPathBlend);
	const std::string logKey =
		std::to_string(targetPlat.targetState.targetType) + ":" +
		std::to_string(targetPlat.targetState.targetPlatID) + ":" +
		std::to_string(targetPlat.targetState.targetID) + "#modtran-path-ab";
	const bool stateChanged = m_lastStage5ModtranPathABLogState[logKey] != state.str();
	m_lastStage5ModtranPathABLogState[logKey] = state.str();
	const bool logDue = sampleDue || stateChanged;
	if (!logDue && !m_enableIRVerboseLog)
	{
		return;
	}
	std::cout << "[Stage5 ModtranPathAB]"
		<< " sourceSeq=" << frameSeq
		<< " targetID=" << targetPlat.targetState.targetID
		<< " band=" << IRBandName(components.band)
		<< " rangeKm=" << rangeKm
		<< " obsAltKm=" << observerAltKm
		<< " tgtAltKm=" << targetAltKm
		<< " visibilityKm=" << visibilityKm
		<< " tauUp=" << components.tauUp
		<< " surfaceRadiance=" << surfaceRadiance
		<< " legacyPath=" << components.legacyPathRadiance
		<< " modtranPathRaw=" << components.modtranPathRaw
		<< " modtranPathScaled=" << components.modtranPathScaled
		<< " effectivePathRadiance=" << components.effectivePathRadiance
		<< " sensorInputLegacy=" << components.sensorInputLegacy
		<< " sensorInputModtran=" << components.sensorInputModtran
		<< " sensorInputEffective=" << components.sensorInputRadiance
		<< " pathRadianceSource=" << components.pathRadianceSource
		<< " runtimeMode=" << components.modtranPathRuntimeMode
		<< " unitMode=" << components.modtranPathUnitMode
		<< " pathScale=" << components.modtranPathScale
		<< " pathBlend=" << components.modtranPathBlend
		<< " valid=" << (modtranResult.valid ? "1" : "0")
		<< " fallbackReason=" << components.modtranFallbackReason
		<< std::endl;
}

IRAeroThermalOutput HwaSimIR::EvaluateStage5AeroThermal(TargetPlatformData& targetPlat, IRBand band, float dtSec, const IRRuntimeEnvironment& environment, const std::string& targetKey)
{
	(void)environment;
	const auto aeroStart = std::chrono::steady_clock::now();
	IRAeroThermalInput input;
	input.altitudeM = NormalizeAeroAltitudeMeters(targetPlat.targetState.targetLoc.alt);
	input.speedRaw = std::isfinite(targetPlat.targetState.targetLoc.speed)
		? targetPlat.targetState.targetLoc.speed
		: 0.0;
	input.speedSource = Stage5AeroSpeedSourceForTarget(targetPlat.targetState);
	input.dtSec = static_cast<double>(dtSec);
	input.band = band;
	input.targetType = targetPlat.targetState.targetType;
	input.platformType = static_cast<int>(targetPlat.type);
	// 4A uses an ISA atmosphere from target altitude. Runtime weather temperature remains a future override candidate.
	input.envTemperatureK = 0.0;
	IRAeroThermalOutput output = m_stage5AeroThermalModel.evaluate(
		input,
		m_stage5AeroThermalOptions,
		&m_stage5AeroThermalStateByTarget[targetKey]);
	m_stage5AeroThermalMsCurrent += std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - aeroStart).count();
	return output;
}

void HwaSimIR::LogAeroSpeedState(const TargetPlatformData& targetPlat, bool renderVisible)
{
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const double selectedSpeedKmh = std::isfinite(targetPlat.targetState.targetLoc.speed)
		? targetPlat.targetState.targetLoc.speed
		: 0.0;
	const std::string selectedSpeedSource = Stage5AeroSpeedSourceForTarget(targetPlat.targetState);
	const std::string logKey =
		std::to_string(targetPlat.targetState.targetType) + ":" +
		std::to_string(targetPlat.targetState.targetPlatID) + ":" +
		std::to_string(targetPlat.targetState.targetID) + "#aero-speed-state";
	std::ostringstream state;
	state << Stage5ModtranCacheDouble(QuantizeForCache(selectedSpeedKmh, 25.0))
		<< ":" << Stage5ModtranCacheDouble(QuantizeForCache(targetPlat.targetState.targetLoc.alt, 500.0))
		<< ":" << selectedSpeedSource
		<< ":" << (renderVisible ? 1 : 0);
	const bool stateChanged = m_lastAeroSpeedStateLogState[logKey] != state.str();
	m_lastAeroSpeedStateLogState[logKey] = state.str();
	const bool sampleDue = frameSeq <= 3 || (frameSeq % 600) == 0;
	if (!sampleDue && !stateChanged && !m_enableIRVerboseLog)
	{
		return;
	}
	std::cout << "[AeroSpeedState]"
		<< " sourceSeq=" << frameSeq
		<< " targetID=" << targetPlat.targetState.targetID
		<< " targetType=" << TargetTypeHexString(targetPlat.targetState.targetType)
		<< " platform=" << Stage4PlatformName(targetPlat.type)
		<< " renderVisible=" << (renderVisible ? "1" : "0")
		<< " targetState.targetLoc.speed=" << targetPlat.targetState.targetLoc.speed
		<< " targetState.targetLoc.alt=" << targetPlat.targetState.targetLoc.alt
		<< " selectedSpeedKmh=" << selectedSpeedKmh
		<< " selectedSpeedSource=" << selectedSpeedSource
		<< " speedUnit=km/h"
		<< std::endl;
}

void HwaSimIR::LogStage5AeroThermal(const TargetPlatformData& targetPlat, const IRRadianceComponents& components, const IRAeroThermalOutput& aeroOutput)
{
	if (!m_stage5AeroDebugLog && !m_stage5LogComponents && !m_enableIRVerboseLog)
	{
		return;
	}
	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const bool sampleDue = frameSeq <= 3 ||
		(m_stage5AeroLogEveryFrames > 0 && (frameSeq % static_cast<std::uint64_t>(m_stage5AeroLogEveryFrames)) == 0);
	const std::string logKey =
		std::to_string(targetPlat.targetState.targetType) + ":" +
		std::to_string(targetPlat.targetState.targetPlatID) + ":" +
		std::to_string(targetPlat.targetState.targetID) + "#aero-thermal";
	std::ostringstream state;
	state << Stage5ModtranCacheDouble(QuantizeForCache(aeroOutput.altitudeM, 500.0))
		<< ":" << Stage5ModtranCacheDouble(QuantizeForCache(aeroOutput.speedRawKmh, 25.0))
		<< ":" << Stage5ModtranCacheDouble(QuantizeForCache(aeroOutput.mach, 0.05))
		<< ":" << Stage5ModtranCacheDouble(QuantizeForCache(aeroOutput.bodyAeroDeltaK, 0.25))
		<< ":" << Stage5ModtranCacheDouble(QuantizeForCache(components.bodyAeroDeltaKEffective, 0.25))
		<< ":" << (components.aeroAppliedToRadiance ? 1 : 0)
		<< ":" << (aeroOutput.valid ? 1 : 0)
		<< ":" << aeroOutput.selectedSpeedSource
		<< ":" << aeroOutput.fallbackReason;
	const bool stateChanged = m_lastStage5AeroThermalLogState[logKey] != state.str();
	m_lastStage5AeroThermalLogState[logKey] = state.str();
	if (!sampleDue && !stateChanged && !m_enableIRVerboseLog)
	{
		return;
	}
	if (aeroOutput.speedRawKmh > 0.0 && aeroOutput.mach <= 1.0e-9)
	{
		std::cout << "[Stage5 AeroThermal][WARN]"
			<< " sourceSeq=" << frameSeq
			<< " targetID=" << targetPlat.targetState.targetID
			<< " speedRawKmh=" << aeroOutput.speedRawKmh
			<< " mach=" << aeroOutput.mach
			<< " reason=positive_speed_but_zero_mach"
			<< std::endl;
	}
	if (m_stage5ApplyAeroToRadiance && aeroOutput.speedRawKmh <= 0.0)
	{
		std::cout << "[Stage5 AeroThermal][WARN]"
			<< " sourceSeq=" << frameSeq
			<< " targetID=" << targetPlat.targetState.targetID
			<< " ApplyAeroToRadiance=1"
			<< " speedRawKmh=" << aeroOutput.speedRawKmh
			<< " reason=no_effective_speed_runtime_ab_has_no_visual_meaning"
			<< std::endl;
	}
	std::cout << "[Stage5 AeroThermal]"
		<< " sourceSeq=" << frameSeq
		<< " targetID=" << targetPlat.targetState.targetID
		<< " targetType=" << TargetTypeHexString(targetPlat.targetState.targetType)
		<< " selectedSpeedSource=" << aeroOutput.selectedSpeedSource
		<< " altitudeM=" << aeroOutput.altitudeM
		<< " speedRaw=" << aeroOutput.speedRaw
		<< " speedRawKmh=" << aeroOutput.speedRawKmh
		<< " speedUnit=" << aeroOutput.speedUnit
		<< " speedMps=" << aeroOutput.speedMps
		<< " airTempK=" << aeroOutput.airTempK
		<< " speedOfSoundMps=" << aeroOutput.speedOfSoundMps
		<< " mach=" << aeroOutput.mach
		<< " recoveryTempK=" << aeroOutput.recoveryTempK
		<< " aeroDeltaK=" << aeroOutput.aeroDeltaK
		<< " bodyAeroDeltaK=" << aeroOutput.bodyAeroDeltaK
		<< " bodyAeroDeltaKRaw=" << components.bodyAeroDeltaKRaw
		<< " bodyAeroDeltaKEffective=" << components.bodyAeroDeltaKEffective
		<< " bodyRadianceNoAero=" << components.bodyRadianceNoAero
		<< " bodyRadianceWithAero=" << components.bodyRadianceWithAero
		<< " surfaceRadianceNoAero=" << components.surfaceRadianceNoAero
		<< " surfaceRadianceWithAero=" << components.surfaceRadianceWithAero
		<< " tauUp=" << components.tauUp
		<< " tauUpSource=" << components.tauUpSource
		<< " tauUpValid=" << (components.tauUpValid ? "1" : "0")
		<< " tauFallbackReason=" << components.tauFallbackReason
		<< " pathRadiance=" << components.pathRadiance
		<< " sensorInputNoAero=" << components.sensorInputNoAero
		<< " sensorInputWithAero=" << components.sensorInputWithAero
		<< " sensorInputRadiance=" << components.sensorInputRadiance
		<< " sensorInputRatio=" << (components.sensorInputNoAero > 1.0e-12
			? components.sensorInputWithAero / components.sensorInputNoAero
			: 1.0)
		<< " aeroRadianceRatio=" << components.aeroRadianceRatio
		<< " noseAeroDeltaK=" << aeroOutput.noseAeroDeltaK
		<< " edgeAeroDeltaK=" << aeroOutput.edgeAeroDeltaK
		<< " rearAeroDeltaK=" << aeroOutput.rearAeroDeltaK
		<< " aeroAppliedToRadiance=" << (components.aeroAppliedToRadiance ? "1" : "0")
		<< " valid=" << (aeroOutput.valid ? "1" : "0")
		<< " fallbackReason=" << aeroOutput.fallbackReason
		<< std::endl;
}

void HwaSimIR::ApplyStage5RadianceDebug(TargetPlatformData& targetPlat, const IRObjectRadianceOutput& radiance, const IRHotspotState& rearHotspot, const IRBrightSpotState& brightSpot, bool rearEnabledForShader, float rearIntensityForShader, const std::string& targetKey, float dtSec)
{
	if (targetPlat.nodePath.is_empty())
	{
		return;
	}

	const auto stage5ComponentStart = std::chrono::steady_clock::now();
	const IRBand stage5Band = static_cast<IRBand>(static_cast<int>(radiance.bandIndex));
	const int stage5BandIndex = Stage5BandIndex(stage5Band);
	const bool stage5UseBaseTextureModulation = m_stage5UseBaseTextureModulationByBand[stage5BandIndex];
	const bool sensorInputDisplayBandAllowed = stage5Band == m_stage5SensorInputDisplayBand;
	const bool useSensorInputForDisplayEffective =
		m_stage5UseSensorInputForDisplay && sensorInputDisplayBandAllowed;
	const bool stage5DisplayUsesSensorInput = useSensorInputForDisplayEffective || m_enableStage5RadianceDebug;
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_radiance_debug_en", LVecBase2i(stage5DisplayUsesSensorInput ? 1 : 0, 0));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_debug_view_mode", LVecBase2i(m_stage5DebugViewMode, 0));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_use_sensor_input_for_display", LVecBase2i(useSensorInputForDisplayEffective ? 1 : 0, 0));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_use_base_texture_modulation", LVecBase2i(stage5UseBaseTextureModulation ? 1 : 0, 0));
	if (!m_enableStage5PhysicalPipeline)
	{
		m_stage5RadianceComponentMsCurrent += std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - stage5ComponentStart).count();
		return;
	}

	IRRadianceModelV2Input stage5Input;
	stage5Input.band = stage5Band;
	stage5Input.materialName = MaterialNameForPlatform(targetPlat.type);
	stage5Input.materialTemperatureK = radiance.temperatureK;
	stage5Input.materialEmissivity = radiance.emissivity;
	stage5Input.materialReflectance = radiance.reflectance;
	const double rawTauUp = static_cast<double>(radiance.tauUp);
	double selectedTauUp = rawTauUp;
	std::string selectedTauSource = m_irAtmosphereModel.useModtranTauForAtmosphere()
		? "modtran_tau_or_legacy_atmosphere"
		: "legacy_atmosphere";
	bool selectedTauValid = true;
	std::string tauFallbackReason = "none";
	if (!std::isfinite(rawTauUp))
	{
		selectedTauUp = 1.0;
		selectedTauSource = "fallback_unity";
		selectedTauValid = true;
		tauFallbackReason = "radiance_tau_nonfinite_fallback_unity";
	}
	else if (rawTauUp <= 1.0e-6)
	{
		selectedTauUp = 1.0;
		selectedTauSource = "fallback_unity";
		selectedTauValid = true;
		tauFallbackReason = "radiance_tau_near_zero_fallback_unity";
	}
	else if (rawTauUp > 1.0)
	{
		selectedTauUp = 1.0;
		selectedTauSource += "_clamped";
		selectedTauValid = true;
		tauFallbackReason = "radiance_tau_out_of_range_clamped";
	}
	stage5Input.tauUp = selectedTauUp;
	stage5Input.tauUpSource = selectedTauSource;
	stage5Input.tauUpValid = selectedTauValid;
	stage5Input.tauFallbackReason = tauFallbackReason;
	const IRRuntimeEnvironment environment = m_irRadianceModel.environment();
	stage5Input.solarStrength = environment.sunStrength;
	stage5Input.ndotl = 1.0;
	stage5Input.textureLuma = 1.0;
	stage5Input.solarReflectanceWeight = m_stage5DebugConfigs[stage5BandIndex].solarReflectanceWeight;
	stage5Input.hotspotTemperatureK = rearEnabledForShader ? rearHotspot.currentTempK : radiance.temperatureK;
	stage5Input.hotspotIntensity = rearEnabledForShader ? std::max(0.0f, rearIntensityForShader) : 0.0f;
	stage5Input.brightspotIntensity = brightSpot.enabled ? std::max(0.0f, brightSpot.intensity) : 0.0f;
	const IRAeroThermalOutput aeroOutput = EvaluateStage5AeroThermal(targetPlat, stage5Band, dtSec, environment, targetKey);
	stage5Input.altitudeM = aeroOutput.altitudeM;
	stage5Input.speedRawKmh = aeroOutput.speedRawKmh;
	stage5Input.speedSource = aeroOutput.selectedSpeedSource;
	stage5Input.speedMps = aeroOutput.speedMps;
	stage5Input.mach = aeroOutput.mach;
	stage5Input.airTempK = aeroOutput.airTempK;
	stage5Input.recoveryTempK = aeroOutput.recoveryTempK;
	stage5Input.aeroDeltaK = aeroOutput.aeroDeltaK;
	const bool aeroApplyBandAllowed = stage5Band == m_stage5AeroApplyOnlyBand;
	const double bodyAeroDeltaKRaw = std::max(0.0, aeroOutput.bodyAeroDeltaK);
	const double scaledBodyAeroDeltaK = QuantizeForCache(bodyAeroDeltaKRaw * m_stage5AeroApplyScale, 0.05);
	const double bodyAeroDeltaKEffective =
		(m_stage5ApplyAeroToRadiance && aeroOutput.valid && aeroApplyBandAllowed)
		? ClampStage5Double(scaledBodyAeroDeltaK, 0.0, m_stage5AeroApplyClampBodyDeltaK)
		: 0.0;
	stage5Input.bodyAeroDeltaK = bodyAeroDeltaKEffective;
	stage5Input.bodyAeroDeltaKRaw = bodyAeroDeltaKRaw;
	stage5Input.bodyAeroDeltaKEffective = bodyAeroDeltaKEffective;
	stage5Input.noseAeroDeltaK = aeroOutput.noseAeroDeltaK;
	stage5Input.edgeAeroDeltaK = aeroOutput.edgeAeroDeltaK;
	stage5Input.rearAeroDeltaK = aeroOutput.rearAeroDeltaK;
	stage5Input.aeroValid = aeroOutput.valid;
	stage5Input.aeroFallbackReason = aeroOutput.fallbackReason;
	stage5Input.aeroAppliedToRadiance = m_stage5ApplyAeroToRadiance && aeroOutput.valid && aeroApplyBandAllowed;
	double plumeRadiance = 0.0;
	const std::map<std::string, Stage5PlumeRuntimeCache>::const_iterator plumeIt = m_stage5PlumeRuntimeCache.find(targetKey);
	if (plumeIt != m_stage5PlumeRuntimeCache.end() && plumeIt->second.hasOutput)
	{
		const IREnginePlumeOutput& plume = plumeIt->second.output;
		plumeRadiance = std::max(
			static_cast<double>(plume.coreGray) * static_cast<double>(plume.coreOpacity),
			static_cast<double>(plume.haloGray) * static_cast<double>(plume.haloOpacity));
	}
	stage5Input.plumeRadiance = plumeRadiance;
	const double legacyPathRadiance = std::max(0.0f, radiance.pathRadiance);
	const bool modtranCompareEnabled = Stage5ModtranRadianceCompareEnabled();
	IRModtranRadianceResult modtranRadiance;
	if (modtranCompareEnabled)
	{
		modtranRadiance = QueryStage5ModtranRadiance(targetPlat, environment, radiance, targetKey);
	}
	else
	{
		modtranRadiance.valid = false;
		modtranRadiance.fallbackReason = "disabled";
		modtranRadiance.interpolationMode = "disabled";
		modtranRadiance.sourceFile = "disabled";
	}
	stage5Input.legacyPathRadiance = legacyPathRadiance;
	const double modtranPathRaw = modtranRadiance.valid ? std::max(0.0, modtranRadiance.pathRadiance) : 0.0;
	const double modtranPathScaled = ClampStage5Double(
		modtranPathRaw * m_stage5ModtranPathScale + m_stage5ModtranPathOffset,
		m_stage5ModtranPathClampMin,
		m_stage5ModtranPathClampMax);
	stage5Input.modtranPathRadiance = modtranPathRaw;
	stage5Input.modtranPathRaw = modtranPathRaw;
	stage5Input.modtranPathScaled = modtranPathScaled;
	stage5Input.modtranPathUnitMode = m_stage5ModtranPathUnitMode;
	stage5Input.modtranPathScale = m_stage5ModtranPathScale;
	stage5Input.modtranPathOffset = m_stage5ModtranPathOffset;
	stage5Input.modtranPathBlend = m_stage5ModtranPathBlend;
	stage5Input.modtranPathRuntimeMode = m_stage5ModtranPathRuntimeMode;
	stage5Input.modtranSkyRadiance = modtranRadiance.valid ? modtranRadiance.skyRadiance : 0.0;
	stage5Input.modtranSolarIrradiance = modtranRadiance.valid ? modtranRadiance.solarIrradiance : 0.0;
	stage5Input.modtranRadianceValid = modtranRadiance.valid;
	stage5Input.modtranInterpolationMode = modtranRadiance.interpolationMode;
	stage5Input.modtranFallbackReason = modtranRadiance.fallbackReason;
	const bool modeReplace = m_stage5ModtranPathRuntimeMode == "ReplaceLegacy";
	const bool modeBlend = m_stage5ModtranPathRuntimeMode == "BlendLegacy";
	const bool runtimeAffectsImage = Stage5ModtranPathRuntimeAffectsImage();
	const bool runtimeBandAllowed =
		stage5Band == IRBand::MidWaveInfrared &&
		m_stage5ModtranPathRuntimeBand == IRBand::MidWaveInfrared;
	const bool modtranPathRuntimeAllowed =
		runtimeAffectsImage &&
		runtimeBandAllowed &&
		modtranRadiance.valid;
	if (runtimeAffectsImage && !runtimeBandAllowed)
	{
		stage5Input.modtranFallbackReason = "runtime_band_not_supported";
		const int bandWarnKey = static_cast<int>(stage5Band) * 16 + static_cast<int>(m_stage5ModtranPathRuntimeBand);
		if (!m_stage5ModtranPathRuntimeBandWarned[bandWarnKey])
		{
			m_stage5ModtranPathRuntimeBandWarned[bandWarnKey] = true;
			std::cout << "[Stage5 ModtranPathRuntime][WARN]"
				<< " requestedBand=" << m_stage5ModtranPathRuntimeBandName
				<< " activeBand=" << IRBandName(stage5Band)
				<< " runtimeMode=" << m_stage5ModtranPathRuntimeMode
				<< " fallback=legacy"
				<< " reason=only_MWIR_path_runtime_allowed_in_stage3C"
				<< std::endl;
		}
	}
	else if (runtimeAffectsImage && !modtranRadiance.valid)
	{
		stage5Input.modtranFallbackReason = modtranRadiance.fallbackReason.empty()
			? "modtran_invalid"
			: modtranRadiance.fallbackReason;
	}
	stage5Input.pathRadiance = legacyPathRadiance;
	stage5Input.pathRadianceSource = legacyPathRadiance > 0.0 ? "legacy_empirical" : "disabled";
	if (modtranPathRuntimeAllowed && modeReplace)
	{
		stage5Input.pathRadiance = modtranPathScaled;
		stage5Input.pathRadianceSource = "modtran_runtime_scaled";
	}
	else if (modtranPathRuntimeAllowed && modeBlend)
	{
		stage5Input.pathRadiance =
			legacyPathRadiance * (1.0 - m_stage5ModtranPathBlend) +
			modtranPathScaled * m_stage5ModtranPathBlend;
		stage5Input.pathRadianceSource = "modtran_runtime_blend";
	}
	if (runtimeAffectsImage && !modtranPathRuntimeAllowed)
	{
		stage5Input.pathRadianceSource = legacyPathRadiance > 0.0 ? "fallback_legacy" : "disabled";
	}
	stage5Input.effectivePathRadiance = stage5Input.pathRadiance;
	stage5Input.sourceFlags = "body+reflected";
	if (stage5Input.hotspotIntensity > 0.0)
	{
		stage5Input.sourceFlags += "+rearHotspot";
	}
	if (stage5Input.plumeRadiance > 0.0)
	{
		stage5Input.sourceFlags += "+plume";
	}
	if (stage5Input.brightspotIntensity > 0.0)
	{
		stage5Input.sourceFlags += "+brightspot";
	}
	if (stage5Input.aeroAppliedToRadiance)
	{
		stage5Input.sourceFlags += "+bodyAero";
	}
	if (stage5Input.pathRadiance > 0.0)
	{
		if (stage5Input.pathRadianceSource == "modtran_runtime_scaled")
		{
			stage5Input.sourceFlags += "+modtranPathScaled";
		}
		else if (stage5Input.pathRadianceSource == "modtran_runtime_blend")
		{
			stage5Input.sourceFlags += "+modtranPathBlend";
		}
		else
		{
			stage5Input.sourceFlags += "+legacyPath";
		}
	}
	if (modtranCompareEnabled)
	{
		stage5Input.sourceFlags += "+modtranCompare";
	}
	stage5Input.enableDebugFloor = true;
	stage5Input.debugConfig = m_stage5DebugConfigs[stage5BandIndex];

	IRRadianceComponents components = m_irRadianceModelV2.evaluateComponents(stage5Input);
	components.sensorInputToDisplayEnabled = useSensorInputForDisplayEffective;
	IRRadianceModelV2Output stage5 = m_irRadianceModelV2.evaluate(stage5Input);
	double observerAltKmForLog = 10.0;
	if (m_stage0DisplayFrameCount > 0 && IsReasonableAltitudeMeters(m_realTimeSceneData.platLoc.alt))
	{
		observerAltKmForLog = m_realTimeSceneData.platLoc.alt / 1000.0;
	}
	else if (m_isAddPlatform && IsReasonableAltitudeMeters(m_initSceneData.platParam[0].spatial.alt))
	{
		observerAltKmForLog = m_initSceneData.platParam[0].spatial.alt / 1000.0;
	}
	const double targetAltKmForLog = IsReasonableAltitudeMeters(targetPlat.targetState.targetLoc.alt)
		? targetPlat.targetState.targetLoc.alt / 1000.0
		: observerAltKmForLog;
	const double rangeKmForLog = std::max(0.001, static_cast<double>(EstimateRangeToCamera(targetPlat.nodePath)) / 1000.0);
	const IRRadianceModelV2DebugConfig& displayConfig = m_stage5DebugConfigs[stage5BandIndex];
	const auto clamp01 = [](double value) -> double {
		return std::max(0.0, std::min(1.0, value));
	};
	double compositeMinGray = clamp01(displayConfig.compositeMinGray);
	double compositeMaxGray = clamp01(displayConfig.compositeMaxGray);
	if (compositeMaxGray < compositeMinGray)
	{
		compositeMaxGray = compositeMinGray;
	}
	const double bodyGrayDisplay = clamp01(stage5.bodyGrayAfterFloor * std::max(0.0, displayConfig.bodyDisplayGain));
	const double reflectedGrayDisplay = clamp01(stage5.reflectedGray * std::max(0.0, displayConfig.reflectedDisplayGain));
	const double hotspotGrayRawDisplay = clamp01(stage5.hotspotGray * std::max(0.0, displayConfig.hotspotDisplayGain));
	const double brightspotGrayRawDisplay = clamp01(stage5.brightspotGray * std::max(0.0, displayConfig.brightspotDisplayGain));
	const double plumeGrayDisplay = clamp01(components.plumeRadiance * std::max(0.0, displayConfig.hotspotDisplayGain));
	const double atmosphereGrayDisplay = clamp01(components.pathRadiance * std::max(0.0, displayConfig.bodyDisplayGain));
	const double sensorInputDisplayGray = MapSensorInputToDisplayGray(components.sensorInputRadiance);
	const double sensorInputRatio = components.sensorInputNoAero > 1.0e-12
		? components.sensorInputWithAero / components.sensorInputNoAero
		: 1.0;
	const double legacyRearHotspotDisplay = rearEnabledForShader
		? clamp01(std::max(0.0f, rearIntensityForShader) * std::max(0.0, displayConfig.hotspotDisplayGain))
		: 0.0;
	const double legacyBrightspotDisplay = brightSpot.enabled
		? clamp01(std::max(0.0f, brightSpot.intensity) * std::max(0.0, displayConfig.brightspotDisplayGain))
		: 0.0;
	const double hotspotGrayDisplay = std::max(hotspotGrayRawDisplay, legacyRearHotspotDisplay);
	const double brightspotGrayDisplay = std::max(brightspotGrayRawDisplay, legacyBrightspotDisplay);
	const bool stage5DisplayFallbackApplied =
		(hotspotGrayDisplay > hotspotGrayRawDisplay + 1.0e-6) ||
		(brightspotGrayDisplay > brightspotGrayRawDisplay + 1.0e-6);
	const double compositeGrayDisplay = clamp01(
		std::max(compositeMinGray, std::min(compositeMaxGray,
			bodyGrayDisplay + reflectedGrayDisplay + hotspotGrayDisplay + plumeGrayDisplay + brightspotGrayDisplay + atmosphereGrayDisplay)));
	const double finalGrayDebugDisplay = useSensorInputForDisplayEffective
		? sensorInputDisplayGray
		: compositeGrayDisplay;

	SetShaderInputCached(targetPlat.nodePath, "u_material_temp_K", LVecBase2f(static_cast<float>(components.materialTempK), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_material_emissivity", LVecBase2f(static_cast<float>(stage5Input.materialEmissivity), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_body_radiance", LVecBase2f(static_cast<float>(components.bodyRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_rear_hotspot_radiance", LVecBase2f(static_cast<float>(components.rearHotspotRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_plume_radiance", LVecBase2f(static_cast<float>(components.plumeRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_brightspot_radiance", LVecBase2f(static_cast<float>(components.brightspotRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_path_radiance", LVecBase2f(static_cast<float>(components.pathRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_sensor_input_radiance", LVecBase2f(static_cast<float>(components.sensorInputRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_sensor_input_display_gray", LVecBase2f(static_cast<float>(sensorInputDisplayGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_body_radiance_scale", LVecBase2f(static_cast<float>(stage5.bodyGrayBeforeFloor), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_body_gray", LVecBase2f(static_cast<float>(stage5.bodyGrayAfterFloor), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_reflected_radiance", LVecBase2f(static_cast<float>(stage5.reflectedRadiance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_reflected_gray", LVecBase2f(static_cast<float>(stage5.reflectedGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_reflectance_band", LVecBase2f(static_cast<float>(stage5Input.materialReflectance), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_solar_weight", LVecBase2f(static_cast<float>(stage5Input.solarReflectanceWeight), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_sun_dir_local", Stage5SunDirectionLocal(environment.sunAzimuthDeg, environment.sunElevationDeg));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_hotspot_gray", LVecBase2f(static_cast<float>(stage5.hotspotGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_brightspot_gray", LVecBase2f(static_cast<float>(stage5.brightspotGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_final_gray_debug", LVecBase2f(static_cast<float>(finalGrayDebugDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_body_display_gray", LVecBase2f(static_cast<float>(bodyGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_reflected_display_gray", LVecBase2f(static_cast<float>(reflectedGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_hotspot_display_gray", LVecBase2f(static_cast<float>(hotspotGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_brightspot_display_gray", LVecBase2f(static_cast<float>(brightspotGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_plume_display_gray", LVecBase2f(static_cast<float>(plumeGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_atmosphere_display_gray", LVecBase2f(static_cast<float>(atmosphereGrayDisplay), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_composite_min_gray", LVecBase2f(static_cast<float>(compositeMinGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_composite_max_gray", LVecBase2f(static_cast<float>(compositeMaxGray), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_display_fallback_applied", LVecBase2i(stage5DisplayFallbackApplied ? 1 : 0, 0));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_aero_body_delta_K", LVecBase2f(static_cast<float>(components.bodyAeroDeltaK), 0.0f));
	SetShaderInputCached(targetPlat.nodePath, "u_stage5_aero_mach", LVecBase2f(static_cast<float>(components.mach), 0.0f));

	const std::map<PLATFORM_TYPE, PlatformResPath>::const_iterator resIter = m_platformResMap.find(targetPlat.type);
	bool baseTextureAvailable = false;
	if (resIter != m_platformResMap.end())
	{
		baseTextureAvailable = !resIter->second.texturePath.empty() && FileExists(resIter->second.texturePath);
	}

	const std::uint64_t frameSeq = m_currentFrameTelemetry.sourceSeq > 0
		? m_currentFrameTelemetry.sourceSeq : m_stage0DisplayFrameCount;
	const std::string componentLogKey = targetKey + "#radiance-components";
	const std::string componentLogState =
		std::to_string(static_cast<int>(stage5Input.band)) + ":" +
		std::to_string(targetPlat.targetState.engineState ? 1 : 0) + ":" +
		std::to_string(rearEnabledForShader ? 1 : 0) + ":" +
		std::to_string(brightSpot.enabled ? 1 : 0) + ":" +
		std::to_string(static_cast<int>(brightSpot.part)) + ":" +
		std::to_string(m_stage5DebugViewMode) + ":" +
		stage5Input.pathRadianceSource + ":" +
		std::to_string(stage5Input.aeroAppliedToRadiance ? 1 : 0) + ":" +
		std::to_string(useSensorInputForDisplayEffective ? 1 : 0) + ":" +
		m_stage5SensorInputDisplayMode + ":" +
		Stage5ModtranCacheDouble(stage5Input.bodyAeroDeltaK) + ":" +
		Stage5ModtranCacheDouble(stage5Input.tauUp);
	const bool componentStateChanged =
		m_lastStage5RadianceComponentLogState[componentLogKey] != componentLogState;
	m_lastStage5RadianceComponentLogState[componentLogKey] = componentLogState;
	const bool stage5LogDue =
		frameSeq <= 3 ||
		(m_stage5ComponentLogEveryFrames > 0 && (frameSeq % static_cast<std::uint64_t>(m_stage5ComponentLogEveryFrames)) == 0) ||
		componentStateChanged;
	const bool logStage5 =
		m_enableIRVerboseLog ||
		((m_stage5LogComponents || m_enableStage5RadianceDebug) && stage5LogDue);
	if (tauFallbackReason != "none" && (stage5LogDue || m_enableIRVerboseLog))
	{
		std::cout << "[Stage5 Tau][WARN]"
			<< " sourceSeq=" << frameSeq
			<< " targetID=" << targetPlat.targetState.targetID
			<< " rawTauUp=" << rawTauUp
			<< " tauUp=" << components.tauUp
			<< " tauUpSource=" << components.tauUpSource
			<< " tauFallbackReason=" << components.tauFallbackReason
			<< std::endl;
	}
	if (logStage5)
	{
		std::cout << "[Stage5 RadianceComponents]"
			<< " sourceSeq=" << frameSeq
			<< " targetPlatID=" << targetPlat.targetState.targetPlatID
			<< " targetID=" << targetPlat.targetState.targetID
			<< " band=" << IRBandName(components.band)
			<< " materialName=" << components.materialName
			<< " materialTempK=" << components.materialTempK
			<< " emissivity=" << components.emissivity
			<< " reflectance=" << components.reflectance
			<< " bodyRadiance=" << components.bodyRadiance
			<< " reflectedRadiance=" << components.reflectedRadiance
			<< " rearHotspotRadiance=" << components.rearHotspotRadiance
			<< " plumeRadiance=" << components.plumeRadiance
			<< " brightspotRadiance=" << components.brightspotRadiance
			<< " surfaceRadiance=" << components.surfaceRadiance
			<< " surfaceRadianceNoAero=" << components.surfaceRadianceNoAero
			<< " surfaceRadianceWithAero=" << components.surfaceRadianceWithAero
			<< " tauUp=" << components.tauUp
			<< " tauUpSource=" << components.tauUpSource
			<< " tauUpValid=" << (components.tauUpValid ? "1" : "0")
			<< " tauFallbackReason=" << components.tauFallbackReason
			<< " pathRadiance=" << components.pathRadiance
			<< " legacyPathRadiance=" << components.legacyPathRadiance
			<< " modtranPathRadiance=" << components.modtranPathRadiance
			<< " modtranPathRaw=" << components.modtranPathRaw
			<< " modtranPathScaled=" << components.modtranPathScaled
			<< " effectivePathRadiance=" << components.effectivePathRadiance
			<< " modtranPathRuntimeMode=" << components.modtranPathRuntimeMode
			<< " modtranPathUnitMode=" << components.modtranPathUnitMode
			<< " modtranPathScale=" << components.modtranPathScale
			<< " modtranPathBlend=" << components.modtranPathBlend
			<< " modtranSkyRadiance=" << components.modtranSkyRadiance
			<< " modtranSolarIrradiance=" << components.modtranSolarIrradiance
			<< " modtranRadianceValid=" << (components.modtranRadianceValid ? "1" : "0")
			<< " pathRadianceSource=" << components.pathRadianceSource
			<< " modtranFallbackReason=" << components.modtranFallbackReason
			<< " modtranInterpolationMode=" << components.modtranInterpolationMode
			<< " altitudeM=" << components.altitudeM
			<< " speedRawKmh=" << components.speedRawKmh
			<< " speedSource=" << components.speedSource
			<< " speedMps=" << components.speedMps
			<< " mach=" << components.mach
			<< " airTempK=" << components.airTempK
			<< " recoveryTempK=" << components.recoveryTempK
			<< " aeroDeltaK=" << components.aeroDeltaK
			<< " bodyAeroDeltaK=" << components.bodyAeroDeltaK
			<< " bodyTempBaseK=" << components.bodyTempBaseK
			<< " bodyAeroDeltaKRaw=" << components.bodyAeroDeltaKRaw
			<< " bodyAeroDeltaKEffective=" << components.bodyAeroDeltaKEffective
			<< " bodyTempAeroAppliedK=" << components.bodyTempAeroAppliedK
			<< " bodyRadianceNoAero=" << components.bodyRadianceNoAero
			<< " bodyRadianceWithAero=" << components.bodyRadianceWithAero
			<< " sensorInputNoAero=" << components.sensorInputNoAero
			<< " sensorInputWithAero=" << components.sensorInputWithAero
			<< " sensorInputRatio=" << sensorInputRatio
			<< " aeroRadianceRatio=" << components.aeroRadianceRatio
			<< " noseAeroDeltaK=" << components.noseAeroDeltaK
			<< " edgeAeroDeltaK=" << components.edgeAeroDeltaK
			<< " rearAeroDeltaK=" << components.rearAeroDeltaK
			<< " aeroValid=" << (components.aeroValid ? "1" : "0")
			<< " aeroFallbackReason=" << components.aeroFallbackReason
			<< " aeroAppliedToRadiance=" << (components.aeroAppliedToRadiance ? "1" : "0")
			<< " sensorInputLegacy=" << components.sensorInputLegacy
			<< " sensorInputModtran=" << components.sensorInputModtran
			<< " sensorInputRadiance=" << components.sensorInputRadiance
			<< " displayPreview=" << components.displayPreview
			<< " displayPreviewNoAero=" << components.displayPreviewNoAero
			<< " displayPreviewWithAero=" << components.displayPreviewWithAero
			<< " sensorInputDisplayGray=" << sensorInputDisplayGray
			<< " sensorInputToDisplayEnabled=" << (components.sensorInputToDisplayEnabled ? "1" : "0")
			<< " sensorInputDisplayRequested=" << (m_stage5UseSensorInputForDisplay ? "1" : "0")
			<< " sensorInputDisplayBandAllowed=" << (sensorInputDisplayBandAllowed ? "1" : "0")
			<< " sensorInputDisplayMode=" << m_stage5SensorInputDisplayMode
			<< " sensorInputDisplayScale=" << m_stage5SensorInputDisplayScale
			<< " sensorInputDisplayOffset=" << m_stage5SensorInputDisplayOffset
			<< " sensorInputDisplayClampMin=" << m_stage5SensorInputDisplayClampMin
			<< " sensorInputDisplayClampMax=" << m_stage5SensorInputDisplayClampMax
			<< " sensorInputDisplayGamma=" << m_stage5SensorInputDisplayGamma
			<< " sensorInputDisplayBand=" << m_stage5SensorInputDisplayBandName
			<< " sourceFlags=" << components.sourceFlags
			<< " DebugView=" << m_stage5DebugViewModeName
			<< std::endl;

		std::cout << "[Stage5 Radiance]"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " debugViewMode=" << m_stage5DebugViewModeName
			<< " toneMap=" << Stage5ToneMapName(stage5Input.debugConfig.toneMap)
			<< " useBaseTextureModulation=" << (stage5UseBaseTextureModulation ? "1" : "0")
			<< " materialTempK=" << stage5Input.materialTemperatureK
			<< " materialTempEffectiveK=" << components.materialTempK
			<< " emissivity=" << stage5Input.materialEmissivity
			<< " tauUp=" << stage5Input.tauUp
			<< " tauUpSource=" << components.tauUpSource
			<< " tauUpValid=" << (components.tauUpValid ? "1" : "0")
			<< " tauFallbackReason=" << components.tauFallbackReason
			<< " sunElevation=" << environment.sunElevationDeg
			<< " sunAzimuth=" << environment.sunAzimuthDeg
			<< " ndotl=" << stage5Input.ndotl
			<< " textureLuma=" << stage5Input.textureLuma
			<< " reflectanceBand=" << stage5Input.materialReflectance
			<< " solarWeight=" << stage5Input.solarReflectanceWeight
			<< " bodyRadiance=" << stage5.bodyRadiance
			<< " reflectedRadiance=" << stage5.reflectedRadiance
			<< " hotspotRadiance=" << stage5.hotspotRadiance
			<< " plumeRadiance=" << components.plumeRadiance
			<< " brightspotRadiance=" << stage5.brightspotRadiance
			<< " surfaceRadiance=" << components.surfaceRadiance
			<< " pathRadiance=" << components.pathRadiance
			<< " pathRadianceSource=" << components.pathRadianceSource
			<< " legacyPathRadiance=" << components.legacyPathRadiance
			<< " modtranPathRadiance=" << components.modtranPathRadiance
			<< " modtranPathRaw=" << components.modtranPathRaw
			<< " modtranPathScaled=" << components.modtranPathScaled
			<< " effectivePathRadiance=" << components.effectivePathRadiance
			<< " modtranPathRuntimeMode=" << components.modtranPathRuntimeMode
			<< " modtranSkyRadiance=" << components.modtranSkyRadiance
			<< " modtranSolarIrradiance=" << components.modtranSolarIrradiance
			<< " modtranRadianceValid=" << (components.modtranRadianceValid ? "1" : "0")
			<< " modtranFallbackReason=" << components.modtranFallbackReason
			<< " modtranInterpolationMode=" << components.modtranInterpolationMode
			<< " altitudeM=" << components.altitudeM
			<< " speedRawKmh=" << components.speedRawKmh
			<< " speedSource=" << components.speedSource
			<< " speedMps=" << components.speedMps
			<< " mach=" << components.mach
			<< " aeroDeltaK=" << components.aeroDeltaK
			<< " bodyAeroDeltaK=" << components.bodyAeroDeltaK
			<< " bodyTempBaseK=" << components.bodyTempBaseK
			<< " bodyAeroDeltaKRaw=" << components.bodyAeroDeltaKRaw
			<< " bodyAeroDeltaKEffective=" << components.bodyAeroDeltaKEffective
			<< " bodyTempAeroAppliedK=" << components.bodyTempAeroAppliedK
			<< " bodyRadianceNoAero=" << components.bodyRadianceNoAero
			<< " bodyRadianceWithAero=" << components.bodyRadianceWithAero
			<< " sensorInputNoAero=" << components.sensorInputNoAero
			<< " sensorInputWithAero=" << components.sensorInputWithAero
			<< " sensorInputRatio=" << sensorInputRatio
			<< " aeroRadianceRatio=" << components.aeroRadianceRatio
			<< " noseAeroDeltaK=" << components.noseAeroDeltaK
			<< " edgeAeroDeltaK=" << components.edgeAeroDeltaK
			<< " rearAeroDeltaK=" << components.rearAeroDeltaK
			<< " aeroAppliedToRadiance=" << (components.aeroAppliedToRadiance ? "1" : "0")
			<< " sensorInputLegacy=" << components.sensorInputLegacy
			<< " sensorInputModtran=" << components.sensorInputModtran
			<< " sensorInputRadiance=" << components.sensorInputRadiance
			<< " sensorInputDisplayGray=" << sensorInputDisplayGray
			<< " sensorInputToDisplayEnabled=" << (components.sensorInputToDisplayEnabled ? "1" : "0")
			<< " bodyGrayBeforeFloor=" << stage5.bodyGrayBeforeFloor
			<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
			<< " reflectedGray=" << stage5.reflectedGray
			<< " hotspotGray=" << stage5.hotspotGray
			<< " plumeGray=" << plumeGrayDisplay
			<< " brightspotGray=" << stage5.brightspotGray
			<< " finalGrayDebug=" << stage5.finalGrayDebug
			<< " debugFloorApplied=" << (stage5.debugFloorApplied ? "1" : "0")
			<< " stage5DisplayFallbackApplied=" << (stage5DisplayFallbackApplied ? "1" : "0")
			<< std::endl;

		std::cout << "[Stage5C VisualCalib]"
			<< " band=" << IRBandName(stage5Input.band)
			<< " bodyGray=" << bodyGrayDisplay
			<< " reflectedGray=" << reflectedGrayDisplay
			<< " hotspotGrayRaw=" << stage5.hotspotGray
			<< " hotspotGrayDisplay=" << hotspotGrayDisplay
			<< " plumeGrayDisplay=" << plumeGrayDisplay
			<< " brightspotGrayRaw=" << stage5.brightspotGray
			<< " brightspotGrayDisplay=" << brightspotGrayDisplay
			<< " atmosphereGrayDisplay=" << atmosphereGrayDisplay
			<< " sensorInputDisplayGray=" << sensorInputDisplayGray
			<< " finalGrayDebug=" << finalGrayDebugDisplay
			<< " fallbackApplied=" << (stage5DisplayFallbackApplied ? "1" : "0")
			<< " legacyRearHotspotIntensity=" << legacyRearHotspotDisplay
			<< " legacyBrightspotIntensity=" << legacyBrightspotDisplay
			<< std::endl;
	}
	LogStage5AeroThermal(targetPlat, components, aeroOutput);
	LogStage5ModtranRadianceCompare(targetPlat, components, modtranRadiance, rangeKmForLog, observerAltKmForLog, targetAltKmForLog);
	LogStage5ModtranPathAB(targetPlat, components, modtranRadiance, rangeKmForLog, observerAltKmForLog, targetAltKmForLog);

	const bool stage5DiagnosticsEnabled =
		logStage5 ||
		m_stage5LogComponents ||
		m_enableIRVerboseLog ||
		m_enableStage5RadianceDebug;
	if (stage5DiagnosticsEnabled && stage5.bodyRadiance > 0.0 && stage5.bodyGrayAfterFloor <= 0.0)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_BODY_GRAY_ZERO_AFTER_MAPPING"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " bodyRadiance=" << stage5.bodyRadiance
			<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
			<< std::endl;
	}
	if (stage5DiagnosticsEnabled && !m_stage5BodyGrayPathHintLogged && stage5.bodyGrayAfterFloor > 0.1)
	{
		std::cout << "[Stage5 Radiance][WARN] CHECK_SHADER_BODY_GRAY_PATH_OR_FRAME_OUTPUT"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " bodyGrayAfterFloor=" << stage5.bodyGrayAfterFloor
			<< " debugViewMode=" << m_stage5DebugViewModeName
			<< std::endl;
		m_stage5BodyGrayPathHintLogged = true;
	}
	if (stage5DiagnosticsEnabled && !m_stage5BaseTextureFallbackLogged && stage5Input.solarReflectanceWeight > 0.0 && !baseTextureAvailable)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_BASE_TEXTURE_FALLBACK"
			<< " targetKey=" << targetKey
			<< " band=" << IRBandName(stage5Input.band)
			<< " textureLumaFallback=1"
			<< std::endl;
		m_stage5BaseTextureFallbackLogged = true;
	}
	if (stage5DiagnosticsEnabled && !m_stage5NormalFallbackHintLogged)
	{
		std::cout << "[Stage5 Radiance][WARN] STAGE5_NORMAL_FALLBACK"
			<< " mode=shader_constant_plus_z_if_p3d_Normal_missing"
			<< " targetKey=" << targetKey
			<< std::endl;
		m_stage5NormalFallbackHintLogged = true;
	}
	if ((stage5Input.band == IRBand::Visible || stage5Input.band == IRBand::NearInfrared || stage5Input.band == IRBand::ShortWaveInfrared)
		&& stage5Input.solarReflectanceWeight > 0.0
		&& stage5Input.solarStrength > 0.0
		&& stage5.reflectedGray <= 0.0)
	{
		++m_stage5ConsecutiveReflectedZeroFrames;
		if (m_stage5ConsecutiveReflectedZeroFrames >= 3 && logStage5)
		{
			std::cout << "[Stage5 Radiance][WARN] STAGE5_REFLECTED_GRAY_ZERO"
				<< " targetKey=" << targetKey
				<< " band=" << IRBandName(stage5Input.band)
				<< " solarWeight=" << stage5Input.solarReflectanceWeight
				<< " solarStrength=" << stage5Input.solarStrength
				<< " reflectedRadiance=" << stage5.reflectedRadiance
				<< std::endl;
		}
	}
	else if (stage5.reflectedGray > 0.0)
	{
		m_stage5ConsecutiveReflectedZeroFrames = 0;
	}

	if (stage5DiagnosticsEnabled && stage5.finalGrayDebug <= 0.0)
	{
		++m_stage5ConsecutiveZeroFrames;
		if (m_stage5ConsecutiveZeroFrames >= 3 && logStage5)
		{
			std::cout << "[Stage5 Radiance][WARN] STAGE5_TARGET_BODY_RADIANCE_ZERO"
				<< " targetKey=" << targetKey
				<< " consecutiveFrames=" << m_stage5ConsecutiveZeroFrames
				<< std::endl;
		}
	}
	else
	{
		m_stage5ConsecutiveZeroFrames = 0;
	}
	m_stage5RadianceComponentMsCurrent += std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - stage5ComponentStart).count();
}

void HwaSimIR::LogActiveIRSensorProfile(int protocolBand, const char* reason, bool forceLog)
{
	if (!forceLog && protocolBand == m_lastLoggedSensorProtocolBand)
	{
		return;
	}
	const IRSensorProfile& profile = m_irSensorProfiles.profileForProtocolBand(protocolBand);
	IRBand band = IRBandFromProtocol(protocolBand);
	IRBandRange modelRange = IRDefaultRangeForBand(band);
	std::cout << "[Stage1] Sensor profile (" << reason << "): protocolBand=" << protocolBand
		<< ", band=" << IRBandName(band)
		<< ", sensorRange=" << profile.spectralLowUm << "-" << profile.spectralHighUm << "um"
		<< ", modelRange=" << modelRange.lowUm << "-" << modelRange.highUm << "um"
		<< ", profileSize=" << profile.width << "x" << profile.height
		<< ", ADC=" << profile.adcBits << "bit"
		<< ", display=" << profile.displayBits << "bit"
		<< ", NETD=" << profile.netdK << "K"
		<< ", FOV=" << profile.fovHDeg << "x" << profile.fovVDeg
		<< ", detectorPitch=" << profile.detectorPitchMm
		<< "mm, focalLength=" << profile.focalLengthMm
		<< "mm, lensFnumber=" << profile.lensFNumber
		<< ", blackHot=" << (profile.blackHot ? "1" : "0")
		<< ", source=" << profile.sourcePath << std::endl;
	std::cout << "[SensorWave Usage]"
		<< " band=" << IRBandName(band)
		<< " file=" << profile.sourcePath
		<< " usedFields=" << profile.usedFields
		<< " fallbackFields=" << profile.fallbackFields
		<< " ignoredPresagisFields=" << profile.ignoredPresagisFields
		<< std::endl;
	m_lastLoggedSensorProtocolBand = protocolBand;
}

double HwaSimIR::CurrentSimulationHour() const
{
	if (m_stage0DisplayFrameCount > 0 && m_realTimeSceneData.time >= 0.0)
	{
		double hour = std::fmod(m_realTimeSceneData.time / 3600000.0, 24.0);
		return hour < 0.0 ? hour + 24.0 : hour;
	}
	// 没有实时帧时间时使用正午，便于在功能测试中获得稳定太阳照射。
	return 12.0;
}

IRRuntimeEnvironment HwaSimIR::BuildRuntimeEnvironment() const
{
	IRRuntimeEnvironment environment = m_irRadianceModel.environment();
	int protocolBand = (m_sensorParam.trackerSensorBand >= 0 && m_sensorParam.trackerSensorBand <= 4)
		? m_sensorParam.trackerSensorBand : 2;
	environment.band = IRBandFromProtocol(protocolBand);
	environment.simulationHour = CurrentSimulationHour();

	if (m_irWeatherReady)
	{
		// 场景 profile 提供地区/日期相关的太阳高度、方位角和逐小时温度。
		IRWeatherSample sample = m_irWeatherProfile.sampleForHour(environment.simulationHour);
		environment.airTemperatureC = sample.airTemperatureC;
		environment.sunAzimuthDeg = sample.sunAzimuthDeg;
		environment.sunElevationDeg = sample.sunElevationDeg;
	}

	if (m_isAddPlatform)
	{
		// 环境优先级：UDP 初始化参数 > 场景 profile > 内置默认值。
		if (m_initSceneData.trackingInit.envTemp > -80.0 && m_initSceneData.trackingInit.envTemp < 80.0)
		{
			environment.airTemperatureC = m_initSceneData.trackingInit.envTemp;
		}
		if (m_initSceneData.trackingInit.envVisibility > 1.0)
		{
			environment.visibilityMeters = m_initSceneData.trackingInit.envVisibility;
		}
		if (m_initSceneData.trackingInit.envHumidity >= 0.0 && m_initSceneData.trackingInit.envHumidity <= 100.0)
		{
			environment.humidityPercent = m_initSceneData.trackingInit.envHumidity;
		}
		if (m_initSceneData.trackingInit.envWindV >= 0.0 && m_initSceneData.trackingInit.envWindV < 120.0)
		{
			environment.windSpeedMps = m_initSceneData.trackingInit.envWindV;
		}
		if (m_initSceneData.trackingInit.envWindDir >= 0.0 && m_initSceneData.trackingInit.envWindDir <= 360.0)
		{
			environment.windDirectionDeg = m_initSceneData.trackingInit.envWindDir;
		}
		environment.weatherCode = m_initSceneData.trackingInit.envSky;
	}

	environment.sunStrength = WeatherSunStrength(environment.weatherCode, environment.sunElevationDeg);
	return environment;
}

void HwaSimIR::LogActiveIREnvironment(const IRRuntimeEnvironment& environment, const char* reason, bool forceLog)
{
	int hourKey = static_cast<int>(std::floor(environment.simulationHour));
	if (!forceLog && hourKey == m_lastLoggedEnvironmentHour && environment.weatherCode == m_lastLoggedEnvironmentWeather)
	{
		return;
	}
	std::cout << "[Stage3] Environment (" << reason << "): hour=" << environment.simulationHour
		<< ", airTempC=" << environment.airTemperatureC
		<< ", visibilityM=" << environment.visibilityMeters
		<< ", humidity=" << environment.humidityPercent
		<< ", wind=" << environment.windSpeedMps << "m/s@" << environment.windDirectionDeg << "deg"
		<< ", weatherCode=" << environment.weatherCode
		<< ", sunAzimuth=" << environment.sunAzimuthDeg
		<< ", sunElevation=" << environment.sunElevationDeg
		<< ", sunStrength=" << environment.sunStrength
		<< ", profile=" << (m_irWeatherReady ? m_irWeatherProfile.loadedPath() : "fallback")
		<< std::endl;
	m_lastLoggedEnvironmentHour = hourKey;
	m_lastLoggedEnvironmentWeather = environment.weatherCode;
}

// 动态更新红外状态
void HwaSimIR::UpdatePlatformIRStatus() {
	IRUpdateBreakdown breakdown;
	const std::uint64_t breakdownIndex = ++m_irBreakdownUpdateCounter;
	const bool profileBreakdown = breakdownIndex <= 3 || (breakdownIndex % 30) == 0;
	breakdown.timingSample = profileBreakdown;
	ResetShaderInputCounters();
	m_stage5RadianceComponentMsCurrent = 0.0;
	m_stage5AeroThermalMsCurrent = 0.0;
	m_stage5ModtranLookupMsCurrent = 0.0;
	m_stage5ModtranCacheHitCurrent = 0;
	m_stage5ModtranCacheMissCurrent = 0;
	m_measureShaderInputApplyTime = profileBreakdown;
	std::chrono::steady_clock::time_point envBuildStart;
	if (profileBreakdown)
	{
		envBuildStart = std::chrono::steady_clock::now();
	}
	double current_time = ClockObject::get_global_clock()->get_frame_time();
	float stage4DtSec = 0.033f;
	if (m_lastStage4UpdateTime >= 0.0)
	{
		stage4DtSec = static_cast<float>(current_time - m_lastStage4UpdateTime);
	}
	m_lastStage4UpdateTime = current_time;

	IRRuntimeEnvironment environment = BuildRuntimeEnvironment();
	const float ambientTempK = static_cast<float>(environment.airTemperatureC + 273.15);
	int protocolBand = (m_sensorParam.trackerSensorBand >= 0 && m_sensorParam.trackerSensorBand <= 4)
		? m_sensorParam.trackerSensorBand : 2;
	LogActiveIRSensorProfile(protocolBand, "runtime-band", false);
	LogActiveIREnvironment(environment, "runtime", false);
	m_irRadianceModel.setEnvironment(environment);
	if (profileBreakdown)
	{
		breakdown.irEnvBuildMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - envBuildStart).count();
	}

	std::chrono::steady_clock::time_point stage7Start;
	if (profileBreakdown)
	{
		stage7Start = std::chrono::steady_clock::now();
	}
	const bool hasStage7Background = m_enableStage7SkyHorizon && (!m_skyNode.is_empty() || !m_stage7LowerShellNode.is_empty());
	if (hasStage7Background)
	{
		const std::string stage7Key = BuildStage7UpdateKey(environment);
		const double stage7IntervalSec = 1.0 / std::max(1.0, m_stage7UpdateHz);
		const bool stage7Dirty = stage7Key != m_stage7LastFullUpdateKey;
		const bool stage7Due = m_stage7LastFullUpdateTime < 0.0 ||
			(current_time - m_stage7LastFullUpdateTime) >= stage7IntervalSec;
		if (stage7Dirty || stage7Due)
		{
			UpdateStage7SkyHorizon(environment, "runtime", false);
			if (!m_skyNode.is_empty())
			{
				IRObjectRadianceOutput skyRadiance = EvaluateNodeRadiance("BM_AIR", m_skyNode, false, false, true, false, 0.0);
				ApplyRadianceInputs(m_skyNode, skyRadiance, 1);
				SetShaderInputCached(m_skyNode, "u_time", LVecBase2f(static_cast<float>(current_time), 0.0f));
			}
			if (!m_stage7LowerShellNode.is_empty())
			{
				const bool seaTerrain = m_isAddPlatform && m_initSceneData.trackingInit.envTerrain == 2;
				const char* groundMaterial = seaTerrain ? "BM_WATER-OCEAN" : "BM_SAND";
				IRObjectRadianceOutput groundRadiance = EvaluateNodeRadiance(groundMaterial, m_stage7LowerShellNode, false, false, false, false, 0.0);
				ApplyRadianceInputs(m_stage7LowerShellNode, groundRadiance, 1);
				SetShaderInputCached(m_stage7LowerShellNode, "u_time", LVecBase2f(static_cast<float>(current_time), 0.0f));
			}
			UpdateStage7WeatherNodes(m_stage7WeatherState, current_time);
			m_stage7LastFullUpdateTime = current_time;
			m_stage7LastFullUpdateKey = stage7Key;
			++breakdown.stage7FullUpdateCount;
		}
		else
		{
			UpdateStage7SkyHorizonPositionOnly();
			++breakdown.stage7PositionOnlyCount;
		}
	}
	else
	{
		++breakdown.stage7SkipCount;
	}
	if (profileBreakdown)
	{
		breakdown.stage7SkyGroundMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - stage7Start).count();
	}

	// 更新飞机平台 (PlatParamPak)
	for (auto& pakPlat : m_pakPlatformList) {
		if (pakPlat.isExist) {
			std::chrono::steady_clock::time_point platformRadianceStart;
			if (profileBreakdown)
			{
				platformRadianceStart = std::chrono::steady_clock::now();
			}
			IRObjectRadianceOutput radiance = EvaluateNodeRadiance(MaterialNameForPlatform(pakPlat.type), pakPlat.nodePath, true, false, false, false, 0.0, pakPlat.platParam.spatial.alt);
			if (profileBreakdown)
			{
				breakdown.platformRadianceMs += std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - platformRadianceStart).count();
			}
			std::chrono::steady_clock::time_point shaderApplyStart;
			if (profileBreakdown)
			{
				shaderApplyStart = std::chrono::steady_clock::now();
			}
			SetShaderInputCached(pakPlat.nodePath, "u_time", LVecBase2f(static_cast<float>(current_time), 0.0f));
			ApplyRadianceInputs(pakPlat.nodePath, radiance, 0);

			// 阶段4不为挂载平台恢复常开尾喷；飞机平台暂无协议engineState，默认保持关闭。
			SetShaderInputCached(pakPlat.nodePath, "u_hotspot_rear_en", LVecBase2i(0, 0));
			if (profileBreakdown)
			{
				breakdown.shaderInputApplyMs += std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - shaderApplyStart).count();
			}
		}
	}

	// 更新目标导弹平台 (TargetState)
	int stage5PlumeNodeCount = 0;
	int stage5VisiblePlumeCount = 0;
	double updatePlumeMs = 0.0;
	for (auto& targetPlat : m_targetPlatformList) {
		if (targetPlat.isExist) {
			if (!targetPlat.enginePlumeCoreNodePath.is_empty())
			{
				++stage5PlumeNodeCount;
			}
			if (!targetPlat.enginePlumeHaloNodePath.is_empty())
			{
				++stage5PlumeNodeCount;
			}
			if (targetPlat.targetState.targetID < 0)
			{
				targetPlat.nodePath.hide();
				HideEnginePlume(targetPlat);
				continue;
			}
			const bool targetRenderable = targetPlat.targetState.viewValid && !targetPlat.nodePath.is_hidden();
			IRObjectRadianceOutput stage5BaseRadiance;
			if (targetRenderable)
			{
				bool damaged = (targetPlat.targetState.targetState == 0x02 || targetPlat.targetState.targetState == 0x03);
				std::chrono::steady_clock::time_point targetRadianceStart;
				if (profileBreakdown)
				{
					targetRadianceStart = std::chrono::steady_clock::now();
				}
				stage5BaseRadiance = EvaluateNodeRadiance(MaterialNameForPlatform(targetPlat.type), targetPlat.nodePath, targetPlat.targetState.engineState, damaged, false, false, 0.0, targetPlat.targetState.targetLoc.alt);
				if (profileBreakdown)
				{
					breakdown.targetRadianceMs += std::chrono::duration<double, std::milli>(
						std::chrono::steady_clock::now() - targetRadianceStart).count();
				}
				std::chrono::steady_clock::time_point shaderApplyStart;
				if (profileBreakdown)
				{
					shaderApplyStart = std::chrono::steady_clock::now();
				}
				SetShaderInputCached(targetPlat.nodePath, "u_time", LVecBase2f(static_cast<float>(current_time), 0.0f));
				ApplyRadianceInputs(targetPlat.nodePath, stage5BaseRadiance, 0);
				if (profileBreakdown)
				{
					breakdown.shaderInputApplyMs += std::chrono::duration<double, std::milli>(
						std::chrono::steady_clock::now() - shaderApplyStart).count();
				}

				if (m_enableStage5RadianceDebug)
				{
					// Stage5A body radiance uses the material/environment baseline; engineState stays local to rear ThermalHotspot.
					std::chrono::steady_clock::time_point targetDebugRadianceStart;
					if (profileBreakdown)
					{
						targetDebugRadianceStart = std::chrono::steady_clock::now();
					}
					stage5BaseRadiance = EvaluateNodeRadiance(MaterialNameForPlatform(targetPlat.type), targetPlat.nodePath, false, false, false, false, 0.0, targetPlat.targetState.targetLoc.alt);
					if (profileBreakdown)
					{
						breakdown.targetRadianceMs += std::chrono::duration<double, std::milli>(
							std::chrono::steady_clock::now() - targetDebugRadianceStart).count();
					}
				}
			}
			IREnginePlumeOutput plumeOutput;
			if (targetRenderable)
			{
				const auto plumeUpdateStart = std::chrono::high_resolution_clock::now();
				plumeOutput = UpdateEnginePlumeForTarget(
					targetPlat,
					stage4DtSec,
					ambientTempK,
					environment.band,
					true,
					current_time,
					nullptr);
				updatePlumeMs += std::chrono::duration<double, std::milli>(
					std::chrono::high_resolution_clock::now() - plumeUpdateStart).count();
			}
			else
			{
				HideEnginePlume(targetPlat);
			}
			std::chrono::steady_clock::time_point stage4Start;
			if (profileBreakdown)
			{
				stage4Start = std::chrono::steady_clock::now();
			}
			const bool stage4WroteInputs = ApplyStage4TargetState(targetPlat, m_realTimeSceneData.weaponState, stage4DtSec, ambientTempK, stage5BaseRadiance, targetRenderable);
			if (stage4WroteInputs)
			{
				++breakdown.stage4UpdateCount;
			}
			else
			{
				++breakdown.stage4SkipCount;
			}
			if (profileBreakdown)
			{
				breakdown.stage4HotspotMs += std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - stage4Start).count();
			}
			if (plumeOutput.coreNodeVisible && targetRenderable)
			{
				++stage5VisiblePlumeCount;
			}
			if (plumeOutput.haloNodeVisible && targetRenderable)
			{
				++stage5VisiblePlumeCount;
			}
		}
	}
	breakdown.stage5PlumeMs = updatePlumeMs;
	breakdown.stage5RadianceComponentMs = m_stage5RadianceComponentMsCurrent;
	breakdown.stage5AeroThermalMs = m_stage5AeroThermalMsCurrent;
	breakdown.stage5ModtranLookupMs = m_stage5ModtranLookupMsCurrent;
	breakdown.stage5ModtranCacheHitCount = m_stage5ModtranCacheHitCurrent;
	breakdown.stage5ModtranCacheMissCount = m_stage5ModtranCacheMissCurrent;
	const ShaderInputCacheStats shaderStats = SnapshotShaderInputCounters();
	breakdown.shaderInputSetCount = shaderStats.setCount;
	breakdown.shaderInputSkipCount = shaderStats.skipCount;
	breakdown.shaderInputApplyMs = shaderStats.applyMs;
	m_measureShaderInputApplyTime = false;
	m_perfStats.recordIrUpdateBreakdown(breakdown);
	m_perfStats.recordPlumeUpdate(updatePlumeMs);
	LogStage5PlumePerf(stage5PlumeNodeCount, stage5VisiblePlumeCount, updatePlumeMs);
}




void HwaSimIR::SetRenderMode(bool isSync, double targetFPS) {
	m_bSyncRenderMode.store(isSync);
	const double configuredTarget = targetFPS > 0.0
		? targetFPS
		: static_cast<double>(m_targetVideoFps.load());
	m_perfStats.configure(isSync, configuredTarget);
	if (m_pTcpThread)
	{
		m_pTcpThread->setSyncMode(isSync);
	}

	ClockObject* globalClock = ClockObject::get_global_clock();
	if (isSync) {
		// 同步模式：解除帧率限制，完全由 UDP 的到达频率驱动
		globalClock->set_mode(ClockObject::M_normal);
		std::cout << "切换至【同步渲染模式】，1组UDP数据渲染1帧" << std::endl;
	}
	else {
		// 异步模式：启用内部帧率限制
		if (targetFPS > 0.0) {
			globalClock->set_mode(ClockObject::M_limited);
			globalClock->set_frame_rate(targetFPS);
			std::cout << "切换至【异步渲染模式】，锁定帧率: " << targetFPS << " FPS" << std::endl;
		}
		else {
			globalClock->set_mode(ClockObject::M_normal);
			std::cout << "切换至【异步渲染模式】，不限帧（性能拉满）" << std::endl;
		}
	}
}

void HwaSimIR::OnTcpFrameSent(
	const IRFrameTelemetry& telemetry,
	std::uint64_t outputOrdinal,
	double flipMs,
	double resizeMs,
	double jpegMs,
	double tcpSendMs,
	int queueDepth,
	double queueWaitMs,
	bool overwritten)
{
	const std::int64_t nowWallNs = IRPerfStats::wallTimeNs();
	const std::int64_t nowSteadyNs = IRPerfStats::steadyTimeNs();
	const double latencyMs = telemetry.udpReceiveTimeNs > 0
		? static_cast<double>(nowWallNs - telemetry.udpReceiveTimeNs) / 1.0e6
		: -1.0;
	const double frameMs = telemetry.processStartTimeNs > 0
		? static_cast<double>(nowSteadyNs - telemetry.processStartTimeNs) / 1.0e6
		: -1.0;
	m_perfStats.recordTcpOutput(
		jpegMs,
		tcpSendMs,
		latencyMs,
		queueDepth,
		telemetry.sourceSeq,
		m_latestUdpSourceSeq.load());
	const double targetFps = m_perfStats.videoFpsTarget();
	const double frameBudgetMs = targetFps > 0.0 ? 1000.0 / targetFps : 0.0;
	const bool overrun = m_bSyncRenderMode.load() && frameBudgetMs > 0.0 &&
		(frameMs > frameBudgetMs || queueWaitMs > frameBudgetMs);
	if (overrun)
	{
		m_perfStats.recordSyncOverrun();
	}

	const std::uint64_t previousSourceSeq = m_lastOutputSourceSeq.exchange(telemetry.sourceSeq);
	const bool sourceSeqContinuous = previousSourceSeq == 0 || telemetry.sourceSeq == previousSourceSeq + 1;
	m_lastSourceSeqContinuous.store(sourceSeqContinuous);
	if (m_bSyncRenderMode.load() &&
		(outputOrdinal <= 3 ||
			(outputOrdinal % 120) == 0 ||
			(overrun && (outputOrdinal % 30) == 0) ||
			overwritten))
	{
		std::ostringstream syncLine;
		syncLine << std::fixed << std::setprecision(3)
			<< "[SyncFrame]"
			<< " sourceSeq=" << telemetry.sourceSeq
			<< " outputOrdinal=" << outputOrdinal
			<< " inputQueueDepth=" << telemetry.inputQueueDepth
			<< " tcpQueueDepth=" << queueDepth
			<< " sourceSeqContinuous=" << (sourceSeqContinuous ? "1" : "0")
			<< " videoFpsTarget=" << targetFps
			<< " frameMs=" << frameMs
			<< " latencyMs=" << latencyMs
			<< " readbackMs=" << telemetry.readbackMs
			<< " flipMs=" << flipMs
			<< " resizeMs=" << resizeMs
			<< " jpegMs=" << jpegMs
			<< " tcpSendMs=" << tcpSendMs
			<< " overrun=" << (overrun ? "1" : "0")
			<< " overwritten=" << (overwritten ? "1" : "0");
		std::cout << syncLine.str() << std::endl;
	}
	m_perfStats.maybeLog();
}







// HwaSimIR 的每帧更新任务回调
AsyncTask::DoneStatus HwaSimIR::shader_update_task(GenericAsyncTask* task, void* data) {
	HwaSimIR* self = static_cast<HwaSimIR*>(data);
	if (self->m_isSimRunning.load() &&
		(!self->m_bSyncRenderMode.load() || self->m_syncFrameActive.load())) {
		const std::uint64_t sourceSeq = self->m_currentFrameTelemetry.sourceSeq;
		const double targetFps = std::max(1.0, static_cast<double>(self->m_targetVideoFps.load()));
		const std::uint64_t updateStride = static_cast<std::uint64_t>(
			std::max(1.0, std::floor(targetFps / std::max(1.0, self->m_irUpdateHz) + 0.5)));
		std::ostringstream state;
		state << self->m_sensorParam.trackerSensorBand
			<< ":" << self->m_realTimeSceneData.targetNumValid
			<< ":" << self->m_realTimeSceneData.weaponState.strikeFlag
			<< ":" << self->m_realTimeSceneData.weaponState.strikePart
			<< ":" << self->m_realTimeSceneData.weaponState.targetType
			<< ":" << self->m_realTimeSceneData.weaponState.targetPlatID
			<< ":" << self->m_realTimeSceneData.weaponState.targetID;
		for (int i = 0; i < 5; ++i)
		{
			const BYHWICD::TargetState& target = self->m_realTimeSceneData.targetState[i];
			state << "|" << target.targetType
				<< ":" << target.targetPlatID
				<< ":" << target.targetID
				<< ":" << target.engineState
				<< ":" << target.viewValid
				<< ":" << target.targetState;
		}
		const std::string stateKey = state.str();
		const bool stateChanged = stateKey != self->m_lastIrUpdateState;
		const bool updateDue = !self->m_bSyncRenderMode.load() ||
			sourceSeq <= 3 ||
			self->m_lastIrUpdateSourceSeq == 0 ||
			sourceSeq >= self->m_lastIrUpdateSourceSeq + updateStride;
		if (stateChanged || updateDue)
		{
			const auto begin = std::chrono::steady_clock::now();
			self->UpdatePlatformIRStatus();
			self->m_perfStats.recordIrUpdate(std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - begin).count());
			self->m_lastIrUpdateSourceSeq = sourceSeq;
			self->m_lastIrUpdateState = stateKey;
		}
		else
		{
			self->m_perfStats.recordIrUpdate(0.0);
		}
	}
	return AsyncTask::DS_cont;
}

// ================= 新增：主线程执行的场景更新任务 =================
AsyncTask::DoneStatus HwaSimIR::scene_update_task(GenericAsyncTask* task, void* data) {
	(void)task;
	(void)data;
	return AsyncTask::DS_cont;
}

AsyncTask::DoneStatus HwaSimIR::capture_task(GenericAsyncTask* task, void* data) {
	HwaSimIR* self = static_cast<HwaSimIR*>(data);
	if (!self->m_pTcpThread)
	{
		return AsyncTask::DS_cont;
	}
	if (self->m_bSyncRenderMode.load() &&
		(!self->m_isSimRunning.load() || !self->m_syncFrameActive.load()))
	{
		return AsyncTask::DS_cont;
	}

	const auto readbackBegin = std::chrono::steady_clock::now();
	if (self->m_renderTex->has_ram_image()) {
		CPTA_uchar ram_image = self->m_renderTex->get_ram_image_as("RGB");
		const double readbackMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - readbackBegin).count();
		if (ram_image) {
			int width = self->m_renderTex->get_x_size();
			int height = self->m_renderTex->get_y_size();
			int frameWidth = width;
			int frameHeight = height;
			const uchar* frameData = ram_image.p();
			std::vector<uchar> sensorSizedFrame;
			double resizeMs = 0.0;

			if (self->m_sensorDisplayConfigReady &&
				(width != self->m_sensorDisplayConfig.width || height != self->m_sensorDisplayConfig.height)) {
				const auto resizeBegin = std::chrono::steady_clock::now();
				cv::Mat rawFrame(height, width, CV_8UC3, const_cast<uchar*>(ram_image.p()));
				cv::Mat resizedFrame;
				cv::resize(rawFrame, resizedFrame, cv::Size(self->m_sensorDisplayConfig.width, self->m_sensorDisplayConfig.height));
				frameWidth = self->m_sensorDisplayConfig.width;
				frameHeight = self->m_sensorDisplayConfig.height;
				sensorSizedFrame.resize(static_cast<size_t>(frameWidth) * static_cast<size_t>(frameHeight) * 3u);
				memcpy(sensorSizedFrame.data(), resizedFrame.data, sensorSizedFrame.size());
				frameData = sensorSizedFrame.data();
				resizeMs = std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - resizeBegin).count();
			}

			BYHWICD::DisplayC2cObjTrackingData trackingSnapshot;
			unsigned long long displayFrameIndex = 0;
			IRFrameTelemetry telemetry;
			{
				std::lock_guard<std::mutex> lock(self->m_mtx);
				trackingSnapshot = self->m_realTimeSceneData;
				telemetry = self->m_currentFrameTelemetry;
				displayFrameIndex = telemetry.sourceSeq > 0 ? telemetry.sourceSeq : self->m_stage0DisplayFrameCount;
			}
			telemetry.readbackMs = readbackMs;
			if (self->m_bSyncRenderMode.load() &&
				(telemetry.sourceSeq == 0 || telemetry.sourceSeq == self->m_lastCapturedSourceSeq))
			{
				return AsyncTask::DS_cont;
			}
			self->UpdateStage6AgcFromFrame(
				frameData,
				frameWidth,
				frameHeight,
				telemetry.sourceSeq);
			if (trackingSnapshot.flag != 0x38)
			{
				trackingSnapshot.flag = 0x38;
			}

			const bool annotationEnabled = self->m_sensorParam.realtimeAnnotation && self->m_annotationManager.isEnabled();
			AnnotationFrameRecord annotationSnapshot = self->m_annotationManager.latestRecord();
			if (!annotationEnabled)
			{
				annotationSnapshot.targets.clear();
			}
			if (annotationSnapshot.frameIndex == 0)
			{
				annotationSnapshot.frameIndex = displayFrameIndex;
			}
			annotationSnapshot.simTimeMs = trackingSnapshot.time;
			annotationSnapshot.sensorID = trackingSnapshot.sensorID;
			if (annotationSnapshot.width <= 0)
			{
				annotationSnapshot.width = frameWidth;
			}
			if (annotationSnapshot.height <= 0)
			{
				annotationSnapshot.height = frameHeight;
			}

			// 将同源 final sensor 像素、实时数据、标注快照和帧遥测推送给 TCP 子线程。
			const IRFrameEnqueueResult enqueueResult = self->m_pTcpThread->updateFrame(
				frameData,
				frameWidth,
				frameHeight,
				trackingSnapshot,
				annotationSnapshot,
				annotationEnabled,
				telemetry);
			if (!enqueueResult.accepted)
			{
				return AsyncTask::DS_cont;
			}
			if (self->m_bSyncRenderMode.load())
			{
				self->m_lastCapturedSourceSeq = telemetry.sourceSeq;
				if (enqueueResult.queueWasFull)
				{
					self->m_perfStats.recordSyncOverrun();
				}
			}
			self->m_perfStats.recordCapture(
				readbackMs,
				resizeMs,
				enqueueResult.copyMs,
				enqueueResult.queueDepth);
			++self->m_stage6CaptureLogCounter;
			if (self->m_stage6CaptureLogCounter <= 3 || (self->m_stage6CaptureLogCounter % 120) == 0) {
				std::ostringstream captureLog;
				captureLog << "[Stage6 Capture]"
					<< " frameWidth=" << frameWidth
					<< " frameHeight=" << frameHeight
					<< " tcpWidth=" << frameWidth
					<< " tcpHeight=" << frameHeight
					<< " renderTextureWidth=" << width
					<< " renderTextureHeight=" << height
					<< " source=final_sensor"
					<< " channels=RGB8"
					<< " sourceSeq=" << telemetry.sourceSeq
					<< " readbackMs=" << readbackMs
					<< " resizeMs=" << resizeMs
					<< " frameCopyMs=" << enqueueResult.copyMs
					<< " tcpQueueDepth=" << enqueueResult.queueDepth;
				std::cout << captureLog.str() << std::endl;
			}

			if (self->m_stage5OutputFrameDumpEnabled && self->m_enableStage5RadianceDebug && !self->m_stage5OutputFrameDumpPath.empty()) {
				++self->m_stage5OutputFrameCounter;
				if (self->m_stage5OutputFrameCounter % self->m_stage5OutputFrameDumpEvery == 0) {
					try {
						cv::Mat rawFrame(frameHeight, frameWidth, CV_8UC3, const_cast<uchar*>(frameData));
						cv::Mat flippedFrame;
						cv::flip(rawFrame, flippedFrame, 0);
						cv::Mat bgrFrame;
						cv::cvtColor(flippedFrame, bgrFrame, cv::COLOR_RGB2BGR);
						const std::string tempDumpPath = self->m_stage5OutputFrameDumpPath + ".tmp.png";
						const bool tempWriteOk = cv::imwrite(tempDumpPath, bgrFrame);
						bool finalWriteOk = false;
						if (tempWriteOk) {
							std::remove(self->m_stage5OutputFrameDumpPath.c_str());
							finalWriteOk = (std::rename(tempDumpPath.c_str(), self->m_stage5OutputFrameDumpPath.c_str()) == 0);
							if (!finalWriteOk) {
								std::remove(tempDumpPath.c_str());
							}
						}
						if (finalWriteOk) {
							++self->m_stage5OutputFrameDumpWrites;
							if (self->m_stage5OutputFrameDumpWrites <= 3) {
								std::cout << "[Stage5 OutputCapture] renderTextureDump=OK"
									<< " path=" << self->m_stage5OutputFrameDumpPath
									<< " width=" << frameWidth
									<< " height=" << frameHeight
									<< " writes=" << self->m_stage5OutputFrameDumpWrites
									<< std::endl;
							}
						}
						else if (!self->m_stage5OutputFrameDumpFailureLogged) {
							std::cout << "[Stage5 OutputCapture][WARN] renderTextureDump=FAILED"
								<< " reason=" << (tempWriteOk ? "rename_failed" : "imwrite_failed")
								<< " path=" << self->m_stage5OutputFrameDumpPath
								<< " width=" << frameWidth
								<< " height=" << frameHeight
								<< std::endl;
							self->m_stage5OutputFrameDumpFailureLogged = true;
						}
					}
					catch (const cv::Exception& e) {
						if (!self->m_stage5OutputFrameDumpFailureLogged) {
							std::cout << "[Stage5 OutputCapture][WARN] renderTextureDump=FAILED"
								<< " reason=cv_exception"
								<< " message=" << e.what()
								<< " path=" << self->m_stage5OutputFrameDumpPath
								<< std::endl;
							self->m_stage5OutputFrameDumpFailureLogged = true;
						}
					}
				}
			}
		}
	}
	// 返回 DS_cont 让任务在下一帧继续执行
	return AsyncTask::DoneStatus::DS_cont;
}
