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
    return 32796;
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

    self->type = 0;

    self->sn = 0;

    self->cmd = 0;

    self->len = 0;

    self->crc = 0;

    DDS_OctetSeq_initialize_ex(&self->data, pool, allocateMemory);

    if (allocateMemory)
    {
        if (!DDS_OctetSeq_set_maximum(&self->data, 32768))
        {
            printf("Set maximum for self->data failed.");
            return false;
        }
    }
    else
    {
    }
    return true;
}

void ShapeTypeFinalizeEx(
    ShapeType* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    DDS_OctetSeq_finalize(&self->data);
    if (deletePointers)
    {
    }
}

DDS_Boolean ShapeTypeCopyEx(
    ShapeType* dst,
    const ShapeType* src,
    ZRMemPool* pool)
{
    dst->x = src->x;
    dst->type = src->type;
    dst->sn = src->sn;
    dst->cmd = src->cmd;
    dst->len = src->len;
    dst->crc = src->crc;
    if (!DDS_OctetSeq_copy(&dst->data, &src->data))
    {
        printf("copy member data failed.");
        return false;
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
    printf("sample->x: %d\n", sample->x);
    printf("\n");

    printf("sample->type: %d\n", sample->type);
    printf("\n");

    printf("sample->sn: %d\n", sample->sn);
    printf("\n");

    printf("sample->cmd: %d\n", sample->cmd);
    printf("\n");

    printf("sample->len: %d\n", sample->len);
    printf("\n");

    printf("sample->crc: %d\n", sample->crc);
    printf("\n");

    DDS_ULong dataTmpLen = DDS_OctetSeq_get_length(&sample->data);
    printf("sample->data: %d\n", dataTmpLen);
    for (DDS_ULong i = 0; i < dataTmpLen; ++i)
    {
        printf("sample->data[%u]: 0x%02x\n", i, *DDS_OctetSeq_get_reference(&sample->data, i));
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
        printf("Get Member type TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "type",
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
        printf("Get Member sn TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "sn",
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
        printf("Get Member cmd TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "cmd",
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
        printf("Get Member len TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "len",
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
        printf("Get Member crc TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "crc",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_UCHAR);
    if (memberTc != NULL)
    {
        memberTc = factory.createSequenceTC(32768, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member data TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "data",
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

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->type, 4))
    {
        printf("serialize sample->type failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sn, 4))
    {
        printf("serialize sample->sn failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->cmd, 4))
    {
        printf("serialize sample->cmd failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->len, 4))
    {
        printf("serialize sample->len failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->crc, 4))
    {
        printf("serialize sample->crc failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &(sample->data)._length, 4))
    {
        printf("serialize length of sample->data failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if ((sample->data)._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)(sample->data)._contiguousBuffer, (sample->data)._length, 1))
            {
                printf("serialize sample->data failed.");
                return -2;
            }
        }
        else
        {
            for (DDS_ULong i = 0; i < (sample->data)._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_OctetSeq_get_reference(&sample->data, i), 1))
                {
                    printf("serialize sample->data failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

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
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->type, 4))
    {
        sample->type = 0;
        sample->sn = 0;
        sample->cmd = 0;
        sample->len = 0;
        sample->crc = 0;
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sn, 4))
    {
        sample->sn = 0;
        sample->cmd = 0;
        sample->len = 0;
        sample->crc = 0;
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->cmd, 4))
    {
        sample->cmd = 0;
        sample->len = 0;
        sample->crc = 0;
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->len, 4))
    {
        sample->len = 0;
        sample->crc = 0;
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->crc, 4))
    {
        sample->crc = 0;
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    DDS_ULong dataTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &dataTmpLen, 4))
    {
        DDS_OctetSeq_initialize_ex(&sample->data, pool, true);
        return 0;
    }
    if (!DDS_OctetSeq_ensure_length(&sample->data, dataTmpLen, dataTmpLen))
    {
        printf("Set maxiumum member sample->data failed.");
        return -3;
    }
    if (sample->data._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->data._contiguousBuffer, dataTmpLen, 1))
        {
            printf("deserialize sample->data failed.");
            return -2;
        }
    }
    else
    {
        for (DDS_ULong i = 0; i < dataTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_OctetSeq_get_reference(&sample->data, i), 1))
            {
                printf("deserialize sample->data failed.");
                return -2;
            }
        }
    }
    return 0;
}

DDS_ULong ShapeTypeGetSerializedSampleSize(const ShapeType* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    DDS_ULong dataLen = DDS_OctetSeq_get_length(&sample->data);
    if (dataLen != 0)
    {
        currentAlignment += 1 * dataLen;
    }

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
    DDS_Char* rst = (DDS_Char*)(sample->data._contiguousBuffer);
    if (takeBuffer)
    {
        sample->data._length = 0;
        sample->data._maximum = 0;
        sample->data._contiguousBuffer = NULL;
    }
    return rst;
}

void ShapeTypeReturnSampleBuf(DDS_Char* sampleBuf)
{
    ZRDealloc(NULL, sampleBuf);
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
#ifdef _ZRDDS_INCLUDE_DR_NO_SERIALIZE_MODE
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->x), 4))
        {
            printf("deserialize x failed.");
            return -2;
        }
    }
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->type), 4))
        {
            printf("deserialize type failed.");
            return -2;
        }
    }
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->sn), 4))
        {
            printf("deserialize sn failed.");
            return -2;
        }
    }
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->cmd), 4))
        {
            printf("deserialize cmd failed.");
            return -2;
        }
    }
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->len), 4))
        {
            printf("deserialize len failed.");
            return -2;
        }
    }
    if (totalNum == 1 || curIndex == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->crc), 4))
        {
            printf("deserialize crc failed.");
            return -2;
        }
    }
    DDS_OctetSeq* sampleSeq = &(sampleBuf->data);
    DDS_Char** fragments = sampleSeq->_fixedFragments;
    DDS_Char** headers = sampleSeq->_fixedHeader;
    if (totalNum > 64)
    {
        if (sampleSeq->_variousFragments == NULL || sampleSeq->_fragmentNum < totalNum)
        {
            ZRDealloc(NULL, sampleSeq->_variousFragments);
            ZRDealloc(NULL, sampleSeq->_variousHeader);
            // 分片数量大于64，需要动态分配
            sampleSeq->_variousFragments = (DDS_Char**)ZRMalloc(NULL, totalNum * sizeof(DDS_Char*));
            sampleSeq->_variousHeader = (DDS_Char**)ZRMalloc(NULL, totalNum * sizeof(DDS_Char*));
            if (NULL == sampleSeq->_variousFragments || NULL == sampleSeq->_variousHeader)
            {
                printf("malloc for _variousFragments failed.\n");
                return -1;
            }
            memset(sampleSeq->_variousFragments, 0, sizeof(totalNum * sizeof(DDS_Char*)));
            memset(sampleSeq->_variousHeader, 0, sizeof(totalNum * sizeof(DDS_Char*)));
        }
        fragments = sampleSeq->_variousFragments;
        headers = sampleSeq->_variousHeader;
    }
    sampleSeq->_fragmentNum = totalNum;
    if (totalNum == 1)
    {
        sampleSeq->_length = *(DDS_ULong*)(base + offset + fixedHeaderLen - 4);
        sampleSeq->_maximum = sampleSeq->_length;
        sampleSeq->_firstFragSize = space - fixedHeaderLen;
        fragments[curIndex] = base + offset + fixedHeaderLen;
        headers[curIndex] = base;
    }
    else if (curIndex == 0)
    {
        sampleSeq->_length = *(DDS_ULong*)(base + offset + fixedHeaderLen - 4);
        sampleSeq->_maximum = sampleSeq->_length;
        sampleSeq->_firstFragSize = space - fixedHeaderLen;
        fragments[curIndex] = base + offset + fixedHeaderLen;
        headers[curIndex] = base;
    }
    else if (curIndex == totalNum - 1)
    {
        sampleSeq->_lastFragSize = space;
        fragments[curIndex] = base + offset;
        headers[curIndex] = base;
    }
    else
    {
        sampleSeq->_fragmentSize = space;
        fragments[curIndex] = base + offset;
        headers[curIndex] = base;
    }
#endif /* _ZRDDS_INCLUDE_DR_NO_SERIALIZE_MODE */
    return 0;
}

#endif /*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long ShapeTypeOnSiteDeserialize(CDRDeserializer* cdr,
    ShapeType* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    DDS_OctetSeq* seqMember = &(sample->data);
    if (!DDS_OctetSeq_set_maximum(seqMember, totalSize - fixedHeaderLen))
    {
        printf("Set maxiumum member data failed.");
        return -3;
    }
    if (offset == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->x, 4))
        {
            printf("deserialize sample->x failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->type, 4))
        {
            printf("deserialize sample->type failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sn, 4))
        {
            printf("deserialize sample->sn failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->cmd, 4))
        {
            printf("deserialize sample->cmd failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->len, 4))
        {
            printf("deserialize sample->len failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->crc, 4))
        {
            printf("deserialize sample->crc failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*)&seqMember->_length, 4))
        {
            printf("get data length failed.");
            return -1;
        }
        memcpy(seqMember->_contiguousBuffer,
            (DDS_Char*)payload + fixedHeaderLen,
            payloadLen - fixedHeaderLen);
        return 0;
    }
    memcpy(seqMember->_contiguousBuffer + offset - fixedHeaderLen,
        payload, payloadLen);
    return 0;
}

DDS_Boolean ShapeTypeNoSerializingSupported()
{
    return true;
}

DDS_ULong ShapeTypeFixedHeaderLength()
{
    DDS_ULong curLen = 0;
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    return curLen;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
