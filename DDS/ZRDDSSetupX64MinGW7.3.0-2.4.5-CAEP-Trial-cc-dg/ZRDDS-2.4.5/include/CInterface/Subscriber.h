/**
 * @file:       Subscriber.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef Subscriber_h__
#define Subscriber_h__

#include "SubscriberQos.h"
#include "SubscriberListener.h"
#include "StatusKindMask.h"
#include "DataReaderQos.h"
#include "TopicQos.h"
#include "SampleStateMask.h"
#include "ViewStateMask.h"
#include "InstanceStateMask.h"
#include "ReturnCode_t.h"
#include "DataReader.h"
#include "ZRDDSTypeSupport.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_qos( DDS_Subscriber* self, DDS_SubscriberQos* qoslist);
 *
 * @ingroup CSubscription
 *
 * @brief   获取该订阅者的QoS配置。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  qoslist 出口参数，用于保存订阅者的QoS配置。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示获取成功；
 *          - #DDS_RETCODE_ERROR :表示失败，原因可能为复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_qos(
    DDS_Subscriber* self,
    DDS_SubscriberQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_qos( DDS_Subscriber* self, const DDS_SubscriberQos* qoslist);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法设置为订阅者设置的QoS。
 *
 * @param [in,out]  self    指向目标。
 * @param   qoslist 表示用户想要设置的QoS配置。
 *
 * @details 可以使用特殊值 #DDS_SUBSCRIBER_QOS_DEFAULT 表示使用存储在域参与者中的QoS配置。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示获取成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :表示 @e qoslist 含有无效的QoS配置；
 *          - #DDS_RETCODE_INCONSISTENT :表示 @e qoslist 含有不兼容的QoS配置；
 *          - #DDS_RETCODE_IMMUTABLE_POLICY :表示用户尝试修改使能后不可变的QoS配置；
 *          - #DDS_RETCODE_ERROR :表示未归类的错误，错误详细信息输出在日志中；
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_qos(
    DDS_Subscriber* self, 
    const DDS_SubscriberQos* qoslist);

/**
 * @fn  DCPSDLL DDS_SubscriberListener* DDS_Subscriber_get_listener( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法获取通过 #DDS_Subscriber_set_listener 方法或者创建时为订阅者设置的监听器对象。
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @return  当前可能的返回值：
 *          - NULL表示未设置监听器；
 *          - 非空表示应用通过 #DDS_Subscriber_set_listener 或者在创建时设置的监听器对象。
 */

DCPSDLL DDS_SubscriberListener* DDS_Subscriber_get_listener(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_listener( DDS_Subscriber* self, DDS_SubscriberListener* listener, DDS_StatusKindMask mask);
 *
 * @ingroup CSubscription
 *
 * @brief   设置该订阅者的监听器。
 *
 * @details  本方法将覆盖原有监听器，如果设置空对象表示清除原先设置的监听器。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  listener  为该订阅者设置的监听器对象。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  当前总是返回 #DDS_RETCODE_OK 表示设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_listener(
    DDS_Subscriber* self,
    DDS_SubscriberListener* listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_enable( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   手动使能该实体，参见@ref entity-enable 。
 *
 * @param [in,out]  self    指向目标。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK ，表示获取成功；  
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET ，表示所属的父实体尚未使能
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_enable(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader( DDS_Subscriber* self, DDS_TopicDescription* topic, const DDS_DataReaderQos* qos, DDS_DataReaderListener* listener, DDS_StatusKindMask mask);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法在订阅者下创建一个数据读者子实体，并设置关联的主题、QoS以及监听器。
 *
 * @details 用户使用该数据读者从域内读取/获取指定主题数据，返回的数据读者对象为数据读者关联的用户数据类型相关的数据读者的父类指针，
 *          用户需要将返回值动态转化为用户数据类型的数据读者对象，具体代码参见 @ref subscription_example.c 。
 *          。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  topic     关联的主题描述，用户可以关联主题以及基于内容过滤的主题，
 *                              该主题描述必须在调用该方法之前在同一个域参与者下调用 #DDS_DomainParticipant_create_topic
 *                               #DDS_DomainParticipant_create_contentfilteredtopic 方法创建的主题的父类；
 * @param   qos             表示为该数据读者设置的QoS， #DDS_DATAREADER_QOS_DEFAULT 表明使用订阅者中保存的默认的QoS。
 * @param [in,out]  listener  为该订阅者设置的监听器，此参数可以为空。 ZRDDS不会接管监听器对象的内存管理，由用户负责释放。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  非NULL表示创建成功，否则表示失败，失败的原因可能为：
 *          - @e topic 不是有效的主题对象；
 *          - @e topic 的父实体与该订阅者不属于一个域参与者实体；
 *          - @e qos 中含有无效的QoS或者含有不一致的QoS配置；
 *          - 分配内容错误等未归类错误，详细参见日志。
 */

DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader(
    DDS_Subscriber* self, 
    DDS_TopicDescription* topic,
    const DDS_DataReaderQos* qos,
    DDS_DataReaderListener* listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_delete_datareader( DDS_Subscriber* self, DDS_DataReader* reader);
 *
 * @ingroup CSubscription
 *
 * @brief   删除指定的数据读者。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  reader    指明需要删除的数据读者。
 *
 * @details 被删除的数据读者应满足删除条件，主要包括：
 *          - 数据读者创建的所有“子实体”读取条件已经全部被删除；
 *          - 通过该数据读者的 @ref read-take 系列方法租借给用户的空间已经全部回收成功；
 *
 * @return  当前可能的返回值如下：
 *          - #DDS_RETCODE_OK ：删除成功；
 *          - #DDS_RETCODE_BAD_PARAMETER ：参数指定的数据读者不是有效的数据读者对象；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET ：
 *              - 参数指定的数据读者不属于本；
 *              - 指定的数据读者不满足删除条件；
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_delete_datareader(
    DDS_Subscriber* self, 
    DDS_DataReader* reader);

/**
 * @fn  DCPSDLL DDS_DataReader* DDS_Subscriber_lookup_datareader( DDS_Subscriber* self, const DDS_Char* topicName);
 *
 * @ingroup CSubscription
 * @brief   根据主题名查找数据读者。
 *
 * @details 如果存在多个满足条件的数据读者，则返回数据读者地址最小的那个。
 *
 * @param [in,out]  self    指向目标。
 * @param   topicName  查询的主题名。
 *
 * @return  返回空表示没有满足条件的数据读者，否则返回相应的数据读者。
 */

DCPSDLL DDS_DataReader* DDS_Subscriber_lookup_datareader(
    DDS_Subscriber* self, 
    const DDS_Char* topicName);

#ifdef _ZRDDS_INCLUDE_PRESENTATION_QOS

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_begin_access( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   数据读者端开启一致性访问。
 *
 * @details  此方法只有在订阅者 DDS_PresentationQosPolicy::access_scope == #DDS_GROUP_PRESENTATION_QOS
 *           时有效，该方法应与 #DDS_Subscriber_end_access 方法配合使用。
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @return  当前总是返回 #DDS_RETCODE_UNSUPPORTED 。
 *
 * @warning 该方法未实现。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_begin_access(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_end_access( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   与 #DDS_Subscriber_begin_access 方法对应，表示数据访问结束。
 *
 * @param [in,out]  self    指向目标。
 * 
 * @return  当前总是返回 #DDS_RETCODE_UNSUPPORTED 。
 *
 * @warning 该方法未实现。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_end_access(
    DDS_Subscriber* self);

#endif /* _ZRDDS_INCLUDE_PRESENTATION_QOS */

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_datareaders( DDS_Subscriber* self, DDS_DataReaderSeq* readers, DDS_SampleStateMask sampleStates, DDS_ViewStateMask viewStates, DDS_InstanceStateMask instanceStates);
 *
 * @ingroup CSubscription
 *
 * @brief   查找底层含有处于特定状态的数据样本的数据读者的结合。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  readers  出口参数，用于存储满足条件的数据读者列表。
 * @param   sampleStates       需要满足的样本状态条件。
 * @param   viewStates         需要满足的视图状态条件。
 * @param   instanceStates     需要满足的实例状态条件。
 *
 * @details  可以设置的条件包括 DDS_SampleStateKind 、DDS_ViewStateKind 、 DDS_InstanceStateKind
 *           均通过掩码来表示状态的集合。数据读者中只要有一个样本满足则该数据读者符合条件。
 *
 * @return  当前可能的返回值如下：
 *          - #DDS_RETCODE_OK ：获取成功；
 *          - #DDS_RETCODE_OUT_OF_RESOURCES ：当@e readers 提供的空间不足，且底层扩容失败；
 *          - #DDS_RETCODE_NOT_ENABLED ：本订阅者尚未使能；
 *          - #DDS_RETCODE_ERROR ：内部错误，详细参见日志；
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_datareaders(
    DDS_Subscriber* self,
    DDS_DataReaderSeq* readers,
    DDS_SampleStateMask sampleStates,
    DDS_ViewStateMask viewStates,
    DDS_InstanceStateMask instanceStates);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_notify_datareaders( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   本方法将尝试调用所有处于 #DDS_DATA_AVAILABLE_STATUS 状态的数据读者关联的监听器的
 *          DDS_DataReaderListener::on_data_available 方法。
 *
 * @details 当数据读者底层有新的数据到达时，将会处于 #DDS_DATA_AVAILABLE_STATUS 状态，当用户回调成功或者用户
 *          通过 @ref read-take 系列方法读取数据时清理这个状态，该方法通常在
 *          DDS_SusbcriberListener::data_on_reader 回调函数中使用。
 *
 * @param [in,out]  self    指向目标。
 * 
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK ：通知成功；
 *          - #DDS_RETCODE_NOT_ENABLED ：本订阅者尚未使能；
 *          - #DDS_RETCODE_ERROR ：内部错误，详细参见日志；
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_notify_datareaders(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_Subscriber_get_participant( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   获得订阅者的父实体域参与者。
 *
 * @param [in,out]  self    指向目标。
 * 
 * @return  返回该订阅者的父实体的域参与者。
 */

DCPSDLL DDS_DomainParticipant* DDS_Subscriber_get_participant(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos( DDS_Subscriber* self, const DDS_DataReaderQos* qoslist);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法设置为数据读者保存的默认QoS配置。
 *
 * @details 默认的QoS在创建新的数据读者时指定QoS参数为 #DDS_DATAREADER_QOS_DEFAULT 时使用的QoS配置，
 *          使用特殊的值 #DDS_DATAREADER_QOS_DEFAULT 发布者QoS中的各个配置的设置为默认值。
 *
 * @param [in,out]  self    指向目标。
 * @param   qoslist 指明QoS配置。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示设置成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :表示@e qoslist为空，或者@e qoslist 具有无效值；
 *          - #DDS_RETCODE_INCONSISTENT :表示@e qoslist 中具有不相容的配置；
 *          - #DDS_RETCODE_ERROR :表示失败，例如复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos(
    DDS_Subscriber* self, 
    const DDS_DataReaderQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_default_datareader_qos( DDS_Subscriber* self, DDS_DataReaderQos* qoslist);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法获取为数据读者保存的默认QoS配置。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  qoslist 出口参数表示获取的结果.
 *
 * @return  当前的返回值类型：
 *          - #DDS_RETCODE_OK :表示设置成功；
 *          - #DDS_RETCODE_ERROR :表示失败，例如复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_get_default_datareader_qos(
    DDS_Subscriber* self, 
    DDS_DataReaderQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_copy_from_topic_qos( DDS_DataReaderQos* datareaderQos, const DDS_TopicQos* topicQos);
 *
 * @ingroup CSubscription
 *
 * @brief   从主题QoS中构造相应的数据读者QoS。
 *
 * @param [in,out]  datareaderQos    出口参数，表示构造的结果数据读取QoS配置。
 * @param   topicQos                 源主题QoS配置。
 *
 * @return  返回表示操作结果的返回码：
 *          - #DDS_RETCODE_OK ：构造成功；
 *          - #DDS_RETCODE_ERROR ：内部错误，具体参见日志文件，可能的原因为分配内存失败。
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_copy_from_topic_qos(
    DDS_DataReaderQos* datareaderQos, 
    const DDS_TopicQos* topicQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_delete_contained_entities( DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   该方法删除该订阅者创建的所有实体。
 *
 * @details 该方法会对子实体进行递归调用 delete_contained_entities 方法，最终的删除的实体包括数据读者、读取条件等；
 *          该方法采取尽力而为的策略删除，即满足删除条件的实体进行删除，如果有不满足删除条件的实体，则返回特定的错误码。
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示删除成功；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :表示有部分实体不满足删除条件，仅删除部分实体；
 *          - #DDS_RETCODE_ERROR :表示未归类错误，详细参见日志；
 */

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_delete_contained_entities(
    DDS_Subscriber* self);

/**
 * @fn  DCPSDLL DDS_Entity* DDS_Subscriber_as_entity(DDS_Subscriber* self);
 *
 * @ingroup CSubscription
 *
 * @brief   将参与者转化为“父类”实体对象。
 *
 * @param [in,out]  self    指向目标。
 *
 * @return  空表示转化失败，否则指向“父类”实体对象。
 */

DCPSDLL DDS_Entity* DDS_Subscriber_as_entity(DDS_Subscriber* self);

/**
 * @struct DDS_SubscriberSeq 
 *
 * @ingroup CSubscription
 *
 * @brief   声明 DDS_Subscriber 指针的序列类型，参见 #DDS_USER_SEQUENCE_C 。
 */
DDS_SEQUENCE_C(DDS_SubscriberSeq, DDS_Subscriber*);

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_lookup_named_datareaders(
    DDS_Subscriber* self,
    const char* pattern, DDS_StringSeq* reader_names);

DCPSDLL DDS_DataReader* DDS_Subscriber_lookup_datareader_by_name(
    DDS_Subscriber* self, const DDS_Char* name);

DCPSDLL const DDS_Char* DDS_Subscriber_get_entity_name(
    DDS_Subscriber* self);

DCPSDLL DDS_DomainParticipant* DDS_Subscriber_get_factory(
    DDS_Subscriber* self);

DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_from_xml_string(
    DDS_Subscriber* self,
    const DDS_Char* xml_content);

DCPSDLL DDS_ReturnCode_t DDS_Subscriber_to_xml(
    DDS_Subscriber* self,
    const DDS_Char** result,
    DDS_Boolean contained_qos);

#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

/**
 * @fn  DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_with_topic_and_qos_profile( DDS_Subscriber* self, const DDS_Char* topicName, DDS_TypeSupport* typeSupport, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DataReaderListener* drListener, DDS_StatusKindMask mask) DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_with_qos_profile( DDS_Subscriber* self, DDS_TopicDescription *topic, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DataReaderListener *dr_listener, DDS_StatusKindMask mask);
 *
 * @ingroup CSubscription
 *
 * @brief   创建指定主题名称的数据读者，当主题名称关联的主题未创建时，将自动创建，否则将利用已经创建的主题创建数据读者。
 *
 * @param [in,out]  self        指明订阅者。
 * @param   topicName           数据读者关联的主题名称。
 * @param [in,out]  typeSupport 数据读者关联的数据类型的类型支持全局对象地址，DDS将为每中数据类型均生成一个全局对象，对象名称规则为： 类型名称TypeSupport_instance 例如零拷贝类型： DDS_ZeroCopyBytesTypeSupport_instance 。
 * @param   library_name        QoS库的名字，不允许为NULL。
 * @param   profile_name        QoS配置的名字，不允许为NULL。
 * @param   qos_name            QoS的名字，允许为NULL，将转换为default字符串。
 * @param [in,out]  drListener  数据读者的监听器。
 * @param   mask                监听器掩码。
 *
 * @return  NULL表示失败，否则返回数据读者指针。
 */

DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_with_topic_and_qos_profile(
    DDS_Subscriber* self,
    const DDS_Char* topicName,
    DDS_TypeSupport* typeSupport,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name,
    DDS_DataReaderListener* drListener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_with_qos_profile( DDS_Subscriber* self, DDS_TopicDescription *topic, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DataReaderListener *dr_listener, DDS_StatusKindMask mask);
 *
 * @ingroup CSubscription
 *
 * @brief   从QoS仓库中获取数据读者QoS并用其创建数据读者
 *
 * @param [in,out]  self        指向目标
 * @param [in,out]  topic       数据读者关联的主题
 * @param   library_name        QoS库的名字，不允许为NULL。
 * @param   profile_name        QoS配置的名字，不允许为NULL。
 * @param   qos_name            QoS的名字，允许为NULL，将转换为default字符串。
 * @param [in,out]  dr_listener 为该订阅者设置的监听器，此参数可以为空。 ZRDDS不会接管监听器对象的内存管理，由用户负责释放。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  非NULL表示创建成功，否则表示失败，失败的原因可能为：
 *          - @e a_topic 不是有效的主题对象；
 *          - @e a_topic 的父实体与该订阅者不属于一个域参与者实体；
 *          - @e library_name @e profile_name @e qos_name 指定的QoS中含有无效的QoS或者含有不一致的QoS配置；
 *          - 分配内存错误等未归类错误，详细参见日志。
 */
DCPSDLL DDS_DataReader* DDS_Subscriber_create_datareader_with_qos_profile(
    DDS_Subscriber* self,
    DDS_TopicDescription *topic,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name,
    DDS_DataReaderListener *dr_listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos_with_profile( DDS_Subscriber* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CSubscription
 *
 * @brief   从QoS仓库中获取数据读者QoS并将其设置为默认DataReaderQos
 *
 * @param [in,out]  self    指向目标
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_default_datareader_qos_with_profile(
    DDS_Subscriber* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_qos_with_profile( DDS_Subscriber* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CSubscription
 *
 * @brief   从QoS仓库中获取订阅者QoS并将其设置到订阅者中
 *
 * @param [in,out]  self    指向目标
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_Subscriber_set_qos_with_profile(
    DDS_Subscriber* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

#ifdef __cplusplus
}
#endif

#endif /* Subscriber_h__*/
