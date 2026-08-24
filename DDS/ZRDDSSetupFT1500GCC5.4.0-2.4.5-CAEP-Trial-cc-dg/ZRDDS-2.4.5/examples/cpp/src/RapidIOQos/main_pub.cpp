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

    ZRSleep(2000);

    // 初始化数据
    ShapeType data;
    data.y = 0;
    ShapeTypeInitialize(&data);    
    strcpy(data.z, "RapidIOQos");

    // 循环发送数据
    int i = 0;
    while (true)
    {
        data.x = i;
        rtn = dw->write(data, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }
        i++;
        ZRSleep(1000);
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