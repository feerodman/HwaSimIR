#include "IRSensorPostProcess.h"

#include <algorithm>
#include <cmath>

namespace
{
double ClampGray(double value)
{
	return std::max(0.0, std::min(255.0, value));
}

unsigned char GrayToByte(double value)
{
	return static_cast<unsigned char>(std::floor(ClampGray(value) + 0.5));
}
}

IRSensorPostProcess::IRSensorPostProcess()
	: m_noiseState(0x6d2b79f5u)
{
}

void IRSensorPostProcess::processRgb8(
	const unsigned char* input,
	int width,
	int height,
	const IRSensorPostProcessConfig& config,
	std::vector<unsigned char>& output)
{
	if (input == nullptr || width <= 0 || height <= 0) {
		output.clear();
		return;
	}

	const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
	output.resize(pixelCount * 3u);

	for (std::size_t i = 0; i < pixelCount; ++i) {
		const std::size_t offset = i * 3u;
		const double r = static_cast<double>(input[offset + 0u]);
		const double g = static_cast<double>(input[offset + 1u]);
		const double b = static_cast<double>(input[offset + 2u]);
		double gray = 0.299 * r + 0.587 * g + 0.114 * b;

		gray = gray * config.displayGain + config.displayOffset;
		if (config.noiseEnable && config.noiseSigmaGray > 0.0) {
			gray += nextUniformNoise(config.noiseSigmaGray);
		}
		gray = ClampGray(gray);
		if (!config.whiteHot) {
			gray = 255.0 - gray;
		}

		const unsigned char value = GrayToByte(gray);
		output[offset + 0u] = value;
		output[offset + 1u] = value;
		output[offset + 2u] = value;
	}
}

std::uint32_t IRSensorPostProcess::nextRandom()
{
	m_noiseState ^= (m_noiseState << 13);
	m_noiseState ^= (m_noiseState >> 17);
	m_noiseState ^= (m_noiseState << 5);
	return m_noiseState;
}

double IRSensorPostProcess::nextUniformNoise(double sigma)
{
	const double unit = static_cast<double>(nextRandom() & 0x00ffffffu) / 16777215.0;
	return (unit * 2.0 - 1.0) * sigma;
}
