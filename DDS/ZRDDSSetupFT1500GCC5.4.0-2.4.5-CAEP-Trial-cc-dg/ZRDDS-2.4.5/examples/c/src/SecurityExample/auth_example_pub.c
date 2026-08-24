#include "ZRDDSCSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pub(int argc, char* argv[])
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_Init("./config/zrdds_security_qos.xml", "security_example");
    if (factory == NULL) { printf("DDSIF::Initialize failed.\n"); return -1; }
    // 加载内置插件
    DDS_ReturnCode_t retCode = DDS_DomainParticipantFactory_load_security_plugin(
        factory,
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
    DDS_DomainParticipant* udpDp = DDS_CreateDP(150, "auth_example_pub");
    if (NULL == udpDp) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建数据写者
    DDS_DataWriter* rawWriter = DDS_PubTopic(
        udpDp, "auth_example", &DDS_BytesTypeSupport_instance, "reliable", NULL);
    DDS_BytesDataWriter* writer = (DDS_BytesDataWriter*)rawWriter;
    if (NULL == writer) { printf("DDS_PubTopic failed.\n"); return -5; }
    // 发送数据
    DDS_Bytes sample;
    DDS_OctetSeq_initialize(&sample.value);
    DDS_OctetSeq_ensure_length(&sample.value, 1024, 1024);
    unsigned int counter = 0;
    while (++counter < 100)
    {
        // 发送auth_example主题数据
        sprintf((char*)sample.value._contiguousBuffer, "sample(%u) from auth_example_pub.", counter);
        DDS_ReturnCode_t retCode = DDS_BytesDataWriter_write(writer, &sample, &DDS_HANDLE_NIL_NATIVE);
        if (retCode != DDS_RETCODE_OK)
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