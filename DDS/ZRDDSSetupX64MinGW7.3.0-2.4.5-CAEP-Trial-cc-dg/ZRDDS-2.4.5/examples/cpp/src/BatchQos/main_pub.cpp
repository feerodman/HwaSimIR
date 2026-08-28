/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
* 
* Description: 先启动Subscription工程，再启动Publication工程
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
        "BATCHQOS", 
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

    /*
    * 设置数据写者的qos
    * 本次示例通过设置dwQos.batch.max_samples，以发送样本数量为批量发送标准。
    * 此外，还可以通过设置其它批量发送标准，包括：
    * dwQos.max_data_bytes ： 样本累计长度
    * dwQos.max_flush_delay ：最大发送延迟
    */
    DataWriterQos dwQos;
    rtn = pub->get_default_datawriter_qos(dwQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default datawriter qos failed\n");
    }
    dwQos.batch.enable = true;
    dwQos.batch.max_samples = 5;
    dwQos.history.depth = 10;

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
    data.x = 0;
    data.y = 0;
#if defined(_WIN32)
    strcpy(data.z, "BatchQosPolicy");
#elif defined(_LINUX)
    data.z = "BatchQosPolicy";
#endif 
    // 发送7包数据
    for (int i = 0; i < 7;i++)
    {
        rtn = dw->write(data, HANDLE_NIL_NATIVE);
        if (rtn != RETCODE_OK)
        {
            printf("write failed\n");
        }
        data.x++;
        ZRSleep(1000);
    }

    getchar();

    // 手动刷新批量发送
    rtn = dw->flush();
    if (rtn != RETCODE_OK)
    {
        printf("flush failed\n");
    }

    // 等待发送完毕
    ZRSleep(1000);

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

    rtn = TheParticipantFactory->finalize_instance();
    if (rtn != RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }

    ShapeTypeFinalize(&data);
    ShapeTypeTypeSupport::finalize_instance();
    return 0;
}