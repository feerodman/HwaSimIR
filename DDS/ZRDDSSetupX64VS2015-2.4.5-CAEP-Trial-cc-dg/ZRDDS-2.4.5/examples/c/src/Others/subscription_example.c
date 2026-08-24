#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "DataReader.h"
#include "Topic.h"
#include "DataReaderListener.h"
#include "Foo.h"
#include "FooDataReader.h"
#include "FooTypeSupport.h"
#include <stdio.h>
#include <string.h>

void MyListener_on_data_available(DDS_DataReader *the_reader)
{
    printf("received data.\n");
    FooDataReader *_reader = (FooDataReader*)the_reader;
    FooSeq data_values;
    FooSeq_initialize(&data_values);
    DDS_SampleInfoSeq sample_infos;
    DDS_SampleInfoSeq_initialize(&sample_infos);
    if (NULL == the_reader)
    {
        printf("datareader error.\n");
        return;
    }
    FooDataReader_take(_reader, &data_values, &sample_infos, MAX_INT32_VALUE, DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
    for (int i = 0; i < sample_infos._length; i++)
    {
        FooPrintData(FooSeq_get_reference(&data_values, i));
    }
    FooDataReader_return_loan(_reader, &data_values, &sample_infos);
    return;
}

int main()
{
    /*域*/
    DDS_DomainId_t domainId = 11;
    DDS_DomainParticipantFactory* factory = DDS_DomainParticipantFactory_get_instance();
    if (NULL == factory)
    {
        printf("get insance failed.\n");
        return -1;
    }
    /*域参与者*/
    DDS_DomainParticipant *participant = DDS_DomainParticipantFactory_create_participant(factory, domainId, &DOMAINPARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == participant)
    {
        printf("create participant failed.\n");
        return -1;
    }
    /*注册类型*/
    const char* type_name = FooTypeSupport_get_type_name();
    if (NULL == type_name)
    {
        printf("get type name failed.\n");
        return -1;
    }
    DDS_ReturnCode_t ret = FooTypeSupport_register_type(participant, type_name);
    if (ret != DDS_RETCODE_OK)
    {
        printf("register type failed.\n");
        return -1;
    }
    /*主题*/
    DDS_Topic *topic = DDS_DomainParticipant_create_topic(participant, "example", type_name, &TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == topic)
    {
        printf("get topic failed.\n");
        return -1;
    }
    /*订阅端*/
    DDS_Subscriber *subscriber = DDS_DomainParticipant_create_subscriber(participant, &SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == subscriber)
    {
        printf("create suscriber failed.\n");
        return -1;
    }
    /*设置监听器*/
    DDS_DataReaderListener readerListener;
    memset(&readerListener, 0, sizeof(readerListener));
    readerListener.on_data_available = MyListener_on_data_available;
    /*datareader*/
    DDS_DataReaderQos datareader_qos;
    DDS_Subscriber_get_default_datareader_qos(subscriber, &datareader_qos);
    datareader_qos.history.depth = 5;
    DDS_DataReader *reader = DDS_Subscriber_create_datareader(subscriber, (DDS_TopicDescription*)topic, &datareader_qos, &readerListener, DDS_STATUS_MASK_ALL);
    if (NULL == reader)
    {
        printf("create datareader failed.\n");
        return -1;
    }
    FooDataReader *_reader = (FooDataReader*)(reader);
    while (true)
    {
        ZRSleep(1000);
    }
    return 0;
}