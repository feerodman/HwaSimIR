#include "IRWorldCloudStreaming.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
double ClampCloudValue(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}
}

void IRWorldCloudStreaming::setConfig(const IRWorldCloudStreamingConfig& requested)
{
	m_config = requested;
	m_config.cellSizeM = std::max(100.0, requested.cellSizeM);
	m_config.streamingRadiusM = std::max(m_config.cellSizeM, requested.streamingRadiusM);
	m_config.deactivationRadiusM = std::max(
		m_config.streamingRadiusM + 1.0,
		requested.deactivationRadiusM);
	m_config.maxActiveVolumes = std::max(1, std::min(32, requested.maxActiveVolumes));
	m_config.maxVisibleVolumes = std::max(1, std::min(
		m_config.maxActiveVolumes,
		requested.maxVisibleVolumes));
	m_config.minCloudAltitudeM = std::min(requested.minCloudAltitudeM, requested.maxCloudAltitudeM);
	m_config.maxCloudAltitudeM = std::max(requested.minCloudAltitudeM, requested.maxCloudAltitudeM);
	m_config.minRadiusXYM = std::max(10.0, std::min(requested.minRadiusXYM, requested.maxRadiusXYM));
	m_config.maxRadiusXYM = std::max(m_config.minRadiusXYM, requested.maxRadiusXYM);
	m_config.minRadiusZM = std::max(10.0, std::min(requested.minRadiusZM, requested.maxRadiusZM));
	m_config.maxRadiusZM = std::max(m_config.minRadiusZM, requested.maxRadiusZM);
	m_config.fadeDistanceM = std::max(1.0, requested.fadeDistanceM);
}

std::uint64_t IRWorldCloudStreaming::mix64(std::uint64_t value)
{
	value += 0x9E3779B97F4A7C15ULL;
	value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
	value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
	return value ^ (value >> 31);
}

std::uint64_t IRWorldCloudStreaming::profileHash(const std::string& text)
{
	std::uint64_t hash = 1469598103934665603ULL;
	for (size_t i = 0; i < text.size(); ++i)
	{
		hash ^= static_cast<unsigned char>(text[i]);
		hash *= 1099511628211ULL;
	}
	return hash;
}

double IRWorldCloudStreaming::unitRandom(std::uint64_t& state)
{
	state = mix64(state);
	return static_cast<double>(state >> 11) * (1.0 / 9007199254740992.0);
}

double IRWorldCloudStreaming::rangeRandom(std::uint64_t& state, double low, double high)
{
	return low + (high - low) * unitRandom(state);
}

IRWorldCloudDescriptor IRWorldCloudStreaming::descriptorForCell(
	int cellX,
	int cellY,
	double groundReferenceZ,
	const std::string& weatherProfile,
	double cloudProbability,
	double densityScale,
	int densityTemplateCount) const
{
	IRWorldCloudDescriptor descriptor;
	descriptor.cellX = cellX;
	descriptor.cellY = cellY;

	const std::uint64_t xBits = static_cast<std::uint64_t>(static_cast<std::int64_t>(cellX));
	const std::uint64_t yBits = static_cast<std::uint64_t>(static_cast<std::int64_t>(cellY));
	descriptor.seed = mix64(
		m_config.weatherSeed ^
		mix64(xBits + 0x632BE59BD9B4E019ULL) ^
		mix64(yBits + 0x8CB92BA72F3D8DD7ULL) ^
		profileHash(weatherProfile));
	descriptor.cloudId = mix64(descriptor.seed ^ 0xD1B54A32D192ED03ULL);

	std::uint64_t randomState = descriptor.seed;
	descriptor.hasCloud = unitRandom(randomState) < ClampCloudValue(cloudProbability, 0.0, 1.0);
	const double cellOriginX = static_cast<double>(cellX) * m_config.cellSizeM;
	const double cellOriginY = static_cast<double>(cellY) * m_config.cellSizeM;
	const double inset = m_config.cellSizeM * 0.14;
	descriptor.worldX = cellOriginX + rangeRandom(randomState, inset, m_config.cellSizeM - inset);
	descriptor.worldY = cellOriginY + rangeRandom(randomState, inset, m_config.cellSizeM - inset);
	descriptor.worldZ = groundReferenceZ + rangeRandom(
		randomState,
		m_config.minCloudAltitudeM,
		m_config.maxCloudAltitudeM);
	descriptor.radiusX = rangeRandom(randomState, m_config.minRadiusXYM, m_config.maxRadiusXYM);
	descriptor.radiusY = descriptor.radiusX * rangeRandom(randomState, 0.72, 1.18);
	descriptor.radiusZ = rangeRandom(randomState, m_config.minRadiusZM, m_config.maxRadiusZM);
	descriptor.density = ClampCloudValue(
		rangeRandom(randomState, 0.62, 1.0) * densityScale,
		0.0,
		1.5);
	descriptor.densityTemplate = densityTemplateCount > 0
		? static_cast<int>(unitRandom(randomState) * densityTemplateCount) % densityTemplateCount
		: 0;
	descriptor.temperatureOffsetK = rangeRandom(randomState, -5.0, 3.0);
	descriptor.rotationDeg = rangeRandom(randomState, 0.0, 360.0);
	descriptor.noiseOffsetX = unitRandom(randomState);
	descriptor.noiseOffsetY = unitRandom(randomState);
	descriptor.noiseOffsetZ = unitRandom(randomState);
	return descriptor;
}

std::vector<IRWorldCloudDescriptor> IRWorldCloudStreaming::queryCandidates(
	double streamingCenterX,
	double streamingCenterY,
	double groundReferenceZ,
	const std::string& weatherProfile,
	double cloudProbability,
	double densityScale,
	int densityTemplateCount) const
{
	std::vector<IRWorldCloudDescriptor> candidates;
	const int centerCellX = static_cast<int>(std::floor(streamingCenterX / m_config.cellSizeM));
	const int centerCellY = static_cast<int>(std::floor(streamingCenterY / m_config.cellSizeM));
	const int radiusCells = static_cast<int>(std::ceil(m_config.streamingRadiusM / m_config.cellSizeM)) + 1;
	for (int cellY = centerCellY - radiusCells; cellY <= centerCellY + radiusCells; ++cellY)
	{
		for (int cellX = centerCellX - radiusCells; cellX <= centerCellX + radiusCells; ++cellX)
		{
			IRWorldCloudDescriptor descriptor = descriptorForCell(
				cellX,
				cellY,
				groundReferenceZ,
				weatherProfile,
				cloudProbability,
				densityScale,
				densityTemplateCount);
			if (!descriptor.hasCloud)
			{
				continue;
			}
			const double dx = descriptor.worldX - streamingCenterX;
			const double dy = descriptor.worldY - streamingCenterY;
			descriptor.centerDistanceM = std::sqrt(dx * dx + dy * dy);
			if (descriptor.centerDistanceM <= m_config.streamingRadiusM)
			{
				candidates.push_back(descriptor);
			}
		}
	}
	std::sort(candidates.begin(), candidates.end(), [](const IRWorldCloudDescriptor& left, const IRWorldCloudDescriptor& right) {
		if (left.centerDistanceM != right.centerDistanceM)
		{
			return left.centerDistanceM < right.centerDistanceM;
		}
		return left.cloudId < right.cloudId;
	});
	return candidates;
}

std::string IRWorldCloudStreaming::cloudIdText(std::uint64_t cloudId)
{
	std::ostringstream text;
	text << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << cloudId;
	return text.str();
}
