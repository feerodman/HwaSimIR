#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
struct Checks
{
	int failures = 0;
	int passes = 0;
	void expect(bool condition, const std::string& name)
	{
		std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
		if (condition) ++passes; else ++failures;
	}
};

IRModtranRadianceQuery Query(IRBand band, const std::string& profile,
	double altitude, double range, double visibility, double sza)
{
	IRModtranRadianceQuery query;
	query.band = band;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = profile;
	query.observerAltKm = altitude;
	query.targetAltKm = altitude;
	query.rangeKm = range;
	query.visibilityKm = visibility;
	query.solarZenithDeg = sza;
	return query;
}

bool FiveComponents(const IRModtranRadianceResult& result)
{
	return result.valid && result.tauUp >= 0.0 && result.tauUp <= 1.0 &&
		std::isfinite(result.pathThermalWm2SrUm) && result.pathThermalWm2SrUm >= 0.0 &&
		std::isfinite(result.directSolarIrradianceWm2Um) && result.directSolarIrradianceWm2Um > 0.0 &&
		std::isfinite(result.downwardSkyDiffuseIrradianceWm2Um) && result.downwardSkyDiffuseIrradianceWm2Um > 0.0 &&
		std::isfinite(result.pathScatteringRadianceWm2SrUm) && result.pathScatteringRadianceWm2SrUm >= 0.0;
}

bool Near(double actual, double expected, double tolerance = 1.0e-12)
{
	return std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected));
}
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "usage: humidity_query <formal_humidity_rows.csv> <full_formal_lut.csv>\n";
		return 2;
	}
	Checks checks;
	IRModtranRadianceLut candidate;
	checks.expect(candidate.load(argv[1]), "isolated_humidity_candidate_loads");
	checks.expect(candidate.entryCount() == 216, "isolated_candidate_has_exactly_216_vertices");
	checks.expect(candidate.hasBand(IRBand::ShortWaveInfrared) &&
		candidate.hasBand(IRBand::MidWaveInfrared), "candidate_has_swir_and_mwir");

	const IRBand bands[] = {IRBand::ShortWaveInfrared, IRBand::MidWaveInfrared};
	const char* profiles[] = {
		"scaled_mls_surface_rh30", "scaled_mls_surface_rh60", "scaled_mls_surface_rh85"
	};
	const double altitudes[] = {0.001, 1.0};
	const double ranges[] = {0.1, 0.5, 1.0};
	const double visibilities[] = {6.0, 23.0};
	const double szas[] = {20.0, 45.0, 70.0};
	int exactVertices = 0;
	for (IRBand band : bands) for (const char* profile : profiles)
		for (double altitude : altitudes) for (double range : ranges)
			for (double visibility : visibilities) for (double sza : szas)
	{
		const IRModtranRadianceResult result = candidate.query(
			Query(band, profile, altitude, range, visibility, sza));
		if (FiveComponents(result) && result.interpolationMode == "exact_match") ++exactVertices;
	}
	checks.expect(exactVertices == 216, "all_216_exact_classified_vertices_have_five_components");

	for (IRBand band : bands) for (const char* profile : profiles)
	{
		const IRModtranRadianceResult interior = candidate.query(
			Query(band, profile, 0.5, 0.3, 10.0, 32.5));
		checks.expect(FiveComponents(interior) && interior.interpolationMode ==
			"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
			std::string("five_axis_interior_") + profile);
	}

	for (IRBand band : bands)
	{
		const IRModtranRadianceResult dry = candidate.query(
			Query(band, profiles[0], 0.5, 0.3, 10.0, 32.5));
		const IRModtranRadianceResult middle = candidate.query(
			Query(band, profiles[1], 0.5, 0.3, 10.0, 32.5));
		const IRModtranRadianceResult humid = candidate.query(
			Query(band, profiles[2], 0.5, 0.3, 10.0, 32.5));
		checks.expect(dry.valid && middle.valid && humid.valid &&
			dry.tauUp > middle.tauUp && middle.tauUp > humid.tauUp,
			"classified_humidity_tau_response_strict");
	}
	const IRModtranRadianceResult unknown = candidate.query(
		Query(IRBand::ShortWaveInfrared, "scaled_mls_surface_rh45", 0.5, 0.3, 10.0, 32.5));
	checks.expect(!unknown.valid && unknown.fallbackReason == "category_missing",
		"undeclared_numeric_humidity_category_fails_closed");
	for (IRBand band : bands)
	{
		IRModtranRadianceQuery numeric = Query(band, "ignored_by_numeric_query", 0.5, 0.3, 10.0, 32.5);
		const IRModtranRadianceResult rh60 = candidate.queryRelativeHumidity(numeric, 60.0);
		const IRModtranRadianceResult exact60 = candidate.query(
			Query(band, profiles[1], 0.5, 0.3, 10.0, 32.5));
		checks.expect(rh60.valid && Near(rh60.tauUp, exact60.tauUp) &&
			rh60.interpolationMode.find("relativeHumidity_exact_profile+") == 0,
			"numeric_rh60_selects_audited_profile_exactly");
		const IRModtranRadianceResult low = candidate.query(
			Query(band, profiles[1], 0.5, 0.3, 10.0, 32.5));
		const IRModtranRadianceResult high = candidate.query(
			Query(band, profiles[2], 0.5, 0.3, 10.0, 32.5));
		const IRModtranRadianceResult rh725 = candidate.queryRelativeHumidity(numeric, 72.5);
		const double expectedTau = std::exp(0.5 * (std::log(low.tauUp) + std::log(high.tauUp)));
		checks.expect(rh725.valid && Near(rh725.tauUp, expectedTau) &&
			Near(rh725.pathThermalWm2SrUm,
				0.5 * (low.pathThermalWm2SrUm + high.pathThermalWm2SrUm)) &&
			rh725.interpolationMode.find("relativeHumidity_linear_components_tau_od+") == 0,
			"numeric_rh72p5_mixes_tau_in_optical_depth_and_components_linearly");
		const IRModtranRadianceResult tooDry = candidate.queryRelativeHumidity(numeric, 29.0);
		const IRModtranRadianceResult tooWet = candidate.queryRelativeHumidity(numeric, 86.0);
		checks.expect(!tooDry.valid && !tooWet.valid &&
			tooDry.fallbackAxis == "relativeHumidityPercent" &&
			tooWet.fallbackAxis == "relativeHumidityPercent",
			"numeric_humidity_outside_measured_envelope_fails_closed");
	}
	IRModtranRadianceQuery unequal = Query(IRBand::MidWaveInfrared,
		profiles[1], 0.5, 0.5, 6.0, 45.0);
	unequal.targetAltKm = 0.6;
	const IRModtranRadianceResult unequalResult = candidate.query(unequal);
	checks.expect(!unequalResult.valid, "unequal_altitude_missing_cell_still_fails_closed");

	IRModtranRadianceLut formal;
	checks.expect(formal.load(argv[2]), "published_full_formal_lut_loads");
	for (IRBand band : bands) for (const char* profile : profiles)
		checks.expect(FiveComponents(formal.query(Query(band, profile, 0.5, 0.3, 10.0, 32.5))),
			std::string("published_profile_query_") + profile);
	checks.expect(FiveComponents(formal.query(Query(IRBand::ShortWaveInfrared,
		"default", 0.5, 0.3, 10.0, 32.5))), "published_default_swir_retained");
	checks.expect(FiveComponents(formal.query(Query(IRBand::MidWaveInfrared,
		"default", 0.5, 0.3, 10.0, 32.5))), "published_default_mwir_retained");

	std::cout << "SUMMARY passes=" << checks.passes << " failures=" << checks.failures
		<< " exactVertices=" << exactVertices << '\n';
	return checks.failures == 0 ? 0 : 1;
}
