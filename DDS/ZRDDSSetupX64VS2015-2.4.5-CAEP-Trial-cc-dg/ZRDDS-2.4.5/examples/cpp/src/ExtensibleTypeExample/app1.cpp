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
    // 创建使用“老”的数据类型的数据写者
    DataWriter* rawWriter = DDSIF::PubTopic(
        dp, "extension_example_topic", OrignalTypeTypeSupport::get_instance(), "default", NULL);
    OrignalTypeDataWriter* typedDw = dynamic_cast<OrignalTypeDataWriter*>(rawWriter);
    if (NULL == typedDw) { printf("DDSIF::PubTopic failed.\n"); return -4; }
    // 发送数据
    OrignalType sample;
    unsigned int i = 0;
    while (i++ < 2000)
    {
        sample.x = i;
        sample.y = i + 1;
        ReturnCode_t retCode = typedDw->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        printf("send sample x(%d) y(%d)\n", sample.x, sample.y);
        ZRSleep(1000);
    }
    DDSIF::Finalize();
    return 0;
}
