/**
* @file:       main_sub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动接收端（Subscription）程序，再启动发送端（Publication）程序
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "StatusKind.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

//接收数据的标志位
int info;                 

ShapeTypeDataReader* dr = NULL;
ShapeTypeSeq dataSeq;
DDS_SampleInfoSeq infoSeq;

void lifespanQos_on_data_available(DDS_DataReader *reader)
{
    DDS_ReturnCode_t rtn;
    unsigned int i;

    dr = (ShapeTypeDataReader*)(reader);

    ShapeTypeSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据
    rtn = ShapeTypeDataReader_read(
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
        // 在使用数据之前，应检查数据的有效性
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            continue;
        }

        // 打印接收数据
        ShapeTypePrintData(ShapeTypeSeq_get_reference(&dataSeq, i));

        //接收到一包数据后，更新标志位
        info = 1;
    }

    // 返还数据空间
    rtn = ShapeTypeDataReader_return_loan(dr, &dataSeq, &infoSeq);
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
    DDS_Topic* tp;
    DDS_Subscriber* sub;
    DDS_DataReader* reader;
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
        dp, 
        "LIFESPANQOS", 
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

    // 创建监听器
    DDS_DataReaderListener_initial(&tlistener);
    tlistener.on_data_available = lifespanQos_on_data_available;

    // 创建数据读者
    reader = DDS_Subscriber_create_datareader(
        sub, tp, &DDS_DATAREADER_QOS_DEFAULT, &tlistener, DDS_DATA_AVAILABLE_STATUS);
    dr = (ShapeTypeDataReader*)(reader);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }
    
    //等待接收数据
    while (info == 0)
    {                
        ZRSleep(4000);
        printf("wait for receive data\n");
    }

    ShapeTypeSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    //等待数据过期
    ZRSleep(3500);
    
    //再次取出数据
    rtn = ShapeTypeDataReader_read(
        dr, 
        &dataSeq, 
        &infoSeq, 
        LENGTH_UNLIMITED, 
        DDS_READ_SAMPLE_STATE, 
        DDS_ANY_VIEW_STATE, 
        DDS_ANY_INSTANCE_STATE);
    if (rtn == DDS_RETCODE_NO_DATA)
    {
        printf("old data expired\n");
    }
    else
    {
        printf("read data error\n");
        return -1;
    }

    // 返还数据空间
    rtn = ShapeTypeDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return -1;
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
    getchar();
    return 0;
}