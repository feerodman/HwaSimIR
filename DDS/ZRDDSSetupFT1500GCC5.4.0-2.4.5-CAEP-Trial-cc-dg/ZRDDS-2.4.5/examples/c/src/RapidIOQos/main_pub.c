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
#include "StatusKind.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <stdio.h>
#include <string.h>

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
    DDS_DomainParticipantFactoryQos dpFactQos;
    DDS_DomainParticipantQos dpQos;
    ShapeType data;
    const ZR_INT8* tmpAddr = "rapidio://0//0";

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    //设置 DomainparticipantFactoryQos
    DDS_DefaultDomainParticipantFactoryQosInitial(&dpFactQos);
    rtn = DDS_DomainParticipantFactory_get_qos(
        DDS_DomainParticipantFactory_get_instance(), &dpFactQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get domainparticipant factory qos failed\n");
        return -1;
    }
    dpFactQos.dds_log.console_mask = 0xffff;
    dpFactQos.dds_log.file_mask = 0xffff;
#ifdef _ZRDDS_RIO
    if (!dpFactQos.rapidio_config.controllers_config.ensure_length(1, 1))
    {
        printf("ensure rio controllers config length failed.\n");
    }
    dpFactQos.rapidio_config.controllers_config[0].rapidio_controller = 0;
    dpFactQos.rapidio_config.controllers_config[0].receive_window_base_address = 0x10000000;
    dpFactQos.rapidio_config.controllers_config[0].rapidio_address = 0x20000000;
    dpFactQos.rapidio_config.controllers_config[0].receive_window_size = 0x00400000;
    dpFactQos.rapidio_config.controllers_config[0].receive_subwindow_size = 0x00100000;
#endif //_ZRDDS_RIO
    rtn = DDS_DomainParticipantFactory_set_qos(factory, &dpFactQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("set domainparticipant factory qos failed\n");
        return -1;
    }

    //设置 DomainparticipantQos
    DDS_DefaultDomainParticipantQosInitial(&dpQos);
    rtn = DDS_DomainParticipantFactory_get_default_participant_qos(factory,&dpQos);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("get default domainparticipant qos failed\n");
    }
#ifdef _ZRDDS_RIO
    dpQos.rapidio_controller.controller_id = 0;
#endif //_ZRDDS_RIO
    
    if (!DDS_StringSeq_ensure_length(&(dpQos.usertraffic_receive_addresses.addresses), 1, 1))
    {
        printf("ensure usertraffic_receive_addresses length failed.\n");
    }
    DDS_StringSeq_set(&(dpQos.usertraffic_receive_addresses.addresses), 0, &tmpAddr);

    //创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &dpQos, NULL, DDS_STATUS_MASK_NONE);
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
        "RAPIDIOQOS", 
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

    //创建数据写者
    _dw = DDS_Publisher_create_datawriter(
        pub, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
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
    strcpy(data.z, "RapidIOQos");

    // 发送数据
    while (true)
    {
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
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

    rtn = DDS_DomainParticipantFactory_finalize_instance();
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }
    return 0;
}