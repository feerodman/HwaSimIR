/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description: 先执行接收端（Subscription）程序，再执行发送端（Publication）程序
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
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

    // 创建域参与者
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return - 1;
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
        "OWNERSHIPQOS", 
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

    //修改writer的qos
    DataWriterQos dwQos1;
    DataWriterQos dwQos2;
    pub->get_default_datawriter_qos(dwQos1);
    pub->get_default_datawriter_qos(dwQos2);
    dwQos1.ownership.kind = EXCLUSIVE_OWNERSHIP_QOS;
    dwQos2.ownership.kind = EXCLUSIVE_OWNERSHIP_QOS;
    dwQos1.ownership_strength.value = 1;
    dwQos2.ownership_strength.value = 2;
    dwQos1.deadline.period.sec = 2;
    dwQos2.deadline.period.sec = 2;

    // 创建两个数据写者
    DataWriter* _dw1 = pub->create_datawriter(tp, dwQos1, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw1 = dynamic_cast<ShapeTypeDataWriter*>(_dw1);
    if (dw1 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }
    DataWriter* _dw2 = pub->create_datawriter(tp, dwQos2, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw2 = dynamic_cast<ShapeTypeDataWriter*>(_dw2);
    if (dw2 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeType data1, data2;
    ShapeTypeInitialize(&data1);    
    ShapeTypeInitialize(&data2);
    data1.x = 0;
    data2.x = 0;
    data1.y = 0;
    data2.y = 0;
    strcpy(data1.z, "from dw1");
    strcpy(data2.z, "from dw2");

    // 匹配等待
    ZRSleep(2000);

    // dw1先发送数据
    int i = 0;
    for (; i < 10; i++)
    {
        data1.y = i;
        rtn = dw1->write(data1, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }
        ZRSleep(1000);
    }

    // dw2开始与dw1一起发送数据（dw2因权重较大，将在发送数据后，获得该实例的所有权）
    while (true)
    {
        data1.y = i;
        data2.y = i;

        rtn = dw1->write(data1, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }

        ZRSleep(1000);

        rtn = dw2->write(data2, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }

        i++;
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

    ShapeTypeFinalize(&data1);
    ShapeTypeFinalize(&data2);
    ShapeTypeTypeSupport::finalize_instance();
    TheParticipantFactory->finalize_instance();
    return 0;
}