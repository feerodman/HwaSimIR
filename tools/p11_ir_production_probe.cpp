#include "../HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
IRRadianceModelV2Input FormalInput(IRBand band)
{
    IRRadianceModelV2Input input;
    input.band = band;
    input.materialTemperatureK = 300.0;
    input.materialEmissivity = 0.8;
    input.materialReflectance = 0.2;
    input.tauUp = 0.7;
    input.tauUpValid = true;
    input.tauFallbackReason = "none";
    // Deliberately non-zero legacy values: they must not enter the formal sum.
    input.solarStrength = 0.73;
    input.ndotl = 0.5;
    input.textureLuma = 0.41;
    input.solarReflectanceWeight = 0.15;
    input.useM1Physics = true;
    input.m1RuntimeAffectsImage = true;
    input.m1TauUp = 0.7;
    input.directSolarIrradiance = 2.0;
    input.skyDiffuseIrradiance = 0.5;
    input.pathThermalRadiance = 0.1;
    input.pathScatteringRadiance = 0.02;
    input.activeSurfaceRadiance = 0.04;
    input.activeSensorRadiance = 0.028;
    input.activeContributionEnabled = true;
    return input;
}

void PrintComponents(const IRRadianceComponents& value)
{
    std::cout
        << "{\"body\":" << value.bodyRadiance
        << ",\"legacy_empirical_reflected\":" << value.legacyEmpiricalReflectedRadiance
        << ",\"solar_reflected\":" << value.solarReflectedRadiance
        << ",\"sky_reflected\":" << value.skyReflectedRadiance
        << ",\"physical_reflected\":" << value.physicalReflectedRadiance
        << ",\"active_surface\":" << value.activeSurfaceRadiance
        << ",\"surface\":" << value.m1SurfaceRadiance
        << ",\"tau\":" << value.m1TauUp
        << ",\"path_thermal\":" << value.pathThermalRadiance
        << ",\"path_scattering\":" << value.pathScatteringRadiance
        << ",\"active_sensor\":" << value.activeSensorRadiance
        << ",\"sensor\":" << value.m1SensorRadiance << "}";
}
}

int main()
{
    IRRadianceModelV2 model;
    const std::vector<double> temperatures = {250.0, 300.0, 500.0, 900.0, 1200.0};
    const std::vector<double> taus = {0.0, 1.0e-12, 1.0e-8, 1.0e-6, 0.7, 1.0};

    std::cout << std::setprecision(17) << "{\n  \"planck\":{\n";
    bool first = true;
    for (int bandIndex = 0; bandIndex < 2; ++bandIndex)
    {
        const IRBand band = bandIndex == 0 ? IRBand::ShortWaveInfrared : IRBand::MidWaveInfrared;
        const char* bandName = bandIndex == 0 ? "SWIR" : "MWIR";
        for (size_t index = 0; index < temperatures.size(); ++index)
        {
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "    \"" << bandName << "_" << static_cast<int>(temperatures[index])
                << "K\":" << IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(
                    band, temperatures[index]);
        }
    }
    std::cout << "\n  },\n  \"formal_cases\":{\n";
    first = true;
    for (int bandIndex = 0; bandIndex < 2; ++bandIndex)
    {
        const IRBand band = bandIndex == 0 ? IRBand::ShortWaveInfrared : IRBand::MidWaveInfrared;
        const std::string bandName = bandIndex == 0 ? "SWIR" : "MWIR";
        struct NamedInput { std::string name; IRRadianceModelV2Input input; };
        std::vector<NamedInput> cases;
        IRRadianceModelV2Input base = FormalInput(band);
        cases.push_back({"base", base});
        IRRadianceModelV2Input noDirect = base; noDirect.directSolarIrradiance = 0.0;
        cases.push_back({"no_direct", noDirect});
        IRRadianceModelV2Input noSky = base; noSky.skyDiffuseIrradiance = 0.0;
        cases.push_back({"no_sky", noSky});
        IRRadianceModelV2Input noPath = base; noPath.pathThermalRadiance = 0.0; noPath.pathScatteringRadiance = 0.0;
        cases.push_back({"no_path", noPath});
        IRRadianceModelV2Input noActive = base; noActive.activeSurfaceRadiance = 0.0;
        noActive.activeSensorRadiance = 0.0; noActive.activeContributionEnabled = false;
        cases.push_back({"no_active", noActive});
        IRRadianceModelV2Input legacyPerturbed = base; legacyPerturbed.solarStrength = 1234.0;
        legacyPerturbed.textureLuma = 1.0; legacyPerturbed.solarReflectanceWeight = 8.0;
        cases.push_back({"legacy_perturbed", legacyPerturbed});
        for (size_t index = 0; index < cases.size(); ++index)
        {
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "    \"" << bandName << "_" << cases[index].name << "\":";
            PrintComponents(model.evaluateComponents(cases[index].input));
        }
    }
    std::cout << "\n  },\n  \"tau_cases\":{\n";
    first = true;
    for (int bandIndex = 0; bandIndex < 2; ++bandIndex)
    {
        const IRBand band = bandIndex == 0 ? IRBand::ShortWaveInfrared : IRBand::MidWaveInfrared;
        const char* bandName = bandIndex == 0 ? "SWIR" : "MWIR";
        for (size_t index = 0; index < taus.size(); ++index)
        {
            IRRadianceModelV2Input input = FormalInput(band);
            input.materialReflectance = 0.0;
            input.directSolarIrradiance = 0.0;
            input.skyDiffuseIrradiance = 0.0;
            input.activeSurfaceRadiance = 0.0;
            input.activeSensorRadiance = 0.0;
            input.activeContributionEnabled = false;
            input.pathThermalRadiance = 0.125;
            input.pathScatteringRadiance = 0.0;
            input.m1TauUp = taus[index];
            if (!first) std::cout << ",\n";
            first = false;
            std::cout << "    \"" << bandName << "_" << index << "\":";
            PrintComponents(model.evaluateComponents(input));
        }
    }
    std::cout << "\n  },\n  \"tau_values\":[";
    for (size_t index = 0; index < taus.size(); ++index)
    {
        if (index) std::cout << ',';
        std::cout << taus[index];
    }
    std::cout << "]\n}\n";
    return 0;
}
