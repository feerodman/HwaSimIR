/**
* @file:       main_sub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRDDSCWrapper.h"
#include "WaitSet.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif

void TakeAndShowData(ShapeTypeDataReader *dr)
{
    ShapeTypeSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    int seqLen;
    unsigned int i;
    ShapeTypeSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据
    rtn = ShapeTypeDataReader_take(dr, &dataSeq, &infoSeq, LENGTH_UNLIMITED,
        DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
    if (rtn == DDS_RETCODE_NO_DATA)
    {
        printf("no data\n");
        ShapeTypeSeq_finalize(&dataSeq);
        DDS_SampleInfoSeq_finalize(&infoSeq);
        return;
    }

    if (rtn != DDS_RETCODE_OK)
    {
        printf("take data failed\n");
        ShapeTypeSeq_finalize(&dataSeq);
        DDS_SampleInfoSeq_finalize(&infoSeq);
        return;
    }

    // 遍历读取接收数据
    seqLen = DDS_SampleInfoSeq_get_length(&infoSeq);

    for (i = 0; i < seqLen; ++i)
    {
        // 在使用数据之前，应检查数据的有效性
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            continue;
        }

        // 打印接收数据
        ShapeTypePrintData(ShapeTypeSeq_get_reference(&dataSeq, i));
    }

    // 返还数据空间
    rtn = ShapeTypeDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
    }
    ShapeTypeSeq_finalize(&dataSeq);
    DDS_SampleInfoSeq_finalize(&infoSeq);
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory *factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Subscriber* sub;
    DDS_DataReader* dr;
    ShapeTypeDataReader* shapeTypeReader;
    DDS_ReadCondition *recvCondition;
    DDS_WaitSet *readWaitSet;
    DDS_ConditionSeq activeConditionSeq;

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
    rtn = ShapeTypeTypeSupport_register_type(dp, ShapeTypeTypeSupport_get_type_name());
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(
        dp, 
        "DATA_RECEIVE_BY_WAIT_SET",
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

    // 创建数据读者
    dr = DDS_Subscriber_create_datareader(
        sub, tp, &DDS_DATAREADER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    shapeTypeReader = (ShapeTypeDataReader*)dr;
    if (shapeTypeReader == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 创建读取数据条件
    recvCondition = ShapeTypeDataReader_create_readcondition(
        shapeTypeReader, DDS_NOT_READ_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);

    // WaitSet关联条件
    readWaitSet = DDS_WaitSet_new();
    DDS_WaitSet_attach_condition(readWaitSet, recvCondition);

    DDS_ConditionSeq_initialize(&activeConditionSeq);
    // 等待状态改变，接收数据
    while (true)
    {
        DDS_ConditionSeq_clear(&activeConditionSeq);
        if (DDS_WaitSet_wait(readWaitSet, &activeConditionSeq, &INFINITE_DURATION) == DDS_RETCODE_OK)
        {
            // 读取并显示数据
            TakeAndShowData(shapeTypeReader);
        }
    }
    DDS_ConditionSeq_finalize(&activeConditionSeq);


    // 释放DDS资源
    ShapeTypeDataReader_delete_readcondition(shapeTypeReader, recvCondition);
    DDS_WaitSet_delete(readWaitSet);

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