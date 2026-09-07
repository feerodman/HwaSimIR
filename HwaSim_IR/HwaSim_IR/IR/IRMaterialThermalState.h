#pragma once

#include <array>
#include <string>

struct IRMaterialThermalOptions
{
	bool enabled = false;
	double defaultEffectiveThicknessM = 0.02;
	double convectionBaseWm2K = 8.0;
	double convectionSpeedCoeffWm2KPerSqrtMps = 2.5;
	double coreRelaxationWm2K = 12.0;
	double skyFactor = 0.5;
	double maxSolarDeltaK = 120.0;
};

struct IRMaterialThermalProperties
{
	double solarAbsorptivity = 0.5;
	double thermalEmissivity = 0.9;
	double densityKgM3 = 1000.0;
	double specificHeatJkgK = 1000.0;
	double conductivityWmK = 1.0;
	double effectiveThicknessM = 0.02;
	std::string thicknessSource = "fallback";
};

struct IRMaterialThermalInput
{
	double dtSec = 0.0;
	double baseTempK = 288.15;
	double aeroDeltaK = 0.0;
	double airTempK = 288.15;
	double environmentTempK = 288.15;
	double targetSpeedMps = 0.0;
	double directIrradianceWm2 = 0.0;
	double diffuseIrradianceWm2 = 0.0;
	double sunVisibilityThermal = 1.0;
	std::array<double, 3> sunDirectionLocal = {{0.0, 0.0, 1.0}};
};

struct IRMaterialThermalState
{
	std::array<double, 6> solarDeltaK = {{0.0,0.0,0.0,0.0,0.0,0.0}}; // +X,-X,+Y,-Y,+Z,-Z
	bool initialized = false;
};

struct IRMaterialThermalOutput
{
	bool valid = false;
	std::array<double, 6> solarDeltaK = {{0.0,0.0,0.0,0.0,0.0,0.0}};
	std::array<double, 6> finalTempK = {{0.0,0.0,0.0,0.0,0.0,0.0}};
	double representativeSolarDeltaK = 0.0;
	double arealHeatCapacityJm2K = 0.0;
	double convectionCoefficientWm2K = 0.0;
	double peakSolarFluxWm2 = 0.0;
	std::string fallbackReason = "not_evaluated";
};

class IRMaterialThermalModel
{
public:
	IRMaterialThermalOutput update(const IRMaterialThermalInput& input,
		const IRMaterialThermalProperties& properties,
		const IRMaterialThermalOptions& options,
		IRMaterialThermalState* state) const;
};
