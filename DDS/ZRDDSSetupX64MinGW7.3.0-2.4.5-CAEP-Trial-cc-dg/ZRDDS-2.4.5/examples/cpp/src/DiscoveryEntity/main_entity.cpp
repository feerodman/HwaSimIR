/**
* @file:       main_entity.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
* 
* Descrption:  先启动Listener，再启动Entity
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include "ShapeTypeDataReader.h"
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
        "DISCOVERYENTITY", 
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

    //创建订阅者
    Subscriber* sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }
    
    printf("创建数据写者！\n");

    // 创建数据写者
    DataWriter* _dw = pub->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (_dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    //等待对端发现
    ZRSleep(1000);

    printf("创建数据读者！\n");

    // 创建数据读者
    DataReader* _dr = sub->create_datareader(tp, DATAREADER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (_dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    //等待对端发现
    ZRSleep(1000);

    printf("输入任意字符，删除数据写者和数据读者！\n");
    getchar();

    //删除数据写者
    rtn = pub->delete_datawriter(_dw);
    if (rtn != RETCODE_OK)
    {
        printf("delete datawriter failed\n");
        return -1;
    }

    //等待对端发现
    ZRSleep(1000);

    //删除数据读者
    rtn = sub->delete_datareader(_dr);
    if (rtn != RETCODE_OK)
    {
        printf("delete datareader failed\n");
        return -1;
    }

    //等待对端发现
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

    ShapeTypeTypeSupport::finalize_instance();
    return 0;
}