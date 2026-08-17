#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

enum class RawVideoPixelFormat
{
	Bgr24,
	Rgb24,
	Gray8
};

struct RawVideoFrame
{
	const std::uint8_t* data = nullptr;
	int width = 0;
	int height = 0;
	int stride = 0;
	RawVideoPixelFormat pixelFormat = RawVideoPixelFormat::Bgr24;
	bool flipVertical = false;
	std::int64_t ptsMs = 0;
};

struct VideoEncoderConfig
{
	int width = 0;
	int height = 0;
	int fps = 60;
	int bitrateKbps = 4000;
	int gopFrames = 30;
	int jpegQuality = 100;
	bool jpegGray = false;
	bool lowLatency = true;
	bool forceKeyFrameOnStart = true;
};

struct EncodedVideoFrame
{
	std::vector<std::uint8_t> payload;
	std::string payloadCodec;
	std::string encoderName;
	std::string fallbackReason;
	bool keyFrame = false;
	std::int64_t ptsMs = 0;
	double preprocessMs = 0.0;
	double encodeMs = 0.0;
	int inputChannels = 0;

	void clearForReuse()
	{
		payload.clear();
		payloadCodec.clear();
		encoderName.clear();
		fallbackReason.clear();
		keyFrame = false;
		ptsMs = 0;
		preprocessMs = 0.0;
		encodeMs = 0.0;
		inputChannels = 0;
	}
};

class IVideoEncoder
{
public:
	virtual ~IVideoEncoder() = default;
	virtual bool configure(const VideoEncoderConfig& config, std::string& error) = 0;
	virtual bool encode(const RawVideoFrame& frame, EncodedVideoFrame& encoded, std::string& error) = 0;
	virtual void requestKeyFrame() = 0;
	virtual void reset() = 0;
	virtual bool isAvailable() const = 0;
	virtual const char* name() const = 0;
};

class JpegFrameEncoder final : public IVideoEncoder
{
public:
	bool configure(const VideoEncoderConfig& config, std::string& error) override;
	bool encode(const RawVideoFrame& frame, EncodedVideoFrame& encoded, std::string& error) override;
	void requestKeyFrame() override {}
	void reset() override;
	bool isAvailable() const override { return true; }
	const char* name() const override { return "opencv_imencode_jpeg"; }

private:
	VideoEncoderConfig m_config;
	std::vector<int> m_params;
};

class H264FfmpegEncoder final : public IVideoEncoder
{
public:
	H264FfmpegEncoder();
	~H264FfmpegEncoder() override;
	bool configure(const VideoEncoderConfig& config, std::string& error) override;
	bool encode(const RawVideoFrame& frame, EncodedVideoFrame& encoded, std::string& error) override;
	void requestKeyFrame() override;
	void reset() override;
	bool isAvailable() const override;
	const char* name() const override;

private:
	struct Impl;
	Impl* m_impl;
	std::atomic<bool> m_forceKeyFrame{ true };
};

#if defined(HWASIMIR_HAS_RKMPP)
class H264MppEncoder final : public IVideoEncoder
{
public:
	H264MppEncoder();
	~H264MppEncoder() override;
	bool configure(const VideoEncoderConfig& config, std::string& error) override;
	bool encode(const RawVideoFrame& frame, EncodedVideoFrame& encoded, std::string& error) override;
	void requestKeyFrame() override;
	void reset() override;
	bool isAvailable() const override;
	const char* name() const override;

private:
	struct Impl;
	Impl* m_impl;
	std::atomic<bool> m_forceKeyFrame{ true };
	std::atomic<bool> m_successLogged{ false };
};
#endif
