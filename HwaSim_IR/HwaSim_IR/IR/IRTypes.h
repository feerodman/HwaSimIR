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
