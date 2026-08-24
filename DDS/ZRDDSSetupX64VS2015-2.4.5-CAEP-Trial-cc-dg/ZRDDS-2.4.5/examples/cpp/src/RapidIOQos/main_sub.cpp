/**
* @file:       main_sub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class tDataReaderListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {
        ShapeTypeDataReader* dr = dynamic_cast<ShapeTypeDataReader*>(reader);
        ShapeTypeSeq dataSeq;
        SampleInfoSeq infoSeq;
        ReturnCode_t rtn;

        // 取出接收数据
        rtn = dr->take(
            dataSeq, 
            infoSeq, 
            LENGTH_UNLIMITED, 
            ANY_SAMPLE_STATE, 
            ANY_VIEW_STATE, 
            ANY_INSTANCE_STATE);
        if (rtn != RETCODE_OK)
        {
            printf("take data failed\n");
            return;
        }
        
        // 遍历读取接收数据
        for (unsigned int i = 0; i < infoSeq.length(); ++i)
        {
            // 在使用数据之前，应检查数据的有效性
            if (!infoSeq[i].valid_data)
            {
                continue;
            }
            // 打印接收数据
            ShapeTypePrintData(&dataSeq[i]);
        }

        // 返还数据空间
        rtn = dr->return_loan(dataSeq, infoSeq);
        if (rtn != RETCODE_OK)
        {
            printf("return loan failed\n");
            return;
        }
    }
};

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

    //设置 DomainParticipantFactoryQos
    DomainParticipantFactoryQos dpFactQos;
    rtn = TheParticipantFactory->get_qos(dpFactQos);
    if (rtn != RETCODE_OK)
    {
        printf("get domainparticipant factory qos failed\n");
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
    rtn = TheParticipantFactory->set_qos(dpFactQos);
    if (rtn != RETCODE_OK)
    {
        printf("set domainparticipant factory qos failed\n");
    }

    //设置 DomainParticipantQos
    DomainParticipantQos dpQos;
    rtn = TheParticipantFactory->get_default_participant_qos(dpQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default domainparticipant qos failed\n");
    }
#ifdef _ZRDDS_RIO
    dpQos.rapidio_controller.controller_id = 0;
#endif //_ZRDDS_RIO
    const ZR_INT8* tmpAddr = "rapidio://0//0";
    if (!dpQos.usertraffic_receive_addresses.addresses.ensure_length(1, 1))
    {
        printf("ensure usertraffic_receive_addresses length failed.\n");
    }
    dpQos.usertraffic_receive_addresses.addresses.set_at(0, tmpAddr);

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
        "RAPIDIOQOS", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建订阅者
    Subscriber* sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建监听器
    tDataReaderListener tlistener;

    // 创建数据读者
    DataReader* dr = sub->create_datareader(
        tp, DATAREADER_QOS_DEFAULT, &tlistener, DATA_AVAILABLE_STATUS);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
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

    ShapeTypeTypeSupport::finalize_instance();
    TheParticipantFactory->finalize_instance();
    return 0;
}