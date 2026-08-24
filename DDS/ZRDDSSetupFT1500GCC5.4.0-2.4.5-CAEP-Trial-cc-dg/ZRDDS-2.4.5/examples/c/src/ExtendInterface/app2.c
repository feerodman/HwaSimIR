#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Publisher.h"
#include "Subscriber.h"
#include "ZRBuiltinTypesDataWriter.h"
#include "ZRBuiltinTypesDataReader.h"
#include "ZRBuiltinTypesTypeSupport.h"
#include "DefaultQos.h"
#include "ZRSleep.h"
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
    /*** @brief   用于UDP通信的实体。 */
    DDS_DomainParticipant* m_udpDp;
    /*** @brief   用于接收主题1数据。 */
    DDS_ZeroCopyBytesDataReader* m_airTrackDr;
    /*** @brief   数据读者监听器。 */
    DDS_DataReaderListener m_listener;
}ZRDDSDemoModules;

/*** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

/* 实现数据读者回调接口 */
DDS_SimpleDataReaderListener(SeaTrackTopic, DDS_ZeroCopyBytes)(DDS_DataReader* reader, DDS_ZeroCopyBytes* sample, DDS_SampleInfo* info)
{
    printf("received data %s\n", sample->value);
}

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_DomainParticipantFactory_get_instance_w_profile(
        NULL, "lib1", "profile1", "app2");
    if (factory == NULL)
    {
        printf("DDS_DomainParticipantFactory_get_instance failed.\n");
        return -1;
    }
    // 初始化使用udp的域参与者
    g_zrddsModule.m_udpDp = DDS_DomainParticipantFactory_create_participant_with_qos_profile(
        factory, 150, "lib1", "profile1", "udp_dp", NULL, DDS_STATUS_MASK_NONE);
    if (NULL == g_zrddsModule.m_udpDp)
    {
        printf("DDS_DomainParticipantFactory_create_participant_with_qos_profile failed.\n");
        return -2;
    }
    // 创建数据读者，并设置监听器
    memset(&g_zrddsModule.m_listener, 0, sizeof(g_zrddsModule.m_listener));
    g_zrddsModule.m_listener.on_data_available = SeaTrackTopic_on_data_available;
    g_zrddsModule.m_airTrackDr = (DDS_ZeroCopyBytesDataReader*)DDS_DomainParticipant_create_datareader_with_topic_and_qos_profile(
        g_zrddsModule.m_udpDp, "AirTrack", &DDS_ZeroCopyBytesTypeSupport_instance, "lib1", "profile1", "SeaTrack", &g_zrddsModule.m_listener, DDS_STATUS_MASK_ALL);
    if (NULL == g_zrddsModule.m_airTrackDr)
    {
        printf("DDS_Subscriber_create_datareader_with_qos_profile failed.\n");
        return -4;
    }
    return 0;
}

void ZRDDSDemoModulesFinialize(ZRDDSDemoModules* ddsModule)
{
    // 回收DDS模块资源
    DDS_DomainParticipantFactory_delete_contained_entities(DDS_DomainParticipantFactory_get_instance());
    DDS_DomainParticipantFactory_finalize_instance();
}

int main()
{
    if (ZRDDSDemoModulesInitial() < 0)
    {
        getchar();
        return -1;
    }
    // 等待数据
    getchar();
    ZRDDSDemoModulesFinialize(&g_zrddsModule);
}