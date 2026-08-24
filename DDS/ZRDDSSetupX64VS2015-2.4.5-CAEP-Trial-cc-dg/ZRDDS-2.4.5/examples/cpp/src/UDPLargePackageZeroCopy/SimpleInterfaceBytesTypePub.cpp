#include "ZRDDSCppSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

typedef struct ZRDDSDemoModules
{
    BytesDataWriter* m_airTrackDw;
    DataReaderListener* m_listener;
}ZRDDSDemoModules;

ZRDDSDemoModules g_zrddsModule;

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DDSIF::Init(NULL, "non_rio");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 初始化使用udp的域参与者
    DomainParticipant* udpDp = DDSIF::CreateDP(150, "udp_dp");
    if (NULL == udpDp) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建非零拷贝数据写者
    DataWriter* rawWriter = DDSIF::PubTopic(
        udpDp, "AirTrack", BytesTypeSupport::get_instance(), "best_effort_writer", NULL);
    g_zrddsModule.m_airTrackDw = dynamic_cast<BytesDataWriter*>(rawWriter);
    if (NULL == g_zrddsModule.m_airTrackDw) { printf("DDSIF::PublishTopic failed.\n"); return -4; }
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
        sprintf(buffer, "air track value %u from simpleInterfaceBytesTypePub.", i);
        // 第一种发送数据方法，直接指定DataWrtier的指针
        //Bytes sample;
        //DDSIF::BytesWrapper(sample, buffer, length);
        //ReturnCode_t retCode = g_zrddsModule.m_airTrackDw->write(sample, HANDLE_NIL_NATIVE);
        // 第二种发送数据的方法，指定域与主题名称
        ReturnCode_t retCode = DDSIF::BytesWrite(150, "AirTrack", buffer, length);
        if (retCode != RETCODE_OK)
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
    DDSIF::Finalize();
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
extern "C" void start_dds_pub()
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