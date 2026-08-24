#include "ZRDDSCppSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

typedef struct ZRDDSDemoModules
{
    ZeroCopyBytesDataWriter* m_seaTrackDw;
}ZRDDSDemoModules;

ZRDDSDemoModules g_zrddsModule;

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DDSIF::Init(NULL, "rio_config");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 初始化使用rio的域参与者
    DomainParticipant* rioDp = DDSIF::CreateDP(100, "rio_dp");
    if (NULL == rioDp) { printf("DDSIF::CreateDP failed.\n"); return -3; }
    // 创建使用零拷贝数据类型的数据写者
    DataWriter* rawWriter = DDSIF::PubTopic(
        rioDp, "SeaTrack", ZeroCopyBytesTypeSupport::get_instance(), "zerocopy", NULL);
    g_zrddsModule.m_seaTrackDw = dynamic_cast<ZeroCopyBytesDataWriter*>(rawWriter);
    if (NULL == g_zrddsModule.m_seaTrackDw) { printf("Publisher_create_datawriter_with_qos_profile failed.\n"); return -4; }
    return 0;
}

#if defined(_VXWORKS) || defined(_REWORKS)
int dds_pub()
#else
int main()
#endif /* defined  */
{
    if (ZRDDSDemoModulesInitial() < 0) { getchar(); return -1; }
    ZeroCopyBytes* sample = DDSIF::ZeroCopyBytesCreate(4096);
    unsigned int i = 0;
    while (i++ < 20)
    {
        sprintf(sample->userBuffer, "sea track value(%u) from simpleInterfaceZeroCopyPub.", i);
        sample->userLength = strlen(sample->userBuffer) + 1;
        // 第一种发送数据方法，直接指定DataWrtier的指针
        ReturnCode_t retCode = g_zrddsModule.m_seaTrackDw->write(*sample, HANDLE_NIL_NATIVE);
        // 第二种发送数据的方法，指定域与主题名称
        //ReturnCode_t retCode = DDSIF::ZeroCopyBytesWrite(100, "SeaTrack", sample);
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
    DDSIF::ZeroCopyBytesDestroy(sample);
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