/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriter* _dw;
    ShapeTypeDataWriter* dw;
    DDS_InstanceHandleSeq handleSeq;
    DDS_SubscriptionBuiltinTopicData subscriptionData;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    // 创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, 
        domain_id, 
        &DDS_DOMAINPARTICIPANT_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
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
        "GETMATCHEDSTATUS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建发布者
    pub = DDS_DomainParticipant_create_publisher(
        dp, &DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    // 创建数据写者
    _dw = DDS_Publisher_create_datawriter(
        pub, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dw = (ShapeTypeDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 获取writer匹配的reader信息
    while (true)
    {
        int i;
        //获取所有匹配的wirter句柄
        DDS_InstanceHandleSeq_initialize(&handleSeq);
        ShapeTypeDataWriter_get_matched_subscriptions(dw, &handleSeq);
        printf("当前匹配%d个数据读者\n", DDS_InstanceHandleSeq_get_length(&handleSeq));

        //获取句柄对应的详细信息
        for (i = 0; i < DDS_InstanceHandleSeq_get_length(&handleSeq); i++)
        {
            DDS_SubscriptionBuiltinTopicDataInitial(&subscriptionData);
            ShapeTypeDataWriter_get_matched_subscription_data(
                dw, &subscriptionData, DDS_InstanceHandleSeq_get_reference(&handleSeq, i));

            //通过subscriptionData可以获取匹配的数据读者的详细信息
            printf("subscriptionData_id = 0x%08X%08X%08X%08X\n\n",
                subscriptionData.key[0], subscriptionData.key[1],
                subscriptionData.key[2], subscriptionData.key[3]);
        }
        ZRSleep(2000);
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