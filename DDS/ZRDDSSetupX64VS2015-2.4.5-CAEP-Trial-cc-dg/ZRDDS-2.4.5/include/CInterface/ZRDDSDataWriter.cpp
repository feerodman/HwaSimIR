/**
 * @file:       ZRDDSDataWriter.cpp
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#include "ZRDDSDataWriter.h"
#include "DataWriter.h"
#include <stdlib.h>

#define ZRDDSDataWriter_WRITE_METHODS_IMPL(TDataWriter, TType)  \
DDS_ReturnCode_t TDataWriter##_write(               \
    TDataWriter *self,                              \
    const TType *sample,                            \
    const DDS_InstanceHandle_t *handle)             \
{                                                   \
    return DataWriterWrite(                         \
        (DataWriterImpl*)self,                      \
        sample,                                     \
        handle);                                    \
}                                                   \
\
DDS_ReturnCode_t TDataWriter##_write_w_timestamp(   \
    TDataWriter *self,                              \
    const TType *sample,                            \
    const DDS_InstanceHandle_t *handle,             \
    const DDS_Time_t *timestamp)                    \
{                                                   \
    return DataWriterWriteWTimestamp(               \
        (DataWriterImpl*)self,                      \
        sample,                                     \
        handle,                                     \
        timestamp);                                 \
}                                                   \
\
DDS_ReturnCode_t TDataWriter##_write_w_dst(         \
    TDataWriter *self,                              \
    const TType *sample,                            \
    const DDS_InstanceHandle_t *handle,             \
    const DDS_Time_t *timestamp,                    \
    const DDS_InstanceHandle_t *dst_handle)         \
{                                                   \
    return DataWriterWriteWDst(                     \
        (DataWriterImpl*)self,                      \
        sample,                                     \
        handle,                                     \
        timestamp,                                  \
        dst_handle);                                \
}

#define ZRDDSDataWriter_INSTANCE_METHODS_IMPL(TDataWriter, TType)   \
DDS_InstanceHandle_t TDataWriter##_register_instance(               \
    TDataWriter *self,                                              \
    TType *instance)                                                \
{                                                                   \
    return DataWriterRegisterInstance(                              \
        (DataWriterImpl*)self,                                      \
        instance);                                                  \
}                                                                   \
\
DDS_InstanceHandle_t TDataWriter##_register_instance_w_timestamp(   \
    TDataWriter *self,                                              \
    const TType *instance,                                          \
    const DDS_Time_t *timestamp)                                    \
{                                                                   \
    return DataWriterRegisterInstanceWTimestamp(                    \
        (DataWriterImpl*)self,                                      \
        instance,                                                   \
        timestamp);                                                 \
}                                                                   \
\
DDS_ReturnCode_t TDataWriter##_unregister_instance(                 \
    TDataWriter *self,                                              \
    const TType *instance,                                          \
    const DDS_InstanceHandle_t *handle)                             \
{                                                                   \
    return DataWriterUnregisterInstance(                            \
            (DataWriterImpl*)self,                                  \
            instance,                                               \
            handle);                                                \
}   \
    \
DDS_ReturnCode_t TDataWriter##_unregister_instance_w_timestamp(     \
        TDataWriter *self,                                          \
        const TType *instance,                                      \
        const DDS_InstanceHandle_t *handle,                         \
        const DDS_Time_t *timestamp)                                \
{                                                                   \
    return DataWriterUnregisterInstanceWTimestamp(                  \
            (DataWriterImpl*)self,                                  \
            instance,                                               \
            handle,                                                 \
            timestamp);                                             \
}                                                                   \
\
DDS_ReturnCode_t TDataWriter##_dispose(                             \
    TDataWriter *self,                                              \
    const TType *instance,                                          \
    const DDS_InstanceHandle_t *handle)                             \
{   \
    return DataWriterDispose(                                       \
            (DataWriterImpl*)self,                                  \
            instance,                                               \
            handle);                                                \
}                                                                   \
\
DDS_ReturnCode_t TDataWriter##_dispose_w_timestamp(                 \
    TDataWriter *self,                                              \
    const TType *instance,                                          \
    const DDS_InstanceHandle_t *handle,                             \
    const DDS_Time_t *timestamp)                                    \
{                                                                   \
    return DataWriterDisposeWTimestamp(                             \
        (DataWriterImpl*)self,                                      \
        instance,                                                   \
        handle,                                                     \
        timestamp);                                                 \
}                                                                   \
\
DDS_ReturnCode_t TDataWriter##_get_key_value(                       \
    TDataWriter *self,                                              \
    TType *keyHolder,                                               \
    const DDS_InstanceHandle_t *handle)                             \
{                                                                   \
    return DataWriterGetKeyValue(                                   \
                (DataWriterImpl*)self,                              \
                keyHolder,                                          \
                handle);                                            \
}                                                                   \
\
DDS_InstanceHandle_t TDataWriter##_lookup_instance(                 \
    TDataWriter *self,                                              \
    const TType *instance)                                          \
{                                                                   \
    return DataWriterLookupInstance(                                \
                (DataWriterImpl*)self,                              \
                instance);                                          \
}

/* liveliness相关接口 */
#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSDataWriter_LIVELINESS_METHODS_IMPL(TDataWriter)    \
DDS_ReturnCode_t TDataWriter##_assert_liveliness(               \
    TDataWriter *self)                                          \
{                                                               \
    return DataWriterAssertLiveliness((DataWriterImpl*)self);   \
}                                                               \
\
DDS_ReturnCode_t TDataWriter##_get_liveliness_lost_status(      \
    TDataWriter *self,                                          \
    DDS_LivelinessLostStatus *status)                           \
{                                                               \
    return DataWriterGetLivelinessLostStatus(                   \
                (DataWriterImpl*)self,                          \
                status);                                        \
}
#else 
#define ZRDDSDataWriter_LIVELINESS_METHODS_IMPL(TDataWriter)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

/* deadline相关接口 */
#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSDataWriter_DEADLINE_METHODS_IMPL(TDataWriter)          \
DDS_ReturnCode_t TDataWriter##_get_offered_deadline_missed_status(  \
    TDataWriter *self,                                              \
    DDS_OfferedDeadlineMissedStatus *status)                        \
{                                                                   \
    return DataWriterGetOfferedDeadlineMissedStatus(                \
                (DataWriterImpl*)self,                              \
                status);                                            \
}
#else
#define ZRDDSDataWriter_DEADLINE_METHODS_IMPL(TDataWriter)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSDataWriter_MATCH_METHODS_IMPL(TDataWriter)         \
DDS_ReturnCode_t TDataWriter##_get_publication_matched_status(  \
    TDataWriter *self,                                          \
    DDS_PublicationMatchedStatus *status)                       \
{                                                               \
    return DataWriterGetPublicationMatchedStatus(               \
                (DataWriterImpl*)self,                          \
                status);                                        \
}                                                               \
\
DDS_ReturnCode_t TDataWriter##_get_matched_subscription_data(   \
    TDataWriter *self,                                          \
    DDS_SubscriptionBuiltinTopicData *subscriptionData,         \
    const DDS_InstanceHandle_t *subscriptionHandle)             \
{                                                               \
    return DataWriterGetMatchedSubscriptionData(                \
                (DataWriterImpl*)self,                          \
                subscriptionHandle,                             \
                subscriptionData);                              \
}                                                               \
\
DDS_ReturnCode_t TDataWriter##_get_matched_subscriptions(       \
    TDataWriter *self,                                          \
    DDS_InstanceHandleSeq *subscriptionHandles)                 \
{                                                               \
    return DataWriterGetMatchedSubscriptions(                   \
                (DataWriterImpl*)self,                          \
                subscriptionHandles);                           \
}

#define ZRDDSDataWriter_ENTITY_METHODS_IMPL(TDataWriter)                \
DDS_ReturnCode_t TDataWriter##_set_qos(                                 \
    TDataWriter *self,                                                  \
    const DDS_DataWriterQos *qoslist)                                   \
{                                                                       \
    return DataWriterSetQos(                                            \
                (DataWriterImpl*)self,                                  \
                qoslist);                                               \
}                                                                       \
\
DDS_ReturnCode_t TDataWriter##_get_qos(                                 \
    TDataWriter *self,                                                  \
    DDS_DataWriterQos *qoslist)                                         \
{                                                                       \
    return DataWriterGetQos(                                            \
                (DataWriterImpl*)self,                                  \
                qoslist);                                               \
}                                                                       \
\
DDS_ReturnCode_t TDataWriter##_set_listener(                            \
    TDataWriter *self,                                                  \
    DDS_DataWriterListener *listener,                                   \
    DDS_StatusKindMask mask)                                            \
{                                                                       \
    return DataWriterSetListener(                                       \
                (DataWriterImpl*)self,                                  \
                (DataWriterListenerImpl*)listener,                      \
                mask);                                                  \
}                                                                       \
\
DDS_DataWriterListener* TDataWriter##_get_listener(TDataWriter *self)   \
{                                                                       \
    return (DDS_DataWriterListener*)DataWriterGetListener(              \
                (DataWriterImpl*)self);                                 \
}                                                                       \
                                                                        \
DDS_ReturnCode_t TDataWriter##_enable(TDataWriter *self)                \
{                                                                       \
    return DataWriterEnable((DataWriterImpl*)self);                     \
}                                                                       \
                                                                        \
DDS_Entity* TDataWriter##_as_entity(TDataWriter* self)                  \
{                                                                       \
    return DataWriterAsEntity((DataWriterImpl*)self);                   \
} 

#ifdef _ZRDDS_INCLUDE_BATCH
#define ZRDDSDataWriter_BATCH_METHODS_IMPL(TDataWriter) \
DDS_ReturnCode_t TDataWriter##_flush(TDataWriter *self) \
{                                                       \
    return DataWriterFlush((DataWriterImpl*)self);      \
}

#else // _ZRDDS_INCLUDE_BATCH
#define ZRDDSDataWriter_BATCH_METHODS_IMPL(TDataWriter)
#endif // _ZRDDS_INCLUDE_BATCH

#define ZRDDSDataWriter_FACTORY_METHODS_IMPL(TDataWriter)       \
DDS_Topic* TDataWriter##_get_topic(TDataWriter *self)           \
{                                                               \
    return DataWriterGetTopic(                                  \
                (DataWriterImpl*)self);                         \
}                                                               \
\
DDS_Publisher* TDataWriter##_get_publisher(TDataWriter *self)   \
{                                                               \
    return DataWriterGetPublisher(                              \
                (DataWriterImpl*)self);                         \
}

#define ZRDDSDataWriter_COMMON_STATUS_METHODS_IMPL(TDataWriter)     \
DDS_ReturnCode_t TDataWriter##_get_offered_incompatible_qos_status( \
    TDataWriter *self,                                              \
    DDS_OfferedIncompatibleQosStatus *status)                       \
{   \
    return DataWriterGetOfferedIncompatibleQosStatus(               \
                (DataWriterImpl*)self,                              \
                status);                                            \
}

#define ZRDDSDataWriter_HISTORY_METHODS_IMPL(TDataWriter)   \
DDS_ReturnCode_t TDataWriter##_wait_for_acknowledgments(    \
    TDataWriter *self,                                      \
    const DDS_Duration_t *maxWait)                          \
{                                                           \
    return DataWriterWaitForAcknowledgments(                \
        (DataWriterImpl*)self,                              \
        maxWait);                                           \
}

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSDataWriter_XML_QOS_METHODS_IMPL(TDataWriter)       \
    DDS_ReturnCode_t TDataWriter##_set_qos_with_profile(        \
        TDataWriter* dw,                                        \
        const DDS_Char* library_name,                           \
        const DDS_Char* profile_name,                           \
        const DDS_Char* qos_name)                               \
    {                                                           \
        return DataWriterSetQosWithProfile(                     \
            (DataWriterImpl*)dw,                                \
            library_name,                                       \
            profile_name,                                       \
            qos_name);                                          \
    }
#else /*_ZRXMLQOSINTERFACE*/
#define ZRDDSDataWriter_XML_QOS_METHODS_IMPL(TDataWriter)
#endif /*_ZRXMLQOSINTERFACE*/

#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSDataWriter_XML_ENTITY_METHODS_IMPL(TDataWriter)    \
    DDS_ReturnCode_t TDataWriter##_to_xml(                      \
        TDataWriter* dw,                                        \
        const DDS_Char** result,                                \
        DDS_Boolean contained_qos)                              \
    {                                                           \
        return DataWriterToXML((DataWriterImpl*)dw,             \
                result, contained_qos);                         \
    }                                                           \
    DDS_ReturnCode_t TDataWriter##_parse_write_sample_info_xml( \
        TDataWriter* dw,                                        \
        const DDS_Char* xml_content,                            \
        DDS_SampleInfo* sample_info,                            \
        DDS_SampleInfoValidMember* valid_sample_info_member)    \
    {                                                           \
        return DataWriterParseWriteSampleInfoXML(               \
            (DataWriterImpl*)dw, xml_content, sample_info,      \
                valid_sample_info_member);                      \
    }                                                           \
    DDS_ReturnCode_t TDataWriter##_parse_write_sample_xml(      \
        TDataWriter* dw,                                        \
        const DDS_Char* xml_content,                            \
        ZRDynamicData** data)                                   \
    {                                                           \
        return DataWriterParseWriteSampleXML(                   \
            (DataWriterImpl*)dw, xml_content, data);            \
    }                                                           \
    DDS_ReturnCode_t TDataWriter##_return_xml_sample(           \
        TDataWriter* dw,                                        \
        ZRDynamicData* data)                                    \
    {                                                           \
        return DataWriterReturnXMLSample((DataWriterImpl*)dw,   \
                data);                                          \
    }                                                           \
    const DDS_Char* TDataWriter##_get_entity_name(              \
        TDataWriter* dw)                                        \
    {                                                           \
        return EntityGetEntityName(                             \
            DataWriterAsEntity((DataWriterImpl*)dw));           \
    }                                                           \
    DDS_Publisher* TDataWriter##_get_factory(                   \
        TDataWriter* dw)                                        \
    {                                                           \
        return DataWriterGetPublisher((DataWriterImpl*)dw);     \
    }
#else /* _ZRXMLENTITYINTERFACE */
#define ZRDDSDataWriter_XML_ENTITY_METHODS_IMPL(TDataWriter)
#endif /* _ZRXMLENTITYINTERFACE */
#else /* _ZRXMLINTERFACE */
#define ZRDDSDataWriter_XML_QOS_METHODS_IMPL(TDataWriter)
#define ZRDDSDataWriter_XML_ENTITY_METHODS_IMPL(TDataWriter)
#endif /* _ZRXMLINTERFACE */

#define ZRDDSDataWriter_STATUS_METHODS_IMPL(TDataWriter)                          \
    DDS_ReturnCode_t TDataWriter##_get_send_status(  \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status)                              \
    {                                                               \
        return DataWriterGetSendStatus(                \
                    (DataWriterImpl*)self,                          \
                    status);                              \
    }                                                               \
    DDS_ReturnCode_t TDataWriter##_print_send_status(   \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatusSeq *status)                    \
    {                                                               \
        return DataWriterPrintSendStatus(                \
                    (DataWriterImpl*)self,                          \
                    status);                              \
    }                                                               \
    DDS_ReturnCode_t TDataWriter##_get_send_status_w_handle(       \
        TDataWriter *self,                                                  \
        DDS_PublicationSendStatus *status,                      \
        const DDS_InstanceHandle_t *dst_handle)                 \
    {                                                               \
        return DataWriterGetSendStatusWithHandle(                \
                    (DataWriterImpl*)self,                          \
                    status,                          \
                    dst_handle);                              \
    }

/* ZRDDSDataWriter模板实现*/
#define ZRDDSDataWriterImpl(TDataWriter, TType)                 \
    ZRDDSDataWriter_WRITE_METHODS_IMPL(TDataWriter, TType)      \
    ZRDDSDataWriter_INSTANCE_METHODS_IMPL(TDataWriter, TType)   \
    ZRDDSDataWriter_LIVELINESS_METHODS_IMPL(TDataWriter)        \
    ZRDDSDataWriter_DEADLINE_METHODS_IMPL(TDataWriter)          \
    ZRDDSDataWriter_MATCH_METHODS_IMPL(TDataWriter)             \
    ZRDDSDataWriter_ENTITY_METHODS_IMPL(TDataWriter)            \
    ZRDDSDataWriter_BATCH_METHODS_IMPL(TDataWriter)             \
    ZRDDSDataWriter_FACTORY_METHODS_IMPL(TDataWriter)           \
    ZRDDSDataWriter_COMMON_STATUS_METHODS_IMPL(TDataWriter)     \
    ZRDDSDataWriter_HISTORY_METHODS_IMPL(TDataWriter)           \
    ZRDDSDataWriter_XML_QOS_METHODS_IMPL(TDataWriter)           \
    ZRDDSDataWriter_XML_ENTITY_METHODS_IMPL(TDataWriter)        \
    ZRDDSDataWriter_STATUS_METHODS_IMPL(TDataWriter)
