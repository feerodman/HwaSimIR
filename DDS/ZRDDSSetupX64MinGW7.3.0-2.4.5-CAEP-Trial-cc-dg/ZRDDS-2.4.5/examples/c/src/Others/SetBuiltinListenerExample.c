#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Topic.h"
#include "DataReader.h"
#include "DataReaderListener.h"
#include "BuiltinDataDataReader.h"

/*内置DataReader的Listener的监听器*/

void Builtin_on_data_available(DDS_DataReader *the_reader)
{
        //底层回调的方法*/
}



int setupBuiltinDataReaderListener()
{
    /*设置DomainParticipantFactory的实体工厂QoS为false*/
    DDS_DomainParticipantFactoryQos dpfQos;
    DDS_DomainParticipantFactory *factory = DDS_DomainParticipantFactory_get_instance();
    DDS_DomainParticipantFactory_get_qos(factory,&dpfQos);
    dpfQos.entity_factory.autoenable_created_entities = false;
    DDS_DomainParticipantFactory_set_qos(factory,&dpfQos);
    /*创建Participant，此时创建的Participant为未使能状态*/
    DDS_DomainParticipant* participant = DDS_DomainParticipantFactory_create_participant(factory,11,
        &DOMAINPARTICIPANT_QOS_DEFAULT,
        NULL, DDS_STATUS_MASK_ALL);
    if (participant == NULL)
    {
        return -1;
    }
    /*获取内置的Subscriber*/
    DDS_Subscriber* builtinSub = DDS_DomainParticipant_get_builtin_subscriber(participant);
    /*查找内置的ParticipantBuiltinTopicDataDataReader，并设置监听器*/
    DDS_ParticipantBuiltinTopicDataDataReader* participantDr = (DDS_ParticipantBuiltinTopicDataDataReader*)
        DDS_Subscriber_lookup_datareader(builtinSub, BUILTIN_PARTICIPANT_TOPIC_NAME);
    assert(participantDr != NULL);

    DDS_DataReaderListener readerListener1;
    memset(&readerListener1, 0, sizeof(readerListener1));
    readerListener1.on_data_available = Builtin_on_data_available;
    DDS_ParticipantBuiltinTopicDataDataReader_set_listener(participantDr, &readerListener1,
        DDS_STATUS_MASK_ALL);
    /*查找内置的PublicationBuiltinTopicDataDataReader，并设置监听器*/
    DDS_PublicationBuiltinTopicDataDataReader* publicationDr = (DDS_PublicationBuiltinTopicDataDataReader*)
        DDS_Subscriber_lookup_datareader(builtinSub, BUILTIN_PUBLICATION_TOPIC_NAME);
    assert(publicationDr != NULL);

    DDS_DataReaderListener readerListener2;
    memset(&readerListener2, 0, sizeof(readerListener2));
    readerListener2.on_data_available = Builtin_on_data_available;
    DDS_PublicationBuiltinTopicDataDataReader_set_listener(publicationDr, &readerListener2,
        DDS_STATUS_MASK_ALL);

    /*查找内置的SubscriptionBuiltinTopicDataDataReader，并设置监听器*/
    DDS_SubscriptionBuiltinTopicDataDataReader* subscriptionDr = (DDS_SubscriptionBuiltinTopicDataDataReader*)
        DDS_Subscriber_lookup_datareader(builtinSub,BUILTIN_SUBSCRIPTION_TOPIC_NAME);
    assert(subscriptionDr != NULL);

    DDS_DataReaderListener readerListener3;
    memset(&readerListener3, 0, sizeof(readerListener3));
    readerListener3.on_data_available = Builtin_on_data_available;
    DDS_SubscriptionBuiltinTopicDataDataReader_set_listener(subscriptionDr, &readerListener3,
        DDS_STATUS_MASK_ALL);
    /*手动使能participant*/
    if (DDS_RETCODE_OK !=DDS_DomainParticipant_enable(participant))
    {
        return -1;
    }
    return 0;
}