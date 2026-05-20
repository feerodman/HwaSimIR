#include "IRSimulation.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace
{
const double kPi = 3.14159265358979323846;

std::vector<std::string> splitCsvLine(const std::string& line)
{
	std::vector<std::string> values;
	std::string current;
	std::istringstream stream(line);
	while (std::getline(stream, current, ','))
	{
		values.push_back(current);
	}
	return values;
}

double parseDouble(const std::vector<std::string>& values, size_t index, double fallback)
{
	if (index >= values.size())
	{
		return fallback;
	}
	try
	{
		return std::stod(values[index]);
	}
	catch (...)
	{
		return fallback;
	}
}
}

IRMaterial::IRMaterial()
	: solarAbsorptivity(0.4),
	thermalEmissivity(0.85),
	characteristicLength(0.0),
	specificHeat(0.8),
	conductivity(1.0),
	density(1000.0),
	transmissivity(0.0),
	roughness(0.5)
{
}

IRRuntimeEnvironment::IRRuntimeEnvironment()
	: band(IRBand::MidWaveInfrared),
	airTemperatureC(25.0),
	visibilityMeters(23000.0),
	sunElevationDeg(45.0),
	sunStrength(1.0)
{
}

IRObjectRadianceInput::IRObjectRadianceInput()
	: rangeMeters(500.0),
	engineOn(false),
	damaged(false),
	isCloud(false),
	isSky(false),
	cloudDensity(0.35)
{
}

bool IRMaterialDatabase::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}

	m_materials.clear();
	std::string line;
	bool inRawMaterials = false;
	bool skippedHeader = false;

	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (line[0] == '!')
		{
			inRawMaterials = (line.find("!RawMaterials") == 0);
			skippedHeader = false;
			if (!inRawMaterials && !m_materials.empty())
			{
				break;
			}
			continue;
		}
		if (!inRawMaterials)
		{
			continue;
		}
		if (!skippedHeader)
		{
			skippedHeader = true;
			continue;
		}

		std::vector<std::string> values = splitCsvLine(line);
		if (values.size() < 11 || values[0].empty())
		{
			continue;
		}

		IRMaterial material;
		material.name = values[0];
		material.category = values[1];
		material.solarAbsorptivity = parseDouble(values, 3, material.solarAbsorptivity);
		material.thermalEmissivity = parseDouble(values, 4, material.thermalEmissivity);
		material.characteristicLength = parseDouble(values, 5, material.characteristicLength);
		material.specificHeat = parseDouble(values, 6, material.specificHeat);
		material.conductivity = parseDouble(values, 7, material.conductivity);
		material.density = parseDouble(values, 8, material.density);
		material.transmissivity = parseDouble(values, 9, material.transmissivity);
		material.roughness = parseDouble(values, 10, material.roughness);
		m_materials[material.name] = material;
	}

	if (m_materials.find("BM_METAL-ALUMINIUM") != m_materials.end())
	{
		m_defaultMaterial = m_materials["BM_METAL-ALUMINIUM"];
	}
	return !m_materials.empty();
}

bool IRMaterialDatabase::empty() const
{
	return m_materials.empty();
}

const IRMaterial& IRMaterialDatabase::get(const std::string& name) const
{
	std::map<std::string, IRMaterial>::const_iterator it = m_materials.find(name);
	if (it != m_materials.end())
	{
		return it->second;
	}
	return m_defaultMaterial;
}

const IRMaterial& IRMaterialDatabase::defaultMaterial() const
{
	return m_defaultMaterial;
}

bool IRAtmosphereModel::loadTransmissionTable(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}

	m_samples.clear();
	m_referencePathMeters = 500.0;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (line.find("Reference_Path_Length") != std::string::npos)
		{
			size_t colon = line.find(':');
			if (colon != std::string::npos)
			{
				try
				{
					m_referencePathMeters = std::stod(line.substr(colon + 1));
				}
				catch (...)
				{
					m_referencePathMeters = 500.0;
				}
			}
			continue;
		}

		std::istringstream stream(line);
		IRAtmosphereSample sample;
		if (stream >> sample.wavelengthUm >> sample.transmittance)
		{
			sample.transmittance = std::max(0.0, std::min(1.0, sample.transmittance));
			m_samples.push_back(sample);
		}
	}
	return !m_samples.empty();
}

bool IRAtmosphereModel::empty() const
{
	return m_samples.empty();
}

double IRAtmosphereModel::averageTransmittance(IRBand band) const
{
	if (m_samples.empty())
	{
		return 0.85;
	}

	IRBandRange range = IRRadianceModel::rangeForBand(band);
	double sum = 0.0;
	int count = 0;
	for (size_t i = 0; i < m_samples.size(); ++i)
	{
		if (m_samples[i].wavelengthUm >= range.lowUm && m_samples[i].wavelengthUm <= range.highUm)
		{
			sum += m_samples[i].transmittance;
			++count;
		}
	}
	if (count == 0)
	{
		return 0.75;
	}
	return sum / static_cast<double>(count);
}

double IRAtmosphereModel::transmittanceForRange(IRBand band, double rangeMeters) const
{
	double referenceTau = std::max(0.01, averageTransmittance(band));
	double pathScale = std::max(0.0, rangeMeters) / std::max(1.0, m_referencePathMeters);
	return std::max(0.0, std::min(1.0, std::pow(referenceTau, pathScale)));
}

IRRadianceModel::IRRadianceModel()
	: m_database(nullptr), m_atmosphere(nullptr)
{
}

void IRRadianceModel::setMaterialDatabase(const IRMaterialDatabase* database)
{
	m_database = database;
}

void IRRadianceModel::setAtmosphereModel(const IRAtmosphereModel* atmosphere)
{
	m_atmosphere = atmosphere;
}

void IRRadianceModel::setEnvironment(const IRRuntimeEnvironment& environment)
{
	m_environment = environment;
}

const IRRuntimeEnvironment& IRRadianceModel::environment() const
{
	return m_environment;
}

IRObjectRadianceOutput IRRadianceModel::evaluate(const IRObjectRadianceInput& input) const
{
	const IRMaterial& material = m_database ? m_database->get(input.materialName) : IRMaterial();
	double emissivity = clamp(material.thermalEmissivity, 0.01, 1.0);
	double reflectance = clamp(1.0 - material.solarAbsorptivity - material.transmissivity, 0.02, 0.95);
	double tau = m_atmosphere ? m_atmosphere->transmittanceForRange(m_environment.band, input.rangeMeters) : 0.85;
	double envK = m_environment.airTemperatureC + 273.15;

	double objectTempK = envK + material.solarAbsorptivity * 18.0;
	if (material.category == "Metal")
	{
		objectTempK += 6.0;
	}
	if (input.engineOn)
	{
		objectTempK += (m_environment.band == IRBand::MidWaveInfrared) ? 320.0 : 180.0;
	}
	if (input.damaged)
	{
		objectTempK += 180.0;
	}
	if (input.isSky)
	{
		objectTempK = envK - 28.0;
		emissivity = 0.35;
		reflectance = 0.10;
	}
	if (input.isCloud)
	{
		objectTempK = envK - 12.0 + input.cloudDensity * 8.0;
		emissivity = 0.82;
		reflectance = 0.35;
	}

	double sunAngle = std::sin(clamp(m_environment.sunElevationDeg, 0.0, 90.0) * kPi / 180.0);
	double solar = solarWeight(m_environment.band) * reflectance * m_environment.sunStrength * sunAngle;
	double thermal = thermalWeight(m_environment.band) * emissivity * normalizedBlackbodyRadiance(m_environment.band, objectTempK);
	double sky = (1.0 - tau) * normalizedBlackbodyRadiance(m_environment.band, envK) * 0.30;
	double path = (1.0 - tau) * (0.04 + normalizedBlackbodyRadiance(m_environment.band, envK) * 0.18);
	double base = tau * (solar + thermal) + path + sky * reflectance;

	if (input.isSky)
	{
		base = (m_environment.band == IRBand::MidWaveInfrared) ? 0.05 : 0.20;
	}
	if (input.isCloud)
	{
		base = clamp(base + input.cloudDensity * 0.18, 0.0, 1.0);
	}

	IRObjectRadianceOutput output;
	output.bandIndex = static_cast<float>(m_environment.band);
	output.emissivity = static_cast<float>(emissivity);
	output.reflectance = static_cast<float>(reflectance);
	output.temperatureK = static_cast<float>(objectTempK);
	output.tauUp = static_cast<float>(tau);
	output.pathRadiance = static_cast<float>(path);
	output.skyRadiance = static_cast<float>(sky);
	output.baseRadiance = static_cast<float>(clamp(base, 0.0, 1.5));
	output.solarWeight = static_cast<float>(solarWeight(m_environment.band));
	output.thermalWeight = static_cast<float>(thermalWeight(m_environment.band));
	output.displayGain = (m_environment.band == IRBand::MidWaveInfrared) ? 0.85f : 1.10f;
	output.displayOffset = 0.02f;
	return output;
}

IRBand IRRadianceModel::bandFromProtocol(int protocolBand)
{
	switch (protocolBand)
	{
	case 1: return IRBand::NearInfrared;
	case 0: return IRBand::ShortWaveInfrared;
	case 2: return IRBand::MidWaveInfrared;
	case 3: return IRBand::LongWaveInfrared;
	case 4: return IRBand::Visible;
	default: return IRBand::MidWaveInfrared;
	}
}

IRBandRange IRRadianceModel::rangeForBand(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return{ band, 0.40, 0.70 };
	case IRBand::NearInfrared: return{ band, 0.70, 1.10 };
	case IRBand::ShortWaveInfrared: return{ band, 1.10, 2.50 };
	case IRBand::MidWaveInfrared: return{ band, 3.00, 5.00 };
	case IRBand::LongWaveInfrared: return{ band, 8.00, 14.00 };
	default: return{ IRBand::MidWaveInfrared, 3.00, 5.00 };
	}
}

const char* IRRadianceModel::bandName(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return "VIS";
	case IRBand::NearInfrared: return "NIR";
	case IRBand::ShortWaveInfrared: return "SWIR";
	case IRBand::MidWaveInfrared: return "MWIR";
	case IRBand::LongWaveInfrared: return "LWIR";
	default: return "MWIR";
	}
}

double IRRadianceModel::normalizedBlackbodyRadiance(IRBand band, double temperatureK) const
{
	IRBandRange range = rangeForBand(band);
	const int samples = 12;
	const double c1 = 1.191042e8;
	const double c2 = 1.4387752e4;
	double sum = 0.0;
	for (int i = 0; i < samples; ++i)
	{
		double t = (static_cast<double>(i) + 0.5) / static_cast<double>(samples);
		double lambda = range.lowUm + (range.highUm - range.lowUm) * t;
		double denominator = std::pow(lambda, 5.0) * (std::exp(c2 / (lambda * temperatureK)) - 1.0);
		if (denominator > 0.0)
		{
			sum += c1 / denominator;
		}
	}

	double bandRadiance = sum / static_cast<double>(samples);
	double referenceTemp = (band == IRBand::MidWaveInfrared) ? 760.0 :
		(band == IRBand::LongWaveInfrared ? 330.0 : 900.0);
	double reference = 0.0;
	for (int i = 0; i < samples; ++i)
	{
		double t = (static_cast<double>(i) + 0.5) / static_cast<double>(samples);
		double lambda = range.lowUm + (range.highUm - range.lowUm) * t;
		double denominator = std::pow(lambda, 5.0) * (std::exp(c2 / (lambda * referenceTemp)) - 1.0);
		if (denominator > 0.0)
		{
			reference += c1 / denominator;
		}
	}
	reference /= static_cast<double>(samples);
	if (reference <= 0.0)
	{
		return 0.0;
	}
	return clamp(bandRadiance / reference, 0.0, 1.5);
}

double IRRadianceModel::solarWeight(IRBand band) const
{
	switch (band)
	{
	case IRBand::Visible: return 0.95;
	case IRBand::NearInfrared: return 0.80;
	case IRBand::ShortWaveInfrared: return 0.55;
	case IRBand::MidWaveInfrared: return 0.14;
	case IRBand::LongWaveInfrared: return 0.02;
	default: return 0.14;
	}
}

double IRRadianceModel::thermalWeight(IRBand band) const
{
	switch (band)
	{
	case IRBand::Visible: return 0.01;
	case IRBand::NearInfrared: return 0.05;
	case IRBand::ShortWaveInfrared: return 0.12;
	case IRBand::MidWaveInfrared: return 0.86;
	case IRBand::LongWaveInfrared: return 0.98;
	default: return 0.86;
	}
}

double IRRadianceModel::clamp(double value, double low, double high) const
{
	return std::max(low, std::min(high, value));
}
