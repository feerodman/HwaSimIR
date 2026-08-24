/**
* @file:       main_sub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Descroption: 先启动接收端，再启动发送端
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
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
        "PRESENTATIONQOS", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    //获取默认的sub的qos并设置
    SubscriberQos subQos;
    rtn = dp->get_default_subscriber_qos(subQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default subscriber qos failed\n");
        return -1;
    }
    subQos.presentation.coherent_access = true;
    subQos.presentation.ordered_access = true;
    subQos.presentation.access_scope = INSTANCE_PRESENTATION_QOS;

    // 创建订阅者
    Subscriber* sub = dp->create_subscriber(subQos, NULL, STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    //获取默认的DataReaderQos并设置
    DataReaderQos drQos;
    rtn = sub->get_default_datareader_qos(drQos);
    if (rtn != RETCODE_OK)
    {
        printf("get default datareader qos failed\n");
        return -1;
    }
    drQos.history.kind = KEEP_ALL_HISTORY_QOS;
    drQos.reliability.kind = RELIABLE_RELIABILITY_QOS;

    // 创建监听器
    tDataReaderListener tlistener;

    // 创建数据读者
    DataReader* dr = sub->create_datareader(tp, drQos, &tlistener, DATA_AVAILABLE_STATUS);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

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