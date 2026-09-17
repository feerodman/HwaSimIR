#pragma once

#include "IRTypes.h"

#include <array>
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
	std::string speedSource = "unknown";
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
	double speedRawKmh = 0.0;
	std::string selectedSpeedSource = "unknown";
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

// Spatial distribution is deliberately separate from the recovery-temperature
// model.  The recovery model supplies candidate deltas; this contract prevents
// any of them from being added to the complete target body.
struct IRAeroSpatialInput
{
	std::array<double, 3> localPosition = {{0.0, 0.0, 0.0}};
	std::array<double, 3> localNormal = {{0.0, 1.0, 0.0}};
	std::array<double, 3> boundsCenter = {{0.0, 0.0, 0.0}};
	std::array<double, 3> boundsHalfExtent = {{1.0, 1.0, 1.0}};
	std::array<double, 3> forwardAxis = {{0.0, 1.0, 0.0}};
	double noseDeltaK = 0.0;
	double edgeDeltaK = 0.0;
	double rearDeltaK = 0.0;
};

struct IRAeroSpatialOutput
{
	double noseMask = 0.0;
	double edgeMask = 0.0;
	double rearMask = 0.0;
	double deltaK = 0.0;
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
	static IRAeroSpatialOutput evaluateSpatial(const IRAeroSpatialInput& input);
};
