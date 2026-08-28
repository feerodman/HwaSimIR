#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Publisher.h"
#include "DataWriter.h"
#include "FooTypeSupport.h"
#include "FooDataWriter.h"
#include "Foo.h"
#include <stdio.h>
#include "ZRSleep.h"

int main()
{
    /*域*/
    DDS_DomainId_t domainId = 11;
    DDS_DomainParticipantFactory* factory = DDS_DomainParticipantFactory_get_instance();
    if (NULL == factory)
    {
        printf("get instance failed.\n");
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
    DDS_ReturnCode_t rtn = FooTypeSupport_register_type(participant, type_name);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed.\n");
        return -1;
    }
    /*主题*/
    DDS_Topic *topic = DDS_DomainParticipant_create_topic(participant, "example", type_name, &TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == topic)
    {
        printf("create topic failed.\n");
        return -1;
    }
    /*发布端*/
    DDS_Publisher *publisher = DDS_DomainParticipant_create_publisher(participant, &PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == publisher)
    {
        printf("create publisher failed.\n");
        return -1;
    }
    /*datawriter*/
    DDS_DataWriterQos datawriter_qos;
    DDS_Publisher_get_default_datawriter_qos(publisher, &datawriter_qos);
    datawriter_qos.history.depth = 5;
    DDS_DataWriter *writer = DDS_Publisher_create_datawriter(publisher, topic, &datawriter_qos, NULL, DDS_STATUS_MASK_NONE);
    if (NULL == writer)
    {
        printf("create datawriter failed.\n");
        return -1;
    }
    FooDataWriter *_writer = (FooDataWriter*)(writer);
    /*data*/
    Foo data;
    FooInitialize(&data);
    DDS_InstanceHandle_t handle = FooDataWriter_register_instance(_writer, &data);
    while (true)
    {
        /*此处可以对data赋值*/
        FooDataWriter_write(_writer, &data, &handle);
        printf("send a data\n");
        ZRSleep(1000);
    }
    return 0;
}