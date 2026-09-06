#include "IRModtranRadianceLut.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace
{
const double kAxisEpsilon = 1.0e-8;

std::string Trim(const std::string& value)
{
	size_t begin = value.find_first_not_of(" \t\r\n\"");
	if (begin == std::string::npos) return std::string();
	size_t end = value.find_last_not_of(" \t\r\n\"");
	return value.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitCsv(const std::string& line)
{
	std::vector<std::string> values;
	std::string current;
	std::istringstream stream(line);
	while (std::getline(stream, current, ',')) values.push_back(Trim(current));
	return values;
}

bool Has(const std::map<std::string, size_t>& columns, const std::string& name)
{
	return columns.find(name) != columns.end();
}

std::string Text(const std::vector<std::string>& values, const std::map<std::string, size_t>& columns, const std::string& name)
{
	std::map<std::string, size_t>::const_iterator it = columns.find(name);
	return it == columns.end() || it->second >= values.size() ? std::string() : values[it->second];
}

double Number(const std::vector<std::string>& values, const std::map<std::string, size_t>& columns,
	const std::string& name, double fallback)
{
	const std::string value = Text(values, columns, name);
	if (value.empty()) return fallback;
	try { return std::stod(value); }
	catch (...) { return fallback; }
}

bool Same(double a, double b)
{
	return std::abs(a - b) <= kAxisEpsilon;
}

std::string MergeProvenance(const std::string& a, const std::string& b)
{
	if (a == b) return a;
	return "interpolated_multiple_cases";
}
}

IRModtranRadianceQuery::IRModtranRadianceQuery()
	: band(IRBand::MidWaveInfrared),
	atmosphereModel("Mid-Latitude Summer"), aerosolModel("Rural"), humidityProfile("default"),
	observerAltKm(10.0), targetAltKm(5.0), rangeKm(10.0), visibilityKm(23.0), solarZenithDeg(45.0)
{
}

IRModtranRadianceResult::IRModtranRadianceResult()
	: valid(false), tauUp(1.0), pathThermalWm2SrUm(0.0), directSolarIrradianceWm2Um(0.0),
	downwardSkyDiffuseIrradianceWm2Um(0.0), pathScatteringRadianceWm2SrUm(0.0),
	pathRadiance(0.0), skyRadiance(0.0), solarIrradiance(0.0),
	radianceUnit("W/(m^2 sr um)"), irradianceUnit("W/(m^2 um)"), responseMode("unknown"),
	interpolationMode("none"), fallbackReason("modtran_si_lut_missing"), fallbackAxis("none"),
	fallbackQuery(0.0), fallbackMin(0.0), fallbackMax(0.0),
	sourceCaseIds("missing"), sourceFiles("missing")
{
}

bool IRModtranRadianceLut::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) return false;
	std::string headerLine;
	if (!std::getline(file, headerLine)) return false;
	const std::vector<std::string> header = SplitCsv(headerLine);
	std::map<std::string, size_t> columns;
	for (size_t i = 0; i < header.size(); ++i) columns[header[i]] = i;
	const char* required[] = {
		"band", "atmosphere_model", "aerosol_model", "humidity_profile", "visibility_km",
		"observer_alt_km", "target_alt_km", "range_km", "solar_zenith_deg", "tau_up",
		"path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
		"downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um",
		"radiance_unit", "irradiance_unit", "response_mode", "source_case_ids", "source_files"
	};
	for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
	{
		if (!Has(columns, required[i])) return false;
	}

	std::vector<Entry> loaded;
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty()) continue;
		const std::vector<std::string> values = SplitCsv(line);
		Entry entry;
		if (!bandFromName(Text(values, columns, "band"), entry.band)) continue;
		if (entry.band != IRBand::NearInfrared && entry.band != IRBand::MidWaveInfrared) continue;
		entry.atmosphereModel = Text(values, columns, "atmosphere_model");
		entry.aerosolModel = Text(values, columns, "aerosol_model");
		entry.humidityProfile = Text(values, columns, "humidity_profile");
		entry.visibilityKm = Number(values, columns, "visibility_km", std::numeric_limits<double>::quiet_NaN());
		entry.observerAltKm = Number(values, columns, "observer_alt_km", std::numeric_limits<double>::quiet_NaN());
		entry.targetAltKm = Number(values, columns, "target_alt_km", std::numeric_limits<double>::quiet_NaN());
		entry.rangeKm = Number(values, columns, "range_km", std::numeric_limits<double>::quiet_NaN());
		entry.solarZenithDeg = Number(values, columns, "solar_zenith_deg", 0.0);
		entry.tauUp = Number(values, columns, "tau_up", std::numeric_limits<double>::quiet_NaN());
		entry.pathThermal = Number(values, columns, "path_thermal_W_m2_sr_um", 0.0);
		entry.directSolar = Number(values, columns, "direct_solar_irradiance_at_target_W_m2_um", 0.0);
		entry.skyDiffuse = Number(values, columns, "downward_sky_diffuse_irradiance_W_m2_um", 0.0);
		entry.pathScattering = Number(values, columns, "los_path_scattering_radiance_W_m2_sr_um", 0.0);
		entry.responseMode = Text(values, columns, "response_mode");
		entry.sourceCaseIds = Text(values, columns, "source_case_ids");
		entry.sourceFiles = Text(values, columns, "source_files");
		const bool unitsOk = Text(values, columns, "radiance_unit") == "W/(m^2 sr um)" &&
			Text(values, columns, "irradiance_unit") == "W/(m^2 um)";
		const bool numericOk = std::isfinite(entry.visibilityKm) && entry.visibilityKm > 0.0 &&
			std::isfinite(entry.observerAltKm) && std::isfinite(entry.targetAltKm) &&
			std::isfinite(entry.rangeKm) && entry.rangeKm > 0.0 && std::isfinite(entry.solarZenithDeg) &&
			std::isfinite(entry.tauUp) && entry.tauUp > 0.0 && entry.tauUp <= 1.0 &&
			std::isfinite(entry.pathThermal) && entry.pathThermal >= 0.0 &&
			std::isfinite(entry.directSolar) && entry.directSolar >= 0.0 &&
			std::isfinite(entry.skyDiffuse) && entry.skyDiffuse >= 0.0 &&
			std::isfinite(entry.pathScattering) && entry.pathScattering >= 0.0;
		const bool componentsOk = entry.band == IRBand::MidWaveInfrared
			? !Text(values, columns, "path_thermal_W_m2_sr_um").empty()
			: !Text(values, columns, "direct_solar_irradiance_at_target_W_m2_um").empty() &&
			  !Text(values, columns, "downward_sky_diffuse_irradiance_W_m2_um").empty() &&
			  !Text(values, columns, "los_path_scattering_radiance_W_m2_sr_um").empty();
		if (!unitsOk || !numericOk || !componentsOk || entry.responseMode.empty()) continue;
		entry.tauUp = clampTau(entry.tauUp);
		loaded.push_back(entry);
	}
	if (loaded.empty()) return false;
	m_entries.swap(loaded);
	m_loadedPath = filePath;
	return true;
}

bool IRModtranRadianceLut::empty() const { return m_entries.empty(); }
const std::string& IRModtranRadianceLut::loadedPath() const { return m_loadedPath; }
size_t IRModtranRadianceLut::entryCount() const { return m_entries.size(); }

IRModtranRadianceResult IRModtranRadianceLut::query(const IRModtranRadianceQuery& query) const
{
	IRModtranRadianceResult result;
	if (m_entries.empty()) return result;
	if (!std::isfinite(query.observerAltKm) || !std::isfinite(query.targetAltKm) ||
		!std::isfinite(query.rangeKm) || !std::isfinite(query.visibilityKm) ||
		!std::isfinite(query.solarZenithDeg) || query.rangeKm <= 0.0 || query.visibilityKm <= 0.0)
	{
		result.fallbackReason = "invalid_query";
		return result;
	}
	if (query.rangeKm + kAxisEpsilon < std::abs(query.observerAltKm - query.targetAltKm))
	{
		result.fallbackReason = "invalid_geometry";
		return result;
	}

	std::vector<const Entry*> candidates;
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		const Entry& entry = m_entries[i];
		if (entry.band == query.band && entry.atmosphereModel == query.atmosphereModel &&
			entry.aerosolModel == query.aerosolModel && entry.humidityProfile == query.humidityProfile)
		{
			candidates.push_back(&entry);
		}
	}
	if (candidates.empty())
	{
		result.fallbackReason = "category_missing";
		return result;
	}

	const bool nir = query.band == IRBand::NearInfrared;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		const Entry& e = *candidates[i];
		if (Same(e.targetAltKm, query.targetAltKm) && Same(e.observerAltKm, query.observerAltKm) &&
			Same(e.rangeKm, query.rangeKm) && Same(e.visibilityKm, query.visibilityKm) &&
			(!nir || Same(e.solarZenithDeg, query.solarZenithDeg)))
		{
			result.valid = true;
			result.tauUp = e.tauUp;
			result.pathThermalWm2SrUm = e.pathThermal;
			result.directSolarIrradianceWm2Um = e.directSolar;
			result.downwardSkyDiffuseIrradianceWm2Um = e.skyDiffuse;
			result.pathScatteringRadianceWm2SrUm = e.pathScattering;
			result.pathRadiance = nir ? e.pathScattering : e.pathThermal;
			result.solarIrradiance = e.directSolar;
			result.responseMode = e.responseMode;
			result.interpolationMode = "exact_match";
			result.fallbackReason = "none";
			result.sourceCaseIds = e.sourceCaseIds;
			result.sourceFiles = e.sourceFiles;
			return result;
		}
	}

	Sample sample;
	InterpolationError error;
	if (!interpolate(candidates, query, 0, sample, error))
	{
		result.fallbackReason = error.reason;
		result.fallbackAxis = error.axis;
		result.fallbackQuery = error.query;
		result.fallbackMin = error.minimum;
		result.fallbackMax = error.maximum;
		return result;
	}
	result.valid = true;
	result.tauUp = std::exp(-sample.opticalDepth);
	result.pathThermalWm2SrUm = sample.pathThermal;
	result.directSolarIrradianceWm2Um = sample.directSolar;
	result.downwardSkyDiffuseIrradianceWm2Um = sample.skyDiffuse;
	result.pathScatteringRadianceWm2SrUm = sample.pathScattering;
	result.pathRadiance = nir ? sample.pathScattering : sample.pathThermal;
	result.solarIrradiance = sample.directSolar;
	result.responseMode = "RectangularBand";
	result.interpolationMode = nir
		? "staged_linear_target_observer_range_visibility_solarZenith_tau_od"
		: "staged_linear_target_observer_range_visibility_tau_od";
	result.fallbackReason = "none";
	result.sourceCaseIds = sample.sourceCaseIds;
	result.sourceFiles = sample.sourceFiles;
	return result;
}

bool IRModtranRadianceLut::interpolate(const std::vector<const Entry*>& entries,
	const IRModtranRadianceQuery& query, size_t axisIndex, Sample& sample, InterpolationError& error) const
{
	const bool nir = query.band == IRBand::NearInfrared;
	const size_t axisCount = nir ? 5 : 4;
	if (axisIndex >= axisCount)
	{
		if (entries.size() != 1)
		{
			error.reason = "cell_missing_or_duplicate";
			error.axis = "cell";
			return false;
		}
		const Entry& e = *entries[0];
		sample.opticalDepth = -std::log(clampTau(e.tauUp));
		sample.pathThermal = e.pathThermal;
		sample.directSolar = e.directSolar;
		sample.skyDiffuse = e.skyDiffuse;
		sample.pathScattering = e.pathScattering;
		sample.sourceCaseIds = e.sourceCaseIds;
		sample.sourceFiles = e.sourceFiles;
		return true;
	}

	std::vector<double> values;
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const double value = axisValue(*entries[i], axisIndex, nir);
		if (std::find_if(values.begin(), values.end(), [value](double x) { return Same(x, value); }) == values.end())
			values.push_back(value);
	}
	std::sort(values.begin(), values.end());
	const double requested = queryAxisValue(query, axisIndex, nir);
	if (values.empty() || requested < values.front() - kAxisEpsilon || requested > values.back() + kAxisEpsilon)
	{
		error.reason = "out_of_range";
		error.axis = axisName(axisIndex, nir);
		error.query = requested;
		error.minimum = values.empty() ? 0.0 : values.front();
		error.maximum = values.empty() ? 0.0 : values.back();
		return false;
	}
	double low = values.front();
	double high = values.back();
	for (size_t i = 0; i < values.size(); ++i)
	{
		if (Same(values[i], requested)) { low = high = values[i]; break; }
		if (values[i] < requested) low = values[i];
		if (values[i] > requested) { high = values[i]; break; }
	}
	auto subset = [&](double selected) {
		std::vector<const Entry*> result;
		for (size_t i = 0; i < entries.size(); ++i)
			if (Same(axisValue(*entries[i], axisIndex, nir), selected)) result.push_back(entries[i]);
		return result;
	};
	if (Same(low, high)) return interpolate(subset(low), query, axisIndex + 1, sample, error);
	Sample lowSample, highSample;
	InterpolationError lowError, highError;
	if (!interpolate(subset(low), query, axisIndex + 1, lowSample, lowError))
	{
		error = lowError;
		if (error.reason.empty()) { error.reason = "cell_missing"; error.axis = axisName(axisIndex, nir); }
		return false;
	}
	if (!interpolate(subset(high), query, axisIndex + 1, highSample, highError))
	{
		error = highError;
		if (error.reason.empty()) { error.reason = "cell_missing"; error.axis = axisName(axisIndex, nir); }
		return false;
	}
	sample = mix(lowSample, highSample, (requested - low) / (high - low));
	return true;
}

IRModtranRadianceLut::Sample IRModtranRadianceLut::mix(const Sample& low, const Sample& high, double t)
{
	Sample result;
	result.opticalDepth = low.opticalDepth + (high.opticalDepth - low.opticalDepth) * t;
	result.pathThermal = low.pathThermal + (high.pathThermal - low.pathThermal) * t;
	result.directSolar = low.directSolar + (high.directSolar - low.directSolar) * t;
	result.skyDiffuse = low.skyDiffuse + (high.skyDiffuse - low.skyDiffuse) * t;
	result.pathScattering = low.pathScattering + (high.pathScattering - low.pathScattering) * t;
	result.sourceCaseIds = MergeProvenance(low.sourceCaseIds, high.sourceCaseIds);
	result.sourceFiles = MergeProvenance(low.sourceFiles, high.sourceFiles);
	return result;
}

double IRModtranRadianceLut::axisValue(const Entry& entry, size_t axisIndex, bool nir)
{
	(void)nir;
	switch (axisIndex)
	{
	case 0: return entry.targetAltKm;
	case 1: return entry.observerAltKm;
	case 2: return entry.rangeKm;
	case 3: return entry.visibilityKm;
	default: return entry.solarZenithDeg;
	}
}

double IRModtranRadianceLut::queryAxisValue(const IRModtranRadianceQuery& query, size_t axisIndex, bool nir)
{
	(void)nir;
	switch (axisIndex)
	{
	case 0: return query.targetAltKm;
	case 1: return query.observerAltKm;
	case 2: return query.rangeKm;
	case 3: return query.visibilityKm;
	default: return query.solarZenithDeg;
	}
}

const char* IRModtranRadianceLut::axisName(size_t axisIndex, bool nir)
{
	(void)nir;
	switch (axisIndex)
	{
	case 0: return "targetAltKm";
	case 1: return "observerAltKm";
	case 2: return "rangeKm";
	case 3: return "visibilityKm";
	default: return "solarZenithDeg";
	}
}

bool IRModtranRadianceLut::bandFromName(const std::string& value, IRBand& band)
{
	if (value == "NIR") { band = IRBand::NearInfrared; return true; }
	if (value == "MWIR") { band = IRBand::MidWaveInfrared; return true; }
	return false;
}

double IRModtranRadianceLut::clampTau(double value)
{
	if (!std::isfinite(value)) return 1.0e-12;
	return std::max(1.0e-12, std::min(1.0, value));
}
