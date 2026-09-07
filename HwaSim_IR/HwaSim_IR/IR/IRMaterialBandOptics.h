#pragma once

#include "../IRSimulation.h"

#include <map>
#include <string>

struct IRBandReflectance
{
	double nir = 0.0;
	double mwir = 0.0;
	std::string nirSource = "missing";
	std::string mwirSource = "missing";
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
		bool hasMwir = false;
		double nir = 0.0;
		double mwir = 0.0;
		std::string source;
		std::string notes;
	};

	std::map<std::string, Entry> m_entries;
	std::string m_loadedPath;
};
