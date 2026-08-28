#ifndef ShapeType_H_
#define ShapeType_H_

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

typedef struct ShapeType
{
    DDS_Long x; // @ID(0) @Key
    DDS_Long y; // @ID(1)
    DDS_Char* z; // @ID(2) /* maximum length = (255) */
} ShapeType; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(ShapeTypeSeq, ShapeType);

// 用户使用接口
DDS_Boolean ShapeTypeInitialize(
    ShapeType* self);

DDS_Boolean ShapeTypeInitializeEx(
    ShapeType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void ShapeTypeFinalize(
    ShapeType* self);

void ShapeTypeFinalizeEx(
    ShapeType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean ShapeTypeCopy(
    ShapeType* dst,
    const ShapeType* src);

DDS_Boolean ShapeTypeCopyEx(
    ShapeType* dst,
    const ShapeType* src,
    ZRMemPool* pool);

void ShapeTypePrintData(
    const ShapeType* sample);

DDS::TypeCode* ShapeTypeGetTypeCode();

// 底层使用函数
ShapeType* ShapeTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void ShapeTypeDestroySample(
    ZRMemPool* pool,
    ShapeType* sample);

DDS_ULong ShapeTypeGetSerializedSampleMaxSize();

DDS_ULong ShapeTypeGetSerializedSampleSize(
    const ShapeType* sample,
    DDS_ULong currentAlignment);

DDS_Long ShapeTypeSerialize(
    const ShapeType* sample,
    CDRSerializer* cdr);

DDS_Long ShapeTypeDeserialize(
    ShapeType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong ShapeTypeGetSerializedKeyMaxSize();

DDS_ULong ShapeTypeGetSerializedKeySize(
    const ShapeType* sample,
    DDS_ULong currentAlignment);

DDS_Long ShapeTypeSerializeKey(
    const ShapeType* sample,
    CDRSerializer* cdr);

DDS_Long ShapeTypeDeserializeKey(
    ShapeType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long ShapeTypeGetKeyHash(
    const ShapeType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean ShapeTypeHasKey();

TypeCodeHeader* ShapeTypeGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean ShapeTypeNoSerializingSupported();

DDS_ULong ShapeTypeFixedHeaderLength();

DDS_Long ShapeTypeOnSiteDeserialize(CDRDeserializer* cdr,
    ShapeType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ShapeTypeLoanSampleBuf(ShapeType* sample, DDS_Boolean takeBuffer);

void ShapeTypeReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long ShapeTypeLoanDeserialize(ShapeType* sampleBuf,
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
