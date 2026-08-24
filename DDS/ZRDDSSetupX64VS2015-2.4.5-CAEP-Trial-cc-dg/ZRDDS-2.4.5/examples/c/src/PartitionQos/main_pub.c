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
    DDS_PublisherQos pubQos1;
    const char* part1 = "group1";
    DDS_Publisher* pub1;
    DDS_DataWriter* _dw1;
    ShapeTypeDataWriter* dw1;
    DDS_PublisherQos pubQos2;
    const char* part2 = "group2";
    DDS_Publisher* pub2;
    DDS_DataWriter* _dw2;
    ShapeTypeDataWriter* dw2;
    DDS_PublisherQos pubQos3;
    const char* part3 = "group3";
    DDS_Publisher* pub3;
    DDS_DataWriter* _dw3;
    ShapeTypeDataWriter* dw3;
    ShapeType data;
    int in;

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
        "PARTITIONQOS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }
    /*******************************分区配置1**********************************/

    //设置发布者qos
    DDS_DefaultPublisherQosInitial(&pubQos1);
    rtn = DDS_DomainParticipant_get_default_publisher_qos(dp, &pubQos1);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    DDS_StringSeq_set_maximum(&pubQos1.partition.name, 1);
    DDS_StringSeq_set_length(&pubQos1.partition.name, 1);
    DDS_StringSeq_set(&pubQos1.partition.name, 0, &part1);
    
    // 创建发布者
    pub1 = DDS_DomainParticipant_create_publisher(dp, &pubQos1, NULL, DDS_STATUS_MASK_NONE);
    if (pub1 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    _dw1 = DDS_Publisher_create_datawriter(
        pub1, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dw1 = (ShapeTypeDataWriter*)(_dw1);
    if (dw1 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    /*******************************分区配置2**********************************/

    //设置发布者qos
    DDS_DefaultPublisherQosInitial(&pubQos2);
    rtn = DDS_DomainParticipant_get_default_publisher_qos(dp, &pubQos2);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    DDS_StringSeq_set_maximum(&pubQos2.partition.name, 1);
    DDS_StringSeq_set_length(&pubQos2.partition.name, 1);
    DDS_StringSeq_set(&pubQos2.partition.name, 0, &part2);

    // 创建发布者
    pub2 = DDS_DomainParticipant_create_publisher(dp, &pubQos2, NULL, DDS_STATUS_MASK_NONE);
    if (pub2 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    _dw2 = DDS_Publisher_create_datawriter(
        pub2, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dw2 = (ShapeTypeDataWriter*)(_dw2);
    if (dw2 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    /*******************************分区配置3**********************************/

    //设置发布者qos
    DDS_DefaultPublisherQosInitial(&pubQos3);
    rtn = DDS_DomainParticipant_get_default_publisher_qos(dp, &pubQos3);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    DDS_StringSeq_set_maximum(&pubQos3.partition.name, 1);
    DDS_StringSeq_set_length(&pubQos3.partition.name, 1);
    DDS_StringSeq_set(&pubQos3.partition.name, 0, &part3);

    // 创建发布者
    pub3 = DDS_DomainParticipant_create_publisher(dp, &pubQos3, NULL, DDS_STATUS_MASK_NONE);
    if (pub3 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    _dw3 = DDS_Publisher_create_datawriter(
        pub3, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dw3 = (ShapeTypeDataWriter*)(_dw3);
    if (dw3 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "PartitionQosPolicy");

    printf("Please choose the group:\n");
    printf("choose group 1: x = 0\n");
    printf("choose group 2: x = 1\n");
    printf("choose group 3: x = 2\n");

    scanf("%d", &in);
    switch (in)
    {
    case 1:
    {
        // 循环发送数据
        while (true)
        {
            rtn = ShapeTypeDataWriter_write(dw1, &data, &DDS_HANDLE_NIL_NATIVE);
            if (rtn != DDS_RETCODE_OK)
            {
                printf("write failed\n");
                return -1;
            }
            data.y++;
            ZRSleep(1000);
        }
        break;
    }
    case 2:
    {
        data.x = 1;
        // 循环发送数据
        while (true)
        {
            rtn = ShapeTypeDataWriter_write(dw2, &data, &DDS_HANDLE_NIL_NATIVE);
            if (rtn != DDS_RETCODE_OK)
            {
                printf("write failed\n");
                return -1;
            }
            data.y++;
            ZRSleep(1000);
        }
        break;
    }
    case 3:
    {
        data.x = 2;
        // 循环发送数据
        while (true)
        {
            rtn = ShapeTypeDataWriter_write(dw3, &data, &DDS_HANDLE_NIL_NATIVE);
            if (rtn != DDS_RETCODE_OK)
            {
                printf("write failed\n");
                return -1;
            }
            data.y++;
            ZRSleep(1000);
        }
        break;
    }
    default:
        printf("input error\n");
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
    ShapeTypeFinalize(&data);
    getchar();
    return 0;
}