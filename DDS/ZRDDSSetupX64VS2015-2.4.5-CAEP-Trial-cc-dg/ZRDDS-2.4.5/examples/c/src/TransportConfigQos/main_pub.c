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
    DDS_DomainParticipantQos dpQos;
    const char* tAddr = "tcpv4://default//0";
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriter* _dw;
    ShapeTypeDataWriter* dw;
    ShapeType data;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    // 设置QoS
    DDS_DefaultDomainParticipantQosInitial(&dpQos);
    DDS_DomainParticipantFactory_get_default_participant_qos(factory, &dpQos);
    // 使用tcp协议进行数据通信
    // 此处选用默认的网卡地址及端口，用户可以设置为指定的网卡地址及端口
    // 用户还可以通过该策略设置其它传输协议，包括：
    ////使用udp raw协议进行数据通信
    // 例：const char* tAddr = "udpv4_raw://default//0";
    ////使用tcp raw协议进行数据通信
    // 例：const char* tAddr = "tcpv4_raw://default//0";
    ////使用共享内存协议进行数据通信
    // 例：const char* tAddr = "shmem://default//1048576";
    DDS_StringSeq_ensure_length(&dpQos.usertraffic_receive_addresses.addresses, 1, 1);
    DDS_StringSeq_set(&dpQos.usertraffic_receive_addresses.addresses, 0, &tAddr);

    // 创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &dpQos, NULL, DDS_STATUS_MASK_NONE);
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
        "TRANSPORTCONFIGQOS", 
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

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "TransportConfigQos");

    // 循环发送数据
    while (true)
    {
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
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

    ShapeTypeFinalize(&data);
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}