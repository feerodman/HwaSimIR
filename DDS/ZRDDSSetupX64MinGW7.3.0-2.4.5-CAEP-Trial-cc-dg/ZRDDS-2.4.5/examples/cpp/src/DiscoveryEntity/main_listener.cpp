/**
* @file:       main_listener.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Descrption:  先启动Listener，再启动Entity
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "BuiltinDataDataReader.h"

#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class dwBuiltListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {
        PublicationBuiltinTopicDataDataReader* dr = 
            dynamic_cast<PublicationBuiltinTopicDataDataReader*>(reader);
        PublicationBuiltinTopicDataSeq dataSeq;
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
        for (unsigned int i = 0; i < dataSeq.length(); ++i)
        {
            // 验证远端数据写者是否上线
            if (!infoSeq[i].valid_data)
            {
                ZR_UINT32 key_arry[4];
                memcpy(key_arry, infoSeq[i].instance_handle.value, 4 * sizeof(ZR_UINT32));

                printf("数据写者下线！\n");
                printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n",
                    key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
            }
            else
            {
                printf("数据写者上线！\n");
                printf("DataWriter_id = 0x%08X%08X%08X%08X\n\n",
                    dataSeq[i].key[0], dataSeq[i].key[1], dataSeq[i].key[2], dataSeq[i].key[3]);
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

class drBuiltListener : public DataReaderListener
{
    void on_data_available(DataReader *reader)
    {
        SubscriptionBuiltinTopicDataDataReader* dr = 
            dynamic_cast<SubscriptionBuiltinTopicDataDataReader*>(reader);
        SubscriptionBuiltinTopicDataSeq dataSeq;
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
        for (unsigned int i = 0; i < dataSeq.length(); ++i)
        {
            // 验证远端数据读者是否上线
            if (!infoSeq[i].valid_data)
            {
                ZR_UINT32 key_arry[4];
                memcpy(key_arry, infoSeq[i].instance_handle.value, 4 * sizeof(ZR_UINT32));

                printf("数据读者下线！\n");
                printf("DataReader_id = 0x%08X%08X%08X%08X\n\n",
                    key_arry[0], key_arry[1], key_arry[2], key_arry[3]);
            }
            else
            {
                printf("数据读者上线！\n");
                printf("DataReader_id = 0x%08X%08X%08X%08X\n\n",
                    dataSeq[i].key[0], dataSeq[i].key[1], dataSeq[i].key[2], dataSeq[i].key[3]);
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

    //设置DomainParticipantFactory的qos为未使能
    DomainParticipantFactoryQos dpfQos;
    TheParticipantFactory->get_qos(dpfQos);
    dpfQos.entity_factory.autoenable_created_entities = false;
    TheParticipantFactory->set_qos(dpfQos);

    // 创建域参与者,域参与者为未使能状态
    DomainParticipant* dp = TheParticipantFactory->create_participant(
        DomainId_t(domain_id), DOMAINPARTICIPANT_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (dp == NULL)
    {
        printf("create dp failed\n");
        return -1;
    }
    
    //获取内置的订阅者
    Subscriber* builtSub = dp->get_builtin_subscriber();
    if (builtSub == NULL)
    {
        printf("get builtin subscriber failed\n");
        return -1;
    }

    // 查找内置的PublicationBuiltinTopicDataDataReader
    DataReader* pubDr = builtSub->lookup_datareader(BUILTIN_PUBLICATION_TOPIC_NAME);
    if (pubDr == NULL)
    {
        printf("get builtin pubDr failed\n");
        return -1;
    }

    //设置监听器
    dwBuiltListener dwListener;
    rtn = pubDr->set_listener(&dwListener, STATUS_MASK_ALL);
    if (rtn != RETCODE_OK)
    {
        printf("set listener failed\n");
        return -1;
    }

    // 查找内置的SubscriptionBuiltinTopicDataDataReader
    DataReader* subDr = builtSub->lookup_datareader(BUILTIN_SUBSCRIPTION_TOPIC_NAME);
    if (subDr == NULL)
    {
        printf("get builtin subDr failed\n");
        return -1;
    }

    //设置监听器
    drBuiltListener drListener;
    rtn = subDr->set_listener(&drListener, STATUS_MASK_ALL);
    if (rtn != RETCODE_OK)
    {
        printf("set listener failed\n");
        return -1;
    }

    //使能域参与者
    rtn = dp->enable();
    if (rtn != RETCODE_OK)
    {
        printf("enable dp failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听回调
    while (true)
    {
        ZRSleep(5000);
        printf("wait for receive data\n");
    }

    //还原DomainParticipantFactoryQos
    dpfQos.entity_factory.autoenable_created_entities = true;
    TheParticipantFactory->set_qos(dpfQos);

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
    return 0;
}