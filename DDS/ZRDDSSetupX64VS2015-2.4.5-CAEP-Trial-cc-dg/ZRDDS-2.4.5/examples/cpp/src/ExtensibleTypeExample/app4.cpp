#include "ZRDDSCppSimpleInterface.h"
#include "ExtensionTypeExampleDataReader.h"
#include "ExtensionTypeExampleTypeSupport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

/* 定义回调接口 */
class NewTypeListener : public SimpleDataReaderListener<NewType, NewTypeSeq, NewTypeDataReader>
{
public:
    virtual void on_process_sample(DataReader* reader, const NewType& sample, const SampleInfo& info)
    {
        // 通过该语句获取收到的样本所属主题名称
        const char* topicName = reader->get_topicdescription()->get_name();
        // TODO 在此填写业务处理
        printf("received x(%d) y(%d) z(%d) angle(%f) from topic(%s)\n", sample.x, sample.y, sample.z, sample.angle, topicName);
    }
};

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
    // 创建使用“新”的数据类型的数据读者
    DataReader* rawReader = DDSIF::SubTopic(
        dp, "extension_example_topic", NewTypeTypeSupport::get_instance(), "default", new NewTypeListener());
    NewTypeDataReader* typedDr = dynamic_cast<NewTypeDataReader*>(rawReader);
    if (NULL == typedDr) { printf("DDSIF::SubTopic failed.\n"); return -4; }
    getchar();
    DDSIF::Finalize();
    return 0;
}
