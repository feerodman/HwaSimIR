/**
* @file:       main_sub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description: 先启动Subscription工程，再启动Publication工程
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

void batchQos_on_data_available(DDS_DataReader *reader)
{
    ShapeTypeDataReader* dr;
    ShapeTypeSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    unsigned int i;

    dr = (ShapeTypeDataReader*)(reader);

    ShapeTypeSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据    
    rtn = ShapeTypeDataReader_take(
        dr, &dataSeq, &infoSeq, 
        LENGTH_UNLIMITED, 
        DDS_ANY_SAMPLE_STATE, 
        DDS_ANY_VIEW_STATE, 
        DDS_ANY_INSTANCE_STATE);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("take data failed\n");
        return -1;
    }
    printf("本组共%d包数据\n", ShapeTypeSeq_get_length(&dataSeq));

    // 遍历读取接收数据
    for (i = 0; i < ShapeTypeSeq_get_length(&dataSeq); ++i)
    {
        // 在使用数据之前，应检查数据的有效性
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            continue;
        }

        // 打印接收数据
        ShapeTypePrintData(ShapeTypeSeq_get_reference(&dataSeq,i));
    }

    // 返还数据空间
    rtn = ShapeTypeDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return -1;
    }
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Subscriber* sub;
    DDS_DataReaderQos drQos;
    DDS_DataReader* dr;
    DDS_DataReaderListener tlistener;

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

    // 注册数据类型
    rtn = ShapeTypeTypeSupport_register_type(dp, NULL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(
        dp, "BATCHQOS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建订阅者
    sub = DDS_DomainParticipant_create_subscriber(
        dp, &DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    //设置数据读者的qos
    DDS_DefaultDataReaderQosInitial(&drQos);
    rtn = DDS_Subscriber_get_default_datareader_qos(sub, &drQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default datareader qos failed\n");
    }
    drQos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;

    // 创建监听器
    DDS_DataReaderListener_initial(&tlistener);
    tlistener.on_data_available = batchQos_on_data_available;

    // 创建数据读者
    dr = DDS_Subscriber_create_datareader(sub, tp, &drQos, &tlistener, DDS_STATUS_MASK_ALL);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听数据到达
    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
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

    rtn = DDS_DomainParticipantFactory_finalize_instance();
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }
    return 0;
}