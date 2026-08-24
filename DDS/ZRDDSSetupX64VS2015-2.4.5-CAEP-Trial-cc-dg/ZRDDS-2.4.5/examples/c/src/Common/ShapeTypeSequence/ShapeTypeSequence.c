/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ShapeTypeSequence.h"

#define T ShapeTypeSequence
#define TSeq ShapeTypeSequenceSeq
#define TINITIALIZE ShapeTypeSequenceInitializeEx
#define TFINALIZE ShapeTypeSequenceFinalizeEx
#define TCOPY ShapeTypeSequenceCopyEx

#include "ZRSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean ShapeTypeSequenceInitializeEx(
    ShapeTypeSequence* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    DDS_LongSeq_initialize_ex(&self->x, pool, allocateMemory);

    DDS_DoubleSeq_initialize_ex(&self->y, pool, allocateMemory);

    DDS_CharSeq_initialize_ex(&self->z, pool, allocateMemory);

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void ShapeTypeSequenceFinalizeEx(
    ShapeTypeSequence* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    DDS_LongSeq_finalize(&self->x);
    DDS_DoubleSeq_finalize(&self->y);
    DDS_CharSeq_finalize(&self->z);
    if (deletePointers)
    {
    }
}

DDS_Boolean ShapeTypeSequenceCopyEx(
    ShapeTypeSequence* dst,
    const ShapeTypeSequence* src,
    ZRMemPool* pool)
{
    DDS_ULong i = 0;
    if (!DDS_LongSeq_copy(&dst->x, &src->x))
    {
        printf("copy member x failed.");
        return false;
    }

    if (!DDS_DoubleSeq_copy(&dst->y, &src->y))
    {
        printf("copy member y failed.");
        return false;
    }

    if (!DDS_CharSeq_copy(&dst->z, &src->z))
    {
        printf("copy member z failed.");
        return false;
    }

    return true;
}

void ShapeTypeSequencePrintData(const ShapeTypeSequence *sample)
{
    DDS_ULong xTmpLen;
    DDS_ULong yTmpLen;
    DDS_ULong zTmpLen;
    DDS_ULong i = 0;
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    xTmpLen = DDS_LongSeq_get_length(&sample->x);
    printf("sample->x: %d\n", xTmpLen);
    for (i = 0; i < xTmpLen; ++i)
    {
        printf("sample->x[%u]: %d\n", i, *DDS_LongSeq_get_reference(&sample->x, i)); /* 只在 VS 下有效 */
    }
    printf("\n");

    yTmpLen = DDS_DoubleSeq_get_length(&sample->y);
    printf("sample->y: %d\n", yTmpLen);
    for (i = 0; i < yTmpLen; ++i)
    {
        printf("sample->y[%u]: %lf\n", i, *DDS_DoubleSeq_get_reference(&sample->y, i)); /* 只在 VS 下有效 */
    }
    printf("\n");

    zTmpLen = DDS_CharSeq_get_length(&sample->z);
    printf("sample->z: %d\n", zTmpLen);
    for (i = 0; i < zTmpLen; ++i)
    {
        printf("sample->z[%u]: 0x%02x\n", i, *DDS_CharSeq_get_reference(&sample->z, i)); /* 只在 VS 下有效 */
    }
    printf("\n");

}

TypeCode* ShapeTypeSequenceGetTypeCode()
{
    DDS_Long ret = 0;
    TypeCode* memberTc = NULL;
    TypeCode* eleTc = NULL;
    static TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;

    s_typeCode = TypeCodeFactoryCreateStructTC(TypeCodeFactoryGetInstance(),
        "ShapeTypeSequence",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct ShapeTypeSequence typecode failed.");
        return s_typeCode;
    }
    memberTc = TypeCodeFactoryGetPrimitiveTC(TypeCodeFactoryGetInstance(), DDS_TK_INT);
    if (memberTc != NULL)
    {
        memberTc = TypeCodeFactoryCreateSequenceTC(TypeCodeFactoryGetInstance(),MAX_UINT32_VALUE, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member x TypeCode failed.");
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = TypeCodeAddMemberToStruct(
        s_typeCode,
        0,
        0,
        "x",
        memberTc,
        true,
        false);
    TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), memberTc);
    if (ret < 0)
    {
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = TypeCodeFactoryGetPrimitiveTC(TypeCodeFactoryGetInstance(), DDS_TK_DOUBLE);
    if (memberTc != NULL)
    {
        memberTc = TypeCodeFactoryCreateSequenceTC(TypeCodeFactoryGetInstance(),MAX_UINT32_VALUE, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member y TypeCode failed.");
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = TypeCodeAddMemberToStruct(
        s_typeCode,
        1,
        1,
        "y",
        memberTc,
        false,
        false);
    TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), memberTc);
    if (ret < 0)
    {
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = TypeCodeFactoryGetPrimitiveTC(TypeCodeFactoryGetInstance(), DDS_TK_CHAR);
    if (memberTc != NULL)
    {
        memberTc = TypeCodeFactoryCreateSequenceTC(TypeCodeFactoryGetInstance(),MAX_UINT32_VALUE, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member z TypeCode failed.");
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = TypeCodeAddMemberToStruct(
        s_typeCode,
        2,
        2,
        "z",
        memberTc,
        false,
        false);
    TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), memberTc);
    if (ret < 0)
    {
        TypeCodeFactoryDeleteTC(TypeCodeFactoryGetInstance(), s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    return s_typeCode;
}

DDS_Long ShapeTypeSequenceSerialize(const ShapeTypeSequence* sample, CDRSerializer *cdr)
{
    DDS_ULong i = 0;
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x._length, 4))
    {
        printf("serialize length of sample->x failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if (sample->x._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)sample->x._contiguousBuffer, sample->x._length, 4))
            {
                printf("serialize sample->x failed.");
                return -2;
            }
        }
        else
        {
            for (i = 0; i < sample->x._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_LongSeq_get_reference(&sample->x, i), 4))
                {
                    printf("serialize sample->x failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->y._length, 4))
    {
        printf("serialize length of sample->y failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if (sample->y._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)sample->y._contiguousBuffer, sample->y._length, 8))
            {
                printf("serialize sample->y failed.");
                return -2;
            }
        }
        else
        {
            for (i = 0; i < sample->y._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_DoubleSeq_get_reference(&sample->y, i), 8))
                {
                    printf("serialize sample->y failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->z._length, 4))
    {
        printf("serialize length of sample->z failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if (sample->z._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)sample->z._contiguousBuffer, sample->z._length, 1))
            {
                printf("serialize sample->z failed.");
                return -2;
            }
        }
        else
        {
            for (i = 0; i < sample->z._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->z, i), 1))
                {
                    printf("serialize sample->z failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

    return 0;
}

DDS_Long ShapeTypeSequenceDeserialize(
    ShapeTypeSequence* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    DDS_ULong xTmpLen;
    DDS_ULong yTmpLen;
    DDS_ULong zTmpLen;
    DDS_ULong i;
    //has key :last key name:x
    xTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &xTmpLen, 4))
    {
        printf("deserialize sample->x failed.");
        return -2;
    }
    if (!DDS_LongSeq_ensure_length(&sample->x, xTmpLen, xTmpLen))
    {
        printf("Set maxiumum member sample->x failed.");
        return -3;
    }
    if (sample->x._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->x._contiguousBuffer, xTmpLen, 4))
        {
            printf("deserialize sample->x failed.");
            return -2;
        }
    }
    else
    {
        for (i = 0; i < xTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_LongSeq_get_reference(&sample->x, i), 4))
            {
                printf("deserialize sample->x failed.");
                return -2;
            }
        }
    }
    //last key :x has been Deserialized
    yTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &yTmpLen, 4))
    {
        DDS_DoubleSeq_initialize_ex(&sample->y, pool, true);
        DDS_CharSeq_initialize_ex(&sample->z, pool, true);
        return 0;
    }
    if (!DDS_DoubleSeq_ensure_length(&sample->y, yTmpLen, yTmpLen))
    {
        printf("Set maxiumum member sample->y failed.");
        return -3;
    }
    if (sample->y._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->y._contiguousBuffer, yTmpLen, 8))
        {
            printf("deserialize sample->y failed.");
            return -2;
        }
    }
    else
    {
        for (i = 0; i < yTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_DoubleSeq_get_reference(&sample->y, i), 8))
            {
                printf("deserialize sample->y failed.");
                return -2;
            }
        }
    }
    zTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &zTmpLen, 4))
    {
        DDS_CharSeq_initialize_ex(&sample->z, pool, true);
        return 0;
    }
    if (!DDS_CharSeq_ensure_length(&sample->z, zTmpLen, zTmpLen))
    {
        printf("Set maxiumum member sample->z failed.");
        return -3;
    }
    if (sample->z._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->z._contiguousBuffer, zTmpLen, 1))
        {
            printf("deserialize sample->z failed.");
            return -2;
        }
    }
    else
    {
        for (i = 0; i < zTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->z, i), 1))
            {
                printf("deserialize sample->z failed.");
                return -2;
            }
        }
    }
    return 0;
}

DDS_ULong ShapeTypeSequenceGetSerializedSampleSize(const ShapeTypeSequence* sample, DDS_ULong currentAlignment)
{

    DDS_ULong i = 0;
    DDS_ULong initialAlignment = currentAlignment;
    DDS_ULong xLen;
    DDS_ULong yLen;
    DDS_ULong zLen;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    xLen = DDS_LongSeq_get_length(&sample->x);
    if (xLen != 0)
    {
        currentAlignment += 4 * xLen;
    }

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    yLen = DDS_DoubleSeq_get_length(&sample->y);
    if (yLen != 0)
    {
        currentAlignment += 4;
        currentAlignment += 8 * yLen;
    }

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    zLen = DDS_CharSeq_get_length(&sample->z);
    if (zLen != 0)
    {
        currentAlignment += 1 * zLen;
    }

    return currentAlignment - initialAlignment;
}

DDS_Long ShapeTypeSequenceSerializeKey(const ShapeTypeSequence* sample, CDRSerializer *cdr)
{
    DDS_ULong i = 0;

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x._length, 4))
    {
        printf("serialize length of sample->x failed.");
        return -2;
    }
    for (i = 0; i < sample->x._length; ++i)
    {
        if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_LongSeq_get_reference(&sample->x, i), 4))
        {
            printf("serialize sample->x failed.");
            return -2;
        }
    }

    return 0;
}

DDS_Long ShapeTypeSequenceDeserializeKey(
    ShapeTypeSequence* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    DDS_ULong xTmpLen = 0;
    DDS_ULong i = 0;
    xTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &xTmpLen, 4))
    {
        printf("deserialize length of sample->x failed.");
        return -2;
    }
    if (!DDS_LongSeq_ensure_length(&sample->x, xTmpLen, xTmpLen))
    {
        printf("Set maxiumum member sample->x failed.");
        return -4;
    }
    for ( i = 0; i < xTmpLen; ++i)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_LongSeq_get_reference(&sample->x, i), 4))
        {
            printf("deserialize sample->x failed.");
            return -2;
        }
    }

    return 0;
}

DDS_ULong ShapeTypeSequenceGetSerializedKeySize(const ShapeTypeSequence* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    DDS_ULong xLen;

    DDS_ULong i = 0;
    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    xLen = DDS_LongSeq_get_length(&sample->x);
    currentAlignment += 4 * xLen;

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ShapeTypeSequenceLoanSampleBuf(ShapeTypeSequence* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void ShapeTypeSequenceReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long ShapeTypeSequenceLoanDeserialize(ShapeTypeSequence* sampleBuf,
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
DDS_Long ShapeTypeSequenceOnSiteDeserialize(CDRDeserializer* cdr,
    ShapeTypeSequence* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean ShapeTypeSequenceNoSerializingSupported()
{
    return false;
}

DDS_ULong ShapeTypeSequenceFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
DDS_Boolean ShapeTypeSequenceInitialize(ShapeTypeSequence* self)
{
    return ShapeTypeSequenceInitializeEx(self, NULL, true);
}

void ShapeTypeSequenceFinalize(ShapeTypeSequence* self)
{
    ShapeTypeSequenceFinalizeEx(self, NULL, true);
}

DDS_Boolean ShapeTypeSequenceCopy(
    ShapeTypeSequence* dst,
    const ShapeTypeSequence* src)
{
    return ShapeTypeSequenceCopyEx(dst, src, NULL);
}

void ShapeTypeSequenceDestroySample(ZRMemPool* pool, ShapeTypeSequence* sample)
{
    if (sample == NULL) return;
    ShapeTypeSequenceFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

ShapeTypeSequence* ShapeTypeSequenceCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    ShapeTypeSequence* newSample = (ShapeTypeSequence*)ZRMalloc(pool, sizeof(ShapeTypeSequence));
    if (newSample == NULL)
    {
        printf("malloc for ShapeTypeSequence failed.");
        return NULL;
    }
    if (!ShapeTypeSequenceInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        ShapeTypeSequenceDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

DDS_ULong ShapeTypeSequenceGetSerializedSampleMaxSize()
{
    return MAX_UINT32_VALUE;
}

DDS_ULong ShapeTypeSequenceGetSerializedKeyMaxSize()
{
    return MAX_UINT32_VALUE;
}

DDS_Long ShapeTypeSequenceGetKeyHash(
    const ShapeTypeSequence* sample,
    CDRSerializer* cdr,
    DDS_KeyHash_t* result)
{
    DDS_Long ret = ShapeTypeSequenceSerializeKey(sample, cdr);
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

DDS_Boolean ShapeTypeSequenceHasKey()
{
    return true;
}

TypeCodeHeader* ShapeTypeSequenceGetInnerTypeCode()
{
    return ShapeTypeSequenceGetTypeCode();
}

