/**
* @file:       main_listener.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
* 
* Descrption:先启动Listener，再启动Entity
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "BuiltinDataDataReader.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

void dwBuiltListener_on_data_available(DDS_DataReader *reader)
{
    DDS_PublicationBuiltinTopicDataDataReader* dr = 
        (DDS_PublicationBuiltinTopicDataDataReader*)(reader);
    DDS_PublicationBuiltinTopicDataSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    unsigned int i;

    DDS_PublicationBuiltinTopicDataSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据
    rtn = DDS_PublicationBuiltinTopicDataDataReader_take(
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
    for (i = 0; i < DDS_PublicationBuiltinTopicDataSeq_get_length(&dataSeq); ++i)
    {
        // 验证远端数据写者是否上线
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            ZR_UINT32 key_arry[4];
            memcpy(key_arry, DDS_SampleInfoSeq_get_reference(&infoSeq, i)->instance_handle.value, 
                4 * sizeof(ZR_UINT32));

            printf("数据写者下线！\n");
            printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n", 
                key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
        }
        else
        {
            printf("数据写者上线！\n");
            printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n",
                DDS_PublicationBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[0], 
                DDS_PublicationBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[1], 
                DDS_PublicationBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[2], 
                DDS_PublicationBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[3]);
        }
    }

    // 返还数据空间
    rtn = DDS_PublicationBuiltinTopicDataDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return;
    }
}

void drBuiltListener_on_data_available(DDS_DataReader *reader)
{
    DDS_SubscriptionBuiltinTopicDataDataReader* dr = 
        (DDS_SubscriptionBuiltinTopicDataDataReader*)(reader);
    DDS_SubscriptionBuiltinTopicDataSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    unsigned int i;

    DDS_SubscriptionBuiltinTopicDataSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据
    rtn = DDS_SubscriptionBuiltinTopicDataDataReader_take(
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
    for (i = 0; i < DDS_SubscriptionBuiltinTopicDataSeq_get_length(&dataSeq); ++i)
    {
        // 验证远端数据读者是否上线
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            ZR_UINT32 key_arry[4];
            memcpy(key_arry, DDS_SampleInfoSeq_get_reference(&infoSeq, i)->instance_handle.value, 
                4 * sizeof(ZR_UINT32));

            printf("数据读者下线！\n");
            printf("DataReader_id = 0x%08X%08X%08X%08X\n\n", 
                key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
        }
        else
        {
            printf("数据读者上线！\n");
            printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n",
                DDS_SubscriptionBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[0],
                DDS_SubscriptionBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[1],
                DDS_SubscriptionBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[2],
                DDS_SubscriptionBuiltinTopicDataSeq_get_reference(&dataSeq, i)->key[3]);
        }
    }

    // 返还数据空间
    rtn = DDS_SubscriptionBuiltinTopicDataDataReader_return_loan(dr, &dataSeq, &infoSeq);
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
    DDS_DomainParticipantFactoryQos dpfQos;
    DDS_DomainParticipant* dp;
    DDS_Subscriber* builtSub;
    DDS_DataReader* pubDr;
    DDS_DataReaderListener dwListener;
    DDS_DataReader* subDr;
    DDS_DataReaderListener drListener;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    //设置DomainParticipantFactory的qos为未使能
    DDS_DefaultDomainParticipantFactoryQosInitial(&dpfQos);
    DDS_DomainParticipantFactory_get_qos(factory, &dpfQos);
    dpfQos.entity_factory.autoenable_created_entities = false;
    DDS_DomainParticipantFactory_set_qos(factory, &dpfQos);

    // 创建域参与者,域参与者为未使能状态                                      
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &DDS_DOMAINPARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    
    //获取内置的订阅者
    builtSub = DDS_DomainParticipant_get_builtin_subscriber(dp);
    if (builtSub == NULL)
    {
        printf("get builtin subscriber failed\n");
        return -1;
    }

    // 查找内置的PublicationBuiltinTopicDataDataReader
    pubDr = DDS_Subscriber_lookup_datareader(builtSub, BUILTIN_PUBLICATION_TOPIC_NAME);
    if (pubDr == NULL)
    {
        printf("get builtin pubDr failed\n");
        return -1;
    }

    //设置监听器
    DDS_DataReaderListener_initial(&dwListener);
    dwListener.on_data_available = dwBuiltListener_on_data_available;
    rtn = ShapeTypeDataReader_set_listener(
        (ShapeTypeDataReader*)pubDr, &dwListener, DDS_STATUS_MASK_ALL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("set listener failed\n");
        return -1;
    }

    // 查找内置的SubscriptionBuiltinTopicDataDataReader
    subDr = DDS_Subscriber_lookup_datareader(builtSub, BUILTIN_SUBSCRIPTION_TOPIC_NAME);
    if (subDr == NULL)
    {
        printf("get builtin subDr failed\n");
        return -1;
    }

    //设置监听器
    DDS_DataReaderListener_initial(&drListener);
    drListener.on_data_available = drBuiltListener_on_data_available;
    rtn = ShapeTypeDataReader_set_listener(
        (ShapeTypeDataReader*)subDr, &drListener, DDS_STATUS_MASK_ALL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("set listener failed\n");
        return -1;
    }

    //使能域参与者
    rtn = DDS_DomainParticipant_enable(dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("enable dp failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听回调
    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
    }

    //还原DomainParticipantFactoryQos
    dpfQos.entity_factory.autoenable_created_entities = true;
    DDS_DomainParticipantFactory_set_qos(factory, &dpfQos);

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

    rtn = DDS_DomainParticipantFactory_finalize_instance();
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }
    return 0;
}