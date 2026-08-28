/**
 * @file:       DataWriter.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DataWriter_h__
#define DataWriter_h__

#include "DataWriterQos.h"
#include "StatusKindMask.h"
#include "ReturnCode_t.h"
#include "SubscriptionBuiltinTopicData.h"
#include "LivelinessLostStatus.h"
#include "OfferedDeadlineMissedStatus.h"
#include "OfferedIncompatibleQosStatus.h"
#include "PublicationMatchedStatus.h"
#include "PublicationSendStatus.h"
#include "Topic.h"
#include "Entity.h"
#include "SampleInfo.h"
#include "ZRDynamicData.h"

#ifdef __cplusplus
extern "C"
{
#endif

DCPSDLL DDS_InstanceHandle_t DataWriterRegisterInstance(
    DDS_DataWriter *writer,
    const void *instance);

DCPSDLL DDS_InstanceHandle_t DataWriterRegisterInstanceWTimestamp(
    DDS_DataWriter *writer,
    const void *instance,
    const DDS_Time_t *timestamp);

DCPSDLL DDS_ReturnCode_t DataWriterUnregisterInstance(
    DDS_DataWriter *writer,
    const void *instance,
    const DDS_InstanceHandle_t *handle);

DCPSDLL DDS_ReturnCode_t DataWriterUnregisterInstanceWTimestamp(
    DDS_DataWriter *writer,
    const void *instance,
    const DDS_InstanceHandle_t *handle,
    const DDS_Time_t *timestamp);

DCPSDLL DDS_ReturnCode_t DataWriterGetKeyValue(
    DDS_DataWriter *writer,
    void *keyHolder,
    const DDS_InstanceHandle_t *handle);

DCPSDLL DDS_InstanceHandle_t DataWriterLookupInstance(
    DDS_DataWriter *writer,
    const void *keyHolder);

DCPSDLL DDS_ReturnCode_t DataWriterWrite(
    DDS_DataWriter *writer,
    const void *sample,
    const DDS_InstanceHandle_t *handle);

DCPSDLL DDS_ReturnCode_t DataWriterWriteWTimestamp(
    DDS_DataWriter *writer,
    const void *sample,
    const DDS_InstanceHandle_t *handle,
    const DDS_Time_t *timestamp);

DCPSDLL DDS_ReturnCode_t DataWriterWriteWDst(
    DDS_DataWriter *writer,
    const void *sample,
    const DDS_InstanceHandle_t *handle,
    const DDS_Time_t *timestamp,
    const DDS_InstanceHandle_t *dst_handle);

DCPSDLL DDS_ReturnCode_t DataWriterDispose(
    DDS_DataWriter *writer,
    const void *instance,
    const DDS_InstanceHandle_t *handle);

DCPSDLL DDS_ReturnCode_t DataWriterDisposeWTimestamp(
    DDS_DataWriter *writer,
    const void *instance,
    const DDS_InstanceHandle_t *handle,
    const DDS_Time_t *timestamp);

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
DCPSDLL DDS_ReturnCode_t DataWriterGetLivelinessLostStatus(
    DDS_DataWriter *writer,
    DDS_LivelinessLostStatus *status);

DCPSDLL DDS_ReturnCode_t DataWriterAssertLiveliness(
    DDS_DataWriter *writer);
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
DCPSDLL DDS_ReturnCode_t DataWriterGetOfferedDeadlineMissedStatus(
    DDS_DataWriter *writer,
    DDS_OfferedDeadlineMissedStatus *status);
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

DCPSDLL DDS_ReturnCode_t DataWriterGetOfferedIncompatibleQosStatus(
    DDS_DataWriter *writer,
    DDS_OfferedIncompatibleQosStatus *status);

DCPSDLL DDS_ReturnCode_t DataWriterGetPublicationMatchedStatus(
    DDS_DataWriter *writer,
    DDS_PublicationMatchedStatus *status);

DCPSDLL DDS_ReturnCode_t DataWriterGetMatchedSubscriptions(
    DDS_DataWriter *writer,
    DDS_InstanceHandleSeq *handleSeq);

DCPSDLL DDS_ReturnCode_t DataWriterGetMatchedSubscriptionData(
    DDS_DataWriter *writer,
    const DDS_InstanceHandle_t *handle,
    DDS_SubscriptionBuiltinTopicData *subscriptionData);

DCPSDLL DDS_ReturnCode_t DataWriterGetSendStatus(
    DDS_DataWriter* dw,
    DDS_PublicationSendStatusSeq* statusSeq);

DCPSDLL DDS_ReturnCode_t DataWriterPrintSendStatus(
    DDS_DataWriter* dw,
    DDS_PublicationSendStatusSeq* statusSeq);

DCPSDLL DDS_ReturnCode_t DataWriterGetSendStatusWithHandle(
    DDS_DataWriter* dw,
    DDS_PublicationSendStatus* status,
    const DDS_InstanceHandle_t* dst_handle);

DCPSDLL DDS_ReturnCode_t DataWriterWaitForAcknowledgments(
    DDS_DataWriter *writer,
    const DDS_Duration_t *maxWait);

DCPSDLL DDS_Topic* DataWriterGetTopic(
    DDS_DataWriter *writer);

DCPSDLL DDS_Publisher* DataWriterGetPublisher(
    DDS_DataWriter *writer);

DCPSDLL DDS_ReturnCode_t DataWriterSetQos(
    DDS_DataWriter *writer,
    const DDS_DataWriterQos *qos);

DCPSDLL DDS_ReturnCode_t DataWriterGetQos(
    DDS_DataWriter *writer,
    DDS_DataWriterQos *qos);

DCPSDLL DDS_ReturnCode_t DataWriterSetListener(
    DDS_DataWriter *writer,
    DataWriterListenerImpl *listener,
    DDS_StatusKindMask mask);

DCPSDLL DataWriterListenerImpl* DataWriterGetListener(
    DDS_DataWriter *writer);

DCPSDLL DDS_ReturnCode_t DataWriterEnable(
    DDS_DataWriter *writer);

#ifdef _ZRDDS_INCLUDE_BATCH
DCPSDLL DDS_ReturnCode_t DataWriterFlush(
    DDS_DataWriter *dw);
#endif // _ZRDDS_INCLUDE_BATCH

DCPSDLL DDS_Entity* DataWriterAsEntity(DDS_DataWriter* self);

#ifdef _ZRDDS_ENABLE_RAW_TRANSFER

DCPSDLL DDS_ReturnCode_t DataWriterWriteRaw(
    DDS_DataWriter* writer,
    const void* data,
    DDS_ULong length);

#endif /* _ZRDDS_ENABLE_RAW_TRANSFER */

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

DCPSDLL const DDS_Char* EntityGetEntityName(DDS_Entity* self);

DCPSDLL DDS_ReturnCode_t DataWriterToXML(
    DDS_DataWriter* dw,
    const DDS_Char** result,
    DDS_Boolean containedQos);

DCPSDLL DDS_ReturnCode_t DataWriterParseWriteSampleInfoXML(
    DDS_DataWriter* dw,
    const DDS_Char* xml_content,
    DDS_SampleInfo* sample_info,
    DDS_SampleInfoValidMember* valid_sample_info_member);

DCPSDLL DDS_ReturnCode_t DataWriterParseWriteSampleXML(
    DDS_DataWriter* dw,
    const DDS_Char* xml_content,
    ZRDynamicData** data);

DCPSDLL DDS_ReturnCode_t DataWriterReturnXMLSample(
    DDS_DataWriter* dw,
    ZRDynamicData* data);

#endif /* _ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

DCPSDLL DDS_ReturnCode_t DataWriterSetQosWithProfile(
    DDS_DataWriter* dw,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

/* ÉùÃ÷DDS_DataWriterSeq */
DDS_SEQUENCE_C(DDS_DataWriterSeq, DDS_DataWriter*);

#ifdef __cplusplus
}
#endif

#endif /* DataWriter_h__*/
