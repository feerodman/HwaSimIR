#include "VideoEncoder.h"

#include <chrono>
#include <opencv2/opencv.hpp>

bool JpegFrameEncoder::configure(const VideoEncoderConfig& config, std::string& error)
{
	if (config.width <= 0 || config.height <= 0 || config.jpegQuality < 1 || config.jpegQuality > 100)
	{
		error = "invalid_jpeg_encoder_config";
		return false;
	}
	m_config = config;
	m_params.clear();
	m_params.push_back(cv::IMWRITE_JPEG_QUALITY);
	m_params.push_back(config.jpegQuality);
	error.clear();
	return true;
}

bool JpegFrameEncoder::encode(
	const RawVideoFrame& frame,
	EncodedVideoFrame& encoded,
	std::string& error)
{
	if (!frame.data || frame.width <= 0 || frame.height <= 0 || frame.stride <= 0)
	{
		error = "invalid_raw_video_frame";
		return false;
	}

	encoded.clearForReuse();
	const int sourceType = frame.pixelFormat == RawVideoPixelFormat::Gray8 ? CV_8UC1 : CV_8UC3;
	cv::Mat source(frame.height, frame.width, sourceType,
		const_cast<std::uint8_t*>(frame.data), static_cast<std::size_t>(frame.stride));
	cv::Mat flipped;
	const cv::Mat* prepared = &source;
	if (frame.flipVertical)
	{
		const auto begin = std::chrono::steady_clock::now();
		cv::flip(source, flipped, 0);
		encoded.preprocessMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - begin).count();
		prepared = &flipped;
	}

	cv::Mat gray;
	const cv::Mat* input = prepared;
	if (m_config.jpegGray && prepared->channels() == 3)
	{
		// Preserve the established R2 conversion exactly.
		cv::cvtColor(*prepared, gray, cv::COLOR_RGB2GRAY);
		input = &gray;
	}

	const auto encodeBegin = std::chrono::steady_clock::now();
	if (!cv::imencode(".jpg", *input, encoded.payload, m_params))
	{
		error = "opencv_jpeg_encode_failed";
		return false;
	}
	encoded.encodeMs = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - encodeBegin).count();
	encoded.payloadCodec = "jpeg";
	encoded.encoderName = name();
	encoded.keyFrame = false;
	encoded.ptsMs = frame.ptsMs;
	encoded.inputChannels = input->channels();
	error.clear();
	return true;
}

void JpegFrameEncoder::reset()
{
	// Stateless codec; retained for the common persistent encoder contract.
}

