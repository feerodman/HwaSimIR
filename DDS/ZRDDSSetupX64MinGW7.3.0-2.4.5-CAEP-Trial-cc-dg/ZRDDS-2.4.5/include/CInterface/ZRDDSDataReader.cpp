/**
 * @file:       ZRDDSDataReader.cpp
 *
 * copyright:   Copyright (c) 2018 ZhenRong Technology, Inc. All rights reserved.
 */

#include "ZRDDSDataReader.h"
#include "DataReader.h"
#include <stdlib.h>

/* read/take方法以及instance查询，不包含condition */
#define ZRDDSDataReader_READ_TAKE_METHODS_IMPL(TDataReader, TTypeSeq, TType)\
DDS_ReturnCode_t TDataReader##ReadOrTake(                                   \
    TDataReader* self,                                                      \
    TTypeSeq* dataValues,                                                   \
    DataReaderReadOrTakeParams* params)                                     \
{                                                                           \
    DDS_ReturnCode_t result = DDS_RETCODE_OK;                               \
    params->isLoan = false;                                                 \
    params->dataPtrArray = NULL;                                            \
    params->dataCount = 0;                                                  \
    if (dataValues == NULL)                                                 \
    {                                                                       \
        return DDS_RETCODE_BAD_PARAMETER;                                   \
    }                                                                       \
    params->dataSeqLen = TType##Seq_get_length(dataValues);                 \
    params->dataSeqMaxLen = TType##Seq_get_maximum(dataValues);             \
    params->dataSeq_has_ownership = TType##Seq_has_ownership(dataValues);   \
    params->dataSeqBuffer = (DDS_Char*)TType##Seq_get_contiguous_buffer(dataValues);\
    params->dataSize = sizeof(TType);                                       \
    params->dataSeqBufferType = 0;                                          \
                                                                            \
    result = DataReaderImplReadOrTake(                              \
        (DataReaderImpl*)self, params);                             \
    if (result == DDS_RETCODE_NO_DATA)                              \
    {                                                               \
        TType##Seq_set_length(dataValues, (DDS_Long)0);             \
        return result;                                              \
    }                                                               \
    if (result != DDS_RETCODE_OK)                                   \
    {                                                               \
        return result;                                              \
    }                                                               \
    if (params->isLoan)                                             \
    {                                                               \
        if (!TType##Seq_loan_discontiguous(dataValues,              \
                (TType**)params->dataPtrArray,                      \
                params->dataCount, params->dataCount))              \
        {                                                           \
           result = DDS_RETCODE_ERROR;                              \
           DataReaderImplReturnLoan((DataReaderImpl*)self,          \
                params->dataPtrArray, params->dataCount,            \
                params->sampleInfos);                               \
        }                                                           \
    }                                                               \
    else                                                            \
    {                                                               \
        if (!TType##Seq_set_length(dataValues, params->dataCount))  \
        {                                                           \
            result = DDS_RETCODE_ERROR;                             \
        }                                                           \
    }                                                               \
    return result;                                                  \
}                                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_read(                \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = NULL;                           \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = false;                          \
    params.isNext = false;                          \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_take(                \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = NULL;                           \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = true;                           \
    params.isNext = false;                          \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_read_next_sample(    \
    TDataReader* self,                              \
    TType* dataValue,                               \
    DDS_SampleInfo* sampleInfo)                     \
{                                                   \
    return DataReaderImplReadOrTakeNextSample(      \
            (DataReaderImpl*)self,                  \
            (DDS_Char*)dataValue,                    \
            sampleInfo,                             \
            false);                                 \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_take_next_sample(    \
    TDataReader* self,                              \
    TType* dataValue,                               \
    DDS_SampleInfo* sampleInfo)                     \
{                                                   \
    return DataReaderImplReadOrTakeNextSample(      \
            (DataReaderImpl*)self,                  \
            (DDS_Char*)dataValue,                    \
            sampleInfo,                             \
            true);                                  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_read_instance(       \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    const DDS_InstanceHandle_t* handle,             \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    if (NULL == handle)                             \
    {                                               \
        return DDS_RETCODE_BAD_PARAMETER;           \
    }                                               \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = handle;                         \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = false;                          \
    params.isNext = false;                          \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_take_instance(       \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    const DDS_InstanceHandle_t* handle,             \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    if (NULL == handle)                             \
    {                                               \
        return DDS_RETCODE_BAD_PARAMETER;           \
    }                                               \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = handle;                         \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = true;                           \
    params.isNext = false;                          \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_read_next_instance(  \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    const DDS_InstanceHandle_t* previousHandle,     \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    if (NULL == previousHandle)                     \
    {                                               \
        return DDS_RETCODE_BAD_PARAMETER;           \
    }                                               \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = previousHandle;                 \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = false;                          \
    params.isNext = true;                           \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_take_next_instance(  \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos,                 \
    DDS_Long maxSamples,                            \
    const DDS_InstanceHandle_t* previousHandle,     \
    DDS_SampleStateMask sampleMask,                 \
    DDS_ViewStateMask viewMask,                     \
    DDS_InstanceStateMask instanceMask)             \
{                                                   \
    DataReaderReadOrTakeParams params;              \
    if (NULL == previousHandle)                     \
    {                                               \
        return DDS_RETCODE_BAD_PARAMETER;           \
    }                                               \
    params.sampleInfos = sampleInfos;               \
    params.maxSamples = maxSamples;                 \
    params.handle = previousHandle;                 \
    params.sampleMask = sampleMask;                 \
    params.viewMask = viewMask;                     \
    params.instanceMask = instanceMask;             \
    params.isTake = true;                           \
    params.isNext = true;                           \
    params.readCondition = NULL;                    \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                                   \
                                                    \
DDS_ReturnCode_t TDataReader##_return_loan(         \
    TDataReader* self,                              \
    TTypeSeq* dataValues,                           \
    DDS_SampleInfoSeq* sampleInfos)                 \
{                                                   \
    DDS_Long dataSeqMaxLen;                         \
    TType **dataSeqDiscontiguousBuffer;             \
    DDS_ReturnCode_t result = DDS_RETCODE_OK;       \
    if (dataValues == NULL)                         \
    {                                               \
        return DDS_RETCODE_BAD_PARAMETER;           \
    }                                               \
    if (TType##Seq_has_ownership(dataValues) &&     \
            DDS_SampleInfoSeq_has_ownership(sampleInfos))   \
    {                                               \
        return result;                              \
    }                                               \
                                                    \
    dataSeqMaxLen = TType##Seq_get_maximum(dataValues);            \
    dataSeqDiscontiguousBuffer = TType##Seq_get_discontiguous_buffer(dataValues);  \
    result = DataReaderImplReturnLoan(                                      \
        (DataReaderImpl*)self,                                              \
        (void**)dataSeqDiscontiguousBuffer, dataSeqMaxLen, sampleInfos);    \
    if (result != DDS_RETCODE_OK)                                           \
    {                                                                       \
        return result;                                                      \
    }                                                                       \
    if (!TType##Seq_unloan(dataValues))                                     \
    {                                                                       \
        result = DDS_RETCODE_ERROR;                                         \
        return result;                                                      \
    }                                                                       \
    if (!DDS_SampleInfoSeq_unloan(sampleInfos))                             \
    {                                                                       \
         result = DDS_RETCODE_ERROR;                                        \
         return result;                                                     \
    }                                                                       \
    return result;                                                          \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_return_recv_buffer(                          \
    TDataReader* self,                                                      \
    DDS_SampleInfo* sampleInfo)                                             \
{                                                                           \
    return DataReaderImplReturnRecvBuffer(                                  \
        (DataReaderImpl*)self,                                              \
        sampleInfo);                                                        \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_loan_recv_buffer(                            \
    TDataReader* self,                                                      \
    DDS_SampleInfo* sampleInfo)                                             \
{                                                                           \
    return DataReaderImplLoanRecvBuffer(                                    \
        (DataReaderImpl*)self,                                              \
        sampleInfo);                                                        \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_get_key_value(                               \
    TDataReader* self,                                                      \
    TType* keyHolder,                                                       \
    const DDS_InstanceHandle_t* handle)                                     \
{                                                                           \
    return DataReaderImplGetKeyValue(                                       \
                (DataReaderImpl*)self,                                      \
                (void*)keyHolder,                                           \
                handle);                                                    \
}                                                                           \
                                                                            \
DDS_InstanceHandle_t TDataReader##_lookup_instance(                         \
    TDataReader* self,                                                      \
    const TType* instance)                                                  \
{                                                                           \
    return DataReaderImplLookupInstance(                                    \
                (DataReaderImpl*)self,                                      \
                (const void*)instance);                                     \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_get_data_instance(                           \
    TDataReader* self,                                                      \
    DDS_InstanceHandleSeq* dataHandles,                                     \
    DDS_SampleStateMask sampleMask,                                         \
    DDS_ViewStateMask viewMask,                                             \
    DDS_InstanceStateMask instanceMask)                                     \
{                                                                           \
    return DataReaderImplGetDataInstance(                                   \
                (DataReaderImpl*)self,                                      \
                dataHandles,                                                \
                sampleMask,                                                 \
                viewMask,                                                   \
                instanceMask);                                              \
}

/* 带condition方法的read/take */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS_IMPL(TDataReader, TTypeSeq, TType)    \
DDS_ReturnCode_t TDataReader##_read_next_instance_w_condition(\
    TDataReader* self,                          \
    TTypeSeq* dataValues,                       \
    DDS_SampleInfoSeq* sampleInfos,             \
    DDS_Long maxSamples,                        \
    const DDS_InstanceHandle_t* previousHandle, \
    DDS_ReadCondition* condition)               \
{                                               \
    DataReaderReadOrTakeParams params;          \
    if (NULL == previousHandle)                 \
    {                                           \
        return DDS_RETCODE_BAD_PARAMETER;       \
    }                                           \
    params.sampleInfos = sampleInfos;           \
    params.maxSamples = maxSamples;             \
    params.handle = previousHandle;             \
    params.isTake = false;                      \
    params.isNext = true;                       \
    params.readCondition = condition;           \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                               \
                                                \
DDS_ReturnCode_t TDataReader##_take_next_instance_w_condition(  \
    TDataReader* self,                          \
    TTypeSeq* dataValues,                       \
    DDS_SampleInfoSeq* sampleInfos,             \
    DDS_Long maxSamples,                        \
    const DDS_InstanceHandle_t* previousHandle, \
    DDS_ReadCondition *condition)               \
{                                               \
    DataReaderReadOrTakeParams params;          \
    if (NULL == previousHandle)                 \
    {                                           \
        return DDS_RETCODE_BAD_PARAMETER;       \
    }                                           \
    params.sampleInfos = sampleInfos;           \
    params.maxSamples = maxSamples;             \
    params.handle = previousHandle;             \
    params.isTake = true;                       \
    params.isNext = true;                       \
    params.readCondition = condition;           \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                               \
                                                \
DDS_ReturnCode_t TDataReader##_read_w_condition(\
    TDataReader* self,                          \
    TTypeSeq* dataValues,                       \
    DDS_SampleInfoSeq* sampleInfos,             \
    DDS_Long maxSamples,                        \
    DDS_ReadCondition *condition)               \
{                                               \
    DataReaderReadOrTakeParams params;          \
    params.sampleInfos = sampleInfos;           \
    params.maxSamples = maxSamples;             \
    params.handle = NULL;                       \
    params.isTake = false;                      \
    params.isNext = false;                      \
    params.readCondition = condition;           \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}                                               \
                                                \
DDS_ReturnCode_t TDataReader##_take_w_condition(\
    TDataReader* self,                          \
    TTypeSeq* dataValues,                       \
    DDS_SampleInfoSeq* sampleInfos,             \
    DDS_Long maxSamples,                        \
    DDS_ReadCondition *condition)               \
{                                               \
    DataReaderReadOrTakeParams params;          \
    params.sampleInfos = sampleInfos;           \
    params.maxSamples = maxSamples;             \
    params.handle = NULL;                       \
    params.isTake = true;                       \
    params.isNext = false;                      \
    params.readCondition = condition;           \
    return TDataReader##ReadOrTake(self, dataValues, &params);  \
}
#else
#define ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS_IMPL(TDataReader, TTypeSeq, TType)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* readcondition的声明周期关联方法 */
#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_CONDITON_METHODS_IMPL(TDataReader)             \
DDS_ReadCondition* TDataReader##_create_readcondition(                      \
    TDataReader* self,                                                      \
    DDS_SampleStateMask sampleMask,                                         \
    DDS_ViewStateMask viewMask,                                             \
    DDS_InstanceStateMask instanceMask)                                     \
{                                                                           \
    return DataReaderImplCreateReadCondition(                               \
                (DataReaderImpl*)self,                                      \
                sampleMask,                                                 \
                viewMask,                                                   \
                instanceMask);                                              \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_delete_readcondition(                        \
    TDataReader* self,                                                      \
    DDS_ReadCondition* condition)                                           \
{                                                                           \
    return DataReaderImplDeleteReadCondition(                               \
                (DataReaderImpl*)self,                                      \
                condition);                                                 \
}                                                                           \
                                                                            \
DDS_ReturnCode_t TDataReader##_delete_contained_entities(TDataReader* self) \
{                                                                           \
    return DataReaderImplDeleteContainedEntities(                           \
                (DataReaderImpl*)self);                                     \
}
#else
#define ZRDDSDataReader_READ_CONDITON_METHODS_IMPL(TDataReader) 
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */

/* queryCondition的生命周期方法 */
#ifdef _ZRDDS_INCLUDE_QUERY_CONDITION
#define ZRDDSDataReader_QUERY_CONDITON_METHODS_IMPL(TDataReader)    \
DDS_QueryCondition* TDataReader##_create_querycondition(            \
    TDataReader* self,                                              \
    DDS_SampleStateMask sampleMask,                                 \
    DDS_ViewStateMask viewMask,                                     \
    DDS_InstanceStateMask instanceMask,                             \
    const DDS_Char* queryExpression,                                \
    const DDS_StringSeq* queryParameters)                           \
{                                                                   \
    return DataReaderImplCreateQueryCondition(                      \
                (DataReaderImpl*)self,                              \
                sampleMask,                                         \
                viewMask,                                           \
                instanceMask,                                       \
                queryExpression,                                    \
                queryParameters);                                   \
}
#else
#define ZRDDSDataReader_QUERY_CONDITON_METHODS_IMPL(TDataReader)
#endif /* _ZRDDS_INCLUDE_QUERY_CONDITION */

#define ZRDDSDataReader_ENTITY_METHODS_IMPL(TDataReader)                \
DDS_ReturnCode_t TDataReader##_set_qos(                                 \
    TDataReader* self,                                                  \
    const DDS_DataReaderQos* qoslist)                                   \
{                                                                       \
    return DataReaderImplSetQos(                                        \
                (DataReaderImpl*)self,                                  \
                qoslist);                                               \
}                                                                       \
                                                                        \
DDS_ReturnCode_t TDataReader##_get_qos(                                 \
    TDataReader* self,                                                  \
    DDS_DataReaderQos* qoslist)                                         \
{                                                                       \
    return DataReaderImplGetQos(                                        \
                (DataReaderImpl*)self,                                  \
                qoslist);                                               \
}                                                                       \
                                                                        \
DDS_ReturnCode_t TDataReader##_set_listener(                            \
    TDataReader* self,                                                  \
    DDS_DataReaderListener *listener,                                   \
    DDS_StatusKindMask mask)                                            \
{                                                                       \
    return DataReaderImplSetListener(                                   \
                (DataReaderImpl*)self,                                  \
                listener,                                               \
                mask);                                                  \
}                                                                       \
                                                                        \
DDS_DataReaderListener* TDataReader##_get_listener(TDataReader* self)   \
{                                                                       \
    return DataReaderImplGetListener(                                   \
                (DataReaderImpl*)self);                                 \
}                                                                       \
                                                                        \
DDS_ReturnCode_t TDataReader##_enable(TDataReader* self)                \
{                                                                       \
    return DataReaderImplEnable(                                        \
                (DataReaderImpl*)self);                                 \
}                                                                       \
                                                                        \
DDS_Entity* TDataReader##_as_entity(TDataReader* self)                  \
{                                                                       \
    return DataReaderImplAsEntity((DataReaderImpl*)self);               \
}

#ifdef _ZRDDS_INCLUDE_LIVELINESS_QOS
#define ZRDDSDataReader_LIVELINESS_METHODS_IMPL(TDataReader)    \
DDS_ReturnCode_t TDataReader##_get_liveliness_changed_status(   \
    TDataReader* self,                                          \
    DDS_LivelinessChangedStatus* status)                        \
{                                                               \
    return DataReaderImplGetLivelinessChangedStatus(            \
                (DataReaderImpl*)self,                          \
                status);                                        \
}
#else 
#define ZRDDSDataReader_LIVELINESS_METHODS_IMPL(TDataReader)
#endif /* _ZRDDS_INCLUDE_LIVELINESS_QOS */

#ifdef _ZRDDS_INCLUDE_DEADLINE_QOS
#define ZRDDSDataReader_DEADLINE_METHODS_IMPL(TDataReader)              \
DDS_ReturnCode_t TDataReader##_get_requested_deadline_missed_status(    \
    TDataReader* self,                                                  \
    DDS_RequestedDeadlineMissedStatus* status)                          \
{                                                                       \
    return DataReaderImplGetRequestedDeadlineMissedStatus(              \
                (DataReaderImpl*)self,                                  \
                status);                                                \
}
#else 
#define ZRDDSDataReader_DEADLINE_METHODS_IMPL(TDataReader)
#endif /* _ZRDDS_INCLUDE_DEADLINE_QOS */

#define ZRDDSDataReader_MATCH_METHODS_IMPL(TDataReader)         \
DDS_ReturnCode_t TDataReader##_get_subscription_matched_status( \
    TDataReader* self,                                          \
    DDS_SubscriptionMatchedStatus* status)                      \
{                                                               \
    return DataReaderImplGetSubscriptionMatchedStatus(          \
                (DataReaderImpl*)self,                          \
                status);                                        \
}                                                               \
DDS_ReturnCode_t TDataReader##_get_matched_publication_data(    \
    TDataReader* self,                                          \
    DDS_PublicationBuiltinTopicData* publicationData,           \
    const DDS_InstanceHandle_t* publicationHandle)              \
{                                                               \
    return DataReaderImplGetMatchedPublicationData(             \
                (DataReaderImpl*)self,                          \
                publicationData,                                \
                publicationHandle);                             \
}                                                               \
                                                                \
DDS_ReturnCode_t TDataReader##_get_matched_publications(        \
    TDataReader* self,                                          \
    DDS_InstanceHandleSeq* publicationHandles)                  \
{                                                               \
    return DataReaderImplGetMatchedPublications(                \
                (DataReaderImpl*)self,                          \
                publicationHandles);                            \
}

#define ZRDDSDataReader_SAMPLE_STATUS_METHODS_IMPL(TDataReader) \
DDS_ReturnCode_t TDataReader##_get_sample_lost_status(      \
    TDataReader* self,                                      \
    DDS_SampleLostStatus* status)                           \
{                                                           \
    return DataReaderImplGetSampleLostStatus(               \
                (DataReaderImpl*)self,                      \
                status);                                    \
}                                                           \
                                                            \
DDS_ReturnCode_t TDataReader##_get_sample_rejected_status(  \
    TDataReader* self,                                      \
    DDS_SampleRejectedStatus* status)                       \
{                                                           \
    return DataReaderImplGetSampleRejectedStatus(           \
                (DataReaderImpl*)self,                      \
                status);                                    \
}

#define ZRDDSDataReader_FACTORY_METHODS_IMPL(TDataReader)                       \
DDS_TopicDescription* TDataReader##_get_topicdescription(TDataReader* self)    \
{                                                                               \
    return DataReaderImplGetTopicDescription(                                   \
                (DataReaderImpl*)self);                                         \
}                                                                               \
                                                                                \
DDS_Subscriber* TDataReader##_get_subscriber(TDataReader* self)                 \
{                                                                               \
    return DataReaderImplGetSubscriber(                                         \
                (DataReaderImpl*)self);                                         \
}

#define ZRDDSDataReader_COMMON_STATUS_METHODS_IMPL(TDataReader)         \
DDS_ReturnCode_t TDataReader##_get_requested_incompatible_qos_status(   \
    TDataReader* self,                                                  \
    DDS_RequestedIncompatibleQosStatus* status)                         \
{                                                                       \
    return DataReaderImplGetRequestedIncompatibleQosStatus(             \
                (DataReaderImpl*)self,                                  \
                status);                                                \
}

#define ZRDDSDataReader_HISTORY_METHODS_IMPL(TDataReader)   \
DDS_ReturnCode_t TDataReader##_wait_for_historical_data(    \
    TDataReader* self, const DDS_Duration_t* maxWait)       \
{                                                           \
    return DataReaderImplWaitForHistoricalData(             \
                (DataReaderImpl*)self,                      \
                maxWait);                                   \
}

#ifdef _ZRDDS_INCLUDE_BREAKPOINT_RESUME 
#define ZRDDSDataReader_BREAKPOINT_RESUME_IMPL(TDataReader)       \
    DDS_ReturnCode_t TDataReader##_record_data(            \
        TDataReader* self,                                  \
        DDS_SampleInfoSeq* sampleInfos,                     \
        ZR_BOOLEAN finish)                                  \
    {                                                       \
         return DataReaderImplRecordData(                  \
             (DataReaderImpl*)self,                         \
             sampleInfos,                                   \
             finish);                                       \
    }
#else /*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/
#define ZRDDSDataReader_BREAKPOINT_RESUME_IMPL(TDataReader)
#endif /*_ZRDDS_INCLUDE_BREAKPOINT_RESUME*/

#ifdef _ZRXMLINTERFACE
#ifdef _ZRXMLQOSINTERFACE
#define ZRDDSDataReader_XML_QOS_METHODS_IMPL(TDataReader)       \
    DDS_ReturnCode_t TDataReader##set_qos_with_profile(     \
        TDataReader* self,                                  \
        const DDS_Char* library_name,                       \
        const DDS_Char* profile_name,                       \
        const DDS_Char* qos_name)                           \
    {                                                       \
        return DataReaderSetQosWithProfile(                 \
            (DataReaderImpl*)self,                          \
            library_name, profile_name, qos_name);          \
    }
#else /* _ZRXMLQOSINTERFACE */
#define ZRDDSDataReader_XML_QOS_METHODS_IMPL(TDataReader)
#endif /* _ZRXMLQOSINTERFACE */
#ifdef _ZRXMLENTITYINTERFACE
#define ZRDDSDataReader_XML_ENTITY_METHODS_IMPL(TDataReader)\
    DDS_ReturnCode_t TDataReader##_to_xml(                  \
        TDataReader* self,                                  \
        const DDS_Char** result,                            \
        DDS_Boolean contained_qos)                          \
    {                                                       \
        return DataReaderImplToXML(                         \
            (DataReaderImpl*)self, result, contained_qos);  \
    }                                                       \
    DDS_ReturnCode_t TDataReader##_read_sample_info_to_xml_string(   \
        TDataReader *self,                                  \
        DDS_SampleInfo* sample_info,                        \
        DDS_SampleInfoValidMember* valid_sample_info_member,\
        const DDS_Char** result)                            \
    {                                                       \
        return DataReaderReadSampleInfoToXMLString(         \
            (DataReaderImpl*)self,                          \
            sample_info,                                    \
            valid_sample_info_member,                       \
            result);                                        \
    }                                                       \
    DDS_ReturnCode_t TDataReader##_sample_to_xml_string(    \
        TDataReader* self,                                  \
        ZRDynamicData* data,                                \
        const DDS_Char** result)                            \
    {                                                       \
        return DataReaderSampleToXMLString(                 \
            (DataReaderImpl*)self, data, result);           \
    }                                                       \
    const DDS_Char* TDataReader##_get_entity_name(          \
        TDataReader* self)                                  \
    {                                                       \
        return EntityGetEntityName(                         \
            DataReaderImplAsEntity((DataReaderImpl*)self)); \
    }                                                       \
    DDS_Subscriber* TDataReader##_get_factory(              \
        TDataReader* self)                                  \
    {                                                       \
        return DataReaderImplGetSubscriber(                 \
            (DataReaderImpl*)self);                         \
    }

#ifdef _ZRDDS_INCLUDE_READ_CONDITION
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS_IMPL(TDataReader) \
    DDS_ReadCondition* TDataReader##_create_named_readcondition(    \
        TDataReader* self,                                          \
        const DDS_Char* name,                                       \
        DDS_SampleStateMask sample_mask,                            \
        DDS_ViewStateMask view_mask,                                \
        DDS_InstanceStateMask instance_mask)                        \
    {                                                               \
        return DataReaderImplCreateNamedReadCondition(              \
            (DataReaderImpl*)self,                                  \
            name, sample_mask, view_mask, instance_mask);           \
    }                                                               \
    DDS_ReturnCode_t TDataReader##_delete_named_readcondition(      \
        TDataReader* self,                                          \
        const DDS_Char* name)                                       \
    {                                                               \
        return DataReaderImplDeleteNamedReadCondition(              \
            (DataReaderImpl*)self, name);                           \
    }                                                               \
    DDS_ReadCondition* TDataReader##_get_named_readcondition(       \
        TDataReader* self,                                          \
        const DDS_Char* name)                                       \
    {                                                               \
        return DataReaderImplGetNamedReadCondition(                 \
            (DataReaderImpl*)self, name);                           \
    }

#ifdef _ZRDDS_INCLUDE_QUERY_CONDITION
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)\
    DDS_QueryCondition* TDataReader##_create_named_querycondition(  \
        TDataReader* self,                                          \
        const DDS_Char* name,                                       \
        const DDS_SampleStateMask sample_mask,                      \
        const DDS_ViewStateMask view_mask,                          \
        const DDS_InstanceStateMask instance_mask,                  \
        const DDS_Char* query_expression,                           \
        const DDS_StringSeq* query_parameters)                      \
    {                                                               \
        return NULL;                                                \
    }
#else /* _ZRDDS_INCLUDE_QUERY_CONDITION */
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)
#endif /* _ZRDDS_INCLUDE_QUERY_CONDITION */

#else /* _ZRDDS_INCLUDE_READ_CONDITION */
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)
#endif /* _ZRDDS_INCLUDE_READ_CONDITION */
#else /* _ZRXMLENTITYINTERFACE */
#define ZRDDSDataReader_XML_ENTITY_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)
#endif /* _ZRXMLENTITYINTERFACE */
#else /* _ZRXMLINTERFACE */
#define ZRDDSDataReader_XML_QOS_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_XML_ENTITY_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_READ_CONDITON_XML_METHODS_IMPL(TDataReader)
#define ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)
#endif /* _ZRXMLINTERFACE */

/* ZRDDSDataReader模板实现*/
#define ZRDDSDataReaderImpl(TDataReader, TTypeSeq, TType)                               \
    ZRDDSDataReader_READ_TAKE_METHODS_IMPL(TDataReader, TTypeSeq, TType)                \
    ZRDDSDataReader_READ_TAKE_WITH_CONDITON_METHODS_IMPL(TDataReader, TTypeSeq, TType)  \
    ZRDDSDataReader_READ_CONDITON_METHODS_IMPL(TDataReader)                             \
    ZRDDSDataReader_QUERY_CONDITON_METHODS_IMPL(TDataReader)                            \
    ZRDDSDataReader_ENTITY_METHODS_IMPL(TDataReader)                                    \
    ZRDDSDataReader_LIVELINESS_METHODS_IMPL(TDataReader)                                \
    ZRDDSDataReader_DEADLINE_METHODS_IMPL(TDataReader)                                  \
    ZRDDSDataReader_MATCH_METHODS_IMPL(TDataReader)                                     \
    ZRDDSDataReader_SAMPLE_STATUS_METHODS_IMPL(TDataReader)                             \
    ZRDDSDataReader_FACTORY_METHODS_IMPL(TDataReader)                                   \
    ZRDDSDataReader_COMMON_STATUS_METHODS_IMPL(TDataReader)                             \
    ZRDDSDataReader_HISTORY_METHODS_IMPL(TDataReader)                                   \
    ZRDDSDataReader_XML_QOS_METHODS_IMPL(TDataReader)                                   \
    ZRDDSDataReader_XML_ENTITY_METHODS_IMPL(TDataReader)                                \
    ZRDDSDataReader_READ_CONDITON_XML_METHODS_IMPL(TDataReader)                         \
    ZRDDSDataReader_QUERY_CONDITON_XML_METHODS_IMPL(TDataReader)                        \
    ZRDDSDataReader_BREAKPOINT_RESUME_IMPL(TDataReader)
