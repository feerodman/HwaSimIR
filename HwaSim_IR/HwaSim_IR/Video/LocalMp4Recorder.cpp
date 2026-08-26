#include "LocalMp4Recorder.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <opencv2/opencv.hpp>

#if defined(_WIN32)
#include <direct.h>
#include <errno.h>
#include <sys/stat.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(HWASIMIR_HAS_AVFORMAT)
extern "C"
{
#include <libavcodec/codec_id.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mem.h>
}
#endif

namespace
{
std::int64_t SteadyNowNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool DirectoryExists(const std::string& path)
{
#if defined(_WIN32)
	struct _stat info;
	return _stat(path.c_str(), &info) == 0 && (info.st_mode & _S_IFDIR) != 0;
#else
	struct stat info;
	return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
}

bool CreateOneDirectory(const std::string& path)
{
	if (path.empty() || DirectoryExists(path)) return true;
#if defined(_WIN32)
	return _mkdir(path.c_str()) == 0 || errno == EEXIST;
#else
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

bool CreateDirectories(const std::string& input)
{
	if (input.empty()) return false;
	std::string path = input;
	std::replace(path.begin(), path.end(), '\\', '/');
	std::string current;
	std::size_t index = 0;
#if defined(_WIN32)
	if (path.size() >= 2 && path[1] == ':')
	{
		current = path.substr(0, 2);
		index = 2;
	}
#else
	if (!path.empty() && path[0] == '/')
	{
		current = "/";
		index = 1;
	}
#endif
	while (index <= path.size())
	{
		const std::size_t slash = path.find('/', index);
		const std::string part = path.substr(index,
			slash == std::string::npos ? std::string::npos : slash - index);
		if (!part.empty())
		{
			if (!current.empty() && current.back() != '/') current += '/';
			current += part;
			if (!CreateOneDirectory(current)) return false;
		}
		if (slash == std::string::npos) break;
		index = slash + 1;
	}
	return DirectoryExists(path);
}

std::string Timestamp()
{
	std::time_t now = std::time(nullptr);
	std::tm value;
#if defined(_WIN32)
	localtime_s(&value, &now);
#else
	localtime_r(&now, &value);
#endif
	char text[32] = {};
	std::strftime(text, sizeof(text), "%Y%m%d_%H%M%S", &value);
	return text;
}

bool ContainsIdr(const std::vector<std::uint8_t>& payload)
{
	for (std::size_t i = 0; i + 4 < payload.size(); ++i)
	{
		std::size_t offset = 0;
		if (payload[i] == 0 && payload[i + 1] == 0 && payload[i + 2] == 1) offset = i + 3;
		else if (payload[i] == 0 && payload[i + 1] == 0 && payload[i + 2] == 0 && payload[i + 3] == 1) offset = i + 4;
		if (offset && offset < payload.size() && (payload[offset] & 0x1f) == 5) return true;
	}
	return false;
}

#if defined(HWASIMIR_HAS_AVFORMAT)
std::string AvError(int code)
{
	char text[AV_ERROR_MAX_STRING_SIZE] = {};
	av_strerror(code, text, sizeof(text));
	return text;
}

std::vector<std::uint8_t> ExtractParameterSets(const std::vector<std::uint8_t>& payload)
{
	std::vector<std::uint8_t> result;
	std::size_t index = 0;
	while (index + 3 < payload.size())
	{
		std::size_t start = std::string::npos;
		std::size_t nal = std::string::npos;
		for (std::size_t i = index; i + 3 < payload.size(); ++i)
		{
			if (payload[i] == 0 && payload[i + 1] == 0 && payload[i + 2] == 1)
			{
				start = i; nal = i + 3; break;
			}
			if (i + 4 < payload.size() && payload[i] == 0 && payload[i + 1] == 0 &&
				payload[i + 2] == 0 && payload[i + 3] == 1)
			{
				start = i; nal = i + 4; break;
			}
		}
		if (start == std::string::npos || nal >= payload.size()) break;
		std::size_t next = payload.size();
		for (std::size_t i = nal + 1; i + 3 < payload.size(); ++i)
		{
			if (payload[i] == 0 && payload[i + 1] == 0 &&
				(payload[i + 2] == 1 || (i + 4 < payload.size() && payload[i + 2] == 0 && payload[i + 3] == 1)))
			{
				next = i; break;
			}
		}
		const std::uint8_t type = payload[nal] & 0x1f;
		if (type == 7 || type == 8)
		{
			static const std::uint8_t startCode[4] = { 0, 0, 0, 1 };
			result.insert(result.end(), startCode, startCode + 4);
			result.insert(result.end(), payload.begin() + nal, payload.begin() + next);
		}
		index = next;
	}
	return result;
}
#endif
}

struct LocalMp4Recorder::Impl
{
	struct Item
	{
		std::vector<std::uint8_t> bytes;
		bool h264 = true;
		bool keyFrame = false;
		int width = 0;
		int height = 0;
	};

	LocalMp4RecorderConfig config;
	std::string channel = "unknown";
	std::atomic<bool> protocolEnabled{ false };
	std::atomic<bool> sessionActive{ false };
	std::atomic<bool> accepting{ false };
	std::atomic<bool> running{ false };
	std::atomic<bool> healthy{ true };
	std::thread worker;
	mutable std::mutex mutex;
	std::condition_variable dataCv;
	std::condition_variable spaceCv;
	std::condition_variable drainedCv;
	std::deque<Item> queue;
	bool writeInProgress = false;
	bool closeRequested = false;
	int round = 0;
	int width = 0;
	int height = 0;
	int fps = 60;
	LocalMp4RecorderStats stats;
	double writeMsTotal = 0.0;
	std::int64_t lastPerfLogNs = 0;
	std::string fatalError;
	cv::VideoWriter cvWriter;
	std::vector<std::uint8_t> flipBuffer;
#if defined(HWASIMIR_HAS_AVFORMAT)
	AVFormatContext* format = nullptr;
	AVStream* stream = nullptr;
	AVPacket* packet = nullptr;
	AVRational frameTimeBase{ 1, 60 };
#endif

	std::string backendName() const
	{
#if defined(HWASIMIR_HAS_AVFORMAT)
		return "shared_h264_remux";
#else
		return "opencv_reencode";
#endif
	}

	void logPerf(bool force)
	{
		if (!config.enablePerfLog) return;
		const std::int64_t now = SteadyNowNs();
		if (!force && now - lastPerfLogNs < 2000000000LL) return;
		LocalMp4RecorderStats snapshot;
		{
			std::lock_guard<std::mutex> lock(mutex);
			snapshot = stats;
			snapshot.queueDepth = queue.size();
			snapshot.writeMsAverage = snapshot.writtenFrames
				? writeMsTotal / static_cast<double>(snapshot.writtenFrames) : 0.0;
		}
		std::cout << std::fixed << std::setprecision(3)
			<< "[LocalRecordingPerf] inputFrames=" << snapshot.inputFrames
			<< " writtenFrames=" << snapshot.writtenFrames
			<< " queueDepth=" << snapshot.queueDepth
			<< " maxQueueDepth=" << snapshot.maxQueueDepth
			<< " writeMsAvg=" << snapshot.writeMsAverage
			<< " writeMsMax=" << snapshot.writeMsMaximum
			<< " droppedFrames=0"
			<< " outputPath=" << snapshot.outputPath
			<< " backend=" << snapshot.backend << std::endl;
		lastPerfLogNs = now;
	}

	std::string makePath()
	{
		std::ostringstream dir;
		dir << config.outputDirectory;
		if (!config.outputDirectory.empty() && config.outputDirectory.back() != '/' &&
			config.outputDirectory.back() != '\\') dir << '/';
		dir << channel;
		if (!CreateDirectories(dir.str())) return std::string();
		std::ostringstream path;
		path << dir.str() << '/' << config.filePrefix << '_' << channel
			<< "_round" << std::setw(3) << std::setfill('0') << round
			<< '_' << Timestamp() << ".mp4";
		return path.str();
	}

#if defined(HWASIMIR_HAS_AVFORMAT)
	bool openAvformat(const Item& first, std::string& error)
	{
		stats.outputPath = makePath();
		if (stats.outputPath.empty()) { error = "cannot create recording directory"; return false; }
		int result = avformat_alloc_output_context2(&format, nullptr, "mp4", stats.outputPath.c_str());
		if (result < 0 || !format) { error = "avformat_alloc_output_context2: " + AvError(result); return false; }
		stream = avformat_new_stream(format, nullptr);
		if (!stream) { error = "avformat_new_stream failed"; return false; }
		frameTimeBase = AVRational{ 1, (std::max)(1, fps) };
		stream->time_base = frameTimeBase;
		stream->avg_frame_rate = AVRational{ (std::max)(1, fps), 1 };
		AVCodecParameters* parameters = stream->codecpar;
		parameters->codec_type = AVMEDIA_TYPE_VIDEO;
		parameters->codec_id = AV_CODEC_ID_H264;
		parameters->width = width;
		parameters->height = height;
		parameters->bit_rate = static_cast<std::int64_t>(config.bitrateKbps) * 1000;
		const std::vector<std::uint8_t> extra = ExtractParameterSets(first.bytes);
		if (extra.empty()) { error = "first H264 AU has no SPS/PPS"; return false; }
		parameters->extradata = static_cast<std::uint8_t*>(av_mallocz(extra.size() + AV_INPUT_BUFFER_PADDING_SIZE));
		if (!parameters->extradata) { error = "av_mallocz extradata failed"; return false; }
		std::memcpy(parameters->extradata, extra.data(), extra.size());
		parameters->extradata_size = static_cast<int>(extra.size());
		if (!(format->oformat->flags & AVFMT_NOFILE))
		{
			result = avio_open(&format->pb, stats.outputPath.c_str(), AVIO_FLAG_WRITE);
			if (result < 0) { error = "avio_open: " + AvError(result); return false; }
		}
		AVDictionary* options = nullptr;
		av_dict_set(&options, "movflags", "+faststart", 0);
		result = avformat_write_header(format, &options);
		av_dict_free(&options);
		if (result < 0) { error = "avformat_write_header: " + AvError(result); return false; }
		packet = av_packet_alloc();
		if (!packet) { error = "av_packet_alloc failed"; return false; }
		stats.backend = "shared_h264_remux";
		std::cout << "[LocalRecording] backend=shared_h264_remux outputPath="
			<< stats.outputPath << " firstPacketIdr=1" << std::endl;
		return true;
	}

	bool writeAvformat(const Item& item, std::string& error)
	{
		if (!format && !openAvformat(item, error)) return false;
		av_packet_unref(packet);
		const int result = av_new_packet(packet, static_cast<int>(item.bytes.size()));
		if (result < 0) { error = "av_new_packet: " + AvError(result); return false; }
		std::memcpy(packet->data, item.bytes.data(), item.bytes.size());
		packet->stream_index = stream->index;
		packet->pts = static_cast<std::int64_t>(stats.writtenFrames);
		packet->dts = packet->pts;
		packet->duration = 1;
		// The MP4 muxer may replace stream->time_base (for example with 1/15360)
		// while writing the header.  Our timestamps are frame ordinals in 1/fps,
		// so rescale every packet into the muxer's final stream time base.
		av_packet_rescale_ts(packet, frameTimeBase, stream->time_base);
		if (item.keyFrame || ContainsIdr(item.bytes)) packet->flags |= AV_PKT_FLAG_KEY;
		const int writeResult = av_interleaved_write_frame(format, packet);
		if (writeResult < 0) { error = "av_interleaved_write_frame: " + AvError(writeResult); return false; }
		return true;
	}
#endif

	bool writeOpenCv(const Item& item, std::string& error)
	{
		if (!cvWriter.isOpened())
		{
			stats.outputPath = makePath();
			if (stats.outputPath.empty()) { error = "cannot create recording directory"; return false; }
			const int codecs[] = {
				cv::VideoWriter::fourcc('H', '2', '6', '4'),
				cv::VideoWriter::fourcc('X', 'V', 'I', 'D'),
				cv::VideoWriter::fourcc('M', 'J', 'P', 'G')
			};
			for (int codec : codecs)
			{
				if (cvWriter.open(stats.outputPath, codec, static_cast<double>(fps), cv::Size(width, height), true)) break;
			}
			if (!cvWriter.isOpened()) { error = "OpenCV VideoWriter open failed"; return false; }
			stats.backend = "opencv_reencode";
			std::cout << "[LocalRecording] backend=opencv_reencode reason=avformat_unavailable outputPath="
				<< stats.outputPath << std::endl;
		}
		cv::Mat frame(item.height, item.width, CV_8UC3, const_cast<std::uint8_t*>(item.bytes.data()));
		cvWriter.write(frame);
		return true;
	}

	void closeFile()
	{
#if defined(HWASIMIR_HAS_AVFORMAT)
		if (format)
		{
			av_write_trailer(format);
			if (!(format->oformat->flags & AVFMT_NOFILE) && format->pb) avio_closep(&format->pb);
			if (packet) av_packet_free(&packet);
			avformat_free_context(format);
			format = nullptr;
			stream = nullptr;
		}
#endif
		if (cvWriter.isOpened()) cvWriter.release();
	}

	bool enqueue(Item&& item, double* backpressureMs, std::string& error)
	{
		if (backpressureMs) *backpressureMs = 0.0;
		if (!accepting.load()) return true;
		const auto before = std::chrono::steady_clock::now();
		std::unique_lock<std::mutex> lock(mutex);
		if (queue.size() >= config.queueMaxFrames)
		{
			if (!config.blockWhenQueueFull)
			{
				error = "recording queue full and dropping is forbidden";
				healthy.store(false);
				accepting.store(false);
				return false;
			}
			spaceCv.wait(lock, [this] {
				return queue.size() < config.queueMaxFrames || !running.load() || !healthy.load();
			});
		}
		if (!healthy.load()) { error = fatalError; return false; }
		const double waited = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - before).count();
		queue.push_back(std::move(item));
		++stats.inputFrames;
		stats.queueDepth = queue.size();
		stats.maxQueueDepth = (std::max)(stats.maxQueueDepth, queue.size());
		if (backpressureMs) *backpressureMs = waited;
		lock.unlock();
		dataCv.notify_one();
		return true;
	}

	void run()
	{
		while (true)
		{
			Item item;
			{
				std::unique_lock<std::mutex> lock(mutex);
				dataCv.wait(lock, [this] { return !queue.empty() || closeRequested || !running.load(); });
				if (queue.empty())
				{
					if (closeRequested)
					{
						closeFile();
						closeRequested = false;
						sessionActive.store(false);
						drainedCv.notify_all();
					}
					if (!running.load()) break;
					continue;
				}
				item = std::move(queue.front());
				queue.pop_front();
				writeInProgress = true;
				spaceCv.notify_all();
			}
			const auto before = std::chrono::steady_clock::now();
			std::string error;
			bool ok = false;
#if defined(HWASIMIR_HAS_AVFORMAT)
			ok = item.h264 && writeAvformat(item, error);
#else
			ok = !item.h264 && writeOpenCv(item, error);
#endif
			const double writeMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - before).count();
			{
				std::lock_guard<std::mutex> lock(mutex);
				writeInProgress = false;
				if (ok)
				{
					++stats.writtenFrames;
					writeMsTotal += writeMs;
					stats.writeMsMaximum = (std::max)(stats.writeMsMaximum, writeMs);
				}
				else
				{
					healthy.store(false);
					accepting.store(false);
					fatalError = error;
					std::cerr << "[LocalRecording][ERROR] reason=" << error << std::endl;
				}
				if (queue.empty()) drainedCv.notify_all();
			}
			logPerf(false);
		}
		closeFile();
		logPerf(true);
	}
};

LocalMp4Recorder::LocalMp4Recorder() : m_impl(new Impl()) {}
LocalMp4Recorder::~LocalMp4Recorder() { shutdown(); }

void LocalMp4Recorder::configure(const LocalMp4RecorderConfig& config, const std::string& channel)
{
	m_impl->config = config;
	m_impl->channel = channel.empty() ? "unknown" : channel;
	std::cout << "[LocalRecording] configured=1 Enable=" << (config.enabled ? "1" : "0")
		<< " backend=" << m_impl->backendName()
		<< " OutputDirectory=" << config.outputDirectory
		<< " QueueMaxFrames=" << config.queueMaxFrames
		<< " BlockWhenQueueFull=" << (config.blockWhenQueueFull ? "1" : "0") << std::endl;
}

void LocalMp4Recorder::setProtocolEnabled(bool saveMp4En)
{
	m_impl->protocolEnabled.store(saveMp4En);
	std::cout << "[LocalRecording] configEnable=" << (m_impl->config.enabled ? "1" : "0")
		<< " saveMP4En=" << (saveMp4En ? "1" : "0")
		<< " effectiveSaveMp4=" << (effectiveEnabled() ? "1" : "0") << std::endl;
}

bool LocalMp4Recorder::effectiveEnabled() const
{
	return m_impl->config.enabled && m_impl->protocolEnabled.load();
}

bool LocalMp4Recorder::wantsH264() const
{
#if defined(HWASIMIR_HAS_AVFORMAT)
	return true;
#else
	return false;
#endif
}

bool LocalMp4Recorder::startPending(int round, int width, int height, int fps, std::string& error)
{
	if (!effectiveEnabled())
	{
		std::cout << "[LocalRecording] startIgnored=1 effectiveSaveMp4=0" << std::endl;
		return true;
	}
	if (m_impl->sessionActive.load() && !stopAndFlush("next_start", error)) return false;
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		m_impl->round = round;
		m_impl->width = width;
		m_impl->height = height;
		m_impl->fps = (std::max)(1, fps);
		m_impl->stats = LocalMp4RecorderStats();
		m_impl->stats.backend = m_impl->backendName();
		m_impl->writeMsTotal = 0.0;
		m_impl->fatalError.clear();
		m_impl->healthy.store(true);
		m_impl->closeRequested = false;
	}
	if (!m_impl->running.load())
	{
		m_impl->running.store(true);
		m_impl->worker = std::thread(&Impl::run, m_impl.get());
	}
	m_impl->sessionActive.store(true);
	m_impl->accepting.store(true);
	std::cout << "[LocalRecording] startPending=1 round=" << round
		<< " size=" << width << "x" << height << " fps=" << fps
		<< " fileCreated=0 requestIdr=1" << std::endl;
	return true;
}

bool LocalMp4Recorder::enqueueH264(const std::uint8_t* data, std::size_t size, bool keyFrame,
	int width, int height, double* backpressureMs, std::string& error)
{
	if (!effectiveEnabled() || !m_impl->accepting.load()) return true;
	if (!data || size == 0) { error = "empty H264 AU"; return false; }
	if (m_impl->width <= 0 || m_impl->height <= 0)
	{
		m_impl->width = width;
		m_impl->height = height;
	}
	Impl::Item item;
	item.bytes.assign(data, data + size);
	item.h264 = true;
	item.keyFrame = keyFrame || ContainsIdr(item.bytes);
	item.width = m_impl->width;
	item.height = m_impl->height;
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		if (m_impl->stats.inputFrames == 0 && !item.keyFrame)
		{
			error = "first recording AU is not IDR";
			m_impl->healthy.store(false);
			m_impl->accepting.store(false);
			std::cerr << "[LocalRecording][ERROR] reason=" << error << std::endl;
			return false;
		}
	}
	return m_impl->enqueue(std::move(item), backpressureMs, error);
}

bool LocalMp4Recorder::enqueueRawBgr24(const std::uint8_t* data, int width, int height,
	bool flipVertical, double* backpressureMs, std::string& error)
{
	if (!effectiveEnabled() || !m_impl->accepting.load()) return true;
	if (!data || width <= 0 || height <= 0) { error = "invalid raw recording frame"; return false; }
	Impl::Item item;
	item.h264 = false;
	item.width = width;
	item.height = height;
	const std::size_t rowBytes = static_cast<std::size_t>(width) * 3u;
	item.bytes.resize(rowBytes * static_cast<std::size_t>(height));
	for (int row = 0; row < height; ++row)
	{
		const int sourceRow = flipVertical ? (height - 1 - row) : row;
		std::memcpy(item.bytes.data() + static_cast<std::size_t>(row) * rowBytes,
			data + static_cast<std::size_t>(sourceRow) * rowBytes, rowBytes);
	}
	return m_impl->enqueue(std::move(item), backpressureMs, error);
}

bool LocalMp4Recorder::stopAndFlush(const char* reason, std::string& error)
{
	if (!m_impl->sessionActive.load()) return true;
	m_impl->accepting.store(false);
	{
		std::unique_lock<std::mutex> lock(m_impl->mutex);
		const bool drained = m_impl->drainedCv.wait_for(lock,
			std::chrono::milliseconds(m_impl->config.flushTimeoutMs),
			[this] { return m_impl->queue.empty() && !m_impl->writeInProgress; });
		if (!drained) { error = "recording queue flush timeout"; return false; }
		m_impl->closeRequested = true;
	}
	m_impl->dataCv.notify_all();
	{
		std::unique_lock<std::mutex> lock(m_impl->mutex);
		const bool closed = m_impl->drainedCv.wait_for(lock,
			std::chrono::milliseconds(m_impl->config.flushTimeoutMs),
			[this] { return !m_impl->closeRequested && !m_impl->sessionActive.load(); });
		if (!closed) { error = "recording close timeout"; return false; }
	}
	m_impl->logPerf(true);
	const LocalMp4RecorderStats value = stats();
	if (!m_impl->healthy.load() || value.inputFrames != value.writtenFrames)
	{
		error = m_impl->fatalError.empty() ? "recording input/written mismatch" : m_impl->fatalError;
		return false;
	}
	std::cout << "[LocalRecording] closed=1 reason=" << (reason ? reason : "unspecified")
		<< " inputFrames=" << value.inputFrames
		<< " writtenFrames=" << value.writtenFrames
		<< " droppedFrames=0 outputPath=" << value.outputPath << std::endl;
	return true;
}

void LocalMp4Recorder::shutdown()
{
	if (!m_impl) return;
	std::string error;
	stopAndFlush("shutdown", error);
	m_impl->accepting.store(false);
	m_impl->running.store(false);
	m_impl->dataCv.notify_all();
	m_impl->spaceCv.notify_all();
	if (m_impl->worker.joinable()) m_impl->worker.join();
}

LocalMp4RecorderStats LocalMp4Recorder::stats() const
{
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	LocalMp4RecorderStats value = m_impl->stats;
	value.queueDepth = m_impl->queue.size();
	value.writeMsAverage = value.writtenFrames
		? m_impl->writeMsTotal / static_cast<double>(value.writtenFrames) : 0.0;
	return value;
}
