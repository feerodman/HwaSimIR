#include "IRMaterialBandOptics.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
double Clamp01(double v) { return std::max(0.0, std::min(1.0, v)); }

std::string Trim(const std::string& value)
{
	const size_t begin = value.find_first_not_of(" \t\r\n\"");
	if (begin == std::string::npos) return std::string();
	const size_t end = value.find_last_not_of(" \t\r\n\"");
	return value.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitCsv(const std::string& line)
{
	std::vector<std::string> values;
	std::string current;
	bool quoted = false;
	for (size_t i = 0; i < line.size(); ++i)
	{
		const char ch = line[i];
		if (ch == '"') quoted = !quoted;
		else if (ch == ',' && !quoted) { values.push_back(Trim(current)); current.clear(); }
		else current += ch;
	}
	values.push_back(Trim(current));
	return values;
}

bool ParseUnitReflectance(const std::string& text, double& value)
{
	if (text.empty()) return false;
	try { value = std::stod(text); }
	catch (...) { return false; }
	return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}
}

bool IRMaterialBandOptics::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) return false;
	std::string line;
	if (!std::getline(file, line)) return false;
	const std::vector<std::string> header = SplitCsv(line);
	std::map<std::string, size_t> columns;
	for (size_t i = 0; i < header.size(); ++i) columns[header[i]] = i;
	const char* required[] = { "Material", "NIRReflectance", "MWIRReflectance", "Source", "Notes" };
	for (size_t i = 0; i < sizeof(required) / sizeof(required[0]); ++i)
		if (columns.find(required[i]) == columns.end()) return false;

	std::map<std::string, Entry> loaded;
	while (std::getline(file, line))
	{
		if (Trim(line).empty() || Trim(line)[0] == '#') continue;
		const std::vector<std::string> values = SplitCsv(line);
		auto text = [&](const char* name) -> std::string {
			const size_t index = columns[name];
			return index < values.size() ? values[index] : std::string();
		};
		const std::string name = text("Material");
		if (name.empty()) continue;
		Entry entry;
		entry.hasNir = ParseUnitReflectance(text("NIRReflectance"), entry.nir);
		entry.hasMwir = ParseUnitReflectance(text("MWIRReflectance"), entry.mwir);
		entry.source = text("Source");
		entry.notes = text("Notes");
		loaded[name] = entry;
	}
	if (loaded.empty()) return false;
	m_entries.swap(loaded);
	m_loadedPath = filePath;
	return true;
}

bool IRMaterialBandOptics::empty() const { return m_entries.empty(); }
const std::string& IRMaterialBandOptics::loadedPath() const { return m_loadedPath; }

IRBandReflectance IRMaterialBandOptics::resolve(const IRMaterial& material) const
{
	IRBandReflectance result;
	const std::map<std::string, Entry>::const_iterator it = m_entries.find(material.name);
	if (it != m_entries.end())
	{
		result.databaseSource = it->second.source;
		result.notes = it->second.notes;
		if (it->second.hasNir)
		{
			result.nir = it->second.nir;
			result.nirSource = "band_database";
		}
		if (it->second.hasMwir)
		{
			result.mwir = it->second.mwir;
			result.mwirSource = "band_database";
		}
	}
	if (result.nirSource != "band_database")
	{
		result.nir = Clamp01(1.0 - material.solarAbsorptivity - material.transmissivity);
		result.nirSource = "solar_absorptivity_fallback";
	}
	if (result.mwirSource != "band_database")
	{
		result.mwir = Clamp01(1.0 - material.thermalEmissivity - material.transmissivity);
		result.mwirSource = "thermal_emissivity_fallback";
	}
	return result;
}
