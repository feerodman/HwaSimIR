#pragma once

#include <string>

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

struct IRSensorProfile
{
	IRBand band;
	std::string name;
	std::string sourcePath;
	bool loadedFromFile;
	double spectralLowUm;
	double spectralHighUm;
	int width;
	int height;
	double fovHDeg;
	double fovVDeg;
	double focalLengthMm;
	double detectorPitchMm;
	int adcBits;
	int displayBits;
	double netdK;

	IRSensorProfile();
};

IRBand IRBandFromProtocol(int protocolBand);
IRBandRange IRDefaultRangeForBand(IRBand band);
const char* IRBandName(IRBand band);
const char* IRSensorProfileFileName(IRBand band);
IRSensorProfile IRDefaultSensorProfile(IRBand band);
