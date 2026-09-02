#include "HwaSimIRSimpleDdsClient.h"

#include "HwaSimIRProtocolV1DataReader.h"
#include "HwaSimIRProtocolV1DataWriter.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <vector>

namespace {

class InitAckListener : public DDS::SimpleDataReaderListener<
    HwaSimIRDds::InitAckV1,
    HwaSimIRDds::InitAckV1Seq,
    HwaSimIRDds::InitAckV1DataReader>
{
public:
    explicit InitAckListener(
        const std::function<void(const HwaSimIRDds::InitAckV1&)>& callback)
        : callback_(callback)
    {
    }

    void on_process_sample(DDS::DataReader*, const HwaSimIRDds::InitAckV1& sample,
        const DDS::SampleInfo&) override
    {
        callback_(sample);
    }

private:
    std::function<void(const HwaSimIRDds::InitAckV1&)> callback_;
};

template <typename Writer, typename Sample>
bool WriteSample(Writer* writer, const Sample& sample, const char* name)
{
    if (!writer)
    {
        std::cerr << "[DDS][ERROR] " << name << " writer is not ready" << std::endl;
        return false;
    }
    const DDS::ReturnCode_t result = writer->write(sample, DDS::HANDLE_NIL_NATIVE);
    if (result != DDS::RETCODE_OK)
    {
        std::cerr << "[DDS][ERROR] " << name << " write failed code="
                  << static_cast<int>(result) << std::endl;
        return false;
    }
    return true;
}

} // namespace

struct HwaSimIRSimpleDdsClient::Impl
{
    struct AckEntry
    {
        unsigned long long sequence;
        HwaSimIRDds::InitAckV1 sample;
    };

    DDS::DomainParticipantFactory* factory = nullptr;
    DDS::DomainParticipant* participant = nullptr;
    DDS::Publisher* publisher = nullptr;
    DDS::Topic* controlTopic = nullptr;
    DDS::Topic* initTopic = nullptr;
    DDS::Topic* realtimeTopic = nullptr;
    DDS::DataWriter* controlBaseWriter = nullptr;
    DDS::DataWriter* initBaseWriter = nullptr;
    DDS::DataWriter* realtimeBaseWriter = nullptr;
    HwaSimIRDds::ControlCommandV1DataWriter* controlWriter = nullptr;
    HwaSimIRDds::InitCommandV1DataWriter* initWriter = nullptr;
    HwaSimIRDds::RealtimeDataV1DataWriter* realtimeWriter = nullptr;
    DDS::DataReader* ackReader = nullptr;
    std::unique_ptr<InitAckListener> ackListener;

    std::mutex ackMutex;
    std::condition_variable ackChanged;
    std::vector<AckEntry> acknowledgments;
    unsigned long long nextAckSequence = 1;
    unsigned long long waitFromSequence = 1;
    bool running = false;
};

HwaSimIRSimpleDdsClient::HwaSimIRSimpleDdsClient() : impl_(new Impl())
{
}

HwaSimIRSimpleDdsClient::~HwaSimIRSimpleDdsClient()
{
    shutdown();
}

bool HwaSimIRSimpleDdsClient::init(int domainId, const std::string& qosFile)
{
    if (impl_->running)
        return true;

    // 加载 QoS，并创建 tcpv4 Participant。
    impl_->factory = DDS::DDSIF::Init(qosFile.c_str(), "hwasimir_factory");
    if (!impl_->factory)
    {
        std::cerr << "[DDS][ERROR] DDSIF::Init failed qos=" << qosFile << std::endl;
        return false;
    }
    impl_->participant = DDS::DDSIF::CreateDP(domainId, "hwasimir_tcp");
    if (!impl_->participant)
    {
        std::cerr << "[DDS][ERROR] DDSIF::CreateDP failed domain=" << domainId << std::endl;
        DDS::DDSIF::Finalize();
        impl_->factory = nullptr;
        return false;
    }

    // 注册四种控制面类型。
    if (HwaSimIRDds::ControlCommandV1TypeSupport::get_instance()->register_type(
            impl_->participant, nullptr) != DDS::RETCODE_OK ||
        HwaSimIRDds::InitCommandV1TypeSupport::get_instance()->register_type(
            impl_->participant, nullptr) != DDS::RETCODE_OK ||
        HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance()->register_type(
            impl_->participant, nullptr) != DDS::RETCODE_OK ||
        HwaSimIRDds::InitAckV1TypeSupport::get_instance()->register_type(
            impl_->participant, nullptr) != DDS::RETCODE_OK)
    {
        std::cerr << "[DDS][ERROR] register_type failed" << std::endl;
        shutdown();
        return false;
    }

    // Topic 名称与 TypeSupport 绑定业务类型
    impl_->controlTopic = impl_->participant->create_topic(
        "HwaSimIR.Control",
        HwaSimIRDds::ControlCommandV1TypeSupport::get_instance()->get_type_name(),
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    impl_->initTopic = impl_->participant->create_topic(
        "HwaSimIR.Init",
        HwaSimIRDds::InitCommandV1TypeSupport::get_instance()->get_type_name(),
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    impl_->realtimeTopic = impl_->participant->create_topic(
        "HwaSimIR.Realtime",
        HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance()->get_type_name(),
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!impl_->controlTopic || !impl_->initTopic || !impl_->realtimeTopic)
    {
        std::cerr << "[DDS][ERROR] create_topic failed" << std::endl;
        shutdown();
        return false;
    }

    // 一个 Publisher 管理三个 Writer
    impl_->publisher = impl_->participant->create_publisher(
        DDS::PUBLISHER_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!impl_->publisher)
    {
        std::cerr << "[DDS][ERROR] create_publisher failed" << std::endl;
        shutdown();
        return false;
    }

    DDS::DataWriterQos writerQos;
    if (impl_->publisher->get_default_datawriter_qos(writerQos) != DDS::RETCODE_OK)
    {
        std::cerr << "[DDS][ERROR] get_default_datawriter_qos failed" << std::endl;
        shutdown();
        return false;
    }
    // 可靠传输、KEEP_ALL；允许一个 Writer 写多个 keyed sensor instance
    writerQos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    writerQos.reliability.max_blocking_time.sec = 60;
    writerQos.reliability.max_blocking_time.nanosec = 0;
    writerQos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    writerQos.resource_limits.max_samples = 4096;
    writerQos.resource_limits.max_samples_per_instance = 4096;
    writerQos.resource_limits.max_instances = 64;

    impl_->controlBaseWriter = impl_->publisher->create_datawriter(
        impl_->controlTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    impl_->initBaseWriter = impl_->publisher->create_datawriter(
        impl_->initTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    impl_->realtimeBaseWriter = impl_->publisher->create_datawriter(
        impl_->realtimeTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    impl_->controlWriter = dynamic_cast<HwaSimIRDds::ControlCommandV1DataWriter*>(
        impl_->controlBaseWriter);
    impl_->initWriter = dynamic_cast<HwaSimIRDds::InitCommandV1DataWriter*>(
        impl_->initBaseWriter);
    impl_->realtimeWriter = dynamic_cast<HwaSimIRDds::RealtimeDataV1DataWriter*>(
        impl_->realtimeBaseWriter);
    if (!impl_->controlWriter || !impl_->initWriter || !impl_->realtimeWriter)
    {
        std::cerr << "[DDS][ERROR] create_datawriter or typed writer cast failed" << std::endl;
        shutdown();
        return false;
    }

    impl_->ackListener.reset(new InitAckListener(
        [this](const HwaSimIRDds::InitAckV1& sample) {
            std::lock_guard<std::mutex> lock(impl_->ackMutex);
            Impl::AckEntry entry = {};
            entry.sequence = impl_->nextAckSequence++;
            entry.sample = sample;
            impl_->acknowledgments.push_back(entry);
            impl_->ackChanged.notify_all();
        }));
    impl_->ackReader = DDS::DDSIF::SubTopic(
        impl_->participant,
        "HwaSimIR.InitAck",
        HwaSimIRDds::InitAckV1TypeSupport::get_instance(),
        "hwasimir_protocol_reader",
        impl_->ackListener.get());
    if (!impl_->ackReader)
    {
        std::cerr << "[DDS][ERROR] create InitAck reader failed" << std::endl;
        shutdown();
        return false;
    }

    impl_->running = true;
    std::cout << "DDS initialized domain=" << domainId
              << " participant=hwasimir_tcp publisherCount=1 writerCount=3" << std::endl;
    return true;
}

bool HwaSimIRSimpleDdsClient::sendControl(
    const HwaSimIRDds::ControlCommandV1& sample)
{
    return WriteSample(impl_->controlWriter, sample, "Control");
}

bool HwaSimIRSimpleDdsClient::sendInit(const HwaSimIRDds::InitCommandV1& sample)
{
    {
        std::lock_guard<std::mutex> lock(impl_->ackMutex);
        // 本次等待只消费本次 Init 写入之后到达的 Ack。
        impl_->waitFromSequence = impl_->nextAckSequence;
    }
    return WriteSample(impl_->initWriter, sample, "Init");
}

bool HwaSimIRSimpleDdsClient::sendRealtime(
    const HwaSimIRDds::RealtimeDataV1& sample)
{
    return WriteSample(impl_->realtimeWriter, sample, "Realtime");
}

bool HwaSimIRSimpleDdsClient::waitForInitAck(
    int expectedPlatID, int expectedSensorID, int timeoutMs,
    HwaSimIRDds::InitAckV1& ack)
{
    std::unique_lock<std::mutex> lock(impl_->ackMutex);
    const auto matches = [this, expectedPlatID, expectedSensorID](
        HwaSimIRDds::InitAckV1* result) {
        for (std::size_t i = 0; i < impl_->acknowledgments.size(); ++i)
        {
            const Impl::AckEntry& entry = impl_->acknowledgments[i];
            if (entry.sequence >= impl_->waitFromSequence &&
                entry.sample.platID == expectedPlatID &&
                entry.sample.sensorID == expectedSensorID &&
                entry.sample.trackingReady)
            {
                if (result)
                {
                    *result = entry.sample;
                    impl_->waitFromSequence = entry.sequence + 1;
                }
                return true;
            }
        }
        return false;
    };

    if (!impl_->ackChanged.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [&matches] { return matches(nullptr); }))
    {
        std::cerr << "[DDS][ERROR] InitAck timeout plat=" << expectedPlatID
                  << " sensor=" << expectedSensorID << std::endl;
        return false;
    }
    return matches(&ack);
}

void HwaSimIRSimpleDdsClient::shutdown()
{
    if (!impl_->factory && !impl_->participant)
        return;

    // DDSIF::Finalize 统一释放 Reader、Writer、Topic、Publisher 和 Participant。
    const DDS::ReturnCode_t result = DDS::DDSIF::Finalize();
    std::cout << "DDS shutdown result=" << static_cast<int>(result) << std::endl;
    impl_->running = false;
    impl_->ackReader = nullptr;
    impl_->ackListener.reset();
    impl_->controlWriter = nullptr;
    impl_->initWriter = nullptr;
    impl_->realtimeWriter = nullptr;
    impl_->controlBaseWriter = nullptr;
    impl_->initBaseWriter = nullptr;
    impl_->realtimeBaseWriter = nullptr;
    impl_->publisher = nullptr;
    impl_->controlTopic = nullptr;
    impl_->initTopic = nullptr;
    impl_->realtimeTopic = nullptr;
    impl_->participant = nullptr;
    impl_->factory = nullptr;
}
