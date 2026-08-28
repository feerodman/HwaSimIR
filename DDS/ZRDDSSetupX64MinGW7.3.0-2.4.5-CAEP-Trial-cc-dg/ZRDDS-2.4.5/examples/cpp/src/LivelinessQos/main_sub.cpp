/**
* @file:       main_sub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description: 先执行接收端（Subscription）程序，再执行发送端（Publication）程序
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

    void on_liveliness_changed(DataReader *reader, const LivelinessChangedStatus &status)
    {
        ZR_UINT32 key_arry[4];
        memcpy(key_arry, status.last_publication_handle.value, 4 * sizeof(ZR_UINT32));

        if (status.alive_count == 1)
        {
            printf("DataWriter liveliness changed : create\n\n");
        }
        else if (status.alive_count == 0)
        {
            printf("DataWriter liveliness changed : delete\n\n");
        }

        // 通过LivelinessChangedStatus可以获取相关详细信息
        printf("LivelinessChangedStatus alive_count:%d\n", status.alive_count);
        printf("LivelinessChangedStatus not_alive_count:%d\n", status.not_alive_count);
        printf("LivelinessChangedStatus alive_count_change:%d\n", status.alive_count_change);
        printf("LivelinessChangedStatus not_alive_count_change:%d\n", status.not_alive_count_change);
        printf("Publication_id = 0x%08X%08X%08X%08X\n",
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
        "LIVELINESSQOSPOLICY", 
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

    // 创建dataReaderQos
    DataReaderQos drQos;
    rtn = sub->get_default_datareader_qos(drQos);
    drQos.liveliness.kind = MANUAL_BY_TOPIC_LIVELINESS_QOS;
    drQos.liveliness.lease_duration.sec = 3;
    drQos.liveliness.lease_duration.nanosec = 0;

    // 创建监听器
    tDataReaderListener tlistener;

    //创建数据读者
    DataReader* dr = sub->create_datareader(tp, drQos, &tlistener, DATA_AVAILABLE_STATUS | LIVELINESS_CHANGED_STATUS);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听数据到达及失活信息
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
    getchar();
    return 0;
}