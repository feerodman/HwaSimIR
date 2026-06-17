#pragma once

#include "IRTypes.h"

#include <string>

enum class IRStage5ToneMap
{
	Linear,
	Log,
	Asinh
};

struct IRRadianceModelV2DebugConfig
{
	IRStage5ToneMap toneMap;
	double bodyRadianceScale;
	double hotspotRadianceScale;
	double brightspotRadianceScale;
	double minBodyGray;
	double solarReflectanceWeight;
	double bodyDisplayGain;
	double reflectedDisplayGain;
	double hotspotDisplayGain;
	double brightspotDisplayGain;
	double compositeMinGray;
	double compositeMaxGray;

	IRRadianceModelV2DebugConfig();
};

struct IRRadianceModelV2Input
{
	IRBand band;
	double materialTemperatureK;
	double materialEmissivity;
	double materialReflectance;
	double tauUp;
	double solarStrength;
	double ndotl;
	double textureLuma;
	double solarReflectanceWeight;
	double hotspotTemperatureK;
	double hotspotIntensity;
	double brightspotIntensity;
	double plumeRadiance;
	double pathRadiance;
	double legacyPathRadiance;
	double modtranPathRadiance;
	double modtranPathRaw;
	double modtranPathScaled;
	std::string modtranPathUnitMode;
	double modtranPathScale;
	double modtranPathOffset;
	double modtranPathBlend;
	std::string modtranPathRuntimeMode;
	double effectivePathRadiance;
	double modtranSkyRadiance;
	double modtranSolarIrradiance;
	bool modtranRadianceValid;
	std::string materialName;
	std::string pathRadianceSource;
	std::string modtranFallbackReason;
	std::string modtranInterpolationMode;
	std::string sourceFlags;
	bool enableDebugFloor;
	IRRadianceModelV2DebugConfig debugConfig;

	IRRadianceModelV2Input();
};

struct IRRadianceComponents
{
	IRBand band;
	std::string materialName;
	double materialTempK;
	double emissivity;
	double reflectance;
	double bodyRadiance;
	double reflectedRadiance;
	double rearHotspotRadiance;
	double plumeRadiance;
	double brightspotRadiance;
	double tauUp;
	double pathRadiance;
	double legacyPathRadiance;
	double modtranPathRadiance;
	double modtranPathRaw;
	double modtranPathScaled;
	std::string modtranPathUnitMode;
	double modtranPathScale;
	double modtranPathOffset;
	double modtranPathBlend;
	std::string modtranPathRuntimeMode;
	double effectivePathRadiance;
	double modtranSkyRadiance;
	double modtranSolarIrradiance;
	bool modtranRadianceValid;
	std::string pathRadianceSource;
	std::string modtranFallbackReason;
	std::string modtranInterpolationMode;
	double sensorInputLegacy;
	double sensorInputModtran;
	double sensorInputRadiance;
	double displayPreview;
	std::string sourceFlags;

	IRRadianceComponents();
};

using IRSceneRadianceOutput = IRRadianceComponents;

struct IRRadianceModelV2Output
{
	double wavelengthCenterUm;
	double bodyRadiance;
	double reflectedRadiance;
	double hotspotRadiance;
	double brightspotRadiance;
	double finalRadianceDebug;
	double bodyGrayBeforeFloor;
	double bodyGrayAfterFloor;
	double reflectedGray;
	double hotspotGray;
	double brightspotGray;
	double finalGrayDebug;
	bool debugFloorApplied;

	IRRadianceModelV2Output();
};

class IRRadianceModelV2
{
public:
	IRRadianceComponents evaluateComponents(const IRRadianceModelV2Input& input) const;
	IRRadianceModelV2Output evaluate(const IRRadianceModelV2Input& input) const;

	static double bandCenterUm(IRBand band);
	static double planckRadianceWm2SrUm(double wavelengthUm, double temperatureK);

private:
	static double applyToneMap(double radiance, double scale, IRStage5ToneMap toneMap);
	static double referenceTemperatureK(IRBand band);
	static double hotspotBandWeight(IRBand band);
	static double brightspotBandWeight(IRBand band);
	static double clamp(double value, double low, double high);
};
