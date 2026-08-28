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
    DDS_ZeroCopyBytesDataWriter* m_seaTrackDw;
}ZRDDSDemoModules;

/*** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_Init(NULL, "tcp_mul_dpf");
    if (factory == NULL) { printf("DDS_DomainParticipantFactory_get_instance failed.\n"); return -1; }
    // 初始化使用rio的域参与者
    DDS_DomainParticipant* tcpDp = DDS_CreateDP(100, "tcp_mul_dp");
    if (NULL == tcpDp) { printf("DDS_DomainParticipantFactory_create_participant_with_qos_profile failed.\n"); return -5; }
    // 创建使用零拷贝数据类型的数据写者
    g_zrddsModule.m_seaTrackDw = (DDS_ZeroCopyBytesDataWriter*)DDS_PubTopic(
       tcpDp, "SeaTrack", &DDS_ZeroCopyBytesTypeSupport_instance, "tcp_mul_dw", NULL);
    if (NULL == g_zrddsModule.m_seaTrackDw) { printf("DDS_Publisher_create_datawriter_with_qos_profile failed.\n"); return -4; }
    return 0;
}

#if defined(_VXWORKS) || defined(_REWORKS)
int dds_pub()
#else
int main()
#endif /* defined  */
{
    if (ZRDDSDemoModulesInitial() < 0) { getchar(); return -1; }
    DDS_ZeroCopyBytes* sample = DDS_ZeroCopyBytesCreate(4096);
    unsigned int i = 0;
    while (i++ < 20)
    {
        // 零拷贝数据类型用户负载从sample->value+sample->reservedLength开始
        sprintf(sample->userBuffer, "sea track value %u from simpleInterfaceZeroCopyPub_c.", i);
        sample->userLength = strlen(sample->userBuffer) + 1;
        // 第一种发送数据方法，直接指定DataWrtier的指针
        DDS_ReturnCode_t retCode = DDS_ZeroCopyBytesDataWriter_write(g_zrddsModule.m_seaTrackDw, sample, &DDS_HANDLE_NIL_NATIVE);
        // 第二种发送数据的方法，指定域与主题名称
        //DDS_ReturnCode_t retCode = DDS_ZeroCopyBytesWrite(100, "SeaTrack", sample);
        if (retCode != DDS_RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        else
        {
            printf("send a data.\n");
        }
        ZRSleep(1000);
    }
    DDS_ZeroCopyBytesDestroy(sample);
    DDS_Finalize(&g_zrddsModule);
}

#if defined(_VXWORKS)
#include <taskLib.h>
#include <sysLib.h>
#include <tickLib.h>
#include <time.h>
int start_dds_pub()
{    
    // 初始化vxWorks平台的tick(计时)
    //sysClkRateSet(1000);
    taskSpawn("dds_pub", 128,
        VX_FP_TASK,
        256 * 1024,
        (FUNCPTR)dds_pub,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    printf("taskSpawn\n");
    return 0;
}

#elif defined(_REWORKS)
#include <pthread.h>
void start_dds_pub()
{   
    pthread_t tid;
    pthread_create2(
        &tid,
        "dds_pub",
        128,
        RE_FP_TASK,
        256 * 1024,
        dds_pub,
        NULL);
}
#endif