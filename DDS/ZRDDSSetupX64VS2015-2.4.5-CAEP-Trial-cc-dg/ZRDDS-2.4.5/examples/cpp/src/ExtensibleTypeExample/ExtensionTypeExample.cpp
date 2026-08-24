/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ExtensionTypeExample.h"

#define T OrignalType
#define TSeq OrignalTypeSeq
#define TINITIALIZE OrignalTypeInitializeEx
#define TFINALIZE OrignalTypeFinalizeEx
#define TCOPY OrignalTypeCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean OrignalTypeInitialize(OrignalType* self)
{
    return OrignalTypeInitializeEx(self, NULL, true);
}

void OrignalTypeFinalize(OrignalType* self)
{
    OrignalTypeFinalizeEx(self, NULL, true);
}

DDS_Boolean OrignalTypeCopy(
    OrignalType* dst,
    const OrignalType* src)
{
    return OrignalTypeCopyEx(dst, src, NULL);
}

OrignalType* OrignalTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    OrignalType* newSample = (OrignalType*)ZRMalloc(pool, sizeof(OrignalType));
    if (newSample == NULL)
    {
        printf("malloc for OrignalType failed.");
        return NULL;
    }
    if (!OrignalTypeInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        OrignalTypeDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void OrignalTypeDestroySample(ZRMemPool* pool, OrignalType* sample)
{
    if (sample == NULL) return;
    OrignalTypeFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong OrignalTypeGetSerializedSampleMaxSize()
{
    return 8;
}

DDS_ULong OrignalTypeGetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long OrignalTypeGetKeyHash(
    const OrignalType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = OrignalTypeSerializeKey(sample, cdr);
    if (ret < 0)
    {
        printf("serialize key failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    ret = CDRSerializeGetKeyHash(cdr, result->value, false);
    if (ret < 0)
    {
        printf("get keyhash failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    result->valid = true;
    return 0;
}

DDS_Boolean OrignalTypeHasKey()
{
    return false;
}

TypeCodeHeader* OrignalTypeGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = OrignalTypeGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean OrignalTypeInitializeEx(
    OrignalType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->x = 0;

    self->y = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void OrignalTypeFinalizeEx(
    OrignalType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean OrignalTypeCopyEx(
    OrignalType* dst,
    const OrignalType* src,
    ZRMemPool* pool)
{
    dst->x = src->x;
    dst->y = src->y;
    return true;
}

void OrignalTypePrintData(const OrignalType *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->x: %d\n", sample->x); // 只在 VS 下有效
    printf("\n");

    printf("sample->y: %d\n", sample->y); // 只在 VS 下有效
    printf("\n");

}

DDS::TypeCode* OrignalTypeGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "OrignalType",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct OrignalType typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member x TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "x",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member y TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "y",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    return s_typeCode;
}

DDS_Long OrignalTypeSerialize(const OrignalType* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("serialize sample->x failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        printf("serialize sample->y failed.");
        return -2;
    }

    return 0;
}

DDS_Long OrignalTypeDeserialize(
    OrignalType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        sample->x = 0;
        sample->y = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        sample->y = 0;
        return 0;
    }
    return 0;
}

DDS_ULong OrignalTypeGetSerializedSampleSize(const OrignalType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long OrignalTypeSerializeKey(const OrignalType* sample, CDRSerializer *cdr)
{
    if (OrignalTypeSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long OrignalTypeDeserializeKey(
    OrignalType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (OrignalTypeDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong OrignalTypeGetSerializedKeySize(const OrignalType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += OrignalTypeGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* OrignalTypeLoanSampleBuf(OrignalType* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void OrignalTypeReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long OrignalTypeLoanDeserialize(OrignalType* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long OrignalTypeOnSiteDeserialize(CDRDeserializer* cdr,
    OrignalType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean OrignalTypeNoSerializingSupported()
{
    return false;
}

DDS_ULong OrignalTypeFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T NewType
#define TSeq NewTypeSeq
#define TINITIALIZE NewTypeInitializeEx
#define TFINALIZE NewTypeFinalizeEx
#define TCOPY NewTypeCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean NewTypeInitialize(NewType* self)
{
    return NewTypeInitializeEx(self, NULL, true);
}

void NewTypeFinalize(NewType* self)
{
    NewTypeFinalizeEx(self, NULL, true);
}

DDS_Boolean NewTypeCopy(
    NewType* dst,
    const NewType* src)
{
    return NewTypeCopyEx(dst, src, NULL);
}

NewType* NewTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    NewType* newSample = (NewType*)ZRMalloc(pool, sizeof(NewType));
    if (newSample == NULL)
    {
        printf("malloc for NewType failed.");
        return NULL;
    }
    if (!NewTypeInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        NewTypeDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void NewTypeDestroySample(ZRMemPool* pool, NewType* sample)
{
    if (sample == NULL) return;
    NewTypeFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong NewTypeGetSerializedSampleMaxSize()
{
    return 16;
}

DDS_ULong NewTypeGetSerializedKeyMaxSize()
{
    return 16;
}

DDS_Long NewTypeGetKeyHash(
    const NewType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = NewTypeSerializeKey(sample, cdr);
    if (ret < 0)
    {
        printf("serialize key failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    ret = CDRSerializeGetKeyHash(cdr, result->value, false);
    if (ret < 0)
    {
        printf("get keyhash failed.");
        *result = DDS_HANDLE_NIL_NATIVE;
        return -1;
    }
    result->valid = true;
    return 0;
}

DDS_Boolean NewTypeHasKey()
{
    return false;
}

TypeCodeHeader* NewTypeGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = NewTypeGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean NewTypeInitializeEx(
    NewType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->x = 0;

    self->y = 0;

    self->z = 0;

    self->angle = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void NewTypeFinalizeEx(
    NewType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean NewTypeCopyEx(
    NewType* dst,
    const NewType* src,
    ZRMemPool* pool)
{
    dst->x = src->x;
    dst->y = src->y;
    dst->z = src->z;
    dst->angle = src->angle;
    return true;
}

void NewTypePrintData(const NewType *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->x: %d\n", sample->x); // 只在 VS 下有效
    printf("\n");

    printf("sample->y: %d\n", sample->y); // 只在 VS 下有效
    printf("\n");

    printf("sample->z: %d\n", sample->z); // 只在 VS 下有效
    printf("\n");

    printf("sample->angle: %f\n", sample->angle); // 只在 VS 下有效
    printf("\n");

}

DDS::TypeCode* NewTypeGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "NewType",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct NewType typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member x TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        2,
        "x",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member y TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        3,
        "y",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member z TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        4,
        "z",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_FLOAT);
    if (memberTc == NULL)
    {
        printf("Get Member angle TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        5,
        "angle",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    return s_typeCode;
}

DDS_Long NewTypeSerialize(const NewType* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("serialize sample->x failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        printf("serialize sample->y failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->z, 4))
    {
        printf("serialize sample->z failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->angle, 4))
    {
        printf("serialize sample->angle failed.");
        return -2;
    }

    return 0;
}

DDS_Long NewTypeDeserialize(
    NewType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        sample->x = 0;
        sample->y = 0;
        sample->z = 0;
        sample->angle = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        sample->y = 0;
        sample->z = 0;
        sample->angle = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->z, 4))
    {
        sample->z = 0;
        sample->angle = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->angle, 4))
    {
        sample->angle = 0;
        return 0;
    }
    return 0;
}

DDS_ULong NewTypeGetSerializedSampleSize(const NewType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long NewTypeSerializeKey(const NewType* sample, CDRSerializer *cdr)
{
    if (NewTypeSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long NewTypeDeserializeKey(
    NewType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (NewTypeDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong NewTypeGetSerializedKeySize(const NewType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += NewTypeGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* NewTypeLoanSampleBuf(NewType* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void NewTypeReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long NewTypeLoanDeserialize(NewType* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long NewTypeOnSiteDeserialize(CDRDeserializer* cdr,
    NewType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean NewTypeNoSerializingSupported()
{
    return false;
}

DDS_ULong NewTypeFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
