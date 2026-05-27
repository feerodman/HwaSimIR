#include "IRSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace
{
const double kPi = 3.14159265358979323846;

std::string trim(const std::string& value)
{
	size_t begin = value.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
	{
		return std::string();
	}
	size_t end = value.find_last_not_of(" \t\r\n");
	return value.substr(begin, end - begin + 1);
}

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

IRWeatherSample::IRWeatherSample()
	: hour(12.0),
	airTemperatureC(25.0),
	sunAzimuthDeg(180.0),
	sunElevationDeg(45.0)
{
}

IRWeatherMetadata::IRWeatherMetadata()
	: latitudeDeg(0.0),
	longitudeDeg(0.0),
	maximumSolarRadiation(1000.0),
	minTemperatureC(25.0),
	maxTemperatureC(25.0),
	meanEarthTemperatureC(25.0)
{
}

IRRuntimeEnvironment::IRRuntimeEnvironment()
	: band(IRBand::MidWaveInfrared),
	airTemperatureC(25.0),
	visibilityMeters(23000.0),
	humidityPercent(40.0),
	windSpeedMps(0.0),
	windDirectionDeg(0.0),
	sunElevationDeg(45.0),
	sunAzimuthDeg(180.0),
	sunStrength(1.0),
	simulationHour(12.0),
	weatherCode(0)
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

bool IRMaterialDatabase::contains(const std::string& name) const
{
	return m_materials.find(name) != m_materials.end();
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
	return transmittanceForRange(band, rangeMeters, 23000.0);
}

double IRAtmosphereModel::transmittanceForRange(IRBand band, double rangeMeters, double visibilityMeters) const
{
	double referenceTau = std::max(0.01, averageTransmittance(band));
	double visibilityScale = std::max(0.15, std::min(2.0, visibilityMeters / 23000.0));
	double pathScale = std::max(0.0, rangeMeters) / std::max(1.0, m_referencePathMeters);
	pathScale /= visibilityScale;
	return std::max(0.0, std::min(1.0, std::pow(referenceTau, pathScale)));
}

bool IRWeatherProfile::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}

	m_samples.clear();
	m_loadedPath = filePath;
	std::string line;
	bool inHourlyTable = false;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		std::vector<std::string> values = splitCsvLine(line);
		if (values.empty())
		{
			continue;
		}
		std::string key = trim(values[0]);
		if (key == "Latitude" && values.size() > 1)
		{
			m_metadata.latitudeDeg = parseCoordinate(values[1]);
			continue;
		}
		if (key == "Longitude" && values.size() > 1)
		{
			m_metadata.longitudeDeg = parseCoordinate(values[1]);
			continue;
		}
		if (key == "Date" && values.size() > 1)
		{
			m_metadata.date = trim(values[1]);
			continue;
		}
		if (key == "MaximumSolarRadiation" && values.size() > 1)
		{
			m_metadata.maximumSolarRadiation = parseDouble(values, 1, m_metadata.maximumSolarRadiation);
			continue;
		}
		if (key == "MinTemperatureC" && values.size() >= 4)
		{
			if (std::getline(file, line))
			{
				std::vector<std::string> tempValues = splitCsvLine(line);
				m_metadata.minTemperatureC = parseDouble(tempValues, 0, m_metadata.minTemperatureC);
				m_metadata.maxTemperatureC = parseDouble(tempValues, 1, m_metadata.maxTemperatureC);
				m_metadata.meanEarthTemperatureC = parseDouble(tempValues, 3, m_metadata.meanEarthTemperatureC);
			}
			continue;
		}
		if (key == "Time")
		{
			inHourlyTable = true;
			continue;
		}
		if (inHourlyTable && values.size() >= 4)
		{
			IRWeatherSample sample;
			sample.hour = parseDouble(values, 0, sample.hour);
			sample.airTemperatureC = parseDouble(values, 1, sample.airTemperatureC);
			sample.sunAzimuthDeg = parseDouble(values, 2, sample.sunAzimuthDeg);
			sample.sunElevationDeg = parseDouble(values, 3, sample.sunElevationDeg);
			m_samples.push_back(sample);
		}
	}

	std::sort(m_samples.begin(), m_samples.end(), [](const IRWeatherSample& a, const IRWeatherSample& b) {
		return a.hour < b.hour;
	});
	return !m_samples.empty();
}

bool IRWeatherProfile::empty() const
{
	return m_samples.empty();
}

IRWeatherSample IRWeatherProfile::sampleForHour(double hour) const
{
	if (m_samples.empty())
	{
		return IRWeatherSample();
	}

	double normalizedHour = std::fmod(hour, 24.0);
	if (normalizedHour < 0.0)
	{
		normalizedHour += 24.0;
	}

	const IRWeatherSample* previous = &m_samples.back();
	const IRWeatherSample* next = &m_samples.front();
	for (size_t i = 0; i < m_samples.size(); ++i)
	{
		if (m_samples[i].hour <= normalizedHour)
		{
			previous = &m_samples[i];
		}
		if (m_samples[i].hour >= normalizedHour)
		{
			next = &m_samples[i];
			break;
		}
	}

	double prevHour = previous->hour;
	double nextHour = next->hour;
	if (nextHour < prevHour)
	{
		nextHour += 24.0;
	}
	double queryHour = normalizedHour;
	if (queryHour < prevHour)
	{
		queryHour += 24.0;
	}
	double span = std::max(0.001, nextHour - prevHour);
	double t = std::max(0.0, std::min(1.0, (queryHour - prevHour) / span));

	IRWeatherSample result;
	result.hour = normalizedHour;
	result.airTemperatureC = previous->airTemperatureC + (next->airTemperatureC - previous->airTemperatureC) * t;
	result.sunAzimuthDeg = previous->sunAzimuthDeg + (next->sunAzimuthDeg - previous->sunAzimuthDeg) * t;
	result.sunElevationDeg = previous->sunElevationDeg + (next->sunElevationDeg - previous->sunElevationDeg) * t;
	return result;
}

const IRWeatherMetadata& IRWeatherProfile::metadata() const
{
	return m_metadata;
}

const std::string& IRWeatherProfile::loadedPath() const
{
	return m_loadedPath;
}

double IRWeatherProfile::parseCoordinate(const std::string& value) const
{
	std::string text = trim(value);
	if (text.empty())
	{
		return 0.0;
	}
	double sign = 1.0;
	char hemi = text[0];
	if (hemi == 'S' || hemi == 'W' || hemi == 's' || hemi == 'w')
	{
		sign = -1.0;
	}
	if (hemi == 'N' || hemi == 'E' || hemi == 'S' || hemi == 'W' || hemi == 'n' || hemi == 'e' || hemi == 's' || hemi == 'w')
	{
		text = text.substr(1);
	}
	std::replace(text.begin(), text.end(), ':', ' ');
	std::istringstream stream(text);
	double deg = 0.0;
	double min = 0.0;
	double sec = 0.0;
	stream >> deg >> min >> sec;
	return sign * (deg + min / 60.0 + sec / 3600.0);
}

double IRWeatherProfile::parseClockHour(const std::string& value, double fallback) const
{
	std::string text = trim(value);
	std::replace(text.begin(), text.end(), ':', ' ');
	std::istringstream stream(text);
	double hour = 0.0;
	double minute = 0.0;
	if (stream >> hour >> minute)
	{
		return hour + minute / 60.0;
	}
	return fallback;
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
	double tau = m_atmosphere ? m_atmosphere->transmittanceForRange(m_environment.band, input.rangeMeters, m_environment.visibilityMeters) : 0.85;
	double envK = m_environment.airTemperatureC + 273.15;

	double sunAngle = std::sin(clamp(m_environment.sunElevationDeg, 0.0, 90.0) * kPi / 180.0);
	double windCooling = clamp(m_environment.windSpeedMps, 0.0, 30.0) * 0.18;
	double objectTempK = envK + material.solarAbsorptivity * 18.0 * sunAngle * m_environment.sunStrength - windCooling;
	if (material.category == "Metal")
	{
		objectTempK += 6.0 * sunAngle;
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

	double solar = solarWeight(m_environment.band) * reflectance * m_environment.sunStrength * sunAngle;
	double thermal = thermalWeight(m_environment.band) * emissivity * normalizedBlackbodyRadiance(m_environment.band, objectTempK);
	double humidityBoost = clamp(m_environment.humidityPercent, 0.0, 100.0) / 100.0;
	double sky = (1.0 - tau) * normalizedBlackbodyRadiance(m_environment.band, envK) * (0.22 + 0.18 * humidityBoost);
	double path = (1.0 - tau) * (0.04 + normalizedBlackbodyRadiance(m_environment.band, envK) * (0.16 + 0.12 * humidityBoost));
	double base = tau * (solar + thermal) + path + sky * reflectance;

	if (input.isSky)
	{
		double daytimeSky = clamp(0.08 + 0.45 * sunAngle * m_environment.sunStrength, 0.02, 0.70);
		double thermalSky = clamp(0.03 + (1.0 - tau) * 0.22 + humidityBoost * 0.08, 0.02, 0.35);
		base = (m_environment.band == IRBand::LongWaveInfrared || m_environment.band == IRBand::MidWaveInfrared) ? thermalSky : daytimeSky;
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
	return IRBandFromProtocol(protocolBand);
}

IRBandRange IRRadianceModel::rangeForBand(IRBand band)
{
	return IRDefaultRangeForBand(band);
}

const char* IRRadianceModel::bandName(IRBand band)
{
	return IRBandName(band);
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
