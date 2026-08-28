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
    DDS_DataWriterQos dwQos1;
    DDS_DataWriterQos dwQos2;
    DDS_DataWriter* _dw1;
    ShapeTypeDataWriter* dw1;
    DDS_DataWriter* _dw2;
    ShapeTypeDataWriter* dw2;
    ShapeType data1, data2;
    int i = 0;

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
        "OWNERSHIPQOS", 
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

    //修改writer的qos
    DDS_DefaultDataWriterQosInitial(&dwQos1);
    DDS_DefaultDataWriterQosInitial(&dwQos2);
    DDS_Publisher_get_default_datawriter_qos(pub, &dwQos1);
    DDS_Publisher_get_default_datawriter_qos(pub, &dwQos2);
    dwQos1.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
    dwQos2.ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;
    dwQos1.ownership_strength.value = 1;
    dwQos2.ownership_strength.value = 2;
    dwQos1.deadline.period.sec = 2;
    dwQos2.deadline.period.sec = 2;

    // 创建两个数据写者
    _dw1 = DDS_Publisher_create_datawriter(pub, tp, &dwQos1, NULL, DDS_STATUS_MASK_NONE);
    dw1 = (ShapeTypeDataWriter*)(_dw1);
    if (dw1 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }
    _dw2 = DDS_Publisher_create_datawriter(pub, tp, &dwQos2, NULL, DDS_STATUS_MASK_NONE);
    dw2 = (ShapeTypeDataWriter*)(_dw2);
    if (dw2 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
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
    for (; i < 10; i++)
    {
        data1.y = i;
        rtn = ShapeTypeDataWriter_write(dw1, &data1, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
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

        rtn = ShapeTypeDataWriter_write(dw1, &data1, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }

        ZRSleep(1000);

        rtn = ShapeTypeDataWriter_write(dw2, &data2, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        i++;
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
    ShapeTypeFinalize(&data1);
    ShapeTypeFinalize(&data2);
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}