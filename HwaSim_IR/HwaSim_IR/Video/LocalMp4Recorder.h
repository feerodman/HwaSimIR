#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct LocalMp4RecorderConfig
{
	bool enabled = false;
	std::string outputDirectory = "/home/linaro/HwaSimIR_Record";
	std::string container = "mp4";
	std::string filePrefix = "HwaSimIR";
	std::string encoder = "auto";
	int bitrateKbps = 4000;
	int gopFrames = 30;
	std::size_t queueMaxFrames = 120;
	bool blockWhenQueueFull = true;
	int flushTimeoutMs = 10000;
	bool enablePerfLog = true;
};

struct LocalMp4RecorderStats
{
	std::uint64_t inputFrames = 0;
	std::uint64_t writtenFrames = 0;
	std::uint64_t droppedFrames = 0;
	std::size_t queueDepth = 0;
	std::size_t maxQueueDepth = 0;
	double writeMsAverage = 0.0;
	double writeMsMaximum = 0.0;
	std::string outputPath;
	std::string backend;
};

// No-drop sender-side recorder. With AVFormat it muxes the same Annex-B AU
// shared by TCP/DDS. The non-AVFormat build has an explicit OpenCV fallback.
class LocalMp4Recorder
{
public:
	LocalMp4Recorder();
	~LocalMp4Recorder();

	void configure(const LocalMp4RecorderConfig& config, const std::string& channel);
	void setProtocolEnabled(bool saveMp4En);
	bool effectiveEnabled() const;
	bool wantsH264() const;

	bool startPending(int round, int width, int height, int fps, std::string& error);
	bool enqueueH264(const std::uint8_t* data, std::size_t size, bool keyFrame, int width, int height,
		double* backpressureMs, std::string& error);
	bool enqueueRawBgr24(const std::uint8_t* data, int width, int height, bool flipVertical,
		double* backpressureMs, std::string& error);
	bool stopAndFlush(const char* reason, std::string& error);
	void shutdown();

	LocalMp4RecorderStats stats() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
