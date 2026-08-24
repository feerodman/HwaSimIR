/**
* @file:       main_sub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动接收端（Subscription）程序，再启动发送端（Publication）程序
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

//接收数据的标志位
int info;

ShapeTypeDataReader* dr = NULL;
ShapeTypeSeq dataSeq;
SampleInfoSeq infoSeq;

class tDataReaderListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {        
        dr = dynamic_cast<ShapeTypeDataReader*>(reader);
        ReturnCode_t rtn;

        // 取出接收数据
        rtn = dr->read(
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

            //接收到一包数据后，更新标志位
            info = 1;
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
        "LIFESPANQOS", 
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

    // 创建数据读者
    DataReader* reader = sub->create_datareader(
        tp, DATAREADER_QOS_DEFAULT, &tlistener, DDS_DATA_AVAILABLE_STATUS);
    dr = dynamic_cast<ShapeTypeDataReader*>(reader);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }
    

    //等待接收数据
    while (info == 0)
    {                
        ZRSleep(4000);
        printf("wait for receive data\n");
    }

    //等待数据过期
    ZRSleep(3500);
    
    //再次取出数据
    rtn = dr->read(
        dataSeq, infoSeq, LENGTH_UNLIMITED, READ_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE);
    if (rtn == RETCODE_NO_DATA)
    {
        printf("old data expired\n");
    }
    else
    {
        printf("read data error\n");
        return -1;
    }

    // 返还数据空间
    rtn = dr->return_loan(dataSeq, infoSeq);
    if (rtn != RETCODE_OK)
    {
        printf("return loan failed\n");
        return -1;
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

    rtn = TheParticipantFactory->finalize_instance();
    if (rtn != RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }

    ShapeTypeTypeSupport::finalize_instance();
    getchar();
    return 0;
}