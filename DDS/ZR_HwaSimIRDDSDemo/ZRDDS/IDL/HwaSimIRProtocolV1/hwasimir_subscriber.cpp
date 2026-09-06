#include "hwasimir_subscriber.h"
#include "HwaSimIRProtocolV1TypeSupport.h"
#include "DataReaderListener.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "Topic.h"
#include <QDebug>

class MyListenerInitAck : public DDS::DataReaderListener
{
public:
    HwaSimIR_RX::ProcessDataCallBack m_callback;
    uint m_domainId;
    QString m_topicName;

    void on_data_available(DDS::DataReader* reader) override
    {
        HwaSimIRDds::InitAckV1DataReader* typedReader =
            dynamic_cast<HwaSimIRDds::InitAckV1DataReader*>(reader);
        if (!typedReader)
            return;
        HwaSimIRDds::InitAckV1Seq samples;
        DDS::SampleInfoSeq infos;
        // 读空当前批次，避免一次只取部分样本后剩余 Ack 没有新事件唤醒。
        const DDS::ReturnCode_t result = typedReader->take(samples, infos,
            LENGTH_UNLIMITED, DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);
        if (result == DDS::RETCODE_NO_DATA)
            return;
        if (result != DDS::RETCODE_OK)
        {
            qWarning() << "[HwaSimIR] InitAck take failed:" << result;
            return;
        }
        // 不做阻塞等待或 Ack 匹配，全部有效样本交给业务 callback。
        try
        {
            for (unsigned int i = 0; i < infos.length(); ++i)
                if (infos[i].valid_data && m_callback)
                    m_callback(&samples[i], m_domainId, m_topicName);
        }
        catch (...)
        {
            qWarning() << "[HwaSimIR] InitAck callback threw an exception";
        }
        // 无论业务回调是否成功，都要归还 DDS 样本。
        const DDS::ReturnCode_t loanResult = typedReader->return_loan(samples, infos);
        if (loanResult != DDS::RETCODE_OK)
            qWarning() << "[HwaSimIR] InitAck return_loan failed:" << loanResult;
    }
};

HwaSimIR_subscriber::HwaSimIR_subscriber(DDS::DomainParticipantFactory* factory,
    DDS::DomainParticipant* participant, HwaSimIR_RX::ProcessDataCallBack callback,
    const QString& topicName, QObject* parent)
    : QObject(parent), m_participant(participant)
{
    if (!factory || !m_participant)
        return;
    const char* typeName = HwaSimIRDds::InitAckV1TypeSupport::get_instance()->get_type_name();
    if (HwaSimIRDds::InitAckV1TypeSupport::get_instance()->register_type(
        m_participant, typeName) != DDS::RETCODE_OK)
    {
        qCritical() << "[HwaSimIR] register InitAck failed";
        return;
    }
    m_topic = m_participant->create_topic(topicName.toStdString().c_str(), typeName,
        DDS::TOPIC_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    m_subscriber = m_participant->create_subscriber(
        DDS::SUBSCRIBER_QOS_DEFAULT, nullptr, DDS::STATUS_MASK_NONE);
    if (!m_topic || !m_subscriber)
    {
        qCritical() << "[HwaSimIR] create InitAck Topic/Subscriber failed";
        return;
    }
    DDS::DataReaderQos readerQos;
    if (factory->get_datareader_qos_from_profile(readerQos,
        "default_lib", "default_profile", "hwasimir_protocol_reader") != DDS::RETCODE_OK)
    {
        qCritical() << "[HwaSimIR] load InitAck Reader QoS failed";
        return;
    }
    m_listener = new MyListenerInitAck;
    m_listener->m_callback = callback;
    m_listener->m_domainId = m_participant->get_domain_id();
    m_listener->m_topicName = topicName;
    DDS::DataReader* reader = m_subscriber->create_datareader(
        m_topic, readerQos, m_listener, DDS::STATUS_MASK_ALL);
    m_reader = dynamic_cast<HwaSimIRDds::InitAckV1DataReader*>(reader);
    qInfo() << "[HwaSimIR] InitAck type/topic/subscriber ready; typed Reader=" << m_reader;
}

HwaSimIR_subscriber::~HwaSimIR_subscriber()
{
    if (m_reader)
        m_reader->set_listener(nullptr, DDS::STATUS_MASK_NONE);
    if (m_subscriber)
    {
        const DDS::ReturnCode_t entities = m_subscriber->delete_contained_entities();
        const DDS::ReturnCode_t subscriberResult = m_participant->delete_subscriber(m_subscriber);
        qInfo() << "[HwaSimIR] InitAck subscriber cleanup=" << entities << subscriberResult;
    }
    delete m_listener;
    if (m_topic)
        m_participant->delete_topic(m_topic);
    // Participant 由 HwaSimIR 发布对象随后删除；公共工厂只能由 Runtime 释放。
}
