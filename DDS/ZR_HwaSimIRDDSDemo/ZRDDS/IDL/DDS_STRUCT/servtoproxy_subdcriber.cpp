#include "servtoproxy_subdcriber.h"
#include <QDebug>

class  MylistenerServToProxy : public DDS::DataReaderListener
{
    // 数据到达回调函数
    void on_data_available(DDS::DataReader *the_reader)
    {
        printf("received data \n");
        // 转化为类型安全的数据读者接口
        ServToProxyDataReader *exampleReader = dynamic_cast<ServToProxyDataReader*> (the_reader);
        ServToProxySeq dataValues;
        DDS::SampleInfoSeq sampleInfos;
        DDS::ReturnCode_t rtn;
        if (exampleReader == NULL)
        {
            printf("datareader error\n");
            return;
        }
        rtn = exampleReader->take(
            dataValues,
            sampleInfos,
            100,
            DDS::ANY_SAMPLE_STATE,
            DDS::ANY_VIEW_STATE,
            DDS::ANY_INSTANCE_STATE);
        if (rtn != DDS::RETCODE_OK)
        {
            printf("take failed.\n");
            return;
        }
        for (unsigned int i = 0; i < sampleInfos.length(); i++)
        {
            // 在使用数据之前，应检查数据的有效性
            if (!sampleInfos[i].valid_data)
            {
                continue;
            }
            // TODO 在此处添加对数据的处理逻辑
            // ELINT_DEVICE_STATUS_PARAMETERSPrintData(&dataValues[i]);
            if(m_callBack)
                m_callBack(&dataValues[i], m_did, m_topicName);
        }
        exampleReader->return_loan(dataValues, sampleInfos);
        return;
    }

public:
    ServToProxy_RX::ProcessDataCallBack m_callBack;
    uint m_did;
    QString m_topicName;
};


ServToProxy_subscriber::ServToProxy_subscriber(DDS::DomainParticipantFactory* factory, uint dId, ServToProxy_RX::ProcessDataCallBack callBack, const QString &topicName, QObject *parent)
    : QObject{parent}
    , m_factory(factory)
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
    // 创建订阅者
    DDS::Subscriber *subscriber = participant->create_subscriber(
        DDS::SUBSCRIBER_QOS_DEFAULT,
        NULL,
        DDS::STATUS_MASK_NONE);
    if (subscriber == NULL)
    {
        printf("create subscriber failed.\n");
        return;
    }
    // 监听器
    m_listener = new MylistenerServToProxy;
    qDebug() << "m_callBack .............." << &callBack;
    m_listener->m_callBack = callBack;
    m_listener->m_did = dId;
    m_listener->m_topicName = topicName;
    // 创建数据读者
    DDS::DataReaderQos readerQos;
    subscriber->get_default_datareader_qos(readerQos);
    // TODO 在此处修改数据读者QoS
    readerQos.history.depth = 5;
    DDS::DataReader *reader = subscriber->create_datareader(
        topic,
        readerQos,
        m_listener,
        DDS::STATUS_MASK_ALL);
    m_reader = dynamic_cast<ServToProxyDataReader*> (reader);
    if(m_reader == NULL)
    {
        printf("create datareader failed.\n");
        return;
    }
}

ServToProxy_subscriber::~ServToProxy_subscriber()
{
    //回收DDS资源
    if (m_reader) m_reader->set_listener(NULL, DDS::STATUS_MASK_NONE);
    delete m_listener;
    m_listener = NULL;
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
    // 不释放公共 Factory，另一套 DDS 仍可能使用它。
}
