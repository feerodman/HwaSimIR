#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "ZRBuiltinTypesDataWriter.h"
#include "ZRBuiltinTypesDataReader.h"
#include "ZRBuiltinTypesTypeSupport.h"
#include "DefaultQos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

/**
 * @typedef struct ZRDDSDemoModules
 *
 * @brief   用于管理DDS通信实体。
 */

typedef struct ZRDDSDemoModules
{
    /** @brief   用于UDP通信的实体。 */
    DomainParticipant* m_udpDp;
    /** @brief   使用RIO通信的实体。 */
    DomainParticipant* m_rioDp;
    /** @brief   用于接收主题2数据。 */
    ZeroCopyBytesDataReader* m_seaTrackDr;
    /** @brief   用于发送主题1数据。 */
    ZeroCopyBytesDataWriter* m_airTrackDw;
    /** @brief   数据读者监听器。 */
    DataReaderListener* m_listener;
}ZRDDSDemoModules;

/** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

/* 实现数据读者回调接口 */
class SeaTrackTopicListener : public SimpleDataReaderListener<ZeroCopyBytes, ZeroCopyBytesSeq, ZeroCopyBytesDataReader>
{
public:
    virtual void on_process_sample(DataReader* reader, const ZeroCopyBytes& sample, const SampleInfo& info)
    {
        printf("received data %s\n", sample.value);
    }
};

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DomainParticipantFactory::get_instance_w_profile(NULL, "lib1", "profile1", "app1");
    if (factory == NULL)
    {
        printf("DomainParticipantFactory::get_instance failed.\n");
        return -1;
    }
    // 初始化使用udp的域参与者
    g_zrddsModule.m_udpDp = factory->create_participant_with_qos_profile(150, "lib1", "profile1", "udp_dp", NULL, STATUS_MASK_NONE);
    if (NULL == g_zrddsModule.m_udpDp)
    {
        printf("DomainParticipantFactory_create_participant_with_qos_profile failed.\n");
        return -2;
    }
    // 初始化使用rio的域参与者
    g_zrddsModule.m_rioDp = factory->create_participant_with_qos_profile(100, "lib1", "profile1", "rio_dp", NULL, STATUS_MASK_NONE);
    if (NULL == g_zrddsModule.m_rioDp)
    {
        printf("DomainParticipantFactory_create_participant_with_qos_profile failed.\n");
        return -5;
    }
    // 创建数据写者
    DataWriter* rawWriter = g_zrddsModule.m_udpDp->create_datawriter_with_topic_and_qos_profile(
        "AirTrack", ZeroCopyBytesTypeSupport::get_instance(), "lib1", "profile1", "AirTrack", NULL, STATUS_MASK_NONE);
    g_zrddsModule.m_airTrackDw = dynamic_cast<ZeroCopyBytesDataWriter*>(rawWriter);
    if (NULL == g_zrddsModule.m_airTrackDw)
    {
        printf("Publisher_create_datawriter_with_qos_profile failed.\n");
        return -4;
    }
    // 创建数据读者，并设置监听器
    g_zrddsModule.m_listener = new SeaTrackTopicListener();
    DataReader* rawReader = g_zrddsModule.m_rioDp->create_datareader_with_topic_and_qos_profile(
        "SeaTrack", ZeroCopyBytesTypeSupport::get_instance(), "lib1", "profile1", "SeaTrack", g_zrddsModule.m_listener, STATUS_MASK_ALL);
    g_zrddsModule.m_seaTrackDr = dynamic_cast<ZeroCopyBytesDataReader*>(rawReader);
    if (NULL == g_zrddsModule.m_seaTrackDr)
    {
        printf("Subscriber_create_datareader_with_qos_profile failed.\n");
        return -4;
    }
    return 0;
}

void ZRDDSDemoModulesFinialize(ZRDDSDemoModules* ddsModule)
{
    // 回收DDS模块资源
    DomainParticipantFactory::get_instance()->delete_contained_entities();
    DomainParticipantFactory::finalize_instance();
    // 回收listener
    delete ddsModule->m_listener;
}

int main()
{
    if (ZRDDSDemoModulesInitial() < 0)
    {
        getchar();
        return -1;
    }
    ZeroCopyBytes sample;
    sample.reservedLength = 1024;
    sample.totalLength = 4096 + sample.reservedLength;
    sample.value = (Char*)malloc(sample.totalLength);
    unsigned int i = 0;
    while (i++ < 100)
    {
        sprintf(sample.value + sample.reservedLength, "air track value %u from app1.", i);
        ReturnCode_t retCode = g_zrddsModule.m_airTrackDw->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        ZRSleep(1000);
    }
    ZRDDSDemoModulesFinialize(&g_zrddsModule);
}