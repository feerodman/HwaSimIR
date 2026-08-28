/**
* @file:       main_other.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动one，再启动other
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
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

    //修改UserDataQos
    DomainParticipantQos dpQos;
    TheParticipantFactory->get_default_participant_qos(dpQos);
    const char *userData = "hello world.";
    dpQos.user_data.value.from_array(userData, strlen(userData) + 1);

    // 创建域参与者
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), dpQos, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    printf("输入字符，删除域参与者并退出程序\n");
    getchar();

    // 释放DDS资源
    rtn = TheParticipantFactory->delete_participant(dp);
    if (rtn != RETCODE_OK)
    {
        printf("dpf delete dp failed\n");
        return -1;
    }

    TheParticipantFactory->finalize_instance();
    return 0;
}