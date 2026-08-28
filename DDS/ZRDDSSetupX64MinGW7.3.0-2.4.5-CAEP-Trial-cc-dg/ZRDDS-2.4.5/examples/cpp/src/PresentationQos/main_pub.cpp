/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
* 
* Descroption: 先启动接收端，再启动发送端
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
        "PRESENTATIONQOS", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    //获取默认的pub的qos并设置
    PublisherQos pubQos;
    rtn = dp->get_default_publisher_qos(pubQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    pubQos.presentation.coherent_access = true;
    pubQos.presentation.ordered_access = true;
    pubQos.presentation.access_scope = INSTANCE_PRESENTATION_QOS;

    // 创建发布者
    Publisher* pub = dp->create_publisher(pubQos, NULL, STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    //获取默认的DataWriterQos并设置
    DataWriterQos dwQos;
    rtn = pub->get_default_datawriter_qos(dwQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default datawriter qos failed\n");
    }
    dwQos.history.kind = KEEP_ALL_HISTORY_QOS;
    dwQos.reliability.kind = RELIABLE_RELIABILITY_QOS;

    // 创建数据写者
    DataWriter* _dw = pub->create_datawriter(tp, dwQos, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw = dynamic_cast<ShapeTypeDataWriter*>(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    ZRSleep(2000);

    // 初始化数据
    ShapeType data;
    ShapeTypeInitialize(&data);    
    strcpy(data.z, "PresentationQos");

    //调用begin_coherent_changes开始发送一组连续的数据样本
    rtn = pub->begin_coherent_changes();
    if (rtn != RETCODE_OK)
    {
        printf("begin coherent changes failed\n");
        return -1;
    }
    
    // 发送3个实例下的数据
    for (int i = 0; i < 10;i++)
    {
        data.x = i % 3;
        data.y = i;
        rtn = dw->write(data, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }
        ZRSleep(1000);
    }

    //调用end_coherent_changes结束发送操作
    rtn = pub->end_coherent_changes();
    if (rtn != RETCODE_OK)
    {
        printf("end coherent changes failed\n");
        return -1;
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