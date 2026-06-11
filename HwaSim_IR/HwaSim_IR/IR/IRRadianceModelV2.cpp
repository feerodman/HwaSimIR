#include "IRRadianceModelV2.h"

#include <algorithm>
#include <cmath>

IRRadianceModelV2DebugConfig::IRRadianceModelV2DebugConfig()
	: toneMap(IRStage5ToneMap::Asinh),
	bodyRadianceScale(0.35),
	hotspotRadianceScale(0.003),
	brightspotRadianceScale(0.004),
	minBodyGray(0.12),
	solarReflectanceWeight(0.0),
	bodyDisplayGain(1.0),
	reflectedDisplayGain(1.0),
	hotspotDisplayGain(1.0),
	brightspotDisplayGain(1.0),
	compositeMinGray(0.0),
	compositeMaxGray(1.0)
{
}

IRRadianceModelV2Input::IRRadianceModelV2Input()
	: band(IRBand::MidWaveInfrared),
	materialTemperatureK(300.0),
	materialEmissivity(0.85),
	materialReflectance(0.15),
	tauUp(0.85),
	solarStrength(1.0),
	ndotl(1.0),
	textureLuma(1.0),
	solarReflectanceWeight(0.0),
	hotspotTemperatureK(300.0),
	hotspotIntensity(0.0),
	brightspotIntensity(0.0),
	enableDebugFloor(false)
{
}

IRRadianceModelV2Output::IRRadianceModelV2Output()
	: wavelengthCenterUm(4.0),
	bodyRadiance(0.0),
	reflectedRadiance(0.0),
	hotspotRadiance(0.0),
	brightspotRadiance(0.0),
	finalRadianceDebug(0.0),
	bodyGrayBeforeFloor(0.0),
	bodyGrayAfterFloor(0.0),
	reflectedGray(0.0),
	hotspotGray(0.0),
	brightspotGray(0.0),
	finalGrayDebug(0.0),
	debugFloorApplied(false)
{
}

IRRadianceModelV2Output IRRadianceModelV2::evaluate(const IRRadianceModelV2Input& input) const
{
	IRRadianceModelV2Output output;
	output.wavelengthCenterUm = bandCenterUm(input.band);

	const double materialTemperatureK = clamp(input.materialTemperatureK, 120.0, 2500.0);
	const double hotspotTemperatureK = clamp(input.hotspotTemperatureK, 120.0, 3500.0);
	const double emissivity = clamp(input.materialEmissivity, 0.01, 1.0);
	const double reflectance = clamp(input.materialReflectance, 0.0, 1.0);
	const double tauUp = clamp(input.tauUp, 0.0, 1.0);
	const double solarStrength = clamp(input.solarStrength, 0.0, 1.0);
	const double ndotl = clamp(input.ndotl, 0.0, 1.0);
	const double textureLuma = clamp(input.textureLuma, 0.0, 1.0);
	const double solarReflectanceWeight = clamp(input.solarReflectanceWeight, 0.0, 1.0);
	const double hotspotIntensity = clamp(input.hotspotIntensity, 0.0, 8.0);
	const double brightspotIntensity = clamp(input.brightspotIntensity, 0.0, 8.0);

	const double referenceRadiance = std::max(
		1.0e-12,
		planckRadianceWm2SrUm(output.wavelengthCenterUm, referenceTemperatureK(input.band)));

	output.bodyRadiance = emissivity * planckRadianceWm2SrUm(output.wavelengthCenterUm, materialTemperatureK);
	output.reflectedRadiance =
		reflectance *
		solarStrength *
		ndotl *
		textureLuma *
		solarReflectanceWeight;
	output.hotspotRadiance =
		hotspotIntensity *
		hotspotBandWeight(input.band) *
		planckRadianceWm2SrUm(output.wavelengthCenterUm, hotspotTemperatureK);
	output.brightspotRadiance =
		brightspotIntensity *
		brightspotBandWeight(input.band) *
		referenceRadiance;

	output.finalRadianceDebug =
		tauUp *
		(output.bodyRadiance + output.reflectedRadiance + output.hotspotRadiance + output.brightspotRadiance);

	output.bodyGrayBeforeFloor = applyToneMap(
		tauUp * output.bodyRadiance,
		input.debugConfig.bodyRadianceScale,
		input.debugConfig.toneMap);
	output.hotspotGray = applyToneMap(
		tauUp * output.hotspotRadiance,
		input.debugConfig.hotspotRadianceScale,
		input.debugConfig.toneMap);
	output.reflectedGray = applyToneMap(
		tauUp * output.reflectedRadiance,
		input.debugConfig.bodyRadianceScale,
		input.debugConfig.toneMap);
	output.brightspotGray = applyToneMap(
		tauUp * output.brightspotRadiance,
		input.debugConfig.brightspotRadianceScale,
		input.debugConfig.toneMap);
	output.bodyGrayAfterFloor = output.bodyGrayBeforeFloor;

	// Debug gray is a temporary visibility normalization for Stage 5A, not final sensor calibration.
	if (input.enableDebugFloor)
	{
		const double floorValue = clamp(input.debugConfig.minBodyGray, 0.0, 0.5);
		if (output.bodyRadiance > 0.0 && output.bodyGrayAfterFloor < floorValue)
		{
			output.bodyGrayAfterFloor = floorValue;
			output.debugFloorApplied = true;
		}
	}
	output.finalGrayDebug = clamp(
		output.bodyGrayAfterFloor + output.reflectedGray + output.hotspotGray + output.brightspotGray,
		0.0,
		1.0);

	return output;
}

double IRRadianceModelV2::bandCenterUm(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 0.55;
	case IRBand::NearInfrared: return 0.90;
	case IRBand::ShortWaveInfrared: return 1.80;
	case IRBand::MidWaveInfrared: return 4.00;
	case IRBand::LongWaveInfrared: return 10.0;
	default: return 4.00;
	}
}

double IRRadianceModelV2::planckRadianceWm2SrUm(double wavelengthUm, double temperatureK)
{
	const double lambdaUm = clamp(wavelengthUm, 0.1, 100.0);
	const double temperature = clamp(temperatureK, 1.0, 6000.0);
	// Constants are matched to wavelength in um; result is W/(m^2 sr um).
	const double c1 = 1.191042e8;
	const double c2 = 1.4387752e4;
	const double exponent = clamp(c2 / (lambdaUm * temperature), 1.0e-9, 700.0);
	const double denominator = std::pow(lambdaUm, 5.0) * (std::exp(exponent) - 1.0);
	if (denominator <= 0.0)
	{
		return 0.0;
	}
	return c1 / denominator;
}

double IRRadianceModelV2::applyToneMap(double radiance, double scale, IRStage5ToneMap toneMap)
{
	const double x = std::max(0.0, radiance * std::max(0.0, scale));
	double mapped = x;
	switch (toneMap)
	{
	case IRStage5ToneMap::Log:
		mapped = std::log(1.0 + x);
		break;
	case IRStage5ToneMap::Asinh:
		mapped = std::log(x + std::sqrt(x * x + 1.0));
		break;
	case IRStage5ToneMap::Linear:
	default:
		mapped = x;
		break;
	}
	return clamp(mapped, 0.0, 1.0);
}

double IRRadianceModelV2::referenceTemperatureK(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 1200.0;
	case IRBand::NearInfrared: return 1100.0;
	case IRBand::ShortWaveInfrared: return 900.0;
	case IRBand::MidWaveInfrared: return 760.0;
	case IRBand::LongWaveInfrared: return 330.0;
	default: return 760.0;
	}
}

double IRRadianceModelV2::hotspotBandWeight(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 0.0;
	case IRBand::NearInfrared: return 0.08;
	case IRBand::ShortWaveInfrared: return 0.18;
	case IRBand::MidWaveInfrared: return 1.0;
	case IRBand::LongWaveInfrared: return 0.75;
	default: return 1.0;
	}
}

double IRRadianceModelV2::brightspotBandWeight(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return 0.18;
	case IRBand::NearInfrared: return 0.55;
	case IRBand::ShortWaveInfrared: return 0.70;
	case IRBand::MidWaveInfrared: return 0.35;
	case IRBand::LongWaveInfrared: return 0.02;
	default: return 0.35;
	}
}

double IRRadianceModelV2::clamp(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}
