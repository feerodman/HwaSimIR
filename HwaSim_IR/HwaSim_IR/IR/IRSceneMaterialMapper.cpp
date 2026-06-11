#include "IRSceneMaterialMapper.h"

#include "filename.h"
#include "lvecBase2.h"
#include "lvecBase4.h"
#include "pta_LVecBase4.h"
#include "pta_float.h"
#include "samplerState.h"
#include "texture.h"
#include "texturePool.h"
#include "textureStage.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
const int kMaxShaderMaterialParams = 8;

bool FileExistsLocal(const std::string& path)
{
	std::ifstream file(path.c_str(), std::ios::binary);
	return file.good();
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

double ClampLocal(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

LVecBase4f MaterialToShaderParamsLocal(const IRMaterial& material)
{
	double emissivity = ClampLocal(material.thermalEmissivity, 0.01, 1.0);
	double reflectance = ClampLocal(1.0 - material.solarAbsorptivity - material.transmissivity, 0.02, 0.95);
	double solarAbsorptivity = ClampLocal(material.solarAbsorptivity, 0.0, 1.0);
	double roughness = ClampLocal(material.roughness, 0.0, 1.0);
	return LVecBase4f(
		static_cast<float>(emissivity),
		static_cast<float>(reflectance),
		static_cast<float>(solarAbsorptivity),
		static_cast<float>(roughness));
}
}

IRMaterialIdEntry::IRMaterialIdEntry()
	: materialId(0)
{
}

IRSceneMaterialBinding::IRSceneMaterialBinding()
	: hasMaterialIdTexture(false),
	hasMaterialMap(false)
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

IRSceneMaterialBinding IRSceneMaterialMapper::bindPlatformNode(NodePath& node, const PlatformResPath& res, const IRMaterialDatabase& materialDb) const
{
	IRSceneMaterialBinding binding;
	binding.displayName = res.displayName;
	binding.defaultMaterialName = res.defaultMaterialName.empty() ? "BM_METAL-ALUMINIUM" : res.defaultMaterialName;
	binding.materialIdTexturePath = res.materialIdTexturePath;
	binding.materialMapPath = res.materialMapPath;

	if (node.is_empty())
	{
		return binding;
	}

	if (!binding.materialMapPath.empty())
	{
		binding.hasMaterialMap = parseCompositeMaterialXml(binding.materialMapPath, materialDb, binding.entries);
	}

	PTA_float materialIds;
	PTA_LVecBase4f materialParams;
	size_t shaderCount = std::min(binding.entries.size(), static_cast<size_t>(kMaxShaderMaterialParams));
	for (int i = 0; i < kMaxShaderMaterialParams; ++i)
	{
		if (i < static_cast<int>(shaderCount))
		{
			const IRMaterialIdEntry& entry = binding.entries[i];
			const IRMaterial& material = materialDb.get(entry.materialName);
			materialIds.push_back(static_cast<float>(ClampLocal(static_cast<double>(entry.materialId) / 255.0, 0.0, 1.0)));
			materialParams.push_back(MaterialToShaderParamsLocal(material));
		}
		else
		{
			const IRMaterial& material = materialDb.get(binding.defaultMaterialName);
			materialIds.push_back(0.0f);
			materialParams.push_back(MaterialToShaderParamsLocal(material));
		}
	}

	// 先绑定材质参数数组；材质 ID 纹理缺失时 shader 会使用平台默认材质参数。
	node.set_shader_input("u_material_param_count", LVecBase2i(static_cast<int>(shaderCount), 0));
	node.set_shader_input("u_material_ids", materialIds);
	node.set_shader_input("u_material_params", materialParams);

	if (!binding.materialIdTexturePath.empty() && FileExistsLocal(binding.materialIdTexturePath))
	{
		Filename materialIdPath = Filename::from_os_specific(binding.materialIdTexturePath);
		PT(Texture) materialIdTexture = TexturePool::load_texture(materialIdPath);
		if (materialIdTexture != nullptr)
		{
			// 材质编号必须逐像素读取，不能让线性过滤把相邻材质 ID 混合。
			materialIdTexture->set_minfilter(SamplerState::FT_nearest);
			materialIdTexture->set_magfilter(SamplerState::FT_nearest);
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
		<< " default=" << binding.defaultMaterialName
		<< std::endl;

	return binding;
}

bool IRSceneMaterialMapper::parseCompositeMaterialXml(const std::string& filePath, const IRMaterialDatabase& materialDb, std::vector<IRMaterialIdEntry>& entries) const
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
