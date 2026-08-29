#ifndef HwaSimIRProtocolV1_H_
#define HwaSimIRProtocolV1_H_

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


namespace HwaSimIRDds
{
typedef struct SpatialStateV1
{
    DDS_Double lat; // @ID(0)
    DDS_Double lon; // @ID(1)
    DDS_Double alt; // @ID(2)
    DDS_Double yaw; // @ID(3)
    DDS_Double pitch; // @ID(4)
    DDS_Double roll; // @ID(5)
    DDS_Double speed; // @ID(6)
} SpatialStateV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(SpatialStateV1Seq, SpatialStateV1);

// 用户使用接口
DDS_Boolean SpatialStateV1Initialize(
    SpatialStateV1* self);

DDS_Boolean SpatialStateV1InitializeEx(
    SpatialStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void SpatialStateV1Finalize(
    SpatialStateV1* self);

void SpatialStateV1FinalizeEx(
    SpatialStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean SpatialStateV1Copy(
    SpatialStateV1* dst,
    const SpatialStateV1* src);

DDS_Boolean SpatialStateV1CopyEx(
    SpatialStateV1* dst,
    const SpatialStateV1* src,
    ZRMemPool* pool);

void SpatialStateV1PrintData(
    const SpatialStateV1* sample);

DDS::TypeCode* SpatialStateV1GetTypeCode();

// 底层使用函数
SpatialStateV1* SpatialStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void SpatialStateV1DestroySample(
    ZRMemPool* pool,
    SpatialStateV1* sample);

DDS_ULong SpatialStateV1GetSerializedSampleMaxSize();

DDS_ULong SpatialStateV1GetSerializedSampleSize(
    const SpatialStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long SpatialStateV1Serialize(
    const SpatialStateV1* sample,
    CDRSerializer* cdr);

DDS_Long SpatialStateV1Deserialize(
    SpatialStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong SpatialStateV1GetSerializedKeyMaxSize();

DDS_ULong SpatialStateV1GetSerializedKeySize(
    const SpatialStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long SpatialStateV1SerializeKey(
    const SpatialStateV1* sample,
    CDRSerializer* cdr);

DDS_Long SpatialStateV1DeserializeKey(
    SpatialStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long SpatialStateV1GetKeyHash(
    const SpatialStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean SpatialStateV1HasKey();

TypeCodeHeader* SpatialStateV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean SpatialStateV1NoSerializingSupported();

DDS_ULong SpatialStateV1FixedHeaderLength();

DDS_Long SpatialStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    SpatialStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* SpatialStateV1LoanSampleBuf(SpatialStateV1* sample, DDS_Boolean takeBuffer);

void SpatialStateV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long SpatialStateV1LoanDeserialize(SpatialStateV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct TrackerSensorParamV1
{
    DDS_Boolean h264En; // @ID(0)
    DDS_Boolean noiseEn; // @ID(1)
    DDS_Double trackerSensorNoise; // @ID(2)
    DDS_Boolean realtimeAnnotation; // @ID(3)
    DDS_Boolean saveMP4En; // @ID(4)
    DDS_Long trackerSensorBand; // @ID(5)
    DDS_Long trackerSensorWidth; // @ID(6)
    DDS_Long trackerSensorHeight; // @ID(7)
    DDS_Long trackerSensorViewMin; // @ID(8)
    DDS_Long trackerSensorViewMax; // @ID(9)
    DDS_Double trackerSensorPixelAngle; // @ID(10)
    DDS_Double trackerX; // @ID(11)
    DDS_Double trackerY; // @ID(12)
    DDS_Double trackerZ; // @ID(13)
    DDS_Double trackerPitch; // @ID(14)
    DDS_Double trackerYaw; // @ID(15)
    DDS_Double trackerRoll; // @ID(16)
    DDS_Double illuminatorX; // @ID(17)
    DDS_Double illuminatorY; // @ID(18)
    DDS_Double illuminatorZ; // @ID(19)
    DDS_Double illuminatorPitch; // @ID(20)
    DDS_Double illuminatorYaw; // @ID(21)
    DDS_Double illuminatorRoll; // @ID(22)
    DDS_Double illuminatorAngle; // @ID(23)
    DDS_Double illuminatorSpotRad; // @ID(24)
    DDS_Long emitterSpotRadius; // @ID(25)
    DDS_Double emitterSpotRad; // @ID(26)
} TrackerSensorParamV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(TrackerSensorParamV1Seq, TrackerSensorParamV1);

// 用户使用接口
DDS_Boolean TrackerSensorParamV1Initialize(
    TrackerSensorParamV1* self);

DDS_Boolean TrackerSensorParamV1InitializeEx(
    TrackerSensorParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void TrackerSensorParamV1Finalize(
    TrackerSensorParamV1* self);

void TrackerSensorParamV1FinalizeEx(
    TrackerSensorParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean TrackerSensorParamV1Copy(
    TrackerSensorParamV1* dst,
    const TrackerSensorParamV1* src);

DDS_Boolean TrackerSensorParamV1CopyEx(
    TrackerSensorParamV1* dst,
    const TrackerSensorParamV1* src,
    ZRMemPool* pool);

void TrackerSensorParamV1PrintData(
    const TrackerSensorParamV1* sample);

DDS::TypeCode* TrackerSensorParamV1GetTypeCode();

// 底层使用函数
TrackerSensorParamV1* TrackerSensorParamV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void TrackerSensorParamV1DestroySample(
    ZRMemPool* pool,
    TrackerSensorParamV1* sample);

DDS_ULong TrackerSensorParamV1GetSerializedSampleMaxSize();

DDS_ULong TrackerSensorParamV1GetSerializedSampleSize(
    const TrackerSensorParamV1* sample,
    DDS_ULong currentAlignment);

DDS_Long TrackerSensorParamV1Serialize(
    const TrackerSensorParamV1* sample,
    CDRSerializer* cdr);

DDS_Long TrackerSensorParamV1Deserialize(
    TrackerSensorParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong TrackerSensorParamV1GetSerializedKeyMaxSize();

DDS_ULong TrackerSensorParamV1GetSerializedKeySize(
    const TrackerSensorParamV1* sample,
    DDS_ULong currentAlignment);

DDS_Long TrackerSensorParamV1SerializeKey(
    const TrackerSensorParamV1* sample,
    CDRSerializer* cdr);

DDS_Long TrackerSensorParamV1DeserializeKey(
    TrackerSensorParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long TrackerSensorParamV1GetKeyHash(
    const TrackerSensorParamV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean TrackerSensorParamV1HasKey();

TypeCodeHeader* TrackerSensorParamV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean TrackerSensorParamV1NoSerializingSupported();

DDS_ULong TrackerSensorParamV1FixedHeaderLength();

DDS_Long TrackerSensorParamV1OnSiteDeserialize(CDRDeserializer* cdr,
    TrackerSensorParamV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TrackerSensorParamV1LoanSampleBuf(TrackerSensorParamV1* sample, DDS_Boolean takeBuffer);

void TrackerSensorParamV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long TrackerSensorParamV1LoanDeserialize(TrackerSensorParamV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct InitObjectTrackingParamV1
{
    DDS_Boolean enable; // @ID(0)
    DDS_Long envTerrain; // @ID(1)
    DDS_Long envSky; // @ID(2)
    DDS_Double envMaxHeightRain; // @ID(3)
    DDS_Double envTransHeightRain; // @ID(4)
    DDS_Double envMaxHeightSnow; // @ID(5)
    DDS_Double envTransHeightSnow; // @ID(6)
    DDS_Double envRainSnowSpeedScale; // @ID(7)
    DDS_Double envRadScaleTerrain; // @ID(8)
    DDS_Double envRadScaleSky; // @ID(9)
    DDS_Double envTemp; // @ID(10)
    DDS_Double envHumidity; // @ID(11)
    DDS_Double envVisibility; // @ID(12)
    DDS_Double envWindV; // @ID(13)
    DDS_Double envWindDir; // @ID(14)
    DDS_Long simMode; // @ID(15)
    DDS_Long videoFps; // @ID(16)
    HwaSimIRDds::TrackerSensorParamV1 trackerSensor[1]; // @ID(17)
} InitObjectTrackingParamV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(InitObjectTrackingParamV1Seq, InitObjectTrackingParamV1);

// 用户使用接口
DDS_Boolean InitObjectTrackingParamV1Initialize(
    InitObjectTrackingParamV1* self);

DDS_Boolean InitObjectTrackingParamV1InitializeEx(
    InitObjectTrackingParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void InitObjectTrackingParamV1Finalize(
    InitObjectTrackingParamV1* self);

void InitObjectTrackingParamV1FinalizeEx(
    InitObjectTrackingParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean InitObjectTrackingParamV1Copy(
    InitObjectTrackingParamV1* dst,
    const InitObjectTrackingParamV1* src);

DDS_Boolean InitObjectTrackingParamV1CopyEx(
    InitObjectTrackingParamV1* dst,
    const InitObjectTrackingParamV1* src,
    ZRMemPool* pool);

void InitObjectTrackingParamV1PrintData(
    const InitObjectTrackingParamV1* sample);

DDS::TypeCode* InitObjectTrackingParamV1GetTypeCode();

// 底层使用函数
InitObjectTrackingParamV1* InitObjectTrackingParamV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void InitObjectTrackingParamV1DestroySample(
    ZRMemPool* pool,
    InitObjectTrackingParamV1* sample);

DDS_ULong InitObjectTrackingParamV1GetSerializedSampleMaxSize();

DDS_ULong InitObjectTrackingParamV1GetSerializedSampleSize(
    const InitObjectTrackingParamV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitObjectTrackingParamV1Serialize(
    const InitObjectTrackingParamV1* sample,
    CDRSerializer* cdr);

DDS_Long InitObjectTrackingParamV1Deserialize(
    InitObjectTrackingParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong InitObjectTrackingParamV1GetSerializedKeyMaxSize();

DDS_ULong InitObjectTrackingParamV1GetSerializedKeySize(
    const InitObjectTrackingParamV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitObjectTrackingParamV1SerializeKey(
    const InitObjectTrackingParamV1* sample,
    CDRSerializer* cdr);

DDS_Long InitObjectTrackingParamV1DeserializeKey(
    InitObjectTrackingParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long InitObjectTrackingParamV1GetKeyHash(
    const InitObjectTrackingParamV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean InitObjectTrackingParamV1HasKey();

TypeCodeHeader* InitObjectTrackingParamV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean InitObjectTrackingParamV1NoSerializingSupported();

DDS_ULong InitObjectTrackingParamV1FixedHeaderLength();

DDS_Long InitObjectTrackingParamV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitObjectTrackingParamV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitObjectTrackingParamV1LoanSampleBuf(InitObjectTrackingParamV1* sample, DDS_Boolean takeBuffer);

void InitObjectTrackingParamV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long InitObjectTrackingParamV1LoanDeserialize(InitObjectTrackingParamV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct PlatParamPakV1
{
    DDS_Long id; // @ID(0)
    DDS_Long type; // @ID(1)
    HwaSimIRDds::SpatialStateV1 spatial; // @ID(2)
} PlatParamPakV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(PlatParamPakV1Seq, PlatParamPakV1);

// 用户使用接口
DDS_Boolean PlatParamPakV1Initialize(
    PlatParamPakV1* self);

DDS_Boolean PlatParamPakV1InitializeEx(
    PlatParamPakV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void PlatParamPakV1Finalize(
    PlatParamPakV1* self);

void PlatParamPakV1FinalizeEx(
    PlatParamPakV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean PlatParamPakV1Copy(
    PlatParamPakV1* dst,
    const PlatParamPakV1* src);

DDS_Boolean PlatParamPakV1CopyEx(
    PlatParamPakV1* dst,
    const PlatParamPakV1* src,
    ZRMemPool* pool);

void PlatParamPakV1PrintData(
    const PlatParamPakV1* sample);

DDS::TypeCode* PlatParamPakV1GetTypeCode();

// 底层使用函数
PlatParamPakV1* PlatParamPakV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void PlatParamPakV1DestroySample(
    ZRMemPool* pool,
    PlatParamPakV1* sample);

DDS_ULong PlatParamPakV1GetSerializedSampleMaxSize();

DDS_ULong PlatParamPakV1GetSerializedSampleSize(
    const PlatParamPakV1* sample,
    DDS_ULong currentAlignment);

DDS_Long PlatParamPakV1Serialize(
    const PlatParamPakV1* sample,
    CDRSerializer* cdr);

DDS_Long PlatParamPakV1Deserialize(
    PlatParamPakV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong PlatParamPakV1GetSerializedKeyMaxSize();

DDS_ULong PlatParamPakV1GetSerializedKeySize(
    const PlatParamPakV1* sample,
    DDS_ULong currentAlignment);

DDS_Long PlatParamPakV1SerializeKey(
    const PlatParamPakV1* sample,
    CDRSerializer* cdr);

DDS_Long PlatParamPakV1DeserializeKey(
    PlatParamPakV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long PlatParamPakV1GetKeyHash(
    const PlatParamPakV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean PlatParamPakV1HasKey();

TypeCodeHeader* PlatParamPakV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean PlatParamPakV1NoSerializingSupported();

DDS_ULong PlatParamPakV1FixedHeaderLength();

DDS_Long PlatParamPakV1OnSiteDeserialize(CDRDeserializer* cdr,
    PlatParamPakV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* PlatParamPakV1LoanSampleBuf(PlatParamPakV1* sample, DDS_Boolean takeBuffer);

void PlatParamPakV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long PlatParamPakV1LoanDeserialize(PlatParamPakV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct WeaponStateV1
{
    DDS_Long targetType; // @ID(0)
    DDS_Long targetPlatID; // @ID(1)
    DDS_Long targetID; // @ID(2)
    DDS_Double xxOutAng[2]; // @ID(3)
    DDS_Boolean lookatEn; // @ID(4)
    DDS_Boolean illuminatorEn; // @ID(5)
    DDS_Double offsetAng[2]; // @ID(6)
    DDS_Boolean viewValid; // @ID(7)
    DDS_Long damageFlag; // @ID(8)
    DDS_Boolean strikeFlag; // @ID(9)
    DDS_Long strikePart; // @ID(10)
} WeaponStateV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(WeaponStateV1Seq, WeaponStateV1);

// 用户使用接口
DDS_Boolean WeaponStateV1Initialize(
    WeaponStateV1* self);

DDS_Boolean WeaponStateV1InitializeEx(
    WeaponStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void WeaponStateV1Finalize(
    WeaponStateV1* self);

void WeaponStateV1FinalizeEx(
    WeaponStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean WeaponStateV1Copy(
    WeaponStateV1* dst,
    const WeaponStateV1* src);

DDS_Boolean WeaponStateV1CopyEx(
    WeaponStateV1* dst,
    const WeaponStateV1* src,
    ZRMemPool* pool);

void WeaponStateV1PrintData(
    const WeaponStateV1* sample);

DDS::TypeCode* WeaponStateV1GetTypeCode();

// 底层使用函数
WeaponStateV1* WeaponStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void WeaponStateV1DestroySample(
    ZRMemPool* pool,
    WeaponStateV1* sample);

DDS_ULong WeaponStateV1GetSerializedSampleMaxSize();

DDS_ULong WeaponStateV1GetSerializedSampleSize(
    const WeaponStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long WeaponStateV1Serialize(
    const WeaponStateV1* sample,
    CDRSerializer* cdr);

DDS_Long WeaponStateV1Deserialize(
    WeaponStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong WeaponStateV1GetSerializedKeyMaxSize();

DDS_ULong WeaponStateV1GetSerializedKeySize(
    const WeaponStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long WeaponStateV1SerializeKey(
    const WeaponStateV1* sample,
    CDRSerializer* cdr);

DDS_Long WeaponStateV1DeserializeKey(
    WeaponStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long WeaponStateV1GetKeyHash(
    const WeaponStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean WeaponStateV1HasKey();

TypeCodeHeader* WeaponStateV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean WeaponStateV1NoSerializingSupported();

DDS_ULong WeaponStateV1FixedHeaderLength();

DDS_Long WeaponStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    WeaponStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* WeaponStateV1LoanSampleBuf(WeaponStateV1* sample, DDS_Boolean takeBuffer);

void WeaponStateV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long WeaponStateV1LoanDeserialize(WeaponStateV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct TargetStateV1
{
    DDS_Long targetType; // @ID(0)
    DDS_Long targetPlatID; // @ID(1)
    DDS_Long targetID; // @ID(2)
    DDS_Boolean engineState; // @ID(3)
    DDS_Boolean viewValid; // @ID(4)
    HwaSimIRDds::SpatialStateV1 targetLoc; // @ID(5)
    DDS_Long targetState; // @ID(6)
} TargetStateV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(TargetStateV1Seq, TargetStateV1);

// 用户使用接口
DDS_Boolean TargetStateV1Initialize(
    TargetStateV1* self);

DDS_Boolean TargetStateV1InitializeEx(
    TargetStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void TargetStateV1Finalize(
    TargetStateV1* self);

void TargetStateV1FinalizeEx(
    TargetStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean TargetStateV1Copy(
    TargetStateV1* dst,
    const TargetStateV1* src);

DDS_Boolean TargetStateV1CopyEx(
    TargetStateV1* dst,
    const TargetStateV1* src,
    ZRMemPool* pool);

void TargetStateV1PrintData(
    const TargetStateV1* sample);

DDS::TypeCode* TargetStateV1GetTypeCode();

// 底层使用函数
TargetStateV1* TargetStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void TargetStateV1DestroySample(
    ZRMemPool* pool,
    TargetStateV1* sample);

DDS_ULong TargetStateV1GetSerializedSampleMaxSize();

DDS_ULong TargetStateV1GetSerializedSampleSize(
    const TargetStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long TargetStateV1Serialize(
    const TargetStateV1* sample,
    CDRSerializer* cdr);

DDS_Long TargetStateV1Deserialize(
    TargetStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong TargetStateV1GetSerializedKeyMaxSize();

DDS_ULong TargetStateV1GetSerializedKeySize(
    const TargetStateV1* sample,
    DDS_ULong currentAlignment);

DDS_Long TargetStateV1SerializeKey(
    const TargetStateV1* sample,
    CDRSerializer* cdr);

DDS_Long TargetStateV1DeserializeKey(
    TargetStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long TargetStateV1GetKeyHash(
    const TargetStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean TargetStateV1HasKey();

TypeCodeHeader* TargetStateV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean TargetStateV1NoSerializingSupported();

DDS_ULong TargetStateV1FixedHeaderLength();

DDS_Long TargetStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    TargetStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TargetStateV1LoanSampleBuf(TargetStateV1* sample, DDS_Boolean takeBuffer);

void TargetStateV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long TargetStateV1LoanDeserialize(TargetStateV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct ControlCommandV1
{
    DDS_Long flag; // @ID(0)
    DDS_Long JB; // @ID(1)
    DDS_Long platID; // @ID(2) @Key
    DDS_Long simCommand; // @ID(3)
    DDS_Long roundCut; // @ID(4)
    DDS_Long currentRound; // @ID(5)
} ControlCommandV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(ControlCommandV1Seq, ControlCommandV1);

// 用户使用接口
DDS_Boolean ControlCommandV1Initialize(
    ControlCommandV1* self);

DDS_Boolean ControlCommandV1InitializeEx(
    ControlCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void ControlCommandV1Finalize(
    ControlCommandV1* self);

void ControlCommandV1FinalizeEx(
    ControlCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean ControlCommandV1Copy(
    ControlCommandV1* dst,
    const ControlCommandV1* src);

DDS_Boolean ControlCommandV1CopyEx(
    ControlCommandV1* dst,
    const ControlCommandV1* src,
    ZRMemPool* pool);

void ControlCommandV1PrintData(
    const ControlCommandV1* sample);

DDS::TypeCode* ControlCommandV1GetTypeCode();

// 底层使用函数
ControlCommandV1* ControlCommandV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void ControlCommandV1DestroySample(
    ZRMemPool* pool,
    ControlCommandV1* sample);

DDS_ULong ControlCommandV1GetSerializedSampleMaxSize();

DDS_ULong ControlCommandV1GetSerializedSampleSize(
    const ControlCommandV1* sample,
    DDS_ULong currentAlignment);

DDS_Long ControlCommandV1Serialize(
    const ControlCommandV1* sample,
    CDRSerializer* cdr);

DDS_Long ControlCommandV1Deserialize(
    ControlCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong ControlCommandV1GetSerializedKeyMaxSize();

DDS_ULong ControlCommandV1GetSerializedKeySize(
    const ControlCommandV1* sample,
    DDS_ULong currentAlignment);

DDS_Long ControlCommandV1SerializeKey(
    const ControlCommandV1* sample,
    CDRSerializer* cdr);

DDS_Long ControlCommandV1DeserializeKey(
    ControlCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long ControlCommandV1GetKeyHash(
    const ControlCommandV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean ControlCommandV1HasKey();

TypeCodeHeader* ControlCommandV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean ControlCommandV1NoSerializingSupported();

DDS_ULong ControlCommandV1FixedHeaderLength();

DDS_Long ControlCommandV1OnSiteDeserialize(CDRDeserializer* cdr,
    ControlCommandV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ControlCommandV1LoanSampleBuf(ControlCommandV1* sample, DDS_Boolean takeBuffer);

void ControlCommandV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long ControlCommandV1LoanDeserialize(ControlCommandV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct InitCommandV1
{
    DDS_Long flag; // @ID(0)
    DDS_Long JB; // @ID(1)
    DDS_Long platID; // @ID(2) @Key
    DDS_Long sensorID; // @ID(3) @Key
    HwaSimIRDds::PlatParamPakV1 platParamInit; // @ID(4)
    HwaSimIRDds::InitObjectTrackingParamV1 trackingInit; // @ID(5)
    DDS_Long MissileMaxCount120; // @ID(6)
    DDS_Long MissileMaxCount9; // @ID(7)
    DDS_Long MissileMaxCountMMD; // @ID(8)
    DDS_Long MissileMaxCountF35; // @ID(9)
    DDS_Long MissileMaxCountF22; // @ID(10)
    DDS_Long MissileMaxCountResv1; // @ID(11)
    DDS_Long MissileMaxCountResv2; // @ID(12)
} InitCommandV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(InitCommandV1Seq, InitCommandV1);

// 用户使用接口
DDS_Boolean InitCommandV1Initialize(
    InitCommandV1* self);

DDS_Boolean InitCommandV1InitializeEx(
    InitCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void InitCommandV1Finalize(
    InitCommandV1* self);

void InitCommandV1FinalizeEx(
    InitCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean InitCommandV1Copy(
    InitCommandV1* dst,
    const InitCommandV1* src);

DDS_Boolean InitCommandV1CopyEx(
    InitCommandV1* dst,
    const InitCommandV1* src,
    ZRMemPool* pool);

void InitCommandV1PrintData(
    const InitCommandV1* sample);

DDS::TypeCode* InitCommandV1GetTypeCode();

// 底层使用函数
InitCommandV1* InitCommandV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void InitCommandV1DestroySample(
    ZRMemPool* pool,
    InitCommandV1* sample);

DDS_ULong InitCommandV1GetSerializedSampleMaxSize();

DDS_ULong InitCommandV1GetSerializedSampleSize(
    const InitCommandV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitCommandV1Serialize(
    const InitCommandV1* sample,
    CDRSerializer* cdr);

DDS_Long InitCommandV1Deserialize(
    InitCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong InitCommandV1GetSerializedKeyMaxSize();

DDS_ULong InitCommandV1GetSerializedKeySize(
    const InitCommandV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitCommandV1SerializeKey(
    const InitCommandV1* sample,
    CDRSerializer* cdr);

DDS_Long InitCommandV1DeserializeKey(
    InitCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long InitCommandV1GetKeyHash(
    const InitCommandV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean InitCommandV1HasKey();

TypeCodeHeader* InitCommandV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean InitCommandV1NoSerializingSupported();

DDS_ULong InitCommandV1FixedHeaderLength();

DDS_Long InitCommandV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitCommandV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitCommandV1LoanSampleBuf(InitCommandV1* sample, DDS_Boolean takeBuffer);

void InitCommandV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long InitCommandV1LoanDeserialize(InitCommandV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct RealtimeDataV1
{
    DDS_Long flag; // @ID(0)
    DDS_Long platID; // @ID(1) @Key
    DDS_Long sensorID; // @ID(2) @Key
    DDS_Double time; // @ID(3)
    HwaSimIRDds::SpatialStateV1 platLoc; // @ID(4)
    HwaSimIRDds::WeaponStateV1 weaponState; // @ID(5)
    DDS_Long targetNumValid; // @ID(6)
    HwaSimIRDds::TargetStateV1 targetState[5]; // @ID(7)
} RealtimeDataV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(RealtimeDataV1Seq, RealtimeDataV1);

// 用户使用接口
DDS_Boolean RealtimeDataV1Initialize(
    RealtimeDataV1* self);

DDS_Boolean RealtimeDataV1InitializeEx(
    RealtimeDataV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void RealtimeDataV1Finalize(
    RealtimeDataV1* self);

void RealtimeDataV1FinalizeEx(
    RealtimeDataV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean RealtimeDataV1Copy(
    RealtimeDataV1* dst,
    const RealtimeDataV1* src);

DDS_Boolean RealtimeDataV1CopyEx(
    RealtimeDataV1* dst,
    const RealtimeDataV1* src,
    ZRMemPool* pool);

void RealtimeDataV1PrintData(
    const RealtimeDataV1* sample);

DDS::TypeCode* RealtimeDataV1GetTypeCode();

// 底层使用函数
RealtimeDataV1* RealtimeDataV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void RealtimeDataV1DestroySample(
    ZRMemPool* pool,
    RealtimeDataV1* sample);

DDS_ULong RealtimeDataV1GetSerializedSampleMaxSize();

DDS_ULong RealtimeDataV1GetSerializedSampleSize(
    const RealtimeDataV1* sample,
    DDS_ULong currentAlignment);

DDS_Long RealtimeDataV1Serialize(
    const RealtimeDataV1* sample,
    CDRSerializer* cdr);

DDS_Long RealtimeDataV1Deserialize(
    RealtimeDataV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong RealtimeDataV1GetSerializedKeyMaxSize();

DDS_ULong RealtimeDataV1GetSerializedKeySize(
    const RealtimeDataV1* sample,
    DDS_ULong currentAlignment);

DDS_Long RealtimeDataV1SerializeKey(
    const RealtimeDataV1* sample,
    CDRSerializer* cdr);

DDS_Long RealtimeDataV1DeserializeKey(
    RealtimeDataV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long RealtimeDataV1GetKeyHash(
    const RealtimeDataV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean RealtimeDataV1HasKey();

TypeCodeHeader* RealtimeDataV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean RealtimeDataV1NoSerializingSupported();

DDS_ULong RealtimeDataV1FixedHeaderLength();

DDS_Long RealtimeDataV1OnSiteDeserialize(CDRDeserializer* cdr,
    RealtimeDataV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* RealtimeDataV1LoanSampleBuf(RealtimeDataV1* sample, DDS_Boolean takeBuffer);

void RealtimeDataV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long RealtimeDataV1LoanDeserialize(RealtimeDataV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct InitAckV1
{
    DDS_Long flag; // @ID(0)
    DDS_Long JB; // @ID(1)
    DDS_Long platID; // @ID(2) @Key
    DDS_Long sensorID; // @ID(3) @Key
    DDS_Boolean trackingReady; // @ID(4)
} InitAckV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(InitAckV1Seq, InitAckV1);

// 用户使用接口
DDS_Boolean InitAckV1Initialize(
    InitAckV1* self);

DDS_Boolean InitAckV1InitializeEx(
    InitAckV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void InitAckV1Finalize(
    InitAckV1* self);

void InitAckV1FinalizeEx(
    InitAckV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean InitAckV1Copy(
    InitAckV1* dst,
    const InitAckV1* src);

DDS_Boolean InitAckV1CopyEx(
    InitAckV1* dst,
    const InitAckV1* src,
    ZRMemPool* pool);

void InitAckV1PrintData(
    const InitAckV1* sample);

DDS::TypeCode* InitAckV1GetTypeCode();

// 底层使用函数
InitAckV1* InitAckV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void InitAckV1DestroySample(
    ZRMemPool* pool,
    InitAckV1* sample);

DDS_ULong InitAckV1GetSerializedSampleMaxSize();

DDS_ULong InitAckV1GetSerializedSampleSize(
    const InitAckV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitAckV1Serialize(
    const InitAckV1* sample,
    CDRSerializer* cdr);

DDS_Long InitAckV1Deserialize(
    InitAckV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong InitAckV1GetSerializedKeyMaxSize();

DDS_ULong InitAckV1GetSerializedKeySize(
    const InitAckV1* sample,
    DDS_ULong currentAlignment);

DDS_Long InitAckV1SerializeKey(
    const InitAckV1* sample,
    CDRSerializer* cdr);

DDS_Long InitAckV1DeserializeKey(
    InitAckV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long InitAckV1GetKeyHash(
    const InitAckV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean InitAckV1HasKey();

TypeCodeHeader* InitAckV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean InitAckV1NoSerializingSupported();

DDS_ULong InitAckV1FixedHeaderLength();

DDS_Long InitAckV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitAckV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitAckV1LoanSampleBuf(InitAckV1* sample, DDS_Boolean takeBuffer);

void InitAckV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long InitAckV1LoanDeserialize(InitAckV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
typedef struct VideoStatusV1
{
    DDS_Long platID; // @ID(0) @Key
    DDS_Long sensorID; // @ID(1) @Key
    DDS_Char* channel; // @ID(2) /* maximum length = (16) */
    DDS_Boolean running; // @ID(3)
    DDS_Char* codec; // @ID(4) /* maximum length = (24) */
    DDS_Char* pixelFormat; // @ID(5) /* maximum length = (24) */
    DDS_Char* videoTopic; // @ID(6) /* maximum length = (128) */
    DDS_Long width; // @ID(7)
    DDS_Long height; // @ID(8)
    DDS_Long fps; // @ID(9)
    DDS_Long bitrateKbps; // @ID(10)
    DDS_Long gopFrames; // @ID(11)
    DDS_Boolean compressed; // @ID(12)
    DDS_Long currentRound; // @ID(13)
} VideoStatusV1; // @Extensibility(EXTENSIBLE)

DDS_USER_SEQUENCE_CPP(VideoStatusV1Seq, VideoStatusV1);

// 用户使用接口
DDS_Boolean VideoStatusV1Initialize(
    VideoStatusV1* self);

DDS_Boolean VideoStatusV1InitializeEx(
    VideoStatusV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory);

void VideoStatusV1Finalize(
    VideoStatusV1* self);

void VideoStatusV1FinalizeEx(
    VideoStatusV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers);

DDS_Boolean VideoStatusV1Copy(
    VideoStatusV1* dst,
    const VideoStatusV1* src);

DDS_Boolean VideoStatusV1CopyEx(
    VideoStatusV1* dst,
    const VideoStatusV1* src,
    ZRMemPool* pool);

void VideoStatusV1PrintData(
    const VideoStatusV1* sample);

DDS::TypeCode* VideoStatusV1GetTypeCode();

// 底层使用函数
VideoStatusV1* VideoStatusV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable);

void VideoStatusV1DestroySample(
    ZRMemPool* pool,
    VideoStatusV1* sample);

DDS_ULong VideoStatusV1GetSerializedSampleMaxSize();

DDS_ULong VideoStatusV1GetSerializedSampleSize(
    const VideoStatusV1* sample,
    DDS_ULong currentAlignment);

DDS_Long VideoStatusV1Serialize(
    const VideoStatusV1* sample,
    CDRSerializer* cdr);

DDS_Long VideoStatusV1Deserialize(
    VideoStatusV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_ULong VideoStatusV1GetSerializedKeyMaxSize();

DDS_ULong VideoStatusV1GetSerializedKeySize(
    const VideoStatusV1* sample,
    DDS_ULong currentAlignment);

DDS_Long VideoStatusV1SerializeKey(
    const VideoStatusV1* sample,
    CDRSerializer* cdr);

DDS_Long VideoStatusV1DeserializeKey(
    VideoStatusV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool);

DDS_Long VideoStatusV1GetKeyHash(
    const VideoStatusV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result);

DDS_Boolean VideoStatusV1HasKey();

TypeCodeHeader* VideoStatusV1GetInnerTypeCode();

#ifdef _ZRDDS_INCLUDE_ONSITE_DESERILIZE
DDS_Boolean VideoStatusV1NoSerializingSupported();

DDS_ULong VideoStatusV1FixedHeaderLength();

DDS_Long VideoStatusV1OnSiteDeserialize(CDRDeserializer* cdr,
    VideoStatusV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* VideoStatusV1LoanSampleBuf(VideoStatusV1* sample, DDS_Boolean takeBuffer);

void VideoStatusV1ReturnSampleBuf(DDS_Char* sampleBuf);

DDS_Long VideoStatusV1LoanDeserialize(VideoStatusV1* sampleBuf,
    CDRDeserializer* cdr,
    DDS_ULong curIndex,
    DDS_ULong totalNum,
    DDS_Char* base,
    DDS_ULong offset,
    DDS_ULong space,
    DDS_ULong fixedHeaderLen);

#endif/*_ZRDDS_INCLUDE_NO_SERIALIZE_MODE*/
}
#endif
