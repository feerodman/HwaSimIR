/**
* @file:       main_sub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include "WaitSet.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class tDataReaderListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {
        ShapeTypeDataReader* dr = dynamic_cast<ShapeTypeDataReader*>(reader);
        ShapeTypeSeq dataSeq;
        SampleInfoSeq infoSeq;
        ReturnCode_t rtn;

        // 取出接收数据
        rtn = dr->take(
            dataSeq, 
            infoSeq, 
            LENGTH_UNLIMITED, 
            ANY_SAMPLE_STATE,
            ANY_VIEW_STATE, 
            ANY_INSTANCE_STATE);
        if (rtn != RETCODE_OK)
        {
            printf("take data failed\n");
            return;
        }

        // 遍历读取接收数据
        for (unsigned int i = 0; i < infoSeq.length(); ++i)
        {
            // 在使用数据之前，应检查数据的有效性
            if (!infoSeq[i].valid_data)
            {
                continue;
            }

            // 打印接收数据
            ShapeTypePrintData(&dataSeq[i]);
        }

        // 返还数据空间
        rtn = dr->return_loan(dataSeq, infoSeq);
        if (rtn != RETCODE_OK)
        {
            printf("return loan failed\n");
            return;
        }
    }

    void on_requested_deadline_missed(
        DataReader* reader, const RequestedDeadlineMissedStatus& status)
    {
        ZR_UINT32 key_arry[4];
        memcpy(key_arry, status.last_instance_handle.value, 4 * sizeof(ZR_UINT32));
        printf("requested deadline missed\n\n");

        // 可以通过RequestedDeadlineMissedStatus获取更多详细信息
        printf("RequestedDeadlineMissedStatus total_count:%d\n", status.total_count);
        printf("RequestedDeadlineMissedStatus total_count_change:%d\n", status.total_count_change);
        printf("DataReader_id = 0x%08X%08X%08X%08X\n",
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
    // 创建订阅者
    Subscriber* sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建监听器
    tDataReaderListener tlistener;

    // 设置dataReaderQos
    DataReaderQos drQos;
    rtn = sub->get_default_datareader_qos(drQos);
    drQos.deadline.period.sec = 3;
    drQos.deadline.period.nanosec = 0;

    // 创建数据读者
    DataReader* dr = sub->create_datareader(
        tp, drQos, &tlistener, DDS_DATA_AVAILABLE_STATUS | DDS_REQUESTED_DEADLINE_MISSED_STATUS);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听数据到达
    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
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

    ShapeTypeTypeSupport::finalize_instance();
    TheParticipantFactory->finalize_instance();
    return 0;
}