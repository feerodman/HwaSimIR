#pragma once

#include "IRTypes.h"

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
	bool enableDebugFloor;
	IRRadianceModelV2DebugConfig debugConfig;

	IRRadianceModelV2Input();
};

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
