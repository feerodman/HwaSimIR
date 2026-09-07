#include "IRActiveIlluminator.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
std::string lower(const std::string& value)
{
	std::string out = value;
	std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return out;
}
}

IRActiveIlluminatorConfig::IRActiveIlluminatorConfig()
	: enabled(false),
	band(IRActiveIlluminatorBand::NearInfrared),
	centerWavelengthUm(0.85),
	bandwidthUm(0.05),
	beamProfile(IRActiveBeamProfile::Gaussian),
	intensityMode(IRActiveIntensityMode::LegacyNormalized),
	referenceRangeM(1000.0),
	referenceIrradianceWm2(1.0),
	legacyMaxReferenceIrradianceWm2(1.0),
	useAtmosphericAttenuation(true),
	useGeometricOcclusion(true),
	debugLog(false),
	debugVisualCone(false)
{
}

IRActiveIlluminatorInput::IRActiveIlluminatorInput()
	: protocolEnabled(false),
	sensorBand(IRBand::NearInfrared),
	sensorLowUm(0.70),
	sensorHighUm(1.10),
	protocolAngleMrad(0.0),
	protocolSpotRadiance(0.0),
	rangeM(0.0),
	beamAngleRad(0.0),
	surfaceNdotL(1.0),
	tauInbound(1.0),
	tauInboundValid(false),
	tauFallbackReason("not_queried"),
	activeVisibility(1.0),
	bandReflectance(0.0),
	reflectanceSource("fallback")
{
}

IRActiveIlluminatorOutput::IRActiveIlluminatorOutput()
	: activeContributionEnabled(false),
	spectralOverlap(false),
	sourceBand("NIR"),
	sensorBand("NIR"),
	sourceLowUm(0.825),
	sourceHighUm(0.875),
	protocolAngleMrad(0.0),
	halfAngleRad(0.0),
	rangeM(0.0),
	beamAngleRad(0.0),
	beamFactor(0.0),
	spotRadiusM(0.0),
	referenceIrradianceWm2(0.0),
	geometricIrradianceWm2(0.0),
	tauOutbound(1.0),
	tauInbound(1.0),
	targetIrradianceWm2(0.0),
	incidentIrradianceWm2(0.0),
	activeSurfaceRadianceWm2Sr(0.0),
	activeSensorRadianceWm2Sr(0.0),
	spectralOverlapWidthUm(0.0),
	activeSurfaceRadianceWm2SrUm(0.0),
	activeSensorRadianceWm2SrUm(0.0),
	activeVisibility(1.0),
	intensitySource("disabled"),
	outboundTauSource("disabled"),
	reflectanceSource("fallback"),
	fallbackReason("disabled")
{
}

IRActiveIlluminatorOutput IRActiveIlluminator::evaluate(
	const IRActiveIlluminatorConfig& config,
	const IRActiveIlluminatorInput& input) const
{
	IRActiveIlluminatorOutput output;
	output.sensorBand = IRBandName(input.sensorBand);
	output.sourceBand = config.band == IRActiveIlluminatorBand::FollowSensor
		? output.sensorBand : bandName(config.band);
	output.protocolAngleMrad = input.protocolAngleMrad;
	output.halfAngleRad = std::max(0.0, input.protocolAngleMrad) * 1.0e-3 * 0.5;
	output.rangeM = std::max(0.0, input.rangeM);
	output.beamAngleRad = std::max(0.0, input.beamAngleRad);
	output.spotRadiusM = output.rangeM * std::tan(output.halfAngleRad);
	output.activeVisibility = clamp(input.activeVisibility, 0.0, 1.0);
	output.reflectanceSource = input.reflectanceSource.empty() ? "fallback" : input.reflectanceSource;

	const bool followSensor = config.band == IRActiveIlluminatorBand::FollowSensor;
	if (followSensor)
	{
		output.sourceLowUm = input.sensorLowUm;
		output.sourceHighUm = input.sensorHighUm;
	}
	else
	{
		const double halfBandwidth = std::max(0.0, config.bandwidthUm) * 0.5;
		output.sourceLowUm = config.centerWavelengthUm - halfBandwidth;
		output.sourceHighUm = config.centerWavelengthUm + halfBandwidth;
	}
	const bool bandCompatible = followSensor ||
		(config.band == IRActiveIlluminatorBand::NearInfrared && input.sensorBand == IRBand::NearInfrared) ||
		(config.band == IRActiveIlluminatorBand::MidWaveInfrared && input.sensorBand == IRBand::MidWaveInfrared);
	output.spectralOverlap = bandCompatible &&
		output.sourceHighUm > input.sensorLowUm && output.sourceLowUm < input.sensorHighUm;
	output.spectralOverlapWidthUm = output.spectralOverlap
		? std::max(0.0, std::min(output.sourceHighUm, input.sensorHighUm) -
			std::max(output.sourceLowUm, input.sensorLowUm))
		: 0.0;

	if (!config.enabled) { output.fallbackReason = "config_disabled"; return output; }
	if (!input.protocolEnabled) { output.fallbackReason = "protocol_illuminator_disabled"; return output; }
	if (input.sensorBand != IRBand::NearInfrared && input.sensorBand != IRBand::MidWaveInfrared)
	{
		output.fallbackReason = "sensor_band_not_supported_in_L2";
		return output;
	}
	if (!output.spectralOverlap) { output.fallbackReason = "spectral_band_mismatch"; return output; }
	if (!(output.halfAngleRad > 0.0)) { output.fallbackReason = "nonpositive_protocol_angle"; return output; }
	if (!(output.rangeM > 0.0)) { output.fallbackReason = "nonpositive_range"; return output; }
	if (output.beamAngleRad > output.halfAngleRad) { output.fallbackReason = "outside_beam_cone"; return output; }
	if (!(output.activeVisibility > 0.0)) { output.fallbackReason = "source_target_occluded"; return output; }

	if (config.beamProfile == IRActiveBeamProfile::TopHat)
	{
		output.beamFactor = 1.0;
	}
	else
	{
		const double normalizedAngle = output.beamAngleRad / output.halfAngleRad;
		// Gaussian is truncated at the protocol full-cone edge.  The edge is 1/16
		// of the center irradiance; center remains exactly one.
		output.beamFactor = std::exp(-4.0 * std::log(2.0) * normalizedAngle * normalizedAngle);
	}

	if (config.intensityMode == IRActiveIntensityMode::BandIrradianceAtReference)
	{
		output.referenceIrradianceWm2 = std::max(0.0, config.referenceIrradianceWm2);
		output.intensitySource = "physical_reference_irradiance";
	}
	else
	{
		output.referenceIrradianceWm2 = std::max(0.0, input.protocolSpotRadiance) *
			std::max(0.0, config.legacyMaxReferenceIrradianceWm2);
		output.intensitySource = "legacy_normalized_config_fallback";
	}
	if (!(output.referenceIrradianceWm2 > 0.0))
	{
		output.fallbackReason = "nonpositive_reference_irradiance";
		return output;
	}

	const double referenceRangeM = std::max(1.0e-6, config.referenceRangeM);
	const double inverseSquare = (referenceRangeM / output.rangeM) * (referenceRangeM / output.rangeM);
	output.geometricIrradianceWm2 = output.referenceIrradianceWm2 * inverseSquare * output.beamFactor;
	if (config.useAtmosphericAttenuation)
	{
		if (!input.tauInboundValid || !std::isfinite(input.tauInbound) ||
			input.tauInbound < 0.0 || input.tauInbound > 1.0)
		{
			output.fallbackReason = input.tauFallbackReason.empty()
				? "invalid_modtran_tau" : "invalid_modtran_tau:" + input.tauFallbackReason;
			output.outboundTauSource = "invalid_modtran_no_active_contribution";
			return output;
		}
		output.tauInbound = input.tauInbound;
		output.tauOutbound = input.tauInbound;
		output.outboundTauSource = "co_located_reciprocal_modtran";
	}
	else
	{
		output.tauInbound = 1.0;
		output.tauOutbound = 1.0;
		output.outboundTauSource = "atmospheric_attenuation_disabled";
	}

	output.targetIrradianceWm2 = output.geometricIrradianceWm2 * output.tauOutbound;
	output.incidentIrradianceWm2 = output.targetIrradianceWm2 *
		clamp(input.surfaceNdotL, 0.0, 1.0) * output.activeVisibility;
	output.activeSurfaceRadianceWm2Sr = clamp(input.bandReflectance, 0.0, 1.0) /
		3.14159265358979323846 * output.incidentIrradianceWm2;
	output.activeSensorRadianceWm2Sr = output.tauInbound * output.activeSurfaceRadianceWm2Sr;
	// Protocol/config intensity is band-integrated W/m^2, while the M1 chain
	// stores band-mean spectral radiance in W/(m^2 sr um).  RectangularBand
	// therefore divides by the actual source/sensor overlap width.
	if (output.spectralOverlapWidthUm > 0.0)
	{
		output.activeSurfaceRadianceWm2SrUm = output.activeSurfaceRadianceWm2Sr / output.spectralOverlapWidthUm;
		output.activeSensorRadianceWm2SrUm = output.activeSensorRadianceWm2Sr / output.spectralOverlapWidthUm;
	}
	output.activeContributionEnabled = output.activeSensorRadianceWm2SrUm > 0.0;
	output.fallbackReason = output.activeContributionEnabled ? "none" : "zero_physical_contribution";
	return output;
}

IRActiveIlluminatorBand IRActiveIlluminator::parseBand(const std::string& value)
{
	const std::string v = lower(value);
	if (v == "mwir" || v == "midwaveinfrared") return IRActiveIlluminatorBand::MidWaveInfrared;
	if (v == "followsensor" || v == "follow_sensor") return IRActiveIlluminatorBand::FollowSensor;
	return IRActiveIlluminatorBand::NearInfrared;
}

IRActiveBeamProfile IRActiveIlluminator::parseBeamProfile(const std::string& value)
{
	return lower(value) == "tophat" ? IRActiveBeamProfile::TopHat : IRActiveBeamProfile::Gaussian;
}

IRActiveIntensityMode IRActiveIlluminator::parseIntensityMode(const std::string& value)
{
	return lower(value) == "bandirradianceatreference"
		? IRActiveIntensityMode::BandIrradianceAtReference
		: IRActiveIntensityMode::LegacyNormalized;
}

const char* IRActiveIlluminator::bandName(IRActiveIlluminatorBand band)
{
	switch (band)
	{
	case IRActiveIlluminatorBand::NearInfrared: return "NIR";
	case IRActiveIlluminatorBand::MidWaveInfrared: return "MWIR";
	case IRActiveIlluminatorBand::FollowSensor: return "FollowSensor";
	default: return "NIR";
	}
}

const char* IRActiveIlluminator::beamProfileName(IRActiveBeamProfile profile)
{
	return profile == IRActiveBeamProfile::TopHat ? "TopHat" : "Gaussian";
}

const char* IRActiveIlluminator::intensityModeName(IRActiveIntensityMode mode)
{
	return mode == IRActiveIntensityMode::BandIrradianceAtReference
		? "BandIrradianceAtReference" : "LegacyNormalized";
}

double IRActiveIlluminator::clamp(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}
