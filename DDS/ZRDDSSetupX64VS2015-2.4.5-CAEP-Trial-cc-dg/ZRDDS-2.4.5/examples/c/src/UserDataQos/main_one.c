/**
* @file:       main_one.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动one，再启动other
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "BuiltinDataDataReader.h"
#include "DataReaderListener.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "StatusKind.h"
#include <stdio.h>
#include <string.h>

void dpDiscoveryListener_on_data_available(DDS_DataReader *reader)
{
    DDS_ParticipantBuiltinTopicDataDataReader* dr;
    DDS_ParticipantBuiltinTopicDataSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    unsigned int i;
    char userData[100];

    dr = (DDS_ParticipantBuiltinTopicDataDataReader*)(reader);
    
    DDS_ParticipantBuiltinTopicDataSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据
    rtn = DDS_ParticipantBuiltinTopicDataDataReader_take(
        dr, 
        &dataSeq, 
        &infoSeq, 
        LENGTH_UNLIMITED, 
        DDS_ANY_SAMPLE_STATE, 
        DDS_ANY_VIEW_STATE, 
        DDS_ANY_INSTANCE_STATE);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("take data failed\n");
        return;
    }

    // 遍历读取接收数据
    for (i = 0; i < DDS_SampleInfoSeq_get_length(&infoSeq); ++i)
    {
        if (DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            printf("discovery domainparticipant online\n");
            
            if (DDS_CharSeq_get_length(
                    &DDS_ParticipantBuiltinTopicDataSeq_get_reference(
                         &dataSeq, i)->user_data.value) != 0)
            {
                // 获取并打印user_data
                DDS_CharSeq_to_array(&DDS_ParticipantBuiltinTopicDataSeq_get_reference(
                    &dataSeq, i)->user_data.value, 
                    userData, 
                    DDS_CharSeq_get_length(
                        &DDS_ParticipantBuiltinTopicDataSeq_get_reference(
                            &dataSeq, i)->user_data.value));
                printf("user_data : %s\n", userData);
            }
        }
    }

    // 返还数据空间
    rtn = DDS_ParticipantBuiltinTopicDataDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return;
    }
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Subscriber* sub;
    DDS_DataReaderListener dpListener;
    DDS_DataReader* reader;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    // 创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &DDS_DOMAINPARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    //获取内置的subscriber
    sub = DDS_DomainParticipant_get_builtin_subscriber(dp);
    if (sub == NULL)
    {
        printf("get builtin sub failed\n");
        return -1;
    }

    //获取内置的datareader
    reader = DDS_Subscriber_lookup_datareader(sub, "BUILTIN_PARTICIPANT_TOPIC_NAME");
    if (reader == NULL)
    {
        printf("get builtin reader failed\n");
        return -1;
    }

    // 创建监听器
    DDS_DataReaderListener_initial(&dpListener);
    dpListener.on_data_available = dpDiscoveryListener_on_data_available;

    //设置内置datareader的监听器
    DDS_ParticipantBuiltinTopicDataDataReader_set_listener(
        (DDS_ParticipantBuiltinTopicDataDataReader*)reader, &dpListener, DDS_DATA_AVAILABLE_STATUS);
    
    // 阻塞主线程，通过Listener监听参与者上下线
    while (true)
    {
        ZRSleep(5000);
        printf("wait discovery domainparticipant\n");
    }

    // 释放DDS资源
    rtn = DDS_DomainParticipant_delete_contained_entities(dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dp delete contained entities failed\n");
        return -1;
    }

    rtn = DDS_DomainParticipantFactory_delete_participant(factory, dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf delete dp failed\n");
        return -1;
    }

    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}