#include "IRSceneMaterialMapper.h"

#include "filename.h"
#include "lvecBase2.h"
#include "lvecBase4.h"
#include "nodePathCollection.h"
#include "pta_LVecBase4.h"
#include "pta_float.h"
#include "samplerState.h"
#include "texture.h"
#include "texturePool.h"
#include "textureStage.h"
#include "transparencyAttrib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>

namespace
{
const int kMaxShaderMaterialParams = 8;

unsigned char CategoricalMode2x2(unsigned char a, unsigned char b,
	unsigned char c, unsigned char d)
{
	const std::array<unsigned char, 4> values = { a, b, c, d };
	unsigned char best = values[0];
	int bestCount = 0;
	for (size_t i = 0; i < values.size(); ++i)
	{
		int count = 0;
		for (size_t j = 0; j < values.size(); ++j)
		{
			if (values[j] == values[i]) ++count;
		}
		// Deterministic numeric tie-break affects only a mathematically equal
		// 2x2 boundary footprint; it never invents a material ID.
		if (count > bestCount || (count == bestCount && values[i] < best))
		{
			best = values[i];
			bestCount = count;
		}
	}
	return best;
}

int ConfigureCategoricalMaterialIdMipmaps(const PT(Texture)& texture)
{
	static std::set<const Texture*> prepared;
	if (texture == nullptr) return 0;
	texture->set_magfilter(SamplerState::FT_nearest);
	texture->set_anisotropic_degree(1);
	if (!prepared.insert(texture.p()).second)
	{
		texture->set_minfilter(texture->get_num_ram_mipmap_images() > 1
			? SamplerState::FT_nearest_mipmap_nearest : SamplerState::FT_nearest);
		return texture->get_num_ram_mipmap_images();
	}

	if (!texture->has_ram_image() || texture->get_num_components() != 1 ||
		texture->get_component_width() != 1)
	{
		texture->set_minfilter(SamplerState::FT_nearest);
		return texture->get_num_ram_mipmap_images();
	}

	int width = texture->get_x_size();
	int height = texture->get_y_size();
	CPTA_uchar base = texture->get_ram_mipmap_image(0);
	if (width <= 0 || height <= 0 || base.size() < static_cast<size_t>(width * height))
	{
		texture->set_minfilter(SamplerState::FT_nearest);
		return texture->get_num_ram_mipmap_images();
	}
	std::vector<unsigned char> current(base.begin(), base.begin() + width * height);
	int level = 1;
	while (width > 1 || height > 1)
	{
		const int nextWidth = std::max(1, width / 2);
		const int nextHeight = std::max(1, height / 2);
		PTA_uchar mip = PTA_uchar::empty_array(static_cast<size_t>(nextWidth * nextHeight));
		for (int y = 0; y < nextHeight; ++y)
		{
			const int y0 = std::min(height - 1, y * 2);
			const int y1 = std::min(height - 1, y0 + 1);
			for (int x = 0; x < nextWidth; ++x)
			{
				const int x0 = std::min(width - 1, x * 2);
				const int x1 = std::min(width - 1, x0 + 1);
				mip[y * nextWidth + x] = CategoricalMode2x2(
					current[y0 * width + x0], current[y0 * width + x1],
					current[y1 * width + x0], current[y1 * width + x1]);
			}
		}
		texture->set_ram_mipmap_image(level, mip);
		current.assign(mip.begin(), mip.end());
		width = nextWidth;
		height = nextHeight;
		++level;
	}
	texture->set_minfilter(SamplerState::FT_nearest_mipmap_nearest);
	return texture->get_num_ram_mipmap_images();
}

bool FileExistsLocal(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	return file.good();
}

std::string EnvironmentValueLocal(const char* name)
{
#if defined(_WIN32)
	char* value = nullptr;
	size_t length = 0;
	if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
	{
		return std::string();
	}
	const std::string result(value);
	std::free(value);
	return result;
#else
	const char* value = std::getenv(name);
	return value != nullptr ? std::string(value) : std::string();
#endif
}

std::string ReadTextFileLocal(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		return std::string();
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

std::string TrimLocal(const std::string& value)
{
	size_t begin = value.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
	{
		return std::string();
	}
	size_t end = value.find_last_not_of(" \t\r\n");
	return value.substr(begin, end - begin + 1);
}

std::string ExtractAttributeLocal(const std::string& tagText, const std::string& name)
{
	std::string token = name + "=\"";
	size_t begin = tagText.find(token);
	if (begin == std::string::npos)
	{
		return std::string();
	}
	begin += token.size();
	size_t end = tagText.find('"', begin);
	if (end == std::string::npos)
	{
		return std::string();
	}
	return tagText.substr(begin, end - begin);
}

std::string ExtractTagValueLocal(const std::string& text, const std::string& tagName)
{
	std::string openTag = "<" + tagName + ">";
	std::string closeTag = "</" + tagName + ">";
	size_t begin = text.find(openTag);
	if (begin == std::string::npos)
	{
		return std::string();
	}
	begin += openTag.size();
	size_t end = text.find(closeTag, begin);
	if (end == std::string::npos)
	{
		return std::string();
	}
	return TrimLocal(text.substr(begin, end - begin));
}

std::string ExtractSectionLocal(const std::string& text, const std::string& sectionName)
{
	std::string openTag = "<" + sectionName;
	std::string closeTag = "</" + sectionName + ">";
	size_t begin = text.find(openTag);
	if (begin == std::string::npos)
	{
		return std::string();
	}
	size_t contentBegin = text.find('>', begin);
	if (contentBegin == std::string::npos)
	{
		return std::string();
	}
	size_t end = text.find(closeTag, contentBegin + 1);
	if (end == std::string::npos)
	{
		return std::string();
	}
	end += closeTag.size();
	return text.substr(begin, end - begin);
}

std::string ExtractPreferredMaterialNameLocal(const std::string& compositeBlock)
{
	// 红外辐射由外表面决定；XML 若包含 Surface_Substrate，则优先使用涂层/玻璃等表层材质。
	std::string surface = ExtractSectionLocal(compositeBlock, "Surface_Substrate");
	if (!surface.empty())
	{
		std::string surfaceName = ExtractTagValueLocal(surface, "Name");
		if (!surfaceName.empty())
		{
			return surfaceName;
		}
	}

	std::string primary = ExtractSectionLocal(compositeBlock, "Primary_Substrate");
	if (!primary.empty())
	{
		std::string primaryName = ExtractTagValueLocal(primary, "Name");
		if (!primaryName.empty())
		{
			return primaryName;
		}
	}

	return ExtractTagValueLocal(compositeBlock, "Name");
}

double ExtractEffectiveThicknessLocal(const std::string& compositeBlock, double fallback)
{
	const std::string primary = ExtractSectionLocal(compositeBlock, "Primary_Substrate");
	const std::string text = primary.empty() ? std::string() : ExtractTagValueLocal(primary, "Thickness");
	if (text.empty()) return fallback;
	try
	{
		const double value = std::stod(text);
		return value > 1.0e-6 ? value : fallback;
	}
	catch (...) { return fallback; }
}

double ClampLocal(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

bool ParseOptionalDoubleLocal(const std::string& block, const std::string& tagName, double& value)
{
	const std::string text = ExtractTagValueLocal(block, tagName);
	if (text.empty()) return false;
	size_t consumed = 0;
	double parsed = 0.0;
	try
	{
		parsed = std::stod(text, &consumed);
	}
	catch (...)
	{
		throw std::runtime_error("Invalid numeric <" + tagName + "> value: " + text);
	}
	if (consumed != text.size() || !std::isfinite(parsed))
	{
		throw std::runtime_error("Invalid finite <" + tagName + "> value: " + text);
	}
	value = parsed;
	return true;
}

void ApplyExplicitBandOpticsLocal(const std::string& block, IRMaterialIdEntry& entry)
{
	double swirR = 0.0, swirE = 0.0, swirT = 0.0;
	double mwirR = 0.0, mwirE = 0.0, mwirT = 0.0;
	const bool fields[6] = {
		ParseOptionalDoubleLocal(block, "SWIRReflectance", swirR),
		ParseOptionalDoubleLocal(block, "SWIREmissivity", swirE),
		ParseOptionalDoubleLocal(block, "SWIRTransmissivity", swirT),
		ParseOptionalDoubleLocal(block, "MWIRReflectance", mwirR),
		ParseOptionalDoubleLocal(block, "MWIREmissivity", mwirE),
		ParseOptionalDoubleLocal(block, "MWIRTransmissivity", mwirT)
	};
	int present = 0;
	for (bool field : fields) present += field ? 1 : 0;
	if (present == 0) return;
	if (present != 6)
	{
		throw std::runtime_error("Partial SWIR/MWIR material override is forbidden for material ID " +
			std::to_string(entry.materialId));
	}
	const auto inUnitInterval = [](double candidate) { return candidate >= 0.0 && candidate <= 1.0; };
	if (!inUnitInterval(swirR) || !inUnitInterval(swirE) || !inUnitInterval(swirT) ||
		!inUnitInterval(mwirR) || !inUnitInterval(mwirE) || !inUnitInterval(mwirT) ||
		std::fabs(swirR + swirE + swirT - 1.0) > 1.0e-6 ||
		std::fabs(mwirR + mwirE + mwirT - 1.0) > 1.0e-6)
	{
		throw std::runtime_error("Invalid SWIR/MWIR energy balance for material ID " +
			std::to_string(entry.materialId));
	}
	entry.bandReflectance.swir = swirR;
	entry.bandReflectance.swirEmissivity = swirE;
	entry.bandReflectance.swirTransmissivity = swirT;
	entry.bandReflectance.mwir = mwirR;
	entry.bandReflectance.mwirEmissivity = mwirE;
	entry.bandReflectance.mwirTransmissivity = mwirT;
	entry.bandReflectance.swirSource = "model_xml_engineering_assumption";
	entry.bandReflectance.swirEmissivitySource = "model_xml_engineering_assumption";
	entry.bandReflectance.mwirSource = "model_xml_engineering_assumption";
	entry.bandReflectance.mwirEmissivitySource = "model_xml_engineering_assumption";
	entry.hasBandOpticsOverride = true;
}

void ApplyExplicitTemperaturesLocal(const std::string& block, IRMaterialIdEntry& entry)
{
	double nominal = 0.0;
	double engineOn = 0.0;
	const bool hasNominal = ParseOptionalDoubleLocal(block, "NominalTemperatureK", nominal);
	const bool hasEngineOn = ParseOptionalDoubleLocal(block, "EngineOnTemperatureK", engineOn);
	if ((hasNominal && (nominal < 120.0 || nominal > 1500.0)) ||
		(hasEngineOn && (engineOn < 120.0 || engineOn > 2500.0)))
	{
		throw std::runtime_error("Material temperature outside supported Kelvin range for material ID " +
			std::to_string(entry.materialId));
	}
	if (hasEngineOn && !hasNominal)
	{
		throw std::runtime_error("EngineOnTemperatureK requires NominalTemperatureK for material ID " +
			std::to_string(entry.materialId));
	}
	entry.nominalTemperatureK = hasNominal ? nominal : 0.0;
	entry.engineOnTemperatureK = hasEngineOn ? engineOn : 0.0;
	entry.temperatureSource = hasNominal ? "model_xml_engineering_assumption" : "platform_runtime";
}

LVecBase4f MaterialToShaderParamsLocal(const IRMaterial& material, const IRBandReflectance& reflectance)
{
	double emissivity = ClampLocal(material.thermalEmissivity, 0.01, 1.0);
	double transmissivity = ClampLocal(material.transmissivity, 0.0, 1.0);
	double roughness = ClampLocal(material.roughness, 0.0, 1.0);
	return LVecBase4f(
		static_cast<float>(emissivity),
		static_cast<float>(reflectance.nir),
		static_cast<float>(transmissivity),
		static_cast<float>(roughness));
}
}

IRMaterialIdEntry::IRMaterialIdEntry()
	: materialId(0), effectiveThicknessM(0.02), thicknessSource("fallback"),
	hasBandOpticsOverride(false), nominalTemperatureK(0.0), engineOnTemperatureK(0.0),
	temperatureSource("platform_runtime")
{
}

IRSceneMaterialBinding::IRSceneMaterialBinding()
	: hasMaterialIdTexture(false),
	hasMaterialMap(false),
	transmissiveMaterialCount(0),
	transmissiveNodeCount(0),
	transmissionCompositeReady(true)
{
}

std::string IRSceneMaterialBinding::primaryMaterialName() const
{
	if (!entries.empty())
	{
		return entries[0].materialName;
	}
	return defaultMaterialName;
}

IRSceneMaterialBinding IRSceneMaterialMapper::bindPlatformNode(NodePath& node, const PlatformResPath& res,
	const IRMaterialDatabase& materialDb, const IRMaterialBandOptics& bandOptics,
	double fallbackThicknessM) const
{
	IRSceneMaterialBinding binding;
	binding.displayName = res.displayName;
	binding.defaultMaterialName = res.defaultMaterialName.empty() ? "BM_METAL-ALUMINIUM" : res.defaultMaterialName;
	binding.materialIdTexturePath = res.materialIdTexturePath;
	binding.materialMapPath = res.materialMapPath;
	const bool p12ControlledGlassRig = binding.displayName == "P12-CONTROLLED-GLASS-RIG";
	const std::string p12TransmissionMode = EnvironmentValueLocal("P12ControlledGlassTransmission");
	const bool p12TransmissionBlocked = p12ControlledGlassRig && p12TransmissionMode == "Blocked";
	if (p12ControlledGlassRig)
	{
		std::cout << "[P12 ControlledGlassComposite]"
			<< " transmission=" << (p12TransmissionBlocked ? "Blocked" : "On")
			<< " blockedMode=" << (p12TransmissionBlocked ? "declared_nonphysical_ablation" : "none")
			<< " model=single_straight_through"
			<< " calibration=NOT_VERIFIED_CALIBRATION"
			<< std::endl;
	}

	if (node.is_empty())
	{
		return binding;
	}

	if (!binding.materialMapPath.empty())
	{
		binding.hasMaterialMap = parseCompositeMaterialXml(binding.materialMapPath, materialDb,
			bandOptics, fallbackThicknessM, binding.entries);
	}

	PTA_float materialIds;
	PTA_LVecBase4f materialParams;
	PTA_LVecBase4f materialBandReflectance;
	PTA_LVecBase4f materialBandEmissivity;
	PTA_LVecBase4f materialBandTransmissivity;
	PTA_LVecBase4f materialTemperatureK;
	if (binding.entries.size() > static_cast<size_t>(kMaxShaderMaterialParams))
	{
		throw std::runtime_error("Material map exceeds 8 GPU slots; refusing silent truncation: " + binding.materialMapPath);
	}
	std::set<int> uniqueIds;
	for (const auto& entry : binding.entries)
	{
		if (entry.materialId < 0 || entry.materialId > 255 || !uniqueIds.insert(entry.materialId).second)
			throw std::runtime_error("Invalid or duplicate 8-bit material ID: " + binding.materialMapPath);
	}
	size_t shaderCount = binding.entries.size();
	for (int i = 0; i < kMaxShaderMaterialParams; ++i)
	{
		if (i < static_cast<int>(shaderCount))
		{
			const IRMaterialIdEntry& entry = binding.entries[i];
			const IRMaterial& material = materialDb.get(entry.materialName);
			materialIds.push_back(static_cast<float>(ClampLocal(static_cast<double>(entry.materialId) / 255.0, 0.0, 1.0)));
			materialParams.push_back(MaterialToShaderParamsLocal(material, entry.bandReflectance));
			materialBandReflectance.push_back(LVecBase4f(static_cast<float>(entry.bandReflectance.nir),
				static_cast<float>(entry.bandReflectance.mwir), static_cast<float>(entry.bandReflectance.swir), 0.0f));
			materialBandEmissivity.push_back(LVecBase4f(static_cast<float>(material.thermalEmissivity),
				static_cast<float>(entry.bandReflectance.mwirEmissivity),
				static_cast<float>(entry.bandReflectance.swirEmissivity),
				static_cast<float>(material.thermalEmissivity)));
			// x/w remain compatibility-opaque.  P11 material transmission is only
			// calibrated and enabled for the formal SWIR/MWIR bands.
			materialBandTransmissivity.push_back(LVecBase4f(0.0f,
				static_cast<float>(p12TransmissionBlocked ? 0.0 : entry.bandReflectance.mwirTransmissivity),
				static_cast<float>(p12TransmissionBlocked ? 0.0 : entry.bandReflectance.swirTransmissivity), 0.0f));
			materialTemperatureK.push_back(LVecBase4f(
				static_cast<float>(entry.nominalTemperatureK),
				static_cast<float>(entry.engineOnTemperatureK), 0.0f, 0.0f));
		}
		else
		{
			const IRMaterial& material = materialDb.get(binding.defaultMaterialName);
			const IRBandReflectance reflectance = bandOptics.resolve(material);
			materialIds.push_back(0.0f);
			materialParams.push_back(MaterialToShaderParamsLocal(material, reflectance));
			materialBandReflectance.push_back(LVecBase4f(static_cast<float>(reflectance.nir),
				static_cast<float>(reflectance.mwir), static_cast<float>(reflectance.swir), 0.0f));
			materialBandEmissivity.push_back(LVecBase4f(static_cast<float>(material.thermalEmissivity),
				static_cast<float>(reflectance.mwirEmissivity),
				static_cast<float>(reflectance.swirEmissivity),
				static_cast<float>(material.thermalEmissivity)));
			materialBandTransmissivity.push_back(LVecBase4f(0.0f,
				static_cast<float>(reflectance.mwirTransmissivity),
				static_cast<float>(reflectance.swirTransmissivity), 0.0f));
			materialTemperatureK.push_back(LVecBase4f(0.0f));
		}
	}

	// 先绑定材质参数数组；材质 ID 纹理缺失时 shader 会使用平台默认材质参数。
	node.set_shader_input("u_material_param_count", LVecBase2i(static_cast<int>(shaderCount), 0));
	node.set_shader_input("u_material_ids", materialIds);
	node.set_shader_input("u_material_params", materialParams);
	node.set_shader_input("u_material_band_reflectance", materialBandReflectance);
	node.set_shader_input("u_material_band_emissivity", materialBandEmissivity);
	node.set_shader_input("u_material_band_transmissivity", materialBandTransmissivity);
	node.set_shader_input("u_material_temperature_K", materialTemperatureK);
	node.set_shader_input("u_material_transmission_composite_en", LVecBase2i(0, 0));

	// Material-ID lookup alone cannot make only part of a shared draw transparent:
	// blend/depth state belongs to a scene-graph node.  P11 assets therefore mark
	// every transmissive region as an independent Egg Group.  Enable premultiplied
	// physical compositing only on a group whose p11_material_id resolves to an
	// explicitly transmissive SWIR/MWIR entry; unmatched legacy geometry remains
	// opaque instead of silently producing a dark, incomplete contribution.
	for (const auto& entry : binding.entries)
	{
		const bool transmissive = !p12TransmissionBlocked &&
			(entry.bandReflectance.swirTransmissivity > 1.0e-8 ||
			 entry.bandReflectance.mwirTransmissivity > 1.0e-8);
		if (!transmissive) continue;
		++binding.transmissiveMaterialCount;
		const std::string matchPattern = "**/=p11_material_id=" + std::to_string(entry.materialId);
		NodePathCollection matches = node.find_all_matches(matchPattern);
		if (matches.get_num_paths() == 0)
		{
			binding.transmissionCompositeReady = false;
			std::cerr << "[P11 MaterialTransmission][WARN]"
				<< " materialId=" << entry.materialId
				<< " semantic=" << entry.semanticName
				<< " action=fail_closed_opaque"
				<< " reason=transmissive_material_requires_independent_tagged_geometry"
				<< std::endl;
			continue;
		}
		for (int matchIndex = 0; matchIndex < matches.get_num_paths(); ++matchIndex)
		{
			NodePath transmissiveNode = matches.get_path(matchIndex);
			transmissiveNode.set_shader_input("u_material_transmission_composite_en", LVecBase2i(1, 0));
			transmissiveNode.set_transparency(TransparencyAttrib::M_premultiplied_alpha);
			transmissiveNode.set_depth_test(true);
			transmissiveNode.set_depth_write(false);
			transmissiveNode.set_bin("transparent", 20);
			++binding.transmissiveNodeCount;
			std::cout << "[P11 MaterialTransmission]"
				<< " node=" << transmissiveNode.get_name()
				<< " materialId=" << entry.materialId
				<< " swirTau=" << entry.bandReflectance.swirTransmissivity
				<< " mwirTau=" << entry.bandReflectance.mwirTransmissivity
				<< " blend=premultiplied_alpha"
				<< " depthTest=1 depthWrite=0 bin=transparent sort=20"
				<< std::endl;
		}
	}

	if (!binding.materialIdTexturePath.empty() && FileExistsLocal(binding.materialIdTexturePath))
	{
		Filename materialIdPath = Filename::from_os_specific(binding.materialIdTexturePath);
		PT(Texture) materialIdTexture = TexturePool::load_texture(materialIdPath);
		if (materialIdTexture != nullptr)
		{
			// ID data must stay linear even when global color-texture policy changes.
			if (materialIdTexture->get_num_components() == 1)
			{
				// Use the GLES3-native single-channel representation.  The IR shader
				// samples only .r, so this is numerically equivalent to the former
				// legacy luminance format while avoiding a driver-side conversion.
				materialIdTexture->set_format(Texture::F_red);
				std::cout << "[MaterialIdTexture] name=asset"
					<< " path=" << binding.materialIdTexturePath
					<< " format=R8_UNORM components=1"
					<< " reason=gles3_legacy_luminance_unsupported"
					<< std::endl;
			}
			// Linear color mipmaps are invalid for categorical material numbers.
			// Build a mode-reduced R8 chain: every texel remains one source ID,
			// while minification no longer selects a different full-resolution
			// texel on every sub-pixel movement.
			const int materialIdMipLevels = ConfigureCategoricalMaterialIdMipmaps(materialIdTexture);
			std::cout << "[MaterialIdSampling] path=" << binding.materialIdTexturePath
				<< " min=nearest_mipmap_nearest mag=nearest reduction=mode_2x2"
				<< " tieBreak=lowest_existing_id mipLevels=" << materialIdMipLevels
				<< std::endl;
			node.set_shader_input("u_material_id_texture", materialIdTexture);
			// 绑定到第二纹理通道，shader 通过 Panda3D 内置 sampler p3d_Texture1 读取。
			PT(TextureStage) materialIdStage = new TextureStage("material_id_stage");
			materialIdStage->set_sort(1);
			node.set_texture(materialIdStage, materialIdTexture, 1);
			node.set_shader_input("u_material_id_ready", LVecBase2i(1, 0));
			binding.hasMaterialIdTexture = true;
		}
		else
		{
			std::cerr << "[Stage2] 材质ID纹理加载失败：" << binding.materialIdTexturePath << std::endl;
		}
	}
	else if (!binding.materialIdTexturePath.empty())
	{
		std::cerr << "[Stage2] 材质ID纹理不存在：" << binding.materialIdTexturePath << std::endl;
	}

	std::cout << "[Stage2] 材质绑定："
		<< (binding.displayName.empty() ? "UNKNOWN" : binding.displayName)
		<< " materialIdTex=" << (binding.hasMaterialIdTexture ? "OK" : "fallback")
		<< " materialMap=" << (binding.hasMaterialMap ? "OK" : "fallback")
		<< " entries=" << binding.entries.size()
		<< " gpuSlots=" << shaderCount << " capacity=8 truncated=0 sampler=categorical_mode_mip_nearest"
		<< " transmissiveMaterials=" << binding.transmissiveMaterialCount
		<< " transmissiveNodes=" << binding.transmissiveNodeCount
		<< " transmissionCompositeReady=" << (binding.transmissionCompositeReady ? 1 : 0)
		<< " default=" << binding.defaultMaterialName
		<< std::endl;
	for (size_t i = 0; i < binding.entries.size(); ++i)
	{
		const IRMaterialIdEntry& entry = binding.entries[i];
		std::cout << "[L1 MaterialOptics] material=" << entry.materialName
			<< " materialId=" << entry.materialId
			<< " nirReflectance=" << entry.bandReflectance.nir
			<< " nirReflectanceSource=" << entry.bandReflectance.nirSource
			<< " swirReflectance=" << entry.bandReflectance.swir
			<< " swirEmissivity=" << entry.bandReflectance.swirEmissivity
			<< " swirTransmissivity=" << entry.bandReflectance.swirTransmissivity
			<< " swirSource=" << entry.bandReflectance.swirSource
			<< " mwirReflectance=" << entry.bandReflectance.mwir
			<< " mwirEmissivity=" << entry.bandReflectance.mwirEmissivity
			<< " mwirTransmissivity=" << entry.bandReflectance.mwirTransmissivity
			<< " mwirReflectanceSource=" << entry.bandReflectance.mwirSource
			<< " bandOpticsOverride=" << (entry.hasBandOpticsOverride ? 1 : 0)
			<< " nominalTemperatureK=" << entry.nominalTemperatureK
			<< " engineOnTemperatureK=" << entry.engineOnTemperatureK
			<< " temperatureSource=" << entry.temperatureSource
			<< " effectiveThicknessM=" << entry.effectiveThicknessM
			<< " thicknessSource=" << entry.thicknessSource << std::endl;
	}

	return binding;
}

bool IRSceneMaterialMapper::parseCompositeMaterialXml(const std::string& filePath, const IRMaterialDatabase& materialDb,
	const IRMaterialBandOptics& bandOptics, double fallbackThicknessM,
	std::vector<IRMaterialIdEntry>& entries) const
{
	entries.clear();
	if (!FileExistsLocal(filePath))
	{
		std::cerr << "[Stage2] 材质映射XML不存在：" << filePath << std::endl;
		return false;
	}

	std::string text = ReadTextFileLocal(filePath);
	if (text.empty())
	{
		std::cerr << "[Stage2] 材质映射XML为空：" << filePath << std::endl;
		return false;
	}

	size_t pos = 0;
	while (true)
	{
		// 只匹配真实材质项，避免把根节点 Composite_Material_Table 误当成第一个材质。
		size_t begin = text.find("<Composite_Material ", pos);
		if (begin == std::string::npos)
		{
			break;
		}
		size_t tagEnd = text.find('>', begin);
		size_t end = text.find("</Composite_Material>", begin);
		if (tagEnd == std::string::npos || end == std::string::npos)
		{
			break;
		}
		end += std::string("</Composite_Material>").size();

		std::string openTag = text.substr(begin, tagEnd - begin + 1);
		std::string block = text.substr(begin, end - begin);
		std::string indexText = ExtractAttributeLocal(openTag, "index");
		std::string materialName = ExtractPreferredMaterialNameLocal(block);
		std::string semanticName = ExtractTagValueLocal(block, "Name");

		if (!indexText.empty() && !materialName.empty())
		{
			IRMaterialIdEntry entry;
			entry.materialId = std::atoi(indexText.c_str());
			entry.materialName = materialName;
			entry.semanticName = semanticName;
			entry.effectiveThicknessM = ExtractEffectiveThicknessLocal(block, fallbackThicknessM);
			entry.thicknessSource = ExtractSectionLocal(block, "Primary_Substrate").empty() ||
				ExtractTagValueLocal(ExtractSectionLocal(block, "Primary_Substrate"), "Thickness").empty()
				? "fallback" : "model_xml";
			entry.bandReflectance = bandOptics.resolve(materialDb.get(entry.materialName));
			ApplyExplicitBandOpticsLocal(block, entry);
			ApplyExplicitTemperaturesLocal(block, entry);
			if (!materialDb.empty() && !materialDb.contains(entry.materialName))
			{
				std::cerr << "[Stage2] 材质库未找到 " << entry.materialName << "，该ID将使用默认材质参数：" << filePath << std::endl;
			}
			entries.push_back(entry);
		}
		pos = end;
	}

	return !entries.empty();
}
