#include "hwasimir_publiser.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "DefaultQos.h"
#include "Publisher.h"
#include <QDebug>

HwaSimIR_publiser::HwaSimIR_publiser(DDS::DomainParticipantFactory* factory,
    uint domainId, QObject* parent)
    : QObject(parent), m_factory(factory)
{
    if (!m_factory)
        return;
    // HwaSimIR 必须使用 TCP profile，不能照搬 ZR 的默认 Participant QoS。
    m_participant = m_factory->create_participant_with_qos_profile(
        domainId, "default_lib", "default_profile", "hwasimir_tcp",
        nullptr, DDS::STATUS_MASK_NONE);
    if (!m_participant)
    {
        qCritical() << "[HwaSimIR] hwasimir_tcp Participant failed, domain=" << domainId;
        return;
    }
    qInfo() << "[HwaSimIR] Participant created, domain=" << domainId
            << "factory=" << m_factory;

    const char* controlType = HwaSimIRDds::ControlCommandV1TypeSupport::get_instance()->get_type_name();
    const char* initType = HwaSimIRDds::InitCommandV1TypeSupport::get_instance()->get_type_name();
    const char* realtimeType = HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance()->get_type_name();
    if (HwaSimIRDds::ControlCommandV1TypeSupport::get_instance()->register_type(m_participant, controlType) != DDS::RETCODE_OK ||
        HwaSimIRDds::InitCommandV1TypeSupport::get_instance()->register_type(m_participant, initType) != DDS::RETCODE_OK ||
        HwaSimIRDds::RealtimeDataV1TypeSupport::get_instance()->register_type(m_participant, realtimeType) != DDS::RETCODE_OK)
    {
        qCritical() << "[HwaSimIR] register writer types failed";
        return;
    }

    DDS::Topic* controlTopic = m_participant->create_topic("HwaSimIR.Control", controlType,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    DDS::Topic* initTopic = m_participant->create_topic("HwaSimIR.Init", initType,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    DDS::Topic* realtimeTopic = m_participant->create_topic("HwaSimIR.Realtime", realtimeType,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    DDS::Publisher* publisher = m_participant->create_publisher(
        DDS::PUBLISHER_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!controlTopic || !initTopic || !realtimeTopic || !publisher)
    {
        qCritical() << "[HwaSimIR] create Topic/Publisher failed";
        return;
    }

    DDS::DataWriterQos writerQos;
    if (m_factory->get_datawriter_qos_from_profile(writerQos,
        "default_lib", "default_profile", "hwasimir_protocol_writer") != DDS::RETCODE_OK)
    {
        qCritical() << "[HwaSimIR] load Writer QoS failed";
        return;
    }
    // 保留参考客户端的多 sensor 实例与最长阻塞时间，XML 原样保留。
    writerQos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    writerQos.reliability.max_blocking_time.sec = 60;
    writerQos.reliability.max_blocking_time.nanosec = 0;
    writerQos.history.kind = DDS::KEEP_ALL_HISTORY_QOS;
    writerQos.resource_limits.max_samples = 4096;
    writerQos.resource_limits.max_samples_per_instance = 4096;
    writerQos.resource_limits.max_instances = 64;

    DDS::DataWriter* controlWriter = publisher->create_datawriter(
        controlTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    DDS::DataWriter* initWriter = publisher->create_datawriter(
        initTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    DDS::DataWriter* realtimeWriter = publisher->create_datawriter(
        realtimeTopic, writerQos, nullptr, DDS::STATUS_MASK_NONE);
    m_controlWriter = dynamic_cast<HwaSimIRDds::ControlCommandV1DataWriter*>(controlWriter);
    m_initWriter = dynamic_cast<HwaSimIRDds::InitCommandV1DataWriter*>(initWriter);
    m_realtimeWriter = dynamic_cast<HwaSimIRDds::RealtimeDataV1DataWriter*>(realtimeWriter);
    qInfo() << "[HwaSimIR] writer types/topics/publisher ready; typed Writer count="
            << int(m_controlWriter != nullptr) + int(m_initWriter != nullptr) + int(m_realtimeWriter != nullptr);
}

HwaSimIR_publiser::~HwaSimIR_publiser()
{
    // InitAck 订阅对象须先析构，再删除本模块 Participant；不能释放公共 Factory。
    if (m_participant)
    {
        const DDS::ReturnCode_t entities = m_participant->delete_contained_entities();
        const DDS::ReturnCode_t participantResult = m_factory->delete_participant(m_participant);
        qInfo() << "[HwaSimIR] Participant cleanup=" << entities << participantResult;
    }
}

bool HwaSimIR_publiser::isReady() const
{
    return m_controlWriter && m_initWriter && m_realtimeWriter;
}

bool HwaSimIR_publiser::pubControl(const HwaSimIRDds::ControlCommandV1& data)
{
    return m_controlWriter && m_controlWriter->write(data, DDS::HANDLE_NIL_NATIVE) == DDS::RETCODE_OK;
}

bool HwaSimIR_publiser::pubInit(const HwaSimIRDds::InitCommandV1& data)
{
    return m_initWriter && m_initWriter->write(data, DDS::HANDLE_NIL_NATIVE) == DDS::RETCODE_OK;
}

bool HwaSimIR_publiser::pubRealtime(const HwaSimIRDds::RealtimeDataV1& data)
{
    return m_realtimeWriter && m_realtimeWriter->write(data, DDS::HANDLE_NIL_NATIVE) == DDS::RETCODE_OK;
}
