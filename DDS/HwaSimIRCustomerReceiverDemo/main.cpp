#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

using namespace DDS;

struct Options
{
    int domain = 150;
    int timeoutSec = 120;
    std::uint64_t frames = 0;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string statusTopic = "HwaSimIR.VideoStatus";
    std::string output = "received.h264";
};

static Options Parse(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (i + 1 >= argc) throw std::runtime_error("missing value for " + arg);
        const std::string value(argv[++i]);
        if (arg == "--domain") o.domain = std::atoi(value.c_str());
        else if (arg == "--qos") o.qos = value;
        else if (arg == "--status-topic") o.statusTopic = value;
        else if (arg == "--output") o.output = value;
        else if (arg == "--frames") o.frames = std::strtoull(value.c_str(), 0, 10);
        else if (arg == "--timeout-sec") o.timeoutSec = std::atoi(value.c_str());
        else throw std::runtime_error("unknown option " + arg);
    }
    return o;
}

struct StatusSnapshot
{
    bool received = false, running = false, compressed = false;
    int width = 0, height = 0, fps = 0;
    std::string codec, pixelFormat, topic;
};

class StatusListener : public SimpleDataReaderListener<HwaSimIRDds::VideoStatusV1,
    HwaSimIRDds::VideoStatusV1Seq, HwaSimIRDds::VideoStatusV1DataReader>
{
public:
    void on_process_sample(DataReader*, const HwaSimIRDds::VideoStatusV1& s, const SampleInfo&) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        status.received = true; status.running = s.running != 0; status.compressed = s.compressed != 0;
        status.width = s.width; status.height = s.height; status.fps = s.fps;
        status.codec = s.codec; status.pixelFormat = s.pixelFormat; status.topic = s.videoTopic;
        ++count; ready.notify_all();
    }
    bool wait(int timeoutSec)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return ready.wait_for(lock, std::chrono::seconds(timeoutSec), [this] {
            return status.received && status.running && !status.topic.empty();
        });
    }
    StatusSnapshot snapshot() { std::lock_guard<std::mutex> lock(mutex); return status; }
    std::uint64_t statusCount() { std::lock_guard<std::mutex> lock(mutex); return count; }
private:
    std::mutex mutex; std::condition_variable ready; StatusSnapshot status; std::uint64_t count = 0;
};

class VideoListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    VideoListener(const Options& o, const StatusSnapshot& s, std::ofstream& f) : options(o), status(s), output(f) {}
    void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) override
    {
        const std::uint64_t size = sample.value.length();
        const bool raw = status.codec == "raw" || status.codec == "raw_gray8" ||
            status.codec == "raw_bgr24";
        const std::uint64_t channels = status.codec == "raw_bgr24" ||
            status.pixelFormat == "bgr24" ? 3u : 1u;
        const std::uint64_t expected = raw
            ? static_cast<std::uint64_t>(status.width) * status.height * channels : 0;
        std::lock_guard<std::mutex> lock(mutex);
        if (!started) { started = true; first = std::chrono::steady_clock::now(); }
        last = std::chrono::steady_clock::now(); ++samples; bytes += size;
        if ((expected && size != expected) || (size && !sample.value.get_contiguous_buffer())) ++errors;
        else output.write(reinterpret_cast<const char*>(sample.value.get_contiguous_buffer()), static_cast<std::streamsize>(size));
        if (!output) ++errors;
        ready.notify_all();
    }
    bool wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (options.frames == 0)
        {
            return ready.wait_for(lock, std::chrono::seconds(options.timeoutSec), [this] { return samples > 0; });
        }
        return ready.wait_for(lock, std::chrono::seconds(options.timeoutSec), [this] { return samples >= options.frames; });
    }
    int summary(std::uint64_t statusCount, bool complete)
    {
        std::lock_guard<std::mutex> lock(mutex); output.flush();
        const double seconds = started ? std::chrono::duration<double>(last - first).count() : 0.0;
        std::cout << std::fixed << std::setprecision(3)
            << "statusReceived=" << statusCount << " videoSamples=" << samples
            << " videoBytes=" << bytes << " receiveFps=" << (seconds > 0 ? samples / seconds : 0.0)
            << " decodeFps=0 codec=" << status.codec << " pixelFormat=" << status.pixelFormat
            << " width=" << status.width << " height=" << status.height
            << " ddsErrors=" << errors << std::endl;
        return complete && errors == 0 ? 0 : 8;
    }
private:
    const Options& options; StatusSnapshot status; std::ofstream& output; std::mutex mutex;
    std::condition_variable ready; bool started = false; std::chrono::steady_clock::time_point first, last;
    std::uint64_t samples = 0, bytes = 0, errors = 0;
};

int main(int argc, char** argv)
{
    try
    {
        const Options o = Parse(argc, argv);
        DomainParticipantFactory* factory = DDSIF::Init(o.qos.c_str(), "hwasimir_factory");
        if (!factory) throw std::runtime_error("DDSIF::Init failed");
        DomainParticipant* participant = DDSIF::CreateDP(o.domain, "hwasimir_tcp");
        if (!participant) throw std::runtime_error("DDSIF::CreateDP failed");
        StatusListener statusListener;
        DataReader* statusReader = DDSIF::SubTopic(participant, o.statusTopic.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(), "hwasimir_status_reader", &statusListener);
        if (!statusReader || !statusListener.wait(o.timeoutSec)) throw std::runtime_error("VideoStatus timeout");
        const StatusSnapshot status = statusListener.snapshot();
        std::ofstream output(o.output.c_str(), std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("cannot open output");
        VideoListener videoListener(o, status, output);
        DataReader* videoReader = DDSIF::SubTopic(participant, status.topic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &videoListener);
        if (!videoReader) throw std::runtime_error("video SubTopic failed");
        std::cout << "receiverReady=1 statusTopic=" << o.statusTopic << " autoVideoTopic=" << status.topic << std::endl;
        const bool complete = videoListener.wait();
        const int result = videoListener.summary(statusListener.statusCount(), complete);
        DDSIF::UnSubTopic(videoReader); DDSIF::UnSubTopic(statusReader); DDSIF::Finalize();
        return result;
    }
    catch (const std::exception& e) { std::cerr << "receiver_error=" << e.what() << " ddsErrors=1" << std::endl; return 1; }
}
