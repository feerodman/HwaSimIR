#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const char* kHeader =
	"band,atmosphere_model,aerosol_model,humidity_profile,visibility_km,"
	"observer_alt_km,target_alt_km,range_km,solar_zenith_deg,tau_up,"
	"path_thermal_W_m2_sr_um,direct_solar_irradiance_at_target_W_m2_um,"
	"downward_sky_diffuse_irradiance_W_m2_um,los_path_scattering_radiance_W_m2_sr_um,"
	"radiance_unit,irradiance_unit,response_mode,source_case_ids,source_files";

std::string JoinPath(const std::string& directory, const std::string& name)
{
	if (directory.empty()) return name;
	const char last = directory[directory.size() - 1];
	return directory + ((last == '/' || last == '\\') ? "" : "/") + name;
}

std::string JoinCsv(const std::vector<std::string>& fields)
{
	std::ostringstream stream;
	for (size_t index = 0; index < fields.size(); ++index)
	{
		if (index != 0) stream << ',';
		stream << fields[index];
	}
	return stream.str();
}

std::vector<std::string> SwirFields()
{
	return {
		"SWIR", "Mid-Latitude Summer", "Rural", "default", "23",
		"3", "3", "1", "45", "0.8", "0.01", "1.5", "0.3", "0.02",
		"W/(m^2 sr um)", "W/(m^2 um)", "RectangularBand", "SWIR_test", "unit_test"
	};
}

std::vector<std::string> MwirFields()
{
	return {
		"MWIR", "Mid-Latitude Summer", "Rural", "default", "23",
		"3", "3", "1", "45", "0.8", "0.01", "1.5", "0.3", "0.02",
		"W/(m^2 sr um)", "W/(m^2 um)", "RectangularBand", "MWIR_test", "unit_test"
	};
}

std::vector<std::string> LegacyMwirFields()
{
	std::vector<std::string> fields = MwirFields();
	fields[8].clear();
	fields[11].clear();
	fields[12].clear();
	fields[13].clear();
	return fields;
}

bool WriteCsv(const std::string& path, const std::vector<std::string>& fields)
{
	std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
	if (!output.is_open()) return false;
	output << kHeader << '\n' << JoinCsv(fields) << '\n';
	return output.good();
}

IRModtranRadianceQuery SwirQuery()
{
	IRModtranRadianceQuery query;
	query.band = IRBand::ShortWaveInfrared;
	query.atmosphereModel = "Mid-Latitude Summer";
	query.aerosolModel = "Rural";
	query.humidityProfile = "default";
	query.visibilityKm = 23.0;
	query.observerAltKm = 3.0;
	query.targetAltKm = 3.0;
	query.rangeKm = 1.0;
	query.solarZenithDeg = 45.0;
	return query;
}

IRModtranRadianceQuery MwirQuery()
{
	IRModtranRadianceQuery query = SwirQuery();
	query.band = IRBand::MidWaveInfrared;
	return query;
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
	const std::string outputDirectory = argc > 1 ? argv[1] : ".";
	Checks checks;

	const std::string noSwirPath = JoinPath(outputDirectory, "lut_without_swir.csv");
	checks.expect(WriteCsv(noSwirPath, MwirFields()), "write_lut_without_swir");
	IRModtranRadianceLut noSwir;
	checks.expect(noSwir.load(noSwirPath), "mwir_only_lut_loads");
	checks.expect(!noSwir.hasBand(IRBand::ShortWaveInfrared), "missing_swir_band_is_reported");
	const IRModtranRadianceResult missingBand = noSwir.query(SwirQuery());
	checks.expect(!missingBand.valid && missingBand.fallbackReason == "category_missing",
		"missing_swir_query_is_invalid");

	const std::string legacyMwirPath = JoinPath(outputDirectory, "legacy_mwir_tau_path_only.csv");
	checks.expect(WriteCsv(legacyMwirPath, LegacyMwirFields()), "write_legacy_mwir_tau_path_only");
	IRModtranRadianceLut legacyMwir;
	checks.expect(!legacyMwir.load(legacyMwirPath), "reject_legacy_mwir_missing_solar_components");

	const size_t requiredMwirFieldIndices[] = {9, 10, 11, 12, 13};
	const char* requiredMwirFieldNames[] = {
		"tau", "thermal", "direct_solar", "diffuse_sky", "path_scattering"
	};
	for (size_t index = 0; index < sizeof(requiredMwirFieldIndices) / sizeof(requiredMwirFieldIndices[0]); ++index)
	{
		std::vector<std::string> fields = MwirFields();
		fields[requiredMwirFieldIndices[index]].clear();
		const std::string path = JoinPath(outputDirectory,
			std::string("mwir_missing_") + requiredMwirFieldNames[index] + ".csv");
		checks.expect(WriteCsv(path, fields), std::string("write_mwir_missing_") + requiredMwirFieldNames[index]);
		IRModtranRadianceLut incomplete;
		checks.expect(!incomplete.load(path),
			std::string("reject_mwir_missing_") + requiredMwirFieldNames[index]);
	}

	const std::string validMwirPath = JoinPath(outputDirectory, "mwir_complete.csv");
	checks.expect(WriteCsv(validMwirPath, MwirFields()), "write_complete_mwir");
	IRModtranRadianceLut validMwir;
	checks.expect(validMwir.load(validMwirPath), "complete_mwir_lut_loads");
	checks.expect(validMwir.hasBand(IRBand::MidWaveInfrared), "complete_mwir_band_available");
	const IRModtranRadianceResult exactMwir = validMwir.query(MwirQuery());
	checks.expect(exactMwir.valid && exactMwir.interpolationMode == "exact_match" &&
		exactMwir.directSolarIrradianceWm2Um == 1.5,
		"complete_mwir_exact_five_axis_query");
	IRModtranRadianceQuery wrongSun = MwirQuery();
	wrongSun.solarZenithDeg = 20.0;
	const IRModtranRadianceResult wrongSunResult = validMwir.query(wrongSun);
	checks.expect(!wrongSunResult.valid && wrongSunResult.fallbackReason == "out_of_range" &&
		wrongSunResult.fallbackAxis == "solarZenithDeg", "mwir_sza_axis_is_mandatory");

	const size_t requiredSwirFieldIndices[] = {9, 10, 11, 12, 13};
	const char* requiredSwirFieldNames[] = {
		"tau", "thermal", "direct_solar", "diffuse_sky", "path_scattering"
	};
	for (size_t index = 0; index < sizeof(requiredSwirFieldIndices) / sizeof(requiredSwirFieldIndices[0]); ++index)
	{
		std::vector<std::string> fields = SwirFields();
		fields[requiredSwirFieldIndices[index]].clear();
		const std::string path = JoinPath(outputDirectory,
			std::string("swir_missing_") + requiredSwirFieldNames[index] + ".csv");
		checks.expect(WriteCsv(path, fields), std::string("write_swir_missing_") + requiredSwirFieldNames[index]);
		IRModtranRadianceLut incomplete;
		checks.expect(!incomplete.load(path),
			std::string("reject_swir_missing_") + requiredSwirFieldNames[index]);
	}

	const std::string validSwirPath = JoinPath(outputDirectory, "swir_complete.csv");
	checks.expect(WriteCsv(validSwirPath, SwirFields()), "write_complete_swir");
	IRModtranRadianceLut validSwir;
	checks.expect(validSwir.load(validSwirPath), "complete_swir_lut_loads");
	checks.expect(validSwir.hasBand(IRBand::ShortWaveInfrared), "complete_swir_band_available");
	const IRModtranRadianceResult exact = validSwir.query(SwirQuery());
	checks.expect(exact.valid && exact.interpolationMode == "exact_match", "complete_swir_exact_query");

	IRModtranRadianceQuery nearRange = SwirQuery();
	nearRange.rangeKm = 0.5;
	const IRModtranRadianceResult nearResult = validSwir.query(nearRange);
	checks.expect(!nearResult.valid && nearResult.fallbackReason == "out_of_range" &&
		nearResult.fallbackAxis == "rangeKm" && nearResult.fallbackQuery == 0.5 &&
		nearResult.fallbackMin == 1.0 && nearResult.fallbackMax == 1.0,
		"near_range_out_of_range_is_explicit");

	std::cout << "SUMMARY failures=" << checks.failures << '\n';
	return checks.failures == 0 ? 0 : 1;
}
