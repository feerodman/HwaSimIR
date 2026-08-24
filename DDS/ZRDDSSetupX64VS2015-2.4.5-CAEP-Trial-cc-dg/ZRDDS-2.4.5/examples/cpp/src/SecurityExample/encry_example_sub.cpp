#include "ZRDDSCppSimpleInterface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

// 定义零拷贝数据类型回调接口
class BytesTypeListener :
    virtual public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    virtual void on_process_sample(DataReader* reader, const Bytes& sample, const SampleInfo& info)
    {
        // 通过该语句获取收到的样本所属主题名称
        const char* topicName = reader->get_topicdescription()->get_name();
        // TODO 在此填写业务处理，接收端sample.userBuffer为用户负载数据，sample.userLength为用户负载长度，无需考虑预留空间
        printf("received data(%s) from topic(%s)\n", 
            sample.value.get_contiguous_buffer(), 
            topicName);
    }
};

int sub()
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
    DomainParticipant* authDP = DDSIF::CreateDP(150, "encry_example_sub");
    if (NULL == authDP) { printf("DDSIF::CreateDP failed.\n"); return -2; }
    // 创建ac_example_1数据读者，并设置监听器，期望成功
    BytesTypeListener* m_listener = new BytesTypeListener();
    DataReader* authDr = DDSIF::SubTopic(
        authDP, "encry_example_1", BytesTypeSupport::get_instance(), "reliable", m_listener);
    if (NULL == authDr) { printf("DDSIF::SubscribeTopic failed.\n"); return -5; }
    // 创建ac_example_2数据读者，并设置监听器，由于权限配置文件中未指明能够订阅该主题，故而订阅失败
    authDr = DDSIF::SubTopic(
        authDP, "encry_example_2", BytesTypeSupport::get_instance(), "reliable", m_listener);
    if (NULL == authDr) { printf("DDSIF::SubscribeTopic failed.\n"); return -5;  }

    getchar();
    DDSIF::Finalize();
    return 0;
}

int main(int argc, char* argv[])
{
    return sub();
}
