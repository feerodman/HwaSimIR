#include "servtoproxy_publiser.h"

ServToProxy_publiser::ServToProxy_publiser(DDS::DomainParticipantFactory* factory, uint dId, const QString &topicName, QObject *parent)
    : QObject{parent}
    , m_factory(factory)
    , m_did(dId)
    , m_topic(topicName)
{
    // 设置域号
    DDS::DomainId_t domainId = dId;
    // 获取参与者工厂实例
    // 工厂由 DdsRuntime 预先加载 QoS；本模块不再次初始化。
    if (factory == NULL)
    {
        return;
    }
    // 域参与者
    DDS::DomainParticipantQos dpQos;
    factory->get_default_participant_qos(dpQos);
    // TODO 在这里修改DomainParticipantQos
    // 创建域参与者
    participant = factory->create_participant(
        domainId,
        dpQos,
        NULL,
        DDS::STATUS_MASK_NONE);
    if (participant == NULL)
    {
        printf("create participant failed.\n");
        return;
    }
    qInfo() << "[ZR] Participant created, domain=" << domainId << "factory=" << factory;
    // 注册数据类型
    const DDS_Char* typeName = ServToProxyTypeSupport::get_instance()->get_type_name();
    DDS::ReturnCode_t rtn = ServToProxyTypeSupport::get_instance()->register_type(
        participant,
        typeName);
    if (rtn != DDS::RETCODE_OK)
    {
        printf("register type failed.\n");
        return;
    }
    // 创建主题
    DDS::Topic *topic = participant->create_topic(
        topicName.toStdString().c_str(),
        typeName,
        DDS::TOPIC_QOS_DEFAULT,
        NULL,
        DDS::STATUS_MASK_NONE);
    if (topic == NULL)
    {
        printf("create topic failed.\n");
        return;
    }
    // 创建发布者
    DDS::Publisher *publisher = participant->create_publisher(
        DDS::PUBLISHER_QOS_DEFAULT,
        NULL,
        DDS::STATUS_MASK_NONE);
    if (publisher == NULL)
    {
        printf("create publisher failed.\n");
        return;
    }
    // 创建数据写者
    DDS::DataWriterQos writerQos;
    publisher->get_default_datawriter_qos(writerQos);
    //TODO 在这里修改数据写者Qos
    writerQos.history.depth = 5;
    DDS::DataWriter *writer = publisher->create_datawriter(
        topic,
        writerQos,
        NULL,
        DDS::STATUS_MASK_NONE);
    // 转化为类型相关的数据写者
    m_writer = dynamic_cast<ServToProxyDataWriter*> (writer);
    if (m_writer == NULL)
    {
        printf("create datawriter failed.\n");
        return;
    }
}

ServToProxy_publiser::~ServToProxy_publiser()
{
    //回收DDS资源
    if (!participant) return;
    if(participant->delete_contained_entities() != DDS::RETCODE_OK)
    {
        printf("DomainParticipant delete contained entities failed");
        return;
    }
    if(m_factory->delete_participant(participant) != DDS::RETCODE_OK)
    {
        printf("DomainParticipantFactory delete DomainParticipant failed");
        return;
    }
    qInfo() << "[ZR] Participant deleted";
    // 不释放公共 Factory，另一套 DDS 仍可能使用它。
}

void ServToProxy_publiser::pubData(ServToProxy data)
{
    if (!m_writer) return;
    // 创建数据样本
    // ELINT_AOA_DETECTION_DATA data;
    // ELINT_AOA_DETECTION_DATAInitialize(&data);
    //仅对有key的数据调用以下函数
    DDS::InstanceHandle_t handle = m_writer->register_instance(data);
    //发送数据
    //TODO 在此处修改样本值
    DDS::ReturnCode_t rtn = m_writer->write(data, handle);
    if (rtn != DDS::RETCODE_OK)
    {
        qDebug() << "error : "<< rtn;
    }
    else
    {

    }

    ServToProxyFinalize(&data);
}

