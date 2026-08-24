/**
* @file:       main.c
*
* copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
*/

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h" 
#include "Topic.h"
#include "DefaultQos.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#elif defined(__linux__) || defined(linux) || defined(__linux)
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#endif // _WIN32

#ifdef _WIN32

//信号量资源
/// * @brief    信号量
typedef HANDLE SEMA;
/// * @brief    一直阻塞地进行等待信号量
#define SEMA_WAIT(sema) WaitForSingleObject(sema, INFINITE)
/// * @brief    释放信号量
#define SEMA_POST(sema) ReleaseSemaphore(sema, 1, NULL)
/// * @brief    销毁信号量
#define SEMA_DESTROY(sema) CloseHandle(sema)
/// * @brief    初始化信号量， 输入的为：信号量的最大值，初始信号量个数
#define SEMA_INIT(sema, initCount, maxCount) sema = CreateSemaphore(NULL, initCount, maxCount, NULL)

#elif defined(__linux__) || defined(linux) || defined(__linux)

#define SEMA sem_t*
#define SEMA_WAIT(sema) sem_wait(sema)
#define SEMA_POST(sema) sem_post(sema)
#define SEMA_DESTROY(sema)  sem_destroy(sema); free(sema)
#define SEMA_INIT(sema, initCount, maxCount) sema = malloc(sizeof(sem_t)); sem_init(sema,0,initCount)

#endif // _WIN32

SEMA mThread_sema_1;
SEMA mThread_sema_2;

#ifdef _WIN32
void WINAPI mThread1(DDS_DomainParticipant* dp)
#else
void mThread1(DDS_DomainParticipant* dp)
#endif
{
    DDS_Topic* tp;
    DDS_ReturnCode_t rtn;

    // 创建主题
    tp = DDS_DomainParticipant_create_topic(dp, "FIND_TOPIC",
        ShapeTypeTypeSupport_get_type_name(), &DDS_TOPIC_QOS_DEFAULT, NULL, DDS_STATUS_MASK_NONE);
    if (tp == NULL)
    {
        printf("create tp failed\n");
        return -1;
    }

    // 使用该主题，例：创建数据写者、数据读者
    ZRSleep(3000);

    // 删除主题
    rtn = DDS_DomainParticipant_delete_topic(dp, tp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("delete tp failed\n");
    }

    // 线程执行结束后挂起信号量
    SEMA_POST(mThread_sema_1);
}

#ifdef _WIN32
void WINAPI mThread2(DDS_DomainParticipant* dp)
#else
void mThread2(DDS_DomainParticipant* dp)
#endif
{
    DDS_Topic* tp;
    DDS_ReturnCode_t rtn;
    DDS_Duration_t ti;

    ti.sec = 2;
    ti.nanosec = 0;

    // 通过find_topic方法“发现”主题
    tp = DDS_DomainParticipant_find_topic(dp, "FIND_TOPIC", &ti);
    if (tp == NULL)
    {
        printf("find tp failed\n");
        return;
    }

    // 使用该主题，例：创建数据写者、数据读者
    ZRSleep(3000);

    // 由于在本线程通过find_topic“发现”主题，故应在线程结束时，删除该主题一次
    rtn = DDS_DomainParticipant_delete_topic(dp, tp);
    if (rtn != DDS_RETCODE_OK)
    {
        printf("delete topic failed\n");
    }

    // 线程执行结束后挂起信号量
    SEMA_POST(mThread_sema_2);
}

int main(int argc, char** argv)
{
    // 域号
    const int domain_id = 80;  
    DDS_ReturnCode_t rtn;
    DDS_DomainParticipantFactory *factory;
    DDS_DomainParticipant* dp;
#ifdef _WIN32
    uintptr_t retCode;
    unsigned int threadId;
#elif defined(__linux__) || defined(linux) || defined(__linux)
    pthread_t threadId;
#endif // _WIN32

    SEMA_INIT(mThread_sema_1, 0, 1);
    SEMA_INIT(mThread_sema_2, 0, 1);

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
    rtn = ShapeTypeTypeSupport_register_type(dp, ShapeTypeTypeSupport_get_type_name());
    if (rtn != DDS_RETCODE_OK)
    {
        printf("register type failed\n");
        return -1;
    }

    // 启动两个线程
#ifdef _WIN32
    retCode = _beginthreadex(NULL, 0, mThread1, dp, 0, &threadId);
    if (retCode == 0 || retCode == -1)
    {
        printf("_beginthreadex failed errno(%d).\n", errno);
        return -1;
    }

    retCode = _beginthreadex(NULL, 0, mThread2, dp, 0, &threadId);
    if (retCode == 0 || retCode == -1)
    {
        printf("_beginthreadex failed errno(%d).\n", errno);
        return -1;
    }
#elif defined(__linux__) || defined(linux) || defined(__linux)
    if (0 != pthread_create(&threadId, NULL, mThread1, dp))
    {
        printf("pthread_create failed errno(%d).\n", errno);
        return -1;
    }
    if (0 != pthread_create(&threadId, NULL, mThread2, dp))
    {
        printf("pthread_create failed errno(%d).\n", errno);
        return -1;
    }
#endif // _WIN32

    // 阻塞至线程结束
    SEMA_WAIT(mThread_sema_1);
    SEMA_WAIT(mThread_sema_2);

    getchar();

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
    SEMA_DESTROY(mThread_sema_1);
    SEMA_DESTROY(mThread_sema_2);
    return 0;
}