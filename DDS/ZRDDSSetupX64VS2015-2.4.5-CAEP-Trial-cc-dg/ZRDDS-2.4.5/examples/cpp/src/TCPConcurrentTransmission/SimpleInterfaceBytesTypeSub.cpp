#include "ZRDDSCppSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

typedef struct ZRDDSDemoModules
{
    DataReaderListener* m_listener;
}ZRDDSDemoModules;

ZRDDSDemoModules g_zrddsModule;

/* 定义非零拷贝（缓冲区数据类型）回调接口 */
class BytesTypeListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    virtual void on_process_sample(DataReader* reader, const Bytes& sample, const SampleInfo& info)
    {
        // 通过该语句获取收到的样本所属主题名称
        const char* topicName = reader->get_topicdescription()->get_name();
        // 非零拷贝（缓冲区类型）按照如下语句获取缓冲区指针与长度
        const char* buffer = (const char*)sample.value.get_contiguous_buffer();
        const unsigned int length = sample.value.length();
        // TODO 在此填写业务处理
        printf("received data(%s) length(%u) from topic(%s)\n", buffer, length, topicName);
    }
};

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DDSIF::Init(NULL, "tcp_mul_dpf");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 初始化使用udp的域参与者
    DomainParticipant* tcpDp = DDSIF::CreateDP(150, "tcp_mul_dp");
    if (NULL == tcpDp) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建非零拷贝（缓冲区类型）数据读者，并设置监听器
    g_zrddsModule.m_listener = new BytesTypeListener();
    DataReader* airTrackDr = DDSIF::SubTopic(
        tcpDp, "AirTrack", BytesTypeSupport::get_instance(), "tcp_mul_dr", g_zrddsModule.m_listener);
    if (NULL == airTrackDr) { printf("DDSIF::SubscribeTopic failed.\n"); return -4; }
    return 0;
}

#if defined(_VXWORKS) || defined(_REWORKS)
int dds_sub()
#else
int main()
#endif /* defined  */
{
    if (ZRDDSDemoModulesInitial() < 0) { getchar(); return -1; }
    getchar();
#if defined(_VXWORKS) || defined(_REWORKS)
    while(true)
    {
        ZRSleep(1000);
    }
#endif
    DDSIF::Finalize();
    return 0;
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
extern "C" void start_dds_sub()
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