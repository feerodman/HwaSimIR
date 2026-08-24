/**
* @file:       main.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeTypeSequence.h"
#include "ShapeTypeSequenceTypeSupport.h"
#include "ShapeTypeSequenceDataWriter.h"
#include "ShapeTypeSequenceDataReader.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    int i;
    unsigned int j;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Publisher* pub;
    DDS_DataWriter* _dw;
    ShapeTypeSequenceDataWriter* dw;
    DDS_Subscriber* sub;
    DDS_DataReader* _dr;
    ShapeTypeSequenceDataReader* dr;
    ShapeTypeSequence data;
    ShapeTypeSequenceSeq dataSeq;
    DDS_SampleInfoSeq infoSeq; 
    const char array[] = "this is char sequence";
    char tmp[25];
    long longValue;
    double doubleValue;

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
    rtn = ShapeTypeSequenceTypeSupport_register_type(dp, NULL);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(
        dp, 
        "USESEQUENCEDATATYPE", 
        ShapeTypeSequenceTypeSupport_get_type_name(), 
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

    // 创建数据写者
    _dw = DDS_Publisher_create_datawriter(
        pub, tp, &DDS_DATAWRITER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dw = (ShapeTypeSequenceDataWriter*)(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    //创建订阅者
    sub = DDS_DomainParticipant_create_subscriber(
        dp, &DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建数据读者
    _dr = DDS_Subscriber_create_datareader(
        sub, (DDS_TopicDescription*)tp, &DDS_DATAREADER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    dr = (ShapeTypeSequenceDataReader*)(_dr);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 初始化数据
    ShapeTypeSequenceInitialize(&data);

    //设置sequence数据类型数据长度和值
    DDS_LongSeq_ensure_length(&data.x, 10, 10);
    DDS_DoubleSeq_ensure_length(&data.y, 10, 10);
    for (i = 0; i < 10; i++)
    {
        longValue = i;
        doubleValue = i;
        //因为c语言不支持[]形式，所以第一组数据使用set方法
        DDS_LongSeq_set(&data.x, i, &longValue);     
        DDS_DoubleSeq_set(&data.y, i, &doubleValue);
    }

    DDS_CharSeq_from_array(&data.z, array, strlen(array) + 1);

    ShapeTypeSequenceSeq_initialize(&dataSeq);
    DDS_SampleInfoSeq_initialize(&infoSeq);

    //等待匹配
    ZRSleep(3000);

    //发送sequence类型数据
    rtn = ShapeTypeSequenceDataWriter_write(dw, &data, &DDS_HANDLE_NIL_NATIVE);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dw write failed\n");
        return -1;
    }

    //等待发送
    ZRSleep(300);

    // 取出接收数据
    rtn = ShapeTypeSequenceDataReader_take(
        dr, 
        &dataSeq, 
        &infoSeq, 
        LENGTH_UNLIMITED,
        DDS_ANY_SAMPLE_STATE, 
        DDS_ANY_VIEW_STATE, 
        DDS_ANY_INSTANCE_STATE);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("take data failed\n");
        return -1;
    }

    // 遍历读取接收数据并验证内容
    for (i = 0; i < DDS_SampleInfoSeq_get_length(&infoSeq); ++i)
    {        
        // 在使用数据之前，应检查数据的有效性
        if (!DDS_SampleInfoSeq_get_reference(&infoSeq, i)->valid_data)
        {
            continue;
        }
        
            // 验证接收的sequence数据长度和内容均正确
        if (DDS_LongSeq_get_length(&ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->x) != 10)
        {
            printf("take data x failed\n");
            return -1;
        }
        for (j = 0; j < DDS_LongSeq_get_length(
                &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->x); j++)
        {
            longValue = j;
            if (*DDS_LongSeq_get_reference(
                    &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->x, j) != longValue)
            {
                printf("data error:actual value = %d\n", *DDS_LongSeq_get_reference(
                    &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->x, j));
                return -1;
            }
        }

        if (DDS_DoubleSeq_get_length(&ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->y) != 10)
        {
            printf("take data y failed\n");
            return -1;
        }
        for (j = 0; j < DDS_DoubleSeq_get_length(
                &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->y); j++)
        {
            doubleValue = j;
            if (*DDS_DoubleSeq_get_reference(
                    &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->y, j) != doubleValue)
            {
                printf("data error: actual value = %f\n", *DDS_DoubleSeq_get_reference(
                    &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->y, j));
                return -1;
            }
        }
        DDS_CharSeq_to_array(
            &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->z, tmp, DDS_CharSeq_get_length(
                &ShapeTypeSequenceSeq_get_reference(&dataSeq, i)->z));
        
        if (strcmp(tmp, array))
        {
            printf("data error\n");
            return -1;
        }

        // 打印接收数据
        ShapeTypeSequencePrintData(ShapeTypeSequenceSeq_get_reference(&dataSeq, i));
    }

    // 返还数据空间
    rtn = ShapeTypeSequenceDataReader_return_loan(dr, &dataSeq, &infoSeq);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("return loan failed\n");
        return -1;
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

    rtn = DDS_DomainParticipantFactory_finalize_instance();
    if (rtn != DDS_RETCODE_OK)
    {
        printf("dpf finalize instance failed\n");
        return -1;
    }

    ShapeTypeSequenceFinalize(&data);
    getchar();
    return 0;
}