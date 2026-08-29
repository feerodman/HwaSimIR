#include "HwaSimIRProtocolEndpoint.h"

#include "CommonDataDdsAdapter.h"
#include "DdsRuntimeManager.h"
#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1DataWriter.h"
#include "HwaSimIRProtocolV1TypeSupport.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
#endif

#if defined(HWASIMIR_HAS_ZRDDS)
namespace {

template <typename Sample, typename Sequence, typename Reader>
class CopyingListener : public DDS::SimpleDataReaderListener<Sample, Sequence, Reader>
{
public:
    explicit CopyingListener(const std::function<void(const Sample&)>& callback)
        : m_callback(callback) {}
    void on_process_sample(DDS::DataReader*, const Sample& sample,
                           const DDS::SampleInfo&) override
    {
        if (m_callback) m_callback(sample);
    }
private:
    std::function<void(const Sample&)> m_callback;
};

template <typename TypeSupport>
DDS::DataReader* Subscribe(DDS::DomainParticipant* participant,
                           const std::string& topic,
                           const std::string& profile,
                           DDS::DataReaderListener* listener)
{
    return DDS::DDSIF::SubTopic(participant, topic.c_str(),
                           TypeSupport::get_instance(), profile.c_str(), listener);
}

template <typename TypeSupport>
DDS::DataWriter* Publish(DDS::DomainParticipant* participant,
                         const std::string& topic,
                         const std::string& profile)
{
    return DDS::DDSIF::PubTopic(participant, topic.c_str(),
                           TypeSupport::get_instance(), profile.c_str(), nullptr);
}

void CopyBounded(char* target, std::size_t capacity, const std::string& value)
{
    if (!target || capacity == 0) return;
    const std::size_t count = (std::min)(capacity - 1, value.size());
    std::memcpy(target, value.data(), count);
    target[count] = '\0';
}

} // namespace
#endif

struct HwaSimIRProtocolEndpoint::Impl
{
    std::shared_ptr<DdsRuntimeManager> runtime;
    DdsProtocolConfig config;
    DdsProtocolCallbacks callbacks;
    bool running = false;
    mutable std::mutex writeMutex;
    DdsFrameAuxStats frameStats;
#if defined(HWASIMIR_HAS_ZRDDS)
    typedef CopyingListener<HwaSimIRDds::ControlCommandV1,
        HwaSimIRDds::ControlCommandV1Seq,
        HwaSimIRDds::ControlCommandV1DataReader> ControlListener;
    typedef CopyingListener<HwaSimIRDds::InitCommandV1,
        HwaSimIRDds::InitCommandV1Seq,
        HwaSimIRDds::InitCommandV1DataReader> InitListener;
    typedef CopyingListener<HwaSimIRDds::RealtimeDataV1,
        HwaSimIRDds::RealtimeDataV1Seq,
        HwaSimIRDds::RealtimeDataV1DataReader> RealtimeListener;

    std::unique_ptr<ControlListener> controlListener;
    std::unique_ptr<InitListener> initListener;
    std::unique_ptr<RealtimeListener> realtimeListener;
    DDS::DataReader* controlReader = nullptr;
    DDS::DataReader* initReader = nullptr;
    DDS::DataReader* realtimeReader = nullptr;
    DDS::DataWriter* ackWriterBase = nullptr;
    HwaSimIRDds::InitAckV1DataWriter* ackWriter = nullptr;
    DDS::DataWriter* statusWriterBase = nullptr;
    HwaSimIRDds::VideoStatusV1DataWriter* statusWriter = nullptr;
    DDS::DataWriter* metaWriterBase = nullptr;
    HwaSimIRDds::VideoFrameMetaV1DataWriter* metaWriter = nullptr;
    DDS::DataWriter* annotationWriterBase = nullptr;
    HwaSimIRDds::AnnotationFrameV1DataWriter* annotationWriter = nullptr;
#endif
};

HwaSimIRProtocolEndpoint::HwaSimIRProtocolEndpoint() : m_impl(new Impl()) {}
HwaSimIRProtocolEndpoint::~HwaSimIRProtocolEndpoint() { shutdown(); }

bool HwaSimIRProtocolEndpoint::start(
    const std::shared_ptr<DdsRuntimeManager>& runtime,
    const DdsProtocolConfig& config,
    const DdsProtocolCallbacks& callbacks,
    std::string& error)
{
    if (m_impl->running) return true;
    if (!config.enabled) return true;
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)runtime; (void)callbacks;
    error = "DdsProtocol.Enable=true but binary lacks HWASIMIR_HAS_ZRDDS";
    return false;
#else
    if (!runtime || !runtime->running() || !runtime->participant())
    {
        error = "DDS Runtime is not ready";
        return false;
    }
    m_impl->runtime = runtime;
    m_impl->config = config;
    m_impl->callbacks = callbacks;

    m_impl->controlListener.reset(new Impl::ControlListener(
        [this](const HwaSimIRDds::ControlCommandV1& sample) {
            if (m_impl->callbacks.control)
                m_impl->callbacks.control(HwaSimIRDdsAdapter::FromDds(sample));
        }));
    m_impl->initListener.reset(new Impl::InitListener(
        [this](const HwaSimIRDds::InitCommandV1& sample) {
            if (m_impl->callbacks.init)
                m_impl->callbacks.init(HwaSimIRDdsAdapter::FromDds(sample));
        }));
    m_impl->realtimeListener.reset(new Impl::RealtimeListener(
        [this](const HwaSimIRDds::RealtimeDataV1& sample) {
            if (m_impl->callbacks.realtime)
                m_impl->callbacks.realtime(HwaSimIRDdsAdapter::FromDds(sample));
        }));

    DDS::DomainParticipant* participant = runtime->participant();
    m_impl->controlReader = Subscribe<HwaSimIRDds::ControlCommandV1TypeSupport>(
        participant, config.topicControl, config.readerProfile, m_impl->controlListener.get());
    m_impl->initReader = Subscribe<HwaSimIRDds::InitCommandV1TypeSupport>(
        participant, config.topicInit, config.readerProfile, m_impl->initListener.get());
    m_impl->realtimeReader = Subscribe<HwaSimIRDds::RealtimeDataV1TypeSupport>(
        participant, config.topicRealtime, config.readerProfile, m_impl->realtimeListener.get());
    m_impl->ackWriterBase = Publish<HwaSimIRDds::InitAckV1TypeSupport>(
        participant, config.topicInitAck, config.writerProfile);
    m_impl->statusWriterBase = Publish<HwaSimIRDds::VideoStatusV1TypeSupport>(
        participant, config.topicVideoStatus, config.statusWriterProfile);
    m_impl->metaWriterBase = Publish<HwaSimIRDds::VideoFrameMetaV1TypeSupport>(
        participant, config.topicVideoMeta, config.writerProfile);
    m_impl->annotationWriterBase = Publish<HwaSimIRDds::AnnotationFrameV1TypeSupport>(
        participant, config.topicAnnotation, config.writerProfile);
    m_impl->ackWriter = dynamic_cast<HwaSimIRDds::InitAckV1DataWriter*>(m_impl->ackWriterBase);
    m_impl->statusWriter = dynamic_cast<HwaSimIRDds::VideoStatusV1DataWriter*>(m_impl->statusWriterBase);
    m_impl->metaWriter = dynamic_cast<HwaSimIRDds::VideoFrameMetaV1DataWriter*>(m_impl->metaWriterBase);
    m_impl->annotationWriter = dynamic_cast<HwaSimIRDds::AnnotationFrameV1DataWriter*>(m_impl->annotationWriterBase);
    if (!m_impl->controlReader || !m_impl->initReader || !m_impl->realtimeReader ||
        !m_impl->ackWriter || !m_impl->statusWriter || !m_impl->metaWriter ||
        !m_impl->annotationWriter)
    {
        error = "DDS protocol reader/writer creation failed";
        shutdown();
        return false;
    }
    m_impl->running = true;
    std::cout << "[DdsProtocol] ready=1 runtimeInitCount=" << runtime->initCount()
              << " control=" << config.topicControl
              << " init=" << config.topicInit
              << " realtime=" << config.topicRealtime
              << " ack=" << config.topicInitAck
              << " status=" << config.topicVideoStatus
              << " meta=" << config.topicVideoMeta
              << " annotation=" << config.topicAnnotation << std::endl;
    return true;
#endif
}

bool HwaSimIRProtocolEndpoint::publishVideoFrameMeta(
    const DdsVideoFrameMeta& meta, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)meta; error = "DDS unavailable"; return false;
#else
    if (!m_impl->running || !m_impl->metaWriter) { error = "VideoMeta writer is not ready"; return false; }
    HwaSimIRDds::VideoFrameMetaV1 sample;
    if (!HwaSimIRDds::VideoFrameMetaV1Initialize(&sample))
    {
        error = "VideoFrameMetaV1Initialize failed";
        return false;
    }
    sample.platID = meta.platID;
    sample.sensorID = meta.sensorID;
    CopyBounded(sample.channel, 17, meta.channel);
    sample.frameSeq = meta.frameSeq;
    sample.currentRound = meta.currentRound;
    sample.ptsMs = meta.ptsMs;
    sample.keyFrame = meta.keyFrame;
    CopyBounded(sample.codec, 25, meta.codec);
    sample.width = meta.width;
    sample.height = meta.height;
    const auto begin = std::chrono::steady_clock::now();
    DDS::ReturnCode_t result;
    {
        std::lock_guard<std::mutex> lock(m_impl->writeMutex);
        result = m_impl->metaWriter->write(sample, DDS::HANDLE_NIL_NATIVE);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        if (result == DDS::RETCODE_OK)
        {
            ++m_impl->frameStats.metaCount;
            m_impl->frameStats.metaWriteMsTotal += elapsed;
            m_impl->frameStats.metaWriteMsMax = (std::max)(m_impl->frameStats.metaWriteMsMax, elapsed);
        }
        else ++m_impl->frameStats.writeErrors;
    }
    HwaSimIRDds::VideoFrameMetaV1Finalize(&sample);
    if (result != DDS::RETCODE_OK)
    {
        error = "VideoMeta write failed code=" + std::to_string(static_cast<int>(result));
        return false;
    }
    return true;
#endif
}

bool HwaSimIRProtocolEndpoint::publishAnnotationFrame(
    const DdsAnnotationFrame& annotation, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)annotation; error = "DDS unavailable"; return false;
#else
    if (!m_impl->running || !m_impl->annotationWriter) { error = "Annotation writer is not ready"; return false; }
    if (annotation.json.size() > 32768u)
    {
        error = "Annotation JSON exceeds IDL bound 32768";
        std::lock_guard<std::mutex> lock(m_impl->writeMutex);
        ++m_impl->frameStats.writeErrors;
        return false;
    }
    HwaSimIRDds::AnnotationFrameV1 sample;
    if (!HwaSimIRDds::AnnotationFrameV1Initialize(&sample))
    {
        error = "AnnotationFrameV1Initialize failed";
        return false;
    }
    sample.platID = annotation.platID;
    sample.sensorID = annotation.sensorID;
    CopyBounded(sample.channel, 17, annotation.channel);
    sample.frameSeq = annotation.frameSeq;
    sample.currentRound = annotation.currentRound;
    sample.ptsMs = annotation.ptsMs;
    CopyBounded(sample.json, 32769, annotation.json);
    const auto begin = std::chrono::steady_clock::now();
    DDS::ReturnCode_t result;
    {
        std::lock_guard<std::mutex> lock(m_impl->writeMutex);
        result = m_impl->annotationWriter->write(sample, DDS::HANDLE_NIL_NATIVE);
        const double elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - begin).count();
        if (result == DDS::RETCODE_OK)
        {
            ++m_impl->frameStats.annotationCount;
            m_impl->frameStats.annotationWriteMsTotal += elapsed;
            m_impl->frameStats.annotationWriteMsMax = (std::max)(m_impl->frameStats.annotationWriteMsMax, elapsed);
        }
        else ++m_impl->frameStats.writeErrors;
    }
    HwaSimIRDds::AnnotationFrameV1Finalize(&sample);
    if (result != DDS::RETCODE_OK)
    {
        error = "Annotation write failed code=" + std::to_string(static_cast<int>(result));
        return false;
    }
    return true;
#endif
}

bool HwaSimIRProtocolEndpoint::drainFrameOutputs(
    int ackTimeoutSec, int boundedDrainMs, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)ackTimeoutSec; (void)boundedDrainMs; error = "DDS unavailable"; return false;
#else
    DDS::Duration_t timeout;
    timeout.sec = (std::max)(1, ackTimeoutSec);
    timeout.nanosec = 0;
    DDS::ReturnCode_t metaResult = DDS::RETCODE_OK;
    DDS::ReturnCode_t annotationResult = DDS::RETCODE_OK;
    {
        std::lock_guard<std::mutex> lock(m_impl->writeMutex);
        if (m_impl->metaWriterBase)
            metaResult = m_impl->metaWriterBase->wait_for_acknowledgments(timeout);
        if (m_impl->annotationWriterBase)
            annotationResult = m_impl->annotationWriterBase->wait_for_acknowledgments(timeout);
    }
    if (boundedDrainMs > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(boundedDrainMs));
    if (metaResult != DDS::RETCODE_OK || annotationResult != DDS::RETCODE_OK)
    {
        error = "VideoMeta/Annotation wait_for_acknowledgments failed meta=" +
            std::to_string(static_cast<int>(metaResult)) + " annotation=" +
            std::to_string(static_cast<int>(annotationResult));
        return false;
    }
    return true;
#endif
}

void HwaSimIRProtocolEndpoint::resetFrameStats()
{
    std::lock_guard<std::mutex> lock(m_impl->writeMutex);
    m_impl->frameStats = DdsFrameAuxStats();
}

DdsFrameAuxStats HwaSimIRProtocolEndpoint::frameStats() const
{
    std::lock_guard<std::mutex> lock(m_impl->writeMutex);
    return m_impl->frameStats;
}

bool HwaSimIRProtocolEndpoint::publishInitAck(
    const BYHWICD::InitAckC2pObjectTrackingCmd& ack, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)ack; error = "DDS unavailable"; return false;
#else
    if (!m_impl->running || !m_impl->ackWriter) { error = "InitAck writer is not ready"; return false; }
    HwaSimIRDds::InitAckV1 sample = HwaSimIRDdsAdapter::ToDds(ack);
    std::lock_guard<std::mutex> lock(m_impl->writeMutex);
    const DDS::ReturnCode_t result = m_impl->ackWriter->write(sample, DDS::HANDLE_NIL_NATIVE);
    if (result != DDS::RETCODE_OK)
    {
        error = "InitAck write failed code=" + std::to_string(static_cast<int>(result));
        return false;
    }
    std::cout << "[DdsProtocol] type=init_ack published=1 platID=" << ack.platID
              << " sensorID=" << ack.sensorID << std::endl;
    return true;
#endif
}

bool HwaSimIRProtocolEndpoint::publishVideoStatus(
    const DdsVideoStatus& status, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)status; error = "DDS unavailable"; return false;
#else
    if (!m_impl->running || !m_impl->statusWriter) { error = "VideoStatus writer is not ready"; return false; }
    HwaSimIRDds::VideoStatusV1 sample;
    if (!HwaSimIRDds::VideoStatusV1Initialize(&sample))
    {
        error = "VideoStatusV1Initialize failed";
        return false;
    }
    sample.platID = status.platID;
    sample.sensorID = status.sensorID;
    CopyBounded(sample.channel, 17, status.channel);
    sample.running = status.running;
    CopyBounded(sample.codec, 25, status.codec);
    CopyBounded(sample.pixelFormat, 25, status.pixelFormat);
    CopyBounded(sample.videoTopic, 129, status.videoTopic);
    sample.width = status.width;
    sample.height = status.height;
    sample.fps = status.fps;
    sample.bitrateKbps = status.bitrateKbps;
    sample.gopFrames = status.gopFrames;
    sample.compressed = status.compressed;
    sample.currentRound = status.currentRound;
    DDS::ReturnCode_t result;
    {
        std::lock_guard<std::mutex> lock(m_impl->writeMutex);
        result = m_impl->statusWriter->write(sample, DDS::HANDLE_NIL_NATIVE);
    }
    HwaSimIRDds::VideoStatusV1Finalize(&sample);
    if (result != DDS::RETCODE_OK)
    {
        error = "VideoStatus write failed code=" + std::to_string(static_cast<int>(result));
        return false;
    }
    std::cout << "[VideoStatus] published=1 running=" << (status.running ? 1 : 0)
              << " codec=" << status.codec << " pixelFormat=" << status.pixelFormat
              << " topic=" << status.videoTopic << " width=" << status.width
              << " height=" << status.height << " fps=" << status.fps
              << " currentRound=" << status.currentRound << std::endl;
    return true;
#endif
}

void HwaSimIRProtocolEndpoint::shutdown()
{
#if defined(HWASIMIR_HAS_ZRDDS)
    if (m_impl->controlReader) DDS::DDSIF::UnSubTopic(m_impl->controlReader);
    if (m_impl->initReader) DDS::DDSIF::UnSubTopic(m_impl->initReader);
    if (m_impl->realtimeReader) DDS::DDSIF::UnSubTopic(m_impl->realtimeReader);
    if (m_impl->ackWriterBase) DDS::DDSIF::UnPubTopic(m_impl->ackWriterBase);
    if (m_impl->statusWriterBase) DDS::DDSIF::UnPubTopic(m_impl->statusWriterBase);
    if (m_impl->metaWriterBase) DDS::DDSIF::UnPubTopic(m_impl->metaWriterBase);
    if (m_impl->annotationWriterBase) DDS::DDSIF::UnPubTopic(m_impl->annotationWriterBase);
    m_impl->controlReader = nullptr;
    m_impl->initReader = nullptr;
    m_impl->realtimeReader = nullptr;
    m_impl->ackWriterBase = nullptr;
    m_impl->ackWriter = nullptr;
    m_impl->statusWriterBase = nullptr;
    m_impl->statusWriter = nullptr;
    m_impl->metaWriterBase = nullptr;
    m_impl->metaWriter = nullptr;
    m_impl->annotationWriterBase = nullptr;
    m_impl->annotationWriter = nullptr;
    m_impl->controlListener.reset();
    m_impl->initListener.reset();
    m_impl->realtimeListener.reset();
#endif
    m_impl->running = false;
    m_impl->runtime.reset();
}

bool HwaSimIRProtocolEndpoint::running() const { return m_impl->running; }
