/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动发送端（Publication）程序，再启动接收端（Subscription）程序
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "Publisher.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "StatusKind.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <stdio.h>
#include <string.h>

void dwListener_on_publication_matched(
    DDS_DataWriter *the_writer, const DDS_PublicationMatchedStatus *status)
{
    ZR_UINT32 key_arry[4];
    memcpy(key_arry, status->last_subscription_handle.value, 4 * sizeof(ZR_UINT32));

    //已获取匹配状态
    printf("publication matched success!\n");

    //通过PublicationMatchedStatus可以获取匹配的数据读者的状态信息
    printf("PublicationMatchedStatus total_count:%d\n", status->total_count);
    printf("PublicationMatchedStatus total_count_change:%d\n", status->total_count_change);
    printf("PublicationMatchedStatus current_count:%d\n", status->current_count);
    printf("PublicationMatchedStatus current_count_change:%d\n", status->current_count_change);
    printf("Subscription_id = 0x%08X%08X%08X%08X\n\n",
        key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
}

int main(int argc, char** argv)
{
    //域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriterListener tListener;
    DDS_DataWriter* _dw;
    ShapeTypeDataWriter* dw;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    //创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &DDS_DOMAINPARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    //注册数据类型
    rtn = ShapeTypeTypeSupport_register_type(dp, NULL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    //创建主题
    tp = DDS_DomainParticipant_create_topic(
        dp, 
        "PUBLICATIONMATCHEDSTATUS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    //创建发布者
    pub = DDS_DomainParticipant_create_publisher(
        dp, &DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    //创建监听器
    DDS_DataWriterListener_initial(&tListener);
    tListener.on_publication_matched = dwListener_on_publication_matched;

    //创建数据写者
    _dw = DDS_Publisher_create_datawriter(
        pub, tp, &DDS_DATAWRITER_QOS_DEFAULT, &tListener, DDS_PUBLICATION_MATCHED_STATUS);
    dw = (ShapeTypeDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听回调
    while (true)
    {
        ZRSleep(5000);
        printf("wait for publication matched\n");
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