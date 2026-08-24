#include "ZRDDSCppSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

int pub(int argc, char* argv[])
{
    // 获取入口，并指明QoS配置
    DomainParticipantFactory* factory = DDSIF::Init("./config/zrdds_security_qos.xml", "security_example");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 加载内置插件
    ReturnCode_t retCode = factory->load_security_plugin(
        "BuiltinPlugin", 
        "ZRDDSSecBuiltinPlugin.dll", 
        "BuiltinSecPluginGetInstance", 
        "BuiltinSecPluginFinalize");
    if (retCode != NULL)
    {
        printf("load_security_plugin failed(%d).", retCode);
        return -1;
    }
    // 初始化授权的域参与者
    DomainParticipant* udpDp = DDSIF::CreateDP(150, "encry_example_pub");
    if (NULL == udpDp) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建数据写者
    DataWriter* rawWriter1 = DDSIF::PubTopic(
        udpDp, "encry_example_1", BytesTypeSupport::get_instance(), "reliable", NULL);
    BytesDataWriter* acWriter1 = (BytesDataWriter*)rawWriter1;
    if (NULL == acWriter1) { printf("DDSIF::PubTopic failed.\n"); return -5; }
    DataWriter* rawWriter2 = DDSIF::PubTopic(
        udpDp, "encry_example_2", BytesTypeSupport::get_instance(), "reliable", NULL);
    BytesDataWriter* acWriter2 = (BytesDataWriter*)rawWriter2;
    if (NULL == acWriter2) { printf("DDSIF::PubTopic failed.\n"); return -5; }
    // 发送数据
    Bytes sample;
    sample.value.ensure_length(1024, 1024);
    unsigned int counter = 0;
    while (++counter < 100)
    {
        // 发送ac_example_1主题数据
        sprintf((char*)sample.value._contiguousBuffer, "sample(%u) from %s.", counter, acWriter1->get_topic()->get_name());
        ReturnCode_t retCode = acWriter1->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        printf("send sample %s\n", sample.value._contiguousBuffer);
        // 发送ac_example_2主题数据
        sprintf((char*)sample.value._contiguousBuffer, "sample(%u) from %s.", counter, acWriter2->get_topic()->get_name());
        retCode = acWriter2->write(sample, HANDLE_NIL_NATIVE);
        if (retCode != RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        printf("send sample %s\n", sample.value._contiguousBuffer);
        ZRSleep(1000);
    }
    return 0;
}

int main(int argc, char* argv[])
{
    return pub(argc, argv);
}