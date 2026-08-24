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
    /** @brief   用于RIO通信的实体。 */
    DomainParticipant* m_rioDp;
    /** @brief   用于发送主题2数据。 */
    ZeroCopyBytesDataWriter* m_seaTrackDw;
}ZRDDSDemoModules;

/** @brief   实体全局对象，也可以将该对象放在业务对象中。 */
ZRDDSDemoModules g_zrddsModule;

int ZRDDSDemoModulesInitial()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DomainParticipantFactory::get_instance_w_profile(NULL, "lib1", "profile1", "app1");
    if (factory == NULL)
    {
        printf("DomainParticipantFactory::get_instance failed.\n");
        return -1;
    }
    // 初始化使用rio的域参与者
    g_zrddsModule.m_rioDp = factory->create_participant_with_qos_profile(100, "lib1", "profile1", "rio_dp", NULL, STATUS_MASK_NONE);
    if (NULL == g_zrddsModule.m_rioDp)
    {
        printf("DomainParticipantFactory_create_participant_with_qos_profile failed.\n");
        return -5;
    }
    // 创建数据写者
    DataWriter* rawWriter = g_zrddsModule.m_rioDp->create_datawriter_with_topic_and_qos_profile(
        "SeaTrack", ZeroCopyBytesTypeSupport::get_instance(), "lib1", "profile1", "SeaTrack", NULL, STATUS_MASK_NONE);
    g_zrddsModule.m_seaTrackDw = dynamic_cast<ZeroCopyBytesDataWriter*>(rawWriter);
    if (NULL == g_zrddsModule.m_seaTrackDw)
    {
        printf("Publisher_create_datawriter_with_qos_profile failed.\n");
        return -4;
    }
    return 0;
}

void ZRDDSDemoModulesFinialize(ZRDDSDemoModules* ddsModule)
{
    // 回收DDS模块资源
    DomainParticipantFactory::get_instance()->delete_contained_entities();
    DomainParticipantFactory::finalize_instance();
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
        sprintf(sample.value + sample.reservedLength, "sea track value(%u) from app3.", i);
        ReturnCode_t retCode = g_zrddsModule.m_seaTrackDw->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        ZRSleep(1000);
    }
    ZRDDSDemoModulesFinialize(&g_zrddsModule);
}