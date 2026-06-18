#pragma once

#include "IRTypes.h"

#include <string>

struct IRAeroThermalOptions
{
	bool enabled = true;
	double recoveryFactor = 0.85;
	double gamma = 1.4;
	double bodyCoeff = 0.20;
	double noseCoeff = 0.60;
	double edgeCoeff = 0.45;
	double rearCoeff = 0.10;
	double heatTauSec = 2.0;
	double coolTauSec = 5.0;
	double clampMachMin = 0.0;
	double clampMachMax = 4.0;
	double clampDeltaKMax = 250.0;
};

struct IRAeroThermalInput
{
	double altitudeM = 0.0;
	double speedRaw = 0.0;
	double dtSec = 0.0;
	IRBand band = IRBand::MidWaveInfrared;
	int targetType = 0;
	int platformType = 0;
	double envTemperatureK = 0.0;
};

struct IRAeroThermalState
{
	double smoothedBodyAeroDeltaK = 0.0;
	double smoothedNoseAeroDeltaK = 0.0;
	double smoothedEdgeAeroDeltaK = 0.0;
	double smoothedRearAeroDeltaK = 0.0;
	bool initialized = false;
};

struct IRAeroThermalOutput
{
	double altitudeM = 0.0;
	double speedRaw = 0.0;
	std::string speedUnit = "km/h";
	double speedMps = 0.0;
	double airTempK = 0.0;
	double speedOfSoundMps = 0.0;
	double mach = 0.0;
	double recoveryTempK = 0.0;
	double aeroDeltaK = 0.0;
	double bodyAeroDeltaK = 0.0;
	double noseAeroDeltaK = 0.0;
	double edgeAeroDeltaK = 0.0;
	double rearAeroDeltaK = 0.0;
	bool valid = false;
	std::string fallbackReason = "not_evaluated";
};

class IRAeroThermalModel
{
public:
	IRAeroThermalOutput evaluate(
		const IRAeroThermalInput& input,
		const IRAeroThermalOptions& options,
		IRAeroThermalState* state) const;

	static double isaAirTemperatureK(double altitudeM);
	static double speedOfSoundMps(double airTempK, double gamma);
};
