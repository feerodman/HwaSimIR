/**
* @file:       main_one.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动one，再启动other
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "BuiltinDataDataReader.h"
#include "DataReaderListener.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class dpDiscoveryListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {
        ParticipantBuiltinTopicDataDataReader* dr = 
            dynamic_cast<ParticipantBuiltinTopicDataDataReader*>(reader);
        ParticipantBuiltinTopicDataSeq dataSeq;
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
            if (infoSeq[i].valid_data)
            {
                printf("discovery domainparticipant online\n");
                if (dataSeq[i].user_data.value.length() != 0)
                {
                    // 获取并打印user_data
                    char userData[100];
                    dataSeq[i].user_data.value.to_array(userData, dataSeq[i].user_data.value.length());
                    printf("user_data : %s\n", userData);
                }
            }
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
        return - 1;
    }

    //获取内置的subscriber
    Subscriber *sub = dp->get_builtin_subscriber();
    if (sub == NULL)
    {
        printf("get builtin sub failed\n");
        return -1;
    }

    //获取内置的datareader
    DataReader *reader = sub->lookup_datareader("BUILTIN_PARTICIPANT_TOPIC_NAME");
    if (reader == NULL)
    {
        printf("get builtin reader failed\n");
        return -1;
    }

    // 创建监听器
    dpDiscoveryListener dpListener;

    //设置内置datareader的监听器
    reader->set_listener(&dpListener, DATA_AVAILABLE_STATUS);

    // 阻塞主线程，通过Listener监听参与者上下线
    while (true)
    {
        ZRSleep(5000);
        printf("wait discovery domainparticipant\n");
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

    TheParticipantFactory->finalize_instance();
    return 0;
}