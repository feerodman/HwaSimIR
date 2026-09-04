/**
 * @file:       DomainParticipantFactory.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DomainParticipantFactory_h__
#define DomainParticipantFactory_h__

#include "DomainParticipant.h"
#include "DomainParticipantFactoryQos.h"
#include "DomainParticipantQos.h"
#include "DomainParticipantListener.h"
#include "DomainId_t.h"
#include "ZRDynamicData.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance();
 *
 * @ingroup CDomain
 *
 * @brief   获取域参与者单例对象。
 *
 * @return  当前可能的返回值：
 *          - NULL表示创建单例对象失败，可能的原因：
 *              - 初始化资源失败，详细参见日志；
 *              - Mac或者时间验证失败；
 *          - 获取成功的单例对象； 在整个应用程序第一次获取实例时，应进行如下的检查：
 *          @code{cpp}
 *          if (DDS_DomainParticipantFactory_get_instance() == NULL)
 *          {
 *              // get domainparticipant factory faild.
 *          }
 *          @endcode
 *          
 * @note    该方法线程不安全，多个线程同时创建实例不安全，获取实例安全。
 */

DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance();

/**
 * @fn  DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance_w_qos(const DDS_DomainParticipantFactoryQos* qoslist);
 *
 * @ingroup CDomain
 *          
 * @brief   使用指定的DomainParticipantFactoryQos来获取域参与者工厂的单例对象.
 *
 * @param   qoslist 指定的DomainParticipantFactoryQos.
 *
 * @return  当前可能的返回值：
 *          - NULL表示创建单例对象失败，可能的原因：
 *              - 初始化资源失败，详细参见日志；
 *              - Mac或者时间验证失败；
 *          - 获取成功的单例对象。  
 *
 * @note    该方法线程不安全，多个线程同时创建实例不安全，获取实例安全。.
 *
 */

DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance_w_qos(const DDS_DomainParticipantFactoryQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_finalize_instance();
 *
 * @ingroup CDomain
 *
 * @brief   析构单例，该方法同样是线程不安全的，多个线程同时调用该函数，可能会出问题。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示析构成功；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :由该域参与者工厂创建的域参与者未删除完；
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_finalize_instance();

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_create_participant( DDS_DomainParticipantFactory* self, const DDS_DomainId_t domainId, const DDS_DomainParticipantQos* qoslist, DDS_DomainParticipantListener* listener, DDS_StatusKindMask mask);
 *
 * @ingroup CDomain
 *
 * @brief   创建一个新的域参与者实体，并设置QoS以及监听器，域参与者的创建表明应用程序打算加入@e domainId 指定的域中进行通信。
 *
 * @param [in,out]  self        指向目标。
 * @param   domainId            表明需要加入的域号，取值范围为[0-232]。
 * @param   qoslist             表示为该域参与者设置的QoS， #DDS_DOMAINPARTICIPANT_QOS_DEFAULT 表明使用域参与者工厂中保存的默认的QoS。
 * @param [in,out]  listener    为该域参与者设置的监听器。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  创建成功指向创建成功的域参与者实体对象，否则返回NULL，失败的原因可能为：
 *          - 分配空间失败或者初始化资源失败，具体的错误信息参见日志；
 *          - @e qoslist 含有无效值或者含有不一致的QoS。
 */

DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_create_participant(
    DDS_DomainParticipantFactory* self,
    const DDS_DomainId_t domainId,
    const DDS_DomainParticipantQos* qoslist,
    DDS_DomainParticipantListener* listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant( DDS_DomainParticipantFactory* self, DDS_DomainParticipant* dp);
 *
 * @ingroup CDomain
 *
 * @brief   该方法删除指定的域参与者。在调用该方法之前需要保证该域参与者的所有子实体都已经被删除。否则将会返回错误
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  dp      指明需要删除的域参与者实体。
 *
 * @return  可能的返回值如下：
 *          - #DDS_RETCODE_OK :表示删除成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :用户提供的参数不是有效的域参与者对象，包括NULL值；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :指明的域参与者不满足删除条件，即还有子实体未删除；
 *          - #DDS_RETCODE_ERROR :指明的域参与者不是由工厂创建。
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_participant(
    DDS_DomainParticipantFactory* self, 
    DDS_DomainParticipant* dp);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_contained_entities( DDS_DomainParticipantFactory* self);
 *
 * @ingroup CDomain
 *
 * @brief   删除所有的域参与者及其子实体，该函数将尝试删除所有满足删除条件的子实体。
 *
 * @param [in,out]  self    指向目标。
 *
 * @return  可能的返回值如下：
 *          - #DDS_RETCODE_OK :表示删除成功；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET :某些域参与者不满足删除条件，即还有子实体未删除；
 *          - #DDS_RETCODE_ERROR :指明的域参与者不是由工厂创建。.
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_contained_entities(
    DDS_DomainParticipantFactory* self);

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_lookup_participant( DDS_DomainParticipantFactory* self, DDS_DomainId_t domainId);
 *
 * @ingroup CDomain
 *
 * @brief   该方法在指定域@e domainId 下查找域参与者，如果有多个域参与者，则返回其中一个。
 *
 * @param [in,out]  self    指向目标。
 * @param   domainId    需要查找的域。
 *
 * @return  当前可能的返回值：
 *          - NULL表示查找失败，即本地在该域下当前没有域参与者；
 *          - 非空表示查找的结果，返回的结果为该域下域参与者指针最小的域参与者。
 */

DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_lookup_participant(
    DDS_DomainParticipantFactory* self, 
    DDS_DomainId_t domainId);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos( DDS_DomainParticipantFactory* self, const DDS_DomainParticipantQos* qoslist);
 *
 * @ingroup CDomain
 *
 * @brief   该方法设置该工厂为域参与者保存的默认QoS配置。
 *
 * @details 默认的QoS在创建新的域参与者时指定QoS参数为 #DDS_DOMAINPARTICIPANT_QOS_DEFAULT 时使用的QoS配置，
 *          使用特殊的值 #DDS_DOMAINPARTICIPANT_QOS_DEFAULT 域参与者QoS中的各个配置的设置为默认值。
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

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos(
    DDS_DomainParticipantFactory* self, 
    const DDS_DomainParticipantQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_default_participant_qos( DDS_DomainParticipantFactory* self, DDS_DomainParticipantQos* qoslist);
 *
 * @ingroup CDomain
 *
 * @brief   该方法获取该工厂为域参与者保存的默认QoS配置。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  qoslist 出口参数表示获取的结果.
 *
 * @return  当前的返回值类型：
 *          - #DDS_RETCODE_OK :表示设置成功；
 *          - #DDS_RETCODE_ERROR :表示失败，例如复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_default_participant_qos(
    DDS_DomainParticipantFactory* self, 
    DDS_DomainParticipantQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos( DDS_DomainParticipantFactory* self, const DDS_DomainParticipantFactoryQos* qoslist);
 *
 * @ingroup CDomain
 *
 * @brief   该方法设置域参与者工厂本身的QoS，特殊的值 #DDS_DOMAINPARTICIPANT_FACTORY_QOS_DEFAULT 表示设置为默认值。
 *
 * @param [in,out]  self    指向目标。
 * @param   qoslist 指明目标QoS。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK :表示设置成功；
 *          - #DDS_RETCODE_BAD_PARAMETER :表示@e qoslist为空，或者@e qoslist 具有无效值；
 *          - #DDS_RETCODE_INCONSISTENT :表示@e qoslist 中具有不相容的配置；
 *          - #DDS_RETCODE_ERROR :表示失败，例如复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos(
    DDS_DomainParticipantFactory* self, 
    const DDS_DomainParticipantFactoryQos* qoslist);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_qos( DDS_DomainParticipantFactory* self, DDS_DomainParticipantFactoryQos* qoslist);
 *
 * @ingroup CDomain
 *
 * @brief   该方法获取为域参与者工厂设置的QoS。
 *
 * @details 如果调用该方法之前未调用过 #DDS_DomainParticipantFactory_set_qos ，则返回系统默认的QoS配置，
 *          否则返回 #DDS_DomainParticipantFactory_set_qos 的结果。
 *
 * @param [in,out]  self    指向目标。
 * @param [in,out]  qoslist 出口参数，存储获取的结果。
 *
 * @return  当前的返回值类型：
 *          - #DDS_RETCODE_OK :表示设置成功；
 *          - #DDS_RETCODE_ERROR :表示失败，例如复制QoS时发生错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_qos(
    DDS_DomainParticipantFactory* self, 
    DDS_DomainParticipantFactoryQos* qoslist);

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_new_instance(
    DDS_DomainParticipantFactoryQos* qos);

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_delete_instance(
    DDS_DomainParticipantFactory* self);

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_add_to_type_library_from_xml_string(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* xml_content);

DCPSDLL TypeCodeHeader* DDS_DomainParticipantFactory_lookup_type_by_name(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* name);

DCPSDLL ZRDynamicData* DDS_DomainParticipantFactory_gen_dynamic_data(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* type_name,
    const DDS_Char* xml_content);

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_named_participants(
    DDS_DomainParticipantFactory* self,
    const char* pattern,
    DDS_StringSeq* participant_names);

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_named_types(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* pattern,
    DDS_Long reference_depth,
    DDS_StringSeq* participant_names);

DCPSDLL DDS_Entity* DDS_DomainParticipantFactory_lookup_entity_by_name(
    DDS_DomainParticipantFactory* self, const DDS_Char* name);

DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_lookup_participant_by_name(
    DDS_DomainParticipantFactory* self, const DDS_Char* name);

DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_type_to_xml(
    DDS_DomainParticipantFactory* self, const DDS_Char* type_name, const DDS_Char** result);

DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_create_participant_from_xml_string(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* xml_content);

#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_add_qos_library( DDS_DomainParticipantFactory* self, const DDS_Char* xml_representation);
 *
 * @ingroup CDomain
 *
 * @brief   添加一个QoS库
 *
 * @param [in,out]  self        指向目标
 * @param   xml_representation  以XML字符串表示的QoS库。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示添加成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示添加失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_add_qos_library(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* xml_representation);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_qos_library_to_xml( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name, const DDS_Char** result);
 *
 * @ingroup CDomain
 *
 * @brief   将QoS库转换为XML
 *
 * @param [in,out]  self        指向目标
 * @param   qos_library_name    QoS库的名字，不允许为NULL。
 * @param [out] result          转换得到的结果
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示转换成功；
 *         - #DDS_RETCODE_ERROR :表示转换失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_qos_library_to_xml(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name,
    const DDS_Char** result);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_qos_libraries( DDS_DomainParticipantFactory* self, const DDS_Char* pattern, DDS_StringSeq* qos_library_names);
 *
 * @ingroup CDomain
 *
 * @brief   查找名称符合pattern限定的QoS库
 *
 * @param [in,out]  self                指向目标
 * @param   pattern                     查找模式，可以支持*及?，*代表任意数量的任意字符，?代表任意单个字符。
 * @param [in,out]  qos_library_names   查找得到的QoS库名字列表。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示查找成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示查找失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_qos_libraries(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* pattern,
    DDS_StringSeq* qos_library_names);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_remove_qos_library( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name);
 *
 * @ingroup CDomain
 *
 * @brief   从指定QoS库中移除一个QoS配置
 *
 * @param [in,out]  self        指向目标
 * @param   qos_library_name    需要被移除的QoS库的名称
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示移除成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示移除失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_remove_qos_library(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_add_qos_profile( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name, const DDS_Char* xml_representation);
 *
 * @ingroup CDomain
 *
 * @brief   在指定QoS库中添加一个QoS配置
 *
 * @param [in,out]  self        指向目标
 * @param   qos_library_name    需要添加QoS配置的QoS库。
 * @param   xml_representation  以XML表示的QoS配置内容。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示添加成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示添加失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_add_qos_profile(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name,
    const DDS_Char* xml_representation);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_reload_qos_profiles( DDS_DomainParticipantFactory* self);
 *
 * @ingroup CDomain
 *
 * @brief   根据QosProfileQosPolicy（有关QoS配置的配置）配置重新加载QoS配置到库中，在DomainParticipantFactory初始化时隐式调用，
 *          异常处理逻辑：
 *          - 当配置的路径不存在时，忽略该条并提示用户，返回RETCODE_OK
 *          - 当配置的XML存在不可忽略的错误时（XML格式错误等）,提示用户，并返回RETCODE_ERROR.
 *        其中的UserData、GroupData、TopicData可以支持使用String方式和Sequence方式设置
 *          - String方式为将字符串直接作为value节点的text
 *          - Sequence方式为将value当成Sequence使用（即设置其sequenceMaxLength属性），并将每个元素作为item写入
 *
 * @param [in,out]  self    指向目标
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功或配置的路径不存在；
 *         - #DDS_RETCODE_BAD_PARAMETER ：表示参数不正确导致的添加QoS库错误
 *         - #DDS_RETCODE_ERROR :表示XML存在错误导致QoS库添加失败；
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_reload_qos_profiles(
    DDS_DomainParticipantFactory* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_qos_profiles( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name, const DDS_Char* pattern, DDS_StringSeq* qos_profile_names);
 *
 * @ingroup CDomain
 *
 * @brief   在指定QoS库中查找名字符合pattern的QoS配置
 *
 * @param [in,out]  self                指向目标
 * @param   qos_library_name            QoS库的名字，不允许为NULL。
 * @param   pattern                     查找模式，可以支持*及?，*代表任意数量的任意字符，?代表任意单个字符
 * @param [in,out]  qos_profile_names   查找得到的QoS配置名字列表
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示查找成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示查找失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_lookup_qos_profiles(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name,
    const DDS_Char* pattern,
    DDS_StringSeq* qos_profile_names);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_qos_profile_to_xml( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name, const DDS_Char* qos_profile_name, const DDS_Char** result);
 *
 * @ingroup CDomain
 *
 * @brief   将一个QoS配置转换为XML
 *
 * @param [in,out]  self        指向目标
 * @param   qos_library_name    QoS库的名称
 * @param   qos_profile_name    QoS配置的名称
 * @param [out] result          转换得到的结果
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示转换成功；
 *         - #DDS_RETCODE_ERROR :表示转换失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_qos_profile_to_xml(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name,
    const DDS_Char* qos_profile_name,
    const DDS_Char** result);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_remove_qos_profile( DDS_DomainParticipantFactory* self, const DDS_Char* qos_library_name, const DDS_Char* qos_profile_name);
 *
 * @ingroup CDomain
 *
 * @brief   从指定QoS库中移除一个QoS配置
 *
 * @param [in,out]  self        指向目标
 * @param   qos_library_name    QoS库的名称
 * @param   qos_profile_name    QoS配置的名称
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示移除成功；
 *         - #DDS_RETCODE_BAD_PARAMETER :表示参数存在错误，如缺失参数；
 *         - #DDS_RETCODE_ERROR :表示移除失败。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_remove_qos_profile(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* qos_library_name,
    const DDS_Char* qos_profile_name);

/**
 * @fn  DCPSDLL void DDS_DomainParticipantFactory_unload_qos_profiles( DDS_DomainParticipantFactory* self);
 *
 * @ingroup CDomain
 *
 * @brief   卸载所有的QoS配置
 *
 * @param [in,out]  self    指向目标
 */
DCPSDLL void DDS_DomainParticipantFactory_unload_qos_profiles(
    DDS_DomainParticipantFactory* self);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos_with_profile( DDS_DomainParticipantFactory* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取域参与者QoS并将其设为默认域参与者QoS
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
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_default_participant_qos_with_profile(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_datareader_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_DataReaderQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取数据读者QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_datareader_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_DataReaderQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_datawriter_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_DataWriterQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取数据写者QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_datawriter_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_DataWriterQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_participant_factory_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_DomainParticipantFactoryQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取域参与者工厂QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_participant_factory_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_DomainParticipantFactoryQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_participant_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_DomainParticipantQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取域参与者QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_participant_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_DomainParticipantQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_publisher_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_PublisherQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取发布者QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_publisher_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_PublisherQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_subscriber_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_SubscriberQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取订阅者QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_subscriber_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_SubscriberQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_topic_qos_from_profile( DDS_DomainParticipantFactory* self, DDS_TopicQos* qos, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取主题QoS的配置
 *
 * @param [in,out]  self    指向目标
 * @param [in,out]  qos     获取到的QoS
 * @param   library_name    QoS库的名字，不允许为NULL。
 * @param   profile_name    QoS配置的名字，不允许为NULL。
 * @param   qos_name        QoS的名字，允许为NULL，将转换为default字符串。
 *
 * @return  当前可能的返回值如下：
 *         - #DDS_RETCODE_OK :表示设置成功；
 *         - #DDS_RETCODE_ERROR :表示未知错误导致的设置错误。
 */
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_get_topic_qos_from_profile(
    DDS_DomainParticipantFactory* self,
    DDS_TopicQos* qos,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos_with_profile( DDS_DomainParticipantFactory* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取QoS配置并设置到域参与者工厂
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
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_set_qos_with_profile(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_create_participant_with_qos_profile( DDS_DomainParticipantFactory* self, const DDS_DomainId_t domainId, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DomainParticipantListener* listener, DDS_StatusKindMask mask);
 *
 * @ingroup CDomain
 *
 * @brief   从QoS仓库获取QoS配置并用其创建域参与者
 *
 * @param [in,out]  self        指向目标
 * @param   domainId            域Id
 * @param   library_name        QoS库的名字，不允许为NULL。
 * @param   profile_name        QoS配置的名字，不允许为NULL。
 * @param   qos_name            QoS的名字，允许为NULL，将转换为default字符串。
 * @param [in,out]  listener    为该域参与者设置的监听器。
 * @param   mask                设置应用程序感兴趣的状态，只有应用程序感兴趣的状态发生变化时，才会通知应用程序。
 *
 * @return  当前可能的返回值如下：
 *          - 非空表示创建域参与者成功；
 *          - NULL表示创建失败。
 */
DCPSDLL DDS_DomainParticipant* DDS_DomainParticipantFactory_create_participant_with_qos_profile(
    DDS_DomainParticipantFactory* self,
    const DDS_DomainId_t domainId,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name,
    DDS_DomainParticipantListener* listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance_w_profile( const DDS_Char* qosFilePath, const DDS_Char* libName, const DDS_Char* profileName, const DDS_Char* qosName);
 *
 * @ingroup CDomain
 *
 * @brief   以指定参数初始化DDS域参与者工厂。
 *
 * @param   qosFilePath qos配置文件路径，当为NULL时，将默认使用运行目录的 ZRDDS_QOS_PROFILES.xml 文件。
 * @param   libName     Qos库名称，不允许为NULL。
 * @param   profileName Qos配置名称，不允许为NULL。
 * @param   qosName     Qos名称，允许为空，将转化为default字符串。
 *
 * @return  NULL表示失败，否则返回单例指针。
 */

DCPSDLL DDS_DomainParticipantFactory* DDS_DomainParticipantFactory_get_instance_w_profile(
    const DDS_Char* qosFilePath, 
    const DDS_Char* libName, 
    const DDS_Char* profileName, 
    const DDS_Char* qosName);

#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

#ifdef _ZRDDSSECURITY
DCPSDLL DDS_ReturnCode_t DDS_DomainParticipantFactory_load_security_plugin(
    DDS_DomainParticipantFactory* self,
    const DDS_Char* plugin_name,
    const DDS_Char* file_name,
    const DDS_Char* retrieve_instance_func_name,
    const DDS_Char* finalize_instance_func_name);
#endif /* _ZRDDSSECURITY */

#ifdef __cplusplus
}
#endif

#endif /* DomainParticipantFactory_h__*/
