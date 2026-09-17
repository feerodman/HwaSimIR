#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
const char* kHeader =
	"band,atmosphere_model,aerosol_model,humidity_profile,visibility_km,"
	"observer_alt_km,target_alt_km,range_km,solar_zenith_deg,tau_up,"
	"path_thermal_W_m2_sr_um,direct_solar_irradiance_at_target_W_m2_um,"
	"downward_sky_diffuse_irradiance_W_m2_um,los_path_scattering_radiance_W_m2_sr_um,"
	"radiance_unit,irradiance_unit,tau_unit,response_mode,source_case_ids,source_files";

std::string JoinPath(const std::string& directory, const std::string& name)
{
	if (directory.empty()) return name;
	const char last = directory[directory.size() - 1];
	return directory + ((last == '/' || last == '\\') ? "" : "/") + name;
}

std::string Row(const char* band, double observer, double target, double range,
	double tau, double thermal, double direct, double sky, double scatter, const char* id)
{
	std::ostringstream out;
	out.precision(17);
	out << band << ",Mid-Latitude Summer,Rural,default,6,"
		<< observer << ',' << target << ',' << range << ",45,"
		<< tau << ',' << thermal << ',' << direct << ',' << sky << ',' << scatter
		<< ",W/(m^2 sr um),W/(m^2 um),dimensionless,RectangularBand," << id << ",unit_test";
	return out.str();
}

bool WriteFixture(const std::string& path, bool complete)
{
	std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
	if (!output.is_open()) return false;
	output << kHeader << '\n';
	const char* bands[] = {"SWIR", "MWIR"};
	for (const char* band : bands)
	{
		output << Row(band, 0.001, 0.001, 0.5, 0.8, 0.1, 2.0, 0.3, 0.01, "low") << '\n';
		output << Row(band, 1.0, 1.0, complete ? 0.5 : 1.0,
			0.6, 0.2, 4.0, 0.5, 0.03, "high") << '\n';
		// This real but non-horizontal row must never participate in the coupled
		// equal-altitude interpolation.
		output << Row(band, 0.001, 1.0, 0.5, 0.1, 9.0, 90.0, 9.0, 9.0, "off_diagonal") << '\n';
	}
	return output.good();
}

IRModtranRadianceQuery Query(IRBand band, double observer, double target)
{
	IRModtranRadianceQuery query;
	query.band = band;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = "default";
	query.observerAltKm = observer;
	query.targetAltKm = target;
	query.rangeKm = 0.5;
	query.visibilityKm = 6.0;
	query.solarZenithDeg = 45.0;
	return query;
}

bool Near(double actual, double expected, double tolerance = 1.0e-12)
{
	return std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

struct Checks
{
	int failures = 0;
	void expect(bool condition, const std::string& name)
	{
		std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
		if (!condition) ++failures;
	}
};
}

int main(int argc, char** argv)
{
	if (argc != 2) { std::cerr << "usage: probe <output-directory>\n"; return 2; }
	Checks checks;
	const std::string completePath = JoinPath(argv[1], "equal_altitude_complete.csv");
	const std::string missingPath = JoinPath(argv[1], "equal_altitude_missing_cell.csv");
	checks.expect(WriteFixture(completePath, true), "write_complete_equal_altitude_fixture");
	checks.expect(WriteFixture(missingPath, false), "write_missing_equal_altitude_fixture");

	IRModtranRadianceLut complete;
	IRModtranRadianceLut missing;
	checks.expect(complete.load(completePath), "load_complete_equal_altitude_fixture");
	checks.expect(missing.load(missingPath), "load_missing_equal_altitude_fixture");
	const IRBand bands[] = {IRBand::ShortWaveInfrared, IRBand::MidWaveInfrared};
	const char* names[] = {"SWIR", "MWIR"};
	for (size_t index = 0; index < 2; ++index)
	{
		const IRBand band = bands[index];
		const std::string prefix = names[index];
		const IRModtranRadianceResult low = complete.query(Query(band, 0.001, 0.001));
		const IRModtranRadianceResult high = complete.query(Query(band, 1.0, 1.0));
		checks.expect(low.valid && low.interpolationMode == "exact_match" &&
			Near(low.tauUp, 0.8) && Near(low.directSolarIrradianceWm2Um, 2.0),
			prefix + "_low_endpoint_unchanged");
		checks.expect(high.valid && high.interpolationMode == "exact_match" &&
			Near(high.tauUp, 0.6) && Near(high.directSolarIrradianceWm2Um, 4.0),
			prefix + "_high_endpoint_unchanged");

		const double t = (0.5 - 0.001) / (1.0 - 0.001);
		const double expectedTau = std::exp(-(std::log(1.0 / 0.8) +
			(std::log(1.0 / 0.6) - std::log(1.0 / 0.8)) * t));
		const IRModtranRadianceResult middle = complete.query(Query(band, 0.5, 0.5));
		checks.expect(middle.valid && middle.interpolationMode ==
			"coupled_equal_altitude_range_visibility_solarZenith_tau_od",
			prefix + "_half_km_equal_altitude_valid");
		checks.expect(Near(middle.tauUp, expectedTau) &&
			Near(middle.pathThermalWm2SrUm, 0.1 + (0.2 - 0.1) * t) &&
			Near(middle.directSolarIrradianceWm2Um, 2.0 + (4.0 - 2.0) * t),
			prefix + "_half_km_uses_od_and_linear_components");
		checks.expect(middle.directSolarIrradianceWm2Um < 5.0,
			prefix + "_off_diagonal_candidate_excluded");

		IRModtranRadianceQuery roundTripRange = Query(band, 0.001, 0.001);
		roundTripRange.rangeKm = 0.4997593;
		const IRModtranRadianceResult snappedRange = complete.query(roundTripRange);
		checks.expect(snappedRange.valid && Near(snappedRange.tauUp, 0.8),
			prefix + "_sub_metre_range_round_trip_snaps_to_grid");
		roundTripRange.rangeKm = 0.498;
		const IRModtranRadianceResult outsideRange = complete.query(roundTripRange);
		checks.expect(!outsideRange.valid && outsideRange.fallbackReason == "out_of_range" &&
			outsideRange.fallbackAxis == "rangeKm",
			prefix + "_range_beyond_one_metre_still_fails_closed");

		const IRModtranRadianceResult unequal = complete.query(Query(band, 0.5, 0.6));
		checks.expect(!unequal.valid && unequal.fallbackReason != "none",
			prefix + "_unequal_altitude_missing_cell_rejected");
		const IRModtranRadianceResult missingCell = missing.query(Query(band, 0.5, 0.5));
		checks.expect(!missingCell.valid && missingCell.fallbackReason == "out_of_range" &&
			missingCell.fallbackAxis == "rangeKm",
			prefix + "_coupled_missing_corner_rejected");
	}

	std::cout << "SUMMARY failures=" << checks.failures << '\n';
	return checks.failures == 0 ? 0 : 1;
}
