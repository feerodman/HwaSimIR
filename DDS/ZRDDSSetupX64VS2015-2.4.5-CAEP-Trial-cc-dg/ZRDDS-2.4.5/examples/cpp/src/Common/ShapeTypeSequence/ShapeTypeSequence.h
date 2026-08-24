#ifndef ShapeTypeSequence_H_
#define ShapeTypeSequence_H_

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

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ShapeTypeSequence
{
    DDS_LongSeq x; // @ID(0) @Key /* unbounded sequence */ 
    DDS_DoubleSeq y; // @ID(1) /* unbounded sequence */ 
    DDS_CharSeq z; // @ID(2) /* unbounded sequence */ 
} ShapeTypeSequence; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(ShapeTypeSequenceSeq, ShapeTypeSequence);

// 用户使用接口
DDS_Boolean ShapeTypeSequenceInitialize(
    ShapeTypeSequence* self);

DDS_Boolean ShapeTypeSequenceInitializeEx(
    ShapeTypeSequence* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void ShapeTypeSequenceFinalize(
    ShapeTypeSequence* self);

void ShapeTypeSequenceFinalizeEx(
    ShapeTypeSequence* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean ShapeTypeSequenceCopy(
    ShapeTypeSequence* dst,
    const ShapeTypeSequence* src);

DDS_Boolean ShapeTypeSequenceCopyEx(
    ShapeTypeSequence* dst,
    const ShapeTypeSequence* src,
    ZRMemPool* pool);

void ShapeTypeSequencePrintData(
    const ShapeTypeSequence* sample);

DDS::TypeCode* ShapeTypeSequenceGetTypeCode();

// 底层使用函数
ShapeTypeSequence* ShapeTypeSequenceCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void ShapeTypeSequenceDestroySample(
    ZRMemPool* pool,
    ShapeTypeSequence* sample);

DDS_ULong ShapeTypeSequenceGetSerializedSampleMaxSize();

DDS_ULong ShapeTypeSequenceGetSerializedSampleSize(
    const ShapeTypeSequence* sample,
    DDS_ULong currentAlignment);

DDS_Long ShapeTypeSequenceSerialize(
    const ShapeTypeSequence* sample,
    CDRSerializer* cdr);

DDS_Long ShapeTypeSequenceDeserialize(
    ShapeTypeSequence* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong ShapeTypeSequenceGetSerializedKeyMaxSize();

DDS_ULong ShapeTypeSequenceGetSerializedKeySize(
    const ShapeTypeSequence* sample,
    DDS_ULong currentAlignment);

DDS_Long ShapeTypeSequenceSerializeKey(
    const ShapeTypeSequence* sample,
    CDRSerializer* cdr);

DDS_Long ShapeTypeSequenceDeserializeKey(
    ShapeTypeSequence* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long ShapeTypeSequenceGetKeyHash(
    const ShapeTypeSequence* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean ShapeTypeSequenceHasKey();

TypeCodeHeader* ShapeTypeSequenceGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean ShapeTypeSequenceNoSerializingSupported();

DDS_ULong ShapeTypeSequenceFixedHeaderLength();

DDS_Long ShapeTypeSequenceOnSiteDeserialize(CDRDeserializer* cdr,
    ShapeTypeSequence* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ShapeTypeSequenceLoanSampleBuf(ShapeTypeSequence* sample, DDS_Boolean takeBuffer);

void ShapeTypeSequenceReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long ShapeTypeSequenceLoanDeserialize(ShapeTypeSequence* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
#ifdef __cplusplus
}
#endif
#endif
