/**
* @file:       main.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "ShapeTypeSequence.h"
#include "ShapeTypeSequenceTypeSupport.h"
#include "ShapeTypeSequenceDataWriter.h"
#include "ShapeTypeSequenceDataReader.h"
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

    // 创建域参与者
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }

    // 注册数据类型
    rtn = ShapeTypeSequenceTypeSupport::get_instance()->register_type(dp, NULL);
    if (rtn != RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    Topic* tp = dp->create_topic(
        "USESEQUENCEDATATYPE", 
        ShapeTypeSequenceTypeSupport::get_instance()->get_type_name(), 
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

    // 创建数据写者
    DataWriter* _dw = pub->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeSequenceDataWriter* dw = dynamic_cast<ShapeTypeSequenceDataWriter*>(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    //创建订阅者
    Subscriber* sub = dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建数据读者
    DataReader* _dr = sub->create_datareader(tp, DATAREADER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeSequenceDataReader* dr = dynamic_cast<ShapeTypeSequenceDataReader*>(_dr);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeSequence data;
    ShapeTypeSequenceInitialize(&data);

    //设置sequence数据类型数据长度和值
    data.x.ensure_length(10, 10);
    data.y.ensure_length(10, 10);

    for (int i = 0; i < 10; i++)
    {
        data.x[i] = i;
        data.y.set_at(i, i);
    }

    const char array[] = "this is char sequence";
    data.z.from_array(array, strlen(array)+1);

    ShapeTypeSequenceSeq dataSeq;
    SampleInfoSeq infoSeq;

    //等待匹配
    ZRSleep(3000);

    //发送sequence类型数据
    rtn = dw->write(data, HANDLE_NIL_NATIVE);
    if (rtn != RETCODE_OK)
    {
        printf(" dw write failed\n");
        return -1;
    }

    //等待发送
    ZRSleep(300);

    // 取出接收数据
    rtn = dr->take(dataSeq, infoSeq, LENGTH_UNLIMITED, ANY_SAMPLE_STATE, ANY_VIEW_STATE, ANY_INSTANCE_STATE);
    if (rtn != RETCODE_OK)
    {
        printf("take data failed\n");
        return -1;
    }

    // 遍历读取接收数据并验证内容
    for (unsigned int i = 0; i < infoSeq.length(); ++i)
    {
        // 在使用数据之前，应检查数据的有效性
        if (!infoSeq[i].valid_data)
        {
            continue;
        }

        // 验证接收的sequence数据长度和内容均正确
        if (dataSeq[i].x.length() != 10)
        {
            printf("take data x failed\n");
            return -1;
        }
        for (int j = 0; j < dataSeq[i].x.length(); j++)
        {
            if (dataSeq[i].x[j] != j)
            {
                printf("data error:actual value = %d\n", dataSeq[i].x[j]);
                return -1;
            }
        }

        if (dataSeq[i].y.length() != 10)
        {
            printf("take data y failed\n");
            return -1;
        }
        for (int j = 0; j < dataSeq[i].y.length(); j++)
        {
            if (dataSeq[i].y.get_at(j) != j)
            {
                printf("data error: actual value = %d\n", dataSeq[i].y.get_at(j));
                return -1;
            }
        }

        char tmp[25];
        dataSeq[i].z.to_array(tmp, dataSeq[i].z.length());

        if (strcmp(tmp, array))
        {
            printf("data error\n");
            return -1;
        }

        // 打印接收数据
        ShapeTypeSequencePrintData(&dataSeq[i]);
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

    ShapeTypeSequenceFinalize(&data);
    ShapeTypeSequenceTypeSupport::finalize_instance();
    getchar();
    return 0;    
}