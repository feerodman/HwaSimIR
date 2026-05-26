#include "IRTypes.h"

IRSensorProfile::IRSensorProfile()
	: band(IRBand::MidWaveInfrared),
	name("MWIR"),
	loadedFromFile(false),
	spectralLowUm(3.0),
	spectralHighUm(5.0),
	width(640),
	height(480),
	fovHDeg(45.0),
	fovVDeg(33.75),
	focalLengthMm(100.0),
	detectorPitchMm(0.02),
	adcBits(14),
	displayBits(8),
	netdK(0.05)
{
}

IRBand IRBandFromProtocol(int protocolBand)
{
	switch (protocolBand)
	{
	case 1: return IRBand::NearInfrared;
	case 0: return IRBand::ShortWaveInfrared;
	case 2: return IRBand::MidWaveInfrared;
	case 3: return IRBand::LongWaveInfrared;
	case 4: return IRBand::Visible;
	default: return IRBand::MidWaveInfrared;
	}
}

IRBandRange IRDefaultRangeForBand(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return{ band, 0.40, 0.70 };
	case IRBand::NearInfrared: return{ band, 0.70, 1.10 };
	case IRBand::ShortWaveInfrared: return{ band, 1.10, 2.50 };
	case IRBand::MidWaveInfrared: return{ band, 3.00, 5.00 };
	case IRBand::LongWaveInfrared: return{ band, 8.00, 14.00 };
	default: return{ IRBand::MidWaveInfrared, 3.00, 5.00 };
	}
}

const char* IRBandName(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return "VIS";
	case IRBand::NearInfrared: return "NIR";
	case IRBand::ShortWaveInfrared: return "SWIR";
	case IRBand::MidWaveInfrared: return "MWIR";
	case IRBand::LongWaveInfrared: return "LWIR";
	default: return "MWIR";
	}
}

const char* IRSensorProfileFileName(IRBand band)
{
	switch (band)
	{
	case IRBand::Visible: return "default_LLLTV.json";
	case IRBand::NearInfrared: return "default_NVG.json";
	case IRBand::ShortWaveInfrared: return "default_SWIR.json";
	case IRBand::MidWaveInfrared: return "default_MWIR.json";
	case IRBand::LongWaveInfrared: return "default_LWIR.json";
	default: return "default_MWIR.json";
	}
}

IRSensorProfile IRDefaultSensorProfile(IRBand band)
{
	IRSensorProfile profile;
	IRBandRange range = IRDefaultRangeForBand(band);
	profile.band = band;
	profile.name = IRBandName(band);
	profile.spectralLowUm = range.lowUm;
	profile.spectralHighUm = range.highUm;
	profile.sourcePath = "built-in fallback";

	switch (band)
	{
	case IRBand::Visible:
		profile.adcBits = 12;
		profile.netdK = 100.0;
		break;
	case IRBand::NearInfrared:
		profile.adcBits = 14;
		profile.netdK = 100.0;
		break;
	case IRBand::ShortWaveInfrared:
		profile.adcBits = 14;
		profile.netdK = 73.0;
		break;
	case IRBand::MidWaveInfrared:
		profile.adcBits = 14;
		profile.netdK = 0.05;
		break;
	case IRBand::LongWaveInfrared:
		profile.adcBits = 16;
		profile.netdK = 0.02;
		break;
	default:
		break;
	}
	return profile;
}
