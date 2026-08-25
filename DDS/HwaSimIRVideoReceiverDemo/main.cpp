#include "ZRDDSCppSimpleInterface.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace DDS;

namespace {

struct Options {
    int domain = 150;
    std::string topic = "HwaSimIR.Video.precise.H264";
    std::string codec = "h264";
    std::uint64_t width = 0;
    std::uint64_t height = 0;
    std::string qos = "Config/ZRDDS_QOS_PROFILES.xml";
    std::string output = "received.h264";
    std::uint64_t frames = 0;
    int timeoutSec = 120;
};

void PrintUsage() {
    std::cout
        << "Usage: HwaSimIRVideoReceiverDemo --domain N --topic NAME\n"
        << "  --codec h264|raw_gray8|raw_bgr24 --width N --height N\n"
        << "  --qos FILE --output FILE --frames N [--timeout-sec N]\n";
}

std::uint64_t ParseUnsigned(const char* text, const char* option) {
    char* end = NULL;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (!text[0] || (end && *end)) {
        throw std::runtime_error(std::string("invalid value for ") + option);
    }
    return static_cast<std::uint64_t>(value);
}

Options ParseOptions(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        }
        if (i + 1 >= argc) {
            throw std::runtime_error("missing value for " + arg);
        }
        const char* value = argv[++i];
        if (arg == "--domain") options.domain = static_cast<int>(ParseUnsigned(value, "--domain"));
        else if (arg == "--topic") options.topic = value;
        else if (arg == "--codec") options.codec = value;
        else if (arg == "--width") options.width = ParseUnsigned(value, "--width");
        else if (arg == "--height") options.height = ParseUnsigned(value, "--height");
        else if (arg == "--qos") options.qos = value;
        else if (arg == "--output") options.output = value;
        else if (arg == "--frames") options.frames = ParseUnsigned(value, "--frames");
        else if (arg == "--timeout-sec") options.timeoutSec = static_cast<int>(ParseUnsigned(value, "--timeout-sec"));
        else throw std::runtime_error("unknown option: " + arg);
    }
    if (options.domain < 0 || options.domain > 232) throw std::runtime_error("--domain must be 0..232");
    if (options.frames == 0) throw std::runtime_error("--frames must be greater than zero");
    if (options.codec != "h264" && options.codec != "raw_gray8" && options.codec != "raw_bgr24") {
        throw std::runtime_error("unsupported --codec");
    }
    if (options.codec != "h264" && (options.width == 0 || options.height == 0)) {
        throw std::runtime_error("raw codecs require non-zero --width and --height");
    }
    return options;
}

class BytesReceiver : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader> {
public:
    BytesReceiver(const Options& options, std::ofstream& output)
        : options_(options), output_(output), expectedBytes_(ExpectedBytes(options)) {}

    virtual void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) {
        const char* data = reinterpret_cast<const char*>(sample.value.get_contiguous_buffer());
        const std::uint64_t bytes = static_cast<std::uint64_t>(sample.value.length());
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) {
            started_ = true;
            first_ = std::chrono::steady_clock::now();
            minBytes_ = bytes;
        }
        last_ = std::chrono::steady_clock::now();
        ++receivedSamples_;
        receivedBytes_ += bytes;
        minBytes_ = (std::min)(minBytes_, bytes);
        maxBytes_ = (std::max)(maxBytes_, bytes);

        if (expectedBytes_ != 0 && bytes != expectedBytes_) {
            ++ddsErrors_;
            std::cerr << "sample_size_error sample=" << receivedSamples_
                      << " expected=" << expectedBytes_ << " actual=" << bytes << "\n";
        } else if (bytes != 0 && data == NULL) {
            ++ddsErrors_;
            std::cerr << "sample_buffer_error sample=" << receivedSamples_ << "\n";
        } else {
            output_.write(data, static_cast<std::streamsize>(bytes));
            if (!output_) {
                ++ddsErrors_;
                std::cerr << "output_write_error sample=" << receivedSamples_ << "\n";
            }
        }
        if (receivedSamples_ >= options_.frames) condition_.notify_all();
    }

    bool Wait() {
        std::unique_lock<std::mutex> lock(mutex_);
        return condition_.wait_for(lock, std::chrono::seconds(options_.timeoutSec), [this]() {
            return receivedSamples_ >= options_.frames;
        });
    }

    int PrintSummary(bool timedOut) {
        std::lock_guard<std::mutex> lock(mutex_);
        output_.flush();
        if (!output_) ++ddsErrors_;
        const double seconds = started_
            ? std::chrono::duration<double>(last_ - first_).count()
            : 0.0;
        const double fps = seconds > 0.0 ? static_cast<double>(receivedSamples_) / seconds : 0.0;
        const double average = receivedSamples_ != 0
            ? static_cast<double>(receivedBytes_) / static_cast<double>(receivedSamples_)
            : 0.0;
        std::cout << std::fixed << std::setprecision(3)
                  << "receivedSamples=" << receivedSamples_
                  << " receivedBytes=" << receivedBytes_
                  << " sampleBytesMin=" << (receivedSamples_ ? minBytes_ : 0)
                  << " sampleBytesAvg=" << average
                  << " sampleBytesMax=" << maxBytes_
                  << " receiveFps=" << fps
                  << " elapsedSec=" << seconds
                  << " ddsErrors=" << ddsErrors_
                  << " timedOut=" << (timedOut ? 1 : 0) << "\n";
        return (!timedOut && receivedSamples_ == options_.frames && ddsErrors_ == 0) ? 0 : 8;
    }

private:
    static std::uint64_t ExpectedBytes(const Options& options) {
        if (options.codec == "h264") return 0;
        const std::uint64_t channels = options.codec == "raw_bgr24" ? 3 : 1;
        if (options.width > (std::numeric_limits<std::uint64_t>::max)() / options.height ||
            options.width * options.height > (std::numeric_limits<std::uint64_t>::max)() / channels) {
            throw std::runtime_error("raw frame byte size overflow");
        }
        return options.width * options.height * channels;
    }

    const Options& options_;
    std::ofstream& output_;
    const std::uint64_t expectedBytes_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool started_ = false;
    std::chrono::steady_clock::time_point first_;
    std::chrono::steady_clock::time_point last_;
    std::uint64_t receivedSamples_ = 0;
    std::uint64_t receivedBytes_ = 0;
    std::uint64_t minBytes_ = 0;
    std::uint64_t maxBytes_ = 0;
    std::uint64_t ddsErrors_ = 0;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = ParseOptions(argc, argv);
        std::ofstream output(options.output.c_str(), std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open output: " + options.output);

        DomainParticipantFactory* factory = DDSIF::Init(options.qos.c_str(), "hwasimir_factory");
        if (factory == NULL) {
            std::cerr << "DDSIF::Init failed ddsErrors=1\n";
            return 2;
        }
        DomainParticipant* participant = DDSIF::CreateDP(options.domain, "hwasimir_tcp");
        if (participant == NULL) {
            std::cerr << "DDSIF::CreateDP failed ddsErrors=1\n";
            DDSIF::Finalize();
            return 3;
        }

        BytesReceiver listener(options, output);
        DataReader* reader = DDSIF::SubTopic(participant, options.topic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &listener);
        if (reader == NULL) {
            std::cerr << "DDSIF::SubTopic failed ddsErrors=1\n";
            DDSIF::Finalize();
            return 4;
        }

        std::cout << "receiverReady=1 domain=" << options.domain << " topic=" << options.topic
                  << " codec=" << options.codec << " frames=" << options.frames << "\n";
        const bool complete = listener.Wait();
        const ReturnCode_t finalizeResult = DDSIF::Finalize();
        if (finalizeResult != RETCODE_OK) {
            std::cerr << "DDSIF::Finalize failed code=" << finalizeResult << "\n";
            return 7;
        }
        return listener.PrintSummary(!complete);
    } catch (const std::exception& error) {
        std::cerr << "receiver_error=" << error.what() << " ddsErrors=1\n";
        PrintUsage();
        return 1;
    }
}
