/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description: 先启动Subscription工程，再启动Publication工程
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
    int i;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriterQos dwQos;    
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
        "BATCHQOS", 
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
    pub = DDS_DomainParticipant_create_publisher(dp, &DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    /*
    * 设置数据写者的qos
    * 本次示例通过设置dwQos.batch.max_samples，以发送样本数量为批量发送标准。
    * 此外，还可以通过设置其它批量发送标准，包括：
    * dwQos.max_data_bytes ： 样本累计长度
    * dwQos.max_flush_delay ：最大发送延迟
    */

    DDS_DefaultDataWriterQosInitial(&dwQos);
    rtn = DDS_Publisher_get_default_datawriter_qos(pub, &dwQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default datawriter qos failed\n");
    }
    dwQos.batch.enable = true;
    dwQos.batch.max_samples = 5;
    dwQos.history.depth = 10;

    //创建数据写者
    _dw = DDS_Publisher_create_datawriter(pub, tp, &dwQos, NULL, DDS_STATUS_MASK_NONE);
    dw = (ShapeTypeDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    ZRSleep(2000);

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "BatchQosPolicy");

    // 发送7包数据
    for (i = 0; i < 7; i++)
    {
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
        ZRSleep(1000);
    }

    getchar();

    // 手动刷新批量发送
    rtn = ShapeTypeDataWriter_flush(dw);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("flush failed\n");
    }

    // 等待发送完毕
    ZRSleep(1000);

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
    return 0;
}