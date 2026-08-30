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
#include <deque>
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
    int platID = -1;
    int sensorID = -1;
    std::size_t queueMaxFrames = 120;
    std::uint64_t frames = 0;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string statusTopic = "HwaSimIR.VideoStatus";
    std::string channel = "precise";
    std::string sourceTopicOverride;
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
        else if (arg == "--plat-id") o.platID = std::atoi(value.c_str());
        else if (arg == "--sensor-id") o.sensorID = std::atoi(value.c_str());
        else if (arg == "--video-topic") o.sourceTopicOverride = value;
        else if (arg == "--decoded-topic") o.decodedTopic = value;
        else if (arg == "--queue-max-frames") o.queueMaxFrames =
            static_cast<std::size_t>(std::max(1, std::atoi(value.c_str())));
        else if (arg == "--frames") o.frames = std::strtoull(value.c_str(), nullptr, 10);
        else if (arg == "--timeout-sec") o.timeoutSec = std::atoi(value.c_str());
        else if (arg == "--ack-timeout-sec") o.ackTimeoutSec = std::atoi(value.c_str());
        else if (arg == "--shutdown-drain-ms") o.shutdownDrainMs = std::atoi(value.c_str());
        else throw std::runtime_error("unknown option " + arg);
    }
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
    bool received = false;
    bool running = false;
    int platID = 0, sensorID = 0, width = 0, height = 0, fps = 0, currentRound = 0;
    std::string channel, topic;
};

class GatewayState
{
public:
    explicit GatewayState(const Options& options) : o(options) {}
    ~GatewayState() { stopWorker(); }

    bool initialize(DomainParticipant* participant, std::string& error)
    {
        if (!decoder.initialize(error)) return false;
        statusWriterBase = DDSIF::PubTopic(participant, o.statusTopic.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(),
            "hwasimir_status_writer", nullptr);
        statusWriter = dynamic_cast<HwaSimIRDds::VideoStatusV1DataWriter*>(statusWriterBase);
        if (!statusWriter) { error = "gateway status writer creation failed"; return false; }
        worker = std::thread(&GatewayState::workerLoop, this);
        return true;
    }

    bool configureOutput(DomainParticipant* participant, const SourceStatus& s, std::string& error)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            outputTopic = o.decodedTopic.empty()
                ? "HwaSimIR.Decoded." + std::to_string(s.platID) + "." +
                    std::to_string(s.sensorID) + ".RawGray8"
                : o.decodedTopic;
            rawWriterBase = DDSIF::PubTopic(participant, outputTopic.c_str(),
                BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", nullptr);
            if (!rawWriterBase) { error = "gateway raw writer creation failed"; return false; }
            outputReady = true;
            workChanged.notify_all();
        }
        // Publish the discoverable topic/geometry before START so a status-driven
        // receiver can create its Raw reader without losing the first frame.
        publishStatus(false);
        return true;
    }

    bool accepts(const HwaSimIRDds::VideoStatusV1& s) const
    {
        const std::string channel = s.channel ? s.channel : "";
        const std::string codec = s.codec ? s.codec : "";
        const bool identityMatch = (o.platID < 0 || s.platID == o.platID) &&
            (o.sensorID < 0 || s.sensorID == o.sensorID);
        const bool selectorMatch = (o.platID >= 0 || o.sensorID >= 0)
            ? identityMatch : channel == o.channel;
        const bool accepted = selectorMatch && codec == "h264";
        std::cout << "[GatewaySourceStatus] platID=" << s.platID
                  << " sensorID=" << s.sensorID << " channel=" << channel
                  << " codec=" << codec << " running=" << (s.running ? 1 : 0)
                  << " topic=" << (s.videoTopic ? s.videoTopic : "")
                  << " accepted=" << (accepted ? 1 : 0) << std::endl;
        return accepted;
    }

    void onStatus(const HwaSimIRDds::VideoStatusV1& s)
    {
        WorkKind transition = WorkKind::None;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const bool wasRunning = source.running;
            source.received = true;
            source.platID = s.platID; source.sensorID = s.sensorID;
            source.width = s.width; source.height = s.height; source.fps = s.fps;
            source.currentRound = s.currentRound; source.running = s.running != 0;
            source.channel = s.channel ? s.channel : "";
            source.topic = s.videoTopic ? s.videoTopic : "";
            ++statusReceived;
            if (source.running && !wasRunning) transition = WorkKind::Start;
            else if (!source.running && wasRunning) transition = WorkKind::Stop;
            changed.notify_all();
        }
        if (transition != WorkKind::None) enqueueControl(transition);
    }

    void onVideo(const Bytes& sample)
    {
        const std::uint8_t* data = sample.value.get_contiguous_buffer();
        const std::size_t size = static_cast<std::size_t>(sample.value.length());
        if (!data || size == 0)
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++ddsErrors;
            changed.notify_all();
            return;
        }
        const auto copyBegin = std::chrono::steady_clock::now();
        WorkItem item;
        item.kind = WorkKind::AccessUnit;
        item.payload.assign(data, data + size);
        const double copyMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - copyBegin).count();
        std::unique_lock<std::mutex> lock(mutex);
        const auto waitBegin = std::chrono::steady_clock::now();
        queueSpace.wait(lock, [this] { return stopRequested || workQueue.size() < o.queueMaxFrames; });
        if (stopRequested) return;
        queueWaitMs += std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - waitBegin).count();
        callbackCopyMs += copyMs;
        ++sourceAuCount;
        workQueue.push_back(std::move(item));
        maxQueueDepth = (std::max)(maxQueueDepth, workQueue.size());
        workChanged.notify_one();
    }

    bool waitForSource(SourceStatus& value)
    {
        std::unique_lock<std::mutex> lock(mutex);
        const bool ok = changed.wait_for(lock, std::chrono::seconds(o.timeoutSec), [this] {
            return source.received && !source.topic.empty();
        });
        value = source;
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

    void stopWorker()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopRequested) return;
            stopRequested = true;
            workChanged.notify_all();
            queueSpace.notify_all();
        }
        if (worker.joinable()) worker.join();
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
            << " decodeFps=" << (decodeMsTotal > 0.0 ? decodedFrames * 1000.0 / decodeMsTotal : 0.0)
            << " decodeMsAvg=" << (sourceAuCount ? decodeMsTotal / sourceAuCount : 0.0)
            << " decodeMsMax=" << decodeMsMax
            << " callbackCopyMsAvg=" << (sourceAuCount ? callbackCopyMs / sourceAuCount : 0.0)
            << " queueWaitMs=" << queueWaitMs << " maxQueueDepth=" << maxQueueDepth
            << " decodedWidth=" << decodedWidth << " decodedHeight=" << decodedHeight
            << " decodeErrors=" << decodeErrors << " writerErrors=" << writerErrors
            << " ddsErrors=" << ddsErrors << " dropped=0" << std::endl;
        return complete && sourceAuCount == decodedFrames && decodedFrames == rawPublished &&
            decodeErrors == 0 && writerErrors == 0 && ddsErrors == 0 ? 0 : 8;
    }

    DataWriter* rawWriter() const { return rawWriterBase; }
    DataWriter* statusWriterBasePtr() const { return statusWriterBase; }
    std::string decodedTopic() const { std::lock_guard<std::mutex> lock(mutex); return outputTopic; }

private:
    enum class WorkKind { None, Start, AccessUnit, Stop };
    struct WorkItem { WorkKind kind = WorkKind::None; std::vector<std::uint8_t> payload; };

    void enqueueControl(WorkKind kind)
    {
        std::unique_lock<std::mutex> lock(mutex);
        queueSpace.wait(lock, [this] { return stopRequested || workQueue.size() < o.queueMaxFrames; });
        if (stopRequested) return;
        WorkItem item; item.kind = kind;
        workQueue.push_back(std::move(item));
        maxQueueDepth = (std::max)(maxQueueDepth, workQueue.size());
        workChanged.notify_one();
    }

    void workerLoop()
    {
        for (;;)
        {
            WorkItem item;
            {
                std::unique_lock<std::mutex> lock(mutex);
                workChanged.wait(lock, [this] {
                    return stopRequested || (outputReady && !workQueue.empty());
                });
                if (stopRequested && workQueue.empty()) return;
                if (!outputReady || workQueue.empty()) continue;
                item = std::move(workQueue.front());
                workQueue.pop_front();
                queueSpace.notify_all();
            }
            if (item.kind == WorkKind::Start) processStart();
            else if (item.kind == WorkKind::AccessUnit) processAccessUnit(item.payload);
            else if (item.kind == WorkKind::Stop) processStop();
        }
    }

    void processStart()
    {
        if (decoderFlushed)
        {
            decoder.shutdown();
            std::string error;
            if (!decoder.initialize(error))
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++decodeErrors; changed.notify_all(); return;
            }
            decoderFlushed = false;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            decodedRunning = true; stopped = false;
        }
        publishStatus(true);
    }

    void processAccessUnit(const std::vector<std::uint8_t>& payload)
    {
        const auto begin = std::chrono::steady_clock::now();
        std::vector<MppDecodedGrayFrame> frames;
        std::string error;
        const bool ok = decoder.pushAccessUnit(payload.data(), payload.size(), frames, error);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        {
            std::lock_guard<std::mutex> lock(mutex);
            decodeMsTotal += elapsed;
            decodeMsMax = (std::max)(decodeMsMax, elapsed);
            if (!ok) ++decodeErrors;
        }
        if (!ok) std::cerr << "[Gateway][ERROR] decode=" << error << std::endl;
        publishFrames(frames);
    }

    void processStop()
    {
        std::vector<MppDecodedGrayFrame> frames;
        std::string error;
        if (!decoder.flush(frames, error))
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++decodeErrors;
        }
        publishFrames(frames);
        drainRaw();
        publishStatus(false);
        {
            std::lock_guard<std::mutex> lock(mutex);
            decodedRunning = false; decoderFlushed = true; stopped = true;
            changed.notify_all();
        }
    }

    void publishFrames(const std::vector<MppDecodedGrayFrame>& frames)
    {
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            const MppDecodedGrayFrame& frame = frames[i];
            SourceStatus snapshot;
            {
                std::lock_guard<std::mutex> lock(mutex);
                snapshot = source;
                decodedWidth = frame.width; decodedHeight = frame.height;
                if (snapshot.width > 0 &&
                    (frame.width != snapshot.width || frame.height != snapshot.height))
                { ++decodeErrors; continue; }
                ++decodedFrames;
            }
            const ReturnCode_t result = DDSIF::BytesWrite(o.domain,
                const_cast<char*>(outputTopic.c_str()),
                reinterpret_cast<const char*>(frame.gray8.data()),
                static_cast<DDS_Long>(frame.gray8.size()));
            std::lock_guard<std::mutex> lock(mutex);
            if (result != RETCODE_OK) ++writerErrors;
            else { ++rawPublished; rawBytes += frame.gray8.size(); }
            changed.notify_all();
        }
    }

    void publishStatus(bool running)
    {
        SourceStatus snapshot;
        int width = 0, height = 0;
        std::string topic;
        {
            std::lock_guard<std::mutex> lock(mutex);
            snapshot = source; width = decodedWidth; height = decodedHeight; topic = outputTopic;
        }
        HwaSimIRDds::VideoStatusV1 sample;
        if (!HwaSimIRDds::VideoStatusV1Initialize(&sample)) return;
        sample.platID = snapshot.platID; sample.sensorID = snapshot.sensorID;
        CopyBounded(sample.channel, 17, snapshot.channel);
        sample.running = running;
        CopyBounded(sample.codec, 25, "raw_gray8");
        CopyBounded(sample.pixelFormat, 25, "gray8");
        CopyBounded(sample.videoTopic, 129, topic);
        const int statusWidth = width > 0 ? width : snapshot.width;
        const int statusHeight = height > 0 ? height : snapshot.height;
        sample.width = statusWidth;
        sample.height = statusHeight;
        sample.fps = snapshot.fps; sample.bitrateKbps = 0; sample.gopFrames = 0;
        sample.compressed = false; sample.currentRound = snapshot.currentRound;
        if (statusWriter->write(sample, HANDLE_NIL_NATIVE) != RETCODE_OK)
        { std::lock_guard<std::mutex> lock(mutex); ++writerErrors; }
        HwaSimIRDds::VideoStatusV1Finalize(&sample);
        std::cout << "[GatewayStatus] running=" << (running ? 1 : 0)
                  << " topic=" << topic << " width=" << statusWidth
                  << " height=" << statusHeight << " round=" << snapshot.currentRound << std::endl;
    }

    void drainRaw()
    {
        Duration_t timeout; timeout.sec = o.ackTimeoutSec; timeout.nanosec = 0;
        if (rawWriterBase && rawWriterBase->wait_for_acknowledgments(timeout) != RETCODE_OK)
        { std::lock_guard<std::mutex> lock(mutex); ++writerErrors; }
        if (o.shutdownDrainMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(o.shutdownDrainMs));
    }

    const Options& o;
    mutable std::mutex mutex;
    std::condition_variable changed, workChanged, queueSpace;
    std::deque<WorkItem> workQueue;
    std::thread worker;
    SourceStatus source;
    MppH264GrayDecoder decoder;
    DataWriter* rawWriterBase = nullptr;
    DataWriter* statusWriterBase = nullptr;
    HwaSimIRDds::VideoStatusV1DataWriter* statusWriter = nullptr;
    std::string outputTopic;
    bool outputReady = false, stopRequested = false;
    bool decodedRunning = false, decoderFlushed = false, stopped = false;
    std::uint64_t statusReceived = 0, sourceAuCount = 0, decodedFrames = 0;
    std::uint64_t rawPublished = 0, rawBytes = 0;
    std::uint64_t decodeErrors = 0, writerErrors = 0, ddsErrors = 0;
    std::size_t maxQueueDepth = 0;
    int decodedWidth = 0, decodedHeight = 0;
    double decodeMsTotal = 0.0, decodeMsMax = 0.0;
    double callbackCopyMs = 0.0, queueWaitMs = 0.0;
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
        SourceStatus source;
        if (!state.waitForSource(source)) throw std::runtime_error("source H264 status timeout");
        const std::string sourceTopic = o.sourceTopicOverride.empty()
            ? source.topic : o.sourceTopicOverride;
        if (sourceTopic.empty()) throw std::runtime_error("source VideoStatus topic is empty");
        if (!state.configureOutput(participant, source, error)) throw std::runtime_error(error);
        VideoListener videoListener(state);
        DataReader* sourceReader = DDSIF::SubTopic(participant, sourceTopic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &videoListener);
        if (!sourceReader) throw std::runtime_error("source H264 SubTopic failed");
        std::cout << "gatewayReady=1 platID=" << source.platID
                  << " sensorID=" << source.sensorID << " channel=" << source.channel
                  << " sourceTopic=" << sourceTopic
                  << " decodedTopic=" << state.decodedTopic()
                  << " decoder=rkmpp callbackMode=enqueue_only" << std::endl;
        const bool complete = state.waitComplete();
        state.stopWorker();
        const int result = state.summary(complete);
        DDSIF::UnSubTopic(sourceReader);
        DDSIF::UnSubTopic(statusReader);
        if (state.rawWriter()) DDSIF::UnPubTopic(state.rawWriter());
        if (state.statusWriterBasePtr()) DDSIF::UnPubTopic(state.statusWriterBasePtr());
        DDSIF::Finalize();
        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "gateway_error=" << e.what() << std::endl;
        return 1;
    }
}
