/**
* @file:       main_pub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include "WaitSet.h"
#include "PublicationMatchedStatus.h"
#include "StatusKind.h"
#include <stdio.h>
#include <string.h>

void dwListener_on_offered_deadline(
    DDS_DataWriter* writer, const DDS_OfferedDeadlineMissedStatus* status)
{
    ZR_UINT32 key_arry[4];
    memcpy(key_arry, status->last_instance_handle.value, 4 * sizeof(ZR_UINT32));
    printf("Offered Deadline Missed\n");

    // 可以通过OfferedDeadlineMissedStatus获取更多详细信息
    printf("OfferedDeadlineMissedStatus total_count:%d\n", status->total_count);
    printf("OfferedDeadlineMissedStatus total_count_change:%d\n", status->total_count_change);
    printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n",
        key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriterQos dwQos;
    DDS_DataWriterListener dwListener;
    DDS_DataWriter* _dw;
    ShapeTypeDataWriter* dw;
    ShapeType data;
    int i = 0;

    //获取域参与者单例
    factory = DDS_DomainParticipantFactory_get_instance();
    if(factory == NULL)
    {
        printf("get instance failed\n");
        return -1;
    }


    // 创建域参与者
    dp = DDS_DomainParticipantFactory_create_participant(
        factory, domain_id, &DDS_DOMAINPARTICIPANT_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    // 注册数据类型
    rtn = ShapeTypeTypeSupport_register_type(dp, NULL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(
        dp, 
        "DEADLINEQOSPOLICY", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建发布者
    pub = DDS_DomainParticipant_create_publisher(
        dp, &DDS_PUBLISHER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (pub == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }

    //创建监听器
    DDS_DataWriterListener_initial(&dwListener);
    dwListener.on_offered_deadline_missed = dwListener_on_offered_deadline;

    // 创建数据写者
    DDS_DefaultDataWriterQosInitial(&dwQos);
    DDS_Publisher_get_default_datawriter_qos(pub, &dwQos);
    dwQos.deadline.period.sec = 2;
    dwQos.deadline.period.nanosec = 0;
    _dw = DDS_Publisher_create_datawriter(
        pub, tp, &dwQos, &dwListener, DDS_OFFERED_DEADLINE_MISSED_STATUS);
    dw = (ShapeTypeDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "DeadLineQosPolicy");

    // 循环发送数据
    while (true)
    {
        data.x = i;
        rtn = ShapeTypeDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
        if (rtn != DDS_RETCODE_OK)
        {
            printf("write failed\n");
        }
        i++;

        // 数据发送周期为4秒一次（超过接收端截止时间策略规定的时间）
        ZRSleep(4000);
    }

    // 释放DDS资源
    rtn = DDS_DomainParticipant_delete_contained_entities(dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dp delete contained entities failed\n");
        return -1;
    }

    rtn = DDS_DomainParticipantFactory_delete_participant(factory, dp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf delete dp failed\n");
        return -1;
    }

    ShapeTypeFinalize(&data);
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}