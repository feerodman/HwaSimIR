#pragma once

#include "nodePath.h"

#include "../Common/CommonDefine.h"
#include "../IRSimulation.h"
#include "IRMaterialBandOptics.h"

#include <string>
#include <vector>

struct IRMaterialIdEntry
{
	int materialId;              // 材质ID纹理中的像素编号，按0-255灰度值归一化送入shader
	std::string materialName;    // 对应MaterialDatabase.csv中的真实物理材质名
	std::string semanticName;    // XML中的复合材质语义名，便于日志和后续人工校核
	double effectiveThicknessM;  // Primary_Substrate Thickness，单位 m
	std::string thicknessSource; // model_xml / fallback
	IRBandReflectance bandReflectance;
	bool hasBandOpticsOverride;  // 模型XML显式给出的SWIR/MWIR样片假设，优先于通用材料库
	double nominalTemperatureK;  // <=0 表示沿用平台/环境基准温度
	double engineOnTemperatureK; // <=0 表示不随 engineState 切换
	std::string temperatureSource;

	IRMaterialIdEntry();
};

struct IRSceneMaterialBinding
{
	bool hasMaterialIdTexture;               // 是否成功绑定材质ID纹理
	bool hasMaterialMap;                     // 是否成功读取材质编号到物理材质的映射
	std::string displayName;                 // 当前绑定的平台资产名
	std::string defaultMaterialName;         // 映射失败时使用的默认材质名
	std::string materialIdTexturePath;       // 材质ID纹理路径
	std::string materialMapPath;             // 材质映射XML/CSV路径
	std::vector<IRMaterialIdEntry> entries;  // 解析出的材质编号表
	int transmissiveMaterialCount;           // SWIR/MWIR tau>0 的显式材质数
	int transmissiveNodeCount;               // 已启用物理预乘透明合成的独立几何节点数
	bool transmissionCompositeReady;         // 每个透射材质均有可排序的 tagged 独立几何

	IRSceneMaterialBinding();
	std::string primaryMaterialName() const; // CPU辐亮度模型暂用的主材质回退值
};

class IRSceneMaterialMapper
{
public:
	// 将基础目标资产的材质ID纹理、材质表和物理参数数组绑定到Panda3D节点
	IRSceneMaterialBinding bindPlatformNode(NodePath& node, const PlatformResPath& res,
		const IRMaterialDatabase& materialDb, const IRMaterialBandOptics& bandOptics,
		double fallbackThicknessM) const;

private:
	bool parseCompositeMaterialXml(const std::string& filePath, const IRMaterialDatabase& materialDb,
		const IRMaterialBandOptics& bandOptics, double fallbackThicknessM,
		std::vector<IRMaterialIdEntry>& entries) const;
};
