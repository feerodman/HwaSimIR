/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
* 
* Descroption: 先启动接收端，再启动发送端
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
    DDS_PublisherQos pubQos;
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
        return - 1;
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
        "PRESENTATIONQOS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    //获取默认的pub的qos并设置
    DDS_DefaultPublisherQosInitial(&pubQos);
    rtn = DDS_DomainParticipant_get_default_publisher_qos(dp, &pubQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    pubQos.presentation.coherent_access = true;
    pubQos.presentation.ordered_access = true;
    pubQos.presentation.access_scope = DDS_INSTANCE_PRESENTATION_QOS;

    // 创建发布者
    pub = DDS_DomainParticipant_create_publisher(dp, &pubQos, NULL, DDS_STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    //获取默认的DataWriterQos并设置
    DDS_DefaultDataWriterQosInitial(&dwQos);
    rtn = DDS_Publisher_get_default_datawriter_qos(pub, &dwQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default datawriter qos failed\n");
    }
    dwQos.history.kind = DDS_KEEP_ALL_HISTORY_QOS;
    dwQos.reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;

    // 创建数据写者
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
    strcpy(data.z, "PresentationQos");

    //调用begin_coherent_changes开始发送一组连续的数据样本
    rtn = DDS_Publisher_begin_coherent_changes(pub);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("begin coherent changes failed\n");
        return -1;
    }
    
    // 发送3个实例下的数据
    for (i = 0; i < 10;i++)
    {
        data.x = i % 3;
        data.y = i;
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }

        ZRSleep(1000);
    }

    //调用end_coherent_changes结束发送操作
    rtn = DDS_Publisher_end_coherent_changes(pub);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("end coherent changes failed\n");
        return -1;
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