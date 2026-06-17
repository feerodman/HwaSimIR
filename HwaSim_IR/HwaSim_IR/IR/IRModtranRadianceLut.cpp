#include "IRModtranRadianceLut.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace
{
std::string TrimValue(const std::string& value)
{
	size_t begin = value.find_first_not_of(" \t\r\n\"");
	if (begin == std::string::npos)
	{
		return std::string();
	}
	size_t end = value.find_last_not_of(" \t\r\n\"");
	return value.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitCsvSimple(const std::string& line)
{
	std::vector<std::string> values;
	std::string current;
	std::istringstream stream(line);
	while (std::getline(stream, current, ','))
	{
		values.push_back(TrimValue(current));
	}
	return values;
}

double ParseDouble(const std::vector<std::string>& values, size_t index, double fallback)
{
	if (index >= values.size())
	{
		return fallback;
	}
	const std::string& text = values[index];
	if (text.empty())
	{
		return fallback;
	}
	try
	{
		return std::stod(text);
	}
	catch (...)
	{
		return fallback;
	}
}

std::string ColumnValue(const std::vector<std::string>& values, const std::map<std::string, size_t>& columns, const std::string& name)
{
	std::map<std::string, size_t>::const_iterator it = columns.find(name);
	if (it == columns.end() || it->second >= values.size())
	{
		return std::string();
	}
	return values[it->second];
}

bool HasColumn(const std::map<std::string, size_t>& columns, const std::string& name)
{
	return columns.find(name) != columns.end();
}

int BandIndex(IRBand band)
{
	int index = static_cast<int>(band);
	return index >= 0 && index < 5 ? index : 3;
}
}

IRModtranRadianceQuery::IRModtranRadianceQuery()
	: band(IRBand::MidWaveInfrared),
	observerAltKm(10.0),
	targetAltKm(10.0),
	rangeKm(1.0),
	visibilityKm(23.0),
	humidityPercent(50.0),
	solarZenithDeg(45.0)
{
}

IRModtranRadianceResult::IRModtranRadianceResult()
	: valid(false),
	usedNearest(false),
	observerAltKm(0.0),
	targetAltKm(0.0),
	rangeKm(0.0),
	visibilityKm(0.0),
	humidityPercent(0.0),
	tauUp(1.0),
	tauDown(1.0),
	pathRadiance(0.0),
	skyRadiance(0.0),
	solarIrradiance(0.0),
	sourceFile("missing"),
	unitRadiance("unknown"),
	unitIrradiance("unknown"),
	interpolationMode("none"),
	fallbackReason("modtran_radiance_lut_missing")
{
}

bool IRModtranRadianceLut::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open())
	{
		return false;
	}

	std::string headerLine;
	if (!std::getline(file, headerLine))
	{
		return false;
	}

	std::vector<std::string> header = SplitCsvSimple(headerLine);
	std::map<std::string, size_t> columns;
	for (size_t i = 0; i < header.size(); ++i)
	{
		columns[header[i]] = i;
	}

	const char* requiredColumns[] = {
		"band",
		"atmosphere_model",
		"aerosol_model",
		"humidity_profile",
		"visibility_km",
		"observer_alt_km",
		"target_alt_km",
		"range_km",
		"solar_zenith_deg",
		"tau_up_band",
		"tau_down_band",
		"path_radiance_band",
		"sky_radiance_band",
		"solar_irradiance_band"
	};
	for (size_t i = 0; i < sizeof(requiredColumns) / sizeof(requiredColumns[0]); ++i)
	{
		if (!HasColumn(columns, requiredColumns[i]))
		{
			return false;
		}
	}

	std::vector<Entry> entries;
	BandBounds bounds[5];
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}

		std::vector<std::string> values = SplitCsvSimple(line);
		if (ColumnValue(values, columns, "atmosphere_model") != "Mid-Latitude Summer" ||
			ColumnValue(values, columns, "aerosol_model") != "Rural" ||
			ColumnValue(values, columns, "humidity_profile") != "default")
		{
			continue;
		}

		Entry entry;
		if (!bandFromName(ColumnValue(values, columns, "band"), entry.band))
		{
			continue;
		}

		entry.visibilityKm = ParseDouble(values, columns["visibility_km"], std::numeric_limits<double>::quiet_NaN());
		entry.observerAltKm = ParseDouble(values, columns["observer_alt_km"], std::numeric_limits<double>::quiet_NaN());
		entry.targetAltKm = ParseDouble(values, columns["target_alt_km"], std::numeric_limits<double>::quiet_NaN());
		entry.rangeKm = ParseDouble(values, columns["range_km"], std::numeric_limits<double>::quiet_NaN());
		entry.solarZenithDeg = ParseDouble(values, columns["solar_zenith_deg"], std::numeric_limits<double>::quiet_NaN());
		entry.tauUp = ParseDouble(values, columns["tau_up_band"], std::numeric_limits<double>::quiet_NaN());
		entry.tauDown = ParseDouble(values, columns["tau_down_band"], std::numeric_limits<double>::quiet_NaN());
		entry.pathRadiance = ParseDouble(values, columns["path_radiance_band"], 0.0);
		entry.skyRadiance = ParseDouble(values, columns["sky_radiance_band"], 0.0);
		entry.solarIrradiance = ParseDouble(values, columns["solar_irradiance_band"], 0.0);
		entry.unitRadiance = ColumnValue(values, columns, "unit_radiance");
		entry.unitIrradiance = ColumnValue(values, columns, "unit_irradiance");

		if (!std::isfinite(entry.visibilityKm) ||
			!std::isfinite(entry.observerAltKm) ||
			!std::isfinite(entry.targetAltKm) ||
			!std::isfinite(entry.rangeKm) ||
			!std::isfinite(entry.solarZenithDeg) ||
			!std::isfinite(entry.tauUp) ||
			!std::isfinite(entry.tauDown) ||
			!isFiniteNonNegative(entry.pathRadiance) ||
			!isFiniteNonNegative(entry.skyRadiance) ||
			!isFiniteNonNegative(entry.solarIrradiance))
		{
			continue;
		}

		entry.tauUp = clampTau(entry.tauUp);
		entry.tauDown = clampTau(entry.tauDown);
		if (entry.unitRadiance.empty())
		{
			entry.unitRadiance = "MODOUT2_native";
		}
		if (entry.unitIrradiance.empty())
		{
			entry.unitIrradiance = "MODOUT2_native";
		}
		updateBounds(bounds[BandIndex(entry.band)], entry);
		entries.push_back(entry);
	}

	if (entries.empty())
	{
		return false;
	}

	m_entries.swap(entries);
	for (int i = 0; i < 5; ++i)
	{
		m_bounds[i] = bounds[i];
	}
	m_loadedPath = filePath;
	return true;
}

bool IRModtranRadianceLut::empty() const
{
	return m_entries.empty();
}

const std::string& IRModtranRadianceLut::loadedPath() const
{
	return m_loadedPath;
}

IRModtranRadianceResult IRModtranRadianceLut::query(const IRModtranRadianceQuery& query) const
{
	IRModtranRadianceResult result;
	result.observerAltKm = query.observerAltKm;
	result.targetAltKm = query.targetAltKm;
	result.rangeKm = query.rangeKm;
	result.visibilityKm = query.visibilityKm;
	result.humidityPercent = query.humidityPercent;
	result.sourceFile = m_loadedPath.empty() ? "missing" : m_loadedPath;
	if (m_entries.empty())
	{
		return result;
	}
	if (!std::isfinite(query.rangeKm) || !std::isfinite(query.observerAltKm) ||
		!std::isfinite(query.targetAltKm) || !std::isfinite(query.visibilityKm) ||
		query.rangeKm <= 0.0 || query.visibilityKm <= 0.0)
	{
		result.fallbackReason = "invalid_query";
		return result;
	}
	if (query.rangeKm + 1.0e-6 < std::abs(query.observerAltKm - query.targetAltKm))
	{
		result.fallbackReason = "invalid_geometry";
		return result;
	}

	const BandBounds* bounds = boundsForBand(query.band);
	if (bounds == nullptr || !bounds->valid)
	{
		result.fallbackReason = "missing_band";
		return result;
	}
	if (query.observerAltKm < bounds->minObserverAltKm || query.observerAltKm > bounds->maxObserverAltKm ||
		query.targetAltKm < bounds->minTargetAltKm || query.targetAltKm > bounds->maxTargetAltKm ||
		query.rangeKm < bounds->minRangeKm || query.rangeKm > bounds->maxRangeKm ||
		query.visibilityKm < bounds->minVisibilityKm || query.visibilityKm > bounds->maxVisibilityKm)
	{
		result.fallbackReason = "out_of_lut_range";
		return result;
	}

	const Entry* best = nullptr;
	double bestDistance = std::numeric_limits<double>::max();
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		const Entry& entry = m_entries[i];
		if (entry.band != query.band)
		{
			continue;
		}
		const double distance = normalizedDistance(entry, query);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = &entry;
		}
	}

	if (best == nullptr)
	{
		result.fallbackReason = "missing_band";
		return result;
	}

	result.valid = true;
	result.usedNearest = bestDistance > 1.0e-10;
	result.observerAltKm = best->observerAltKm;
	result.targetAltKm = best->targetAltKm;
	result.rangeKm = best->rangeKm;
	result.visibilityKm = best->visibilityKm;
	result.tauUp = best->tauUp;
	result.tauDown = best->tauDown;
	result.pathRadiance = best->pathRadiance;
	result.skyRadiance = best->skyRadiance;
	result.solarIrradiance = best->solarIrradiance;
	result.unitRadiance = best->unitRadiance;
	result.unitIrradiance = best->unitIrradiance;
	result.interpolationMode = result.usedNearest ? "nearest_neighbor" : "exact_match";
	result.fallbackReason = result.usedNearest ? "nearest_neighbor" : "none";
	return result;
}

bool IRModtranRadianceLut::bandFromName(const std::string& value, IRBand& band)
{
	if (value == "VIS")
	{
		band = IRBand::Visible;
		return true;
	}
	if (value == "NIR")
	{
		band = IRBand::NearInfrared;
		return true;
	}
	if (value == "SWIR")
	{
		band = IRBand::ShortWaveInfrared;
		return true;
	}
	if (value == "MWIR")
	{
		band = IRBand::MidWaveInfrared;
		return true;
	}
	if (value == "LWIR")
	{
		band = IRBand::LongWaveInfrared;
		return true;
	}
	return false;
}

double IRModtranRadianceLut::clampTau(double value)
{
	if (!std::isfinite(value))
	{
		return 1.0e-6;
	}
	return std::max(1.0e-6, std::min(1.0, value));
}

double IRModtranRadianceLut::normalizedDistance(const Entry& entry, const IRModtranRadianceQuery& query)
{
	double obs = (entry.observerAltKm - query.observerAltKm) / 20.0;
	double target = (entry.targetAltKm - query.targetAltKm) / 20.0;
	double range = (entry.rangeKm - query.rangeKm) / 50.0;
	double visibility = (entry.visibilityKm - query.visibilityKm) / 50.0;
	double solarZenith = (entry.solarZenithDeg - query.solarZenithDeg) / 90.0;
	return obs * obs + target * target + range * range + visibility * visibility + solarZenith * solarZenith;
}

bool IRModtranRadianceLut::isFiniteNonNegative(double value)
{
	return std::isfinite(value) && value >= 0.0;
}

void IRModtranRadianceLut::updateBounds(BandBounds& bounds, const Entry& entry)
{
	if (!bounds.valid)
	{
		bounds.valid = true;
		bounds.minObserverAltKm = bounds.maxObserverAltKm = entry.observerAltKm;
		bounds.minTargetAltKm = bounds.maxTargetAltKm = entry.targetAltKm;
		bounds.minRangeKm = bounds.maxRangeKm = entry.rangeKm;
		bounds.minVisibilityKm = bounds.maxVisibilityKm = entry.visibilityKm;
		return;
	}
	bounds.minObserverAltKm = std::min(bounds.minObserverAltKm, entry.observerAltKm);
	bounds.maxObserverAltKm = std::max(bounds.maxObserverAltKm, entry.observerAltKm);
	bounds.minTargetAltKm = std::min(bounds.minTargetAltKm, entry.targetAltKm);
	bounds.maxTargetAltKm = std::max(bounds.maxTargetAltKm, entry.targetAltKm);
	bounds.minRangeKm = std::min(bounds.minRangeKm, entry.rangeKm);
	bounds.maxRangeKm = std::max(bounds.maxRangeKm, entry.rangeKm);
	bounds.minVisibilityKm = std::min(bounds.minVisibilityKm, entry.visibilityKm);
	bounds.maxVisibilityKm = std::max(bounds.maxVisibilityKm, entry.visibilityKm);
}

const IRModtranRadianceLut::BandBounds* IRModtranRadianceLut::boundsForBand(IRBand band) const
{
	int index = BandIndex(band);
	const BandBounds& bounds = m_bounds[index];
	return bounds.valid ? &bounds : nullptr;
}
