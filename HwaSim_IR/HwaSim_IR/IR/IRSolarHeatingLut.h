#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct IRSolarHeatingQuery
{
	std::string atmosphereModel = "Mid-Latitude Summer";
	std::string aerosolModel = "Rural";
	std::string humidityProfile = "default";
	double targetAltKm = 5.0;
	double visibilityKm = 23.0;
	double solarZenithDeg = 45.0;
};

struct IRSolarHeatingResult
{
	bool valid = false;
	double directIrradianceWm2 = 0.0;
	double diffuseDownIrradianceWm2 = 0.0;
	std::string irradianceUnit = "W/m^2";
	std::string interpolationMode = "none";
	std::string fallbackReason = "solar_heating_lut_missing";
	std::string fallbackAxis = "none";
	double fallbackQuery = 0.0;
	double fallbackMin = 0.0;
	double fallbackMax = 0.0;
	std::string sourceCaseIds = "missing";
	std::string sourceFiles = "missing";
};

class IRSolarHeatingLut
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	size_t entryCount() const;
	IRSolarHeatingResult query(const IRSolarHeatingQuery& query) const;

private:
	struct Entry
	{
		std::string atmosphereModel;
		std::string aerosolModel;
		std::string humidityProfile;
		double targetAltKm = 0.0;
		double visibilityKm = 0.0;
		double solarZenithDeg = 0.0;
		double direct = 0.0;
		double diffuse = 0.0;
		std::string sourceCaseIds;
		std::string sourceFiles;
	};
	struct Sample { double direct = 0.0; double diffuse = 0.0; std::string cases; std::string files; };
	bool interpolate(const std::vector<const Entry*>& entries, const IRSolarHeatingQuery& query,
		size_t axis, Sample& sample, IRSolarHeatingResult& error) const;
	static double axisValue(const Entry& entry, size_t axis);
	static double queryValue(const IRSolarHeatingQuery& query, size_t axis);
	static const char* axisName(size_t axis);

	std::vector<Entry> m_entries;
	std::string m_loadedPath;
};
