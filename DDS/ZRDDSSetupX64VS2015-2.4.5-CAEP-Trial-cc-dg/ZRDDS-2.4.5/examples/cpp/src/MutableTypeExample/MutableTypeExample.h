#ifndef MutableTypeExample_H_
#define MutableTypeExample_H_

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

typedef struct OrignalType
{
    DDS_Long x; // @ID(0)
    DDS_Long y; // @ID(1)
} OrignalType; // @Extensibility(MUTABLE)

DDS_USER_SEQUENCE_CPP(OrignalTypeSeq, OrignalType);

// 用户使用接口
DDS_Boolean OrignalTypeInitialize(
    OrignalType* self);

DDS_Boolean OrignalTypeInitializeEx(
    OrignalType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void OrignalTypeFinalize(
    OrignalType* self);

void OrignalTypeFinalizeEx(
    OrignalType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean OrignalTypeCopy(
    OrignalType* dst,
    const OrignalType* src);

DDS_Boolean OrignalTypeCopyEx(
    OrignalType* dst,
    const OrignalType* src,
    ZRMemPool* pool);

void OrignalTypePrintData(
    const OrignalType* sample);

DDS::TypeCode* OrignalTypeGetTypeCode();

// 底层使用函数
OrignalType* OrignalTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void OrignalTypeDestroySample(
    ZRMemPool* pool,
    OrignalType* sample);

DDS_ULong OrignalTypeGetSerializedSampleMaxSize();

DDS_ULong OrignalTypeGetSerializedSampleSize(
    const OrignalType* sample,
    DDS_ULong currentAlignment);

DDS_Long OrignalTypeSerialize(
    const OrignalType* sample,
    CDRSerializer* cdr);

DDS_Long OrignalTypeDeserialize(
    OrignalType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong OrignalTypeGetSerializedKeyMaxSize();

DDS_ULong OrignalTypeGetSerializedKeySize(
    const OrignalType* sample,
    DDS_ULong currentAlignment);

DDS_Long OrignalTypeSerializeKey(
    const OrignalType* sample,
    CDRSerializer* cdr);

DDS_Long OrignalTypeDeserializeKey(
    OrignalType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long OrignalTypeGetKeyHash(
    const OrignalType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean OrignalTypeHasKey();

TypeCodeHeader* OrignalTypeGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean OrignalTypeNoSerializingSupported();

DDS_ULong OrignalTypeFixedHeaderLength();

DDS_Long OrignalTypeOnSiteDeserialize(CDRDeserializer* cdr,
    OrignalType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* OrignalTypeLoanSampleBuf(OrignalType* sample, DDS_Boolean takeBuffer);

void OrignalTypeReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long OrignalTypeLoanDeserialize(OrignalType* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
DDS_Long OrignalTypeSerializeIgnoreListEnd(const OrignalType* sample, CDRSerializer *cdr);

DDS_ULong OrignalTypeGetSerializedSampleSizeIgnoreListEnd(const OrignalType* sample, DDS_ULong currentAlignment);

typedef struct NewType
{
    DDS_Long z; // @ID(2)
    DDS_Long x; // @ID(0)
    DDS_Float angle; // @ID(3)
    DDS_Long y; // @ID(1)
} NewType; // @Extensibility(MUTABLE)

DDS_USER_SEQUENCE_CPP(NewTypeSeq, NewType);

// 用户使用接口
DDS_Boolean NewTypeInitialize(
    NewType* self);

DDS_Boolean NewTypeInitializeEx(
    NewType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void NewTypeFinalize(
    NewType* self);

void NewTypeFinalizeEx(
    NewType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean NewTypeCopy(
    NewType* dst,
    const NewType* src);

DDS_Boolean NewTypeCopyEx(
    NewType* dst,
    const NewType* src,
    ZRMemPool* pool);

void NewTypePrintData(
    const NewType* sample);

DDS::TypeCode* NewTypeGetTypeCode();

// 底层使用函数
NewType* NewTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void NewTypeDestroySample(
    ZRMemPool* pool,
    NewType* sample);

DDS_ULong NewTypeGetSerializedSampleMaxSize();

DDS_ULong NewTypeGetSerializedSampleSize(
    const NewType* sample,
    DDS_ULong currentAlignment);

DDS_Long NewTypeSerialize(
    const NewType* sample,
    CDRSerializer* cdr);

DDS_Long NewTypeDeserialize(
    NewType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong NewTypeGetSerializedKeyMaxSize();

DDS_ULong NewTypeGetSerializedKeySize(
    const NewType* sample,
    DDS_ULong currentAlignment);

DDS_Long NewTypeSerializeKey(
    const NewType* sample,
    CDRSerializer* cdr);

DDS_Long NewTypeDeserializeKey(
    NewType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long NewTypeGetKeyHash(
    const NewType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean NewTypeHasKey();

TypeCodeHeader* NewTypeGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean NewTypeNoSerializingSupported();

DDS_ULong NewTypeFixedHeaderLength();

DDS_Long NewTypeOnSiteDeserialize(CDRDeserializer* cdr,
    NewType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* NewTypeLoanSampleBuf(NewType* sample, DDS_Boolean takeBuffer);

void NewTypeReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long NewTypeLoanDeserialize(NewType* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
DDS_Long NewTypeSerializeIgnoreListEnd(const NewType* sample, CDRSerializer *cdr);

DDS_ULong NewTypeGetSerializedSampleSizeIgnoreListEnd(const NewType* sample, DDS_ULong currentAlignment);

#ifdef __cplusplus
}
#endif
#endif
