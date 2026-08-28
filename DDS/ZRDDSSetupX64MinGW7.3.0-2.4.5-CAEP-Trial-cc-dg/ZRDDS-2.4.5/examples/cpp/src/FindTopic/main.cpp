/**
* @file:       main.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <iostream>
#include <string.h>
#include <thread>

using namespace::DDS;
using namespace::std;

void mThread1(DomainParticipant* dp)
{
    // 创建主题
    Topic* tp = dp->create_topic(
        "FINDTOPIC", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return;
    }

    // 使用该主题，例：创建数据写者、数据读者
    ZRSleep(3000);

    // 删除主题
    ReturnCode_t rtn = dp->delete_topic(tp);
    if (rtn != RETCODE_OK)
    {
        printf("delete topic failed\n");
    }
}

void mThread2(DomainParticipant* dp)
{
    Duration_t ti;
    ti.sec = 2;
    ti.nanosec = 0;

    // 通过find_topic方法“发现”主题
    Topic* tp = dp->find_topic("FINDTOPIC", ti);
    if (tp == NULL)
    {
        printf("find tp failed\n");
        return;
    }

    // 使用该主题，例：创建数据写者、数据读者
    ZRSleep(3000);

    // 由于在本线程通过find_topic“发现”主题，故应在线程结束时，删除该主题一次
    ReturnCode_t rtn = dp->delete_topic(tp);
    if (rtn != RETCODE_OK)
    {
        printf("delete topic failed\n");
    }
}

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

    // 启动两个线程
    thread t1(mThread1, dp);
    thread t2(mThread2, dp);

    // 阻塞至线程结束
    t1.join();
    t2.join();

    getchar();

    // 释放DDS资源
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