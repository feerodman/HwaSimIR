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

double SmoothStep(double edge0, double edge1, double value)
{
	if (edge1 <= edge0) return value >= edge1 ? 1.0 : 0.0;
	const double t = Clamp((value - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

std::array<double, 3> Normalize3(const std::array<double, 3>& value, const std::array<double, 3>& fallback)
{
	const double length = std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
	if (!std::isfinite(length) || length <= 1.0e-12) return fallback;
	return {{value[0] / length, value[1] / length, value[2] / length}};
}

double Dot3(const std::array<double, 3>& left, const std::array<double, 3>& right)
{
	return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
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

IRAeroSpatialOutput IRAeroThermalModel::evaluateSpatial(const IRAeroSpatialInput& input)
{
	IRAeroSpatialOutput output;
	std::array<double, 3> normalizedPosition;
	for (int axis = 0; axis < 3; ++axis)
	{
		const double extent = std::max(1.0e-3, std::fabs(input.boundsHalfExtent[axis]));
		normalizedPosition[axis] = Clamp(
			(input.localPosition[axis] - input.boundsCenter[axis]) / extent, -1.5, 1.5);
	}
	const std::array<double, 3> forward = Normalize3(input.forwardAxis, {{0.0, 1.0, 0.0}});
	const std::array<double, 3> normal = Normalize3(input.localNormal, forward);
	const double axial = Clamp(Dot3(normalizedPosition, forward), -1.0, 1.0);
	const std::array<double, 3> radialVector = {{
		normalizedPosition[0] - forward[0] * axial,
		normalizedPosition[1] - forward[1] * axial,
		normalizedPosition[2] - forward[2] * axial}};
	const double radial = Clamp(std::sqrt(Dot3(radialVector, radialVector)) / std::sqrt(2.0), 0.0, 1.0);
	const double normalAlignment = Clamp(Dot3(normal, forward), -1.0, 1.0);

	output.noseMask = SmoothStep(0.35, 0.95, axial) *
		SmoothStep(0.05, 0.65, std::max(0.0, normalAlignment));
	output.edgeMask = SmoothStep(0.55, 0.95, radial) *
		SmoothStep(0.05, 0.85, axial) *
		SmoothStep(0.10, 0.90, 1.0 - std::fabs(normalAlignment));
	output.rearMask = SmoothStep(0.45, 0.95, -axial) *
		SmoothStep(0.05, 0.65, std::max(0.0, -normalAlignment));
	output.deltaK = std::max(
		std::max(output.noseMask * std::max(0.0, input.noseDeltaK),
			output.edgeMask * std::max(0.0, input.edgeDeltaK)),
		output.rearMask * std::max(0.0, input.rearDeltaK));
	return output;
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
