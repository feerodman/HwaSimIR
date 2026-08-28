#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "DataReader.h"
#include "Topic.h"
#include "DataReaderListener.h"
#include "FooDataReader.h"

DDS_DataReader* CreateDisabledDataReader(
    DDS_Subscriber* subscriber,
    DDS_Topic* topic,
    DDS_DataReaderListener* listener)
{
    /*获取默认的QoS*/
    DDS_SubscriberQos subscriber_qos;
    DDS_Subscriber_get_qos(subscriber, &subscriber_qos);
    /*修改QoS*/
    subscriber_qos.entity_factory.autoenable_created_entities = false;
    /*设置QoS*/
    DDS_Subscriber_set_qos(subscriber, &subscriber_qos);
    DDS_DataReader* reader = DDS_Subscriber_create_datareader(subscriber, (DDS_TopicDescription*)topic, &DATAREADER_QOS_DEFAULT, listener, DDS_STATUS_MASK_ALL);
    FooDataReader* _reader = (FooDataReader*)reader;
    /*在使能前的其他操作(初始化，订阅数据前的准备……)*/

    /*手动使能数据读者*/
    FooDataReader_enable(_reader);
    return (DDS_DataReader*)_reader;
}