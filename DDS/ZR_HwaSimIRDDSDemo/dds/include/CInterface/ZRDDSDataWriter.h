/**
 * @file:       ZRDDSDataWriter.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef ZRDDSDataWriter_h__
#define ZRDDSDataWriter_h__

#include "ReturnCode_t.h"
#include "StatusKindMask.h"
#include "LivelinessChangedStatus.h"
#include "OfferedIncompatibleQosStatus.h"
#include "OfferedDeadlineMissedStatus.h"
#include "PublicationMatchedStatus.h"
#include "DataWriterListener.h"
#include "SubscriptionBuiltinTopicData.h"
#include "InstanceHandle_t.h"
#include "DataWriterQos.h"
#include "Duration_t.h"
#include "DataWriter.h"

/* write相关接口  */
#define ZRDDSDataWriter_WRITE_METHODS(TDataWriter, TType)       \
   DCPSDLL DDS_ReturnCode_t TDataWriter##_write(                \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_write_w_timestamp(   \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle,                     \
        const DDS_Time_t *timestamp);                           \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_write_w_dst(         \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle,                     \
        const DDS_Time_t *timestamp,                            \
        const DDS_InstanceHandle_t* dst_handle);                \

/* instance相关接口  */
#define ZRDDSDataWriter_INSTANCE_METHODS(TDataWriter, TType)                \
    DCPSDLL DDS_InstanceHandle_t TDataWriter##_register_instance(           \
        TDataWriter *self,                                                  \
        TType *instance);                                                   \
    \
    DCPSDLL DDS_InstanceHandle_t TDataWriter##_register_instance_w_timestamp(\
        TDataWriter *self,                                                  \
        const TType *instance,                                              \
        const DDS_Time_t *timestamp);                                       \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_unregister_instance(             \
        TDataWriter *self,                                                  \
        const TType *instance,                                              \
        const DDS_InstanceHandle_t *handle);                                \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_unregister_instance_w_timestamp( \
        TDataWriter *self,                                                  \
        const TType *instance,                                              \
        const DDS_InstanceHandle_t *handle,                                 \
        const DDS_Time_t *timestamp);                                       \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_dispose(                         \
        TDataWriter *self,                                                  \
        const TType *instance,                                              \
        const DDS_InstanceHandle_t *handle);                                \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_dispose_w_timestamp(             \
        TDataWriter *self,                                                  \
        const TType *instance,                                              \
        const DDS_InstanceHandle_t *handle,                                 \
        const DDS_Time_t *timestamp);                                       \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_key_value(                   \
        TDataWriter *self,                                                  \
        TType *keyHolder,                                                   \
        const DDS_InstanceHandle_t *handle);                                \
    \
    DCPSDLL DDS_InstanceHandle_t TDataWriter##_lookup_instance(             \
        TDataWriter *self,                                                  \
        const TType *instance);

/* liveliness相关接口 */
#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSDataWriter_LIVELINESS_METHODS(TDataWriter)                 \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_assert_liveliness(           \
        TDataWriter *self);                                             \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_liveliness_lost_status(  \
        TDataWriter *self,                                              \
        DDS_LivelinessLostStatus *status);
#else 
#define ZRDDSDataWriter_LIVELINESS_METHODS(TDataWriter)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

/* deadline相关接口 */
#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSDataWriter_DEADLINE_METHODS(TDataWriter)                           \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_offered_deadline_missed_status(  \
        TDataWriter *self,                                                      \
        DDS_OfferedDeadlineMissedStatus *status);
#else
#define ZRDDSDataWriter_DEADLINE_METHODS(TDataWriter)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSDataWriter_MATCH_METHODS(TDataWriter)                          \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_publication_matched_status(  \
        TDataWriter *self,                                                  \
        DDS_PublicationMatchedStatus *status);                              \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_matched_subscription_data(   \
        TDataWriter *self,                                                  \
        DDS_SubscriptionBuiltinTopicData *subscriptionData,                 \
        const DDS_InstanceHandle_t *subscriptionHandle);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_matched_subscriptions(       \
        TDataWriter *self,                                                  \
        DDS_InstanceHandleSeq *subscriptionHandles);

#define ZRDDSDataWriter_ENTITY_METHODS(TDataWriter)                     \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_set_qos(                     \
        TDataWriter *self,                                              \
        const DDS_DataWriterQos *qoslist);                              \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_qos(                     \
        TDataWriter *self,                                              \
        DDS_DataWriterQos *qoslist);                                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_set_listener(                \
        TDataWriter *self,                                              \
        DDS_DataWriterListener *listener,                               \
        DDS_StatusKindMask mask);                                       \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_enable(TDataWriter *self);   \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_flush(TDataWriter *self);    \
    \
    DCPSDLL DDS_Entity* TDataWriter##_as_entity(TDataWriter *self);     \
    \
    DCPSDLL DDS_DataWriterListener* TDataWriter##_get_listener(TDataWriter *self);

#define ZRDDSDataWriter_FACTORY_METHODS(TDataWriter)                            \
    DCPSDLL DDS_Topic* TDataWriter##_get_topic(TDataWriter *self);              \
    \
    DCPSDLL DDS_Publisher* TDataWriter##_get_publisher(TDataWriter *self);

#define ZRDDSDataWriter_COMMON_STATUS_METHODS(TDataWriter)                      \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_offered_incompatible_qos_status( \
        TDataWriter *self,                                                      \
        DDS_OfferedIncompatibleQosStatus *status);

#define ZRDDSDataWriter_HISTORY_METHODS(TDataWriter)                            \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_wait_for_acknowledgments(            \
        TDataWriter *self,                                                      \
        const DDS_Duration_t *maxWait);

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSDataWriter_XML_ENTITY_METHODS(TDataWriter)                 \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_to_xml(                      \
        TDataWriter* dw,                                                \
        const DDS_Char** result,                                        \
        DDS_Boolean contained_qos);                                     \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_parse_write_sample_info_xml( \
        TDataWriter* dw,                                                \
        const DDS_Char* xml_content,                                    \
        DDS_SampleInfo* sample_info,                                    \
        DDS_SampleInfoValidMember* valid_sample_info_member);           \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_parse_write_sample_xml(      \
        TDataWriter* dw,                                                \
        const DDS_Char* xml_content,                                    \
        ZRDynamicData** data);                                          \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_return_xml_sample(           \
        TDataWriter* dw,                                                \
        ZRDynamicData* data);                                           \
    DCPSDLL const DDS_Char* TDataWriter##_get_entity_name(              \
        TDataWriter* dw);                                               \
    DCPSDLL DDS_Publisher* TDataWriter##_get_factory(                   \
        TDataWriter* dw);
#else
#define ZRDDSDataWriter_XML_ENTITY_METHODS(TDataWriter)
#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSDataWriter_XML_QOS_METHODS(TDataWriter)                        \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_set_qos_with_profile(        \
        TDataWriter* dw,                                                \
        const DDS_Char* library_name,                                   \
        const DDS_Char* profile_name,                                   \
        const DDS_Char* qos_name);
#else
#define ZRDDSDataWriter_XML_QOS_METHODS(TDataWriter)
#endif /* _ZRXMLQOSINTERFACE */
#else
#define ZRDDSDataWriter_XML_ENTITY_METHODS(TDataWriter)
#define ZRDDSDataWriter_XML_QOS_METHODS(TDataWriter)
#endif /* _ZRXMLINTERFACE */

#define ZRDDSDataWriter_STATUS_METHODS(TDataWriter)                          \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_send_status(  \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status);                              \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_print_send_status(   \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status);                    \
    \
    DCPSDLL DDS_ReturnCode_t TDataWriter##_get_send_status_w_handle(       \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatus *status,                      \
        const DDS_InstanceHandle_t *dst_handle);

/**
 *@define  ZRDDSDataWriter
*
 *@brief   模板类，用于实例化构造用户层的DataWriter类。
*/
#define ZRDDSDataWriter(TDataWriter, TType)                 \
    ZRDDSDataWriter_WRITE_METHODS(TDataWriter, TType)       \
    ZRDDSDataWriter_INSTANCE_METHODS(TDataWriter, TType)    \
    ZRDDSDataWriter_LIVELINESS_METHODS(TDataWriter)         \
    ZRDDSDataWriter_DEADLINE_METHODS(TDataWriter)           \
    ZRDDSDataWriter_MATCH_METHODS(TDataWriter)              \
    ZRDDSDataWriter_ENTITY_METHODS(TDataWriter)             \
    ZRDDSDataWriter_FACTORY_METHODS(TDataWriter)            \
    ZRDDSDataWriter_COMMON_STATUS_METHODS(TDataWriter)      \
    ZRDDSDataWriter_HISTORY_METHODS(TDataWriter)            \
    ZRDDSDataWriter_XML_ENTITY_METHODS(TDataWriter)         \
    ZRDDSDataWriter_XML_QOS_METHODS(TDataWriter)            \
    ZRDDSDataWriter_STATUS_METHODS(TDataWriter)

/* write相关接口  */
#define ZRDDSUserDataWriter_WRITE_METHODS(TDataWriter, TType)   \
   DDS_ReturnCode_t TDataWriter##_write(                        \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle);                    \
    \
    DDS_ReturnCode_t TDataWriter##_write_w_timestamp(           \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle,                     \
        const DDS_Time_t *timestamp);                           \
    \
    DDS_ReturnCode_t TDataWriter##_write_w_dst(                 \
        TDataWriter *self,                                      \
        const TType *sample,                                    \
        const DDS_InstanceHandle_t *handle,                     \
        const DDS_Time_t *timestamp,                            \
        const DDS_InstanceHandle_t* dst_handle);

/* instance相关接口  */
#define ZRDDSUserDataWriter_INSTANCE_METHODS(TDataWriter, TType)        \
    DDS_InstanceHandle_t TDataWriter##_register_instance(               \
        TDataWriter *self,                                              \
        TType *instance);                                               \
    \
    DDS_InstanceHandle_t TDataWriter##_register_instance_w_timestamp(   \
        TDataWriter *self,                                              \
        const TType *instance,                                          \
        const DDS_Time_t *timestamp);                                   \
    \
    DDS_ReturnCode_t TDataWriter##_unregister_instance(                 \
        TDataWriter *self,                                              \
        const TType *instance,                                          \
        const DDS_InstanceHandle_t *handle);                            \
    \
    DDS_ReturnCode_t TDataWriter##_unregister_instance_w_timestamp(     \
        TDataWriter *self,                                              \
        const TType *instance,                                          \
        const DDS_InstanceHandle_t *handle,                             \
        const DDS_Time_t *timestamp);                                   \
    \
    DDS_ReturnCode_t TDataWriter##_dispose(                             \
        TDataWriter *self,                                              \
        const TType *instance,                                          \
        const DDS_InstanceHandle_t *handle);                            \
    \
    DDS_ReturnCode_t TDataWriter##_dispose_w_timestamp(                 \
        TDataWriter *self,                                              \
        const TType *instance,                                          \
        const DDS_InstanceHandle_t *handle,                             \
        const DDS_Time_t *timestamp);                                   \
    \
    DDS_ReturnCode_t TDataWriter##_get_key_value(                       \
        TDataWriter *self,                                              \
        TType *keyHolder,                                               \
        const DDS_InstanceHandle_t *handle);                            \
    \
    DDS_InstanceHandle_t TDataWriter##_lookup_instance(                 \
        TDataWriter *self,                                              \
        const TType *instance);

/* liveliness相关接口 */
#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSUserDataWriter_LIVELINESS_METHODS(TDataWriter)             \
    DDS_ReturnCode_t TDataWriter##_assert_liveliness(                   \
        TDataWriter *self);                                             \
    \
    DDS_ReturnCode_t TDataWriter##_get_liveliness_lost_status(          \
        TDataWriter *self,                                              \
        DDS_LivelinessLostStatus *status);
#else 
#define ZRDDSUserDataWriter_LIVELINESS_METHODS(TDataWriter)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

/* deadline相关接口 */
#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSUserDataWriter_DEADLINE_METHODS(TDataWriter)               \
    DDS_ReturnCode_t TDataWriter##_get_offered_deadline_missed_status(  \
        TDataWriter *self,                                              \
        DDS_OfferedDeadlineMissedStatus *status);
#else
#define ZRDDSUserDataWriter_DEADLINE_METHODS(TDataWriter)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSUserDataWriter_MATCH_METHODS(TDataWriter)                  \
    DDS_ReturnCode_t TDataWriter##_get_publication_matched_status(      \
        TDataWriter *self,                                              \
        DDS_PublicationMatchedStatus *status);                          \
    \
    DDS_ReturnCode_t TDataWriter##_get_matched_subscription_data(       \
        TDataWriter *self,                                              \
        DDS_SubscriptionBuiltinTopicData *subscriptionData,             \
        const DDS_InstanceHandle_t *subscriptionHandle);                \
    \
    DDS_ReturnCode_t TDataWriter##_get_matched_subscriptions(           \
        TDataWriter *self,                                              \
        DDS_InstanceHandleSeq *subscriptionHandles);

#define ZRDDSUserDataWriter_ENTITY_METHODS(TDataWriter)                 \
    DDS_ReturnCode_t TDataWriter##_set_qos(                             \
        TDataWriter *self,                                              \
        const DDS_DataWriterQos *qoslist);                              \
    \
    DDS_ReturnCode_t TDataWriter##_get_qos(                             \
        TDataWriter *self,                                              \
        DDS_DataWriterQos *qoslist);                                    \
    \
    DDS_Entity* TDataWriter##_as_entity(TDataWriter *self);             \
    \
    DDS_ReturnCode_t TDataWriter##_set_listener(                        \
        TDataWriter *self,                                              \
        DDS_DataWriterListener *listener,                               \
        DDS_StatusKindMask mask);                                       \
    \
    DDS_ReturnCode_t TDataWriter##_enable(TDataWriter *self);           \
    \
    DDS_DataWriterListener* TDataWriter##_get_listener(TDataWriter *self);

#define ZRDDSUserDataWriter_FACTORY_METHODS(TDataWriter)                \
    DDS_Topic* TDataWriter##_get_topic(TDataWriter *self);              \
    \
    DDS_Publisher* TDataWriter##_get_publisher(TDataWriter *self);

#define ZRDDSUserDataWriter_COMMON_STATUS_METHODS(TDataWriter)          \
    DDS_ReturnCode_t TDataWriter##_get_offered_incompatible_qos_status( \
        TDataWriter *self,                                              \
        DDS_OfferedIncompatibleQosStatus *status);

#define ZRDDSUserDataWriter_HISTORY_METHODS(TDataWriter)                \
    DDS_ReturnCode_t TDataWriter##_wait_for_acknowledgments(            \
        TDataWriter *self,                                              \
        const DDS_Duration_t *maxWait);

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSUserDataWriter_XML_ENTITY_METHODS(TDataWriter)     \
    DDS_ReturnCode_t TDataWriter##_to_xml(                      \
        TDataWriter* dw,                                        \
        const DDS_Char** result,                                \
        DDS_Boolean containedQos);                              \
    DDS_ReturnCode_t TDataWriter##_parse_write_sample_info_xml( \
        TDataWriter* dw,                                        \
        const DDS_Char* xml_content,                            \
        DDS_SampleInfo* sample_info,                            \
        DDS_SampleInfoValidMember* valid_sample_info_member);   \
    DDS_ReturnCode_t TDataWriter##_parse_write_sample_xml(      \
        TDataWriter* dw,                                        \
        const DDS_Char* xml_content,                            \
        ZRDynamicData** data);                                  \
    DDS_ReturnCode_t TDataWriter##_return_xml_sample(           \
        TDataWriter* dw,                                        \
        ZRDynamicData* data);                                   \
    const DDS_Char* TDataWriter##_get_entity_name(              \
        TDataWriter* dw);                                       \
    DDS_Publisher* TDataWriter##_get_factory(                   \
        TDataWriter* dw);
#else
#define ZRDDSUserDataWriter_XML_ENTITY_METHODS(TDataWriter)
#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSUserDataWriter_XML_QOS_METHODS(TDataWriter)        \
    DDS_ReturnCode_t TDataWriter##_set_qos_with_profile(        \
        TDataWriter* dw,                                        \
        const DDS_Char* library_name,                           \
        const DDS_Char* profile_name,                           \
        const DDS_Char* qos_name);
#else
#define ZRDDSUserDataWriter_XML_QOS_METHODS(TDataWriter)
#endif /* _ZRXMLQOSINTERFACE */
#else
#define ZRDDSUserDataWriter_XML_ENTITY_METHODS(TDataWriter)
#define ZRDDSUserDataWriter_XML_QOS_METHODS(TDataWriter)
#endif /* _ZRXMLINTERFACE */

#define ZRDDSUserDataWriter_STATUS_METHODS(TDataWriter)                          \
    DDS_ReturnCode_t TDataWriter##_get_send_status(  \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status);                              \
    \
    DDS_ReturnCode_t TDataWriter##_print_send_status(   \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status);                    \
    \
    DDS_ReturnCode_t TDataWriter##_get_send_status_w_handle(       \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatus *status,                      \
        const DDS_InstanceHandle_t *subscriptionHandle);

/**
*@define  ZRDDSUserDataWriter
*
*@brief   模板类，用于实例化构造用户层的DataWriter类。
*/
#define ZRDDSUserDataWriter(TDataWriter, TType)                 \
    ZRDDSUserDataWriter_WRITE_METHODS(TDataWriter, TType)       \
    ZRDDSUserDataWriter_INSTANCE_METHODS(TDataWriter, TType)    \
    ZRDDSUserDataWriter_LIVELINESS_METHODS(TDataWriter)         \
    ZRDDSUserDataWriter_DEADLINE_METHODS(TDataWriter)           \
    ZRDDSUserDataWriter_MATCH_METHODS(TDataWriter)              \
    ZRDDSUserDataWriter_ENTITY_METHODS(TDataWriter)             \
    ZRDDSUserDataWriter_FACTORY_METHODS(TDataWriter)            \
    ZRDDSUserDataWriter_COMMON_STATUS_METHODS(TDataWriter)      \
    ZRDDSUserDataWriter_HISTORY_METHODS(TDataWriter)            \
    ZRDDSUserDataWriter_XML_ENTITY_METHODS(TDataWriter)         \
    ZRDDSUserDataWriter_XML_QOS_METHODS(TDataWriter)            \
    ZRDDSUserDataWriter_STATUS_METHODS(TDataWriter)
#endif /* ZRDDSDataWriter_h__*/
