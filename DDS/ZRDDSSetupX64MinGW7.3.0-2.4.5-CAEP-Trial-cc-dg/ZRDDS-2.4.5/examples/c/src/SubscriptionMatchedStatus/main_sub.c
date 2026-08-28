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

void drListener_on_subscription_matched(
    DDS_DataReader *reader, const DDS_SubscriptionMatchedStatus* status)
{
    ZR_UINT32 key_arry[4];
    memcpy(key_arry, status->last_publication_handle.value, 4 * sizeof(ZR_UINT32));

    //已获取匹配状态
    printf("subscription matched success!\n");

    //通过SubscriptionMatchedStatus可以获取匹配的数据写者的详细信息
    printf("SubscriptionMatchedStatus total_count:%d\n", status->total_count);
    printf("SubscriptionMatchedStatus total_count_change:%d\n", status->total_count_change);
    printf("SubscriptionMatchedStatus current_count:%d\n", status->current_count);
    printf("SubscriptionMatchedStatus current_count_change:%d\n", status->current_count_change);
    printf("Publication_id = 0x%08X%08X%08X%08X\n\n",
        key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
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
        dp, 
        "SUBSCRIPTIONMATCHEDSTATUS", 
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
    tlistener.on_subscription_matched = drListener_on_subscription_matched;

    // 创建数据读者
    dr = DDS_Subscriber_create_datareader(
        sub, tp, &DDS_DATAREADER_QOS_DEFAULT, &tlistener, DDS_SUBSCRIPTION_MATCHED_STATUS);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听回调
    while (true)
    {
        ZRSleep(5000);
        printf("wait for subscription matched\n");
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