/**
 * @file:       Topic.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef TopicDescription_h__
#define TopicDescription_h__

#include "InconsistentTopicStatus.h"
#include "ReturnCode_t.h"
#include "TopicQos.h"
#include "TopicListener.h"
#include "StatusKindMask.h"
#include "Entity.h"

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief	内置域参与者信息数据读者关联的主题名称。@ingroup   CoreVar */
DCPSDLL extern const DDS_Char* BUILTIN_PARTICIPANT_TOPIC_NAME;
/** @brief	内置数据写者者信息数据读者关联的主题名称。@ingroup   CoreVar */
DCPSDLL extern const DDS_Char* BUILTIN_PUBLICATION_TOPIC_NAME;
/** @brief	内置数据读者信息数据读者关联的主题名称。@ingroup   CoreVar */
DCPSDLL extern const DDS_Char* BUILTIN_SUBSCRIPTION_TOPIC_NAME;

/**
 * @fn  DCPSDLL const DDS_Char* DDS_TopicDescription_get_name( const DDS_TopicDescription* self);
 *
 * @ingroup CTopic
 *
 * @brief   获取该主题的名称。
 *
 * @param [in,out]  self    指向目标。
 *
 * @return  主题名称。
 */

DCPSDLL const DDS_Char* DDS_TopicDescription_get_name(
    const DDS_TopicDescription* self);

/**
 * @fn  DCPSDLL const DDS_Char* DDS_TopicDescription_get_type_name( const DDS_TopicDescription* self);
 *
 * @ingroup CTopic
 *
 * @param [in,out]  self    指向目标。  
 *  
 * @brief   获取与该主题关联的数据类型在域参与者中注册的名称。
 *
 * @return  数据类型的名称。
 */

DCPSDLL const DDS_Char* DDS_TopicDescription_get_type_name(
    const DDS_TopicDescription* self);

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_TopicDescription_get_participant( const DDS_TopicDescription* self);
 *
 * @ingroup CTopic
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @brief   获取创建该主题所属的域参与者。
 *
 * @return  返回该主题所属的域参与者对象。
 */

DCPSDLL DDS_DomainParticipant* DDS_TopicDescription_get_participant(
    const DDS_TopicDescription* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Topic_get_inconsistent_topic_status( DDS_Topic* self, DDS_InconsistentTopicStatus* status);
 *
 * @ingroup CTopic
 *
 * @brief   获取该主题关联的 #DDS_INCONSISTENT_TOPIC_STATUS 状态.
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  status  出口参数表示当前的状态。
 *
 * @return  当前总是返回 #DDS_RETCODE_OK 表示获取成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Topic_get_inconsistent_topic_status(
    DDS_Topic* self, 
    DDS_InconsistentTopicStatus* status);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Topic_set_qos( DDS_Topic* self, const DDS_TopicQos* qoslist);
 *
 * @ingroup CTopic
 *
 * @param [in,out]  self    指向目标。
 * @param   qoslist 表示用户想要设置的QoS配置。
 *
 * @details 可以使用特殊值 #DDS_TOPIC_QOS_DEFAULT 。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示获取成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :表示 @e qoslist 含有无效的QoS配置；
 *          - #DDS_RETCODE_INCONSISTENT :表示 @e qoslist 含有不兼容的QoS配置；
 *          - #DDS_RETCODE_IMMUTABLE_POLICY :表示用户尝试修改使能后不可变的QoS配置；
 *          - #DDS_RETCODE_ERROR :表示未归类的错误，错误详细信息输出在日志中；
 */

DCPSDLL DDS_ReturnCode_t DDS_Topic_set_qos(
    DDS_Topic* self, 
    const DDS_TopicQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Topic_get_qos( DDS_Topic* self, DDS_TopicQos* qos);
 *
 * @ingroup CTopic
 *
 * @brief   获取该主题的QoS配置。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  qos 出口参数，用于保存该主题的的QoS配置。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示获取成功；
 *          - #DDS_RETCODE_ERROR :表示失败，原因可能为复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Topic_get_qos(
    DDS_Topic* self, 
    DDS_TopicQos* qos);

/**
 * @fn  DCPSDLL DDS_TopicListener* DDS_Topic_get_listener( DDS_Topic* self);
 *
 * @ingroup CTopic
 *
 * @brief   该方法获取通过 #DDS_Topic_set_listener 方法或者创建时为主题设置的监听器对象。
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @return  当前可能的返回值：
 *          - NULL表示未设置监听器；
 *          - 非空表示应用通过 #DDS_Topic_set_listener 或者在创建时设置的监听器对象。
 */

DCPSDLL DDS_TopicListener* DDS_Topic_get_listener(
    DDS_Topic* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Topic_set_listener( DDS_Topic* self, DDS_TopicListener* listener, DDS_StatusKindMask mask);
 *
 * @ingroup CTopic
 *
 * @brief   设置该主题的的监听器。
 *
 * @details  本方法将覆盖原有监听器，如果设置空对象表示清除原先设置的监听器。
 *
 * @param [in,out]  self    指向目标。
 *                  
 * @param [in,out]  listener  为该主题设置的监听器对象。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  当前总是返回 #DDS_RETCODE_OK 表示设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Topic_set_listener(
    DDS_Topic* self,
    DDS_TopicListener* listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_Entity* DDS_Topic_as_entity(DDS_Topic* topic);
 *
 * @ingroup CTopic
 *
 * @brief   将主题转化为“父类”实体对象。
 *
 * @param [in,out]  topic    指向目标。
 *
 * @return  空表示转化失败，否则指向“父类”实体对象
 */

DCPSDLL DDS_Entity* DDS_Topic_as_entity(DDS_Topic* topic);

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

DCPSDLL DDS_ReturnCode_t DDS_Topic_to_xml(
    DDS_Topic* self,
    const DDS_Char** result,
    DDS_Boolean contained_qos);

DCPSDLL const DDS_Char* DDS_Topic_get_entity_name(
    DDS_Topic* self);

DCPSDLL DDS_DomainParticipant* DDS_Topic_get_factory(
    DDS_Topic* self);

#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_Topic_set_qos_with_profile( DDS_Topic* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CTopic
 *
 * @brief   从QoS仓库中获取主题QoS并将其设置到主题中
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
DCPSDLL DDS_ReturnCode_t DDS_Topic_set_qos_with_profile(
    DDS_Topic* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);
#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

#ifdef __cplusplus
}
#endif

#endif /* TopicDescription_h__*/
