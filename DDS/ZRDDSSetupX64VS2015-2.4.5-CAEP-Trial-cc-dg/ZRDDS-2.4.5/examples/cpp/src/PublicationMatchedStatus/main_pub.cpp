/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*
* Description：先启动发送端（Publication）程序，再启动接收端（Subscription）程序
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "Publisher.h"
#include "ShapeType.h"
#include "ShapeTypeTypeSupport.h"
#include "ShapeTypeDataWriter.h"
#include <iostream>
#include <string.h>

using namespace::DDS;
using namespace::std;

class tDataWriterListener : public DataWriterListener
{
    void on_publication_matched(DataWriter *the_writer, const PublicationMatchedStatus &status)
    {
        ZR_UINT32 key_arry[4];
        memcpy(key_arry, status.last_subscription_handle.value, 4 * sizeof(ZR_UINT32));

        //已获取匹配状态
        printf("publication matched success!\n");

        //通过PublicationMatchedStatus可以获取匹配的数据读者的状态信息
        printf("PublicationMatchedStatus total_count:%d\n", status.total_count);
        printf("PublicationMatchedStatus total_count_change:%d\n",status.total_count_change);
        printf("PublicationMatchedStatus current_count:%d\n",status.current_count);
        printf("PublicationMatchedStatus current_count_change:%d\n",status.current_count_change);
        printf("Subscription_id = 0x%08X%08X%08X%08X\n", 
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
        "PUBLICATIONMATCHEDSTATUS", 
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

    // 创建监听器
    tDataWriterListener tlistener;

    // 创建数据写者
    DataWriter* _dw = pub->create_datawriter(
        tp, DATAWRITER_QOS_DEFAULT, &tlistener, PUBLICATION_MATCHED_STATUS);
    ShapeTypeDataWriter* dw = dynamic_cast<ShapeTypeDataWriter*>(_dw);
    if (dw == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 阻塞主线程，通过Listener监听回调
    while (true)
    {
        ZRSleep(5000);
        printf("wait for publication matched\n");
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
    return 0;
}