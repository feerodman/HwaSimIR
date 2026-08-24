/**
 * @file:       ZRDDSDataReader.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRDDSDataReader_h__
#define ZRDDSDataReader_h__

#include "ReturnCode_t.h"
#include "SampleInfo.h"
#include "SampleStateMask.h"
#include "ViewStateMask.h"
#include "InstanceStateMask.h"
#include "StatusKindMask.h"
#include "LivelinessChangedStatus.h"
#include "RequestedIncompatibleQosStatus.h"
#include "RequestedDeadlineMissedStatus.h"
#include "PublicationBuiltinTopicData.h"
#include "SampleLostStatus.h"
#include "SampleRejectedStatus.h"
#include "SubscriptionMatchedStatus.h"
#include "DataReaderListener.h"
#include "DataReaderQos.h"
#include "Condition.h"
#include "DataReader.h"

/* read/take方法以及instance查询，不包含condition */
#define ZRDDSDataReader_READ_TAKE_METHODS(TDataReader, TTypeSeq, TType)    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read(                           \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take(                \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_next_sample(    \
        TDataReader* self,                                      \
        TType* dataValue,                                       \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take_next_sample(    \
        TDataReader* self,                                      \
        TType* dataValue,                                       \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_instance(       \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* handle,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take_instance(       \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* handle,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_next_instance(  \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* previousHandle,             \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take_next_instance(  \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* previousHandle,             \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_return_loan(         \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos);                        \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_return_recv_buffer(  \
        TDataReader* self,                                      \
        DDS_SampleInfo* sample_info);                           \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_loan_recv_buffer(    \
        TDataReader* self,                                      \
        DDS_SampleInfo* sample_info);                           \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_key_value(       \
        TDataReader* self,                                      \
        TType* keyHolder,                                       \
        const DDS_InstanceHandle_t* handle);                    \
    \
    DCPSDLL DDS_InstanceHandle_t TDataReader##_lookup_instance( \
        TDataReader* self,                                      \
        const TType* instance);                                 \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_data_instance(   \
        TDataReader* self,                                      \
        DDS_InstanceHandleSeq* dataHandles,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);
#ifdef _ZRDDS_INCLUDE_BREAKPOINT_RESUME
#define ZRDDSDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)         \
    DDS_ReturnCode_t TDataReader##_record_data(                   \
    TDataReader* self,                                             \
    DDS_SampleInfoSeq* sampleInfos,                                \
    ZR_BOOLEAN finish);                                             
#else/*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/
#define ZRDDSDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)
#endif /*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/

/* 带condition方法的read/take */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_next_instance_w_condition(  \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        const DDS_InstanceHandle_t* previousHandle,                         \
        DDS_ReadCondition* condition);                                      \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take_next_instance_w_condition(  \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        const DDS_InstanceHandle_t* previousHandle,                         \
        DDS_ReadCondition *condition);                                      \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_w_condition(                \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        DDS_ReadCondition *condition);                                      \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_take_w_condition(                \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        DDS_ReadCondition *condition);
#else
#define ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* readcondition的声明周期关联方法 */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_CONDITON_METHODS(TDataReader)          \
    DCPSDLL DDS_ReadCondition* TDataReader##_create_readcondition(  \
        TDataReader* self,                                          \
        DDS_SampleStateMask sampleMask,                             \
        DDS_ViewStateMask viewMask,                                 \
        DDS_InstanceStateMask instanceMask);                        \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_delete_readcondition(    \
        TDataReader* self,                                          \
        DDS_ReadCondition* condition);                              \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_delete_contained_entities(TDataReader* self);
#else 
#define ZRDDSDataReader_READ_CONDITON_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* queryCondition的生命周期方法 */
#ifdef _ZRDDS_INCLUDE_QUERY_CONDITION
#define ZRDDSDataReader_QUERY_CONDITON_METHODS(TDataReader)             \
    DCPSDLL DDS_QueryCondition* TDataReader##_create_querycondition(    \
        TDataReader* self,                                              \
        DDS_SampleStateMask sampleMask,                                 \
        DDS_ViewStateMask viewMask,                                     \
        DDS_InstanceStateMask instanceMask,                             \
        const DDS_Char* queryExpression,                                \
        const DDS_StringSeq* queryParameters);
#else
#define ZRDDSDataReader_QUERY_CONDITON_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_QUERY_CONDITION */

#define ZRDDSDataReader_ENTITY_METHODS(TDataReader)                     \
    DCPSDLL DDS_ReturnCode_t TDataReader##_set_qos(                     \
        TDataReader* self,                                              \
        const DDS_DataReaderQos* qoslist);                              \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_qos(                     \
        TDataReader* self,                                              \
        DDS_DataReaderQos* qoslist);                                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_set_listener(                \
        TDataReader* self,                                              \
        DDS_DataReaderListener *listener,                               \
        DDS_StatusKindMask mask);                                       \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_enable(TDataReader* self);   \
    \
    DCPSDLL DDS_Entity* TDataReader##_as_entity(TDataReader* self);     \
    \
    DCPSDLL DDS_DataReaderListener* TDataReader##_get_listener(TDataReader* self);

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSDataReader_LIVELINESS_METHODS(TDataReader)                     \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_liveliness_changed_status(   \
        TDataReader* self,                                                  \
        DDS_LivelinessChangedStatus* status);
#else 
#define ZRDDSDataReader_LIVELINESS_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSDataReader_DEADLINE_METHODS(TDataReader)                               \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_requested_deadline_missed_status(    \
        TDataReader* self,                                                          \
        DDS_RequestedDeadlineMissedStatus* status);
#else 
#define ZRDDSDataReader_DEADLINE_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSDataReader_MATCH_METHODS(TDataReader)                          \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_subscription_matched_status( \
        TDataReader* self,                                                  \
        DDS_SubscriptionMatchedStatus* status);                             \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_matched_publication_data(    \
        TDataReader* self,                                                  \
        DDS_PublicationBuiltinTopicData* publicationData,                   \
        const DDS_InstanceHandle_t* publicationHandle);                     \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_matched_publications(        \
        TDataReader* self,                                                  \
        DDS_InstanceHandleSeq* publicationHandles);

#define ZRDDSDataReader_SAMPLE_STATUS_METHODS(TDataReader)              \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_sample_lost_status(      \
        TDataReader* self,                                              \
        DDS_SampleLostStatus* status);                                  \
    \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_sample_rejected_status(  \
        TDataReader* self,                                              \
        DDS_SampleRejectedStatus* status);

#define ZRDDSDataReader_FACTORY_METHODS(TDataReader)                           \
    DCPSDLL DDS_Subscriber* TDataReader##_get_subscriber(TDataReader* self);    \
    \
    DCPSDLL DDS_TopicDescription* TDataReader##_get_topicdescription(TDataReader* self);

#define ZRDDSDataReader_COMMON_STATUS_METHODS(TDataReader)                         \
    DCPSDLL DDS_ReturnCode_t TDataReader##_get_requested_incompatible_qos_status(   \
        TDataReader* self,                                                          \
        DDS_RequestedIncompatibleQosStatus* status);

#define ZRDDSDataReader_HISTORY_METHODS(TDataReader)                   \
    DCPSDLL DDS_ReturnCode_t TDataReader##_wait_for_historical_data(    \
        TDataReader* self,                                              \
        const DDS_Duration_t* maxWait); 

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSDataReader_XML_ENTITY_METHODS(TDataReader)                     \
    DCPSDLL DDS_ReturnCode_t TDataReader##_read_sample_info_to_xml_string(  \
        TDataReader *self,                                          \
        DDS_SampleInfo* sample_info,                                \
        DDS_SampleInfoValidMember* valid_sample_info_member,        \
        const DDS_Char** result);                                   \
    DCPSDLL DDS_ReturnCode_t TDataReader##_to_xml(                  \
        TDataReader* self,                                          \
        const DDS_Char** result,                                    \
        DDS_Boolean contained_qos);                                 \
    DCPSDLL DDS_ReturnCode_t TDataReader##_sample_to_xml_string(    \
        TDataReader* self,                                          \
        ZRDynamicData* data,                                        \
        const DDS_Char** result);                                   \
    DCPSDLL const DDS_Char* TDataReader##_get_entity_name(          \
        TDataReader* self);                                         \
    DCPSDLL DDS_Subscriber* TDataReader##_get_factory(              \
        TDataReader* self);
#else
#define ZRDDSDataReader_XML_ENTITY_METHODS(TDataReader) 
#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSDataReader_XML_QOS_METHODS(TDataReader)                \
    DCPSDLL DDS_ReturnCode_t TDataReader##_set_qos_with_profile(    \
        TDataReader* self,                                          \
        const DDS_Char* library_name,                               \
        const DDS_Char* profile_name,                               \
        const DDS_Char* qos_name);
#else
#define ZRDDSDataReader_XML_QOS_METHODS(TDataReader) 
#endif /* _ZRXMLQOSINTERFACE */

#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS(TDataReader)  \
    DDS_ReadCondition* TDataReader##_create_named_readcondition(\
        TDataReader* self,                                      \
        const DDS_Char* name,                                   \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    DDS_ReturnCode_t TDataReader##_delete_named_readcondition(  \
        TDataReader* self,                                      \
        const DDS_Char* name);                                  \
    DDS_ReadCondition* TDataReader##_get_named_readcondition(   \
        TDataReader* self,                                      \
        const DDS_Char* name);

#ifdef _ZRDDS_INCLUDE_QUERY_CONDITION
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS(TDataReader)     \
    DDS_QueryCondition* TDataReader##_create_named_querycondition(  \
        TDataReader* self,                                          \
        const DDS_Char* name,                                       \
        const DDS_SampleStateMask sample_mask,                      \
        const DDS_ViewStateMask view_mask,                          \
        const DDS_InstanceStateMask instance_mask,                  \
        const DDS_Char* query_expression,                           \
        const DDS_StringSeq* query_parameters);
#else /* _ZRDDS_INCLUDE_QUERY_CONDITION */
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_QUERY_CONDITION */

#else /* _ZRDDS_INCLUDE_READ_CONDITION */
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */
#else /* _ZRXMLINTERFACE */
#define ZRDDSDataReader_XML_ENTITY_METHODS(TDataReader)
#define ZRDDSDataReader_XML_QOS_METHODS(TDataReader)  
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS(TDataReader)
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS(TDataReader)
#endif /* _ZRXMLINTERFACE */

/**
 * @define  ZRDDSDataReader
 *
 * @brief   模板类，用于实例化构造用户层的DataReader类。
 */
#define ZRDDSDataReader(TDataReader, TTypeSeq, TType)                               \
    ZRDDSDataReader_READ_TAKE_METHODS(TDataReader, TTypeSeq, TType)                 \
    ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)   \
    ZRDDSDataReader_READ_CONDITON_METHODS(TDataReader)                              \
    ZRDDSDataReader_QUERY_CONDITON_METHODS(TDataReader)                             \
    ZRDDSDataReader_ENTITY_METHODS(TDataReader)                                     \
    ZRDDSDataReader_LIVELINESS_METHODS(TDataReader)                                 \
    ZRDDSDataReader_DEADLINE_METHODS(TDataReader)                                   \
    ZRDDSDataReader_MATCH_METHODS(TDataReader)                                      \
    ZRDDSDataReader_SAMPLE_STATUS_METHODS(TDataReader)                              \
    ZRDDSDataReader_FACTORY_METHODS(TDataReader)                                    \
    ZRDDSDataReader_COMMON_STATUS_METHODS(TDataReader)                              \
    ZRDDSDataReader_HISTORY_METHODS(TDataReader)                                    \
    ZRDDSDataReader_XML_ENTITY_METHODS(TDataReader)                                 \
    ZRDDSDataReader_XML_QOS_METHODS(TDataReader)                                    \
    ZRDDSDataReader_READ_CONDITON_XML_METHODS(TDataReader)                          \
    ZRDDSDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)

/* read/take方法，不包含condition */
#define ZRDDSUserDataReader_READ_TAKE_METHODS(TDataReader, TTypeSeq, TType)    \
    DDS_ReturnCode_t TDataReader##_read(                        \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_take(                        \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_read_next_sample(            \
        TDataReader* self,                                      \
        TType* dataValue,                                       \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DDS_ReturnCode_t TDataReader##_take_next_sample(            \
        TDataReader* self,                                      \
        TType* dataValue,                                       \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DDS_ReturnCode_t TDataReader##_read_instance(               \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* handle,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_take_instance(               \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* handle,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_read_next_instance(          \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* previousHandle,             \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_take_next_instance(          \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos,                         \
        DDS_Long maxSamples,                                    \
        const DDS_InstanceHandle_t* previousHandle,             \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);                    \
    \
    DDS_ReturnCode_t TDataReader##_return_loan(                 \
        TDataReader* self,                                      \
        TTypeSeq* dataValues,                                   \
        DDS_SampleInfoSeq* sampleInfos);                        \
    \
    DDS_ReturnCode_t TDataReader##_return_recv_buffer(          \
        TDataReader* self,                                      \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DDS_ReturnCode_t TDataReader##_loan_recv_buffer(            \
        TDataReader* self,                                      \
        DDS_SampleInfo* sampleInfo);                            \
    \
    DDS_ReturnCode_t TDataReader##_get_key_value(               \
        TDataReader* self,                                      \
        TType* keyHolder,                                       \
        const DDS_InstanceHandle_t* handle);                    \
    \
    DDS_InstanceHandle_t TDataReader##_lookup_instance(         \
        TDataReader* self,                                      \
        const TType* instance);                                 \
    \
    DDS_ReturnCode_t TDataReader##_get_data_instance(           \
        TDataReader* self,                                      \
        DDS_InstanceHandleSeq* dataHandles,                     \
        DDS_SampleStateMask sampleMask,                         \
        DDS_ViewStateMask viewMask,                             \
        DDS_InstanceStateMask instanceMask);
#ifdef _ZRDDS_INCLUDE_BREAKPOINT_RESUME
#define ZRDDSUserDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)         \
    DDS_ReturnCode_t TDataReader##_record_data(                   \
    TDataReader* self,                                             \
    DDS_SampleInfoSeq* sampleInfos,                                \
    ZR_BOOLEAN finish);                                            
#else/*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/
#define ZRDDSUserDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)
#endif /*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/

/* 带condition方法的read/take */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSUserDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)    \
    DDS_ReturnCode_t TDataReader##_read_next_instance_w_condition(          \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        const DDS_InstanceHandle_t* previousHandle,                         \
        DDS_ReadCondition* condition);                                      \
    \
    DDS_ReturnCode_t TDataReader##_take_next_instance_w_condition(          \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        const DDS_InstanceHandle_t* previousHandle,                         \
        DDS_ReadCondition *condition);                                      \
    \
    DDS_ReturnCode_t TDataReader##_read_w_condition(                        \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        DDS_ReadCondition *condition);                                      \
    \
    DDS_ReturnCode_t TDataReader##_take_w_condition(                        \
        TDataReader* self,                                                  \
        TTypeSeq* dataValues,                                               \
        DDS_SampleInfoSeq* sampleInfos,                                     \
        DDS_Long maxSamples,                                                \
        DDS_ReadCondition *condition);
#else
#define ZRDDSUserDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* readcondition的声明周期关联方法 */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSUserDataReader_READ_CONDITON_METHODS(TDataReader)      \
    DDS_ReadCondition* TDataReader##_create_readcondition(          \
        TDataReader* self,                                          \
        DDS_SampleStateMask sampleMask,                             \
        DDS_ViewStateMask viewMask,                                 \
        DDS_InstanceStateMask instanceMask);                        \
    \
    DDS_ReturnCode_t TDataReader##_delete_readcondition(            \
        TDataReader* self,                                          \
        DDS_ReadCondition* condition);                              \
    \
    DDS_ReturnCode_t TDataReader##_delete_contained_entities(TDataReader* self);
#else 
#define ZRDDSUserDataReader_READ_CONDITON_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* queryCondition的生命周期方法 */
#ifdef _ZRDDS_INCLUDE_QUERY_CONDITION
#define ZRDDSUserDataReader_QUERY_CONDITON_METHODS(TDataReader)         \
    DDS_QueryCondition* TDataReader##_create_querycondition(            \
        TDataReader* self,                                              \
        DDS_SampleStateMask sampleMask,                                 \
        DDS_ViewStateMask viewMask,                                     \
        DDS_InstanceStateMask instanceMask,                             \
        const DDS_Char* queryExpression,                                \
        const DDS_StringSeq* queryParameters);
#else
#define ZRDDSUserDataReader_QUERY_CONDITON_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_QUERY_CONDITION */

#define ZRDDSUserDataReader_ENTITY_METHODS(TDataReader)                 \
    DDS_ReturnCode_t TDataReader##_set_qos(                             \
        TDataReader* self,                                              \
        const DDS_DataReaderQos* qoslist);                              \
    \
    DDS_ReturnCode_t TDataReader##_get_qos(                             \
        TDataReader* self,                                              \
        DDS_DataReaderQos* qoslist);                                    \
    \
    DDS_ReturnCode_t TDataReader##_set_listener(                        \
        TDataReader* self,                                              \
        DDS_DataReaderListener *listener,                               \
        DDS_StatusKindMask mask);                                       \
    \
    DDS_ReturnCode_t TDataReader##_enable(TDataReader* self);           \
    \
    DDS_Entity* TDataReader##_as_entity(TDataReader* self);             \
    \
    DDS_DataReaderListener* TDataReader##_get_listener(TDataReader* self);

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSUserDataReader_LIVELINESS_METHODS(TDataReader)                 \
    DDS_ReturnCode_t TDataReader##_get_liveliness_changed_status(           \
        TDataReader* self,                                                  \
        DDS_LivelinessChangedStatus* status);
#else 
#define ZRDDSUserDataReader_LIVELINESS_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSUserDataReader_DEADLINE_METHODS(TDataReader)                           \
    DDS_ReturnCode_t TDataReader##_get_requested_deadline_missed_status(            \
        TDataReader* self,                                                          \
        DDS_RequestedDeadlineMissedStatus* status);
#else 
#define ZRDDSUserDataReader_DEADLINE_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSUserDataReader_MATCH_METHODS(TDataReader)                      \
    DDS_ReturnCode_t TDataReader##_get_subscription_matched_status(         \
        TDataReader* self,                                                  \
        DDS_SubscriptionMatchedStatus* status);                             \
    \
    DDS_ReturnCode_t TDataReader##_get_matched_publication_data(            \
        TDataReader* self,                                                  \
        DDS_PublicationBuiltinTopicData* publicationData,                   \
        const DDS_InstanceHandle_t* publicationHandle);                     \
    \
    DDS_ReturnCode_t TDataReader##_get_matched_publications(                \
        TDataReader* self,                                                  \
        DDS_InstanceHandleSeq* publicationHandles);

#define ZRDDSUserDataReader_SAMPLE_STATUS_METHODS(TDataReader)          \
    DDS_ReturnCode_t TDataReader##_get_sample_lost_status(              \
        TDataReader* self,                                              \
        DDS_SampleLostStatus* status);                                  \
    \
    DDS_ReturnCode_t TDataReader##_get_sample_rejected_status(          \
        TDataReader* self,                                              \
        DDS_SampleRejectedStatus* status);

#define ZRDDSUserDataReader_FACTORY_METHODS(TDataReader)                \
    DDS_Subscriber* TDataReader##_get_subscriber(TDataReader* self);    \
    \
    DDS_TopicDescription* TDataReader##_get_topicdescription(TDataReader* self);

#define ZRDDSUserDataReader_COMMON_STATUS_METHODS(TDataReader)              \
    DDS_ReturnCode_t TDataReader##_get_requested_incompatible_qos_status(   \
        TDataReader* self,                                                  \
        DDS_RequestedIncompatibleQosStatus* status);

#define ZRDDSUserDataReader_HISTORY_METHODS(TDataReader)                \
    DDS_ReturnCode_t TDataReader##_wait_for_historical_data(            \
        TDataReader* self,                                              \
        const DDS_Duration_t* maxWait); 

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSUserDataReader_XML_ENTITY_METHODS(TDataReader)         \
    DDS_ReturnCode_t TDataReader##_to_xml(                          \
        TDataReader* self,                                          \
        const DDS_Char** result,                                    \
        DDS_Boolean contained_qos);                                 \
    DDS_ReturnCode_t TDataReader##_read_sample_info_to_xml_string(  \
        TDataReader *self,                                          \
        DDS_SampleInfo* sample_info,                                \
        DDS_SampleInfoValidMember* valid_sample_info_member,        \
        const DDS_Char** result);                                   \
    DDS_ReturnCode_t TDataReader##_sample_to_xml_string(            \
        TDataReader* self,                                          \
        ZRDynamicData* data,                                        \
        const DDS_Char** result);                                   \
    const DDS_Char* TDataReader##_get_entity_name(                  \
        TDataReader* self);                                         \
    DDS_Subscriber* TDataReader##_get_factory(                      \
        TDataReader* self);
#else
#define ZRDDSUserDataReader_XML_ENTITY_METHODS(TDataReader)
#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSUserDataReader_XML_QOS_METHODS(TDataReader)            \
    DDS_ReturnCode_t TDataReader##_set_qos_with_profile(            \
        TDataReader* self,                                          \
        const DDS_Char* library_name,                               \
        const DDS_Char* profile_name,                               \
        const DDS_Char* qos_name);
#else
#define ZRDDSUserDataReader_XML_QOS_METHODS(TDataReader) 
#endif /* _ZRXMLQOSINTERFACE */

#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSUserDataReader_READ_CONDITON_XML_METHODS(TDataReader)  \
    DDS_ReadCondition* TDataReader##_create_named_readcondition(    \
        TDataReader* self,                                          \
        const DDS_Char* name,                                       \
        DDS_SampleStateMask sampleMask,                             \
        DDS_ViewStateMask viewMask,                                 \
        DDS_InstanceStateMask instanceMask);                        \
    DDS_ReturnCode_t TDataReader##_delete_named_readcondition(      \
        TDataReader* self,                                          \
        const DDS_Char* name);                                      \
    DDS_ReadCondition* TDataReader##_get_named_readcondition(       \
        TDataReader* self,                                          \
        const DDS_Char* name);
#else /* _ZRDDS_INCLUDE_READ_CONDITION */
#define ZRDDSUserDataReader_READ_CONDITON_XML_METHODS(TDataReader)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */
#else /* _ZRXMLINTERFACE */
#define ZRDDSUserDataReader_XML_ENTITY_METHODS(TDataReader)
#define ZRDDSUserDataReader_XML_QOS_METHODS(TDataReader)  
#define ZRDDSUserDataReader_READ_CONDITON_XML_METHODS(TDataReader)
#define ZRDDSUserDataReader_QUERY_CONDITON_XML_METHODS(TDataReader)
#endif /* _ZRXMLINTERFACE */

/**
 * @define  ZRDDSDataReader
 *
 * @brief   模板类，用于实例化构造用户层的DataReader类。
 */
#define ZRDDSUserDataReader(TDataReader, TTypeSeq, TType)                               \
    ZRDDSUserDataReader_READ_TAKE_METHODS(TDataReader, TTypeSeq, TType)                 \
    ZRDDSUserDataReader_READ_TAKE_WITH_CONDITON_METHODS(TDataReader, TTypeSeq, TType)   \
    ZRDDSUserDataReader_READ_CONDITON_METHODS(TDataReader)                              \
    ZRDDSUserDataReader_QUERY_CONDITON_METHODS(TDataReader)                             \
    ZRDDSUserDataReader_ENTITY_METHODS(TDataReader)                                     \
    ZRDDSUserDataReader_LIVELINESS_METHODS(TDataReader)                                 \
    ZRDDSUserDataReader_DEADLINE_METHODS(TDataReader)                                   \
    ZRDDSUserDataReader_MATCH_METHODS(TDataReader)                                      \
    ZRDDSUserDataReader_SAMPLE_STATUS_METHODS(TDataReader)                              \
    ZRDDSUserDataReader_FACTORY_METHODS(TDataReader)                                    \
    ZRDDSUserDataReader_COMMON_STATUS_METHODS(TDataReader)                              \
    ZRDDSUserDataReader_HISTORY_METHODS(TDataReader)                                    \
    ZRDDSUserDataReader_XML_ENTITY_METHODS(TDataReader)                                 \
    ZRDDSUserDataReader_XML_QOS_METHODS(TDataReader)                                    \
    ZRDDSUserDataReader_READ_CONDITON_XML_METHODS(TDataReader)                          \
    ZRDDSDataReader_QUERY_CONDITON_XML_METHODS(TDataReader)                             \
    ZRDDSUserDataReader_BREAKPOINT_RESUME_METHOD(TDataReader)

#endif /* ZRDDSDataReader_h__*/
