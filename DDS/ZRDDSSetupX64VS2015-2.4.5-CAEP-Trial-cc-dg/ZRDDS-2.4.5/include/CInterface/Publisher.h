/**
 * @file:       Publisher.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef Publisher_h__
#define Publisher_h__

#include "DataWriter.h"
#include "Topic.h"
#include "PublisherQos.h"
#include "PublisherListener.h"
#include "Entity.h"
#include "ZRDDSTypeSupport.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @fn  DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter( DDS_Publisher *publisher, DDS_Topic *topic, const DDS_DataWriterQos *writerQos, DDS_DataWriterListener*writerListener, DDS_StatusKindMask mask);
 *
 * @ingroup CPublication
 *          
 * @brief    创建DataWriter。
 *
 * @param [in,out]  publisher  指向目标。
 * @param    topic             用于关联DataWriter的Topic实例指针。
 * @param    writerQos         DataWriter的QoS配置。
 * @param    writerListener    需要安装到DataWriter的Listener。
 * @param    mask              状态掩码，指明需要被Listener捕获的状态。
 *
 * @return  可能的返回值如下：
 *           - NULL创建数据写者失败；
 *           - 创建的数据写者。
 */

DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter(
    DDS_Publisher *publisher,
    DDS_Topic *topic,
    const DDS_DataWriterQos *writerQos,
    DDS_DataWriterListener*writerListener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_delete_datawriter( DDS_Publisher *publisher, DDS_DataWriter *writer);
 *
 * @ingroup CPublication
 *          
 * @brief	删除一个DataWriter。
 *
 * @param [in,out]  publisher   指向目标。
 * @param           writer      需要被删除的DataWriter。
 *
 * @return  可能的返回值如下：
 *          - #DDS_RETCODE_BAD_PARAMETER 传入的参数有误。
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET 传入的DataWriter有误，具体内容见日志记录。
 *          - #DDS_RETCODE_ERROR 未归类错误，具体内容见日志记录。
 *          - #DDS_RETCODE_OK 删除成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_delete_datawriter(
    DDS_Publisher *publisher,
    DDS_DataWriter *writer);

/**
 * @fn  DCPSDLL DDS_DataWriter* DDS_Publisher_lookup_datawriter( DDS_Publisher *publisher, const DDS_Char *topicName);
 *
 * @ingroup CPublication
 *          
 * @brief    根据主题名查找对应的数据写者。
 *
 * @param [in,out]  publisher        指向目标。
 * @param    topicName  关联到数据写者主题的主题名称。
 *
 * @return   可能的返回值如下：
 *           - NULL未查找到数据写者；
 *           - 查到到的首个数据写者。
 */

DCPSDLL DDS_DataWriter* DDS_Publisher_lookup_datawriter(
    DDS_Publisher *publisher,
    const DDS_Char *topicName);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_suspend_publications( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    挂起数据发布。
 *                
 * @details  当数据发布被挂起之后，该发布者创建的所有数据写者发布的数据都不再被发出。
 *           直到挂起被完全取消之后才会继续发出数据。
 *           该函数可以被多次调用，但是取消挂起也需要配对使用，亦即如果该函数被多次调用之后，必须取消相同次数才能重新开始发布数据。
 *
 * @param [in,out]  publisher        指向目标。
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_NOT_ENABLED 该发布者未被使能；
 *           - #DDS_RETCODE_OK 操作成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_suspend_publications(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_resume_publications( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    恢复数据发布，与 #DDS_Publisher_suspend_publications 配对使用。
 *           
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_NOT_ENABLED 该发布者未被使能；
 *           - #DDS_RETCODE_PRECONDITION_NOT_MET 该发布者未调用过 #DDS_Publisher_suspend_publications ；
 *           - #DDS_RETCODE_OK 操作成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_resume_publications(
    DDS_Publisher *publisher);

#ifdef _ZRDDS_INCLUDE_PRESENTATION_QOS

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_begin_coherent_changes( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    开始进行“连续”的修改。
 *                  
 * @details  当该数据写者所属的发布者的QoS #DDS_PresentationQosPolicy::coherent_access == true，使用此函数开始进行“原子”操作。
 *           在该函数调用之后直到 #DDS_Publisher_end_coherent_changes 被调用之前发布的所有数据会被接收端一次性访问到。
 *           亦即在 #DDS_Publisher_end_coherent_changes 被调用之前，所有提交的数据对于接收端来说都是不可访问的。
 *           而在 #DDS_Publisher_end_coherent_changes 调用之后，接收端会收到一批数据。
 *
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return	可能的返回值如下：
 *           - #DDS_RETCODE_NOT_ENABLED 发布者未使能；
 *           - #DDS_RETCODE_ERROR 未归类错误
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_begin_coherent_changes(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_end_coherent_changes( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    结束“连续”的修改，使接收端可以访问修改的值，必须在 #DDS_Publisher_begin_coherent_changes 调用之后调用。
 *
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_NOT_ENABLED 发布者未使能。
 *           - #DDS_RETCODE_PRECONDITION_NOT_MET 发布者未调用过 #DDS_Publisher_begin_coherent_changes 。
 *           - #DDS_RETCODE_ERROR 未归类错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_end_coherent_changes(
    DDS_Publisher *publisher);

#endif /* _ZRDDS_INCLUDE_PRESENTATION_QOS */

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_wait_for_acknowledgments( DDS_Publisher *publisher, const DDS_Duration_t *maxWait);
 *
 * @ingroup CPublication
 *
 * @brief    阻塞调用该函数的线程直到该发布者创建的数据写者发送的所有数据都被接收端所响应或者超时。
 *
 * @param [in,out]  publisher        指向目标。
 * @param    maxWait    该函数的最长阻塞时间。
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_NOT_ENABLED 发布者未使能。
 *           - #DDS_RETCODE_TIMEOUT 等待超时。
 *           - #DDS_RETCODE_ERROR 未归类错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_wait_for_acknowledgments(
    DDS_Publisher *publisher,
    const DDS_Duration_t *maxWait);

/**
 * @fn  DCPSDLL DDS_DomainParticipant* DDS_Publisher_get_participant( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    获取创建该发布者的域参与者。
 *
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return   可能的返回值如下：
 *           - 创建该Publisher的DomainParticipant。
 */

DCPSDLL DDS_DomainParticipant* DDS_Publisher_get_participant(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_delete_contained_entities( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    删除Publisher所包含的所有DataWriter。
 *
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_ERROR 未归类错误；
 *           - #DDS_RETCODE_OK 删除成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_delete_contained_entities(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos( DDS_Publisher *publisher, const DDS_DataWriterQos *writerQos);
 *
 * @ingroup CPublication
 *
 * @brief    设置数据写者的默认QoS。
 *
 * @details  当创建数据写者时可以使用 #DDS_DATAWRITER_QOS_DEFAULT 值作为DataWriterQoS传入。
 *           如果用户使用了 #DDS_DATAWRITER_QOS_DEFAULT ，具体的QoS配置将由该函数调用时传入的QoS决定。
 *
 * @param [in,out]  publisher        指向目标。
 * @param    writerQos 需要设置的数据写者QoS，如果使用DEFAULT_DATAWRITER_QOS作为参数调用该函数，设置的默认QoS将被重置。
 *
 * @return   参见 #FooDataWriter_set_qos 。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos(
    DDS_Publisher *publisher,
    const DDS_DataWriterQos *writerQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_get_default_datawriter_qos( DDS_Publisher *publisher, DDS_DataWriterQos *writerQos);
 *
 * @ingroup CPublication
 *
 * @brief    获取由 #DDS_Publisher_set_default_datawriter_qos 设置的DataWriterQos。
 *
 * @param [in,out]  publisher        指向目标。
 * @param [in,out]   writerQos 获取到的数据写者QoS
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_ERROR 未归类错误；
 *           - #DDS_RETCODE_OK 获取成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_get_default_datawriter_qos(
    DDS_Publisher *publisher,
    DDS_DataWriterQos *writerQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_copy_from_topic_qos( DDS_DataWriterQos *writerQos, const DDS_TopicQos *topicQos);
 *
 * @ingroup CPublication
 *
 * @brief    使用TopicQos中的对应项赋值DataWriterQos。
 *
 * @param    topicQos             主题QoS，作为拷贝的数据源。
 * @param [in,out]   writerQos    数据写者QoS，保存拷贝结果。
 *
 * @return	可能的返回值如下：
 *           - #DDS_RETCODE_ERROR 未归类错误；
 *           - #DDS_RETCODE_OK 设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_copy_from_topic_qos(
    DDS_DataWriterQos *writerQos,
    const DDS_TopicQos *topicQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_qos( DDS_Publisher *publisher, const DDS_PublisherQos *publisherQos);
 *
 * @ingroup CPublication
 *
 * @brief    设置发布者QoS。
 *
 * @param [in,out]  publisher        指向目标。
 * @param    publisherQos 待设置的发布者QoS，可以使用 #DDS_PUBLISHER_QOS_DEFAULT 作为参数以使用在域参与者中保存的默认发布者QoS
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_IMMUTABLE_POLICY 不可在使能后修改的QoS；
 *           - #DDS_RETCODE_INCONSISTENT QoS存在冲突；
 *           - #DDS_RETCODE_BAD_PARAMETER QoS存在不合法的值；
 *           - #DDS_RETCODE_ERROR 未归类错误；
 *           - #DDS_RETCODE_OK 设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_qos(
    DDS_Publisher *publisher,
    const DDS_PublisherQos *publisherQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_get_qos( DDS_Publisher *publisher, DDS_PublisherQos *publisherQos);
 *
 * @ingroup CPublication
 *
 * @brief    获取发布者QoS。
 *
 * @param [in,out]  publisher     指向目标。
 * @param [in,out]   publisherQos 保存获取到的发布者QoS值。
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_OK 获取成功，
 *           - #DDS_RETCODE_ERROR 未归类错误。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_get_qos(
    DDS_Publisher *publisher,
    DDS_PublisherQos *publisherQos);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_listener( DDS_Publisher *publisher, DDS_PublisherListener *publisherListener, DDS_StatusKindMask mask);
 *
 * @ingroup CPublication
 *
 * @brief    设置发布者的监听器。
 *
 * @details  设置新的监听器不会使原有的监听器被释放，用户需要自己管理监听器对象的分配和释放。
 *
 * @param [in,out]  publisher   指向目标。
 * @param    publisherListener  用户提供的监听器对象，可以传入NULL解除监听。
 * @param    mask        状态掩码，指明需要被监听器捕获的状态。
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_OK 设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_listener(
    DDS_Publisher *publisher,
    DDS_PublisherListener *publisherListener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_PublisherListener* DDS_Publisher_get_listener( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief    获取发布者当前的监听器。
 *
 * @param [in,out]  publisher        指向目标。
 *                  
 * @return   可能的返回值如下：
 *           - NULL表示未设置监听器；
 *           - 非空表示应用通过 #DDS_Publisher_set_listener 或者在创建时设置的监听器对象。
 */

DCPSDLL DDS_PublisherListener* DDS_Publisher_get_listener(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_enable( DDS_Publisher *publisher);
 *
 * @ingroup CPublication
 *
 * @brief   手动使能该实体，参见@ref entity-enable 。
 *
 * @param [in,out]  publisher        指向目标。
 *
 * @return  当前可能的返回值：
 *          - #DDS_RETCODE_OK ，表示获取成功；
 *          - #DDS_RETCODE_PRECONDITION_NOT_MET ，表示所属的父实体尚未使能
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_enable(
    DDS_Publisher *publisher);

/**
 * @fn  DCPSDLL DDS_Entity* DDS_Publisher_as_entity(DDS_Publisher* publisher);
 *
 * @ingroup CPublication
 *
 * @brief   将发布者转化为“父类”实体对象。
 *
 * @param [in,out]  publisher    指向目标。
 *
 * @return  空表示转化失败，否则指向“父类”实体对象。
 */

DCPSDLL DDS_Entity* DDS_Publisher_as_entity(DDS_Publisher* publisher);

/**
 * @struct DDS_PublisherSeq 
 *
 * @ingroup CPublication
 *
 * @brief   声明 DDS_Publisher 指针的序列类型，参见 #DDS_USER_SEQUENCE_C 。
 */
DDS_SEQUENCE_C(DDS_PublisherSeq, DDS_Publisher*);

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

/**
 * @fn  DDS_ReturnCode_t DDS_Publisher_lookup_named_datawriters( DDS_Publisher* self, const char* pattern, DDS_StringSeq* writer_names);
 *
 * @ingroup CPublication
 *
 * @brief 查找名称符合pattern限定的数据写者名称
 *
 * @param [in,out]  self        指向目标。
 * @param pattern               查找模式，可以支持*及?，*代表任意数量的任意字符，?代表任意单个字符
 * @param [in,out] writer_names 查找得到数据写者名字列表
 *
 * @return   可能的返回值如下：
 *           - #DDS_RETCODE_BAD_PARAMETER 参数存在错误；
 *           - #DDS_RETCODE_OK 设置成功。
 */

DCPSDLL DDS_ReturnCode_t DDS_Publisher_lookup_named_datawriters(
    DDS_Publisher* self,
    const char* pattern,
    DDS_StringSeq* writer_names);

DCPSDLL DDS_DataWriter* DDS_Publisher_lookup_datawriter_by_name(
    DDS_Publisher* self, const DDS_Char* name);

DCPSDLL const DDS_Char* DDS_Publisher_get_entity_name(
    DDS_Publisher* self);

DCPSDLL DDS_DomainParticipant* DDS_Publisher_get_factory(
    DDS_Publisher* self);

/**
 * @fn  DDS_DataWriter* DDS_Publisher_create_datawriter_from_xml_string( DDS_Publisher* self, const DDS_Char* xml_content);
 *
 * @ingroup CPublication
 *
 * @brief 从XML创建一个数据写者，XML根节点为data_writer
 *
 * @param [in,out]  self        指向目标。
 * @param xml_content XML字符串
 *
 * @return  可能的返回值如下：
 *           - NULL创建数据写者失败；
 *           - 创建的数据写者。
 */

DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter_from_xml_string(
    DDS_Publisher* self,
    const DDS_Char* xml_content);

DCPSDLL DDS_ReturnCode_t DDS_Publisher_to_xml(
    DDS_Publisher* self,
    const DDS_Char** result,
    DDS_Boolean contained_qos);

#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

/**
 * @fn  DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter_with_topic_and_qos_profile( DDS_Publisher* self, const DDS_Char* topicName, DDS_TypeSupport* typeSupport, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DataWriterListener* dwListener, DDS_StatusKindMask mask);
 *
 * @ingroup  CPublication
 *
 * @brief   创建指定主题名称的数据写者，当主题名称关联的主题未创建时，将自动创建， 否则将利用已经创建的主题创建数据写者。
 *
 * @param [in,out]  self        指明发布者。
 * @param   topicName           数据写者关联的主题名称。
 * @param [in,out]  typeSupport 数据写者关联的数据类型的类型支持全局对象地址，DDS将为每中数据类型均生成一个全局对象，对象名称规则为： 类型名称TypeSupport_instance 例如零拷贝类型： DDS_ZeroCopyBytesTypeSupport_instance 。
 * @param   library_name        QoS库的名字，不允许为NULL。
 * @param   profile_name        QoS配置的名字，不允许为NULL。
 * @param   qos_name            QoS的名字，允许为NULL，将转换为default字符串。
 * @param [in,out]  dwListener  数据写者的监听器。
 * @param   mask                监听器掩码。
 *
 * @return  NULL表示失败，否则返回数据写者指针。
 */

DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter_with_topic_and_qos_profile(
    DDS_Publisher* self,
    const DDS_Char* topicName,
    DDS_TypeSupport* typeSupport,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name,
    DDS_DataWriterListener* dwListener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter_with_qos_profile( DDS_Publisher* self, DDS_Topic* topic, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name, DDS_DataWriterListener *dw_listener, DDS_StatusKindMask mask);
 *
 * @ingroup CPublication
 *
 * @brief   从QoS仓库中获取数据写者Qos并用其创建数据写者
 *
 * @param [in,out]  self    指向目标。
 * @param [in]  topic       关联的主题
 * @param   library_name    QoSLibrary名字
 * @param   profile_name    QoSProfile名字
 * @param   qos_name        QoS名字
 * @param [in]  dw_listener 用户提供的监听器对象
 * @param   mask            状态掩码，指明需要被监听器捕获的状态
 *
 * @return  可能的返回值如下：
 *           - NULL创建数据写者失败；
 *           - 创建的数据写者。
 */
DCPSDLL DDS_DataWriter* DDS_Publisher_create_datawriter_with_qos_profile(
    DDS_Publisher* self,
    DDS_Topic* topic,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name,
    DDS_DataWriterListener *dw_listener,
    DDS_StatusKindMask mask);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos_with_profile( DDS_Publisher* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CPublication
 *
 * @brief   从QoS仓库中获取数据写者QoS并将其设为默认数据写者QoS
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
DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_default_datawriter_qos_with_profile(
    DDS_Publisher* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

/**
 * @fn  DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_qos_with_profile( DDS_Publisher* self, const DDS_Char* library_name, const DDS_Char* profile_name, const DDS_Char* qos_name);
 *
 * @ingroup CPublication
 *
 * @brief   从QoS仓库中获取发布者QoS并将其设置到发布者中
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
DCPSDLL DDS_ReturnCode_t DDS_Publisher_set_qos_with_profile(
    DDS_Publisher* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

#ifdef __cplusplus
}
#endif

#endif /* Publisher_h__*/
