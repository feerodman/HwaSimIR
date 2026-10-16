/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CustomerTxVideo.h"

#define T HD264Stream
#define TSeq HD264StreamSeq
#define TINITIALIZE HD264StreamInitializeEx
#define TFINALIZE HD264StreamFinalizeEx
#define TCOPY HD264StreamCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean HD264StreamInitialize(HD264Stream* self)
{
    return HD264StreamInitializeEx(self, NULL, true);
}

void HD264StreamFinalize(HD264Stream* self)
{
    HD264StreamFinalizeEx(self, NULL, true);
}

DDS_Boolean HD264StreamCopy(
    HD264Stream* dst,
    const HD264Stream* src)
{
    return HD264StreamCopyEx(dst, src, NULL);
}

HD264Stream* HD264StreamCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    HD264Stream* newSample = (HD264Stream*)ZRMalloc(pool, sizeof(HD264Stream));
    if (newSample == NULL)
    {
        printf("malloc for HD264Stream failed.");
        return NULL;
    }
    if (!HD264StreamInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        HD264StreamDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void HD264StreamDestroySample(ZRMemPool* pool, HD264Stream* sample)
{
    if (sample == NULL) return;
    HD264StreamFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong HD264StreamGetSerializedSampleMaxSize()
{
    return 1404;
}

DDS_ULong HD264StreamGetSerializedKeyMaxSize()
{
    return 1404;
}

DDS_Long HD264StreamGetKeyHash(
    const HD264Stream* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = HD264StreamSerializeKey(sample, cdr);
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

DDS_Boolean HD264StreamHasKey()
{
    return false;
}

TypeCodeHeader* HD264StreamGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = HD264StreamGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean HD264StreamInitializeEx(
    HD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->DataLength = 0;


    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void HD264StreamFinalizeEx(
    HD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean HD264StreamCopyEx(
    HD264Stream* dst,
    const HD264Stream* src,
    ZRMemPool* pool)
{
    dst->DataLength = src->DataLength;
    memcpy(dst->VedioStream, src->VedioStream, 1 * 1400);
    return true;
}

void HD264StreamPrintData(const HD264Stream *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->DataLength: %u\n", sample->DataLength);
    printf("\n");

    DDS_Octet* VedioStreamArray = (DDS_Octet*) sample->VedioStream;
    for (DDS_ULong VedioStreamIndex = 0; VedioStreamIndex < 1400; ++VedioStreamIndex)
    {
        printf("VedioStreamArray[%u]: 0x%02x\n", VedioStreamIndex, VedioStreamArray[VedioStreamIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* HD264StreamGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HD264Stream",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct HD264Stream typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member DataLength TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "DataLength",
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
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 1400;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member VedioStream TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "VedioStream",
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

DDS_Long HD264StreamSerialize(const HD264Stream* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->DataLength, 4))
    {
        printf("serialize sample->DataLength failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->VedioStream, 1400, 1))
    {
        printf("serialize sample->VedioStream failed.");
        return -2;
    }

    return 0;
}

DDS_Long HD264StreamDeserialize(
    HD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->DataLength, 4))
    {
        sample->DataLength = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->VedioStream, 1400, 1))
    {
        return 0;
    }
    return 0;
}

DDS_ULong HD264StreamGetSerializedSampleSize(const HD264Stream* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);
    currentAlignment += 1 * 1399;

    return currentAlignment - initialAlignment;
}

DDS_Long HD264StreamSerializeKey(const HD264Stream* sample, CDRSerializer *cdr)
{
    if (HD264StreamSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long HD264StreamDeserializeKey(
    HD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (HD264StreamDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong HD264StreamGetSerializedKeySize(const HD264Stream* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += HD264StreamGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* HD264StreamLoanSampleBuf(HD264Stream* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void HD264StreamReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long HD264StreamLoanDeserialize(HD264Stream* sampleBuf,
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

#endif /*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long HD264StreamOnSiteDeserialize(CDRDeserializer* cdr,
    HD264Stream* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean HD264StreamNoSerializingSupported()
{
    return false;
}

DDS_ULong HD264StreamFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T LD264Stream
#define TSeq LD264StreamSeq
#define TINITIALIZE LD264StreamInitializeEx
#define TFINALIZE LD264StreamFinalizeEx
#define TCOPY LD264StreamCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean LD264StreamInitialize(LD264Stream* self)
{
    return LD264StreamInitializeEx(self, NULL, true);
}

void LD264StreamFinalize(LD264Stream* self)
{
    LD264StreamFinalizeEx(self, NULL, true);
}

DDS_Boolean LD264StreamCopy(
    LD264Stream* dst,
    const LD264Stream* src)
{
    return LD264StreamCopyEx(dst, src, NULL);
}

LD264Stream* LD264StreamCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    LD264Stream* newSample = (LD264Stream*)ZRMalloc(pool, sizeof(LD264Stream));
    if (newSample == NULL)
    {
        printf("malloc for LD264Stream failed.");
        return NULL;
    }
    if (!LD264StreamInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        LD264StreamDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void LD264StreamDestroySample(ZRMemPool* pool, LD264Stream* sample)
{
    if (sample == NULL) return;
    LD264StreamFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong LD264StreamGetSerializedSampleMaxSize()
{
    return 1404;
}

DDS_ULong LD264StreamGetSerializedKeyMaxSize()
{
    return 1404;
}

DDS_Long LD264StreamGetKeyHash(
    const LD264Stream* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = LD264StreamSerializeKey(sample, cdr);
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

DDS_Boolean LD264StreamHasKey()
{
    return false;
}

TypeCodeHeader* LD264StreamGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = LD264StreamGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean LD264StreamInitializeEx(
    LD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->DataLength = 0;


    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void LD264StreamFinalizeEx(
    LD264Stream* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean LD264StreamCopyEx(
    LD264Stream* dst,
    const LD264Stream* src,
    ZRMemPool* pool)
{
    dst->DataLength = src->DataLength;
    memcpy(dst->VedioStream, src->VedioStream, 1 * 1400);
    return true;
}

void LD264StreamPrintData(const LD264Stream *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->DataLength: %u\n", sample->DataLength);
    printf("\n");

    DDS_Octet* VedioStreamArray = (DDS_Octet*) sample->VedioStream;
    for (DDS_ULong VedioStreamIndex = 0; VedioStreamIndex < 1400; ++VedioStreamIndex)
    {
        printf("VedioStreamArray[%u]: 0x%02x\n", VedioStreamIndex, VedioStreamArray[VedioStreamIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* LD264StreamGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "LD264Stream",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct LD264Stream typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member DataLength TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "DataLength",
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
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 1400;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member VedioStream TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "VedioStream",
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

DDS_Long LD264StreamSerialize(const LD264Stream* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->DataLength, 4))
    {
        printf("serialize sample->DataLength failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->VedioStream, 1400, 1))
    {
        printf("serialize sample->VedioStream failed.");
        return -2;
    }

    return 0;
}

DDS_Long LD264StreamDeserialize(
    LD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->DataLength, 4))
    {
        sample->DataLength = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->VedioStream, 1400, 1))
    {
        return 0;
    }
    return 0;
}

DDS_ULong LD264StreamGetSerializedSampleSize(const LD264Stream* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);
    currentAlignment += 1 * 1399;

    return currentAlignment - initialAlignment;
}

DDS_Long LD264StreamSerializeKey(const LD264Stream* sample, CDRSerializer *cdr)
{
    if (LD264StreamSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long LD264StreamDeserializeKey(
    LD264Stream* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (LD264StreamDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong LD264StreamGetSerializedKeySize(const LD264Stream* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += LD264StreamGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* LD264StreamLoanSampleBuf(LD264Stream* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void LD264StreamReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long LD264StreamLoanDeserialize(LD264Stream* sampleBuf,
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

#endif /*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Long LD264StreamOnSiteDeserialize(CDRDeserializer* cdr,
    LD264Stream* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean LD264StreamNoSerializingSupported()
{
    return false;
}

DDS_ULong LD264StreamFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T HDDecoder
#define TSeq HDDecoderSeq
#define TINITIALIZE HDDecoderInitializeEx
#define TFINALIZE HDDecoderFinalizeEx
#define TCOPY HDDecoderCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean HDDecoderInitialize(HDDecoder* self)
{
    return HDDecoderInitializeEx(self, NULL, true);
}

void HDDecoderFinalize(HDDecoder* self)
{
    HDDecoderFinalizeEx(self, NULL, true);
}

DDS_Boolean HDDecoderCopy(
    HDDecoder* dst,
    const HDDecoder* src)
{
    return HDDecoderCopyEx(dst, src, NULL);
}

HDDecoder* HDDecoderCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    HDDecoder* newSample = (HDDecoder*)ZRMalloc(pool, sizeof(HDDecoder));
    if (newSample == NULL)
    {
        printf("malloc for HDDecoder failed.");
        return NULL;
    }
    if (!HDDecoderInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        HDDecoderDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void HDDecoderDestroySample(ZRMemPool* pool, HDDecoder* sample)
{
    if (sample == NULL) return;
    HDDecoderFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong HDDecoderGetSerializedSampleMaxSize()
{
    return 640008;
}

DDS_ULong HDDecoderGetSerializedKeyMaxSize()
{
    return 640008;
}

DDS_Long HDDecoderGetKeyHash(
    const HDDecoder* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = HDDecoderSerializeKey(sample, cdr);
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

DDS_Boolean HDDecoderHasKey()
{
    return false;
}

TypeCodeHeader* HDDecoderGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = HDDecoderGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean HDDecoderInitializeEx(
    HDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->frameId = 0;

    DDS_CharSeq_initialize_ex(&self->stream, pool, allocateMemory);

    if (allocateMemory)
    {
        if (!DDS_CharSeq_set_maximum(&self->stream, 640000))
        {
            printf("Set maximum for self->stream failed.");
            return false;
        }
    }
    else
    {
    }
    return true;
}

void HDDecoderFinalizeEx(
    HDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    DDS_CharSeq_finalize(&self->stream);
    if (deletePointers)
    {
    }
}

DDS_Boolean HDDecoderCopyEx(
    HDDecoder* dst,
    const HDDecoder* src,
    ZRMemPool* pool)
{
    dst->frameId = src->frameId;
    if (!DDS_CharSeq_copy(&dst->stream, &src->stream))
    {
        printf("copy member stream failed.");
        return false;
    }
    return true;
}

void HDDecoderPrintData(const HDDecoder *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->frameId: %d\n", sample->frameId);
    printf("\n");

    DDS_ULong streamTmpLen = DDS_CharSeq_get_length(&sample->stream);
    printf("sample->stream: %d\n", streamTmpLen);
    for (DDS_ULong i = 0; i < streamTmpLen; ++i)
    {
        printf("sample->stream[%u]: 0x%02x\n", i, *DDS_CharSeq_get_reference(&sample->stream, i));
    }
    printf("\n");

}

DDS::TypeCode* HDDecoderGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HDDecoder",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct HDDecoder typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member frameId TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "frameId",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_CHAR);
    if (memberTc != NULL)
    {
        memberTc = factory.createSequenceTC(640000, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member stream TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "stream",
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

DDS_Long HDDecoderSerialize(const HDDecoder* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
    {
        printf("serialize sample->frameId failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &(sample->stream)._length, 4))
    {
        printf("serialize length of sample->stream failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if ((sample->stream)._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)(sample->stream)._contiguousBuffer, (sample->stream)._length, 1))
            {
                printf("serialize sample->stream failed.");
                return -2;
            }
        }
        else
        {
            for (DDS_ULong i = 0; i < (sample->stream)._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->stream, i), 1))
                {
                    printf("serialize sample->stream failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

    return 0;
}

DDS_Long HDDecoderDeserialize(
    HDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
    {
        sample->frameId = 0;
        DDS_CharSeq_initialize_ex(&sample->stream, pool, true);
        return 0;
    }
    DDS_ULong streamTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &streamTmpLen, 4))
    {
        DDS_CharSeq_initialize_ex(&sample->stream, pool, true);
        return 0;
    }
    if (!DDS_CharSeq_ensure_length(&sample->stream, streamTmpLen, streamTmpLen))
    {
        printf("Set maxiumum member sample->stream failed.");
        return -3;
    }
    if (sample->stream._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->stream._contiguousBuffer, streamTmpLen, 1))
        {
            printf("deserialize sample->stream failed.");
            return -2;
        }
    }
    else
    {
        for (DDS_ULong i = 0; i < streamTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->stream, i), 1))
            {
                printf("deserialize sample->stream failed.");
                return -2;
            }
        }
    }
    return 0;
}

DDS_ULong HDDecoderGetSerializedSampleSize(const HDDecoder* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    DDS_ULong streamLen = DDS_CharSeq_get_length(&sample->stream);
    if (streamLen != 0)
    {
        currentAlignment += 1 * streamLen;
    }

    return currentAlignment - initialAlignment;
}

DDS_Long HDDecoderSerializeKey(const HDDecoder* sample, CDRSerializer *cdr)
{
    if (HDDecoderSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long HDDecoderDeserializeKey(
    HDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (HDDecoderDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong HDDecoderGetSerializedKeySize(const HDDecoder* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += HDDecoderGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* HDDecoderLoanSampleBuf(HDDecoder* sample, DDS_Boolean takeBuffer)
{
    DDS_Char* rst = (DDS_Char*)(sample->stream._contiguousBuffer);
    if (takeBuffer)
    {
        sample->stream._length = 0;
        sample->stream._maximum = 0;
        sample->stream._contiguousBuffer = NULL;
    }
    return rst;
}

void HDDecoderReturnSampleBuf(DDS_Char* sampleBuf)
{
    ZRDealloc(NULL, sampleBuf);
}

DDS_Long HDDecoderLoanDeserialize(HDDecoder* sampleBuf,
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
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->frameId), 4))
        {
            printf("deserialize frameId failed.");
            return -2;
        }
    }
    DDS_CharSeq* sampleSeq = &(sampleBuf->stream);
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
DDS_Long HDDecoderOnSiteDeserialize(CDRDeserializer* cdr,
    HDDecoder* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    DDS_CharSeq* seqMember = &(sample->stream);
    if (!DDS_CharSeq_set_maximum(seqMember, totalSize - fixedHeaderLen))
    {
        printf("Set maxiumum member stream failed.");
        return -3;
    }
    if (offset == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
        {
            printf("deserialize sample->frameId failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*)&seqMember->_length, 4))
        {
            printf("get stream length failed.");
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

DDS_Boolean HDDecoderNoSerializingSupported()
{
    return true;
}

DDS_ULong HDDecoderFixedHeaderLength()
{
    DDS_ULong curLen = 0;
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    return curLen;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T LDDecoder
#define TSeq LDDecoderSeq
#define TINITIALIZE LDDecoderInitializeEx
#define TFINALIZE LDDecoderFinalizeEx
#define TCOPY LDDecoderCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean LDDecoderInitialize(LDDecoder* self)
{
    return LDDecoderInitializeEx(self, NULL, true);
}

void LDDecoderFinalize(LDDecoder* self)
{
    LDDecoderFinalizeEx(self, NULL, true);
}

DDS_Boolean LDDecoderCopy(
    LDDecoder* dst,
    const LDDecoder* src)
{
    return LDDecoderCopyEx(dst, src, NULL);
}

LDDecoder* LDDecoderCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    LDDecoder* newSample = (LDDecoder*)ZRMalloc(pool, sizeof(LDDecoder));
    if (newSample == NULL)
    {
        printf("malloc for LDDecoder failed.");
        return NULL;
    }
    if (!LDDecoderInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        LDDecoderDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void LDDecoderDestroySample(ZRMemPool* pool, LDDecoder* sample)
{
    if (sample == NULL) return;
    LDDecoderFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong LDDecoderGetSerializedSampleMaxSize()
{
    return 640008;
}

DDS_ULong LDDecoderGetSerializedKeyMaxSize()
{
    return 640008;
}

DDS_Long LDDecoderGetKeyHash(
    const LDDecoder* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = LDDecoderSerializeKey(sample, cdr);
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

DDS_Boolean LDDecoderHasKey()
{
    return false;
}

TypeCodeHeader* LDDecoderGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = LDDecoderGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean LDDecoderInitializeEx(
    LDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->frameId = 0;

    DDS_CharSeq_initialize_ex(&self->stream, pool, allocateMemory);

    if (allocateMemory)
    {
        if (!DDS_CharSeq_set_maximum(&self->stream, 640000))
        {
            printf("Set maximum for self->stream failed.");
            return false;
        }
    }
    else
    {
    }
    return true;
}

void LDDecoderFinalizeEx(
    LDDecoder* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    DDS_CharSeq_finalize(&self->stream);
    if (deletePointers)
    {
    }
}

DDS_Boolean LDDecoderCopyEx(
    LDDecoder* dst,
    const LDDecoder* src,
    ZRMemPool* pool)
{
    dst->frameId = src->frameId;
    if (!DDS_CharSeq_copy(&dst->stream, &src->stream))
    {
        printf("copy member stream failed.");
        return false;
    }
    return true;
}

void LDDecoderPrintData(const LDDecoder *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->frameId: %d\n", sample->frameId);
    printf("\n");

    DDS_ULong streamTmpLen = DDS_CharSeq_get_length(&sample->stream);
    printf("sample->stream: %d\n", streamTmpLen);
    for (DDS_ULong i = 0; i < streamTmpLen; ++i)
    {
        printf("sample->stream[%u]: 0x%02x\n", i, *DDS_CharSeq_get_reference(&sample->stream, i));
    }
    printf("\n");

}

DDS::TypeCode* LDDecoderGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "LDDecoder",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct LDDecoder typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member frameId TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "frameId",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_CHAR);
    if (memberTc != NULL)
    {
        memberTc = factory.createSequenceTC(640000, memberTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member stream TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "stream",
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

DDS_Long LDDecoderSerialize(const LDDecoder* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
    {
        printf("serialize sample->frameId failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &(sample->stream)._length, 4))
    {
        printf("serialize length of sample->stream failed.");
        return -2;
    }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    if (!CDRSerializerNoSerializingMode(cdr))
    {
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
        if ((sample->stream)._contiguousBuffer)
        {
            if (!CDRSerializerPutUntypeArray(cdr, (DDS_Octet*)(sample->stream)._contiguousBuffer, (sample->stream)._length, 1))
            {
                printf("serialize sample->stream failed.");
                return -2;
            }
        }
        else
        {
            for (DDS_ULong i = 0; i < (sample->stream)._length; ++i)
            {
                if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->stream, i), 1))
                {
                    printf("serialize sample->stream failed.");
                    return -2;
                }
            }
        }
#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
    }
#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/

    return 0;
}

DDS_Long LDDecoderDeserialize(
    LDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
    {
        sample->frameId = 0;
        DDS_CharSeq_initialize_ex(&sample->stream, pool, true);
        return 0;
    }
    DDS_ULong streamTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &streamTmpLen, 4))
    {
        DDS_CharSeq_initialize_ex(&sample->stream, pool, true);
        return 0;
    }
    if (!DDS_CharSeq_ensure_length(&sample->stream, streamTmpLen, streamTmpLen))
    {
        printf("Set maxiumum member sample->stream failed.");
        return -3;
    }
    if (sample->stream._contiguousBuffer)
    {
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->stream._contiguousBuffer, streamTmpLen, 1))
        {
            printf("deserialize sample->stream failed.");
            return -2;
        }
    }
    else
    {
        for (DDS_ULong i = 0; i < streamTmpLen; ++i)
        {
            if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &*DDS_CharSeq_get_reference(&sample->stream, i), 1))
            {
                printf("deserialize sample->stream failed.");
                return -2;
            }
        }
    }
    return 0;
}

DDS_ULong LDDecoderGetSerializedSampleSize(const LDDecoder* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);
    DDS_ULong streamLen = DDS_CharSeq_get_length(&sample->stream);
    if (streamLen != 0)
    {
        currentAlignment += 1 * streamLen;
    }

    return currentAlignment - initialAlignment;
}

DDS_Long LDDecoderSerializeKey(const LDDecoder* sample, CDRSerializer *cdr)
{
    if (LDDecoderSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long LDDecoderDeserializeKey(
    LDDecoder* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (LDDecoderDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong LDDecoderGetSerializedKeySize(const LDDecoder* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += LDDecoderGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* LDDecoderLoanSampleBuf(LDDecoder* sample, DDS_Boolean takeBuffer)
{
    DDS_Char* rst = (DDS_Char*)(sample->stream._contiguousBuffer);
    if (takeBuffer)
    {
        sample->stream._length = 0;
        sample->stream._maximum = 0;
        sample->stream._contiguousBuffer = NULL;
    }
    return rst;
}

void LDDecoderReturnSampleBuf(DDS_Char* sampleBuf)
{
    ZRDealloc(NULL, sampleBuf);
}

DDS_Long LDDecoderLoanDeserialize(LDDecoder* sampleBuf,
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
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &(sampleBuf->frameId), 4))
        {
            printf("deserialize frameId failed.");
            return -2;
        }
    }
    DDS_CharSeq* sampleSeq = &(sampleBuf->stream);
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
DDS_Long LDDecoderOnSiteDeserialize(CDRDeserializer* cdr,
    LDDecoder* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    DDS_CharSeq* seqMember = &(sample->stream);
    if (!DDS_CharSeq_set_maximum(seqMember, totalSize - fixedHeaderLen))
    {
        printf("Set maxiumum member stream failed.");
        return -3;
    }
    if (offset == 0)
    {
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameId, 4))
        {
            printf("deserialize sample->frameId failed.");
            return -2;
        }
        if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*)&seqMember->_length, 4))
        {
            printf("get stream length failed.");
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

DDS_Boolean LDDecoderNoSerializingSupported()
{
    return true;
}

DDS_ULong LDDecoderFixedHeaderLength()
{
    DDS_ULong curLen = 0;
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    curLen += CDRSerializerGetUntypeSize(4, curLen);
    return curLen;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
