#pragma once

#include <map>
#include <string>
#include <vector>

#include "IR/IRModtranTauLut.h"
#include "IR/IRTypes.h"

struct IRMaterial
{
	std::string name;
	std::string category;
	double solarAbsorptivity;
	double thermalEmissivity;
	double characteristicLength;
	double specificHeat;
	double conductivity;
	double density;
	double transmissivity;
	double roughness;

	IRMaterial();
};

struct IRAtmosphereSample
{
	double wavelengthUm;
	double transmittance;
};

struct IRWeatherSample
{
	double hour;             // 当地小时，范围 0-24
	double airTemperatureC;  // 对应小时空气温度
	double sunAzimuthDeg;    // 太阳方位角，来自天气/太阳位置 profile
	double sunElevationDeg;  // 太阳高度角，低于 0 表示太阳在地平线以下

	IRWeatherSample();
};

struct IRWeatherMetadata
{
	double latitudeDeg;             // profile 观测点纬度
	double longitudeDeg;            // profile 观测点经度
	double maximumSolarRadiation;   // profile 记录的最大太阳辐照度
	double minTemperatureC;         // profile 当日最低温度
	double maxTemperatureC;         // profile 当日最高温度
	double meanEarthTemperatureC;   // profile 地表平均温度
	std::string date;               // profile 日期

	IRWeatherMetadata();
};

struct IRRuntimeEnvironment
{
	IRBand band;
	double airTemperatureC;
	double visibilityMeters;
	double humidityPercent;
	double windSpeedMps;
	double windDirectionDeg;
	double sunElevationDeg;
	double sunAzimuthDeg;
	double sunStrength;
	double simulationHour;
	int weatherCode;

	IRRuntimeEnvironment();
};

struct IRObjectRadianceInput
{
	std::string materialName;
	double rangeMeters;
	bool engineOn;
	bool damaged;
	bool isCloud;
	bool isSky;
	double cloudDensity;
	double observerAltitudeMeters;
	double targetAltitudeMeters;
	bool hasObserverAltitude;
	bool hasTargetAltitude;

	IRObjectRadianceInput();
};

struct IRObjectRadianceOutput
{
	float bandIndex;
	float emissivity;
	float reflectance;
	float temperatureK;
	float tauUp;
	float pathRadiance;
	float skyRadiance;
	float baseRadiance;
	float solarWeight;
	float thermalWeight;
	float displayGain;
	float displayOffset;
};

class IRMaterialDatabase
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	bool contains(const std::string& name) const; // 材质映射阶段用于判断 XML 中的材质名是否在数据库内
	const IRMaterial& get(const std::string& name) const;
	const IRMaterial& defaultMaterial() const;

private:
	std::map<std::string, IRMaterial> m_materials;
	IRMaterial m_defaultMaterial;
};

class IRAtmosphereModel
{
public:
	IRAtmosphereModel();

	bool loadTransmissionTable(const std::string& filePath);
	bool loadModtranBandLut(const std::string& filePath);
	bool modtranTauLutLoaded() const;
	void setModtranTauDebugEnabled(bool enabled);
	bool modtranTauDebugEnabled() const;
	void setUseModtranTauForAtmosphere(bool enabled);
	bool useModtranTauForAtmosphere() const;
	bool empty() const;
	double averageTransmittance(IRBand band) const;
	double transmittanceForRange(IRBand band, double rangeMeters) const;
	double transmittanceForRange(IRBand band, double rangeMeters, double visibilityMeters) const; // 阶段3：能见度调制上行透过率
	double transmittanceForRange(const IRModtranTauQuery& query) const; // 阶段3：可选 MODTRAN tau-active 受控实验

private:
	std::vector<IRAtmosphereSample> m_samples;
	double m_referencePathMeters;
	IRModtranTauLut m_modtranTauLut;
	bool m_modtranTauDebugEnabled;
	bool m_useModtranTauForAtmosphere;
};

class IRWeatherProfile
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	IRWeatherSample sampleForHour(double hour) const;
	const IRWeatherMetadata& metadata() const;
	const std::string& loadedPath() const;

private:
	double parseCoordinate(const std::string& value) const;
	double parseClockHour(const std::string& value, double fallback) const;

	std::vector<IRWeatherSample> m_samples;
	IRWeatherMetadata m_metadata;
	std::string m_loadedPath;
};

class IRRadianceModel
{
public:
	IRRadianceModel();

	void setMaterialDatabase(const IRMaterialDatabase* database);
	void setAtmosphereModel(const IRAtmosphereModel* atmosphere);
	void setEnvironment(const IRRuntimeEnvironment& environment);

	const IRRuntimeEnvironment& environment() const;
	IRObjectRadianceOutput evaluate(const IRObjectRadianceInput& input) const;

	static IRBand bandFromProtocol(int protocolBand);
	static IRBandRange rangeForBand(IRBand band);
	static const char* bandName(IRBand band);

private:
	double normalizedBlackbodyRadiance(IRBand band, double temperatureK) const;
	double solarWeight(IRBand band) const;
	double thermalWeight(IRBand band) const;
	double clamp(double value, double low, double high) const;

	const IRMaterialDatabase* m_database;
	const IRAtmosphereModel* m_atmosphere;
	IRRuntimeEnvironment m_environment;
};
