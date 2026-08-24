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
#include "ZRSleep.h"
#include <stdio.h>

// 继承DataReaderListener
class Mylistener : public DataReaderListener
{
    // 回调函数
    void on_data_available(DataReader *the_reader)
    {
        printf("received data.\n");
        FooDataReader *foo_reader = dynamic_cast<FooDataReader*>(the_reader);
        if (foo_reader == NULL)
        {
            printf("cast reader failed.\n");
            return;
        }
        FooSeq data_values;
        SampleInfoSeq sample_infos;
        ReturnCode_t rtn;
        rtn = foo_reader->take(data_values, 
            sample_infos, 
            MAX_INT32_VALUE, 
            ANY_SAMPLE_STATE, 
            ANY_VIEW_STATE, 
            ANY_INSTANCE_STATE);
        if (RETCODE_ERROR == rtn)
        {
            printf("take failed.\n");
            return;
        }
        if (RETCODE_NO_DATA == rtn)
        {
            printf("no data.\n");
            return;
        }
        for (int i = 0; i < sample_infos.length(); i++)        
        {
            if (sample_infos[i].valid_data)
            {
                FooPrintData(&data_values[i]);
            }
        }
        foo_reader->return_loan(data_values, sample_infos);
    }
};

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
    if (NULL == type_name)
    {
        return -1;
    }
    ReturnCode_t rtn = FooTypeSupport::get_instance()->register_type(participant, type_name);
    if (RETCODE_OK != rtn)
    {
        printf("register type failed.\n");
        return -1;
    }
    // 创建主题
    Topic *topic = participant->create_topic("example", type_name, TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (NULL == topic)
    {
        printf("create topic failed.\n");
        return -1;
    }
    // 创建订阅端
    Subscriber *subscriber = participant->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT, 
        NULL, STATUS_MASK_NONE);
    if (NULL == subscriber)
    {
        printf("create subscriber failed.\n");
        return -1;
    }
    // 监听器
    Mylistener *listener = new Mylistener;
    // 创建数据读者
    DataReaderQos reader_qos;
    subscriber->get_default_datareader_qos(reader_qos);
    reader_qos.history.depth = 5;
    DataReader *reader = subscriber->create_datareader(
        topic, reader_qos, listener, STATUS_MASK_ALL);
    FooDataReader *_reader = dynamic_cast<FooDataReader*>(reader);
    if (NULL == reader)
    {
        printf("create datareader failed.\n");
        return -1;
    }
    while (true)
    {
        ZRSleep(1000);
    }
    return 0;
}
