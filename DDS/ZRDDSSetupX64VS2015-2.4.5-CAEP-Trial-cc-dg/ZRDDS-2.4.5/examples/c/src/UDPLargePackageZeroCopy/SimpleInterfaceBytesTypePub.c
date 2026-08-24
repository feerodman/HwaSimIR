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
    /*** @brief   用于发送主题1数据。 */
    DDS_BytesDataWriter* m_airTrackDw;
}ZRDDSDemoModules;

/*** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_Init(NULL, "non_rio");
    if (factory == NULL) { printf("DDS_Initialize failed.\n"); return -1; }
    // 初始化使用udp的域参与者
    DDS_DomainParticipant* udpDp = DDS_CreateDP(150, "udp_dp");
    if (NULL == udpDp) { printf("DDS_CreateDP udp_dp failed.\n"); return -2; }
    // 创建非零拷贝数据写者
    g_zrddsModule.m_airTrackDw = (DDS_BytesDataWriter*)DDS_PubTopic(
        udpDp, "AirTrack", &DDS_BytesTypeSupport_instance, "best_effort_writer", NULL);
    if (NULL == g_zrddsModule.m_airTrackDw) { printf("DDS_PublishTopic failed.\n"); return -4; }
    return 0;
}

#if defined(_VXWORKS) || defined(_REWORKS)
int dds_pub()
#else
int main()
#endif /* defined  */
{
    if (ZRDDSDemoModulesInitial() < 0) { getchar(); return -1; }
    unsigned int length = 4096;
    char* buffer = (char*)malloc(length);
    unsigned int i = 0;
    while (i++ < 20)
    {
        // 零拷贝数据类型用户负载从sample->value+sample->reservedLength开始
        sprintf(buffer, "air track value %u from simpleInterfaceBytesTypePub_c.", i);
        // 第一种发送数据方法，直接指定DataWrtier的指针
        //DDS_Bytes sample;
        //DDS_BytesWrapper(&sample, buffer, length);
        //DDS_ReturnCode_t retCode = DDS_BytesDataWriter_write(g_zrddsModule.m_airTrackDw, &sample, &DDS_HANDLE_NIL_NATIVE);
        // 第二种发送数据的方法，指定域与主题名称
        DDS_ReturnCode_t retCode = DDS_BytesWrite(150, "AirTrack", buffer, length);
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
    free(buffer);
    DDS_Finalize();
    return 0;
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