/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "DDS_Struct.h"

#define T ServToProxy
#define TSeq ServToProxySeq
#define TINITIALIZE ServToProxyInitializeEx
#define TFINALIZE ServToProxyFinalizeEx
#define TCOPY ServToProxyCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean ServToProxyInitialize(ServToProxy* self)
{
    return ServToProxyInitializeEx(self, NULL, true);
}

void ServToProxyFinalize(ServToProxy* self)
{
    ServToProxyFinalizeEx(self, NULL, true);
}

DDS_Boolean ServToProxyCopy(
    ServToProxy* dst,
    const ServToProxy* src)
{
    return ServToProxyCopyEx(dst, src, NULL);
}

ServToProxy* ServToProxyCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    ServToProxy* newSample = (ServToProxy*)ZRMalloc(pool, sizeof(ServToProxy));
    if (newSample == NULL)
    {
        printf("malloc for ServToProxy failed.");
        return NULL;
    }
    if (!ServToProxyInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        ServToProxyDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void ServToProxyDestroySample(ZRMemPool* pool, ServToProxy* sample)
{
    if (sample == NULL) return;
    ServToProxyFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong ServToProxyGetSerializedSampleMaxSize()
{
    return 272;
}

DDS_ULong ServToProxyGetSerializedKeyMaxSize()
{
    return 272;
}

DDS_Long ServToProxyGetKeyHash(
    const ServToProxy* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = ServToProxySerializeKey(sample, cdr);
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

DDS_Boolean ServToProxyHasKey()
{
    return false;
}

TypeCodeHeader* ServToProxyGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = ServToProxyGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean ServToProxyInitializeEx(
    ServToProxy* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->SysIdx = 0;

    self->ReqCmd = 0;

    self->evtype = 0;

    self->evtsub = 0;

    self->HaveTimeOut = 0;

    self->ValidLen = 0;


    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void ServToProxyFinalizeEx(
    ServToProxy* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean ServToProxyCopyEx(
    ServToProxy* dst,
    const ServToProxy* src,
    ZRMemPool* pool)
{
    dst->SysIdx = src->SysIdx;
    dst->ReqCmd = src->ReqCmd;
    dst->evtype = src->evtype;
    dst->evtsub = src->evtsub;
    dst->HaveTimeOut = src->HaveTimeOut;
    dst->ValidLen = src->ValidLen;
    memcpy(dst->ucData, src->ucData, 1 * 256);
    return true;
}

void ServToProxyPrintData(const ServToProxy *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->SysIdx: %u\n", sample->SysIdx);
    printf("\n");

    printf("sample->ReqCmd: %u\n", sample->ReqCmd);
    printf("\n");

    printf("sample->evtype: %u\n", sample->evtype);
    printf("\n");

    printf("sample->evtsub: %u\n", sample->evtsub);
    printf("\n");

    printf("sample->HaveTimeOut: %u\n", sample->HaveTimeOut);
    printf("\n");

    printf("sample->ValidLen: %u\n", sample->ValidLen);
    printf("\n");

    DDS_Octet* ucDataArray = (DDS_Octet*) sample->ucData;
    for (DDS_ULong ucDataIndex = 0; ucDataIndex < 256; ++ucDataIndex)
    {
        printf("ucDataArray[%u]: 0x%02x\n", ucDataIndex, ucDataArray[ucDataIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* ServToProxyGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "ServToProxy",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct ServToProxy typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member SysIdx TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "SysIdx",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member ReqCmd TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "ReqCmd",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member evtype TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "evtype",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member evtsub TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "evtsub",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member HaveTimeOut TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "HaveTimeOut",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member ValidLen TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "ValidLen",
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
        labels[0] = 256;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member ucData TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "ucData",
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

DDS_Long ServToProxySerialize(const ServToProxy* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->SysIdx, 2))
    {
        printf("serialize sample->SysIdx failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->ReqCmd, 2))
    {
        printf("serialize sample->ReqCmd failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->evtype, 4))
    {
        printf("serialize sample->evtype failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->evtsub, 4))
    {
        printf("serialize sample->evtsub failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->HaveTimeOut, 2))
    {
        printf("serialize sample->HaveTimeOut failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->ValidLen, 2))
    {
        printf("serialize sample->ValidLen failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->ucData, 256, 1))
    {
        printf("serialize sample->ucData failed.");
        return -2;
    }

    return 0;
}

DDS_Long ServToProxyDeserialize(
    ServToProxy* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->SysIdx, 2))
    {
        sample->SysIdx = 0;
        sample->ReqCmd = 0;
        sample->evtype = 0;
        sample->evtsub = 0;
        sample->HaveTimeOut = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->ReqCmd, 2))
    {
        sample->ReqCmd = 0;
        sample->evtype = 0;
        sample->evtsub = 0;
        sample->HaveTimeOut = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->evtype, 4))
    {
        sample->evtype = 0;
        sample->evtsub = 0;
        sample->HaveTimeOut = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->evtsub, 4))
    {
        sample->evtsub = 0;
        sample->HaveTimeOut = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->HaveTimeOut, 2))
    {
        sample->HaveTimeOut = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->ValidLen, 2))
    {
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->ucData, 256, 1))
    {
        return 0;
    }
    return 0;
}

DDS_ULong ServToProxyGetSerializedSampleSize(const ServToProxy* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);
    currentAlignment += 1 * 255;

    return currentAlignment - initialAlignment;
}

DDS_Long ServToProxySerializeKey(const ServToProxy* sample, CDRSerializer *cdr)
{
    if (ServToProxySerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long ServToProxyDeserializeKey(
    ServToProxy* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (ServToProxyDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong ServToProxyGetSerializedKeySize(const ServToProxy* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += ServToProxyGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ServToProxyLoanSampleBuf(ServToProxy* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void ServToProxyReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long ServToProxyLoanDeserialize(ServToProxy* sampleBuf,
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
DDS_Long ServToProxyOnSiteDeserialize(CDRDeserializer* cdr,
    ServToProxy* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean ServToProxyNoSerializingSupported()
{
    return false;
}

DDS_ULong ServToProxyFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T ProxyToServ
#define TSeq ProxyToServSeq
#define TINITIALIZE ProxyToServInitializeEx
#define TFINALIZE ProxyToServFinalizeEx
#define TCOPY ProxyToServCopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean ProxyToServInitialize(ProxyToServ* self)
{
    return ProxyToServInitializeEx(self, NULL, true);
}

void ProxyToServFinalize(ProxyToServ* self)
{
    ProxyToServFinalizeEx(self, NULL, true);
}

DDS_Boolean ProxyToServCopy(
    ProxyToServ* dst,
    const ProxyToServ* src)
{
    return ProxyToServCopyEx(dst, src, NULL);
}

ProxyToServ* ProxyToServCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    ProxyToServ* newSample = (ProxyToServ*)ZRMalloc(pool, sizeof(ProxyToServ));
    if (newSample == NULL)
    {
        printf("malloc for ProxyToServ failed.");
        return NULL;
    }
    if (!ProxyToServInitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        ProxyToServDestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void ProxyToServDestroySample(ZRMemPool* pool, ProxyToServ* sample)
{
    if (sample == NULL) return;
    ProxyToServFinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong ProxyToServGetSerializedSampleMaxSize()
{
    return 262;
}

DDS_ULong ProxyToServGetSerializedKeyMaxSize()
{
    return 262;
}

DDS_Long ProxyToServGetKeyHash(
    const ProxyToServ* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = ProxyToServSerializeKey(sample, cdr);
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

DDS_Boolean ProxyToServHasKey()
{
    return false;
}

TypeCodeHeader* ProxyToServGetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = ProxyToServGetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean ProxyToServInitializeEx(
    ProxyToServ* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->SysIdx = 0;

    self->RespCmd = 0;

    self->ValidLen = 0;


    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void ProxyToServFinalizeEx(
    ProxyToServ* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean ProxyToServCopyEx(
    ProxyToServ* dst,
    const ProxyToServ* src,
    ZRMemPool* pool)
{
    dst->SysIdx = src->SysIdx;
    dst->RespCmd = src->RespCmd;
    dst->ValidLen = src->ValidLen;
    memcpy(dst->ucData, src->ucData, 1 * 256);
    return true;
}

void ProxyToServPrintData(const ProxyToServ *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->SysIdx: %u\n", sample->SysIdx);
    printf("\n");

    printf("sample->RespCmd: %u\n", sample->RespCmd);
    printf("\n");

    printf("sample->ValidLen: %u\n", sample->ValidLen);
    printf("\n");

    DDS_Octet* ucDataArray = (DDS_Octet*) sample->ucData;
    for (DDS_ULong ucDataIndex = 0; ucDataIndex < 256; ++ucDataIndex)
    {
        printf("ucDataArray[%u]: 0x%02x\n", ucDataIndex, ucDataArray[ucDataIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* ProxyToServGetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "ProxyToServ",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct ProxyToServ typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member SysIdx TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "SysIdx",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member RespCmd TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "RespCmd",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_USHORT);
    if (memberTc == NULL)
    {
        printf("Get Member ValidLen TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "ValidLen",
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
        labels[0] = 256;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member ucData TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "ucData",
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

DDS_Long ProxyToServSerialize(const ProxyToServ* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->SysIdx, 2))
    {
        printf("serialize sample->SysIdx failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->RespCmd, 2))
    {
        printf("serialize sample->RespCmd failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->ValidLen, 2))
    {
        printf("serialize sample->ValidLen failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->ucData, 256, 1))
    {
        printf("serialize sample->ucData failed.");
        return -2;
    }

    return 0;
}

DDS_Long ProxyToServDeserialize(
    ProxyToServ* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->SysIdx, 2))
    {
        sample->SysIdx = 0;
        sample->RespCmd = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->RespCmd, 2))
    {
        sample->RespCmd = 0;
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->ValidLen, 2))
    {
        sample->ValidLen = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->ucData, 256, 1))
    {
        return 0;
    }
    return 0;
}

DDS_ULong ProxyToServGetSerializedSampleSize(const ProxyToServ* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(2, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);
    currentAlignment += 1 * 255;

    return currentAlignment - initialAlignment;
}

DDS_Long ProxyToServSerializeKey(const ProxyToServ* sample, CDRSerializer *cdr)
{
    if (ProxyToServSerialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long ProxyToServDeserializeKey(
    ProxyToServ* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (ProxyToServDeserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong ProxyToServGetSerializedKeySize(const ProxyToServ* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += ProxyToServGetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ProxyToServLoanSampleBuf(ProxyToServ* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void ProxyToServReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long ProxyToServLoanDeserialize(ProxyToServ* sampleBuf,
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
DDS_Long ProxyToServOnSiteDeserialize(CDRDeserializer* cdr,
    ProxyToServ* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean ProxyToServNoSerializingSupported()
{
    return false;
}

DDS_ULong ProxyToServFixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
