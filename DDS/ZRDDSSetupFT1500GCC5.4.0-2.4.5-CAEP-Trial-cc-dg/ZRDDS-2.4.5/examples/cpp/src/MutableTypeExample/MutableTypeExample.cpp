/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MutableTypeExample.h"

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

#ifdef _ZRDDS_INCLUDE_SPARE_TYPE
DDS_Boolean OrignalTypeCompare(
    OrignalType* dst,
    const OrignalType* src)
{
    if(memcmp(&dst->x, &src->x, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->y, &src->y, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    return true;
}
#endif //_ZRDDS_INCLUDE_SPARE_TYPE
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
    return 20;
}

DDS_ULong OrignalTypeGetSerializedKeyMaxSize()
{
    return 20;
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
    ret = CDRSerializeGetKeyHash(cdr, result->value, true);
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
        DDS_MUTABLE_EXTENSIBILITY);
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

DDS_Long OrignalTypeSerializeIgnoreListEnd(const OrignalType* sample, CDRSerializer *cdr)
{
    DDS_Boolean useExtend = false;
    DDS_ULong beforeLen = 0, afterLen = 0;
    DDS_Char* lenAddr = NULL;
    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        0);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("serialize sample->x failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        1);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        printf("serialize sample->y failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    return 0;
}

DDS_Long OrignalTypeSerialize(const OrignalType* sample, CDRSerializer *cdr)
{
    if (OrignalTypeSerializeIgnoreListEnd(sample, cdr) < 0)
    {
        printf("serialize sample ignore list end failed.");
        return -1;
    }
    if (ParameterHeaderSerialize(cdr, false, true, PID_LIST_END) == NULL)
    {
        printf("serialize parameter list end failed.");
        return -2;
    }
    return 0;
}

DDS_Long OrignalTypeDeserialize(
    OrignalType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    unsigned long combineValue = 0;
    DDS_ULong memberId = 0;
    DDS_ULong length = 0;
    DDS_Boolean useExtend = false;
    DDS_Boolean mustUnderstand = false;
    DDS_Long readLen = 0;
    DDS_ULong seqLen = 0;
    while (memberId != PID_LIST_END)
    {
        if (ParameterHeaderDeserialize(cdr, &memberId, &length, &useExtend, &mustUnderstand) < 0)
        {
            printf("deserialize parameter header failed.");
            return -2;
        }
        readLen = CDRDeserializerGetBufferLength(cdr);
        switch(memberId)
        {
        case 0:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
            {
                printf("deserialize sample->x failed.");
                return -2;
            }
            break;
        }

        case 1:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->y, 4))
            {
                printf("deserialize sample->y failed.");
                return -2;
            }
            break;
        }

        case PID_LIST_END:
            break;
        default:
        {
            if (mustUnderstand)
            {
                printf("do not understand must understad member");
                return -3;
            }
            break;
        }
        }
        if (!CDRDeserializerAheadLength(cdr, readLen, length))
        {
            printf("move to next header failed(%d, %d).", readLen, length);
            return -4;
        }
    }
    return 0;
}

DDS_ULong OrignalTypeGetSerializedSampleSizeIgnoreListEnd(const OrignalType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;
    DDS_ULong tmpAlignment;

    currentAlignment += (currentAlignment & 1) == 0 ? 0 : (2 - (currentAlignment & 1));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    return currentAlignment - initialAlignment;
}

DDS_ULong OrignalTypeGetSerializedSampleSize(const OrignalType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += OrignalTypeGetSerializedSampleSizeIgnoreListEnd(sample, currentAlignment);
    currentAlignment += 4;
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

#ifdef _ZRDDS_INCLUDE_SPARE_TYPE
DDS_Boolean NewTypeCompare(
    NewType* dst,
    const NewType* src)
{
    if(memcmp(&dst->z, &src->z, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->x, &src->x, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->angle, &src->angle, sizeof(DDS_Float)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->y, &src->y, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    return true;
}
#endif //_ZRDDS_INCLUDE_SPARE_TYPE
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
    return 36;
}

DDS_ULong NewTypeGetSerializedKeyMaxSize()
{
    return 36;
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
    ret = CDRSerializeGetKeyHash(cdr, result->value, true);
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
    self->z = 0;

    self->x = 0;

    self->angle = 0;

    self->y = 0;

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
    dst->z = src->z;
    dst->x = src->x;
    dst->angle = src->angle;
    dst->y = src->y;
    return true;
}

void NewTypePrintData(const NewType *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->z: %d\n", sample->z); // 只在 VS 下有效
    printf("\n");

    printf("sample->x: %d\n", sample->x); // 只在 VS 下有效
    printf("\n");

    printf("sample->angle: %f\n", sample->angle); // 只在 VS 下有效
    printf("\n");

    printf("sample->y: %d\n", sample->y); // 只在 VS 下有效
    printf("\n");

}

DDS::TypeCode* NewTypeGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "NewType",
        DDS_MUTABLE_EXTENSIBILITY);
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
        printf("Get Member z TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        2,
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

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member x TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
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

    memberTc = factory.getPrimitiveTC(DDS_TK_FLOAT);
    if (memberTc == NULL)
    {
        printf("Get Member angle TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        3,
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

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member y TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
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

DDS_Long NewTypeSerializeIgnoreListEnd(const NewType* sample, CDRSerializer *cdr)
{
    DDS_Boolean useExtend = false;
    DDS_ULong beforeLen = 0, afterLen = 0;
    DDS_Char* lenAddr = NULL;
    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        2);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->z, 4))
    {
        printf("serialize sample->z failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        0);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("serialize sample->x failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        3);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->angle, 4))
    {
        printf("serialize sample->angle failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    useExtend = false;
    lenAddr = ParameterHeaderSerialize(
        cdr,
        useExtend,
        true,
        1);
    if (lenAddr == NULL)
    {
        printf("serialize parameter header failed.");
        return -2;
    }
    beforeLen = CDRSerializerGetBufferSpace(cdr);
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        printf("serialize sample->y failed.");
        return -2;
    }
    afterLen = CDRSerializerGetBufferSpace(cdr);
    ParameterLenSerialize(cdr, lenAddr, beforeLen - afterLen, useExtend ? 4 : 2);

    return 0;
}

DDS_Long NewTypeSerialize(const NewType* sample, CDRSerializer *cdr)
{
    if (NewTypeSerializeIgnoreListEnd(sample, cdr) < 0)
    {
        printf("serialize sample ignore list end failed.");
        return -1;
    }
    if (ParameterHeaderSerialize(cdr, false, true, PID_LIST_END) == NULL)
    {
        printf("serialize parameter list end failed.");
        return -2;
    }
    return 0;
}

DDS_Long NewTypeDeserialize(
    NewType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    unsigned long combineValue = 0;
    DDS_ULong memberId = 0;
    DDS_ULong length = 0;
    DDS_Boolean useExtend = false;
    DDS_Boolean mustUnderstand = false;
    DDS_Long readLen = 0;
    DDS_ULong seqLen = 0;
    while (memberId != PID_LIST_END)
    {
        if (ParameterHeaderDeserialize(cdr, &memberId, &length, &useExtend, &mustUnderstand) < 0)
        {
            printf("deserialize parameter header failed.");
            return -2;
        }
        readLen = CDRDeserializerGetBufferLength(cdr);
        switch(memberId)
        {
        case 2:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->z, 4))
            {
                printf("deserialize sample->z failed.");
                return -2;
            }
            break;
        }

        case 0:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
            {
                printf("deserialize sample->x failed.");
                return -2;
            }
            break;
        }

        case 3:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->angle, 4))
            {
                printf("deserialize sample->angle failed.");
                return -2;
            }
            break;
        }

        case 1:
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->y, 4))
            {
                printf("deserialize sample->y failed.");
                return -2;
            }
            break;
        }

        case PID_LIST_END:
            break;
        default:
        {
            if (mustUnderstand)
            {
                printf("do not understand must understad member");
                return -3;
            }
            break;
        }
        }
        if (!CDRDeserializerAheadLength(cdr, readLen, length))
        {
            printf("move to next header failed(%d, %d).", readLen, length);
            return -4;
        }
    }
    return 0;
}

DDS_ULong NewTypeGetSerializedSampleSizeIgnoreListEnd(const NewType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;
    DDS_ULong tmpAlignment;

    currentAlignment += (currentAlignment & 1) == 0 ? 0 : (2 - (currentAlignment & 1));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    currentAlignment += 4;
    tmpAlignment = currentAlignment;
    currentAlignment = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    currentAlignment += tmpAlignment;
    currentAlignment += (currentAlignment & 3) == 0 ? 0 : (4 - (currentAlignment & 3));

    return currentAlignment - initialAlignment;
}

DDS_ULong NewTypeGetSerializedSampleSize(const NewType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += NewTypeGetSerializedSampleSizeIgnoreListEnd(sample, currentAlignment);
    currentAlignment += 4;
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
