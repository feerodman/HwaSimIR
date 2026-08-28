/**
* @file:       main_pub.cpp
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
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
        "PARTITIONQOS", 
        ShapeTypeTypeSupport::get_instance()->get_type_name(), 
        TOPIC_QOS_DEFAULT, 
        NULL, 
        STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }
    /*******************************分区配置1**********************************/

    //设置发布者qos
    PublisherQos pubQos1;
    rtn = dp->get_default_publisher_qos(pubQos1);
    if (rtn != RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    const char* part1 = "group1";
    pubQos1.partition.name.ensure_length(1,1);
    pubQos1.partition.name.set_at(0, part1);

    // 创建发布者
    Publisher* pub1 = dp->create_publisher(pubQos1, NULL, STATUS_MASK_NONE);
    if (pub1 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    DataWriter* _dw1 = pub1->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw1 = dynamic_cast<ShapeTypeDataWriter*>(_dw1);
    if (dw1 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    /*******************************分区配置2**********************************/

    //设置发布者qos
    PublisherQos pubQos2;
    rtn = dp->get_default_publisher_qos(pubQos2);
    if (rtn != RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    const char* part2 = "group2";
    pubQos2.partition.name.ensure_length(1, 1);
    pubQos2.partition.name.set_at(0, part2);

    // 创建发布者
    Publisher* pub2 = dp->create_publisher(pubQos2, NULL, STATUS_MASK_NONE);
    if (pub2 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    DataWriter* _dw2 = pub2->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw2 = dynamic_cast<ShapeTypeDataWriter*>(_dw2);
    if (dw2 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    /*******************************分区配置3**********************************/

    //设置发布者qos
    PublisherQos pubQos3;
    rtn = dp->get_default_publisher_qos(pubQos3);
    if (rtn != RETCODE_OK)
    {
        printf("get default publisher qos failed\n");
    }
    const char* part3 = "group3";
    pubQos3.partition.name.ensure_length(1, 1);
    pubQos3.partition.name.set_at(0, part3);

    // 创建发布者
    Publisher* pub3 = dp->create_publisher(pubQos3, NULL, STATUS_MASK_NONE);
    if (pub3 == NULL)
    {
        printf("create pub failed\n");
        return -1;
    }
    // 创建数据写者
    DataWriter* _dw3 = pub3->create_datawriter(tp, DATAWRITER_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    ShapeTypeDataWriter* dw3 = dynamic_cast<ShapeTypeDataWriter*>(_dw3);
    if (dw3 == NULL)
    {
        printf("create dw failed\n");
        return -1;
    }

    // 初始化数据
    ShapeType data;
    ShapeTypeInitialize(&data);
    data.x = 0;
    data.y = 0;
    strcpy(data.z, "PartitionQosPolicy");

    cout << "Please choose the group:" << endl;
    cout << "choose group 1: x = 0" << endl;
    cout << "choose group 2: x = 1" << endl;
    cout << "choose group 3: x = 2" << endl;

    int in;
    cin >> in;
    switch (in)
    {
    case 1:
    {
        // 循环发送数据
        while (true)
        {
            rtn = dw1->write(data, HANDLE_NIL_NATIVE);
            if (rtn != RETCODE_OK)
            {
                printf("write failed\n");
            }

            data.y++;

            ZRSleep(1000);
        }
        break;
    }
    case 2:
    {
        data.x = 1;
        // 循环发送数据
        while (true)
        {
            rtn = dw2->write(data, HANDLE_NIL_NATIVE);
            if (rtn != RETCODE_OK)
            {
                printf("write failed\n");
            }

            data.y++;

            ZRSleep(1000);
        }
        break;
    }
    case 3:
    {
        data.x = 2;
        // 循环发送数据
        while (true)
        {
            rtn = dw3->write(data, HANDLE_NIL_NATIVE);
            if (rtn != RETCODE_OK)
            {
                printf("write failed\n");
            }

            data.y++;

            ZRSleep(1000);
        }
        break;
    }
    default:
        printf("input error\n");
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

    ShapeTypeFinalize(&data);
    ShapeTypeTypeSupport::finalize_instance();
    return 0;
}