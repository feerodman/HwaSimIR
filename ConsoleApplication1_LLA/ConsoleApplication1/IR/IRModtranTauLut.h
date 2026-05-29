#pragma once

#include <string>
#include <vector>

#include "IRTypes.h"

struct IRModtranTauQuery
{
	IRBand band;
	double observerAltKm;
	double targetAltKm;
	double rangeKm;
	double visibilityKm;
	double solarZenithDeg;
	std::string fallbackInput;

	IRModtranTauQuery();
};

struct IRModtranTauResult
{
	bool found;
	bool usedNearest;
	double tauUp;
	double tauDown;
	std::string interpolationMode;
	std::string fallbackState;

	IRModtranTauResult();
};

class IRModtranTauLut
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	IRModtranTauResult query(const IRModtranTauQuery& query) const;

private:
	struct Entry
	{
		IRBand band;
		double observerAltKm;
		double targetAltKm;
		double rangeKm;
		double visibilityKm;
		double solarZenithDeg;
		double tauUp;
		double tauDown;
	};

	static bool bandFromName(const std::string& value, IRBand& band);
	static double clampTau(double value);
	static double opticalDepthToTau(double opticalDepth);
	static double normalizedDistance(const Entry& entry, const IRModtranTauQuery& query);

	std::vector<Entry> m_entries;
	std::string m_loadedPath;
};
