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
#include "WaitSet.h"
#include "PublicationMatchedStatus.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class tDataWriterListener : public DataWriterListener
{
    void on_offered_deadline_missed(
        DataWriter *the_writer, const OfferedDeadlineMissedStatus &status)
    {
        ZR_UINT32 key_arry[4];
        memcpy(key_arry, status.last_instance_handle.value, 4 * sizeof(ZR_UINT32));
        printf("Offered Deadline Missed\n");

        //可以通过OfferedDeadlineMissedStatus获取更多信息
        printf("OfferedDeadlineMissedStatus total_count:%d\n", status.total_count);
        printf("OfferedDeadlineMissedStatus total_count_change:%d\n", status.total_count_change);
        printf("DataWriter_id = 0x%08X%08X%08X%08X\n", 
            key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
    }
};

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
        "DEADLINEQOSPOLICY", 
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

    //创建监听器
    tDataWriterListener dataWriterListener;
    
    // 创建数据写者
    DataWriterQos dwQos;
    pub->get_default_datawriter_qos(dwQos);
    dwQos.deadline.period.sec = 2;
    dwQos.deadline.period.nanosec = 0;
    DataWriter* _dw = pub->create_datawriter(
        tp, dwQos, &dataWriterListener, OFFERED_DEADLINE_MISSED_STATUS);
    ShapeTypeDataWriter* dw = dynamic_cast<ShapeTypeDataWriter*>(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeType data;
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "DeadLineQosPolicy");

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

        // 数据发送周期为4秒一次（超过接收端截止时间策略规定的时间）
        ZRSleep(4000);
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