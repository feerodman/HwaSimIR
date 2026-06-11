#include "IRModtranTauLut.h"

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
	try
	{
		return std::stod(values[index]);
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
}

IRModtranTauQuery::IRModtranTauQuery()
	: band(IRBand::MidWaveInfrared),
	observerAltKm(10.0),
	targetAltKm(10.0),
	rangeKm(1.0),
	visibilityKm(23.0),
	solarZenithDeg(45.0),
	fallbackInput("none")
{
}

IRModtranTauResult::IRModtranTauResult()
	: found(false),
	usedNearest(false),
	tauUp(1.0),
	tauDown(1.0),
	interpolationMode("none"),
	fallbackState("modtran_lut_missing")
{
}

bool IRModtranTauLut::load(const std::string& filePath)
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
		"tau_down_band"
	};
	for (size_t i = 0; i < sizeof(requiredColumns) / sizeof(requiredColumns[0]); ++i)
	{
		if (!HasColumn(columns, requiredColumns[i]))
		{
			return false;
		}
	}

	std::vector<Entry> entries;
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

		if (!std::isfinite(entry.visibilityKm) ||
			!std::isfinite(entry.observerAltKm) ||
			!std::isfinite(entry.targetAltKm) ||
			!std::isfinite(entry.rangeKm) ||
			!std::isfinite(entry.solarZenithDeg) ||
			!std::isfinite(entry.tauUp) ||
			!std::isfinite(entry.tauDown))
		{
			continue;
		}

		entry.tauUp = clampTau(entry.tauUp);
		entry.tauDown = clampTau(entry.tauDown);
		entries.push_back(entry);
	}

	if (entries.empty())
	{
		return false;
	}

	m_entries.swap(entries);
	m_loadedPath = filePath;
	return true;
}

bool IRModtranTauLut::empty() const
{
	return m_entries.empty();
}

const std::string& IRModtranTauLut::loadedPath() const
{
	return m_loadedPath;
}

IRModtranTauResult IRModtranTauLut::query(const IRModtranTauQuery& query) const
{
	IRModtranTauResult result;
	if (m_entries.empty())
	{
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

		double distance = normalizedDistance(entry, query);
		if (distance < bestDistance)
		{
			bestDistance = distance;
			best = &entry;
		}
	}

	if (best == nullptr)
	{
		return result;
	}

	double odUp = -std::log(clampTau(best->tauUp));
	double odDown = -std::log(clampTau(best->tauDown));
	result.found = true;
	result.usedNearest = bestDistance > 1.0e-10;
	result.tauUp = opticalDepthToTau(odUp);
	result.tauDown = opticalDepthToTau(odDown);
	result.interpolationMode = "nearest_od";
	result.fallbackState = result.usedNearest ? "nearest_neighbor" : "exact_match";
	return result;
}

bool IRModtranTauLut::bandFromName(const std::string& value, IRBand& band)
{
	if (value == "NIR")
	{
		band = IRBand::NearInfrared;
		return true;
	}
	if (value == "MWIR")
	{
		band = IRBand::MidWaveInfrared;
		return true;
	}
	return false;
}

double IRModtranTauLut::clampTau(double value)
{
	if (!std::isfinite(value))
	{
		return 1.0e-6;
	}
	return std::max(1.0e-6, std::min(1.0, value));
}

double IRModtranTauLut::opticalDepthToTau(double opticalDepth)
{
	if (!std::isfinite(opticalDepth))
	{
		return 1.0e-6;
	}
	return std::max(0.0, std::min(1.0, std::exp(-opticalDepth)));
}

double IRModtranTauLut::normalizedDistance(const Entry& entry, const IRModtranTauQuery& query)
{
	double obs = (entry.observerAltKm - query.observerAltKm) / 20.0;
	double target = (entry.targetAltKm - query.targetAltKm) / 20.0;
	double range = (entry.rangeKm - query.rangeKm) / 50.0;
	double visibility = (entry.visibilityKm - query.visibilityKm) / 50.0;
	double solarZenith = (entry.solarZenithDeg - query.solarZenithDeg) / 90.0;
	return obs * obs + target * target + range * range + visibility * visibility + solarZenith * solarZenith;
}
