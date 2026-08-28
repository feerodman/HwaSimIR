#include "ZRDDSCSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DDS_SimpleDataReaderListener(BytesTypeListener, DDS_Bytes)(DDS_DataReader* reader, DDS_Bytes* sample, DDS_SampleInfo* info)
{
    // 通过该语句获取收到的样本所属主题名称
    const DDS_Char* topicName = DDS_TopicDescription_get_name(DataReaderImplGetTopicDescription(reader));
    // TODO 在此填写业务处理，接收端sample.userBuffer为用户负载数据，sample.userLength为用户负载长度，无需考虑预留空间
    printf("received data(%s) from topic(%s)\n",
        sample->value._contiguousBuffer,
        topicName);
}

int sub()
{
    // 获取入口，并指明QoS配置
    DDS_DomainParticipantFactory* factory = DDS_Init("./config/zrdds_security_qos.xml", "security_example");
    if (factory == NULL) { printf("DDS_Initialize failed.\n"); return -1; }
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
    DDS_DomainParticipant* authDP = DDS_CreateDP(150, "ac_example_sub");
    if (NULL == authDP) { printf("DDS_CreateDP failed.\n"); return -2; }
    // 创建ac_example_1数据读者，并设置监听器，期望成功
    DDS_DataReaderListener m_listener;
    memset(&m_listener, 0, sizeof(m_listener));
    m_listener.on_data_available = BytesTypeListener_on_data_available;
    DDS_DataReader* authDr = DDS_SubTopic(
        authDP, "ac_example_1", &DDS_BytesTypeSupport_instance, "reliable", &m_listener);
    if (NULL == authDr) { printf("DDS_SubscribeTopic failed.\n"); return -5; }
    // 创建ac_example_2数据读者，并设置监听器，由于权限配置文件中未指明能够订阅该主题，故而订阅失败
    authDr = DDS_SubTopic(
        authDP, "ac_example_2", &DDS_BytesTypeSupport_instance, "reliable", &m_listener);
    if (NULL == authDr) { printf("DDS_SubscribeTopic ac_example_2 failed.\n"); }

    getchar();
    DDS_Finalize();
    return 0;
}

int main(int argc, char* argv[])
{
    return sub();
}
