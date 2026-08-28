#include "ZRDDSCppSimpleInterface.h"
#include "ExtensionTypeExampleDataWriter.h"
#include "ExtensionTypeExampleTypeSupport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

int main()
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DDSIF::Init(NULL, "default");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 创建域参与者
    DomainParticipant* dp = DDSIF::CreateDP(100, "udp_dp");
    if (NULL == dp) { printf("DDSIF::CreateDP failed.\n"); return -3; }
    // 如果需要注册不同默认名称则以下两步需使用标准接口创建主题
    // 注册数据类型
    const char* registeredTypeName = OrignalTypeTypeSupport::get_instance()->get_type_name();
    ReturnCode_t retCode = NewTypeTypeSupport::get_instance()->register_type(dp, registeredTypeName);
    if (retCode != RETCODE_OK) { printf("register_type failed(%d).\n", retCode); return -4; }
    // 创建主题
    Topic* topic = dp->create_topic("extension_example_topic", registeredTypeName, TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (NULL == topic) { printf("DomainParticipant::create_topic failed.\n"); return -4; }
    // 创建使用“新”的数据类型的数据写者
    DataWriter* rawWriter = DDSIF::PubTopic(
        dp, "extension_example_topic", NewTypeTypeSupport::get_instance(), "default", NULL);
    NewTypeDataWriter* typedDw = dynamic_cast<NewTypeDataWriter*>(rawWriter);
    if (NULL == typedDw) { printf("DDSIF::PubTopic failed.\n"); return -4; }
    // 发送数据
    NewType sample;
    unsigned int i = 0;
    while (i++ < 2000)
    {
        sample.x = i;
        sample.y = i + 1;
        sample.z = i + 2;
        sample.angle = i + 3;
        ReturnCode_t retCode = typedDw->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        printf("send sample x(%d) y(%d) z(%d) angle(%f)\n", sample.x, sample.y, sample.z, sample.angle);
        ZRSleep(1000);
    }
    DDSIF::Finalize();
    return 0;
}
