#include "DdsStimClient.h"

#include "CommonDataDdsAdapter.h"
#include "DdsRuntimeManager.h"
#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1DataWriter.h"
#include "HwaSimIRProtocolV1TypeSupport.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <vector>

#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
#endif

#if defined(HWASIMIR_HAS_ZRDDS)
class StimAckListener : public DDS::SimpleDataReaderListener<
    HwaSimIRDds::InitAckV1, HwaSimIRDds::InitAckV1Seq,
    HwaSimIRDds::InitAckV1DataReader>
{
public:
    explicit StimAckListener(std::function<void(const HwaSimIRDds::InitAckV1&)> callback)
        : m_callback(callback) {}
    void on_process_sample(DDS::DataReader*, const HwaSimIRDds::InitAckV1& sample,
                           const DDS::SampleInfo&) override
    {
        m_callback(sample);
    }
private:
    std::function<void(const HwaSimIRDds::InitAckV1&)> m_callback;
};
#endif

struct DdsStimClient::Impl
{
    std::shared_ptr<DdsRuntimeManager> runtime;
    mutable std::mutex mutex;
    std::condition_variable ackReady;
    BYHWICD::InitAckC2pObjectTrackingCmd lastAck = {};
    unsigned long long ackCount = 0;
    unsigned long long ackConsumedCount = 0;
    std::vector<BYHWICD::InitAckC2pObjectTrackingCmd> acknowledgments;
    std::function<void(const BYHWICD::InitAckC2pObjectTrackingCmd&)> ackCallback;
    bool observeStopStatus = false, stopPending = false, stopObserved = false, runningObserved = false;
    int statusPlatID = -1, statusSensorID = -1, statusRound = -1;
    std::string statusChannel;
#if defined(HWASIMIR_HAS_ZRDDS)
    class StatusListener : public DDS::SimpleDataReaderListener<HwaSimIRDds::VideoStatusV1,
        HwaSimIRDds::VideoStatusV1Seq, HwaSimIRDds::VideoStatusV1DataReader>
    {
    public:
        explicit StatusListener(Impl* owner) : impl(owner) {}
        void on_process_sample(DDS::DataReader*, const HwaSimIRDds::VideoStatusV1& sample,
                               const DDS::SampleInfo&) override
        {
            std::lock_guard<std::mutex> lock(impl->mutex);
            if(sample.platID != impl->statusPlatID || sample.sensorID != impl->statusSensorID ||
               sample.currentRound != impl->statusRound ||
               (!impl->statusChannel.empty() && impl->statusChannel != (sample.channel ? sample.channel : ""))) return;
            if(sample.running) impl->runningObserved = true;
            if(impl->stopPending && impl->runningObserved && !sample.running) {
                impl->stopObserved = true;
                impl->ackReady.notify_all();
            }
        }
    private:
        Impl* impl;
    };
    std::unique_ptr<StatusListener> statusListener;
    DDS::DataReader* statusReader = nullptr;
    std::unique_ptr<StimAckListener> ackListener;
    DDS::DataWriter* controlBase = nullptr;
    DDS::DataWriter* initBase = nullptr;
    DDS::DataWriter* realtimeBase = nullptr;
    DDS::DataReader* ackReader = nullptr;
    HwaSimIRDds::ControlCommandV1DataWriter* control = nullptr;
    HwaSimIRDds::InitCommandV1DataWriter* init = nullptr;
    HwaSimIRDds::RealtimeDataV1DataWriter* realtime = nullptr;
#endif
};

DdsStimClient::DdsStimClient() : m_impl(new Impl()) {}
DdsStimClient::~DdsStimClient() { shutdown(); }

bool DdsStimClient::start(const DdsStimConfig& config, std::string& error)
{
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)config; error = "DDS stim requested but binary lacks HWASIMIR_HAS_ZRDDS"; return false;
#else
    if (m_impl->runtime && m_impl->runtime->running()) return true;
    DdsRuntimeConfig runtimeConfig;
    runtimeConfig.domainId = config.domainId;
    runtimeConfig.qosFile = config.qosFile;
    m_impl->runtime.reset(new DdsRuntimeManager());
    if (!m_impl->runtime->start(runtimeConfig, error)) return false;
    DDS::DomainParticipant* participant = m_impl->runtime->participant();
    m_impl->ackListener.reset(new StimAckListener([this](const HwaSimIRDds::InitAckV1& sample) {
        const BYHWICD::InitAckC2pObjectTrackingCmd ack = HwaSimIRDdsAdapter::FromDds(sample);
        std::function<void(const BYHWICD::InitAckC2pObjectTrackingCmd&)> callback;
        {
            std::lock_guard<std::mutex> lock(m_impl->mutex);
            m_impl->lastAck = ack;
            m_impl->acknowledgments.push_back(ack);
            ++m_impl->ackCount;
            callback = m_impl->ackCallback;
            m_impl->ackReady.notify_all();
        }
        if (callback) callback(ack);
    }));
    m_impl->controlBase = DDS::DDSIF::PubTopic(participant, config.topicControl.c_str(),
        HwaSimIRDds::ControlCommandV1TypeSupport::get_instance(), config.writerProfile.c_str(), nullptr);
    m_impl->initBase = DDS::DDSIF::PubTopic(participant, config.topicInit.c_str(),
        HwaSimIRDds::InitCommandV1TypeSupport::get_instance(), config.writerProfile.c_str(), nullptr);
    m_impl->realtimeBase = DDS::DDSIF::PubTopic(participant, config.topicRealtime.c_str(),
        HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance(), config.writerProfile.c_str(), nullptr);
    m_impl->ackReader = DDS::DDSIF::SubTopic(participant, config.topicInitAck.c_str(),
        HwaSimIRDds::InitAckV1TypeSupport::get_instance(), config.readerProfile.c_str(), m_impl->ackListener.get());
    m_impl->observeStopStatus = config.observeStopStatus;
    m_impl->statusChannel = config.channel;
    if(config.observeStopStatus) {
        m_impl->statusListener.reset(new Impl::StatusListener(m_impl.get()));
        m_impl->statusReader = DDS::DDSIF::SubTopic(participant, config.topicVideoStatus.c_str(),
            HwaSimIRDds::VideoStatusV1TypeSupport::get_instance(), config.statusReaderProfile.c_str(), m_impl->statusListener.get());
    }
    m_impl->control = dynamic_cast<HwaSimIRDds::ControlCommandV1DataWriter*>(m_impl->controlBase);
    m_impl->init = dynamic_cast<HwaSimIRDds::InitCommandV1DataWriter*>(m_impl->initBase);
    m_impl->realtime = dynamic_cast<HwaSimIRDds::RealtimeDataV1DataWriter*>(m_impl->realtimeBase);
    if (!m_impl->control || !m_impl->init || !m_impl->realtime || !m_impl->ackReader ||
        (config.observeStopStatus && !m_impl->statusReader))
    {
        error = "DDS stim writer/reader creation failed";
        shutdown();
        return false;
    }
    std::cout << "[DdsStim] ready=1 runtimeInitCount=" << runtimeInitCount()
              << " domain=" << config.domainId << std::endl;
    return true;
#endif
}

void DdsStimClient::shutdown()
{
#if defined(HWASIMIR_HAS_ZRDDS)
    if (m_impl->controlBase) DDS::DDSIF::UnPubTopic(m_impl->controlBase);
    if (m_impl->initBase) DDS::DDSIF::UnPubTopic(m_impl->initBase);
    if (m_impl->realtimeBase) DDS::DDSIF::UnPubTopic(m_impl->realtimeBase);
    if (m_impl->ackReader) DDS::DDSIF::UnSubTopic(m_impl->ackReader);
    if (m_impl->statusReader) DDS::DDSIF::UnSubTopic(m_impl->statusReader);
    m_impl->statusReader = nullptr;
    m_impl->statusListener.reset();
    m_impl->controlBase = m_impl->initBase = m_impl->realtimeBase = nullptr;
    m_impl->ackReader = nullptr;
    m_impl->control = nullptr; m_impl->init = nullptr; m_impl->realtime = nullptr;
    m_impl->ackListener.reset();
#endif
    if (m_impl->runtime) m_impl->runtime->shutdown();
    m_impl->runtime.reset();
}

#if defined(HWASIMIR_HAS_ZRDDS)
template <typename Writer, typename Sample>
static bool WriteTyped(Writer* writer, const Sample& sample, const char* name, std::string& error)
{
    if (!writer) { error = std::string(name) + " writer not ready"; return false; }
    const DDS::ReturnCode_t rc = writer->write(sample, DDS::HANDLE_NIL_NATIVE);
    if (rc != DDS::RETCODE_OK)
    {
        error = std::string(name) + " write failed code=" + std::to_string(static_cast<int>(rc));
        return false;
    }
    return true;
}
#endif

bool DdsStimClient::sendControl(const BYHWICD::ControlP2cX1ObjTrackingCmd& value, std::string& error)
{
#if defined(HWASIMIR_HAS_ZRDDS)
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->statusPlatID = value.platID;
        m_impl->statusRound = value.currentRound;
        m_impl->stopPending = value.simCommand == 3;
        m_impl->stopObserved = false;
        if(value.simCommand == 1 || value.simCommand == 2) m_impl->runningObserved = false;
    }
    return WriteTyped(m_impl->control, HwaSimIRDdsAdapter::ToDds(value), "Control", error);
#else
    (void)value; error = "DDS unavailable"; return false;
#endif
}
bool DdsStimClient::sendInit(const BYHWICD::InitP2cObjectTrackingCmd& value, std::string& error)
{
#if defined(HWASIMIR_HAS_ZRDDS)
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->statusPlatID = value.platID;
        m_impl->statusSensorID = value.sensorID;
    }
    return WriteTyped(m_impl->init, HwaSimIRDdsAdapter::ToDds(value), "Init", error);
#else
    (void)value; error = "DDS unavailable"; return false;
#endif
}
bool DdsStimClient::sendRealtime(const BYHWICD::DisplayC2cObjTrackingData& value, std::string& error)
{
#if defined(HWASIMIR_HAS_ZRDDS)
    return WriteTyped(m_impl->realtime, HwaSimIRDdsAdapter::ToDds(value), "Realtime", error);
#else
    (void)value; error = "DDS unavailable"; return false;
#endif
}

bool DdsStimClient::waitForInitAck(int timeoutMs, BYHWICD::InitAckC2pObjectTrackingCmd& value)
{
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    const unsigned long long initial = m_impl->ackConsumedCount;
    if (!m_impl->ackReady.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this, initial] { return m_impl->ackCount > initial; })) return false;
    m_impl->ackConsumedCount = m_impl->ackCount;
    value = m_impl->lastAck;
    return true;
}

bool DdsStimClient::waitForInitAcks(int timeoutMs, std::size_t expectedCount,
    std::vector<BYHWICD::InitAckC2pObjectTrackingCmd>& values)
{
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    const unsigned long long initial = m_impl->ackConsumedCount;
    if (!m_impl->ackReady.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this, initial, expectedCount] {
            return m_impl->ackCount >= initial + expectedCount;
        })) return false;
    values.clear();
    const std::size_t begin = static_cast<std::size_t>(initial);
    const std::size_t end = begin + expectedCount;
    if (m_impl->acknowledgments.size() < end) return false;
    values.insert(values.end(), m_impl->acknowledgments.begin() + begin,
        m_impl->acknowledgments.begin() + end);
    m_impl->ackConsumedCount = initial + expectedCount;
    return true;
}

bool DdsStimClient::waitForStopStatus(int timeoutMs, std::string& error)
{
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    if(!m_impl->observeStopStatus || !m_impl->stopPending) return true;
    const bool completed = m_impl->ackReady.wait_for(lock, std::chrono::milliseconds(timeoutMs),
        [this] { return m_impl->stopObserved; });
    std::cout << "[StimStopStatus] observed=" << (completed ? 1 : 0)
              << " platID=" << m_impl->statusPlatID << " sensorID=" << m_impl->statusSensorID
              << " round=" << m_impl->statusRound << " source=existing_video_status" << std::endl;
    if(!completed) error = "renderer did not confirm STOP before application drain timeout";
    return completed;
}

bool DdsStimClient::waitForAcknowledgments(int timeoutMs, std::string& error)
{
#if defined(HWASIMIR_HAS_ZRDDS)
    if (!m_impl->controlBase || !m_impl->initBase || !m_impl->realtimeBase)
    {
        error = "DDS stim writers are not ready";
        return false;
    }
    DDS::Duration_t timeout;
    timeout.sec = timeoutMs / 1000;
    timeout.nanosec = (timeoutMs % 1000) * 1000000;
    const DDS::ReturnCode_t controlRc = m_impl->controlBase->wait_for_acknowledgments(timeout);
    const DDS::ReturnCode_t initRc = m_impl->initBase->wait_for_acknowledgments(timeout);
    const DDS::ReturnCode_t realtimeRc = m_impl->realtimeBase->wait_for_acknowledgments(timeout);
    std::cout << "[StimDrain] ackControl=" << static_cast<int>(controlRc)
              << " ackInit=" << static_cast<int>(initRc)
              << " ackRealtime=" << static_cast<int>(realtimeRc) << std::endl;
    if (controlRc != DDS::RETCODE_OK || initRc != DDS::RETCODE_OK ||
        realtimeRc != DDS::RETCODE_OK)
    {
        error = "wait_for_acknowledgments failed";
        return false;
    }
    return true;
#else
    (void)timeoutMs;
    error = "DDS unavailable";
    return false;
#endif
}
unsigned long long DdsStimClient::ackCount() const
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->ackCount;
}
int DdsStimClient::runtimeInitCount() const
{
    return m_impl->runtime ? m_impl->runtime->initCount() : 0;
}

void DdsStimClient::setAckCallback(
    const std::function<void(const BYHWICD::InitAckC2pObjectTrackingCmd&)>& callback)
{
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->ackCallback = callback;
}
