#pragma once

#include <string>
#include <map>

enum class IRBand
{
	Visible = 0,
	NearInfrared = 1,
	ShortWaveInfrared = 2,
	MidWaveInfrared = 3,
	LongWaveInfrared = 4
};

struct IRBandRange
{
	IRBand band;
	double lowUm;
	double highUm;
};

struct IRDisplayPreset
{
    double gamma = 1.0;
    double gain = 1.0;
    double offsetGray = 0.0;
    bool whiteHot = true;
    bool automatic = false;
    int statisticsSize = 64;
    double statisticsHz = 5.0;
    double lowPercentile = 2.0;
    double highPercentile = 98.0;
    double smoothingAlpha = 0.15;
    std::string toneMap = "LinearClamp"; // Public display only; no temperature conversion.
    double minimumInputSpan = 0.02; // Common scaled linear input after fixed gain/offset.
    double targetLow = 0.05, targetHigh = 0.95;
    double minGain = 0.25, maxGain = 8.0, minOffset = -1.0, maxOffset = 1.0;
};

struct IRSensorProfile
{
	IRBand band;
	std::string name;
	std::string sourcePath;
	bool loadedFromFile;
    std::string loadError;
    std::string contentHash;
    std::string revision;
    int schemaVersion = 0;
    std::string defaultDisplayPreset = "Legacy";
    std::map<std::string, IRDisplayPreset> displayPresets;
	double spectralLowUm;
	double spectralHighUm;
	int width;
	int height;
	double fovHDeg;
	double fovVDeg;
	double focalLengthMm;
	double detectorPitchMm;
	double lensFNumber;
	int adcBits;
	int displayBits;
	double netdK;
	bool blackHot;
	std::string usedFields;
	std::string fallbackFields;
	std::string ignoredPresagisFields;

	IRSensorProfile();
};

IRBand IRBandFromProtocol(int protocolBand);
IRBandRange IRDefaultRangeForBand(IRBand band);
const char* IRBandName(IRBand band);
const char* IRSensorProfileFileName(IRBand band);
IRSensorProfile IRDefaultSensorProfile(IRBand band);
