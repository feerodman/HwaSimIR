#include "../HwaSim_IR/HwaSim_IR/IR/IRSolarHeatingLut.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
struct Checks
{
	int passes = 0;
	int failures = 0;
	void expect(bool condition, const std::string& name)
	{
		std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
		if (condition) ++passes; else ++failures;
	}
};

IRSolarHeatingQuery Query(const std::string& profile, double altitude,
	double visibility, double sza)
{
	IRSolarHeatingQuery query;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = profile;
	query.targetAltKm = altitude;
	query.visibilityKm = visibility;
	query.solarZenithDeg = sza;
	return query;
}

bool Physical(const IRSolarHeatingResult& result)
{
	return result.valid && std::isfinite(result.directIrradianceWm2) &&
		result.directIrradianceWm2 > 0.0 &&
		std::isfinite(result.diffuseDownIrradianceWm2) &&
		result.diffuseDownIrradianceWm2 >= 0.0 &&
		result.irradianceUnit == "W/m^2" && result.fallbackReason == "none";
}

bool Near(double a, double b)
{
	return std::abs(a - b) <= 1.0e-10 * std::max(1.0, std::max(std::abs(a), std::abs(b)));
}

bool MakeMissingCellFixture(const char* sourcePath, const char* outputPath)
{
	std::ifstream source(sourcePath);
	std::ofstream output(outputPath);
	if (!source.is_open() || !output.is_open()) return false;
	const std::string removed = "SWHEAT_P11_scaled_mls_surface_rh85_tar1_vis23_sza70";
	std::string line;
	bool found = false;
	while (std::getline(source, line))
	{
		if (line.find(removed) != std::string::npos) { found = true; continue; }
		output << line << '\n';
	}
	return found && output.good();
}
}

int main(int argc, char** argv)
{
	if (argc != 4)
	{
		std::cerr << "usage: solar_heating_query <candidate.csv> <formal.csv> <missing_fixture.csv>\n";
		return 2;
	}
	Checks checks;
	IRSolarHeatingLut candidate;
	checks.expect(candidate.load(argv[1]), "isolated_candidate_loads");
	checks.expect(candidate.entryCount() == 48, "isolated_candidate_has_48_vertices");

	const char* profiles[] = {
		"default", "scaled_mls_surface_rh30", "scaled_mls_surface_rh60",
		"scaled_mls_surface_rh85"
	};
	const double altitudes[] = {0.001, 1.0};
	const double visibilities[] = {6.0, 23.0};
	const double szas[] = {20.0, 45.0, 70.0};
	int exactVertices = 0;
	for (const char* profile : profiles) for (double altitude : altitudes)
		for (double visibility : visibilities) for (double sza : szas)
	{
		const IRSolarHeatingResult result = candidate.query(
			Query(profile, altitude, visibility, sza));
		if (Physical(result) && result.interpolationMode ==
			"staged_linear_targetAlt_visibility_solarZenith" &&
			result.sourceCaseIds.find("SWHEAT_P11_") == 0) ++exactVertices;
	}
	checks.expect(exactVertices == 48, "all_48_exact_vertices_query_physical_values");

	for (const char* profile : profiles)
	{
		const IRSolarHeatingResult interior = candidate.query(
			Query(profile, 0.5, 14.0, 32.5));
		checks.expect(Physical(interior) &&
			interior.sourceCaseIds == "interpolated_multiple_cases",
			std::string("three_axis_interior_") + profile);
		const IRSolarHeatingResult lowSza = candidate.query(
			Query(profile, 0.001, 6.0, 20.0));
		const IRSolarHeatingResult midSza = candidate.query(
			Query(profile, 0.001, 6.0, 45.0));
		const IRSolarHeatingResult highSza = candidate.query(
			Query(profile, 0.001, 6.0, 70.0));
		checks.expect(Physical(lowSza) && Physical(midSza) && Physical(highSza) &&
			lowSza.directIrradianceWm2 > midSza.directIrradianceWm2 &&
			midSza.directIrradianceWm2 > highSza.directIrradianceWm2,
			std::string("direct_strictly_descends_with_sza_") + profile);
	}
	const double humidityPlanes[] = {30.0, 60.0, 85.0};
	const char* measuredProfiles[] = {
		"scaled_mls_surface_rh30", "scaled_mls_surface_rh60",
		"scaled_mls_surface_rh85"
	};
	for (size_t index = 0; index < 3; ++index)
	{
		const IRSolarHeatingQuery spatial = Query("ignored_by_numeric_query", 0.5, 14.0, 32.5);
		const IRSolarHeatingResult numeric = candidate.queryRelativeHumidity(
			spatial, humidityPlanes[index]);
		const IRSolarHeatingResult categorical = candidate.query(
			Query(measuredProfiles[index], 0.5, 14.0, 32.5));
		checks.expect(Physical(numeric) && Physical(categorical) &&
			Near(numeric.directIrradianceWm2, categorical.directIrradianceWm2) &&
			Near(numeric.diffuseDownIrradianceWm2, categorical.diffuseDownIrradianceWm2) &&
			Near(numeric.relativeHumidityPercent, humidityPlanes[index]) &&
			numeric.humidityMode == "numeric_exact_profile" &&
			numeric.interpolationMode.find("relativeHumidity_exact_profile+") == 0,
			std::string("numeric_rh_exact_") + measuredProfiles[index]);
		for (double altitude : altitudes)
		{
			const IRSolarHeatingResult endpoint = candidate.queryRelativeHumidity(
				Query("ignored_by_numeric_query", altitude, 14.0, 32.5),
				humidityPlanes[index]);
			checks.expect(Physical(endpoint), std::string("numeric_rh_altitude_endpoint_") +
				measuredProfiles[index] + (altitude < 0.01 ? "_0p001km" : "_1km"));
		}
	}
	const IRSolarHeatingQuery humidityInterior = Query(
		"ignored_by_numeric_query", 0.5, 14.0, 32.5);
	const IRSolarHeatingResult rh60 = candidate.query(
		Query("scaled_mls_surface_rh60", 0.5, 14.0, 32.5));
	const IRSolarHeatingResult rh85 = candidate.query(
		Query("scaled_mls_surface_rh85", 0.5, 14.0, 32.5));
	const IRSolarHeatingResult rh725 = candidate.queryRelativeHumidity(humidityInterior, 72.5);
	checks.expect(Physical(rh725) && Physical(rh60) && Physical(rh85) &&
		Near(rh725.directIrradianceWm2,
			0.5 * (rh60.directIrradianceWm2 + rh85.directIrradianceWm2)) &&
		Near(rh725.diffuseDownIrradianceWm2,
			0.5 * (rh60.diffuseDownIrradianceWm2 + rh85.diffuseDownIrradianceWm2)) &&
		Near(rh725.relativeHumidityPercent, 72.5) &&
		rh725.humidityMode == "numeric_linear_components" &&
		rh725.interpolationMode.find("relativeHumidity_linear_components+") == 0,
		"numeric_rh72p5_linear_components_at_three_axis_interior");
	const IRSolarHeatingResult tooDry = candidate.queryRelativeHumidity(humidityInterior, 29.0);
	const IRSolarHeatingResult tooWet = candidate.queryRelativeHumidity(humidityInterior, 86.0);
	checks.expect(!tooDry.valid && !tooWet.valid &&
		tooDry.fallbackReason == "out_of_range" && tooWet.fallbackReason == "out_of_range" &&
		tooDry.fallbackAxis == "relativeHumidityPercent" &&
		tooWet.fallbackAxis == "relativeHumidityPercent" &&
		Near(tooDry.fallbackMin, 30.0) && Near(tooDry.fallbackMax, 85.0) &&
		Near(tooWet.fallbackMin, 30.0) && Near(tooWet.fallbackMax, 85.0),
		"numeric_rh29_and_rh86_fail_closed");

	const IRSolarHeatingResult unknown = candidate.query(
		Query("scaled_mls_surface_rh45", 0.5, 14.0, 32.5));
	checks.expect(!unknown.valid && unknown.fallbackReason == "category_missing",
		"unknown_humidity_category_fails_closed");
	const IRSolarHeatingResult above = candidate.query(
		Query("scaled_mls_surface_rh60", 1.1, 14.0, 32.5));
	checks.expect(!above.valid && above.fallbackReason == "out_of_range" &&
		above.fallbackAxis == "targetAltKm", "classified_altitude_outside_grid_fails_closed");
	const IRSolarHeatingResult invalid = candidate.query(
		Query("scaled_mls_surface_rh60", 0.5, 0.0, 32.5));
	checks.expect(!invalid.valid && invalid.fallbackReason == "invalid_query",
		"invalid_visibility_fails_closed");

	checks.expect(MakeMissingCellFixture(argv[1], argv[3]), "missing_cell_fixture_created");
	IRSolarHeatingLut missing;
	checks.expect(missing.load(argv[3]) && missing.entryCount() == 47,
		"missing_cell_fixture_loads_47_vertices");
	const IRSolarHeatingResult absent = missing.query(
		Query("scaled_mls_surface_rh85", 1.0, 23.0, 70.0));
	checks.expect(!absent.valid && (absent.fallbackReason == "out_of_range" ||
		absent.fallbackReason == "cell_missing_or_duplicate"),
		"missing_exact_cell_fails_closed");

	IRSolarHeatingLut formal;
	checks.expect(formal.load(argv[2]), "published_formal_lut_loads");
	checks.expect(formal.entryCount() == 93, "published_formal_lut_has_93_rows");
	int preservedEndpoints = 0;
	for (const char* profile : profiles) for (double altitude : altitudes)
		for (double visibility : visibilities) for (double sza : szas)
	{
		const IRSolarHeatingResult isolated = candidate.query(
			Query(profile, altitude, visibility, sza));
		const IRSolarHeatingResult published = formal.query(
			Query(profile, altitude, visibility, sza));
		if (Physical(isolated) && Physical(published) &&
			Near(isolated.directIrradianceWm2, published.directIrradianceWm2) &&
			Near(isolated.diffuseDownIrradianceWm2, published.diffuseDownIrradianceWm2))
			++preservedEndpoints;
	}
	checks.expect(preservedEndpoints == 48, "all_published_p11_endpoints_unchanged");
	checks.expect(Physical(formal.query(Query("default", 0.5, 14.0, 32.5))),
		"published_default_civil_interior_query_valid");
	checks.expect(Physical(formal.query(Query("default", 5.0, 23.0, 45.0))),
		"published_legacy_default_standard_query_valid");
	checks.expect(Physical(formal.queryRelativeHumidity(
		Query("ignored_by_numeric_query", 0.5, 14.0, 32.5), 72.5)),
		"published_numeric_rh_and_spatial_interior_query_valid");

	std::cout << "SUMMARY passes=" << checks.passes << " failures=" << checks.failures
		<< " exactVertices=" << exactVertices
		<< " preservedEndpoints=" << preservedEndpoints << '\n';
	return checks.failures == 0 ? 0 : 1;
}
