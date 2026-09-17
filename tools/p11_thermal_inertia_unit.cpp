#include "../HwaSim_IR/HwaSim_IR/IR/IRMaterialThermalState.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

namespace
{
struct CheckSink
{
    int failures = 0;
    void expect(const std::string& name, bool ok, double measured, const std::string& expected)
    {
        std::cout << name << ',' << (ok ? "PASS" : "FAIL") << ','
                  << std::setprecision(17) << measured << ',' << expected << '\n';
        if (!ok) ++failures;
    }
};

IRMaterialThermalInput BaseInput()
{
    IRMaterialThermalInput input;
    input.dtSec = 1.0;
    input.baseTempK = 288.15;
    input.airTempK = 288.15;
    input.environmentTempK = 288.15;
    input.targetSpeedMps = 0.0;
    input.directIrradianceWm2 = 0.0;
    input.diffuseIrradianceWm2 = 0.0;
    input.sunVisibilityThermal = 1.0;
    input.sunDirectionLocal = {{1.0, 0.0, 0.0}};
    return input;
}

IRMaterialThermalProperties PaintProperties()
{
    IRMaterialThermalProperties properties;
    properties.solarAbsorptivity = 0.62;
    properties.thermalEmissivity = 0.86;
    properties.densityKgM3 = 1200.0;
    properties.specificHeatJkgK = 1050.0;
    properties.conductivityWmK = 0.24;
    properties.effectiveThicknessM = 0.002;
    properties.thicknessSource = "P11_controlled_paint_assumption";
    return properties;
}

IRMaterialThermalOptions Options()
{
    IRMaterialThermalOptions options;
    options.enabled = true;
    options.defaultEffectiveThicknessM = 0.02;
    options.convectionBaseWm2K = 8.0;
    options.convectionSpeedCoeffWm2KPerSqrtMps = 2.5;
    options.coreRelaxationWm2K = 12.0;
    options.skyFactor = 0.5;
    options.maxSolarDeltaK = 120.0;
    return options;
}

IRMaterialThermalOutput Step(IRMaterialThermalModel& model, IRMaterialThermalState& state,
    IRMaterialThermalInput input, const IRMaterialThermalProperties& properties,
    const IRMaterialThermalOptions& options, int count)
{
    IRMaterialThermalOutput output;
    for (int i = 0; i < count; ++i) output = model.update(input, properties, options, &state);
    return output;
}

IRRadianceComponents FormalSurface(IRBand band, double temperatureK, double directWm2Um)
{
    IRRadianceModelV2Input input;
    input.band = band;
    input.materialTemperatureK = temperatureK;
    input.materialEmissivity = 0.86;
    input.materialReflectance = 0.12;
    input.useM1Physics = true;
    input.m1RuntimeAffectsImage = true;
    input.m1TauUp = 1.0;
    input.tauUp = 1.0;
    input.tauUpValid = true;
    input.tauFallbackReason = "none";
    input.directSolarIrradiance = directWm2Um;
    input.skyDiffuseIrradiance = 0.0;
    input.pathThermalRadiance = 0.0;
    input.pathScatteringRadiance = 0.0;
    input.sunVisibility = 1.0;
    input.skyVisibility = 0.0;
    input.ndotl = 1.0;
    return IRRadianceModelV2().evaluateComponents(input);
}
}

int main()
{
    std::cout << "check,status,measured,expected\n";
    CheckSink checks;
    IRMaterialThermalModel model;
    const IRMaterialThermalProperties properties = PaintProperties();
    const IRMaterialThermalOptions options = Options();

    IRMaterialThermalState darkState;
    IRMaterialThermalInput dark = BaseInput();
    const IRMaterialThermalOutput darkOut = Step(model, darkState, dark, properties, options, 60);
    checks.expect("dark_equilibrium_no_spurious_heating",
        darkOut.valid && std::abs(darkOut.solarDeltaK[0]) < 1.0e-12,
        darkOut.solarDeltaK[0], "abs(deltaK)<1e-12");

    IRMaterialThermalState sunState;
    IRMaterialThermalInput sun = BaseInput();
    sun.directIrradianceWm2 = 800.0;
    sun.diffuseIrradianceWm2 = 100.0;
    const IRMaterialThermalOutput afterOneSecond = Step(model, sunState, sun, properties, options, 1);
    const double oneSecondHotFace = afterOneSecond.solarDeltaK[0];
    const double oneSecondBackFace = afterOneSecond.solarDeltaK[1];
    checks.expect("directional_face_heats_more_than_back",
        oneSecondHotFace > oneSecondBackFace && oneSecondBackFace > 0.0,
        oneSecondHotFace - oneSecondBackFace, ">0 K directional contrast");
    checks.expect("finite_heat_capacity_prevents_instant_temperature_jump",
        oneSecondHotFace > 0.0 && oneSecondHotFace < 0.30,
        oneSecondHotFace, "0<deltaK<0.30 after 1 s");

    const IRMaterialThermalOutput afterTenMinutes = Step(model, sunState, sun, properties, options, 599);
    checks.expect("solar_temperature_response_accumulates_with_time",
        afterTenMinutes.solarDeltaK[0] > oneSecondHotFace + 1.0,
        afterTenMinutes.solarDeltaK[0], ">one-second delta+1 K");
    checks.expect("thermal_state_remains_within_declared_limit",
        afterTenMinutes.solarDeltaK[0] <= options.maxSolarDeltaK,
        afterTenMinutes.solarDeltaK[0], "<=120 K");

    const double heatedBeforeShade = afterTenMinutes.solarDeltaK[0];
    IRMaterialThermalInput shade = BaseInput();
    shade.sunVisibilityThermal = 0.0;
    const IRMaterialThermalOutput oneSecondShade = Step(model, sunState, shade, properties, options, 1);
    checks.expect("shade_transition_is_not_instantaneous_cooling",
        oneSecondShade.solarDeltaK[0] > 0.9 * heatedBeforeShade,
        oneSecondShade.solarDeltaK[0] / heatedBeforeShade, ">0.9 retained after 1 s");
    const IRMaterialThermalOutput tenMinutesShade = Step(model, sunState, shade, properties, options, 599);
    checks.expect("shade_transition_cools_over_time",
        tenMinutesShade.solarDeltaK[0] < oneSecondShade.solarDeltaK[0],
        tenMinutesShade.solarDeltaK[0], "<one-second-shade deltaK");

    IRMaterialThermalState stillState;
    IRMaterialThermalState movingState;
    IRMaterialThermalInput still = sun;
    IRMaterialThermalInput moving = sun;
    moving.targetSpeedMps = 30.0;
    const IRMaterialThermalOutput stillOut = Step(model, stillState, still, properties, options, 600);
    const IRMaterialThermalOutput movingOut = Step(model, movingState, moving, properties, options, 600);
    checks.expect("air_speed_changes_heat_transfer_not_instant_gray_gain",
        movingOut.solarDeltaK[0] < stillOut.solarDeltaK[0] &&
        movingOut.convectionCoefficientWm2K > stillOut.convectionCoefficientWm2K,
        stillOut.solarDeltaK[0] - movingOut.solarDeltaK[0], ">0 K due to higher convection");

    const IRRadianceComponents swirDark = FormalSurface(IRBand::ShortWaveInfrared, 288.15, 0.0);
    const IRRadianceComponents swirLit = FormalSurface(IRBand::ShortWaveInfrared, 288.15, 80.0);
    const double expectedReflectedStep = 0.12 * 80.0 / 3.14159265358979323846;
    checks.expect("reflection_responds_immediately_and_separately",
        std::abs((swirLit.solarReflectedRadiance - swirDark.solarReflectedRadiance) - expectedReflectedStep) < 1.0e-12,
        swirLit.solarReflectedRadiance - swirDark.solarReflectedRadiance,
        "rho*E/pi exactly");

    const double coolMwir = IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(
        IRBand::MidWaveInfrared, 288.15);
    const double warmMwir = IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(
        IRBand::MidWaveInfrared, 288.15 + stillOut.solarDeltaK[0]);
    checks.expect("thermal_radiance_follows_delayed_temperature_state",
        warmMwir > coolMwir,
        warmMwir / coolMwir, ">1 after accumulated heating");

    return checks.failures == 0 ? 0 : 1;
}
