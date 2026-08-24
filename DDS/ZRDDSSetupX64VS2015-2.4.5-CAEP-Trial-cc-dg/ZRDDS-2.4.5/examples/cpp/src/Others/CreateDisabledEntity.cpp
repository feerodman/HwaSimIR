#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "DataReader.h"
#include "Topic.h"
#include "DataReaderListener.h"
using namespace DDS;

DataReader* CreateDisabledDataReader(
    Subscriber* subscriber,
    Topic* topic,
    DataReaderListener* listener)
{
    // 获取默认的QoS
    SubscriberQos subscriber_qos;
    subscriber->get_qos(subscriber_qos);
    // 修改QoS
    subscriber_qos.entity_factory.autoenable_created_entities = false;
    // 设置QoS 
    subscriber->set_qos(subscriber_qos);
    DataReader* reader = subscriber->create_datareader(
        topic,
        DATAREADER_QOS_DEFAULT,
        listener,
        STATUS_MASK_ALL);
    // 在使能前的其他操作(初始化，订阅数据前的准备……) 

    // 手动使能数据读者
    reader->enable();
    return reader;
}