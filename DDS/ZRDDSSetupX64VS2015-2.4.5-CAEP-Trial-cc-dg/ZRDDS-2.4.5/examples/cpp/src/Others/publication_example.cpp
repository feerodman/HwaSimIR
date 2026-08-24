#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Publisher.h"
#include "DataWriter.h"
#include "Topic.h"
#include "Foo.h"
#include "FooDataWriter.h"
#include "FooTypeSupport.h"
#include "ZRSleep.h"
#include <stdio.h>

int main()
{
    // 域
    DomainId_t domainId = 11;
    DomainParticipantFactory *factory = DomainParticipantFactory::get_instance();
    // 域参与者
    DomainParticipant *participant = factory->create_participant(
        domainId, 
        DOMAINPARTICIPANT_QOS_DEFAULT, 
        NULL, STATUS_MASK_NONE);
    if (NULL == participant)
    {
        printf("create participant failed.\n");
        return -1;
    }
    // 注册类型
    const Char* type_name = FooTypeSupport::get_instance()->get_type_name();
    if (type_name == NULL)
    {
        return -1;
    }
    ReturnCode_t rtn = FooTypeSupport::get_instance()->register_type(participant, type_name);
    if (rtn != RETCODE_OK)
    {
        printf("register type failed.\n");
        return -1;
    }
    // 创建主题
    Topic *topic = participant->create_topic("example", type_name, TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (topic == NULL)
    {
        printf("create topic failed.\n");
        return -1;
    }
    // 创建发布端
    Publisher *publisher = participant->create_publisher(
        PUBLISHER_QOS_DEFAULT, 
        NULL, STATUS_MASK_NONE);
    if (publisher == NULL)
    {
        printf("create publisher failed.\n");
        return -1;
    }
    // 创建数据写者
    DataWriterQos writer_qos;
    publisher->get_default_datawriter_qos(writer_qos);
    writer_qos.history.depth = 5;
    DataWriter *writer = publisher->create_datawriter(
        topic, writer_qos, NULL, STATUS_MASK_NONE);
    FooDataWriter *_writer = dynamic_cast<FooDataWriter*>(writer);
    if (writer == NULL)
    {
        printf("create datawriter failed.\n");
        return -1;
    }
    // 创建数据样本
    Foo sample;
    while (true)
    {
        // 在此处修改数据样本的值
        
        // 发布数据样本
        _writer->write(sample, HANDLE_NIL_NATIVE);
        ZRSleep(1000);
    }
    return 0;
}
