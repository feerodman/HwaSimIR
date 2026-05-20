#pragma once

#include <map>
#include <string>
#include <vector>

enum class IRBand
{
	Visible = 0,
	NearInfrared = 1,
	ShortWaveInfrared = 2,
	MidWaveInfrared = 3,
	LongWaveInfrared = 4
};

struct IRBandRange
{
	IRBand band;
	double lowUm;
	double highUm;
};

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

struct IRRuntimeEnvironment
{
	IRBand band;
	double airTemperatureC;
	double visibilityMeters;
	double sunElevationDeg;
	double sunStrength;

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
	const IRMaterial& get(const std::string& name) const;
	const IRMaterial& defaultMaterial() const;

private:
	std::map<std::string, IRMaterial> m_materials;
	IRMaterial m_defaultMaterial;
};

class IRAtmosphereModel
{
public:
	bool loadTransmissionTable(const std::string& filePath);
	bool empty() const;
	double averageTransmittance(IRBand band) const;
	double transmittanceForRange(IRBand band, double rangeMeters) const;

private:
	std::vector<IRAtmosphereSample> m_samples;
	double m_referencePathMeters;
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
