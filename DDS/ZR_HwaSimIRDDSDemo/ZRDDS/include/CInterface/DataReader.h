/**
 * @file:       DataReader.h
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#ifndef DataReaderUser_h__
#define DataReaderUser_h__

#include "ReturnCode_t.h"
#include "SampleInfo.h"
#include "SampleStateMask.h"
#include "ViewStateMask.h"
#include "InstanceStateMask.h"
#include "StatusKindMask.h"
#include "LivelinessChangedStatus.h"
#include "RequestedIncompatibleQosStatus.h"
#include "RequestedDeadlineMissedStatus.h"
#include "SampleLostStatus.h"
#include "SampleRejectedStatus.h"
#include "SubscriptionMatchedStatus.h"
#include "PublicationBuiltinTopicData.h"
#include "Condition.h"
#include "Entity.h"
#include "Topic.h"
#include "DataReaderQos.h"
#include "DataReaderListener.h"
#include "InstanceHandle_t.h"
#include "Duration_t.h"
#include "ZRDynamicData.h"
#include "DataReaderReadOrTakeParams.h"

#ifdef __cplusplus
extern "C"
{
#endif

DCPSDLL extern DDS_ReturnCode_t DataReaderImplReadOrTake(
    DDS_DataReader* self,
    DataReaderReadOrTakeParams* params);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplReadOrTakeNextSample(
    DDS_DataReader* self,
    ZR_INT8* dataValue,
    DDS_SampleInfo* sampleInfo,
    ZR_BOOLEAN isTake);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplReturnLoan(
    DDS_DataReader* self,
    void** dataSeqBuffer,
    ZR_INT32 dataSeqMaxLen,
    DDS_SampleInfoSeq* sampleInfos);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplReturnRecvBuffer(
    DDS_DataReader* self,
    DDS_SampleInfo* sampleInfo);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplLoanRecvBuffer(
    DataReaderImpl* self,
    DDS_SampleInfo* sampleInfo);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetKeyValue(
    DDS_DataReader* self,
    void* keyHolder,
    const DDS_InstanceHandle_t* handle);

DCPSDLL extern DDS_InstanceHandle_t DataReaderImplLookupInstance(
    DDS_DataReader* self,
    const void* instance);
/* 以下是类型无关的方法 */
DCPSDLL extern DDS_ReadCondition* DataReaderImplCreateReadCondition(
    DDS_DataReader* self,
    DDS_SampleStateMask sampleMask,
    DDS_ViewStateMask viewMask,
    DDS_InstanceStateMask instanceMask);

DCPSDLL extern DDS_QueryCondition* DataReaderImplCreateQueryCondition(
    DDS_DataReader* self,
    DDS_SampleStateMask sampleMask,
    DDS_ViewStateMask viewMask,
    DDS_InstanceStateMask instanceMask,
    const ZR_INT8* queryExpression,
    const DDS_StringSeq* queryParameters);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplDeleteReadCondition(
    DDS_DataReader* self,
    DDS_ReadCondition* condition);

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetLivelinessChangedStatus(
    DDS_DataReader* self,
    DDS_LivelinessChangedStatus* status);
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetRequestedDeadlineMissedStatus(
    DDS_DataReader* self,
    DDS_RequestedDeadlineMissedStatus* status);
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetRequestedIncompatibleQosStatus(
    DDS_DataReader* self,
    DDS_RequestedIncompatibleQosStatus* status);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetSampleLostStatus(
    DDS_DataReader* self,
    DDS_SampleLostStatus* status);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetSampleRejectedStatus(
    DDS_DataReader* self,
    DDS_SampleRejectedStatus* status);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetSubscriptionMatchedStatus(
    DDS_DataReader* self,
    DDS_SubscriptionMatchedStatus* status);

DCPSDLL extern DDS_TopicDescription* DataReaderImplGetTopicDescription(DDS_DataReader* self);

DCPSDLL extern DDS_Subscriber* DataReaderImplGetSubscriber(DDS_DataReader* self);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplDeleteContainedEntities(DDS_DataReader* self);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplWaitForHistoricalData(
    DDS_DataReader* self,
    const DDS_Duration_t* maxWait);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetMatchedPublicationData(
    DDS_DataReader* self,
    DDS_PublicationBuiltinTopicData* publicationData,
    const DDS_InstanceHandle_t* publicationHandle);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetMatchedPublications(
    DDS_DataReader* self,
    DDS_InstanceHandleSeq* publicationHandles);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplSetQos(
    DDS_DataReader* self,
    const DDS_DataReaderQos* qoslist);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetQos(
    DDS_DataReader* self,
    DDS_DataReaderQos* qoslist);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplSetListener(
    DDS_DataReader* self,
    DDS_DataReaderListener *listener,
    DDS_StatusKindMask mask);

DCPSDLL extern DDS_DataReaderListener* DataReaderImplGetListener(DDS_DataReader* self);

DCPSDLL extern DDS_ReturnCode_t DataReaderImplEnable(DDS_DataReader* self);

DCPSDLL extern DDS_Entity* DataReaderImplAsEntity(DDS_DataReader* self);

#ifdef _ZRDDS_INCLUDE_BREAKPOINT_RESUME
DCPSDLL extern DDS_ReturnCode_t DataReaderImplRecordData(
    DDS_DataReader* self,
    DDS_SampleInfoSeq *sampleInfos,
    ZR_BOOLEAN finish);
#endif /*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/

DCPSDLL extern DDS_ReturnCode_t DataReaderImplGetDataInstance(
    DDS_DataReader* self,
    DDS_InstanceHandleSeq* dataHandles,
    DDS_SampleStateMask sampleMask,
    DDS_ViewStateMask viewMask,
    DDS_InstanceStateMask instanceMask);

/* 声明DataReaderImplSeq */
DDS_SEQUENCE_C(DDS_DataReaderSeq, DDS_DataReader*);

#ifdef _ZRXMLINTERFACE

#ifdef _ZRXMLENTITYINTERFACE

DCPSDLL const DDS_Char* EntityGetEntityName(DDS_Entity* self);

DCPSDLL DDS_ReturnCode_t DataReaderReadSampleInfoToXMLString(
    DDS_DataReader* self,
    DDS_SampleInfo* sample_info,
    DDS_SampleInfoValidMember* valid_sample_info_member,
    const DDS_Char** result);

DCPSDLL DDS_ReturnCode_t DataReaderSampleToXMLString(
    DDS_DataReader* self,
    ZRDynamicData* data,
    const DDS_Char** result);

#ifdef _ZRDDS_INCLUDE_READ_CONDITION
DCPSDLL DDS_ReadCondition* DataReaderImplCreateNamedReadCondition(
    DDS_DataReader* self,
    const ZR_INT8* name,
    DDS_SampleStateMask sampleMask,
    DDS_ViewStateMask viewMask,
    DDS_InstanceStateMask instanceMask);

DCPSDLL DDS_ReturnCode_t DataReaderImplDeleteNamedReadCondition(
    DDS_DataReader* self,
    const ZR_INT8* name);

DCPSDLL DDS_ReadCondition* DataReaderImplGetNamedReadCondition(
    DDS_DataReader* self,
    const ZR_INT8* name);

DCPSDLL DDS_ReturnCode_t DataReaderImplToXML(
    DDS_DataReader* self,
    const DDS_Char** result,
    ZR_BOOLEAN containedQos);

#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

#endif /*_ZRXMLENTITYINTERFACE */

#ifdef _ZRXMLQOSINTERFACE

DCPSDLL DDS_ReturnCode_t DataReaderSetQosWithProfile(
    DDS_DataReader* self,
    const DDS_Char* library_name,
    const DDS_Char* profile_name,
    const DDS_Char* qos_name);

#endif /* _ZRXMLQOSINTERFACE */

#endif /* _ZRXMLINTERFACE */

#ifdef __cplusplus
}
#endif

#endif /* DataReaderUser_h__*/
