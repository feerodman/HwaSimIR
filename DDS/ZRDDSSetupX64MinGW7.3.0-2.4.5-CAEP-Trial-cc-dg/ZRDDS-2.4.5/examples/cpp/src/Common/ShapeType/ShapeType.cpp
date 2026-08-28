/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ShapeType.h"

#define T ShapeType
#define TSeq ShapeTypeSeq
#define TINITIALIZE ShapeTypeInitializeEx
#define TFINALIZE ShapeTypeFinalizeEx
#define TCOPY ShapeTypeCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean ShapeTypeInitialize(ShapeType* self)
{
    return ShapeTypeInitializeEx(self, NULL, true);
}

void ShapeTypeFinalize(ShapeType* self)
{
    ShapeTypeFinalizeEx(self, NULL, true);
}

DDS_Boolean ShapeTypeCompare(
    ShapeType* dst,
    const ShapeType* src)
{
    if(memcmp(&dst->x, &src->x, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->y, &src->y, sizeof(DDS_Long)) != 0)
    {
        return false;
    }
    if(memcmp(&dst->z, &src->z, sizeof(DDS_Char*)) != 0)
    {
        return false;
    }
    return true;
}
DDS_Boolean ShapeTypeCopy(
    ShapeType* dst,
    const ShapeType* src)
{
    return ShapeTypeCopyEx(dst, src, NULL);
}

ShapeType* ShapeTypeCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    ShapeType* newSample = (ShapeType*)ZRMalloc(pool, sizeof(ShapeType));
    if (newSample == NULL)
    {
        printf("malloc for ShapeType failed.");
        return NULL;
    }
    if (!ShapeTypeInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        ShapeTypeDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void ShapeTypeDestroySample(ZRMemPool* pool, ShapeType* sample)
{
    if (sample == NULL) return;
    ShapeTypeFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong ShapeTypeGetSerializedSampleMaxSize()
{
    return 268;
}

DDS_ULong ShapeTypeGetSerializedKeyMaxSize()
{
    return 4;
}

DDS_Long ShapeTypeGetKeyHash(
    const ShapeType* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = ShapeTypeSerializeKey(sample, cdr);
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

DDS_Boolean ShapeTypeHasKey()
{
    return true;
}

TypeCodeHeader* ShapeTypeGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = ShapeTypeGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean ShapeTypeInitializeEx(
    ShapeType* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->x = 0;

    self->y = 0;

    self->z = NULL;

    if (allocateMemory)
    {
        self->z = (DDS_Char*) ZRMalloc(pool, 255 + 1);
        if (self->z == NULL)
        {
            printf("Malloc for self->z failed.");
            return false;
        }
        self->z[0] = '\0';
    }
    else
    {
        if (self->z != NULL)
        {
            self->z[0] = '\0';
        }
    }
    return true;
}

void ShapeTypeFinalizeEx(
    ShapeType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
        ZRDealloc(pool, self->z);
        self->z = NULL;
    }
}

DDS_Boolean ShapeTypeCopyEx(
    ShapeType* dst,
    const ShapeType* src,
    ZRMemPool* pool)
{
    dst->x = src->x;
    dst->y = src->y;
    if (src->z == NULL)
    {
        ZRDealloc(pool, dst->z);
        dst->z = NULL;
    }
    else
    {
        if (dst->z == NULL)
        {
            dst->z = (DDS_Char*) ZRMalloc(pool, 255 + 1);
            if (dst->z == NULL)
            {
                printf("malloc for z failed.");
                return false;
            }
        }
        strcpy(dst->z, src->z);
    }
    return true;
}

void ShapeTypePrintData(const ShapeType *sample)
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

    if (sample->z != NULL)
    {
        printf("sample->z(%d): %s\n", strlen(sample->z), sample->z);
    }
    else
    {
        printf("sample->z(0): NULL\n");
    }
    printf("\n");

}

DDS::TypeCode* ShapeTypeGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "ShapeType",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct ShapeType typecode failed.");
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
        true,
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

    memberTc = factory.createStringTC(255);
    if (memberTc == NULL)
    {
        printf("Get Member z TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "z",
        memberTc,
        false,
        false);
    factory.deleteTC(memberTc);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    return s_typeCode;
}

DDS_Long ShapeTypeSerialize(const ShapeType* sample, CDRSerializer *cdr)
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

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->z, sample->z == NULL ? 0 : strlen(sample->z) + 1))
    {
        printf("serialize sample->z failed.");
        return -2;
    }

    return 0;
}

DDS_Long ShapeTypeDeserialize(
    ShapeType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:x
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("deserialize sample->x failed.");
        return -2;
    }
    //last key :x has been deserialized
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->y, 4))
    {
        sample->y = 0;
        sample->z = NULL;
        return 0;
    }
    DDS_ULong zTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &zTmpLen, 4))
    {
        sample->z = NULL;
        return 0;
    }
    if (0 == zTmpLen)
    {
        ZRDealloc(pool, sample->z);
        sample->z = NULL;
    }
    else
    {
        if (sample->z == NULL)
        {
            sample->z = (DDS_Char*) ZRMalloc(pool, zTmpLen);
            if (sample->z == NULL)
            {
                printf("malloc for sample->z failed(%d).", zTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->z, zTmpLen, 1))
        {
            printf("deserialize member sample->z failed.");
            return -4;
        }
    }
    return 0;
}

DDS_ULong ShapeTypeGetSerializedSampleSize(const ShapeType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->z == NULL ? 0 : strlen(sample->z) + 1, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long ShapeTypeSerializeKey(const ShapeType* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("serialize sample->x failed.");
        return -2;
    }

    return 0;
}

DDS_Long ShapeTypeDeserializeKey(
    ShapeType* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
    {
        printf("deserialize sample->x failed.");
        return -2;
    }

    return 0;
}

DDS_ULong ShapeTypeGetSerializedKeySize(const ShapeType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ShapeTypeLoanSampleBuf(ShapeType* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void ShapeTypeReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long ShapeTypeLoanDeserialize(ShapeType* sampleBuf,
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

#endif/_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long ShapeTypeOnSiteDeserialize(CDRDeserializer* cdr,
    ShapeType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean ShapeTypeNoSerializingSupported()
{
    return false;
}

DDS_ULong ShapeTypeFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
