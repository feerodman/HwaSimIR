#pragma once

#include "IRTypes.h"

#include <string>
#include <vector>

struct IRModtranRadianceQuery
{
	IRBand band;
	double observerAltKm;
	double targetAltKm;
	double rangeKm;
	double visibilityKm;
	double humidityPercent;
	double solarZenithDeg;

	IRModtranRadianceQuery();
};

struct IRModtranRadianceResult
{
	bool valid;
	bool usedNearest;
	double observerAltKm;
	double targetAltKm;
	double rangeKm;
	double visibilityKm;
	double humidityPercent;
	double tauUp;
	double tauDown;
	double pathRadiance;
	double skyRadiance;
	double solarIrradiance;
	std::string sourceFile;
	std::string unitRadiance;
	std::string unitIrradiance;
	std::string interpolationMode;
	std::string fallbackReason;

	IRModtranRadianceResult();
};

class IRModtranRadianceLut
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	IRModtranRadianceResult query(const IRModtranRadianceQuery& query) const;

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
		double pathRadiance;
		double skyRadiance;
		double solarIrradiance;
		std::string unitRadiance;
		std::string unitIrradiance;
	};

	struct BandBounds
	{
		bool valid = false;
		double minObserverAltKm = 0.0;
		double maxObserverAltKm = 0.0;
		double minTargetAltKm = 0.0;
		double maxTargetAltKm = 0.0;
		double minRangeKm = 0.0;
		double maxRangeKm = 0.0;
		double minVisibilityKm = 0.0;
		double maxVisibilityKm = 0.0;
	};

	static bool bandFromName(const std::string& value, IRBand& band);
	static double clampTau(double value);
	static double normalizedDistance(const Entry& entry, const IRModtranRadianceQuery& query);
	static bool isFiniteNonNegative(double value);
	static void updateBounds(BandBounds& bounds, const Entry& entry);
	const BandBounds* boundsForBand(IRBand band) const;

	std::vector<Entry> m_entries;
	BandBounds m_bounds[5];
	std::string m_loadedPath;
};
