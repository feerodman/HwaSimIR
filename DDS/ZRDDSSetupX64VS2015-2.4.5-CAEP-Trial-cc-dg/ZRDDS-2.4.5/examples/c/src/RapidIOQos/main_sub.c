/**
* @file:       main_sub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

void rapidIoQos_on_data_available(DDS_DataReader *reader)
{
    ShapeTypeDataReader* dr;
    ShapeTypeSeq dataSeq;
    DDS_SampleInfoSeq infoSeq;
    DDS_ReturnCode_t rtn;
    unsigned int i;

    dr = (ShapeTypeDataReader*)(reader);

    ShapeTypeSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    // 取出接收数据    
    rtn = ShapeTypeDataReader_take(
        dr, &dataSeq, &infoSeq,
        LENGTH_UNLIMITED,
        DDS_ANY_SAMPLE_STATE,
        DDS_ANY_VIEW_STATE,
        DDS_ANY_INSTANCE_STATE);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("take data failed\n");
        return -1;
    }

    // 遍历读取接收数据
    for (i = 0; i < ShapeTypeSeq_get_length(&dataSeq); ++i)
    {
        // 在使用数据之前，应检查数据的有效性
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            continue;
        }

        // 打印接收数据
        ShapeTypePrintData(ShapeTypeSeq_get_reference(&dataSeq, i));
    }

    // 返还数据空间
    rtn = ShapeTypeDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return -1;
    }
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Subscriber* sub;
    DDS_DataReader* dr;
    DDS_DomainParticipantFactoryQos dpFactQos;
    DDS_DomainParticipantQos dpQos;
    DDS_DataReaderListener tlistener;
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
    rtn = DDS_DomainParticipantFactory_get_default_participant_qos(factory, &dpQos);
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

    // 创建订阅者
    sub = DDS_DomainParticipant_create_subscriber(
        dp, &DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建监听器
    DDS_DataReaderListener_initial(&tlistener);
    tlistener.on_data_available = rapidIoQos_on_data_available;

    // 创建数据读者
    dr = DDS_Subscriber_create_datareader(
        sub, tp, &DDS_DATAREADER_QOS_DEFAULT, &tlistener, DDS_STATUS_MASK_ALL);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听数据到达
    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
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