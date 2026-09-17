#pragma once

#include "../IRSimulation.h"

#include <map>
#include <string>

struct IRBandReflectance
{
	double nir = 0.0;
	double swir = 0.0;
	double mwir = 0.0;
	double swirEmissivity = 0.0;
	double swirTransmissivity = 0.0;
	double mwirEmissivity = 0.0;
	double mwirTransmissivity = 0.0;
	std::string nirSource = "missing";
	std::string swirSource = "missing";
	std::string mwirSource = "missing";
	std::string swirEmissivitySource = "missing";
	std::string mwirEmissivitySource = "missing";
	std::string databaseSource;
	std::string notes;
};

class IRMaterialBandOptics
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	IRBandReflectance resolve(const IRMaterial& material) const;

private:
	struct Entry
	{
		bool hasNir = false;
		bool hasSwir = false;
		bool hasMwir = false;
		bool hasSwirEmissivity = false;
		bool hasSwirTransmissivity = false;
		bool hasMwirEmissivity = false;
		bool hasMwirTransmissivity = false;
		double nir = 0.0;
		double swir = 0.0;
		double mwir = 0.0;
		double swirEmissivity = 0.0;
		double swirTransmissivity = 0.0;
		double mwirEmissivity = 0.0;
		double mwirTransmissivity = 0.0;
		std::string source;
		std::string notes;
	};

	std::map<std::string, Entry> m_entries;
	std::string m_loadedPath;
};
