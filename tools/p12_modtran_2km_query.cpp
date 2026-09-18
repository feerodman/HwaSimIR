#include "../HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {
struct Checks {
    int failures = 0;
    void expect(bool value, const std::string& name) {
        std::cout << (value ? "PASS " : "FAIL ") << name << '\n';
        if (!value) ++failures;
    }
};

IRModtranRadianceQuery Query(IRBand band, double range, double altitude,
                            double visibility, double sza,
                            const std::string& humidity = "default") {
    IRModtranRadianceQuery query;
    query.band = band;
    query.atmosphereModel = "Mid-Latitude Summer";
    query.aerosolModel = "Rural";
    query.humidityProfile = humidity;
    query.observerAltKm = altitude;
    query.targetAltKm = altitude;
    query.rangeKm = range;
    query.visibilityKm = visibility;
    query.solarZenithDeg = sza;
    return query;
}

bool FiveComponents(const IRModtranRadianceResult& value) {
    return value.valid && value.tauUp >= 0.0 && value.tauUp <= 1.0 &&
        std::isfinite(value.pathThermalWm2SrUm) && value.pathThermalWm2SrUm >= 0.0 &&
        std::isfinite(value.directSolarIrradianceWm2Um) && value.directSolarIrradianceWm2Um > 0.0 &&
        std::isfinite(value.downwardSkyDiffuseIrradianceWm2Um) && value.downwardSkyDiffuseIrradianceWm2Um > 0.0 &&
        std::isfinite(value.pathScatteringRadianceWm2SrUm) && value.pathScatteringRadianceWm2SrUm >= 0.0;
}
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: p12_modtran_2km_query <formal_lut.csv>\n"; return 2; }
    Checks checks;
    IRModtranRadianceLut lut;
    checks.expect(lut.load(argv[1]), "formal_lut_loads");
    checks.expect(lut.hasBand(IRBand::ShortWaveInfrared) && lut.hasBand(IRBand::MidWaveInfrared),
                  "swir_and_mwir_present");
    const IRBand bands[] = {IRBand::ShortWaveInfrared, IRBand::MidWaveInfrared};
    const double altitudes[] = {0.001, 1.0};
    const double visibilities[] = {6.0, 23.0};
    const double szas[] = {20.0, 45.0, 70.0};
    const char* profiles[] = {"default", "scaled_mls_surface_rh30", "scaled_mls_surface_rh60", "scaled_mls_surface_rh85"};
    int exactCount = 0;
    for (IRBand band : bands) for (const char* profile : profiles)
        for (double altitude : altitudes) for (double visibility : visibilities) for (double sza : szas) {
            const IRModtranRadianceResult value = lut.query(Query(band, 2.0, altitude, visibility, sza, profile));
            checks.expect(FiveComponents(value) && value.interpolationMode == "exact_match",
                          "exact_2km_five_component_vertex");
            checks.expect(value.sourceCaseIds.find("P12_") != std::string::npos,
                          "exact_2km_provenance_names_p12_real_los");
            ++exactCount;
        }
    for (IRBand band : bands) {
        const IRModtranRadianceResult interior = lut.query(Query(band, 1.5, 0.5, 10.0, 32.5));
        checks.expect(FiveComponents(interior) && interior.interpolationMode ==
                      "coupled_equal_altitude_range_visibility_solarZenith_tau_od",
                      "one_to_two_km_interior_interpolation_valid");
        const IRModtranRadianceResult rh40 = lut.queryRelativeHumidity(Query(band, 2.0, 0.001, 6.0, 45.0), 40.0);
        const IRModtranRadianceResult rh76 = lut.queryRelativeHumidity(Query(band, 2.0, 0.001, 23.0, 45.0), 76.18);
        checks.expect(FiveComponents(rh40) && FiveComponents(rh76), "numeric_humidity_2km_valid");
        const IRModtranRadianceResult outside = lut.query(Query(band, 2.01, 0.001, 6.0, 45.0));
        checks.expect(!outside.valid && outside.fallbackReason == "out_of_range" &&
                      outside.fallbackAxis == "rangeKm" && outside.fallbackMax == 2.0,
                      "range_above_2km_remains_fail_closed");
    }
    std::cout << "SUMMARY failures=" << checks.failures << " exactVertices=" << exactCount
              << " loadedEntries=" << lut.entryCount() << '\n';
    return checks.failures == 0 ? 0 : 1;
}
