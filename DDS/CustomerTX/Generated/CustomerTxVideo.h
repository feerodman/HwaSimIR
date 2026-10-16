#ifndef CustomerTxVideo_H_
#define CustomerTxVideo_H_

/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "OsResource.h"
#include "ZRSequence.h"
#include "TypeCode.h"
#include "CDRStream.h"
#include "ZRDDSCppWrapper.h"
#include "ZRBuiltinTypes.h"

typedef struct ZRMemPool ZRMemPool;


typedef struct HD264Stream
{
    DDS_ULong DataLength; // @ID(0)
    DDS_Octet VedioStream[1400]; // @ID(1)
} HD264Stream; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(HD264StreamSeq, HD264Stream);

// 用户使用接口
DDS_Boolean HD264StreamInitialize(
    HD264Stream* self);

DDS_Boolean HD264StreamInitializeEx(
    HD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void HD264StreamFinalize(
    HD264Stream* self);

void HD264StreamFinalizeEx(
    HD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean HD264StreamCopy(
    HD264Stream* dst,
    const HD264Stream* src);

DDS_Boolean HD264StreamCopyEx(
    HD264Stream* dst,
    const HD264Stream* src,
    ZRMemPool* pool);

void HD264StreamPrintData(
    const HD264Stream* sample);

DDS::TypeCode* HD264StreamGetTypeCode();

// 底层使用函数
HD264Stream* HD264StreamCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void HD264StreamDestroySample(
    ZRMemPool* pool,
    HD264Stream* sample);

DDS_ULong HD264StreamGetSerializedSampleMaxSize();

DDS_ULong HD264StreamGetSerializedSampleSize(
    const HD264Stream* sample,
    DDS_ULong currentAlignment);

DDS_Long HD264StreamSerialize(
    const HD264Stream* sample,
    CDRSerializer* cdr);

DDS_Long HD264StreamDeserialize(
    HD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong HD264StreamGetSerializedKeyMaxSize();

DDS_ULong HD264StreamGetSerializedKeySize(
    const HD264Stream* sample,
    DDS_ULong currentAlignment);

DDS_Long HD264StreamSerializeKey(
    const HD264Stream* sample,
    CDRSerializer* cdr);

DDS_Long HD264StreamDeserializeKey(
    HD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long HD264StreamGetKeyHash(
    const HD264Stream* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean HD264StreamHasKey();

TypeCodeHeader* HD264StreamGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean HD264StreamNoSerializingSupported();

DDS_ULong HD264StreamFixedHeaderLength();

DDS_Long HD264StreamOnSiteDeserialize(CDRDeserializer* cdr,
    HD264Stream* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* HD264StreamLoanSampleBuf(HD264Stream* sample, DDS_Boolean takeBuffer);

void HD264StreamReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long HD264StreamLoanDeserialize(HD264Stream* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct LD264Stream
{
    DDS_ULong DataLength; // @ID(0)
    DDS_Octet VedioStream[1400]; // @ID(1)
} LD264Stream; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(LD264StreamSeq, LD264Stream);

// 用户使用接口
DDS_Boolean LD264StreamInitialize(
    LD264Stream* self);

DDS_Boolean LD264StreamInitializeEx(
    LD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void LD264StreamFinalize(
    LD264Stream* self);

void LD264StreamFinalizeEx(
    LD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean LD264StreamCopy(
    LD264Stream* dst,
    const LD264Stream* src);

DDS_Boolean LD264StreamCopyEx(
    LD264Stream* dst,
    const LD264Stream* src,
    ZRMemPool* pool);

void LD264StreamPrintData(
    const LD264Stream* sample);

DDS::TypeCode* LD264StreamGetTypeCode();

// 底层使用函数
LD264Stream* LD264StreamCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void LD264StreamDestroySample(
    ZRMemPool* pool,
    LD264Stream* sample);

DDS_ULong LD264StreamGetSerializedSampleMaxSize();

DDS_ULong LD264StreamGetSerializedSampleSize(
    const LD264Stream* sample,
    DDS_ULong currentAlignment);

DDS_Long LD264StreamSerialize(
    const LD264Stream* sample,
    CDRSerializer* cdr);

DDS_Long LD264StreamDeserialize(
    LD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong LD264StreamGetSerializedKeyMaxSize();

DDS_ULong LD264StreamGetSerializedKeySize(
    const LD264Stream* sample,
    DDS_ULong currentAlignment);

DDS_Long LD264StreamSerializeKey(
    const LD264Stream* sample,
    CDRSerializer* cdr);

DDS_Long LD264StreamDeserializeKey(
    LD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long LD264StreamGetKeyHash(
    const LD264Stream* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean LD264StreamHasKey();

TypeCodeHeader* LD264StreamGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean LD264StreamNoSerializingSupported();

DDS_ULong LD264StreamFixedHeaderLength();

DDS_Long LD264StreamOnSiteDeserialize(CDRDeserializer* cdr,
    LD264Stream* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* LD264StreamLoanSampleBuf(LD264Stream* sample, DDS_Boolean takeBuffer);

void LD264StreamReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long LD264StreamLoanDeserialize(LD264Stream* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct HDDecoder
{
    DDS_Long frameId; // @ID(0)
    DDS_CharSeq stream; // @ID(1) /* maximum length = (640000) */
} HDDecoder; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(HDDecoderSeq, HDDecoder);

// 用户使用接口
DDS_Boolean HDDecoderInitialize(
    HDDecoder* self);

DDS_Boolean HDDecoderInitializeEx(
    HDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void HDDecoderFinalize(
    HDDecoder* self);

void HDDecoderFinalizeEx(
    HDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean HDDecoderCopy(
    HDDecoder* dst,
    const HDDecoder* src);

DDS_Boolean HDDecoderCopyEx(
    HDDecoder* dst,
    const HDDecoder* src,
    ZRMemPool* pool);

void HDDecoderPrintData(
    const HDDecoder* sample);

DDS::TypeCode* HDDecoderGetTypeCode();

// 底层使用函数
HDDecoder* HDDecoderCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void HDDecoderDestroySample(
    ZRMemPool* pool,
    HDDecoder* sample);

DDS_ULong HDDecoderGetSerializedSampleMaxSize();

DDS_ULong HDDecoderGetSerializedSampleSize(
    const HDDecoder* sample,
    DDS_ULong currentAlignment);

DDS_Long HDDecoderSerialize(
    const HDDecoder* sample,
    CDRSerializer* cdr);

DDS_Long HDDecoderDeserialize(
    HDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong HDDecoderGetSerializedKeyMaxSize();

DDS_ULong HDDecoderGetSerializedKeySize(
    const HDDecoder* sample,
    DDS_ULong currentAlignment);

DDS_Long HDDecoderSerializeKey(
    const HDDecoder* sample,
    CDRSerializer* cdr);

DDS_Long HDDecoderDeserializeKey(
    HDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long HDDecoderGetKeyHash(
    const HDDecoder* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean HDDecoderHasKey();

TypeCodeHeader* HDDecoderGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean HDDecoderNoSerializingSupported();

DDS_ULong HDDecoderFixedHeaderLength();

DDS_Long HDDecoderOnSiteDeserialize(CDRDeserializer* cdr,
    HDDecoder* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* HDDecoderLoanSampleBuf(HDDecoder* sample, DDS_Boolean takeBuffer);

void HDDecoderReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long HDDecoderLoanDeserialize(HDDecoder* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct LDDecoder
{
    DDS_Long frameId; // @ID(0)
    DDS_CharSeq stream; // @ID(1) /* maximum length = (640000) */
} LDDecoder; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(LDDecoderSeq, LDDecoder);

// 用户使用接口
DDS_Boolean LDDecoderInitialize(
    LDDecoder* self);

DDS_Boolean LDDecoderInitializeEx(
    LDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void LDDecoderFinalize(
    LDDecoder* self);

void LDDecoderFinalizeEx(
    LDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean LDDecoderCopy(
    LDDecoder* dst,
    const LDDecoder* src);

DDS_Boolean LDDecoderCopyEx(
    LDDecoder* dst,
    const LDDecoder* src,
    ZRMemPool* pool);

void LDDecoderPrintData(
    const LDDecoder* sample);

DDS::TypeCode* LDDecoderGetTypeCode();

// 底层使用函数
LDDecoder* LDDecoderCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void LDDecoderDestroySample(
    ZRMemPool* pool,
    LDDecoder* sample);

DDS_ULong LDDecoderGetSerializedSampleMaxSize();

DDS_ULong LDDecoderGetSerializedSampleSize(
    const LDDecoder* sample,
    DDS_ULong currentAlignment);

DDS_Long LDDecoderSerialize(
    const LDDecoder* sample,
    CDRSerializer* cdr);

DDS_Long LDDecoderDeserialize(
    LDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong LDDecoderGetSerializedKeyMaxSize();

DDS_ULong LDDecoderGetSerializedKeySize(
    const LDDecoder* sample,
    DDS_ULong currentAlignment);

DDS_Long LDDecoderSerializeKey(
    const LDDecoder* sample,
    CDRSerializer* cdr);

DDS_Long LDDecoderDeserializeKey(
    LDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long LDDecoderGetKeyHash(
    const LDDecoder* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean LDDecoderHasKey();

TypeCodeHeader* LDDecoderGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean LDDecoderNoSerializingSupported();

DDS_ULong LDDecoderFixedHeaderLength();

DDS_Long LDDecoderOnSiteDeserialize(CDRDeserializer* cdr,
    LDDecoder* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* LDDecoderLoanSampleBuf(LDDecoder* sample, DDS_Boolean takeBuffer);

void LDDecoderReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long LDDecoderLoanDeserialize(LDDecoder* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
#endif
