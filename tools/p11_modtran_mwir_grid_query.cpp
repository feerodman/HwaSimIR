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

IRModtranRadianceQuery Query(double range, double visibility, double sza, double altitude = 0.001)
{
	IRModtranRadianceQuery query;
	query.band = IRBand::MidWaveInfrared;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = "default";
	query.observerAltKm = altitude;
	query.targetAltKm = altitude;
	query.rangeKm = range;
	query.visibilityKm = visibility;
	query.solarZenithDeg = sza;
	return query;
}
}

int main(int argc, char** argv)
{
	if (argc < 2 || argc > 3) { std::cerr << "usage: probe <formal_mwir_rows.csv> [full_formal_lut.csv]\n"; return 2; }
	Checks checks;
	IRModtranRadianceLut lut;
	checks.expect(lut.load(argv[1]), "real_mwir_grid_loads");
	checks.expect(lut.entryCount() == 36, "candidate_has_exactly_36_five_component_vertices");
	checks.expect(lut.hasBand(IRBand::MidWaveInfrared), "real_mwir_band_present");

	const double ranges[] = {0.1, 0.5, 1.0};
	const double visibilities[] = {6.0, 23.0};
	const double szas[] = {20.0, 45.0, 70.0};
	const double altitudes[] = {0.001, 1.0};
	for (double altitude : altitudes) for (double range : ranges)
		for (double visibility : visibilities) for (double sza : szas)
	{
		const IRModtranRadianceResult value = lut.query(Query(range, visibility, sza, altitude));
		checks.expect(value.valid && value.interpolationMode == "exact_match", "exact_five_axis_vertex_query");
		checks.expect(value.tauUp >= 0.0 && value.tauUp <= 1.0 &&
			std::isfinite(value.pathThermalWm2SrUm) && value.pathThermalWm2SrUm >= 0.0 &&
			std::isfinite(value.directSolarIrradianceWm2Um) && value.directSolarIrradianceWm2Um > 0.0 &&
			std::isfinite(value.downwardSkyDiffuseIrradianceWm2Um) && value.downwardSkyDiffuseIrradianceWm2Um > 0.0 &&
			std::isfinite(value.pathScatteringRadianceWm2SrUm) && value.pathScatteringRadianceWm2SrUm >= 0.0,
			"exact_vertex_five_components_valid");
	}

	const IRModtranRadianceResult sun20 = lut.query(Query(0.5, 6.0, 20.0));
	const IRModtranRadianceResult sun70 = lut.query(Query(0.5, 6.0, 70.0));
	checks.expect(sun20.valid && sun70.valid &&
		sun20.directSolarIrradianceWm2Um > sun70.directSolarIrradianceWm2Um,
		"mwir_sza_not_collapsed");
	const IRModtranRadianceResult interior = lut.query(Query(0.3, 10.0, 32.5));
	checks.expect(interior.valid && interior.interpolationMode ==
		"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
		"ground_plane_coupled_interior_interpolation_valid");
	checks.expect(interior.tauUp > 0.0 && interior.tauUp < 1.0 &&
		interior.pathThermalWm2SrUm > 0.0 && interior.directSolarIrradianceWm2Um > 0.0 &&
		interior.downwardSkyDiffuseIrradianceWm2Um > 0.0,
		"interior_physics_values_positive");
	const IRModtranRadianceResult highInterior = lut.query(Query(0.3, 10.0, 32.5, 1.0));
	checks.expect(highInterior.valid && highInterior.interpolationMode ==
		"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
		"one_km_plane_coupled_interior_interpolation_valid");

	IRModtranRadianceQuery altitudeOutside = Query(0.5, 6.0, 45.0, 2.0);
	const IRModtranRadianceResult altitudeError = lut.query(altitudeOutside);
	checks.expect(!altitudeError.valid && altitudeError.fallbackReason == "out_of_range" &&
		altitudeError.fallbackAxis == "equalAltitudeKm", "altitude_extrapolation_rejected");
	const IRModtranRadianceResult middleAltitude = lut.query(Query(0.5, 6.0, 45.0, 0.5));
	checks.expect(middleAltitude.valid && middleAltitude.interpolationMode ==
		"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
		"half_km_equal_altitude_interpolation_valid");
	IRModtranRadianceQuery unequalAltitude = Query(0.5, 6.0, 45.0, 0.5);
	unequalAltitude.targetAltKm = 0.6;
	const IRModtranRadianceResult unequalResult = lut.query(unequalAltitude);
	checks.expect(!unequalResult.valid && unequalResult.fallbackReason != "none",
		"half_km_unequal_altitude_missing_cell_rejected");
	checks.expect(lut.query(Query(0.5, 6.0, 45.0, 0.001)).interpolationMode == "exact_match" &&
		lut.query(Query(0.5, 6.0, 45.0, 1.0)).interpolationMode == "exact_match",
		"equal_altitude_endpoints_remain_exact");
	const IRModtranRadianceResult rangeError = lut.query(Query(0.05, 6.0, 45.0));
	checks.expect(!rangeError.valid && rangeError.fallbackReason == "out_of_range" &&
		rangeError.fallbackAxis == "rangeKm", "range_extrapolation_rejected");
	const IRModtranRadianceResult szaError = lut.query(Query(0.5, 6.0, 80.0));
	checks.expect(!szaError.valid && szaError.fallbackReason == "out_of_range" &&
		szaError.fallbackAxis == "solarZenithDeg", "mwir_sza_extrapolation_rejected");

	if (argc == 3)
	{
		IRModtranRadianceLut formal;
		checks.expect(formal.load(argv[2]), "full_formal_lut_loads");
		checks.expect(formal.entryCount() >= 1059 && formal.entryCount() < 1394,
			"blank_legacy_mwir_rows_rejected_but_nir_swir_and_p11_mwir_retained");
		checks.expect(formal.query(Query(0.5, 6.0, 45.0)).valid, "full_formal_closeup_mwir_valid");
		checks.expect(formal.query(Query(0.5, 6.0, 45.0, 1.0)).valid, "full_formal_one_km_mwir_valid");
		IRModtranRadianceQuery legacy = Query(10.0, 23.0, 45.0);
		legacy.observerAltKm = 10.0; legacy.targetAltKm = 5.0;
		const IRModtranRadianceResult rejected = formal.query(legacy);
		checks.expect(!rejected.valid && rejected.fallbackReason == "out_of_range",
			"legacy_tau_path_only_mwir_not_used_as_formal_solar_input");
		checks.expect(formal.hasBand(IRBand::NearInfrared) && formal.hasBand(IRBand::ShortWaveInfrared),
			"nir_and_swir_compatibility_retained");
	}

	std::cout << "SUMMARY failures=" << checks.failures
		<< " interiorTau=" << interior.tauUp
		<< " interiorThermal=" << interior.pathThermalWm2SrUm
		<< " interiorDirectSolar=" << interior.directSolarIrradianceWm2Um
		<< " interiorSky=" << interior.downwardSkyDiffuseIrradianceWm2Um
		<< " interiorScatter=" << interior.pathScatteringRadianceWm2SrUm << '\n';
	return checks.failures == 0 ? 0 : 1;
}
