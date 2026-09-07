#pragma once

#include "IRTypes.h"

#include <string>

enum class IRActiveIlluminatorBand
{
	NearInfrared,
	MidWaveInfrared,
	FollowSensor
};

enum class IRActiveBeamProfile
{
	Gaussian,
	TopHat
};

enum class IRActiveIntensityMode
{
	LegacyNormalized,
	BandIrradianceAtReference
};

struct IRActiveIlluminatorConfig
{
	bool enabled;
	IRActiveIlluminatorBand band;
	double centerWavelengthUm;
	double bandwidthUm;
	IRActiveBeamProfile beamProfile;
	IRActiveIntensityMode intensityMode;
	double referenceRangeM;
	double referenceIrradianceWm2;
	double legacyMaxReferenceIrradianceWm2;
	bool useAtmosphericAttenuation;
	bool useGeometricOcclusion;
	bool debugLog;
	bool debugVisualCone;

	IRActiveIlluminatorConfig();
};

struct IRActiveIlluminatorInput
{
	bool protocolEnabled;
	IRBand sensorBand;
	double sensorLowUm;
	double sensorHighUm;
	double protocolAngleMrad;
	double protocolSpotRadiance;
	double rangeM;
	double beamAngleRad;
	double surfaceNdotL;
	double tauInbound;
	bool tauInboundValid;
	std::string tauFallbackReason;
	double activeVisibility;
	double bandReflectance;
	std::string reflectanceSource;

	IRActiveIlluminatorInput();
};

struct IRActiveIlluminatorOutput
{
	bool activeContributionEnabled;
	bool spectralOverlap;
	std::string sourceBand;
	std::string sensorBand;
	double sourceLowUm;
	double sourceHighUm;
	double protocolAngleMrad;
	double halfAngleRad;
	double rangeM;
	double beamAngleRad;
	double beamFactor;
	double spotRadiusM;
	double referenceIrradianceWm2;
	double geometricIrradianceWm2;
	double tauOutbound;
	double tauInbound;
	double targetIrradianceWm2;
	double incidentIrradianceWm2;
	double activeSurfaceRadianceWm2Sr;
	double activeSensorRadianceWm2Sr;
	double spectralOverlapWidthUm;
	double activeSurfaceRadianceWm2SrUm;
	double activeSensorRadianceWm2SrUm;
	double activeVisibility;
	std::string intensitySource;
	std::string outboundTauSource;
	std::string reflectanceSource;
	std::string fallbackReason;

	IRActiveIlluminatorOutput();
};

class IRActiveIlluminator
{
public:
	IRActiveIlluminatorOutput evaluate(
		const IRActiveIlluminatorConfig& config,
		const IRActiveIlluminatorInput& input) const;

	static IRActiveIlluminatorBand parseBand(const std::string& value);
	static IRActiveBeamProfile parseBeamProfile(const std::string& value);
	static IRActiveIntensityMode parseIntensityMode(const std::string& value);
	static const char* bandName(IRActiveIlluminatorBand band);
	static const char* beamProfileName(IRActiveBeamProfile profile);
	static const char* intensityModeName(IRActiveIntensityMode mode);

private:
	static double clamp(double value, double low, double high);
};
