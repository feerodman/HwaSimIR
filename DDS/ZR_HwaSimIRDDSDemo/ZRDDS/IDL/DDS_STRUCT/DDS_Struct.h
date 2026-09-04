#ifndef DDS_Struct_H_
#define DDS_Struct_H_

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


typedef struct ServToProxy
{
    DDS_UShort SysIdx; // @ID(0)
    DDS_UShort ReqCmd; // @ID(1)
    DDS_ULong evtype; // @ID(2)
    DDS_ULong evtsub; // @ID(3)
    DDS_UShort HaveTimeOut; // @ID(4)
    DDS_UShort ValidLen; // @ID(5)
    DDS_Octet ucData[256]; // @ID(6)
} ServToProxy; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(ServToProxySeq, ServToProxy);

// 用户使用接口
DDS_Boolean ServToProxyInitialize(
    ServToProxy* self);

DDS_Boolean ServToProxyInitializeEx(
    ServToProxy* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void ServToProxyFinalize(
    ServToProxy* self);

void ServToProxyFinalizeEx(
    ServToProxy* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean ServToProxyCopy(
    ServToProxy* dst,
    const ServToProxy* src);

DDS_Boolean ServToProxyCopyEx(
    ServToProxy* dst,
    const ServToProxy* src,
    ZRMemPool* pool);

void ServToProxyPrintData(
    const ServToProxy* sample);

DDS::TypeCode* ServToProxyGetTypeCode();

// 底层使用函数
ServToProxy* ServToProxyCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void ServToProxyDestroySample(
    ZRMemPool* pool,
    ServToProxy* sample);

DDS_ULong ServToProxyGetSerializedSampleMaxSize();

DDS_ULong ServToProxyGetSerializedSampleSize(
    const ServToProxy* sample,
    DDS_ULong currentAlignment);

DDS_Long ServToProxySerialize(
    const ServToProxy* sample,
    CDRSerializer* cdr);

DDS_Long ServToProxyDeserialize(
    ServToProxy* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong ServToProxyGetSerializedKeyMaxSize();

DDS_ULong ServToProxyGetSerializedKeySize(
    const ServToProxy* sample,
    DDS_ULong currentAlignment);

DDS_Long ServToProxySerializeKey(
    const ServToProxy* sample,
    CDRSerializer* cdr);

DDS_Long ServToProxyDeserializeKey(
    ServToProxy* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long ServToProxyGetKeyHash(
    const ServToProxy* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean ServToProxyHasKey();

TypeCodeHeader* ServToProxyGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean ServToProxyNoSerializingSupported();

DDS_ULong ServToProxyFixedHeaderLength();

DDS_Long ServToProxyOnSiteDeserialize(CDRDeserializer* cdr,
    ServToProxy* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ServToProxyLoanSampleBuf(ServToProxy* sample, DDS_Boolean takeBuffer);

void ServToProxyReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long ServToProxyLoanDeserialize(ServToProxy* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct ProxyToServ
{
    DDS_UShort SysIdx; // @ID(0)
    DDS_UShort RespCmd; // @ID(1)
    DDS_UShort ValidLen; // @ID(2)
    DDS_Octet ucData[256]; // @ID(3)
} ProxyToServ; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(ProxyToServSeq, ProxyToServ);

// 用户使用接口
DDS_Boolean ProxyToServInitialize(
    ProxyToServ* self);

DDS_Boolean ProxyToServInitializeEx(
    ProxyToServ* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void ProxyToServFinalize(
    ProxyToServ* self);

void ProxyToServFinalizeEx(
    ProxyToServ* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean ProxyToServCopy(
    ProxyToServ* dst,
    const ProxyToServ* src);

DDS_Boolean ProxyToServCopyEx(
    ProxyToServ* dst,
    const ProxyToServ* src,
    ZRMemPool* pool);

void ProxyToServPrintData(
    const ProxyToServ* sample);

DDS::TypeCode* ProxyToServGetTypeCode();

// 底层使用函数
ProxyToServ* ProxyToServCreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void ProxyToServDestroySample(
    ZRMemPool* pool,
    ProxyToServ* sample);

DDS_ULong ProxyToServGetSerializedSampleMaxSize();

DDS_ULong ProxyToServGetSerializedSampleSize(
    const ProxyToServ* sample,
    DDS_ULong currentAlignment);

DDS_Long ProxyToServSerialize(
    const ProxyToServ* sample,
    CDRSerializer* cdr);

DDS_Long ProxyToServDeserialize(
    ProxyToServ* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong ProxyToServGetSerializedKeyMaxSize();

DDS_ULong ProxyToServGetSerializedKeySize(
    const ProxyToServ* sample,
    DDS_ULong currentAlignment);

DDS_Long ProxyToServSerializeKey(
    const ProxyToServ* sample,
    CDRSerializer* cdr);

DDS_Long ProxyToServDeserializeKey(
    ProxyToServ* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long ProxyToServGetKeyHash(
    const ProxyToServ* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean ProxyToServHasKey();

TypeCodeHeader* ProxyToServGetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean ProxyToServNoSerializingSupported();

DDS_ULong ProxyToServFixedHeaderLength();

DDS_Long ProxyToServOnSiteDeserialize(CDRDeserializer* cdr,
    ProxyToServ* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ProxyToServLoanSampleBuf(ProxyToServ* sample, DDS_Boolean takeBuffer);

void ProxyToServReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long ProxyToServLoanDeserialize(ProxyToServ* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
#endif
