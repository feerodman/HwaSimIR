#include <assert.h>

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "Topic.h"
#include "DataReader.h"
#include "DataReaderListener.h"
#include "BuiltinDataDataReader.h"
using namespace DDS;
// 内置DataReader的Listener的监听器
class BuiltinListener : public DataReaderListener
{
public:
    virtual void on_data_available(DataReader *the_reader)
    {
        //底层回调的方法
    }

};

int setupBuiltinDataReaderListener()
{
    // 设置DomainParticipantFactory的实体工厂QoS为false
    DomainParticipantFactoryQos dpfQos;
    TheParticipantFactory->get_qos(dpfQos);
    dpfQos.entity_factory.autoenable_created_entities = false;
    TheParticipantFactory->set_qos(dpfQos);
    // 创建Participant，此时创建的Participant为未使能状态
    DomainParticipant* participant = TheParticipantFactory->create_participant(150,
        DOMAINPARTICIPANT_QOS_DEFAULT,
        NULL, STATUS_MASK_ALL);
    if (participant == NULL)
    {
        return -1;
    }
    // 获取内置的Subscriber
    Subscriber* builtinSub = participant->get_builtin_subscriber();
    // 查找内置的ParticipantBuiltinTopicDataDataReader，并设置监听器
    ParticipantBuiltinTopicDataDataReader* participantDr = (ParticipantBuiltinTopicDataDataReader*)
        builtinSub->lookup_datareader(BUILTIN_PARTICIPANT_TOPIC_NAME);
    assert(participantDr != NULL);
    participantDr->set_listener(new BuiltinListener,
        STATUS_MASK_ALL);
    // 查找内置的PublicationBuiltinTopicDataDataReader，并设置监听器
    PublicationBuiltinTopicDataDataReader* publicationDr = (PublicationBuiltinTopicDataDataReader*)
        builtinSub->lookup_datareader(BUILTIN_PUBLICATION_TOPIC_NAME);
    assert(publicationDr != NULL);
    publicationDr->set_listener(new BuiltinListener,
        STATUS_MASK_ALL);
    // 查找内置的SubscriptionBuiltinTopicDataDataReader，并设置监听器
    SubscriptionBuiltinTopicDataDataReader* subscriptionDr = (SubscriptionBuiltinTopicDataDataReader*)
        builtinSub->lookup_datareader(BUILTIN_SUBSCRIPTION_TOPIC_NAME);
    assert(subscriptionDr != NULL);
    subscriptionDr->set_listener(new BuiltinListener,
        STATUS_MASK_ALL);
    // 手动使能participant
    if (RETCODE_OK != participant->enable())
    {
        return -1;
    }
    return 0;
}
