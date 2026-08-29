#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1DataWriter.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "MppH264GrayDecoder.h"
#include "ZRDDSCppSimpleInterface.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace DDS;

struct Options
{
    int domain = 150;
    int timeoutSec = 120;
    int ackTimeoutSec = 60;
    int shutdownDrainMs = 5000;
    std::uint64_t frames = 0;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string statusTopic = "HwaSimIR.VideoStatus";
    std::string channel = "precise";
    std::string decodedTopic;
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
        else if (arg == "--channel") o.channel = value;
        else if (arg == "--decoded-topic") o.decodedTopic = value;
        else if (arg == "--frames") o.frames = std::strtoull(value.c_str(), nullptr, 10);
        else if (arg == "--timeout-sec") o.timeoutSec = std::atoi(value.c_str());
        else if (arg == "--ack-timeout-sec") o.ackTimeoutSec = std::atoi(value.c_str());
        else if (arg == "--shutdown-drain-ms") o.shutdownDrainMs = std::atoi(value.c_str());
        else throw std::runtime_error("unknown option " + arg);
    }
    if (o.decodedTopic.empty())
        o.decodedTopic = "HwaSimIR.Decoded." + o.channel + ".RawGray8";
    return o;
}

static void CopyBounded(char* target, std::size_t capacity, const std::string& value)
{
    if (!target || capacity == 0) return;
    const std::size_t count = (std::min)(capacity - 1, value.size());
    std::memcpy(target, value.data(), count);
    target[count] = '\0';
}

struct SourceStatus
{
    int platID = 0, sensorID = 0, width = 0, height = 0, fps = 0, currentRound = 0;
    bool running = false;
    std::string channel, topic;
};

class GatewayState
{
public:
    explicit GatewayState(const Options& options) : o(options) {}

    bool initialize(DomainParticipant* participant, std::string& error)
    {
        if (!decoder.initialize(error)) return false;
        rawWriterBase = DDSIF::PubTopic(participant, o.decodedTopic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", nullptr);
        statusWriterBase = DDSIF::PubTopic(participant, o.statusTopic.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(), "hwasimir_status_writer", nullptr);
        statusWriter = dynamic_cast<HwaSimIRDds::VideoStatusV1DataWriter*>(statusWriterBase);
        if (!rawWriterBase || !statusWriter)
        {
            error = "gateway writer creation failed";
            return false;
        }
        return true;
    }

    bool accepts(const HwaSimIRDds::VideoStatusV1& s) const
    {
        return (s.channel ? s.channel : "") == o.channel &&
            (s.codec ? s.codec : "") == "h264";
    }

    void onStatus(const HwaSimIRDds::VideoStatusV1& s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        source.platID = s.platID; source.sensorID = s.sensorID;
        source.width = s.width; source.height = s.height; source.fps = s.fps;
        source.currentRound = s.currentRound; source.running = s.running != 0;
        source.channel = s.channel ? s.channel : "";
        source.topic = s.videoTopic ? s.videoTopic : "";
        ++statusReceived;
        if (source.running && !decodedRunning)
        {
            if (decoderFlushed)
            {
                decoder.shutdown();
                std::string error;
                if (!decoder.initialize(error))
                {
                    ++decodeErrors;
                    std::cerr << "[Gateway][ERROR] decoderRoundReset=" << error << std::endl;
                    changed.notify_all();
                    return;
                }
                decoderFlushed = false;
            }
            stopped = false;
            decodedRunning = true;
            publishStatusLocked(true);
        }
        else if (!source.running && decodedRunning)
        {
            std::vector<MppDecodedGrayFrame> flushed;
            std::string error;
            if (!decoder.flush(flushed, error))
            {
                ++decodeErrors;
                std::cerr << "[Gateway][ERROR] decoderFlush=" << error << std::endl;
            }
            publishFramesLocked(flushed);
            drainRawLocked();
            publishStatusLocked(false);
            decodedRunning = false;
            decoderFlushed = true;
            stopped = true;
        }
        changed.notify_all();
    }

    void onVideo(const Bytes& sample)
    {
        const std::uint8_t* data = sample.value.get_contiguous_buffer();
        const std::size_t size = static_cast<std::size_t>(sample.value.length());
        std::lock_guard<std::mutex> lock(mutex);
        ++sourceAuCount;
        if (!data || size == 0)
        {
            ++ddsErrors;
            changed.notify_all();
            return;
        }
        const auto begin = std::chrono::steady_clock::now();
        std::vector<MppDecodedGrayFrame> decoded;
        std::string error;
        if (!decoder.pushAccessUnit(data, size, decoded, error))
        {
            ++decodeErrors;
            std::cerr << "[Gateway][ERROR] sourceAU=" << sourceAuCount
                      << " decode=" << error << std::endl;
        }
        publishFramesLocked(decoded);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        decodeMsTotal += elapsed;
        decodeMsMax = (std::max)(decodeMsMax, elapsed);
        changed.notify_all();
    }

    bool waitForSource(std::string& topic)
    {
        std::unique_lock<std::mutex> lock(mutex);
        const bool ok = changed.wait_for(lock, std::chrono::seconds(o.timeoutSec), [this] {
            return source.running && !source.topic.empty();
        });
        topic = source.topic;
        return ok;
    }

    bool waitComplete()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(o.timeoutSec), [this] {
            if (o.frames > 0)
                return rawPublished >= o.frames && sourceAuCount == decodedFrames;
            return stopped && sourceAuCount > 0 && sourceAuCount == decodedFrames;
        });
    }

    int summary(bool complete)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << std::fixed << std::setprecision(3)
            << "statusReceived=" << statusReceived
            << " sourceH264AUs=" << sourceAuCount
            << " decodedFrames=" << decodedFrames
            << " rawPublished=" << rawPublished
            << " rawBytes=" << rawBytes
            << " decodeMsAvg=" << (sourceAuCount ? decodeMsTotal / sourceAuCount : 0.0)
            << " decodeMsMax=" << decodeMsMax
            << " decodedWidth=" << decodedWidth << " decodedHeight=" << decodedHeight
            << " decodeErrors=" << decodeErrors << " writerErrors=" << writerErrors
            << " ddsErrors=" << ddsErrors << " dropped=0" << std::endl;
        return complete && sourceAuCount == decodedFrames && decodedFrames == rawPublished &&
            decodeErrors == 0 && writerErrors == 0 && ddsErrors == 0 ? 0 : 8;
    }

    DataWriter* rawWriter() const { return rawWriterBase; }
    DataWriter* statusWriterBasePtr() const { return statusWriterBase; }

private:
    void publishFramesLocked(const std::vector<MppDecodedGrayFrame>& frames)
    {
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            const MppDecodedGrayFrame& frame = frames[i];
            decodedWidth = frame.width; decodedHeight = frame.height;
            if (source.width > 0 && (frame.width != source.width || frame.height != source.height))
            {
                ++decodeErrors;
                continue;
            }
            ++decodedFrames;
            const ReturnCode_t result = DDSIF::BytesWrite(o.domain,
                const_cast<char*>(o.decodedTopic.c_str()),
                reinterpret_cast<const char*>(frame.gray8.data()),
                static_cast<DDS_Long>(frame.gray8.size()));
            if (result != RETCODE_OK) ++writerErrors;
            else { ++rawPublished; rawBytes += frame.gray8.size(); }
        }
    }

    void publishStatusLocked(bool running)
    {
        HwaSimIRDds::VideoStatusV1 sample;
        if (!HwaSimIRDds::VideoStatusV1Initialize(&sample))
        {
            ++writerErrors;
            return;
        }
        sample.platID = source.platID; sample.sensorID = source.sensorID;
        CopyBounded(sample.channel, 17, source.channel);
        sample.running = running;
        CopyBounded(sample.codec, 25, "raw_gray8");
        CopyBounded(sample.pixelFormat, 25, "gray8");
        CopyBounded(sample.videoTopic, 129, o.decodedTopic);
        sample.width = decodedWidth > 0 ? decodedWidth : source.width;
        sample.height = decodedHeight > 0 ? decodedHeight : source.height;
        sample.fps = source.fps; sample.bitrateKbps = 0; sample.gopFrames = 0;
        sample.compressed = false; sample.currentRound = source.currentRound;
        if (statusWriter->write(sample, HANDLE_NIL_NATIVE) != RETCODE_OK) ++writerErrors;
        HwaSimIRDds::VideoStatusV1Finalize(&sample);
        std::cout << "[GatewayStatus] running=" << (running ? 1 : 0)
                  << " topic=" << o.decodedTopic << " width=" << source.width
                  << " height=" << source.height << " round=" << source.currentRound << std::endl;
    }

    void drainRawLocked()
    {
        Duration_t timeout;
        timeout.sec = o.ackTimeoutSec; timeout.nanosec = 0;
        if (rawWriterBase && rawWriterBase->wait_for_acknowledgments(timeout) != RETCODE_OK)
            ++writerErrors;
        if (o.shutdownDrainMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(o.shutdownDrainMs));
    }

    const Options& o;
    mutable std::mutex mutex;
    std::condition_variable changed;
    SourceStatus source;
    MppH264GrayDecoder decoder;
    DataWriter* rawWriterBase = nullptr;
    DataWriter* statusWriterBase = nullptr;
    HwaSimIRDds::VideoStatusV1DataWriter* statusWriter = nullptr;
    bool decodedRunning = false, decoderFlushed = false, stopped = false;
    std::uint64_t statusReceived = 0, sourceAuCount = 0, decodedFrames = 0;
    std::uint64_t rawPublished = 0, rawBytes = 0;
    std::uint64_t decodeErrors = 0, writerErrors = 0, ddsErrors = 0;
    int decodedWidth = 0, decodedHeight = 0;
    double decodeMsTotal = 0.0, decodeMsMax = 0.0;
};

class StatusListener : public SimpleDataReaderListener<HwaSimIRDds::VideoStatusV1,
    HwaSimIRDds::VideoStatusV1Seq, HwaSimIRDds::VideoStatusV1DataReader>
{
public:
    explicit StatusListener(GatewayState& state) : m_state(state) {}
    void on_process_sample(DataReader*, const HwaSimIRDds::VideoStatusV1& sample,
        const SampleInfo&) override { if (m_state.accepts(sample)) m_state.onStatus(sample); }
private: GatewayState& m_state;
};

class VideoListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    explicit VideoListener(GatewayState& state) : m_state(state) {}
    void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) override
    { m_state.onVideo(sample); }
private: GatewayState& m_state;
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
        GatewayState state(o);
        std::string error;
        if (!state.initialize(participant, error)) throw std::runtime_error(error);
        StatusListener statusListener(state);
        DataReader* statusReader = DDSIF::SubTopic(participant, o.statusTopic.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(),
            "hwasimir_status_reader", &statusListener);
        if (!statusReader) throw std::runtime_error("status SubTopic failed");
        std::string sourceTopic;
        if (!state.waitForSource(sourceTopic)) throw std::runtime_error("source H264 status timeout");
        VideoListener videoListener(state);
        DataReader* sourceReader = DDSIF::SubTopic(participant, sourceTopic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &videoListener);
        if (!sourceReader) throw std::runtime_error("source H264 SubTopic failed");
        std::cout << "gatewayReady=1 channel=" << o.channel << " sourceTopic=" << sourceTopic
                  << " decodedTopic=" << o.decodedTopic << " decoder=rkmpp" << std::endl;
        const bool complete = state.waitComplete();
        const int result = state.summary(complete);
        DDSIF::UnSubTopic(sourceReader);
        DDSIF::UnSubTopic(statusReader);
        DDSIF::UnPubTopic(state.rawWriter());
        DDSIF::UnPubTopic(state.statusWriterBasePtr());
        DDSIF::Finalize();
        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "gateway_error=" << e.what() << std::endl;
        return 1;
    }
}
