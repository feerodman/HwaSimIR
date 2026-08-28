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
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <stdio.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory *factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriter* dw;
    ShapeTypeDataWriter* shapeTypeWriter;
    ShapeType data;

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
        return - 1;
    }

    // 注册数据类型
    rtn = ShapeTypeTypeSupport_register_type(dp, ShapeTypeTypeSupport_get_type_name());
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(dp, "DATA_RECEIVE_BY_WAIT_SET",
        ShapeTypeTypeSupport_get_type_name(), &DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
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
    dw = DDS_Publisher_create_datawriter(
        pub, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    shapeTypeWriter = (ShapeTypeDataWriter*)dw;
    if (shapeTypeWriter == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "Data Receive by WaitSet");

    // 循环发送数据
    while (true)
    {
        rtn = ShapeTypeDataWriter_write(shapeTypeWriter, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
        ZRSleep(1000);
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
    ShapeTypeFinalize(&data);
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}