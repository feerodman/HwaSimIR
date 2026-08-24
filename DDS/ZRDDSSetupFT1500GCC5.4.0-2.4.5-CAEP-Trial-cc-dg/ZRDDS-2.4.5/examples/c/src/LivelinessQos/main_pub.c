/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description: 先执行接收端（Subscription）程序，再执行发送端（Publication）程序
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
#include "WaitSet.h"
#include "PublicationMatchedStatus.h"
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
    DDS_DataWriterQos dwQos;
    DDS_DataWriter* _dw;
    ShapeTypeDataWriter* dw;
    ShapeType data;
    int i;

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
        "LIVELINESSQOSPOLICY", 
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
    DDS_DefaultDataWriterQosInitial(&dwQos);
    DDS_Publisher_get_default_datawriter_qos(pub, &dwQos);

    dwQos.liveliness.kind = DDS_MANUAL_BY_TOPIC_LIVELINESS_QOS;
    dwQos.liveliness.lease_duration.sec = 2;
    dwQos.liveliness.lease_duration.nanosec = 0;

    _dw = DDS_Publisher_create_datawriter(pub, tp, &dwQos, NULL, DDS_STATUS_MASK_NONE);
    dw = (ShapeTypeDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "LivelinessQos");

    //等待对端匹配
    ZRSleep(2000);

    // 连续发送5包数据后停止发送
    for (i = 0; i < 5; ++i)
    {
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
        ZRSleep(1000);
    }
    printf("write stopped\n");
    getchar();

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
    ShapeTypeFinalize(&data);
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}