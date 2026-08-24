/**
* @file:       main_operation.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* detail：        先启动Discovery，再启动Operation，退出Operation后再退出Discovery
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

    // 创建域参与者
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    printf("输入任意字符，删除域参与者并退出程序\n");
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