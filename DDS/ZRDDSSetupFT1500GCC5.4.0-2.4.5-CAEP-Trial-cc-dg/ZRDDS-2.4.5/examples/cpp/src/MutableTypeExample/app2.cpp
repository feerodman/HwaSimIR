#include "ZRDDSCppSimpleInterface.h"
#include "MutableTypeExampleDataReader.h"
#include "MutableTypeExampleTypeSupport.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace DDS;

/* 定义回调接口 */
class OriginTypeListener : public SimpleDataReaderListener<OrignalType, OrignalTypeSeq, OrignalTypeDataReader>
{
public:
    virtual void on_process_sample(DataReader* reader, const OrignalType& sample, const SampleInfo& info)
    {
        // 通过该语句获取收到的样本所属主题名称
        const char* topicName = reader->get_topicdescription()->get_name();
        // TODO 在此填写业务处理
        printf("received x(%d) y(%d) from topic(%s)\n", sample.x, sample.y, topicName);
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
    // 创建使用“老”的数据类型的数据读者
    DataReader* rawReader = DDSIF::SubTopic(
        dp, "mutable_example_topic", OrignalTypeTypeSupport::get_instance(), "default", new OriginTypeListener());
    OrignalTypeDataReader* typedDr = dynamic_cast<OrignalTypeDataReader*>(rawReader);
    if (NULL == typedDr) { printf("DDSIF::SubTopic failed.\n"); return -4; }
    getchar();
    DDSIF::Finalize();
    return 0;
}
