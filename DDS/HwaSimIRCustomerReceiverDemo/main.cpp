#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "MppH264GrayDecoder.h"
#include "ZRDDSCppSimpleInterface.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DDS;

struct Options
{
    int domain = 150;
    int timeoutSec = 120;
    std::uint64_t frames = 0;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string statusTopic = "HwaSimIR.VideoStatus";
    std::string output = "received.h264";
    std::string channel = "precise";
    std::string expectCodec;
    std::string decode = "none";
    std::string grayOutput;
    bool receiveMeta = false;
    bool receiveAnnotation = false;
    std::string metaOutput = "frame_meta.txt";
    std::string annotationOutput = "annotation.txt";
};

static bool ParseBool(const std::string& value)
{
    return value == "1" || value == "true" || value == "on";
}

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
        else if (arg == "--channel") o.channel = value;
        else if (arg == "--expect-codec") o.expectCodec = value;
        else if (arg == "--decode") o.decode = value;
        else if (arg == "--gray-output") o.grayOutput = value;
        else if (arg == "--receive-meta") o.receiveMeta = ParseBool(value);
        else if (arg == "--receive-annotation") o.receiveAnnotation = ParseBool(value);
        else if (arg == "--meta-output") o.metaOutput = value;
        else if (arg == "--annotation-output") o.annotationOutput = value;
        else throw std::runtime_error("unknown option " + arg);
    }
    if (o.decode != "none" && o.decode != "mpp")
        throw std::runtime_error("--decode must be none or mpp");
    return o;
}

struct StatusSnapshot
{
    bool received = false, running = false, compressed = false;
    int width = 0, height = 0, fps = 0, currentRound = 0;
    int platID = 0, sensorID = 0;
    std::string channel, codec, pixelFormat, topic;
};

class ReceiverState
{
public:
    explicit ReceiverState(const Options& options) : o(options) {}

    bool initializeDecoder(std::string& error)
    {
        return o.decode != "mpp" || decoder.initialize(error);
    }

    bool openOutputs(std::string& error)
    {
        videoFile.open(o.output.c_str(), std::ios::binary | std::ios::trunc);
        if (!videoFile) { error = "cannot open video output"; return false; }
        if (!o.grayOutput.empty())
        {
            grayFile.open(o.grayOutput.c_str(), std::ios::binary | std::ios::trunc);
            if (!grayFile) { error = "cannot open gray output"; return false; }
        }
        if (o.receiveMeta)
        {
            metaFile.open(o.metaOutput.c_str(), std::ios::trunc);
            if (!metaFile) { error = "cannot open meta output"; return false; }
        }
        if (o.receiveAnnotation)
        {
            annotationFile.open(o.annotationOutput.c_str(), std::ios::trunc);
            if (!annotationFile) { error = "cannot open annotation output"; return false; }
        }
        return true;
    }

    void setStatus(const HwaSimIRDds::VideoStatusV1& s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const bool wasRunning = status.running;
        status.received = true;
        status.running = s.running != 0;
        status.compressed = s.compressed != 0;
        status.width = s.width; status.height = s.height; status.fps = s.fps;
        status.currentRound = s.currentRound;
        status.platID = s.platID; status.sensorID = s.sensorID;
        status.channel = s.channel ? s.channel : "";
        status.codec = s.codec ? s.codec : "";
        status.pixelFormat = s.pixelFormat ? s.pixelFormat : "";
        status.topic = s.videoTopic ? s.videoTopic : "";
        ++statusCount;
        if (status.running && !wasRunning && o.decode == "mpp" && decoderFlushed)
        {
            decoder.shutdown();
            std::string error;
            if (!decoder.initialize(error))
            {
                ++decodeErrors;
                std::cerr << "[MppDecode][ERROR] roundReset reason=" << error << std::endl;
            }
            else
            {
                decoderFlushed = false;
            }
        }
        else if (status.running && !wasRunning)
        {
            decoderFlushed = false;
        }
        if (wasRunning && !status.running && o.decode == "mpp" && !decoderFlushed)
            flushDecoderLocked();
        ready.notify_all();
    }

    bool waitForRunningStatus(int timeoutSec)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return ready.wait_for(lock, std::chrono::seconds(timeoutSec), [this] {
            return status.received && status.running && !status.topic.empty();
        });
    }

    StatusSnapshot statusSnapshot()
    {
        std::lock_guard<std::mutex> lock(mutex); return status;
    }

    void onVideo(const Bytes& sample)
    {
        const std::uint64_t size = sample.value.length();
        const std::uint8_t* data = sample.value.get_contiguous_buffer();
        std::lock_guard<std::mutex> lock(mutex);
        if (!started) { started = true; first = std::chrono::steady_clock::now(); }
        last = std::chrono::steady_clock::now();
        ++videoSamples; videoBytes += size;
        videoOrdinals.insert(static_cast<std::uint32_t>(videoSamples));
        const bool raw = status.codec == "raw" || status.codec == "raw_gray8" ||
            status.codec == "raw_bgr24";
        const std::uint64_t channels = status.codec == "raw_bgr24" ||
            status.pixelFormat == "bgr24" ? 3u : 1u;
        const std::uint64_t expected = raw
            ? static_cast<std::uint64_t>(status.width) * status.height * channels : 0;
        if (!data || (expected && size != expected)) ++ddsErrors;
        else
        {
            videoFile.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
            if (!videoFile) ++ddsErrors;
            if (o.decode == "mpp")
            {
                const auto begin = std::chrono::steady_clock::now();
                std::vector<MppDecodedGrayFrame> frames;
                std::string error;
                if (!decoder.pushAccessUnit(data, static_cast<std::size_t>(size), frames, error))
                {
                    ++decodeErrors;
                    std::cerr << "[MppDecode][ERROR] sample=" << videoSamples
                              << " reason=" << error << std::endl;
                }
                consumeDecodedLocked(frames);
                const double elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - begin).count();
                decodeMsTotal += elapsed;
                decodeMsMax = (std::max)(decodeMsMax, elapsed);
            }
        }
        reconcileLocked();
        ready.notify_all();
    }

    void onMeta(const HwaSimIRDds::VideoFrameMetaV1& s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++metaCount;
        if (!metaSeqs.insert(s.frameSeq).second) ++syncMismatch;
        if ((lastMetaSeq != 0 && s.frameSeq != lastMetaSeq + 1) ||
            (metaCount == 1 && s.frameSeq != 1) || s.currentRound != status.currentRound)
            ++syncMismatch;
        lastMetaSeq = s.frameSeq;
        if (metaFile)
            metaFile << "round=" << s.currentRound << " frameSeq=" << s.frameSeq
                     << " ptsMs=" << std::fixed << std::setprecision(3) << s.ptsMs
                     << " keyFrame=" << (s.keyFrame ? 1 : 0)
                     << " codec=" << (s.codec ? s.codec : "")
                     << " width=" << s.width << " height=" << s.height << '\n';
        reconcileLocked();
        ready.notify_all();
    }

    void onAnnotation(const HwaSimIRDds::AnnotationFrameV1& s)
    {
        std::lock_guard<std::mutex> lock(mutex);
        ++annotationCount;
        if (!annotationSeqs.insert(s.frameSeq).second) ++syncMismatch;
        if ((lastAnnotationSeq != 0 && s.frameSeq != lastAnnotationSeq + 1) ||
            (annotationCount == 1 && s.frameSeq != 1) || s.currentRound != status.currentRound)
            ++syncMismatch;
        lastAnnotationSeq = s.frameSeq;
        if (annotationFile)
            annotationFile << "round=" << s.currentRound << " frameSeq=" << s.frameSeq
                           << " ptsMs=" << std::fixed << std::setprecision(3) << s.ptsMs
                           << " json=" << (s.json ? s.json : "") << '\n';
        reconcileLocked();
        ready.notify_all();
    }

    bool waitComplete()
    {
        std::unique_lock<std::mutex> lock(mutex);
        return ready.wait_for(lock, std::chrono::seconds(o.timeoutSec), [this] {
            const bool videoDone = o.frames == 0 ? videoSamples > 0 : videoSamples >= o.frames;
            const bool metaDone = !o.receiveMeta || metaCount >= videoSamples;
            const bool annotationDone = !o.receiveAnnotation || annotationCount >= videoSamples;
            const bool decodeDone = o.decode != "mpp" || decodedFrames >= videoSamples;
            return videoDone && metaDone && annotationDone && decodeDone;
        });
    }

    int summary(bool complete)
    {
        std::lock_guard<std::mutex> lock(mutex);
        videoFile.flush(); grayFile.flush(); metaFile.flush(); annotationFile.flush();
        reconcileLocked();
        const double seconds = started ? std::chrono::duration<double>(last - first).count() : 0.0;
        const double decodeSeconds = decodeMsTotal / 1000.0;
        const bool countsOk = (!o.receiveMeta || metaCount == videoSamples) &&
            (!o.receiveAnnotation || annotationCount == videoSamples) &&
            (o.decode != "mpp" || decodedFrames == videoSamples);
        const bool geometryOk = o.decode != "mpp" ||
            (decodedWidth == status.width && decodedHeight == status.height);
        std::cout << std::fixed << std::setprecision(3)
            << "statusReceived=" << statusCount << " videoSamples=" << videoSamples
            << " videoBytes=" << videoBytes
            << " receiveFps=" << (seconds > 0 ? videoSamples / seconds : 0.0)
            << " decodedFrames=" << decodedFrames
            << " decodeFps=" << (decodeSeconds > 0 ? decodedFrames / decodeSeconds : 0.0)
            << " decodeMsAvg=" << (videoSamples ? decodeMsTotal / videoSamples : 0.0)
            << " decodeMsMax=" << decodeMsMax << " decodeErrors=" << decodeErrors
            << " metaCount=" << metaCount << " annotationCount=" << annotationCount
            << " lastFrameSeq=" << lastMetaSeq
            << " pendingMeta=" << pendingMeta << " pendingAnnotation=" << pendingAnnotation
            << " syncMismatch=" << syncMismatch
            << " codec=" << status.codec << " pixelFormat=" << status.pixelFormat
            << " width=" << status.width << " height=" << status.height
            << " decodedWidth=" << decodedWidth << " decodedHeight=" << decodedHeight
            << " ddsErrors=" << ddsErrors << std::endl;
        return complete && countsOk && geometryOk && ddsErrors == 0 && decodeErrors == 0 &&
            syncMismatch == 0 && pendingMeta == 0 && pendingAnnotation == 0 ? 0 : 8;
    }

private:
    void consumeDecodedLocked(const std::vector<MppDecodedGrayFrame>& frames)
    {
        for (std::size_t i = 0; i < frames.size(); ++i)
        {
            const MppDecodedGrayFrame& frame = frames[i];
            ++decodedFrames;
            decodedWidth = frame.width; decodedHeight = frame.height;
            if (frame.width != status.width || frame.height != status.height) ++decodeErrors;
            if (grayFile)
                grayFile.write(reinterpret_cast<const char*>(frame.gray8.data()),
                    static_cast<std::streamsize>(frame.gray8.size()));
            if (grayFile && !grayFile) ++decodeErrors;
        }
    }

    void flushDecoderLocked()
    {
        std::vector<MppDecodedGrayFrame> frames;
        std::string error;
        if (!decoder.flush(frames, error))
        {
            ++decodeErrors;
            std::cerr << "[MppDecode][ERROR] flush reason=" << error << std::endl;
        }
        consumeDecodedLocked(frames);
        decoderFlushed = true;
    }

    void reconcileLocked()
    {
        pendingMeta = 0;
        pendingAnnotation = 0;
        for (std::set<std::uint32_t>::const_iterator it = videoOrdinals.begin();
             it != videoOrdinals.end(); ++it)
        {
            if (o.receiveMeta && metaSeqs.count(*it) == 0) ++pendingMeta;
            if (o.receiveAnnotation && annotationSeqs.count(*it) == 0) ++pendingAnnotation;
        }
        for (std::set<std::uint32_t>::const_iterator it = metaSeqs.begin(); it != metaSeqs.end(); ++it)
            if (videoOrdinals.count(*it) == 0) ++pendingMeta;
        for (std::set<std::uint32_t>::const_iterator it = annotationSeqs.begin(); it != annotationSeqs.end(); ++it)
            if (videoOrdinals.count(*it) == 0) ++pendingAnnotation;
    }

    const Options& o;
    std::mutex mutex;
    std::condition_variable ready;
    StatusSnapshot status;
    std::ofstream videoFile, grayFile, metaFile, annotationFile;
    MppH264GrayDecoder decoder;
    bool decoderFlushed = false, started = false;
    std::chrono::steady_clock::time_point first, last;
    std::uint64_t statusCount = 0, videoSamples = 0, videoBytes = 0;
    std::uint64_t decodedFrames = 0, decodeErrors = 0, ddsErrors = 0;
    std::uint64_t metaCount = 0, annotationCount = 0, syncMismatch = 0;
    std::uint64_t pendingMeta = 0, pendingAnnotation = 0;
    std::uint32_t lastMetaSeq = 0, lastAnnotationSeq = 0;
    int decodedWidth = 0, decodedHeight = 0;
    double decodeMsTotal = 0.0, decodeMsMax = 0.0;
    std::set<std::uint32_t> videoOrdinals, metaSeqs, annotationSeqs;
};

class StatusListener : public SimpleDataReaderListener<HwaSimIRDds::VideoStatusV1,
    HwaSimIRDds::VideoStatusV1Seq, HwaSimIRDds::VideoStatusV1DataReader>
{
public:
    StatusListener(ReceiverState& state, const Options& options) : m_state(state), m_options(options) {}
    void on_process_sample(DataReader*, const HwaSimIRDds::VideoStatusV1& s, const SampleInfo&) override
    {
        const std::string channel = s.channel ? s.channel : "";
        const std::string codec = s.codec ? s.codec : "";
        if (channel != m_options.channel) return;
        if (!m_options.expectCodec.empty() && codec != m_options.expectCodec) return;
        m_state.setStatus(s);
    }
private:
    ReceiverState& m_state; const Options& m_options;
};

class VideoListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    explicit VideoListener(ReceiverState& state) : m_state(state) {}
    void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) override { m_state.onVideo(sample); }
private: ReceiverState& m_state;
};

class MetaListener : public SimpleDataReaderListener<HwaSimIRDds::VideoFrameMetaV1,
    HwaSimIRDds::VideoFrameMetaV1Seq, HwaSimIRDds::VideoFrameMetaV1DataReader>
{
public:
    explicit MetaListener(ReceiverState& state) : m_state(state) {}
    void on_process_sample(DataReader*, const HwaSimIRDds::VideoFrameMetaV1& sample,
        const SampleInfo&) override { m_state.onMeta(sample); }
private: ReceiverState& m_state;
};

class AnnotationListener : public SimpleDataReaderListener<HwaSimIRDds::AnnotationFrameV1,
    HwaSimIRDds::AnnotationFrameV1Seq, HwaSimIRDds::AnnotationFrameV1DataReader>
{
public:
    explicit AnnotationListener(ReceiverState& state) : m_state(state) {}
    void on_process_sample(DataReader*, const HwaSimIRDds::AnnotationFrameV1& sample,
        const SampleInfo&) override { m_state.onAnnotation(sample); }
private: ReceiverState& m_state;
};

int main(int argc, char** argv)
{
    try
    {
        const Options o = Parse(argc, argv);
        ReceiverState state(o);
        std::string error;
        if (!state.initializeDecoder(error)) throw std::runtime_error(error);
        DomainParticipantFactory* factory = DDSIF::Init(o.qos.c_str(), "hwasimir_factory");
        if (!factory) throw std::runtime_error("DDSIF::Init failed");
        DomainParticipant* participant = DDSIF::CreateDP(o.domain, "hwasimir_tcp");
        if (!participant) throw std::runtime_error("DDSIF::CreateDP failed");
        StatusListener statusListener(state, o);
        DataReader* statusReader = DDSIF::SubTopic(participant, o.statusTopic.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(), "hwasimir_status_reader", &statusListener);
        if (!statusReader || !state.waitForRunningStatus(o.timeoutSec))
            throw std::runtime_error("VideoStatus timeout");
        const StatusSnapshot status = state.statusSnapshot();
        if (o.decode == "mpp" && status.codec != "h264")
            throw std::runtime_error("MPP decode requires H264 VideoStatus");
        if (!state.openOutputs(error)) throw std::runtime_error(error);
        VideoListener videoListener(state);
        MetaListener metaListener(state);
        AnnotationListener annotationListener(state);
        DataReader* metaReader = nullptr;
        DataReader* annotationReader = nullptr;
        if (o.receiveMeta)
        {
            const std::string topic = "HwaSimIR.VideoMeta." + status.channel;
            metaReader = DDSIF::SubTopic(participant, topic.c_str(),
                HwaSimIRDds::VideoFrameMetaV1TypeSupport::get_instance(),
                "hwasimir_protocol_reader", &metaListener);
            if (!metaReader) throw std::runtime_error("VideoMeta SubTopic failed");
        }
        if (o.receiveAnnotation)
        {
            const std::string topic = "HwaSimIR.Annotation." + status.channel;
            annotationReader = DDSIF::SubTopic(participant, topic.c_str(),
                HwaSimIRDds::AnnotationFrameV1TypeSupport::get_instance(),
                "hwasimir_protocol_reader", &annotationListener);
            if (!annotationReader) throw std::runtime_error("Annotation SubTopic failed");
        }
        DataReader* videoReader = DDSIF::SubTopic(participant, status.topic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &videoListener);
        if (!videoReader) throw std::runtime_error("video SubTopic failed");
        std::cout << "receiverReady=1 statusTopic=" << o.statusTopic
                  << " autoVideoTopic=" << status.topic
                  << " decode=" << o.decode << std::endl;
        const bool complete = state.waitComplete();
        const int result = state.summary(complete);
        DDSIF::UnSubTopic(videoReader);
        if (metaReader) DDSIF::UnSubTopic(metaReader);
        if (annotationReader) DDSIF::UnSubTopic(annotationReader);
        DDSIF::UnSubTopic(statusReader);
        DDSIF::Finalize();
        return result;
    }
    catch (const std::exception& e)
    {
        std::cerr << "receiver_error=" << e.what() << " ddsErrors=1" << std::endl;
        return 1;
    }
}
