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
        "DATARECEIVEBYFILTER", 
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

    //等待对端匹配
    PublicationMatchedStatus drMatchedStatus;
    while (true)
    {
        dw->get_publication_matched_status(drMatchedStatus);
        if (drMatchedStatus.total_count == 1)
        {
            break;
        }
    }

    // 初始化数据
    ShapeType data;
    ShapeTypeInitialize(&data);
    int i = 0;
    printf("send data!\n");

    // 循环发送数据
    while (true)
    {
        data.x = i%2;
        data.y = i;
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