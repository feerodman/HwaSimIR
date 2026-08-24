/**
* @file:       main_sub.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Subscriber.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataReader.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory* factory;
    DDS_DomainParticipant* dp;
    DDS_Topic* tp;
    DDS_Subscriber* sub;
    DDS_DataReader* dr;
    DDS_InstanceHandleSeq handleSeq;
    DDS_PublicationBuiltinTopicData publicationData;

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
        "GETMATCHEDSTATUS", 
        ShapeTypeTypeSupport_get_type_name(), 
        &DDS_TOPIC_QOS_DEFAULT, 
        NULL, 
        DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 创建订阅者
    sub = DDS_DomainParticipant_create_subscriber(
        dp, &DDS_SUBSCRIBER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (sub == NULL)
    {
        printf("create sub failed\n");
        return -1;
    }

    // 创建数据读者
    dr = DDS_Subscriber_create_datareader(
        sub, tp, &DDS_DATAREADER_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (dr == NULL)
    {
        printf("create dr failed\n");
        return -1;
    }

    // 获取reader匹配的writer信息
    while (true)
    {
        int i;
        //获取所有匹配的wirter句柄
        DDS_InstanceHandleSeq_initialize(&handleSeq);
        ShapeTypeDataReader_get_matched_publications((ShapeTypeDataReader*)dr, &handleSeq);
        printf("当前匹配%d个数据写者\n", DDS_InstanceHandleSeq_get_length(&handleSeq));

        //获取句柄对应的详细信息
        for (i = 0; i < DDS_InstanceHandleSeq_get_length(&handleSeq); i++)
        {
            DDS_PublicationBuiltinTopicDataInitial(&publicationData);
            ShapeTypeDataReader_get_matched_publication_data(
                (ShapeTypeDataReader*)dr, 
                &publicationData, 
                DDS_InstanceHandleSeq_get_reference(&handleSeq, i));
            
            //通过publicationData可以获取匹配的数据写者的详细信息
            printf("publicationData_id = 0x%08X%08X%08X%08X\n\n",
                publicationData.key[0], publicationData.key[1],
                publicationData.key[2], publicationData.key[3]);
        }
        ZRSleep(2000);
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
    DDS_DomainParticipantFactory_finalize_instance();
    return 0;
}