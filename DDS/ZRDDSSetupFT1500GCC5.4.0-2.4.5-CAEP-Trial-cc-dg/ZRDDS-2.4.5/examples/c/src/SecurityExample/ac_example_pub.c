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
    // 初始化使用rio的域参与者
    DDS_DomainParticipant* udpDp = DDS_CreateDP(150, "ac_example_pub");
    if (NULL == udpDp) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建数据写者
    DDS_DataWriter* rawWriter1 = DDS_PubTopic(
        udpDp, "ac_example_1", &DDS_BytesTypeSupport_instance, "reliable", NULL);
    DDS_BytesDataWriter* acWriter1 = (DDS_BytesDataWriter*)rawWriter1;
    if (NULL == acWriter1) { printf("DDSIF::PubTopic failed.\n"); return -5; }
    DDS_DataWriter* rawWriter2 = DDS_PubTopic(
        udpDp, "ac_example_2", &DDS_BytesTypeSupport_instance, "reliable", NULL);
    DDS_BytesDataWriter* acWriter2 = (DDS_BytesDataWriter*)rawWriter2;
    if (NULL == acWriter2) { printf("DDSIF::PubTopic failed.\n"); return -5; }
    // 发送数据
    DDS_Bytes sample;
    DDS_OctetSeq_initialize(&sample.value);
    DDS_OctetSeq_ensure_length(&sample.value, 1024, 1024);
    unsigned int counter = 0;
    while (++counter < 100)
    {
        // 发送ac_example_1主题数据
        sprintf((char*)sample.value._contiguousBuffer, "sample(%u) from %s.", counter, 
            DDS_TopicDescription_get_name((DDS_TopicDescription*)DDS_BytesDataWriter_get_topic(acWriter1)));
        DDS_ReturnCode_t retCode = DDS_BytesDataWriter_write(acWriter1, &sample, &DDS_HANDLE_NIL_NATIVE);
        if (retCode != DDS_RETCODE_OK)
        {
            printf("send failed(%d).\n", retCode);
        }
        printf("send sample %s\n", sample.value._contiguousBuffer);
        // 发送ac_example_2主题数据
        sprintf((char*)sample.value._contiguousBuffer, "sample(%u) from %s.", counter,
            DDS_TopicDescription_get_name((DDS_TopicDescription*)DDS_BytesDataWriter_get_topic(acWriter2)));
        retCode = DDS_BytesDataWriter_write(acWriter2, &sample, &DDS_HANDLE_NIL_NATIVE);
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