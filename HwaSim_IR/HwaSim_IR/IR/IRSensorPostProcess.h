#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct IRSensorPostProcessConfig
{
	bool whiteHot = true;
	double displayGain = 1.0;
	double displayOffset = 0.0;
	bool noiseEnable = false;
	double noiseSigmaGray = 0.0;
	bool noiseOverrideEnable = true;
	bool applyToWindow = true;
	bool applyToCapture = true;
	bool backgroundDisplayEnable = true;
	std::string source = "default";
	std::string noiseSource = "default";
};

class IRSensorPostProcess
{
public:
	IRSensorPostProcess();

	void processRgb8(
		const unsigned char* input,
		int width,
		int height,
		const IRSensorPostProcessConfig& config,
		std::vector<unsigned char>& output);

private:
	std::uint32_t nextRandom();
	double nextUniformNoise(double sigma);

	std::uint32_t m_noiseState;
};
