/**
* @file:       main_other.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动one，再启动other
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "DefaultQos.h"
#include "ZRSleep.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipantQos dpQos;
    const char *userData = "hello world.";
    DDS_DomainParticipant* dp;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    //修改UserDataQos
    DDS_DefaultDomainParticipantQosInitial(&dpQos);
    DDS_DomainParticipantFactory_get_default_participant_qos(factory, &dpQos);
    DDS_CharSeq_from_array(&dpQos.user_data.value, userData, strlen(userData) + 1);

    // 创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &dpQos, NULL, DDS_STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    printf("输入字符，删除域参与者并退出程序\n");
    getchar();

    // 释放DDS资源
    rtn = DDS_DomainParticipantFactory_delete_participant(factory, dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf delete dp failed\n");
        return -1;
    }

    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}