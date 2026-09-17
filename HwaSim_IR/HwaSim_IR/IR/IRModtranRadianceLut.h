#pragma once

#include "IRTypes.h"

#include <cstddef>
#include <string>
#include <vector>

struct IRModtranRadianceQuery
{
	IRBand band;
	std::string atmosphereModel;
	std::string aerosolModel;
	std::string humidityProfile;
	double observerAltKm;
	double targetAltKm;
	double rangeKm;
	double visibilityKm;
	double solarZenithDeg;

	IRModtranRadianceQuery();
};

struct IRModtranRadianceResult
{
	bool valid;
	double tauUp;
	double pathThermalWm2SrUm;
	double directSolarIrradianceWm2Um;
	double downwardSkyDiffuseIrradianceWm2Um;
	double pathScatteringRadianceWm2SrUm;
	// Backward-compatible aliases.  All are SI; skyRadiance is intentionally
	// never populated from irradiance because those quantities are not interchangeable.
	double pathRadiance;
	double skyRadiance;
	double solarIrradiance;
	std::string radianceUnit;
	std::string irradianceUnit;
	std::string responseMode;
	std::string interpolationMode;
	double relativeHumidityPercent;
	std::string humidityMode;
	std::string fallbackReason;
	std::string fallbackAxis;
	double fallbackQuery;
	double fallbackMin;
	double fallbackMax;
	std::string sourceCaseIds;
	std::string sourceFiles;

	IRModtranRadianceResult();
};

class IRModtranRadianceLut
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	size_t entryCount() const;
	bool hasBand(IRBand band) const;
	IRModtranRadianceResult query(const IRModtranRadianceQuery& query) const;
	// The protocol carries a numeric relative humidity, while MODTRAN rows are
	// audited at the classified 30/60/85 percent planes.  Interpolate only inside
	// that measured envelope; tau is mixed in optical-depth space.
	IRModtranRadianceResult queryRelativeHumidity(
		const IRModtranRadianceQuery& query, double relativeHumidityPercent) const;

private:
	struct Entry
	{
		IRBand band;
		std::string atmosphereModel;
		std::string aerosolModel;
		std::string humidityProfile;
		double observerAltKm;
		double targetAltKm;
		double rangeKm;
		double visibilityKm;
		double solarZenithDeg;
		double tauUp;
		double pathThermal;
		double directSolar;
		double skyDiffuse;
		double pathScattering;
		std::string responseMode;
		std::string sourceCaseIds;
		std::string sourceFiles;
	};

	struct Sample
	{
		double opticalDepth;
		double pathThermal;
		double directSolar;
		double skyDiffuse;
		double pathScattering;
		std::string sourceCaseIds;
		std::string sourceFiles;
	};

	struct InterpolationError
	{
		std::string reason;
		std::string axis;
		double query = 0.0;
		double minimum = 0.0;
		double maximum = 0.0;
	};

	static bool bandFromName(const std::string& value, IRBand& band);
	static double clampTau(double value);
	static Sample mix(const Sample& low, const Sample& high, double t);
	bool interpolate(const std::vector<const Entry*>& entries, const IRModtranRadianceQuery& query,
		size_t axisIndex, Sample& sample, InterpolationError& error) const;
	bool interpolateEqualAltitude(const std::vector<const Entry*>& entries,
		const IRModtranRadianceQuery& query, size_t axisIndex,
		Sample& sample, InterpolationError& error) const;
	static double axisValue(const Entry& entry, size_t axisIndex, bool solarAware);
	static double queryAxisValue(const IRModtranRadianceQuery& query, size_t axisIndex, bool solarAware);
	static const char* axisName(size_t axisIndex, bool solarAware);
	static double equalAltitudeAxisValue(const Entry& entry, size_t axisIndex);
	static double equalAltitudeQueryAxisValue(const IRModtranRadianceQuery& query, size_t axisIndex);
	static const char* equalAltitudeAxisName(size_t axisIndex);

	std::vector<Entry> m_entries;
	std::string m_loadedPath;
};
