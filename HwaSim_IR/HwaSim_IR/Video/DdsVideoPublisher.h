#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

struct DdsVideoPublisherConfig
{
	bool enabled = false;
	int domainId = 150;
	std::string qosFile = "Config/DDS/ZRDDS_QOS_PROFILES.xml";
	std::string topic;
	std::string codec = "auto";
	std::string rawPixelFormat = "gray8";
	std::string topicH264Precise = "HwaSimIR.Video.precise.H264";
	std::string topicRawGray8Precise = "HwaSimIR.Video.precise.RawGray8";
	std::string topicRawBgr24Precise = "HwaSimIR.Video.precise.RawBGR24";
	std::string topicH264Coarse = "HwaSimIR.Video.coarse.H264";
	std::string topicRawGray8Coarse = "HwaSimIR.Video.coarse.RawGray8";
	std::string topicRawBgr24Coarse = "HwaSimIR.Video.coarse.RawBGR24";
	std::size_t queueMaxFrames = 120;
	bool blockWhenQueueFull = true;
	int ackTimeoutSec = 60;
	int shutdownDrainMs = 5000;
	bool enablePerfLog = true;
	std::string auditPath;
	std::uint64_t auditMaxSamples = 0;
};

struct DdsVideoPublisherStats
{
	std::uint64_t acceptedSamples = 0;
	std::uint64_t sentSamples = 0;
	std::uint64_t sentBytes = 0;
	std::uint64_t writeErrors = 0;
	std::uint64_t droppedSamples = 0;
	std::size_t queueDepth = 0;
	std::size_t maxQueueDepth = 0;
	double writeMsAverage = 0.0;
	double writeMsMaximum = 0.0;
	double backpressureMs = 0.0;
};

// Owns the process-lifetime ZRDDS publisher. It never reads a Panda texture,
// encodes video, or adds metadata. Every queued vector is written verbatim as
// one DDS::Bytes Sample.
class DdsVideoPublisher
{
public:
	DdsVideoPublisher();
	~DdsVideoPublisher();

	bool start(const DdsVideoPublisherConfig& config, std::string& error);
	bool configureTopic(const std::string& topic, bool* changed, std::string& error);
	bool publishBytes(const std::uint8_t* data, std::size_t size, double* backpressureMs, std::string& error);
	bool flush(int timeoutMs, std::string& error);
	bool endRound(std::string& error);
	void shutdown();

	bool enabled() const;
	bool healthy() const;
	std::string topic() const;
	DdsVideoPublisherStats stats() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
