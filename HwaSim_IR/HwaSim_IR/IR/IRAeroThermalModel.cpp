#include "IRAeroThermalModel.h"

#include <algorithm>
#include <cmath>

namespace
{
double Clamp(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

double SmoothDelta(double previous, double target, double dtSec, double heatTauSec, double coolTauSec, bool initialized)
{
	if (!initialized)
	{
		return target;
	}
	const double tau = target >= previous ? heatTauSec : coolTauSec;
	if (tau <= 1.0e-6 || dtSec <= 0.0)
	{
		return target;
	}
	const double alpha = 1.0 - std::exp(-dtSec / tau);
	return previous + (target - previous) * Clamp(alpha, 0.0, 1.0);
}
}

double IRAeroThermalModel::isaAirTemperatureK(double altitudeM)
{
	const double altitude = Clamp(altitudeM, 0.0, 20000.0);
	if (altitude <= 11000.0)
	{
		return 288.15 - 0.0065 * altitude;
	}
	return 216.65;
}

double IRAeroThermalModel::speedOfSoundMps(double airTempK, double gamma)
{
	const double safeGamma = Clamp(gamma, 1.01, 2.0);
	const double safeTempK = Clamp(airTempK, 120.0, 400.0);
	const double gasConstantDryAir = 287.05287;
	return std::sqrt(safeGamma * gasConstantDryAir * safeTempK);
}

IRAeroThermalOutput IRAeroThermalModel::evaluate(
	const IRAeroThermalInput& input,
	const IRAeroThermalOptions& options,
	IRAeroThermalState* state) const
{
	IRAeroThermalOutput output;
	output.altitudeM = input.altitudeM;
	output.speedRaw = input.speedRaw;
	output.speedRawKmh = input.speedRaw;
	output.selectedSpeedSource = input.speedSource.empty() ? "unknown" : input.speedSource;
	output.speedUnit = "km/h";

	if (!options.enabled)
	{
		output.fallbackReason = "model_disabled";
		return output;
	}
	if (!std::isfinite(input.altitudeM) || !std::isfinite(input.speedRaw))
	{
		output.fallbackReason = "invalid_input";
		return output;
	}
	if (input.altitudeM < -500.0 || input.altitudeM > 100000.0 || input.speedRaw < 0.0)
	{
		output.fallbackReason = "out_of_range";
		return output;
	}

	const double gamma = Clamp(options.gamma, 1.01, 2.0);
	const double recoveryFactor = Clamp(options.recoveryFactor, 0.0, 1.0);
	const double machMin = std::max(0.0, options.clampMachMin);
	const double machMax = std::max(machMin, options.clampMachMax);
	const double deltaMax = std::max(0.0, options.clampDeltaKMax);
	const double airTempK = input.envTemperatureK > 100.0
		? Clamp(input.envTemperatureK, 120.0, 400.0)
		: isaAirTemperatureK(input.altitudeM);
	const double soundMps = speedOfSoundMps(airTempK, gamma);
	const double speedMps = std::max(0.0, input.speedRaw / 3.6);
	const double rawMach = soundMps > 1.0e-6 ? speedMps / soundMps : 0.0;
	const double mach = Clamp(rawMach, machMin, machMax);
	const double recoveryTempK = airTempK * (1.0 + recoveryFactor * (gamma - 1.0) * 0.5 * mach * mach);
	const double aeroDeltaK = Clamp(std::max(0.0, recoveryTempK - airTempK), 0.0, deltaMax);
	const double bodyTarget = Clamp(aeroDeltaK * std::max(0.0, options.bodyCoeff), 0.0, deltaMax);
	const double noseTarget = Clamp(aeroDeltaK * std::max(0.0, options.noseCoeff), 0.0, deltaMax);
	const double edgeTarget = Clamp(aeroDeltaK * std::max(0.0, options.edgeCoeff), 0.0, deltaMax);
	const double rearTarget = Clamp(aeroDeltaK * std::max(0.0, options.rearCoeff), 0.0, deltaMax);
	const double dtSec = Clamp(input.dtSec, 0.0, 10.0);
	const bool initialized = state && state->initialized;

	output.valid = true;
	output.fallbackReason = "none";
	output.speedMps = speedMps;
	output.airTempK = airTempK;
	output.speedOfSoundMps = soundMps;
	output.mach = mach;
	output.recoveryTempK = recoveryTempK;
	output.aeroDeltaK = aeroDeltaK;
	output.bodyAeroDeltaK = SmoothDelta(
		initialized ? state->smoothedBodyAeroDeltaK : bodyTarget,
		bodyTarget,
		dtSec,
		options.heatTauSec,
		options.coolTauSec,
		initialized);
	output.noseAeroDeltaK = SmoothDelta(
		initialized ? state->smoothedNoseAeroDeltaK : noseTarget,
		noseTarget,
		dtSec,
		options.heatTauSec,
		options.coolTauSec,
		initialized);
	output.edgeAeroDeltaK = SmoothDelta(
		initialized ? state->smoothedEdgeAeroDeltaK : edgeTarget,
		edgeTarget,
		dtSec,
		options.heatTauSec,
		options.coolTauSec,
		initialized);
	output.rearAeroDeltaK = SmoothDelta(
		initialized ? state->smoothedRearAeroDeltaK : rearTarget,
		rearTarget,
		dtSec,
		options.heatTauSec,
		options.coolTauSec,
		initialized);

	if (state)
	{
		state->smoothedBodyAeroDeltaK = output.bodyAeroDeltaK;
		state->smoothedNoseAeroDeltaK = output.noseAeroDeltaK;
		state->smoothedEdgeAeroDeltaK = output.edgeAeroDeltaK;
		state->smoothedRearAeroDeltaK = output.rearAeroDeltaK;
		state->initialized = true;
	}
	return output;
}
