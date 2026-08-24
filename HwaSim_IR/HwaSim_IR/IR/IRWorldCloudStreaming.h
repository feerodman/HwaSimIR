#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Deterministic world-cloud descriptors are independent from Panda3D nodes.
// Pool reuse never changes the descriptor generated for a world cell.
struct IRWorldCloudStreamingConfig
{
	double cellSizeM = 2500.0;
	double streamingRadiusM = 6000.0;
	double deactivationRadiusM = 7500.0;
	int maxActiveVolumes = 8;
	int maxVisibleVolumes = 4;
	double minCloudAltitudeM = 2200.0;
	double maxCloudAltitudeM = 4200.0;
	double minRadiusXYM = 350.0;
	double maxRadiusXYM = 1000.0;
	double minRadiusZM = 180.0;
	double maxRadiusZM = 500.0;
	std::uint64_t weatherSeed = 12345;
	double fadeDistanceM = 800.0;
};

struct IRWorldCloudDescriptor
{
	int cellX = 0;
	int cellY = 0;
	std::uint64_t seed = 0;
	std::uint64_t cloudId = 0;
	bool hasCloud = false;
	double worldX = 0.0;
	double worldY = 0.0;
	double worldZ = 0.0;
	double radiusX = 0.0;
	double radiusY = 0.0;
	double radiusZ = 0.0;
	double density = 0.0;
	int densityTemplate = 0;
	double temperatureOffsetK = 0.0;
	double rotationDeg = 0.0;
	double noiseOffsetX = 0.0;
	double noiseOffsetY = 0.0;
	double noiseOffsetZ = 0.0;
	double centerDistanceM = 0.0;
};

class IRWorldCloudStreaming
{
public:
	void setConfig(const IRWorldCloudStreamingConfig& config);
	const IRWorldCloudStreamingConfig& config() const { return m_config; }

	IRWorldCloudDescriptor descriptorForCell(
		int cellX,
		int cellY,
		double groundReferenceZ,
		const std::string& weatherProfile,
		double cloudProbability,
		double densityScale,
		int densityTemplateCount) const;

	std::vector<IRWorldCloudDescriptor> queryCandidates(
		double streamingCenterX,
		double streamingCenterY,
		double groundReferenceZ,
		const std::string& weatherProfile,
		double cloudProbability,
		double densityScale,
		int densityTemplateCount) const;

	static std::string cloudIdText(std::uint64_t cloudId);

private:
	static std::uint64_t mix64(std::uint64_t value);
	static std::uint64_t profileHash(const std::string& text);
	static double unitRandom(std::uint64_t& state);
	static double rangeRandom(std::uint64_t& state, double low, double high);

	IRWorldCloudStreamingConfig m_config;
};
