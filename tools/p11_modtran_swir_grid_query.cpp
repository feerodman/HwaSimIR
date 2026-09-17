#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
struct Checks
{
	int failures = 0;
	void expect(bool condition, const std::string& name)
	{
		std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
		if (!condition) ++failures;
	}
};

IRModtranRadianceQuery Query(double range, double visibility, double sza)
{
	IRModtranRadianceQuery query;
	query.band = IRBand::ShortWaveInfrared;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = "default";
	query.observerAltKm = 0.001;
	query.targetAltKm = 0.001;
	query.rangeKm = range;
	query.visibilityKm = visibility;
	query.solarZenithDeg = sza;
	return query;
}
}

int main(int argc, char** argv)
{
	if (argc != 2) { std::cerr << "usage: probe <formal_swir_rows.csv>\n"; return 2; }
	Checks checks;
	IRModtranRadianceLut lut;
	checks.expect(lut.load(argv[1]), "real_swir_grid_loads");
	checks.expect(lut.entryCount() >= 18, "lut_has_at_least_18_real_swir_vertices");
	checks.expect(lut.hasBand(IRBand::ShortWaveInfrared), "real_swir_band_present");

	const double ranges[] = {0.1, 0.5, 1.0};
	const double visibilities[] = {6.0, 23.0};
	const double szas[] = {20.0, 45.0, 70.0};
	for (double range : ranges) for (double visibility : visibilities) for (double sza : szas)
	{
		const IRModtranRadianceResult value = lut.query(Query(range, visibility, sza));
		checks.expect(value.valid && value.interpolationMode == "exact_match", "exact_vertex_query");
		checks.expect(value.tauUp >= 0.0 && value.tauUp <= 1.0 &&
			std::isfinite(value.pathThermalWm2SrUm) &&
			std::isfinite(value.directSolarIrradianceWm2Um) &&
			std::isfinite(value.downwardSkyDiffuseIrradianceWm2Um) &&
			std::isfinite(value.pathScatteringRadianceWm2SrUm), "exact_vertex_values_valid");
	}

	const IRModtranRadianceResult interior = lut.query(Query(0.3, 10.0, 32.5));
	checks.expect(interior.valid && interior.interpolationMode ==
		"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
		"coupled_equal_altitude_interior_interpolation_valid");
	checks.expect(interior.tauUp > 0.0 && interior.tauUp < 1.0 &&
		interior.directSolarIrradianceWm2Um > 0.0 && interior.downwardSkyDiffuseIrradianceWm2Um > 0.0,
		"interior_physics_values_positive");

	IRModtranRadianceQuery altitudeOutside = Query(0.5, 6.0, 45.0);
	altitudeOutside.targetAltKm = 0.0;
	const IRModtranRadianceResult altitudeError = lut.query(altitudeOutside);
	checks.expect(!altitudeError.valid && altitudeError.fallbackReason == "out_of_range" &&
		altitudeError.fallbackAxis == "targetAltKm", "altitude_extrapolation_rejected");
	const IRModtranRadianceResult rangeError = lut.query(Query(0.05, 6.0, 45.0));
	checks.expect(!rangeError.valid && rangeError.fallbackReason == "out_of_range" &&
		rangeError.fallbackAxis == "rangeKm", "range_extrapolation_rejected");
	const IRModtranRadianceResult szaError = lut.query(Query(0.5, 6.0, 80.0));
	checks.expect(!szaError.valid && szaError.fallbackReason == "out_of_range" &&
		szaError.fallbackAxis == "solarZenithDeg", "sza_extrapolation_rejected");

	std::cout << "SUMMARY failures=" << checks.failures
		<< " interiorTau=" << interior.tauUp
		<< " interiorDirectSolar=" << interior.directSolarIrradianceWm2Um
		<< " interiorSky=" << interior.downwardSkyDiffuseIrradianceWm2Um
		<< " interiorScatter=" << interior.pathScatteringRadianceWm2SrUm << '\n';
	return checks.failures == 0 ? 0 : 1;
}
