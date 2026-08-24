#include "ZRDDSCSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/***
 * @typedef struct ZRDDSDemoModules
 *
 * @brief   用于管理DDS通信实体。
 */

typedef struct ZRDDSDemoModules
{
    /*** @brief   数据读者监听器。 */
    DDS_DataReaderListener m_listener;
}ZRDDSDemoModules;

/*** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

/**
 * @fn  DDS_SimpleDataReaderListener(BytesType, DDS_Bytes)(DDS_DataReader* reader, DDS_Bytes* sample, DDS_SampleInfo* info)
 *
 * @brief   定义类型 DDS_Bytes（非零拷贝缓冲区类型） 主题订阅者的回调函数，关联相同类型（可以是不同的主题）可以使用同一个回调函数，
 *          使用 `const DDS_Char* topicName = DDS_TopicDescription_get_name(DataReaderImplGetTopicDescription(reader));` 获取该数据所属主题。
 *
 * @param   第一个参数   为回调函数名称前缀，将自动生成 参数_on_data_avaliable 的函数，用于注册到DataReaderListener中；
 * @param   第二个参数   为关联的数据类型。
 *
 * @param   reader  指明哪个订阅者收到的样本。
 * @param   sample  收到的数据样本。
 * @param   info    收到的数据样本的元信息，包括时间戳、来源等，通常可以不关注。
 */
DDS_SimpleDataReaderListener(BytesType, DDS_Bytes)(DDS_DataReader* reader, DDS_Bytes* sample, DDS_SampleInfo* info)
{
    // 通过该语句获取收到的样本所属主题名称
    const DDS_Char* topicName = DDS_TopicDescription_get_name(DataReaderImplGetTopicDescription(reader));
    // 非零拷贝（缓冲区类型）按照如下语句获取缓冲区指针与长度
    const char* buffer = (const char*)DDS_OctetSeq_get_contiguous_buffer(&sample->value);
    const unsigned int length = sample->value._length;
    // TODO 在此填写业务处理
    printf("received data(%s) length(%u) from topic(%s)\n", buffer, length, topicName);
}

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_Init(NULL, "non_rio");
    if (factory == NULL) { printf("DDS_DomainParticipantFactory_get_instance failed.\n"); return -1; }
    // 初始化使用udp的域参与者
    DDS_DomainParticipant* udpDp = DDS_CreateDP(150, "udp_dp");
    if (NULL == udpDp) { printf("DDS_DomainParticipantFactory_create_participant_with_qos_profile failed.\n"); return -2; }
    // 创建非零拷贝（缓冲区类型）数据读者，并设置监听器
    memset(&g_zrddsModule.m_listener, 0, sizeof(g_zrddsModule.m_listener));
    g_zrddsModule.m_listener.on_data_available = BytesType_on_data_available;
    DDS_DataReader* airTrackDr = DDS_SubTopic(
        udpDp, "AirTrack", &DDS_BytesTypeSupport_instance, "non_zerocopy_reliable", &g_zrddsModule.m_listener);
    if (NULL == airTrackDr) { printf("DDS_Subscriber_create_datareader_with_qos_profile failed.\n"); return -4; }
    return 0;
}

#if defined(_VXWORKS) || defined(_REWORKS)
int dds_sub()
#else
int main()
#endif /* defined  */
{
    if (ZRDDSDemoModulesInitial() < 0) { getchar(); return -1; }
    // 等待数据
    getchar();
#if defined(_VXWORKS) || defined(_REWORKS)
    while(true)
    {
        ZRSleep(1000);
    }
#endif
    DDS_Finalize();
}

#if defined(_VXWORKS)
#include <taskLib.h>
#include <sysLib.h>
#include <tickLib.h>
#include <time.h>
int start_dds_sub()
{    
    // 初始化vxWorks平台的tick(计时)
    //sysClkRateSet(1000);
    taskSpawn("dds_sub", 128,
        VX_FP_TASK,
        256 * 1024,
        (FUNCPTR)dds_sub,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    printf("taskSpawn\n");
    return 0;
}

#elif defined(_REWORKS)
#include <pthread.h>
void start_dds_sub()
{   
    pthread_t tid;
    pthread_create2(
        &tid,
        "dds_sub",
        128,
        RE_FP_TASK,
        256 * 1024,
        dds_sub,
        NULL);
}
#endif
