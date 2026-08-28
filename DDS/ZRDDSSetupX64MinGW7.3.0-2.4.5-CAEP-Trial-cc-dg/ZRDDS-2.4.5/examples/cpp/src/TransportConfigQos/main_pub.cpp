/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include "WaitSet.h"
#include "PublicationMatchedStatus.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    ReturnCode_t rtn;

    if(TheParticipantFactory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }

    // 设置QoS
    DomainParticipantQos dpQos;
    TheParticipantFactory->get_default_participant_qos(dpQos);

    // 使用tcp协议进行数据通信
    // 此处选用默认的网卡地址及端口，用户可以设置为指定的网卡地址及端口
    // 用户还可以通过该策略设置其它传输协议，包括：
    ////使用udp raw协议进行数据通信
    // 例：const char* tAddr = "udpv4_raw://default//0";
    ////使用tcp raw协议进行数据通信
    // 例：const char* tAddr = "tcpv4_raw://default//0";
    ////使用共享内存协议进行数据通信
    // 例：const char* tAddr = "shmem://default//1024";
    const char* tAddr = "tcpv4://default//0";
    dpQos.usertraffic_receive_addresses.addresses.ensure_length(1, 1);
    dpQos.usertraffic_receive_addresses.addresses.set_at(0, tAddr);

    // 创建域参与者
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), dpQos, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    // 注册数据类型
    rtn = ShapeTypeTypeSupport::get_instance()->register_type(dp, NULL);
    if (rtn != RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    Topic* tp = dp->create_topic(
        "TRANSPORTCONFIGQOS", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建发布者
    Publisher* pub = dp->create_publisher(PUBLISHER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    // 创建数据写者
    DataWriter* _dw = pub->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw = dynamic_cast<ShapeTypeDataWriter*>(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeType data;
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "TransportConfigQos");

    // 循环发送数据
    while (true)
    {
        rtn = dw->write(data, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
        ZRSleep(2000);
    }

    // 释放DDS资源
    rtn = dp->delete_contained_entities();
    if (rtn != RETCODE_OK)
    {
        printf("dp delete contained entities failed\n");
        return -1;
    }

    rtn = TheParticipantFactory->delete_participant(dp);
    if (rtn != RETCODE_OK)
    {
        printf("dpf delete dp failed\n");
        return -1;
    }

    ShapeTypeFinalize(&data);
    ShapeTypeTypeSupport::finalize_instance();
    TheParticipantFactory->finalize_instance();
    return 0;
}