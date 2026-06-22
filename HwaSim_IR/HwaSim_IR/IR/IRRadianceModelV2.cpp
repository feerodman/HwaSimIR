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
	tauUpSource("legacy_atmosphere"),
	tauUpValid(true),
	tauFallbackReason("none"),
	solarStrength(1.0),
	ndotl(1.0),
	textureLuma(1.0),
	solarReflectanceWeight(0.0),
	hotspotTemperatureK(300.0),
	hotspotIntensity(0.0),
	brightspotIntensity(0.0),
	plumeRadiance(0.0),
	pathRadiance(0.0),
	legacyPathRadiance(0.0),
	modtranPathRadiance(0.0),
	modtranPathRaw(0.0),
	modtranPathScaled(0.0),
	modtranPathUnitMode("Native"),
	modtranPathScale(1.0),
	modtranPathOffset(0.0),
	modtranPathBlend(1.0),
	modtranPathRuntimeMode("Off"),
	effectivePathRadiance(0.0),
	altitudeM(0.0),
	speedRawKmh(0.0),
	speedSource("unknown"),
	speedMps(0.0),
	mach(0.0),
	airTempK(0.0),
	recoveryTempK(0.0),
	aeroDeltaK(0.0),
	bodyAeroDeltaK(0.0),
	bodyAeroDeltaKRaw(0.0),
	bodyAeroDeltaKEffective(0.0),
	noseAeroDeltaK(0.0),
	edgeAeroDeltaK(0.0),
	rearAeroDeltaK(0.0),
	aeroValid(false),
	aeroFallbackReason("not_evaluated"),
	aeroAppliedToRadiance(false),
	bodyTempBaseK(300.0),
	bodyTempAeroAppliedK(300.0),
	bodyRadianceNoAero(0.0),
	bodyRadianceWithAero(0.0),
	sensorInputNoAero(0.0),
	sensorInputWithAero(0.0),
	aeroRadianceRatio(1.0),
	modtranSkyRadiance(0.0),
	modtranSolarIrradiance(0.0),
	modtranRadianceValid(false),
	materialName("unknown"),
	pathRadianceSource("disabled"),
	modtranFallbackReason("not_queried"),
	modtranInterpolationMode("none"),
	sourceFlags("body"),
	enableDebugFloor(false)
{
}

IRRadianceComponents::IRRadianceComponents()
	: band(IRBand::MidWaveInfrared),
	materialName("unknown"),
	materialTempK(300.0),
	emissivity(0.85),
	reflectance(0.15),
	bodyRadiance(0.0),
	reflectedRadiance(0.0),
	rearHotspotRadiance(0.0),
	plumeRadiance(0.0),
	brightspotRadiance(0.0),
	surfaceRadiance(0.0),
	surfaceRadianceNoAero(0.0),
	surfaceRadianceWithAero(0.0),
	tauUp(0.85),
	tauUpSource("legacy_atmosphere"),
	tauUpValid(true),
	tauFallbackReason("none"),
	pathRadiance(0.0),
	legacyPathRadiance(0.0),
	modtranPathRadiance(0.0),
	modtranPathRaw(0.0),
	modtranPathScaled(0.0),
	modtranPathUnitMode("Native"),
	modtranPathScale(1.0),
	modtranPathOffset(0.0),
	modtranPathBlend(1.0),
	modtranPathRuntimeMode("Off"),
	effectivePathRadiance(0.0),
	altitudeM(0.0),
	speedRawKmh(0.0),
	speedSource("unknown"),
	speedMps(0.0),
	mach(0.0),
	airTempK(0.0),
	recoveryTempK(0.0),
	aeroDeltaK(0.0),
	bodyAeroDeltaK(0.0),
	bodyAeroDeltaKRaw(0.0),
	bodyAeroDeltaKEffective(0.0),
	noseAeroDeltaK(0.0),
	edgeAeroDeltaK(0.0),
	rearAeroDeltaK(0.0),
	aeroValid(false),
	aeroFallbackReason("not_evaluated"),
	aeroAppliedToRadiance(false),
	bodyTempBaseK(300.0),
	bodyTempAeroAppliedK(300.0),
	bodyRadianceNoAero(0.0),
	bodyRadianceWithAero(0.0),
	sensorInputNoAero(0.0),
	sensorInputWithAero(0.0),
	aeroRadianceRatio(1.0),
	modtranSkyRadiance(0.0),
	modtranSolarIrradiance(0.0),
	modtranRadianceValid(false),
	pathRadianceSource("disabled"),
	modtranFallbackReason("not_queried"),
	modtranInterpolationMode("none"),
	sensorInputLegacy(0.0),
	sensorInputModtran(0.0),
	sensorInputRadiance(0.0),
	displayPreview(0.0),
	displayPreviewNoAero(0.0),
	displayPreviewWithAero(0.0),
	sensorInputToDisplayEnabled(false),
	sourceFlags("body")
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

IRRadianceComponents IRRadianceModelV2::evaluateComponents(const IRRadianceModelV2Input& input) const
{
	IRRadianceComponents components;
	components.band = input.band;
	components.materialName = input.materialName.empty() ? "unknown" : input.materialName;

	const double baseMaterialTemperatureK = clamp(input.materialTemperatureK, 120.0, 2500.0);
	const double aeroBodyOffsetK = input.aeroAppliedToRadiance && input.aeroValid
		? std::max(0.0, input.bodyAeroDeltaKEffective > 0.0 ? input.bodyAeroDeltaKEffective : input.bodyAeroDeltaK)
		: 0.0;
	const double materialTemperatureK = clamp(baseMaterialTemperatureK + aeroBodyOffsetK, 120.0, 2500.0);
	const double hotspotTemperatureK = clamp(input.hotspotTemperatureK, 120.0, 3500.0);
	const double emissivity = clamp(input.materialEmissivity, 0.01, 1.0);
	const double reflectance = clamp(input.materialReflectance, 0.0, 1.0);
	double tauUp = input.tauUp;
	bool tauUpValid = input.tauUpValid && std::isfinite(tauUp) && tauUp >= 0.0 && tauUp <= 1.0;
	std::string tauFallbackReason = input.tauFallbackReason.empty() ? "none" : input.tauFallbackReason;
	if (!std::isfinite(tauUp))
	{
		tauUp = 1.0;
		tauUpValid = true;
		tauFallbackReason = "nonfinite_tau_fallback_unity";
	}
	else if (tauUp < 0.0 || tauUp > 1.0)
	{
		tauUp = clamp(tauUp, 0.0, 1.0);
		tauUpValid = true;
		tauFallbackReason = "tau_out_of_range_clamped";
	}
	else if (tauUp <= 1.0e-6 && tauFallbackReason == "none")
	{
		tauUp = 1.0;
		tauUpValid = true;
		tauFallbackReason = "near_zero_tau_without_reason_fallback_unity";
	}
	const double solarStrength = clamp(input.solarStrength, 0.0, 1.0);
	const double ndotl = clamp(input.ndotl, 0.0, 1.0);
	const double textureLuma = clamp(input.textureLuma, 0.0, 1.0);
	const double solarReflectanceWeight = clamp(input.solarReflectanceWeight, 0.0, 1.0);
	const double hotspotIntensity = clamp(input.hotspotIntensity, 0.0, 8.0);
	const double brightspotIntensity = clamp(input.brightspotIntensity, 0.0, 8.0);
	const double plumeRadiance = std::max(0.0, input.plumeRadiance);
	const double pathRadiance = std::max(0.0, input.pathRadiance);
	const double legacyPathRadiance = input.legacyPathRadiance > 0.0
		? std::max(0.0, input.legacyPathRadiance)
		: (input.pathRadianceSource == "legacy_empirical" ? pathRadiance : 0.0);
	const double modtranPathRaw = input.modtranPathRaw > 0.0
		? std::max(0.0, input.modtranPathRaw)
		: std::max(0.0, input.modtranPathRadiance);
	const double modtranPathScaled = std::max(0.0, input.modtranPathScaled);
	const double modtranPathRadiance = modtranPathRaw;
	const double modtranSkyRadiance = std::max(0.0, input.modtranSkyRadiance);
	const double modtranSolarIrradiance = std::max(0.0, input.modtranSolarIrradiance);
	const double wavelengthCenterUm = bandCenterUm(input.band);

	const double referenceRadiance = std::max(
		1.0e-12,
		planckRadianceWm2SrUm(wavelengthCenterUm, referenceTemperatureK(input.band)));

	components.materialTempK = materialTemperatureK;
	components.emissivity = emissivity;
	components.reflectance = reflectance;
	components.tauUp = tauUp;
	components.tauUpSource = input.tauUpSource.empty() ? "legacy_atmosphere" : input.tauUpSource;
	components.tauUpValid = tauUpValid;
	components.tauFallbackReason = tauFallbackReason;
	components.bodyRadiance = emissivity * planckRadianceWm2SrUm(wavelengthCenterUm, materialTemperatureK);
	components.reflectedRadiance =
		reflectance *
		solarStrength *
		ndotl *
		textureLuma *
		solarReflectanceWeight;
	components.rearHotspotRadiance =
		hotspotIntensity *
		hotspotBandWeight(input.band) *
		planckRadianceWm2SrUm(wavelengthCenterUm, hotspotTemperatureK);
	components.plumeRadiance = plumeRadiance;
	components.brightspotRadiance =
		brightspotIntensity *
		brightspotBandWeight(input.band) *
		referenceRadiance;
	components.pathRadiance = pathRadiance;
	components.legacyPathRadiance = legacyPathRadiance;
	components.modtranPathRadiance = modtranPathRadiance;
	components.modtranPathRaw = modtranPathRaw;
	components.modtranPathScaled = modtranPathScaled;
	components.modtranPathUnitMode = input.modtranPathUnitMode.empty() ? "Native" : input.modtranPathUnitMode;
	components.modtranPathScale = input.modtranPathScale;
	components.modtranPathOffset = input.modtranPathOffset;
	components.modtranPathBlend = clamp(input.modtranPathBlend, 0.0, 1.0);
	components.modtranPathRuntimeMode = input.modtranPathRuntimeMode.empty() ? "Off" : input.modtranPathRuntimeMode;
	components.effectivePathRadiance = pathRadiance;
	components.altitudeM = input.altitudeM;
	components.speedRawKmh = std::max(0.0, input.speedRawKmh);
	components.speedSource = input.speedSource.empty() ? "unknown" : input.speedSource;
	components.speedMps = std::max(0.0, input.speedMps);
	components.mach = std::max(0.0, input.mach);
	components.airTempK = std::max(0.0, input.airTempK);
	components.recoveryTempK = std::max(0.0, input.recoveryTempK);
	components.aeroDeltaK = std::max(0.0, input.aeroDeltaK);
	components.bodyAeroDeltaK = std::max(0.0, input.bodyAeroDeltaK);
	components.bodyAeroDeltaKRaw = input.bodyAeroDeltaKRaw > 0.0
		? std::max(0.0, input.bodyAeroDeltaKRaw)
		: std::max(0.0, input.bodyAeroDeltaK);
	components.bodyAeroDeltaKEffective = aeroBodyOffsetK;
	components.noseAeroDeltaK = std::max(0.0, input.noseAeroDeltaK);
	components.edgeAeroDeltaK = std::max(0.0, input.edgeAeroDeltaK);
	components.rearAeroDeltaK = std::max(0.0, input.rearAeroDeltaK);
	components.aeroValid = input.aeroValid;
	components.aeroFallbackReason = input.aeroFallbackReason.empty() ? "not_evaluated" : input.aeroFallbackReason;
	components.aeroAppliedToRadiance = input.aeroAppliedToRadiance && input.aeroValid;
	components.bodyTempBaseK = baseMaterialTemperatureK;
	components.bodyTempAeroAppliedK = materialTemperatureK;
	components.modtranSkyRadiance = modtranSkyRadiance;
	components.modtranSolarIrradiance = modtranSolarIrradiance;
	components.modtranRadianceValid = input.modtranRadianceValid;
	components.pathRadianceSource = input.pathRadianceSource.empty() ? "disabled" : input.pathRadianceSource;
	components.modtranFallbackReason = input.modtranFallbackReason.empty() ? "not_queried" : input.modtranFallbackReason;
	components.modtranInterpolationMode = input.modtranInterpolationMode.empty() ? "none" : input.modtranInterpolationMode;
	const double surfaceRadiance =
		components.bodyRadiance +
		components.reflectedRadiance +
		components.rearHotspotRadiance +
		components.plumeRadiance +
		components.brightspotRadiance;
	components.surfaceRadiance = surfaceRadiance;
	components.bodyRadianceNoAero = emissivity * planckRadianceWm2SrUm(wavelengthCenterUm, baseMaterialTemperatureK);
	components.bodyRadianceWithAero = components.bodyRadiance;
	const double nonBodySurfaceRadiance =
		components.reflectedRadiance +
		components.rearHotspotRadiance +
		components.plumeRadiance +
		components.brightspotRadiance;
	components.surfaceRadianceNoAero = components.bodyRadianceNoAero + nonBodySurfaceRadiance;
	components.surfaceRadianceWithAero = components.bodyRadianceWithAero + nonBodySurfaceRadiance;
	components.sensorInputNoAero = tauUp * components.surfaceRadianceNoAero + components.pathRadiance;
	components.sensorInputWithAero = tauUp * components.surfaceRadianceWithAero + components.pathRadiance;
	components.aeroRadianceRatio = components.bodyRadianceNoAero > 1.0e-12
		? components.bodyRadianceWithAero / components.bodyRadianceNoAero
		: 1.0;
	components.sensorInputLegacy = tauUp * surfaceRadiance + components.legacyPathRadiance;
	components.sensorInputModtran = tauUp * surfaceRadiance + components.modtranPathScaled;
	components.sensorInputRadiance = tauUp * surfaceRadiance + components.pathRadiance;
	double bodyPreview = applyToneMap(
		tauUp * components.bodyRadiance,
		input.debugConfig.bodyRadianceScale,
		input.debugConfig.toneMap);
	if (input.enableDebugFloor)
	{
		const double floorValue = clamp(input.debugConfig.minBodyGray, 0.0, 0.5);
		if (components.bodyRadiance > 0.0 && bodyPreview < floorValue)
		{
			bodyPreview = floorValue;
		}
	}
	const double reflectedPreview = applyToneMap(
		tauUp * components.reflectedRadiance,
		input.debugConfig.bodyRadianceScale,
		input.debugConfig.toneMap);
	const double rearPreview = applyToneMap(
		tauUp * components.rearHotspotRadiance,
		input.debugConfig.hotspotRadianceScale,
		input.debugConfig.toneMap);
	const double plumePreview = applyToneMap(
		tauUp * components.plumeRadiance,
		input.debugConfig.hotspotRadianceScale,
		input.debugConfig.toneMap);
	const double brightPreview = applyToneMap(
		tauUp * components.brightspotRadiance,
		input.debugConfig.brightspotRadianceScale,
		input.debugConfig.toneMap);
	const double atmospherePreview = applyToneMap(
		components.pathRadiance,
		input.debugConfig.bodyRadianceScale,
		input.debugConfig.toneMap);
	components.displayPreview = clamp(
		bodyPreview + reflectedPreview + rearPreview + plumePreview + brightPreview + atmospherePreview,
		0.0,
		1.0);
	components.displayPreviewNoAero = clamp(
		applyToneMap(
			tauUp * components.surfaceRadianceNoAero + components.pathRadiance,
			input.debugConfig.bodyRadianceScale,
			input.debugConfig.toneMap),
		0.0,
		1.0);
	components.displayPreviewWithAero = clamp(
		applyToneMap(
			tauUp * components.surfaceRadianceWithAero + components.pathRadiance,
			input.debugConfig.bodyRadianceScale,
			input.debugConfig.toneMap),
		0.0,
		1.0);
	components.sourceFlags = input.sourceFlags.empty() ? "body" : input.sourceFlags;
	return components;
}

IRRadianceModelV2Output IRRadianceModelV2::evaluate(const IRRadianceModelV2Input& input) const
{
	IRRadianceModelV2Output output;
	output.wavelengthCenterUm = bandCenterUm(input.band);

	IRRadianceComponents components = evaluateComponents(input);
	const double tauUp = components.tauUp;
	output.bodyRadiance = components.bodyRadiance;
	output.reflectedRadiance = components.reflectedRadiance;
	output.hotspotRadiance = components.rearHotspotRadiance;
	output.brightspotRadiance = components.brightspotRadiance;

	output.finalRadianceDebug = components.sensorInputRadiance;

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
