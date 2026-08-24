#include <WaitSet.h>
#include <string.h>

#include <iostream>
#include <random>

#include "ZRDDSCppSimpleInterface.h"
#include <QVector>

using namespace DDS;
using namespace std;


int main() {
    // 域号
    const int domain_id = 10;
    ReturnCode_t rtn;

    DomainParticipantFactory* factory = DomainParticipantFactory::get_instance();
    if (factory == NULL)
        {
            printf("DomainParticipantFactory::get_instance failed.\n");
            return -1;
        }
    // 初始化使用udp的域参与者
    DomainParticipantQos dp_qos;
    factory->get_default_participant_qos(dp_qos);
    DomainParticipant* dp = factory->create_participant(domain_id, dp_qos, nullptr, STATUS_MASK_NONE);

    // 注册数据类型
    const char* type_name1 = BytesTypeSupport::get_instance()->get_type_name();
    rtn = BytesTypeSupport::get_instance()->register_type(dp, nullptr);
    if (rtn != RETCODE_OK) {
            printf("register type failed\n");
            return -1;
        }
    // 创建主题
    Topic* tp1 = dp->create_topic("DDSTest", type_name1, TOPIC_QOS_DEFAULT, NULL, STATUS_MASK_NONE);
    if (tp1 == NULL) {
            printf("create tp failed\n");
            return -1;
        }

    // 创建发布者
    PublisherQos pub_qos;
    dp->get_default_publisher_qos(pub_qos);
    Publisher* pub = dp->create_publisher(pub_qos, NULL, STATUS_MASK_NONE);
    if (pub == NULL) {
            printf("create sub failed\n");
            return -1;
        }

    // 创建数据写者
    DataWriterQos dw_qos;
    pub->get_default_datawriter_qos(dw_qos);
    DataWriter* _dw1 = pub->create_datawriter(tp1, dw_qos, NULL, STATUS_MASK_ALL);
    if (_dw1 == NULL) {
            printf("create datareader failed\n");
            return -1;
        }
    BytesDataWriter* dw1 = dynamic_cast<BytesDataWriter*>(_dw1);

    unsigned int length = 1024;
    char* buffer = (char*)malloc(length);
    memset(buffer, 'a', length);
    QVector<char> vec;
    vec.reserve(1024);
    memcpy(vec.data(),buffer,1024);
    unsigned int i = 0;

    for (int i = 0; i < 20; ++i) {
            Bytes sample;
            sample.value.ensure_length(1024,1024);
            DDSIF::BytesWrapper(sample, vec.data(), length);
            ReturnCode_t retCode = dw1->write(sample, DDS::HANDLE_NIL_NATIVE);
            if (retCode != RETCODE_OK)
                {
                    printf("send failed(%d).\n", retCode);
                }
            else
                {
                    printf("send a data.\n");
                }
            ZRSleep(1000);
        }
    free(buffer);
    DDSIF::Finalize();
    return 0;
}
