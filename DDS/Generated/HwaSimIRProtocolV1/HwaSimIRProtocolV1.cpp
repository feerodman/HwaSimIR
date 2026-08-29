/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/
#include "ZRMemPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HwaSimIRProtocolV1.h"

namespace HwaSimIRDds
{

#define T SpatialStateV1
#define TSeq SpatialStateV1Seq
#define TINITIALIZE SpatialStateV1InitializeEx
#define TFINALIZE SpatialStateV1FinalizeEx
#define TCOPY SpatialStateV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean SpatialStateV1Initialize(SpatialStateV1* self)
{
    return SpatialStateV1InitializeEx(self, NULL, true);
}

void SpatialStateV1Finalize(SpatialStateV1* self)
{
    SpatialStateV1FinalizeEx(self, NULL, true);
}

DDS_Boolean SpatialStateV1Copy(
    SpatialStateV1* dst,
    const SpatialStateV1* src)
{
    return SpatialStateV1CopyEx(dst, src, NULL);
}

SpatialStateV1* SpatialStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    SpatialStateV1* newSample = (SpatialStateV1*)ZRMalloc(pool, sizeof(SpatialStateV1));
    if (newSample == NULL)
    {
        printf("malloc for SpatialStateV1 failed.");
        return NULL;
    }
    if (!SpatialStateV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        SpatialStateV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void SpatialStateV1DestroySample(ZRMemPool* pool, SpatialStateV1* sample)
{
    if (sample == NULL) return;
    SpatialStateV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong SpatialStateV1GetSerializedSampleMaxSize()
{
    return 56;
}

DDS_ULong SpatialStateV1GetSerializedKeyMaxSize()
{
    return 56;
}

DDS_Long SpatialStateV1GetKeyHash(
    const SpatialStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = SpatialStateV1SerializeKey(sample, cdr);
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

DDS_Boolean SpatialStateV1HasKey()
{
    return false;
}

TypeCodeHeader* SpatialStateV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = SpatialStateV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean SpatialStateV1InitializeEx(
    SpatialStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->lat = 0;

    self->lon = 0;

    self->alt = 0;

    self->yaw = 0;

    self->pitch = 0;

    self->roll = 0;

    self->speed = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void SpatialStateV1FinalizeEx(
    SpatialStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean SpatialStateV1CopyEx(
    SpatialStateV1* dst,
    const SpatialStateV1* src,
    ZRMemPool* pool)
{
    dst->lat = src->lat;
    dst->lon = src->lon;
    dst->alt = src->alt;
    dst->yaw = src->yaw;
    dst->pitch = src->pitch;
    dst->roll = src->roll;
    dst->speed = src->speed;
    return true;
}

void SpatialStateV1PrintData(const SpatialStateV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->lat: %lf\n", sample->lat);
    printf("\n");

    printf("sample->lon: %lf\n", sample->lon);
    printf("\n");

    printf("sample->alt: %lf\n", sample->alt);
    printf("\n");

    printf("sample->yaw: %lf\n", sample->yaw);
    printf("\n");

    printf("sample->pitch: %lf\n", sample->pitch);
    printf("\n");

    printf("sample->roll: %lf\n", sample->roll);
    printf("\n");

    printf("sample->speed: %lf\n", sample->speed);
    printf("\n");

}

DDS::TypeCode* SpatialStateV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::SpatialStateV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct SpatialStateV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member lat TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "lat",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member lon TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "lon",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member alt TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "alt",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member yaw TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "yaw",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member pitch TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "pitch",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member roll TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "roll",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member speed TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "speed",
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

DDS_Long SpatialStateV1Serialize(const SpatialStateV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->lat, 8))
    {
        printf("serialize sample->lat failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->lon, 8))
    {
        printf("serialize sample->lon failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->alt, 8))
    {
        printf("serialize sample->alt failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->yaw, 8))
    {
        printf("serialize sample->yaw failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->pitch, 8))
    {
        printf("serialize sample->pitch failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->roll, 8))
    {
        printf("serialize sample->roll failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->speed, 8))
    {
        printf("serialize sample->speed failed.");
        return -2;
    }

    return 0;
}

DDS_Long SpatialStateV1Deserialize(
    SpatialStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->lat, 8))
    {
        sample->lat = 0;
        sample->lon = 0;
        sample->alt = 0;
        sample->yaw = 0;
        sample->pitch = 0;
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->lon, 8))
    {
        sample->lon = 0;
        sample->alt = 0;
        sample->yaw = 0;
        sample->pitch = 0;
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->alt, 8))
    {
        sample->alt = 0;
        sample->yaw = 0;
        sample->pitch = 0;
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->yaw, 8))
    {
        sample->yaw = 0;
        sample->pitch = 0;
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->pitch, 8))
    {
        sample->pitch = 0;
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->roll, 8))
    {
        sample->roll = 0;
        sample->speed = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->speed, 8))
    {
        sample->speed = 0;
        return 0;
    }
    return 0;
}

DDS_ULong SpatialStateV1GetSerializedSampleSize(const SpatialStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long SpatialStateV1SerializeKey(const SpatialStateV1* sample, CDRSerializer *cdr)
{
    if (SpatialStateV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long SpatialStateV1DeserializeKey(
    SpatialStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (SpatialStateV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong SpatialStateV1GetSerializedKeySize(const SpatialStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += SpatialStateV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* SpatialStateV1LoanSampleBuf(SpatialStateV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void SpatialStateV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long SpatialStateV1LoanDeserialize(SpatialStateV1* sampleBuf,
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
DDS_Long SpatialStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    SpatialStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean SpatialStateV1NoSerializingSupported()
{
    return false;
}

DDS_ULong SpatialStateV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T TrackerSensorParamV1
#define TSeq TrackerSensorParamV1Seq
#define TINITIALIZE TrackerSensorParamV1InitializeEx
#define TFINALIZE TrackerSensorParamV1FinalizeEx
#define TCOPY TrackerSensorParamV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean TrackerSensorParamV1Initialize(TrackerSensorParamV1* self)
{
    return TrackerSensorParamV1InitializeEx(self, NULL, true);
}

void TrackerSensorParamV1Finalize(TrackerSensorParamV1* self)
{
    TrackerSensorParamV1FinalizeEx(self, NULL, true);
}

DDS_Boolean TrackerSensorParamV1Copy(
    TrackerSensorParamV1* dst,
    const TrackerSensorParamV1* src)
{
    return TrackerSensorParamV1CopyEx(dst, src, NULL);
}

TrackerSensorParamV1* TrackerSensorParamV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    TrackerSensorParamV1* newSample = (TrackerSensorParamV1*)ZRMalloc(pool, sizeof(TrackerSensorParamV1));
    if (newSample == NULL)
    {
        printf("malloc for TrackerSensorParamV1 failed.");
        return NULL;
    }
    if (!TrackerSensorParamV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        TrackerSensorParamV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void TrackerSensorParamV1DestroySample(ZRMemPool* pool, TrackerSensorParamV1* sample)
{
    if (sample == NULL) return;
    TrackerSensorParamV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong TrackerSensorParamV1GetSerializedSampleMaxSize()
{
    return 176;
}

DDS_ULong TrackerSensorParamV1GetSerializedKeyMaxSize()
{
    return 176;
}

DDS_Long TrackerSensorParamV1GetKeyHash(
    const TrackerSensorParamV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = TrackerSensorParamV1SerializeKey(sample, cdr);
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

DDS_Boolean TrackerSensorParamV1HasKey()
{
    return false;
}

TypeCodeHeader* TrackerSensorParamV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = TrackerSensorParamV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean TrackerSensorParamV1InitializeEx(
    TrackerSensorParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->h264En = 0;

    self->noiseEn = 0;

    self->trackerSensorNoise = 0;

    self->realtimeAnnotation = 0;

    self->saveMP4En = 0;

    self->trackerSensorBand = 0;

    self->trackerSensorWidth = 0;

    self->trackerSensorHeight = 0;

    self->trackerSensorViewMin = 0;

    self->trackerSensorViewMax = 0;

    self->trackerSensorPixelAngle = 0;

    self->trackerX = 0;

    self->trackerY = 0;

    self->trackerZ = 0;

    self->trackerPitch = 0;

    self->trackerYaw = 0;

    self->trackerRoll = 0;

    self->illuminatorX = 0;

    self->illuminatorY = 0;

    self->illuminatorZ = 0;

    self->illuminatorPitch = 0;

    self->illuminatorYaw = 0;

    self->illuminatorRoll = 0;

    self->illuminatorAngle = 0;

    self->illuminatorSpotRad = 0;

    self->emitterSpotRadius = 0;

    self->emitterSpotRad = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void TrackerSensorParamV1FinalizeEx(
    TrackerSensorParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean TrackerSensorParamV1CopyEx(
    TrackerSensorParamV1* dst,
    const TrackerSensorParamV1* src,
    ZRMemPool* pool)
{
    dst->h264En = src->h264En;
    dst->noiseEn = src->noiseEn;
    dst->trackerSensorNoise = src->trackerSensorNoise;
    dst->realtimeAnnotation = src->realtimeAnnotation;
    dst->saveMP4En = src->saveMP4En;
    dst->trackerSensorBand = src->trackerSensorBand;
    dst->trackerSensorWidth = src->trackerSensorWidth;
    dst->trackerSensorHeight = src->trackerSensorHeight;
    dst->trackerSensorViewMin = src->trackerSensorViewMin;
    dst->trackerSensorViewMax = src->trackerSensorViewMax;
    dst->trackerSensorPixelAngle = src->trackerSensorPixelAngle;
    dst->trackerX = src->trackerX;
    dst->trackerY = src->trackerY;
    dst->trackerZ = src->trackerZ;
    dst->trackerPitch = src->trackerPitch;
    dst->trackerYaw = src->trackerYaw;
    dst->trackerRoll = src->trackerRoll;
    dst->illuminatorX = src->illuminatorX;
    dst->illuminatorY = src->illuminatorY;
    dst->illuminatorZ = src->illuminatorZ;
    dst->illuminatorPitch = src->illuminatorPitch;
    dst->illuminatorYaw = src->illuminatorYaw;
    dst->illuminatorRoll = src->illuminatorRoll;
    dst->illuminatorAngle = src->illuminatorAngle;
    dst->illuminatorSpotRad = src->illuminatorSpotRad;
    dst->emitterSpotRadius = src->emitterSpotRadius;
    dst->emitterSpotRad = src->emitterSpotRad;
    return true;
}

void TrackerSensorParamV1PrintData(const TrackerSensorParamV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->h264En: %d\n", sample->h264En);
    printf("\n");

    printf("sample->noiseEn: %d\n", sample->noiseEn);
    printf("\n");

    printf("sample->trackerSensorNoise: %lf\n", sample->trackerSensorNoise);
    printf("\n");

    printf("sample->realtimeAnnotation: %d\n", sample->realtimeAnnotation);
    printf("\n");

    printf("sample->saveMP4En: %d\n", sample->saveMP4En);
    printf("\n");

    printf("sample->trackerSensorBand: %d\n", sample->trackerSensorBand);
    printf("\n");

    printf("sample->trackerSensorWidth: %d\n", sample->trackerSensorWidth);
    printf("\n");

    printf("sample->trackerSensorHeight: %d\n", sample->trackerSensorHeight);
    printf("\n");

    printf("sample->trackerSensorViewMin: %d\n", sample->trackerSensorViewMin);
    printf("\n");

    printf("sample->trackerSensorViewMax: %d\n", sample->trackerSensorViewMax);
    printf("\n");

    printf("sample->trackerSensorPixelAngle: %lf\n", sample->trackerSensorPixelAngle);
    printf("\n");

    printf("sample->trackerX: %lf\n", sample->trackerX);
    printf("\n");

    printf("sample->trackerY: %lf\n", sample->trackerY);
    printf("\n");

    printf("sample->trackerZ: %lf\n", sample->trackerZ);
    printf("\n");

    printf("sample->trackerPitch: %lf\n", sample->trackerPitch);
    printf("\n");

    printf("sample->trackerYaw: %lf\n", sample->trackerYaw);
    printf("\n");

    printf("sample->trackerRoll: %lf\n", sample->trackerRoll);
    printf("\n");

    printf("sample->illuminatorX: %lf\n", sample->illuminatorX);
    printf("\n");

    printf("sample->illuminatorY: %lf\n", sample->illuminatorY);
    printf("\n");

    printf("sample->illuminatorZ: %lf\n", sample->illuminatorZ);
    printf("\n");

    printf("sample->illuminatorPitch: %lf\n", sample->illuminatorPitch);
    printf("\n");

    printf("sample->illuminatorYaw: %lf\n", sample->illuminatorYaw);
    printf("\n");

    printf("sample->illuminatorRoll: %lf\n", sample->illuminatorRoll);
    printf("\n");

    printf("sample->illuminatorAngle: %lf\n", sample->illuminatorAngle);
    printf("\n");

    printf("sample->illuminatorSpotRad: %lf\n", sample->illuminatorSpotRad);
    printf("\n");

    printf("sample->emitterSpotRadius: %d\n", sample->emitterSpotRadius);
    printf("\n");

    printf("sample->emitterSpotRad: %lf\n", sample->emitterSpotRad);
    printf("\n");

}

DDS::TypeCode* TrackerSensorParamV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::TrackerSensorParamV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct TrackerSensorParamV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member h264En TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "h264En",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member noiseEn TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "noiseEn",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerSensorNoise TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "trackerSensorNoise",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member realtimeAnnotation TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "realtimeAnnotation",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member saveMP4En TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "saveMP4En",
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
        printf("Get Member trackerSensorBand TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "trackerSensorBand",
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
        printf("Get Member trackerSensorWidth TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "trackerSensorWidth",
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
        printf("Get Member trackerSensorHeight TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "trackerSensorHeight",
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
        printf("Get Member trackerSensorViewMin TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "trackerSensorViewMin",
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
        printf("Get Member trackerSensorViewMax TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "trackerSensorViewMax",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerSensorPixelAngle TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        10,
        10,
        "trackerSensorPixelAngle",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerX TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        11,
        11,
        "trackerX",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerY TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        12,
        12,
        "trackerY",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerZ TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        13,
        13,
        "trackerZ",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerPitch TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        14,
        14,
        "trackerPitch",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerYaw TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        15,
        15,
        "trackerYaw",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member trackerRoll TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        16,
        16,
        "trackerRoll",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorX TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        17,
        17,
        "illuminatorX",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorY TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        18,
        18,
        "illuminatorY",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorZ TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        19,
        19,
        "illuminatorZ",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorPitch TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        20,
        20,
        "illuminatorPitch",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorYaw TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        21,
        21,
        "illuminatorYaw",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorRoll TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        22,
        22,
        "illuminatorRoll",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorAngle TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        23,
        23,
        "illuminatorAngle",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorSpotRad TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        24,
        24,
        "illuminatorSpotRad",
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
        printf("Get Member emitterSpotRadius TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        25,
        25,
        "emitterSpotRadius",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member emitterSpotRad TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        26,
        26,
        "emitterSpotRad",
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

DDS_Long TrackerSensorParamV1Serialize(const TrackerSensorParamV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->h264En, 1))
    {
        printf("serialize sample->h264En failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->noiseEn, 1))
    {
        printf("serialize sample->noiseEn failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorNoise, 8))
    {
        printf("serialize sample->trackerSensorNoise failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->realtimeAnnotation, 1))
    {
        printf("serialize sample->realtimeAnnotation failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->saveMP4En, 1))
    {
        printf("serialize sample->saveMP4En failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorBand, 4))
    {
        printf("serialize sample->trackerSensorBand failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorWidth, 4))
    {
        printf("serialize sample->trackerSensorWidth failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorHeight, 4))
    {
        printf("serialize sample->trackerSensorHeight failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorViewMin, 4))
    {
        printf("serialize sample->trackerSensorViewMin failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorViewMax, 4))
    {
        printf("serialize sample->trackerSensorViewMax failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerSensorPixelAngle, 8))
    {
        printf("serialize sample->trackerSensorPixelAngle failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerX, 8))
    {
        printf("serialize sample->trackerX failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerY, 8))
    {
        printf("serialize sample->trackerY failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerZ, 8))
    {
        printf("serialize sample->trackerZ failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerPitch, 8))
    {
        printf("serialize sample->trackerPitch failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerYaw, 8))
    {
        printf("serialize sample->trackerYaw failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackerRoll, 8))
    {
        printf("serialize sample->trackerRoll failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorX, 8))
    {
        printf("serialize sample->illuminatorX failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorY, 8))
    {
        printf("serialize sample->illuminatorY failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorZ, 8))
    {
        printf("serialize sample->illuminatorZ failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorPitch, 8))
    {
        printf("serialize sample->illuminatorPitch failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorYaw, 8))
    {
        printf("serialize sample->illuminatorYaw failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorRoll, 8))
    {
        printf("serialize sample->illuminatorRoll failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorAngle, 8))
    {
        printf("serialize sample->illuminatorAngle failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorSpotRad, 8))
    {
        printf("serialize sample->illuminatorSpotRad failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->emitterSpotRadius, 4))
    {
        printf("serialize sample->emitterSpotRadius failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->emitterSpotRad, 8))
    {
        printf("serialize sample->emitterSpotRad failed.");
        return -2;
    }

    return 0;
}

DDS_Long TrackerSensorParamV1Deserialize(
    TrackerSensorParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->h264En, 1))
    {
        sample->h264En = 0;
        sample->noiseEn = 0;
        sample->trackerSensorNoise = 0;
        sample->realtimeAnnotation = 0;
        sample->saveMP4En = 0;
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->noiseEn, 1))
    {
        sample->noiseEn = 0;
        sample->trackerSensorNoise = 0;
        sample->realtimeAnnotation = 0;
        sample->saveMP4En = 0;
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorNoise, 8))
    {
        sample->trackerSensorNoise = 0;
        sample->realtimeAnnotation = 0;
        sample->saveMP4En = 0;
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->realtimeAnnotation, 1))
    {
        sample->realtimeAnnotation = 0;
        sample->saveMP4En = 0;
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->saveMP4En, 1))
    {
        sample->saveMP4En = 0;
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorBand, 4))
    {
        sample->trackerSensorBand = 0;
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorWidth, 4))
    {
        sample->trackerSensorWidth = 0;
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorHeight, 4))
    {
        sample->trackerSensorHeight = 0;
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorViewMin, 4))
    {
        sample->trackerSensorViewMin = 0;
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorViewMax, 4))
    {
        sample->trackerSensorViewMax = 0;
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerSensorPixelAngle, 8))
    {
        sample->trackerSensorPixelAngle = 0;
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerX, 8))
    {
        sample->trackerX = 0;
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerY, 8))
    {
        sample->trackerY = 0;
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerZ, 8))
    {
        sample->trackerZ = 0;
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerPitch, 8))
    {
        sample->trackerPitch = 0;
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerYaw, 8))
    {
        sample->trackerYaw = 0;
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackerRoll, 8))
    {
        sample->trackerRoll = 0;
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorX, 8))
    {
        sample->illuminatorX = 0;
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorY, 8))
    {
        sample->illuminatorY = 0;
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorZ, 8))
    {
        sample->illuminatorZ = 0;
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorPitch, 8))
    {
        sample->illuminatorPitch = 0;
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorYaw, 8))
    {
        sample->illuminatorYaw = 0;
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorRoll, 8))
    {
        sample->illuminatorRoll = 0;
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorAngle, 8))
    {
        sample->illuminatorAngle = 0;
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorSpotRad, 8))
    {
        sample->illuminatorSpotRad = 0;
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->emitterSpotRadius, 4))
    {
        sample->emitterSpotRadius = 0;
        sample->emitterSpotRad = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->emitterSpotRad, 8))
    {
        sample->emitterSpotRad = 0;
        return 0;
    }
    return 0;
}

DDS_ULong TrackerSensorParamV1GetSerializedSampleSize(const TrackerSensorParamV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long TrackerSensorParamV1SerializeKey(const TrackerSensorParamV1* sample, CDRSerializer *cdr)
{
    if (TrackerSensorParamV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long TrackerSensorParamV1DeserializeKey(
    TrackerSensorParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (TrackerSensorParamV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong TrackerSensorParamV1GetSerializedKeySize(const TrackerSensorParamV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += TrackerSensorParamV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TrackerSensorParamV1LoanSampleBuf(TrackerSensorParamV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void TrackerSensorParamV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long TrackerSensorParamV1LoanDeserialize(TrackerSensorParamV1* sampleBuf,
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
DDS_Long TrackerSensorParamV1OnSiteDeserialize(CDRDeserializer* cdr,
    TrackerSensorParamV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean TrackerSensorParamV1NoSerializingSupported()
{
    return false;
}

DDS_ULong TrackerSensorParamV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T InitObjectTrackingParamV1
#define TSeq InitObjectTrackingParamV1Seq
#define TINITIALIZE InitObjectTrackingParamV1InitializeEx
#define TFINALIZE InitObjectTrackingParamV1FinalizeEx
#define TCOPY InitObjectTrackingParamV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean InitObjectTrackingParamV1Initialize(InitObjectTrackingParamV1* self)
{
    return InitObjectTrackingParamV1InitializeEx(self, NULL, true);
}

void InitObjectTrackingParamV1Finalize(InitObjectTrackingParamV1* self)
{
    InitObjectTrackingParamV1FinalizeEx(self, NULL, true);
}

DDS_Boolean InitObjectTrackingParamV1Copy(
    InitObjectTrackingParamV1* dst,
    const InitObjectTrackingParamV1* src)
{
    return InitObjectTrackingParamV1CopyEx(dst, src, NULL);
}

InitObjectTrackingParamV1* InitObjectTrackingParamV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    InitObjectTrackingParamV1* newSample = (InitObjectTrackingParamV1*)ZRMalloc(pool, sizeof(InitObjectTrackingParamV1));
    if (newSample == NULL)
    {
        printf("malloc for InitObjectTrackingParamV1 failed.");
        return NULL;
    }
    if (!InitObjectTrackingParamV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        InitObjectTrackingParamV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void InitObjectTrackingParamV1DestroySample(ZRMemPool* pool, InitObjectTrackingParamV1* sample)
{
    if (sample == NULL) return;
    InitObjectTrackingParamV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong InitObjectTrackingParamV1GetSerializedSampleMaxSize()
{
    return 296;
}

DDS_ULong InitObjectTrackingParamV1GetSerializedKeyMaxSize()
{
    return 296;
}

DDS_Long InitObjectTrackingParamV1GetKeyHash(
    const InitObjectTrackingParamV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = InitObjectTrackingParamV1SerializeKey(sample, cdr);
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

DDS_Boolean InitObjectTrackingParamV1HasKey()
{
    return false;
}

TypeCodeHeader* InitObjectTrackingParamV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = InitObjectTrackingParamV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean InitObjectTrackingParamV1InitializeEx(
    InitObjectTrackingParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->enable = 0;

    self->envTerrain = 0;

    self->envSky = 0;

    self->envMaxHeightRain = 0;

    self->envTransHeightRain = 0;

    self->envMaxHeightSnow = 0;

    self->envTransHeightSnow = 0;

    self->envRainSnowSpeedScale = 0;

    self->envRadScaleTerrain = 0;

    self->envRadScaleSky = 0;

    self->envTemp = 0;

    self->envHumidity = 0;

    self->envVisibility = 0;

    self->envWindV = 0;

    self->envWindDir = 0;

    self->simMode = 0;

    self->videoFps = 0;

    HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) self->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, allocateMemory))
        {
            printf("Initialize member self->trackerSensor failed.");
            return false;
        }
    }

    if (allocateMemory)
    {
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
        }
    }
    else
    {
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
        }
    }
    return true;
}

void InitObjectTrackingParamV1FinalizeEx(
    InitObjectTrackingParamV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) self->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            HwaSimIRDds::TrackerSensorParamV1FinalizeEx(&trackerSensorArray[trackerSensorIndex], pool, deletePointers);
        }
    }
}

DDS_Boolean InitObjectTrackingParamV1CopyEx(
    InitObjectTrackingParamV1* dst,
    const InitObjectTrackingParamV1* src,
    ZRMemPool* pool)
{
    dst->enable = src->enable;
    dst->envTerrain = src->envTerrain;
    dst->envSky = src->envSky;
    dst->envMaxHeightRain = src->envMaxHeightRain;
    dst->envTransHeightRain = src->envTransHeightRain;
    dst->envMaxHeightSnow = src->envMaxHeightSnow;
    dst->envTransHeightSnow = src->envTransHeightSnow;
    dst->envRainSnowSpeedScale = src->envRainSnowSpeedScale;
    dst->envRadScaleTerrain = src->envRadScaleTerrain;
    dst->envRadScaleSky = src->envRadScaleSky;
    dst->envTemp = src->envTemp;
    dst->envHumidity = src->envHumidity;
    dst->envVisibility = src->envVisibility;
    dst->envWindV = src->envWindV;
    dst->envWindDir = src->envWindDir;
    dst->simMode = src->simMode;
    dst->videoFps = src->videoFps;
    HwaSimIRDds::TrackerSensorParamV1* trackerSensorSrcArray = (HwaSimIRDds::TrackerSensorParamV1*) src->trackerSensor;
    HwaSimIRDds::TrackerSensorParamV1* trackerSensorDstArray = (HwaSimIRDds::TrackerSensorParamV1*) dst->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        if (!HwaSimIRDds::TrackerSensorParamV1CopyEx(&trackerSensorDstArray[trackerSensorIndex], &trackerSensorSrcArray[trackerSensorIndex], pool))
        {
            printf("copy member trackerSensor failed.");
            return false;
        }
    }
    return true;
}

void InitObjectTrackingParamV1PrintData(const InitObjectTrackingParamV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->enable: %d\n", sample->enable);
    printf("\n");

    printf("sample->envTerrain: %d\n", sample->envTerrain);
    printf("\n");

    printf("sample->envSky: %d\n", sample->envSky);
    printf("\n");

    printf("sample->envMaxHeightRain: %lf\n", sample->envMaxHeightRain);
    printf("\n");

    printf("sample->envTransHeightRain: %lf\n", sample->envTransHeightRain);
    printf("\n");

    printf("sample->envMaxHeightSnow: %lf\n", sample->envMaxHeightSnow);
    printf("\n");

    printf("sample->envTransHeightSnow: %lf\n", sample->envTransHeightSnow);
    printf("\n");

    printf("sample->envRainSnowSpeedScale: %lf\n", sample->envRainSnowSpeedScale);
    printf("\n");

    printf("sample->envRadScaleTerrain: %lf\n", sample->envRadScaleTerrain);
    printf("\n");

    printf("sample->envRadScaleSky: %lf\n", sample->envRadScaleSky);
    printf("\n");

    printf("sample->envTemp: %lf\n", sample->envTemp);
    printf("\n");

    printf("sample->envHumidity: %lf\n", sample->envHumidity);
    printf("\n");

    printf("sample->envVisibility: %lf\n", sample->envVisibility);
    printf("\n");

    printf("sample->envWindV: %lf\n", sample->envWindV);
    printf("\n");

    printf("sample->envWindDir: %lf\n", sample->envWindDir);
    printf("\n");

    printf("sample->simMode: %d\n", sample->simMode);
    printf("\n");

    printf("sample->videoFps: %d\n", sample->videoFps);
    printf("\n");

    HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        HwaSimIRDds::TrackerSensorParamV1PrintData(&trackerSensorArray[trackerSensorIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* InitObjectTrackingParamV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::InitObjectTrackingParamV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct InitObjectTrackingParamV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member enable TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "enable",
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
        printf("Get Member envTerrain TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "envTerrain",
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
        printf("Get Member envSky TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "envSky",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envMaxHeightRain TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "envMaxHeightRain",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envTransHeightRain TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "envTransHeightRain",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envMaxHeightSnow TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "envMaxHeightSnow",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envTransHeightSnow TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "envTransHeightSnow",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envRainSnowSpeedScale TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "envRainSnowSpeedScale",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envRadScaleTerrain TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "envRadScaleTerrain",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envRadScaleSky TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "envRadScaleSky",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envTemp TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        10,
        10,
        "envTemp",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envHumidity TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        11,
        11,
        "envHumidity",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envVisibility TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        12,
        12,
        "envVisibility",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envWindV TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        13,
        13,
        "envWindV",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member envWindDir TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        14,
        14,
        "envWindDir",
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
        printf("Get Member simMode TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        15,
        15,
        "simMode",
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
        printf("Get Member videoFps TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        16,
        16,
        "videoFps",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::TrackerSensorParamV1GetTypeCode();
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 1;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member trackerSensor TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        17,
        17,
        "trackerSensor",
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

DDS_Long InitObjectTrackingParamV1Serialize(const InitObjectTrackingParamV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->enable, 1))
    {
        printf("serialize sample->enable failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envTerrain, 4))
    {
        printf("serialize sample->envTerrain failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envSky, 4))
    {
        printf("serialize sample->envSky failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envMaxHeightRain, 8))
    {
        printf("serialize sample->envMaxHeightRain failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envTransHeightRain, 8))
    {
        printf("serialize sample->envTransHeightRain failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envMaxHeightSnow, 8))
    {
        printf("serialize sample->envMaxHeightSnow failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envTransHeightSnow, 8))
    {
        printf("serialize sample->envTransHeightSnow failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envRainSnowSpeedScale, 8))
    {
        printf("serialize sample->envRainSnowSpeedScale failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envRadScaleTerrain, 8))
    {
        printf("serialize sample->envRadScaleTerrain failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envRadScaleSky, 8))
    {
        printf("serialize sample->envRadScaleSky failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envTemp, 8))
    {
        printf("serialize sample->envTemp failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envHumidity, 8))
    {
        printf("serialize sample->envHumidity failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envVisibility, 8))
    {
        printf("serialize sample->envVisibility failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envWindV, 8))
    {
        printf("serialize sample->envWindV failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->envWindDir, 8))
    {
        printf("serialize sample->envWindDir failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->simMode, 4))
    {
        printf("serialize sample->simMode failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->videoFps, 4))
    {
        printf("serialize sample->videoFps failed.");
        return -2;
    }

    HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        if (HwaSimIRDds::TrackerSensorParamV1Serialize(&trackerSensorArray[trackerSensorIndex], cdr) < 0)
        {
            printf("serialize sample->trackerSensor failed.");
            return -2;
        }
    }

    return 0;
}

DDS_Long InitObjectTrackingParamV1Deserialize(
    InitObjectTrackingParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->enable, 1))
    {
        sample->enable = 0;
        sample->envTerrain = 0;
        sample->envSky = 0;
        sample->envMaxHeightRain = 0;
        sample->envTransHeightRain = 0;
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envTerrain, 4))
    {
        sample->envTerrain = 0;
        sample->envSky = 0;
        sample->envMaxHeightRain = 0;
        sample->envTransHeightRain = 0;
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envSky, 4))
    {
        sample->envSky = 0;
        sample->envMaxHeightRain = 0;
        sample->envTransHeightRain = 0;
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envMaxHeightRain, 8))
    {
        sample->envMaxHeightRain = 0;
        sample->envTransHeightRain = 0;
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envTransHeightRain, 8))
    {
        sample->envTransHeightRain = 0;
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envMaxHeightSnow, 8))
    {
        sample->envMaxHeightSnow = 0;
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envTransHeightSnow, 8))
    {
        sample->envTransHeightSnow = 0;
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envRainSnowSpeedScale, 8))
    {
        sample->envRainSnowSpeedScale = 0;
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envRadScaleTerrain, 8))
    {
        sample->envRadScaleTerrain = 0;
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envRadScaleSky, 8))
    {
        sample->envRadScaleSky = 0;
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envTemp, 8))
    {
        sample->envTemp = 0;
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envHumidity, 8))
    {
        sample->envHumidity = 0;
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envVisibility, 8))
    {
        sample->envVisibility = 0;
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envWindV, 8))
    {
        sample->envWindV = 0;
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->envWindDir, 8))
    {
        sample->envWindDir = 0;
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->simMode, 4))
    {
        sample->simMode = 0;
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->videoFps, 4))
    {
        sample->videoFps = 0;
        HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
        for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
        {
            if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
            {
                printf("Initialize member sample->trackerSensor failed.");
                return -2;
            }
        }
        return 0;
    }
    HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        if (HwaSimIRDds::TrackerSensorParamV1Deserialize(&trackerSensorArray[trackerSensorIndex], cdr, pool) < 0)
        {
            HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
            for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
            {
                if (!HwaSimIRDds::TrackerSensorParamV1InitializeEx(&trackerSensorArray[trackerSensorIndex], pool, true))
                {
                    printf("Initialize member sample->trackerSensor failed.");
                    return -2;
                }
            }
            return 0;
        }
    }
    return 0;
}

DDS_ULong InitObjectTrackingParamV1GetSerializedSampleSize(const InitObjectTrackingParamV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    HwaSimIRDds::TrackerSensorParamV1* trackerSensorArray = (HwaSimIRDds::TrackerSensorParamV1*) sample->trackerSensor;
    for (DDS_ULong trackerSensorIndex = 0; trackerSensorIndex < 1; ++trackerSensorIndex)
    {
        currentAlignment += HwaSimIRDds::TrackerSensorParamV1GetSerializedSampleSize(&trackerSensorArray[trackerSensorIndex], currentAlignment);
    }

    return currentAlignment - initialAlignment;
}

DDS_Long InitObjectTrackingParamV1SerializeKey(const InitObjectTrackingParamV1* sample, CDRSerializer *cdr)
{
    if (InitObjectTrackingParamV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long InitObjectTrackingParamV1DeserializeKey(
    InitObjectTrackingParamV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (InitObjectTrackingParamV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong InitObjectTrackingParamV1GetSerializedKeySize(const InitObjectTrackingParamV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += InitObjectTrackingParamV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitObjectTrackingParamV1LoanSampleBuf(InitObjectTrackingParamV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void InitObjectTrackingParamV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long InitObjectTrackingParamV1LoanDeserialize(InitObjectTrackingParamV1* sampleBuf,
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
DDS_Long InitObjectTrackingParamV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitObjectTrackingParamV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean InitObjectTrackingParamV1NoSerializingSupported()
{
    return false;
}

DDS_ULong InitObjectTrackingParamV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T PlatParamPakV1
#define TSeq PlatParamPakV1Seq
#define TINITIALIZE PlatParamPakV1InitializeEx
#define TFINALIZE PlatParamPakV1FinalizeEx
#define TCOPY PlatParamPakV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean PlatParamPakV1Initialize(PlatParamPakV1* self)
{
    return PlatParamPakV1InitializeEx(self, NULL, true);
}

void PlatParamPakV1Finalize(PlatParamPakV1* self)
{
    PlatParamPakV1FinalizeEx(self, NULL, true);
}

DDS_Boolean PlatParamPakV1Copy(
    PlatParamPakV1* dst,
    const PlatParamPakV1* src)
{
    return PlatParamPakV1CopyEx(dst, src, NULL);
}

PlatParamPakV1* PlatParamPakV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    PlatParamPakV1* newSample = (PlatParamPakV1*)ZRMalloc(pool, sizeof(PlatParamPakV1));
    if (newSample == NULL)
    {
        printf("malloc for PlatParamPakV1 failed.");
        return NULL;
    }
    if (!PlatParamPakV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        PlatParamPakV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void PlatParamPakV1DestroySample(ZRMemPool* pool, PlatParamPakV1* sample)
{
    if (sample == NULL) return;
    PlatParamPakV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong PlatParamPakV1GetSerializedSampleMaxSize()
{
    return 64;
}

DDS_ULong PlatParamPakV1GetSerializedKeyMaxSize()
{
    return 64;
}

DDS_Long PlatParamPakV1GetKeyHash(
    const PlatParamPakV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = PlatParamPakV1SerializeKey(sample, cdr);
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

DDS_Boolean PlatParamPakV1HasKey()
{
    return false;
}

TypeCodeHeader* PlatParamPakV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = PlatParamPakV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean PlatParamPakV1InitializeEx(
    PlatParamPakV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->id = 0;

    self->type = 0;

    if (!HwaSimIRDds::SpatialStateV1InitializeEx(&self->spatial, pool, allocateMemory))
    {
        printf("Initialize member self->spatial failed.");
        return false;
    }

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void PlatParamPakV1FinalizeEx(
    PlatParamPakV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    HwaSimIRDds::SpatialStateV1FinalizeEx(&self->spatial, pool, deletePointers);
    if (deletePointers)
    {
    }
}

DDS_Boolean PlatParamPakV1CopyEx(
    PlatParamPakV1* dst,
    const PlatParamPakV1* src,
    ZRMemPool* pool)
{
    dst->id = src->id;
    dst->type = src->type;
    if (!HwaSimIRDds::SpatialStateV1CopyEx(&dst->spatial, &src->spatial, pool))
    {
        printf("copy member spatial failed.");
        return false;
    }
    return true;
}

void PlatParamPakV1PrintData(const PlatParamPakV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->id: %d\n", sample->id);
    printf("\n");

    printf("sample->type: %d\n", sample->type);
    printf("\n");

    HwaSimIRDds::SpatialStateV1PrintData(&sample->spatial);
    printf("\n");

}

DDS::TypeCode* PlatParamPakV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::PlatParamPakV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct PlatParamPakV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member id TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "id",
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

    memberTc = HwaSimIRDds::SpatialStateV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member spatial TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "spatial",
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

DDS_Long PlatParamPakV1Serialize(const PlatParamPakV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->id, 4))
    {
        printf("serialize sample->id failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->type, 4))
    {
        printf("serialize sample->type failed.");
        return -2;
    }

    if (HwaSimIRDds::SpatialStateV1Serialize(&sample->spatial, cdr) < 0)
    {
        printf("serialize sample->spatial failed.");
        return -2;
    }

    return 0;
}

DDS_Long PlatParamPakV1Deserialize(
    PlatParamPakV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->id, 4))
    {
        sample->id = 0;
        sample->type = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->spatial, pool, true))
        {
            printf("Initialize member sample->spatial failed.");
            return -2;
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->type, 4))
    {
        sample->type = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->spatial, pool, true))
        {
            printf("Initialize member sample->spatial failed.");
            return -2;
        }
        return 0;
    }
    if (HwaSimIRDds::SpatialStateV1Deserialize(&sample->spatial, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->spatial, pool, true))
        {
            printf("Initialize member sample->spatial failed.");
            return -2;
        }
        return 0;
    }
    return 0;
}

DDS_ULong PlatParamPakV1GetSerializedSampleSize(const PlatParamPakV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += HwaSimIRDds::SpatialStateV1GetSerializedSampleSize(&sample->spatial, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long PlatParamPakV1SerializeKey(const PlatParamPakV1* sample, CDRSerializer *cdr)
{
    if (PlatParamPakV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long PlatParamPakV1DeserializeKey(
    PlatParamPakV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (PlatParamPakV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong PlatParamPakV1GetSerializedKeySize(const PlatParamPakV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += PlatParamPakV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* PlatParamPakV1LoanSampleBuf(PlatParamPakV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void PlatParamPakV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long PlatParamPakV1LoanDeserialize(PlatParamPakV1* sampleBuf,
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
DDS_Long PlatParamPakV1OnSiteDeserialize(CDRDeserializer* cdr,
    PlatParamPakV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean PlatParamPakV1NoSerializingSupported()
{
    return false;
}

DDS_ULong PlatParamPakV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T WeaponStateV1
#define TSeq WeaponStateV1Seq
#define TINITIALIZE WeaponStateV1InitializeEx
#define TFINALIZE WeaponStateV1FinalizeEx
#define TCOPY WeaponStateV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean WeaponStateV1Initialize(WeaponStateV1* self)
{
    return WeaponStateV1InitializeEx(self, NULL, true);
}

void WeaponStateV1Finalize(WeaponStateV1* self)
{
    WeaponStateV1FinalizeEx(self, NULL, true);
}

DDS_Boolean WeaponStateV1Copy(
    WeaponStateV1* dst,
    const WeaponStateV1* src)
{
    return WeaponStateV1CopyEx(dst, src, NULL);
}

WeaponStateV1* WeaponStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    WeaponStateV1* newSample = (WeaponStateV1*)ZRMalloc(pool, sizeof(WeaponStateV1));
    if (newSample == NULL)
    {
        printf("malloc for WeaponStateV1 failed.");
        return NULL;
    }
    if (!WeaponStateV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        WeaponStateV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void WeaponStateV1DestroySample(ZRMemPool* pool, WeaponStateV1* sample)
{
    if (sample == NULL) return;
    WeaponStateV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong WeaponStateV1GetSerializedSampleMaxSize()
{
    return 72;
}

DDS_ULong WeaponStateV1GetSerializedKeyMaxSize()
{
    return 72;
}

DDS_Long WeaponStateV1GetKeyHash(
    const WeaponStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = WeaponStateV1SerializeKey(sample, cdr);
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

DDS_Boolean WeaponStateV1HasKey()
{
    return false;
}

TypeCodeHeader* WeaponStateV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = WeaponStateV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean WeaponStateV1InitializeEx(
    WeaponStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->targetType = 0;

    self->targetPlatID = 0;

    self->targetID = 0;


    self->lookatEn = 0;

    self->illuminatorEn = 0;


    self->viewValid = 0;

    self->damageFlag = 0;

    self->strikeFlag = 0;

    self->strikePart = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void WeaponStateV1FinalizeEx(
    WeaponStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean WeaponStateV1CopyEx(
    WeaponStateV1* dst,
    const WeaponStateV1* src,
    ZRMemPool* pool)
{
    dst->targetType = src->targetType;
    dst->targetPlatID = src->targetPlatID;
    dst->targetID = src->targetID;
    memcpy(dst->xxOutAng, src->xxOutAng, 8 * 2);
    dst->lookatEn = src->lookatEn;
    dst->illuminatorEn = src->illuminatorEn;
    memcpy(dst->offsetAng, src->offsetAng, 8 * 2);
    dst->viewValid = src->viewValid;
    dst->damageFlag = src->damageFlag;
    dst->strikeFlag = src->strikeFlag;
    dst->strikePart = src->strikePart;
    return true;
}

void WeaponStateV1PrintData(const WeaponStateV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->targetType: %d\n", sample->targetType);
    printf("\n");

    printf("sample->targetPlatID: %d\n", sample->targetPlatID);
    printf("\n");

    printf("sample->targetID: %d\n", sample->targetID);
    printf("\n");

    DDS_Double* xxOutAngArray = (DDS_Double*) sample->xxOutAng;
    for (DDS_ULong xxOutAngIndex = 0; xxOutAngIndex < 2; ++xxOutAngIndex)
    {
        printf("xxOutAngArray[%u]: %lf\n", xxOutAngIndex, xxOutAngArray[xxOutAngIndex]);
        printf("\n");
    }
    printf("\n");

    printf("sample->lookatEn: %d\n", sample->lookatEn);
    printf("\n");

    printf("sample->illuminatorEn: %d\n", sample->illuminatorEn);
    printf("\n");

    DDS_Double* offsetAngArray = (DDS_Double*) sample->offsetAng;
    for (DDS_ULong offsetAngIndex = 0; offsetAngIndex < 2; ++offsetAngIndex)
    {
        printf("offsetAngArray[%u]: %lf\n", offsetAngIndex, offsetAngArray[offsetAngIndex]);
        printf("\n");
    }
    printf("\n");

    printf("sample->viewValid: %d\n", sample->viewValid);
    printf("\n");

    printf("sample->damageFlag: %d\n", sample->damageFlag);
    printf("\n");

    printf("sample->strikeFlag: %d\n", sample->strikeFlag);
    printf("\n");

    printf("sample->strikePart: %d\n", sample->strikePart);
    printf("\n");

}

DDS::TypeCode* WeaponStateV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::WeaponStateV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct WeaponStateV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member targetType TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "targetType",
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
        printf("Get Member targetPlatID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "targetPlatID",
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
        printf("Get Member targetID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "targetID",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 2;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member xxOutAng TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "xxOutAng",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member lookatEn TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "lookatEn",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member illuminatorEn TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "illuminatorEn",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 2;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member offsetAng TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "offsetAng",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member viewValid TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "viewValid",
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
        printf("Get Member damageFlag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "damageFlag",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member strikeFlag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "strikeFlag",
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
        printf("Get Member strikePart TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        10,
        10,
        "strikePart",
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

DDS_Long WeaponStateV1Serialize(const WeaponStateV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetType, 4))
    {
        printf("serialize sample->targetType failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetPlatID, 4))
    {
        printf("serialize sample->targetPlatID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetID, 4))
    {
        printf("serialize sample->targetID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->xxOutAng, 2, 8))
    {
        printf("serialize sample->xxOutAng failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->lookatEn, 1))
    {
        printf("serialize sample->lookatEn failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->illuminatorEn, 1))
    {
        printf("serialize sample->illuminatorEn failed.");
        return -2;
    }

    if (!CDRSerializerPutUntypeArray(cdr, (const DDS_Octet*) sample->offsetAng, 2, 8))
    {
        printf("serialize sample->offsetAng failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->viewValid, 1))
    {
        printf("serialize sample->viewValid failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->damageFlag, 4))
    {
        printf("serialize sample->damageFlag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->strikeFlag, 1))
    {
        printf("serialize sample->strikeFlag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->strikePart, 4))
    {
        printf("serialize sample->strikePart failed.");
        return -2;
    }

    return 0;
}

DDS_Long WeaponStateV1Deserialize(
    WeaponStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetType, 4))
    {
        sample->targetType = 0;
        sample->targetPlatID = 0;
        sample->targetID = 0;
        sample->lookatEn = 0;
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetPlatID, 4))
    {
        sample->targetPlatID = 0;
        sample->targetID = 0;
        sample->lookatEn = 0;
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetID, 4))
    {
        sample->targetID = 0;
        sample->lookatEn = 0;
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->xxOutAng, 2, 8))
    {
        sample->lookatEn = 0;
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->lookatEn, 1))
    {
        sample->lookatEn = 0;
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->illuminatorEn, 1))
    {
        sample->illuminatorEn = 0;
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*) sample->offsetAng, 2, 8))
    {
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->viewValid, 1))
    {
        sample->viewValid = 0;
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->damageFlag, 4))
    {
        sample->damageFlag = 0;
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->strikeFlag, 1))
    {
        sample->strikeFlag = 0;
        sample->strikePart = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->strikePart, 4))
    {
        sample->strikePart = 0;
        return 0;
    }
    return 0;
}

DDS_ULong WeaponStateV1GetSerializedSampleSize(const WeaponStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);
    currentAlignment += 8 * 1;

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);
    currentAlignment += 8 * 1;

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long WeaponStateV1SerializeKey(const WeaponStateV1* sample, CDRSerializer *cdr)
{
    if (WeaponStateV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long WeaponStateV1DeserializeKey(
    WeaponStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (WeaponStateV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong WeaponStateV1GetSerializedKeySize(const WeaponStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += WeaponStateV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* WeaponStateV1LoanSampleBuf(WeaponStateV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void WeaponStateV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long WeaponStateV1LoanDeserialize(WeaponStateV1* sampleBuf,
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
DDS_Long WeaponStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    WeaponStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean WeaponStateV1NoSerializingSupported()
{
    return false;
}

DDS_ULong WeaponStateV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T TargetStateV1
#define TSeq TargetStateV1Seq
#define TINITIALIZE TargetStateV1InitializeEx
#define TFINALIZE TargetStateV1FinalizeEx
#define TCOPY TargetStateV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean TargetStateV1Initialize(TargetStateV1* self)
{
    return TargetStateV1InitializeEx(self, NULL, true);
}

void TargetStateV1Finalize(TargetStateV1* self)
{
    TargetStateV1FinalizeEx(self, NULL, true);
}

DDS_Boolean TargetStateV1Copy(
    TargetStateV1* dst,
    const TargetStateV1* src)
{
    return TargetStateV1CopyEx(dst, src, NULL);
}

TargetStateV1* TargetStateV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    TargetStateV1* newSample = (TargetStateV1*)ZRMalloc(pool, sizeof(TargetStateV1));
    if (newSample == NULL)
    {
        printf("malloc for TargetStateV1 failed.");
        return NULL;
    }
    if (!TargetStateV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        TargetStateV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void TargetStateV1DestroySample(ZRMemPool* pool, TargetStateV1* sample)
{
    if (sample == NULL) return;
    TargetStateV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong TargetStateV1GetSerializedSampleMaxSize()
{
    return 76;
}

DDS_ULong TargetStateV1GetSerializedKeyMaxSize()
{
    return 76;
}

DDS_Long TargetStateV1GetKeyHash(
    const TargetStateV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = TargetStateV1SerializeKey(sample, cdr);
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

DDS_Boolean TargetStateV1HasKey()
{
    return false;
}

TypeCodeHeader* TargetStateV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = TargetStateV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean TargetStateV1InitializeEx(
    TargetStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->targetType = 0;

    self->targetPlatID = 0;

    self->targetID = 0;

    self->engineState = 0;

    self->viewValid = 0;

    if (!HwaSimIRDds::SpatialStateV1InitializeEx(&self->targetLoc, pool, allocateMemory))
    {
        printf("Initialize member self->targetLoc failed.");
        return false;
    }

    self->targetState = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void TargetStateV1FinalizeEx(
    TargetStateV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    HwaSimIRDds::SpatialStateV1FinalizeEx(&self->targetLoc, pool, deletePointers);
    if (deletePointers)
    {
    }
}

DDS_Boolean TargetStateV1CopyEx(
    TargetStateV1* dst,
    const TargetStateV1* src,
    ZRMemPool* pool)
{
    dst->targetType = src->targetType;
    dst->targetPlatID = src->targetPlatID;
    dst->targetID = src->targetID;
    dst->engineState = src->engineState;
    dst->viewValid = src->viewValid;
    if (!HwaSimIRDds::SpatialStateV1CopyEx(&dst->targetLoc, &src->targetLoc, pool))
    {
        printf("copy member targetLoc failed.");
        return false;
    }
    dst->targetState = src->targetState;
    return true;
}

void TargetStateV1PrintData(const TargetStateV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->targetType: %d\n", sample->targetType);
    printf("\n");

    printf("sample->targetPlatID: %d\n", sample->targetPlatID);
    printf("\n");

    printf("sample->targetID: %d\n", sample->targetID);
    printf("\n");

    printf("sample->engineState: %d\n", sample->engineState);
    printf("\n");

    printf("sample->viewValid: %d\n", sample->viewValid);
    printf("\n");

    HwaSimIRDds::SpatialStateV1PrintData(&sample->targetLoc);
    printf("\n");

    printf("sample->targetState: %d\n", sample->targetState);
    printf("\n");

}

DDS::TypeCode* TargetStateV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::TargetStateV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct TargetStateV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member targetType TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "targetType",
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
        printf("Get Member targetPlatID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "targetPlatID",
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
        printf("Get Member targetID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "targetID",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member engineState TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "engineState",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member viewValid TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "viewValid",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::SpatialStateV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member targetLoc TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "targetLoc",
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
        printf("Get Member targetState TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "targetState",
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

DDS_Long TargetStateV1Serialize(const TargetStateV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetType, 4))
    {
        printf("serialize sample->targetType failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetPlatID, 4))
    {
        printf("serialize sample->targetPlatID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetID, 4))
    {
        printf("serialize sample->targetID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->engineState, 1))
    {
        printf("serialize sample->engineState failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->viewValid, 1))
    {
        printf("serialize sample->viewValid failed.");
        return -2;
    }

    if (HwaSimIRDds::SpatialStateV1Serialize(&sample->targetLoc, cdr) < 0)
    {
        printf("serialize sample->targetLoc failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetState, 4))
    {
        printf("serialize sample->targetState failed.");
        return -2;
    }

    return 0;
}

DDS_Long TargetStateV1Deserialize(
    TargetStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    // no key
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetType, 4))
    {
        sample->targetType = 0;
        sample->targetPlatID = 0;
        sample->targetID = 0;
        sample->engineState = 0;
        sample->viewValid = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetPlatID, 4))
    {
        sample->targetPlatID = 0;
        sample->targetID = 0;
        sample->engineState = 0;
        sample->viewValid = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetID, 4))
    {
        sample->targetID = 0;
        sample->engineState = 0;
        sample->viewValid = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->engineState, 1))
    {
        sample->engineState = 0;
        sample->viewValid = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->viewValid, 1))
    {
        sample->viewValid = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (HwaSimIRDds::SpatialStateV1Deserialize(&sample->targetLoc, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->targetLoc, pool, true))
        {
            printf("Initialize member sample->targetLoc failed.");
            return -2;
        }
        sample->targetState = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetState, 4))
    {
        sample->targetState = 0;
        return 0;
    }
    return 0;
}

DDS_ULong TargetStateV1GetSerializedSampleSize(const TargetStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += HwaSimIRDds::SpatialStateV1GetSerializedSampleSize(&sample->targetLoc, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long TargetStateV1SerializeKey(const TargetStateV1* sample, CDRSerializer *cdr)
{
    if (TargetStateV1Serialize(sample, cdr) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_Long TargetStateV1DeserializeKey(
    TargetStateV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (TargetStateV1Deserialize(sample, cdr, pool) < 0)
    {
        return -1;
    }
    return 0;
}

DDS_ULong TargetStateV1GetSerializedKeySize(const TargetStateV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += TargetStateV1GetSerializedSampleSize(sample, currentAlignment);
    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* TargetStateV1LoanSampleBuf(TargetStateV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void TargetStateV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long TargetStateV1LoanDeserialize(TargetStateV1* sampleBuf,
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
DDS_Long TargetStateV1OnSiteDeserialize(CDRDeserializer* cdr,
    TargetStateV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean TargetStateV1NoSerializingSupported()
{
    return false;
}

DDS_ULong TargetStateV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T ControlCommandV1
#define TSeq ControlCommandV1Seq
#define TINITIALIZE ControlCommandV1InitializeEx
#define TFINALIZE ControlCommandV1FinalizeEx
#define TCOPY ControlCommandV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean ControlCommandV1Initialize(ControlCommandV1* self)
{
    return ControlCommandV1InitializeEx(self, NULL, true);
}

void ControlCommandV1Finalize(ControlCommandV1* self)
{
    ControlCommandV1FinalizeEx(self, NULL, true);
}

DDS_Boolean ControlCommandV1Copy(
    ControlCommandV1* dst,
    const ControlCommandV1* src)
{
    return ControlCommandV1CopyEx(dst, src, NULL);
}

ControlCommandV1* ControlCommandV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    ControlCommandV1* newSample = (ControlCommandV1*)ZRMalloc(pool, sizeof(ControlCommandV1));
    if (newSample == NULL)
    {
        printf("malloc for ControlCommandV1 failed.");
        return NULL;
    }
    if (!ControlCommandV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        ControlCommandV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void ControlCommandV1DestroySample(ZRMemPool* pool, ControlCommandV1* sample)
{
    if (sample == NULL) return;
    ControlCommandV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong ControlCommandV1GetSerializedSampleMaxSize()
{
    return 24;
}

DDS_ULong ControlCommandV1GetSerializedKeyMaxSize()
{
    return 4;
}

DDS_Long ControlCommandV1GetKeyHash(
    const ControlCommandV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = ControlCommandV1SerializeKey(sample, cdr);
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

DDS_Boolean ControlCommandV1HasKey()
{
    return true;
}

TypeCodeHeader* ControlCommandV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = ControlCommandV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean ControlCommandV1InitializeEx(
    ControlCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->flag = 0;

    self->JB = 0;

    self->platID = 0;

    self->simCommand = 0;

    self->roundCut = 0;

    self->currentRound = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void ControlCommandV1FinalizeEx(
    ControlCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean ControlCommandV1CopyEx(
    ControlCommandV1* dst,
    const ControlCommandV1* src,
    ZRMemPool* pool)
{
    dst->flag = src->flag;
    dst->JB = src->JB;
    dst->platID = src->platID;
    dst->simCommand = src->simCommand;
    dst->roundCut = src->roundCut;
    dst->currentRound = src->currentRound;
    return true;
}

void ControlCommandV1PrintData(const ControlCommandV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->flag: %d\n", sample->flag);
    printf("\n");

    printf("sample->JB: %d\n", sample->JB);
    printf("\n");

    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->simCommand: %d\n", sample->simCommand);
    printf("\n");

    printf("sample->roundCut: %d\n", sample->roundCut);
    printf("\n");

    printf("sample->currentRound: %d\n", sample->currentRound);
    printf("\n");

}

DDS::TypeCode* ControlCommandV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::ControlCommandV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct ControlCommandV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member flag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "flag",
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
        printf("Get Member JB TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "JB",
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
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "platID",
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
        printf("Get Member simCommand TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "simCommand",
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
        printf("Get Member roundCut TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "roundCut",
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
        printf("Get Member currentRound TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "currentRound",
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

DDS_Long ControlCommandV1Serialize(const ControlCommandV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("serialize sample->flag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("serialize sample->JB failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->simCommand, 4))
    {
        printf("serialize sample->simCommand failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->roundCut, 4))
    {
        printf("serialize sample->roundCut failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        printf("serialize sample->currentRound failed.");
        return -2;
    }

    return 0;
}

DDS_Long ControlCommandV1Deserialize(
    ControlCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:platID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("deserialize sample->flag failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("deserialize sample->JB failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    //last key :platID has been deserialized
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->simCommand, 4))
    {
        sample->simCommand = 0;
        sample->roundCut = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->roundCut, 4))
    {
        sample->roundCut = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        sample->currentRound = 0;
        return 0;
    }
    return 0;
}

DDS_ULong ControlCommandV1GetSerializedSampleSize(const ControlCommandV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long ControlCommandV1SerializeKey(const ControlCommandV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    return 0;
}

DDS_Long ControlCommandV1DeserializeKey(
    ControlCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong ControlCommandV1GetSerializedKeySize(const ControlCommandV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* ControlCommandV1LoanSampleBuf(ControlCommandV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void ControlCommandV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long ControlCommandV1LoanDeserialize(ControlCommandV1* sampleBuf,
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
DDS_Long ControlCommandV1OnSiteDeserialize(CDRDeserializer* cdr,
    ControlCommandV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean ControlCommandV1NoSerializingSupported()
{
    return false;
}

DDS_ULong ControlCommandV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T InitCommandV1
#define TSeq InitCommandV1Seq
#define TINITIALIZE InitCommandV1InitializeEx
#define TFINALIZE InitCommandV1FinalizeEx
#define TCOPY InitCommandV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean InitCommandV1Initialize(InitCommandV1* self)
{
    return InitCommandV1InitializeEx(self, NULL, true);
}

void InitCommandV1Finalize(InitCommandV1* self)
{
    InitCommandV1FinalizeEx(self, NULL, true);
}

DDS_Boolean InitCommandV1Copy(
    InitCommandV1* dst,
    const InitCommandV1* src)
{
    return InitCommandV1CopyEx(dst, src, NULL);
}

InitCommandV1* InitCommandV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    InitCommandV1* newSample = (InitCommandV1*)ZRMalloc(pool, sizeof(InitCommandV1));
    if (newSample == NULL)
    {
        printf("malloc for InitCommandV1 failed.");
        return NULL;
    }
    if (!InitCommandV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        InitCommandV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void InitCommandV1DestroySample(ZRMemPool* pool, InitCommandV1* sample)
{
    if (sample == NULL) return;
    InitCommandV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong InitCommandV1GetSerializedSampleMaxSize()
{
    return 404;
}

DDS_ULong InitCommandV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long InitCommandV1GetKeyHash(
    const InitCommandV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = InitCommandV1SerializeKey(sample, cdr);
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

DDS_Boolean InitCommandV1HasKey()
{
    return true;
}

TypeCodeHeader* InitCommandV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = InitCommandV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean InitCommandV1InitializeEx(
    InitCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->flag = 0;

    self->JB = 0;

    self->platID = 0;

    self->sensorID = 0;

    if (!HwaSimIRDds::PlatParamPakV1InitializeEx(&self->platParamInit, pool, allocateMemory))
    {
        printf("Initialize member self->platParamInit failed.");
        return false;
    }

    if (!HwaSimIRDds::InitObjectTrackingParamV1InitializeEx(&self->trackingInit, pool, allocateMemory))
    {
        printf("Initialize member self->trackingInit failed.");
        return false;
    }

    self->MissileMaxCount120 = 0;

    self->MissileMaxCount9 = 0;

    self->MissileMaxCountMMD = 0;

    self->MissileMaxCountF35 = 0;

    self->MissileMaxCountF22 = 0;

    self->MissileMaxCountResv1 = 0;

    self->MissileMaxCountResv2 = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void InitCommandV1FinalizeEx(
    InitCommandV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    HwaSimIRDds::PlatParamPakV1FinalizeEx(&self->platParamInit, pool, deletePointers);
    HwaSimIRDds::InitObjectTrackingParamV1FinalizeEx(&self->trackingInit, pool, deletePointers);
    if (deletePointers)
    {
    }
}

DDS_Boolean InitCommandV1CopyEx(
    InitCommandV1* dst,
    const InitCommandV1* src,
    ZRMemPool* pool)
{
    dst->flag = src->flag;
    dst->JB = src->JB;
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    if (!HwaSimIRDds::PlatParamPakV1CopyEx(&dst->platParamInit, &src->platParamInit, pool))
    {
        printf("copy member platParamInit failed.");
        return false;
    }
    if (!HwaSimIRDds::InitObjectTrackingParamV1CopyEx(&dst->trackingInit, &src->trackingInit, pool))
    {
        printf("copy member trackingInit failed.");
        return false;
    }
    dst->MissileMaxCount120 = src->MissileMaxCount120;
    dst->MissileMaxCount9 = src->MissileMaxCount9;
    dst->MissileMaxCountMMD = src->MissileMaxCountMMD;
    dst->MissileMaxCountF35 = src->MissileMaxCountF35;
    dst->MissileMaxCountF22 = src->MissileMaxCountF22;
    dst->MissileMaxCountResv1 = src->MissileMaxCountResv1;
    dst->MissileMaxCountResv2 = src->MissileMaxCountResv2;
    return true;
}

void InitCommandV1PrintData(const InitCommandV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->flag: %d\n", sample->flag);
    printf("\n");

    printf("sample->JB: %d\n", sample->JB);
    printf("\n");

    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    HwaSimIRDds::PlatParamPakV1PrintData(&sample->platParamInit);
    printf("\n");

    HwaSimIRDds::InitObjectTrackingParamV1PrintData(&sample->trackingInit);
    printf("\n");

    printf("sample->MissileMaxCount120: %d\n", sample->MissileMaxCount120);
    printf("\n");

    printf("sample->MissileMaxCount9: %d\n", sample->MissileMaxCount9);
    printf("\n");

    printf("sample->MissileMaxCountMMD: %d\n", sample->MissileMaxCountMMD);
    printf("\n");

    printf("sample->MissileMaxCountF35: %d\n", sample->MissileMaxCountF35);
    printf("\n");

    printf("sample->MissileMaxCountF22: %d\n", sample->MissileMaxCountF22);
    printf("\n");

    printf("sample->MissileMaxCountResv1: %d\n", sample->MissileMaxCountResv1);
    printf("\n");

    printf("sample->MissileMaxCountResv2: %d\n", sample->MissileMaxCountResv2);
    printf("\n");

}

DDS::TypeCode* InitCommandV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::InitCommandV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct InitCommandV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member flag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "flag",
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
        printf("Get Member JB TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "JB",
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
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::PlatParamPakV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member platParamInit TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "platParamInit",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::InitObjectTrackingParamV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member trackingInit TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "trackingInit",
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
        printf("Get Member MissileMaxCount120 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "MissileMaxCount120",
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
        printf("Get Member MissileMaxCount9 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "MissileMaxCount9",
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
        printf("Get Member MissileMaxCountMMD TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "MissileMaxCountMMD",
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
        printf("Get Member MissileMaxCountF35 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "MissileMaxCountF35",
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
        printf("Get Member MissileMaxCountF22 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        10,
        10,
        "MissileMaxCountF22",
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
        printf("Get Member MissileMaxCountResv1 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        11,
        11,
        "MissileMaxCountResv1",
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
        printf("Get Member MissileMaxCountResv2 TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        12,
        12,
        "MissileMaxCountResv2",
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

DDS_Long InitCommandV1Serialize(const InitCommandV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("serialize sample->flag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("serialize sample->JB failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (HwaSimIRDds::PlatParamPakV1Serialize(&sample->platParamInit, cdr) < 0)
    {
        printf("serialize sample->platParamInit failed.");
        return -2;
    }

    if (HwaSimIRDds::InitObjectTrackingParamV1Serialize(&sample->trackingInit, cdr) < 0)
    {
        printf("serialize sample->trackingInit failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCount120, 4))
    {
        printf("serialize sample->MissileMaxCount120 failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCount9, 4))
    {
        printf("serialize sample->MissileMaxCount9 failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountMMD, 4))
    {
        printf("serialize sample->MissileMaxCountMMD failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountF35, 4))
    {
        printf("serialize sample->MissileMaxCountF35 failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountF22, 4))
    {
        printf("serialize sample->MissileMaxCountF22 failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountResv1, 4))
    {
        printf("serialize sample->MissileMaxCountResv1 failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountResv2, 4))
    {
        printf("serialize sample->MissileMaxCountResv2 failed.");
        return -2;
    }

    return 0;
}

DDS_Long InitCommandV1Deserialize(
    InitCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("deserialize sample->flag failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("deserialize sample->JB failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    if (HwaSimIRDds::PlatParamPakV1Deserialize(&sample->platParamInit, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::PlatParamPakV1InitializeEx(&sample->platParamInit, pool, true))
        {
            printf("Initialize member sample->platParamInit failed.");
            return -2;
        }
        if (!HwaSimIRDds::InitObjectTrackingParamV1InitializeEx(&sample->trackingInit, pool, true))
        {
            printf("Initialize member sample->trackingInit failed.");
            return -2;
        }
        sample->MissileMaxCount120 = 0;
        sample->MissileMaxCount9 = 0;
        sample->MissileMaxCountMMD = 0;
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (HwaSimIRDds::InitObjectTrackingParamV1Deserialize(&sample->trackingInit, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::InitObjectTrackingParamV1InitializeEx(&sample->trackingInit, pool, true))
        {
            printf("Initialize member sample->trackingInit failed.");
            return -2;
        }
        sample->MissileMaxCount120 = 0;
        sample->MissileMaxCount9 = 0;
        sample->MissileMaxCountMMD = 0;
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCount120, 4))
    {
        sample->MissileMaxCount120 = 0;
        sample->MissileMaxCount9 = 0;
        sample->MissileMaxCountMMD = 0;
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCount9, 4))
    {
        sample->MissileMaxCount9 = 0;
        sample->MissileMaxCountMMD = 0;
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountMMD, 4))
    {
        sample->MissileMaxCountMMD = 0;
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountF35, 4))
    {
        sample->MissileMaxCountF35 = 0;
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountF22, 4))
    {
        sample->MissileMaxCountF22 = 0;
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountResv1, 4))
    {
        sample->MissileMaxCountResv1 = 0;
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->MissileMaxCountResv2, 4))
    {
        sample->MissileMaxCountResv2 = 0;
        return 0;
    }
    return 0;
}

DDS_ULong InitCommandV1GetSerializedSampleSize(const InitCommandV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += HwaSimIRDds::PlatParamPakV1GetSerializedSampleSize(&sample->platParamInit, currentAlignment);

    currentAlignment += HwaSimIRDds::InitObjectTrackingParamV1GetSerializedSampleSize(&sample->trackingInit, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long InitCommandV1SerializeKey(const InitCommandV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long InitCommandV1DeserializeKey(
    InitCommandV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong InitCommandV1GetSerializedKeySize(const InitCommandV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitCommandV1LoanSampleBuf(InitCommandV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void InitCommandV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long InitCommandV1LoanDeserialize(InitCommandV1* sampleBuf,
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
DDS_Long InitCommandV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitCommandV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean InitCommandV1NoSerializingSupported()
{
    return false;
}

DDS_ULong InitCommandV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T RealtimeDataV1
#define TSeq RealtimeDataV1Seq
#define TINITIALIZE RealtimeDataV1InitializeEx
#define TFINALIZE RealtimeDataV1FinalizeEx
#define TCOPY RealtimeDataV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean RealtimeDataV1Initialize(RealtimeDataV1* self)
{
    return RealtimeDataV1InitializeEx(self, NULL, true);
}

void RealtimeDataV1Finalize(RealtimeDataV1* self)
{
    RealtimeDataV1FinalizeEx(self, NULL, true);
}

DDS_Boolean RealtimeDataV1Copy(
    RealtimeDataV1* dst,
    const RealtimeDataV1* src)
{
    return RealtimeDataV1CopyEx(dst, src, NULL);
}

RealtimeDataV1* RealtimeDataV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    RealtimeDataV1* newSample = (RealtimeDataV1*)ZRMalloc(pool, sizeof(RealtimeDataV1));
    if (newSample == NULL)
    {
        printf("malloc for RealtimeDataV1 failed.");
        return NULL;
    }
    if (!RealtimeDataV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        RealtimeDataV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void RealtimeDataV1DestroySample(ZRMemPool* pool, RealtimeDataV1* sample)
{
    if (sample == NULL) return;
    RealtimeDataV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong RealtimeDataV1GetSerializedSampleMaxSize()
{
    return 556;
}

DDS_ULong RealtimeDataV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long RealtimeDataV1GetKeyHash(
    const RealtimeDataV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = RealtimeDataV1SerializeKey(sample, cdr);
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

DDS_Boolean RealtimeDataV1HasKey()
{
    return true;
}

TypeCodeHeader* RealtimeDataV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = RealtimeDataV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean RealtimeDataV1InitializeEx(
    RealtimeDataV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->flag = 0;

    self->platID = 0;

    self->sensorID = 0;

    self->time = 0;

    if (!HwaSimIRDds::SpatialStateV1InitializeEx(&self->platLoc, pool, allocateMemory))
    {
        printf("Initialize member self->platLoc failed.");
        return false;
    }

    if (!HwaSimIRDds::WeaponStateV1InitializeEx(&self->weaponState, pool, allocateMemory))
    {
        printf("Initialize member self->weaponState failed.");
        return false;
    }

    self->targetNumValid = 0;

    HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) self->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, allocateMemory))
        {
            printf("Initialize member self->targetState failed.");
            return false;
        }
    }

    if (allocateMemory)
    {
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
        }
    }
    else
    {
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
        }
    }
    return true;
}

void RealtimeDataV1FinalizeEx(
    RealtimeDataV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    HwaSimIRDds::SpatialStateV1FinalizeEx(&self->platLoc, pool, deletePointers);
    HwaSimIRDds::WeaponStateV1FinalizeEx(&self->weaponState, pool, deletePointers);
    if (deletePointers)
    {
        HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) self->targetState;
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
            HwaSimIRDds::TargetStateV1FinalizeEx(&targetStateArray[targetStateIndex], pool, deletePointers);
        }
    }
}

DDS_Boolean RealtimeDataV1CopyEx(
    RealtimeDataV1* dst,
    const RealtimeDataV1* src,
    ZRMemPool* pool)
{
    dst->flag = src->flag;
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    dst->time = src->time;
    if (!HwaSimIRDds::SpatialStateV1CopyEx(&dst->platLoc, &src->platLoc, pool))
    {
        printf("copy member platLoc failed.");
        return false;
    }
    if (!HwaSimIRDds::WeaponStateV1CopyEx(&dst->weaponState, &src->weaponState, pool))
    {
        printf("copy member weaponState failed.");
        return false;
    }
    dst->targetNumValid = src->targetNumValid;
    HwaSimIRDds::TargetStateV1* targetStateSrcArray = (HwaSimIRDds::TargetStateV1*) src->targetState;
    HwaSimIRDds::TargetStateV1* targetStateDstArray = (HwaSimIRDds::TargetStateV1*) dst->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        if (!HwaSimIRDds::TargetStateV1CopyEx(&targetStateDstArray[targetStateIndex], &targetStateSrcArray[targetStateIndex], pool))
        {
            printf("copy member targetState failed.");
            return false;
        }
    }
    return true;
}

void RealtimeDataV1PrintData(const RealtimeDataV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->flag: %d\n", sample->flag);
    printf("\n");

    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    printf("sample->time: %lf\n", sample->time);
    printf("\n");

    HwaSimIRDds::SpatialStateV1PrintData(&sample->platLoc);
    printf("\n");

    HwaSimIRDds::WeaponStateV1PrintData(&sample->weaponState);
    printf("\n");

    printf("sample->targetNumValid: %d\n", sample->targetNumValid);
    printf("\n");

    HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        HwaSimIRDds::TargetStateV1PrintData(&targetStateArray[targetStateIndex]);
        printf("\n");
    }
    printf("\n");

}

DDS::TypeCode* RealtimeDataV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::RealtimeDataV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct RealtimeDataV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member flag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "flag",
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
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member time TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "time",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::SpatialStateV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member platLoc TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "platLoc",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::WeaponStateV1GetTypeCode();
    if (memberTc == NULL)
    {
        printf("Get Member weaponState TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "weaponState",
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
        printf("Get Member targetNumValid TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "targetNumValid",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = HwaSimIRDds::TargetStateV1GetTypeCode();
    eleTc = memberTc;
    if (eleTc != NULL)
    {
        DDS_ULong labels[1];
        labels[0] = 5;
        memberTc = factory.createArrayTC(1, labels, eleTc);
    }
    if (memberTc == NULL)
    {
        printf("Get Member targetState TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "targetState",
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

DDS_Long RealtimeDataV1Serialize(const RealtimeDataV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("serialize sample->flag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->time, 8))
    {
        printf("serialize sample->time failed.");
        return -2;
    }

    if (HwaSimIRDds::SpatialStateV1Serialize(&sample->platLoc, cdr) < 0)
    {
        printf("serialize sample->platLoc failed.");
        return -2;
    }

    if (HwaSimIRDds::WeaponStateV1Serialize(&sample->weaponState, cdr) < 0)
    {
        printf("serialize sample->weaponState failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->targetNumValid, 4))
    {
        printf("serialize sample->targetNumValid failed.");
        return -2;
    }

    HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        if (HwaSimIRDds::TargetStateV1Serialize(&targetStateArray[targetStateIndex], cdr) < 0)
        {
            printf("serialize sample->targetState failed.");
            return -2;
        }
    }

    return 0;
}

DDS_Long RealtimeDataV1Deserialize(
    RealtimeDataV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("deserialize sample->flag failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->time, 8))
    {
        sample->time = 0;
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->platLoc, pool, true))
        {
            printf("Initialize member sample->platLoc failed.");
            return -2;
        }
        if (!HwaSimIRDds::WeaponStateV1InitializeEx(&sample->weaponState, pool, true))
        {
            printf("Initialize member sample->weaponState failed.");
            return -2;
        }
        sample->targetNumValid = 0;
        HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
            if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, true))
            {
                printf("Initialize member sample->targetState failed.");
                return -2;
            }
        }
        return 0;
    }
    if (HwaSimIRDds::SpatialStateV1Deserialize(&sample->platLoc, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::SpatialStateV1InitializeEx(&sample->platLoc, pool, true))
        {
            printf("Initialize member sample->platLoc failed.");
            return -2;
        }
        if (!HwaSimIRDds::WeaponStateV1InitializeEx(&sample->weaponState, pool, true))
        {
            printf("Initialize member sample->weaponState failed.");
            return -2;
        }
        sample->targetNumValid = 0;
        HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
            if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, true))
            {
                printf("Initialize member sample->targetState failed.");
                return -2;
            }
        }
        return 0;
    }
    if (HwaSimIRDds::WeaponStateV1Deserialize(&sample->weaponState, cdr, pool) < 0)
    {
        if (!HwaSimIRDds::WeaponStateV1InitializeEx(&sample->weaponState, pool, true))
        {
            printf("Initialize member sample->weaponState failed.");
            return -2;
        }
        sample->targetNumValid = 0;
        HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
            if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, true))
            {
                printf("Initialize member sample->targetState failed.");
                return -2;
            }
        }
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->targetNumValid, 4))
    {
        sample->targetNumValid = 0;
        HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
        for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
        {
            if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, true))
            {
                printf("Initialize member sample->targetState failed.");
                return -2;
            }
        }
        return 0;
    }
    HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        if (HwaSimIRDds::TargetStateV1Deserialize(&targetStateArray[targetStateIndex], cdr, pool) < 0)
        {
            HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
            for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
            {
                if (!HwaSimIRDds::TargetStateV1InitializeEx(&targetStateArray[targetStateIndex], pool, true))
                {
                    printf("Initialize member sample->targetState failed.");
                    return -2;
                }
            }
            return 0;
        }
    }
    return 0;
}

DDS_ULong RealtimeDataV1GetSerializedSampleSize(const RealtimeDataV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += HwaSimIRDds::SpatialStateV1GetSerializedSampleSize(&sample->platLoc, currentAlignment);

    currentAlignment += HwaSimIRDds::WeaponStateV1GetSerializedSampleSize(&sample->weaponState, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    HwaSimIRDds::TargetStateV1* targetStateArray = (HwaSimIRDds::TargetStateV1*) sample->targetState;
    for (DDS_ULong targetStateIndex = 0; targetStateIndex < 5; ++targetStateIndex)
    {
        currentAlignment += HwaSimIRDds::TargetStateV1GetSerializedSampleSize(&targetStateArray[targetStateIndex], currentAlignment);
    }

    return currentAlignment - initialAlignment;
}

DDS_Long RealtimeDataV1SerializeKey(const RealtimeDataV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long RealtimeDataV1DeserializeKey(
    RealtimeDataV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong RealtimeDataV1GetSerializedKeySize(const RealtimeDataV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* RealtimeDataV1LoanSampleBuf(RealtimeDataV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void RealtimeDataV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long RealtimeDataV1LoanDeserialize(RealtimeDataV1* sampleBuf,
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
DDS_Long RealtimeDataV1OnSiteDeserialize(CDRDeserializer* cdr,
    RealtimeDataV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean RealtimeDataV1NoSerializingSupported()
{
    return false;
}

DDS_ULong RealtimeDataV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T InitAckV1
#define TSeq InitAckV1Seq
#define TINITIALIZE InitAckV1InitializeEx
#define TFINALIZE InitAckV1FinalizeEx
#define TCOPY InitAckV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean InitAckV1Initialize(InitAckV1* self)
{
    return InitAckV1InitializeEx(self, NULL, true);
}

void InitAckV1Finalize(InitAckV1* self)
{
    InitAckV1FinalizeEx(self, NULL, true);
}

DDS_Boolean InitAckV1Copy(
    InitAckV1* dst,
    const InitAckV1* src)
{
    return InitAckV1CopyEx(dst, src, NULL);
}

InitAckV1* InitAckV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    InitAckV1* newSample = (InitAckV1*)ZRMalloc(pool, sizeof(InitAckV1));
    if (newSample == NULL)
    {
        printf("malloc for InitAckV1 failed.");
        return NULL;
    }
    if (!InitAckV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        InitAckV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void InitAckV1DestroySample(ZRMemPool* pool, InitAckV1* sample)
{
    if (sample == NULL) return;
    InitAckV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong InitAckV1GetSerializedSampleMaxSize()
{
    return 17;
}

DDS_ULong InitAckV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long InitAckV1GetKeyHash(
    const InitAckV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = InitAckV1SerializeKey(sample, cdr);
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

DDS_Boolean InitAckV1HasKey()
{
    return true;
}

TypeCodeHeader* InitAckV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = InitAckV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean InitAckV1InitializeEx(
    InitAckV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->flag = 0;

    self->JB = 0;

    self->platID = 0;

    self->sensorID = 0;

    self->trackingReady = 0;

    if (allocateMemory)
    {
    }
    else
    {
    }
    return true;
}

void InitAckV1FinalizeEx(
    InitAckV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
    }
}

DDS_Boolean InitAckV1CopyEx(
    InitAckV1* dst,
    const InitAckV1* src,
    ZRMemPool* pool)
{
    dst->flag = src->flag;
    dst->JB = src->JB;
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    dst->trackingReady = src->trackingReady;
    return true;
}

void InitAckV1PrintData(const InitAckV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->flag: %d\n", sample->flag);
    printf("\n");

    printf("sample->JB: %d\n", sample->JB);
    printf("\n");

    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    printf("sample->trackingReady: %d\n", sample->trackingReady);
    printf("\n");

}

DDS::TypeCode* InitAckV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::InitAckV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct InitAckV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member flag TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "flag",
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
        printf("Get Member JB TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "JB",
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
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member trackingReady TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "trackingReady",
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

DDS_Long InitAckV1Serialize(const InitAckV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("serialize sample->flag failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("serialize sample->JB failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->trackingReady, 1))
    {
        printf("serialize sample->trackingReady failed.");
        return -2;
    }

    return 0;
}

DDS_Long InitAckV1Deserialize(
    InitAckV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->flag, 4))
    {
        printf("deserialize sample->flag failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->JB, 4))
    {
        printf("deserialize sample->JB failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->trackingReady, 1))
    {
        sample->trackingReady = 0;
        return 0;
    }
    return 0;
}

DDS_ULong InitAckV1GetSerializedSampleSize(const InitAckV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long InitAckV1SerializeKey(const InitAckV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long InitAckV1DeserializeKey(
    InitAckV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong InitAckV1GetSerializedKeySize(const InitAckV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* InitAckV1LoanSampleBuf(InitAckV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void InitAckV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long InitAckV1LoanDeserialize(InitAckV1* sampleBuf,
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
DDS_Long InitAckV1OnSiteDeserialize(CDRDeserializer* cdr,
    InitAckV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean InitAckV1NoSerializingSupported()
{
    return false;
}

DDS_ULong InitAckV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T VideoStatusV1
#define TSeq VideoStatusV1Seq
#define TINITIALIZE VideoStatusV1InitializeEx
#define TFINALIZE VideoStatusV1FinalizeEx
#define TCOPY VideoStatusV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean VideoStatusV1Initialize(VideoStatusV1* self)
{
    return VideoStatusV1InitializeEx(self, NULL, true);
}

void VideoStatusV1Finalize(VideoStatusV1* self)
{
    VideoStatusV1FinalizeEx(self, NULL, true);
}

DDS_Boolean VideoStatusV1Copy(
    VideoStatusV1* dst,
    const VideoStatusV1* src)
{
    return VideoStatusV1CopyEx(dst, src, NULL);
}

VideoStatusV1* VideoStatusV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    VideoStatusV1* newSample = (VideoStatusV1*)ZRMalloc(pool, sizeof(VideoStatusV1));
    if (newSample == NULL)
    {
        printf("malloc for VideoStatusV1 failed.");
        return NULL;
    }
    if (!VideoStatusV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        VideoStatusV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void VideoStatusV1DestroySample(ZRMemPool* pool, VideoStatusV1* sample)
{
    if (sample == NULL) return;
    VideoStatusV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong VideoStatusV1GetSerializedSampleMaxSize()
{
    return 260;
}

DDS_ULong VideoStatusV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long VideoStatusV1GetKeyHash(
    const VideoStatusV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = VideoStatusV1SerializeKey(sample, cdr);
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

DDS_Boolean VideoStatusV1HasKey()
{
    return true;
}

TypeCodeHeader* VideoStatusV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = VideoStatusV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean VideoStatusV1InitializeEx(
    VideoStatusV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->platID = 0;

    self->sensorID = 0;

    self->channel = NULL;

    self->running = 0;

    self->codec = NULL;

    self->pixelFormat = NULL;

    self->videoTopic = NULL;

    self->width = 0;

    self->height = 0;

    self->fps = 0;

    self->bitrateKbps = 0;

    self->gopFrames = 0;

    self->compressed = 0;

    self->currentRound = 0;

    if (allocateMemory)
    {
        self->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
        if (self->channel == NULL)
        {
            printf("Malloc for self->channel failed.");
            return false;
        }
        self->channel[0] = '\0';
        self->codec = (DDS_Char*) ZRMalloc(pool, 24 + 1);
        if (self->codec == NULL)
        {
            printf("Malloc for self->codec failed.");
            return false;
        }
        self->codec[0] = '\0';
        self->pixelFormat = (DDS_Char*) ZRMalloc(pool, 24 + 1);
        if (self->pixelFormat == NULL)
        {
            printf("Malloc for self->pixelFormat failed.");
            return false;
        }
        self->pixelFormat[0] = '\0';
        self->videoTopic = (DDS_Char*) ZRMalloc(pool, 128 + 1);
        if (self->videoTopic == NULL)
        {
            printf("Malloc for self->videoTopic failed.");
            return false;
        }
        self->videoTopic[0] = '\0';
    }
    else
    {
        if (self->channel != NULL)
        {
            self->channel[0] = '\0';
        }
        if (self->codec != NULL)
        {
            self->codec[0] = '\0';
        }
        if (self->pixelFormat != NULL)
        {
            self->pixelFormat[0] = '\0';
        }
        if (self->videoTopic != NULL)
        {
            self->videoTopic[0] = '\0';
        }
    }
    return true;
}

void VideoStatusV1FinalizeEx(
    VideoStatusV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
        ZRDealloc(pool, self->channel);
        self->channel = NULL;
        ZRDealloc(pool, self->codec);
        self->codec = NULL;
        ZRDealloc(pool, self->pixelFormat);
        self->pixelFormat = NULL;
        ZRDealloc(pool, self->videoTopic);
        self->videoTopic = NULL;
    }
}

DDS_Boolean VideoStatusV1CopyEx(
    VideoStatusV1* dst,
    const VideoStatusV1* src,
    ZRMemPool* pool)
{
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    if (src->channel == NULL)
    {
        ZRDealloc(pool, dst->channel);
        dst->channel = NULL;
    }
    else
    {
        if (dst->channel == NULL)
        {
            dst->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
            if (dst->channel == NULL)
            {
                printf("malloc for channel failed.");
                return false;
            }
        }
        strcpy(dst->channel, src->channel);
    }
    dst->running = src->running;
    if (src->codec == NULL)
    {
        ZRDealloc(pool, dst->codec);
        dst->codec = NULL;
    }
    else
    {
        if (dst->codec == NULL)
        {
            dst->codec = (DDS_Char*) ZRMalloc(pool, 24 + 1);
            if (dst->codec == NULL)
            {
                printf("malloc for codec failed.");
                return false;
            }
        }
        strcpy(dst->codec, src->codec);
    }
    if (src->pixelFormat == NULL)
    {
        ZRDealloc(pool, dst->pixelFormat);
        dst->pixelFormat = NULL;
    }
    else
    {
        if (dst->pixelFormat == NULL)
        {
            dst->pixelFormat = (DDS_Char*) ZRMalloc(pool, 24 + 1);
            if (dst->pixelFormat == NULL)
            {
                printf("malloc for pixelFormat failed.");
                return false;
            }
        }
        strcpy(dst->pixelFormat, src->pixelFormat);
    }
    if (src->videoTopic == NULL)
    {
        ZRDealloc(pool, dst->videoTopic);
        dst->videoTopic = NULL;
    }
    else
    {
        if (dst->videoTopic == NULL)
        {
            dst->videoTopic = (DDS_Char*) ZRMalloc(pool, 128 + 1);
            if (dst->videoTopic == NULL)
            {
                printf("malloc for videoTopic failed.");
                return false;
            }
        }
        strcpy(dst->videoTopic, src->videoTopic);
    }
    dst->width = src->width;
    dst->height = src->height;
    dst->fps = src->fps;
    dst->bitrateKbps = src->bitrateKbps;
    dst->gopFrames = src->gopFrames;
    dst->compressed = src->compressed;
    dst->currentRound = src->currentRound;
    return true;
}

void VideoStatusV1PrintData(const VideoStatusV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    if (sample->channel != NULL)
    {
        printf("sample->channel(%d): %s\n", strlen(sample->channel), sample->channel);
    }
    else
    {
        printf("sample->channel(0): NULL\n");
    }
    printf("\n");

    printf("sample->running: %d\n", sample->running);
    printf("\n");

    if (sample->codec != NULL)
    {
        printf("sample->codec(%d): %s\n", strlen(sample->codec), sample->codec);
    }
    else
    {
        printf("sample->codec(0): NULL\n");
    }
    printf("\n");

    if (sample->pixelFormat != NULL)
    {
        printf("sample->pixelFormat(%d): %s\n", strlen(sample->pixelFormat), sample->pixelFormat);
    }
    else
    {
        printf("sample->pixelFormat(0): NULL\n");
    }
    printf("\n");

    if (sample->videoTopic != NULL)
    {
        printf("sample->videoTopic(%d): %s\n", strlen(sample->videoTopic), sample->videoTopic);
    }
    else
    {
        printf("sample->videoTopic(0): NULL\n");
    }
    printf("\n");

    printf("sample->width: %d\n", sample->width);
    printf("\n");

    printf("sample->height: %d\n", sample->height);
    printf("\n");

    printf("sample->fps: %d\n", sample->fps);
    printf("\n");

    printf("sample->bitrateKbps: %d\n", sample->bitrateKbps);
    printf("\n");

    printf("sample->gopFrames: %d\n", sample->gopFrames);
    printf("\n");

    printf("sample->compressed: %d\n", sample->compressed);
    printf("\n");

    printf("sample->currentRound: %d\n", sample->currentRound);
    printf("\n");

}

DDS::TypeCode* VideoStatusV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::VideoStatusV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct VideoStatusV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(16);
    if (memberTc == NULL)
    {
        printf("Get Member channel TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "channel",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member running TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "running",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(24);
    if (memberTc == NULL)
    {
        printf("Get Member codec TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "codec",
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

    memberTc = factory.createStringTC(24);
    if (memberTc == NULL)
    {
        printf("Get Member pixelFormat TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "pixelFormat",
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

    memberTc = factory.createStringTC(128);
    if (memberTc == NULL)
    {
        printf("Get Member videoTopic TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "videoTopic",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member width TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "width",
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
        printf("Get Member height TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "height",
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
        printf("Get Member fps TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "fps",
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
        printf("Get Member bitrateKbps TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        10,
        10,
        "bitrateKbps",
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
        printf("Get Member gopFrames TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        11,
        11,
        "gopFrames",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member compressed TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        12,
        12,
        "compressed",
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
        printf("Get Member currentRound TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        13,
        13,
        "currentRound",
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

DDS_Long VideoStatusV1Serialize(const VideoStatusV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->channel, sample->channel == NULL ? 0 : strlen(sample->channel) + 1))
    {
        printf("serialize sample->channel failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->running, 1))
    {
        printf("serialize sample->running failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->codec, sample->codec == NULL ? 0 : strlen(sample->codec) + 1))
    {
        printf("serialize sample->codec failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->pixelFormat, sample->pixelFormat == NULL ? 0 : strlen(sample->pixelFormat) + 1))
    {
        printf("serialize sample->pixelFormat failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->videoTopic, sample->videoTopic == NULL ? 0 : strlen(sample->videoTopic) + 1))
    {
        printf("serialize sample->videoTopic failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->width, 4))
    {
        printf("serialize sample->width failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->height, 4))
    {
        printf("serialize sample->height failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->fps, 4))
    {
        printf("serialize sample->fps failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->bitrateKbps, 4))
    {
        printf("serialize sample->bitrateKbps failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->gopFrames, 4))
    {
        printf("serialize sample->gopFrames failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->compressed, 1))
    {
        printf("serialize sample->compressed failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        printf("serialize sample->currentRound failed.");
        return -2;
    }

    return 0;
}

DDS_Long VideoStatusV1Deserialize(
    VideoStatusV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    DDS_ULong channelTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &channelTmpLen, 4))
    {
        sample->channel = NULL;
        sample->running = 0;
        sample->codec = NULL;
        sample->pixelFormat = NULL;
        sample->videoTopic = NULL;
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (0 == channelTmpLen)
    {
        ZRDealloc(pool, sample->channel);
        sample->channel = NULL;
    }
    else
    {
        if (sample->channel == NULL)
        {
            sample->channel = (DDS_Char*) ZRMalloc(pool, channelTmpLen);
            if (sample->channel == NULL)
            {
                printf("malloc for sample->channel failed(%d).", channelTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->channel, channelTmpLen, 1))
        {
            printf("deserialize member sample->channel failed.");
            return -4;
        }
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->running, 1))
    {
        sample->running = 0;
        sample->codec = NULL;
        sample->pixelFormat = NULL;
        sample->videoTopic = NULL;
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    DDS_ULong codecTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &codecTmpLen, 4))
    {
        sample->codec = NULL;
        sample->pixelFormat = NULL;
        sample->videoTopic = NULL;
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (0 == codecTmpLen)
    {
        ZRDealloc(pool, sample->codec);
        sample->codec = NULL;
    }
    else
    {
        if (sample->codec == NULL)
        {
            sample->codec = (DDS_Char*) ZRMalloc(pool, codecTmpLen);
            if (sample->codec == NULL)
            {
                printf("malloc for sample->codec failed(%d).", codecTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->codec, codecTmpLen, 1))
        {
            printf("deserialize member sample->codec failed.");
            return -4;
        }
    }
    DDS_ULong pixelFormatTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &pixelFormatTmpLen, 4))
    {
        sample->pixelFormat = NULL;
        sample->videoTopic = NULL;
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (0 == pixelFormatTmpLen)
    {
        ZRDealloc(pool, sample->pixelFormat);
        sample->pixelFormat = NULL;
    }
    else
    {
        if (sample->pixelFormat == NULL)
        {
            sample->pixelFormat = (DDS_Char*) ZRMalloc(pool, pixelFormatTmpLen);
            if (sample->pixelFormat == NULL)
            {
                printf("malloc for sample->pixelFormat failed(%d).", pixelFormatTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->pixelFormat, pixelFormatTmpLen, 1))
        {
            printf("deserialize member sample->pixelFormat failed.");
            return -4;
        }
    }
    DDS_ULong videoTopicTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &videoTopicTmpLen, 4))
    {
        sample->videoTopic = NULL;
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (0 == videoTopicTmpLen)
    {
        ZRDealloc(pool, sample->videoTopic);
        sample->videoTopic = NULL;
    }
    else
    {
        if (sample->videoTopic == NULL)
        {
            sample->videoTopic = (DDS_Char*) ZRMalloc(pool, videoTopicTmpLen);
            if (sample->videoTopic == NULL)
            {
                printf("malloc for sample->videoTopic failed(%d).", videoTopicTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->videoTopic, videoTopicTmpLen, 1))
        {
            printf("deserialize member sample->videoTopic failed.");
            return -4;
        }
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->width, 4))
    {
        sample->width = 0;
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->height, 4))
    {
        sample->height = 0;
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->fps, 4))
    {
        sample->fps = 0;
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->bitrateKbps, 4))
    {
        sample->bitrateKbps = 0;
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->gopFrames, 4))
    {
        sample->gopFrames = 0;
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->compressed, 1))
    {
        sample->compressed = 0;
        sample->currentRound = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        sample->currentRound = 0;
        return 0;
    }
    return 0;
}

DDS_ULong VideoStatusV1GetSerializedSampleSize(const VideoStatusV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->channel == NULL ? 0 : strlen(sample->channel) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->codec == NULL ? 0 : strlen(sample->codec) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->pixelFormat == NULL ? 0 : strlen(sample->pixelFormat) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->videoTopic == NULL ? 0 : strlen(sample->videoTopic) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long VideoStatusV1SerializeKey(const VideoStatusV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long VideoStatusV1DeserializeKey(
    VideoStatusV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong VideoStatusV1GetSerializedKeySize(const VideoStatusV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* VideoStatusV1LoanSampleBuf(VideoStatusV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void VideoStatusV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long VideoStatusV1LoanDeserialize(VideoStatusV1* sampleBuf,
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
DDS_Long VideoStatusV1OnSiteDeserialize(CDRDeserializer* cdr,
    VideoStatusV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean VideoStatusV1NoSerializingSupported()
{
    return false;
}

DDS_ULong VideoStatusV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T VideoFrameMetaV1
#define TSeq VideoFrameMetaV1Seq
#define TINITIALIZE VideoFrameMetaV1InitializeEx
#define TFINALIZE VideoFrameMetaV1FinalizeEx
#define TCOPY VideoFrameMetaV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean VideoFrameMetaV1Initialize(VideoFrameMetaV1* self)
{
    return VideoFrameMetaV1InitializeEx(self, NULL, true);
}

void VideoFrameMetaV1Finalize(VideoFrameMetaV1* self)
{
    VideoFrameMetaV1FinalizeEx(self, NULL, true);
}

DDS_Boolean VideoFrameMetaV1Copy(
    VideoFrameMetaV1* dst,
    const VideoFrameMetaV1* src)
{
    return VideoFrameMetaV1CopyEx(dst, src, NULL);
}

VideoFrameMetaV1* VideoFrameMetaV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    VideoFrameMetaV1* newSample = (VideoFrameMetaV1*)ZRMalloc(pool, sizeof(VideoFrameMetaV1));
    if (newSample == NULL)
    {
        printf("malloc for VideoFrameMetaV1 failed.");
        return NULL;
    }
    if (!VideoFrameMetaV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        VideoFrameMetaV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void VideoFrameMetaV1DestroySample(ZRMemPool* pool, VideoFrameMetaV1* sample)
{
    if (sample == NULL) return;
    VideoFrameMetaV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong VideoFrameMetaV1GetSerializedSampleMaxSize()
{
    return 92;
}

DDS_ULong VideoFrameMetaV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long VideoFrameMetaV1GetKeyHash(
    const VideoFrameMetaV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = VideoFrameMetaV1SerializeKey(sample, cdr);
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

DDS_Boolean VideoFrameMetaV1HasKey()
{
    return true;
}

TypeCodeHeader* VideoFrameMetaV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = VideoFrameMetaV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean VideoFrameMetaV1InitializeEx(
    VideoFrameMetaV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->platID = 0;

    self->sensorID = 0;

    self->channel = NULL;

    self->frameSeq = 0;

    self->currentRound = 0;

    self->ptsMs = 0;

    self->keyFrame = 0;

    self->codec = NULL;

    self->width = 0;

    self->height = 0;

    if (allocateMemory)
    {
        self->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
        if (self->channel == NULL)
        {
            printf("Malloc for self->channel failed.");
            return false;
        }
        self->channel[0] = '\0';
        self->codec = (DDS_Char*) ZRMalloc(pool, 24 + 1);
        if (self->codec == NULL)
        {
            printf("Malloc for self->codec failed.");
            return false;
        }
        self->codec[0] = '\0';
    }
    else
    {
        if (self->channel != NULL)
        {
            self->channel[0] = '\0';
        }
        if (self->codec != NULL)
        {
            self->codec[0] = '\0';
        }
    }
    return true;
}

void VideoFrameMetaV1FinalizeEx(
    VideoFrameMetaV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
        ZRDealloc(pool, self->channel);
        self->channel = NULL;
        ZRDealloc(pool, self->codec);
        self->codec = NULL;
    }
}

DDS_Boolean VideoFrameMetaV1CopyEx(
    VideoFrameMetaV1* dst,
    const VideoFrameMetaV1* src,
    ZRMemPool* pool)
{
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    if (src->channel == NULL)
    {
        ZRDealloc(pool, dst->channel);
        dst->channel = NULL;
    }
    else
    {
        if (dst->channel == NULL)
        {
            dst->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
            if (dst->channel == NULL)
            {
                printf("malloc for channel failed.");
                return false;
            }
        }
        strcpy(dst->channel, src->channel);
    }
    dst->frameSeq = src->frameSeq;
    dst->currentRound = src->currentRound;
    dst->ptsMs = src->ptsMs;
    dst->keyFrame = src->keyFrame;
    if (src->codec == NULL)
    {
        ZRDealloc(pool, dst->codec);
        dst->codec = NULL;
    }
    else
    {
        if (dst->codec == NULL)
        {
            dst->codec = (DDS_Char*) ZRMalloc(pool, 24 + 1);
            if (dst->codec == NULL)
            {
                printf("malloc for codec failed.");
                return false;
            }
        }
        strcpy(dst->codec, src->codec);
    }
    dst->width = src->width;
    dst->height = src->height;
    return true;
}

void VideoFrameMetaV1PrintData(const VideoFrameMetaV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    if (sample->channel != NULL)
    {
        printf("sample->channel(%d): %s\n", strlen(sample->channel), sample->channel);
    }
    else
    {
        printf("sample->channel(0): NULL\n");
    }
    printf("\n");

    printf("sample->frameSeq: %u\n", sample->frameSeq);
    printf("\n");

    printf("sample->currentRound: %d\n", sample->currentRound);
    printf("\n");

    printf("sample->ptsMs: %lf\n", sample->ptsMs);
    printf("\n");

    printf("sample->keyFrame: %d\n", sample->keyFrame);
    printf("\n");

    if (sample->codec != NULL)
    {
        printf("sample->codec(%d): %s\n", strlen(sample->codec), sample->codec);
    }
    else
    {
        printf("sample->codec(0): NULL\n");
    }
    printf("\n");

    printf("sample->width: %d\n", sample->width);
    printf("\n");

    printf("sample->height: %d\n", sample->height);
    printf("\n");

}

DDS::TypeCode* VideoFrameMetaV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::VideoFrameMetaV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct VideoFrameMetaV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(16);
    if (memberTc == NULL)
    {
        printf("Get Member channel TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "channel",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member frameSeq TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "frameSeq",
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
        printf("Get Member currentRound TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "currentRound",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member ptsMs TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "ptsMs",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_BOOLEAN);
    if (memberTc == NULL)
    {
        printf("Get Member keyFrame TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "keyFrame",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(24);
    if (memberTc == NULL)
    {
        printf("Get Member codec TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        7,
        7,
        "codec",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member width TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        8,
        8,
        "width",
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
        printf("Get Member height TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        9,
        9,
        "height",
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

DDS_Long VideoFrameMetaV1Serialize(const VideoFrameMetaV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->channel, sample->channel == NULL ? 0 : strlen(sample->channel) + 1))
    {
        printf("serialize sample->channel failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->frameSeq, 4))
    {
        printf("serialize sample->frameSeq failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        printf("serialize sample->currentRound failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->ptsMs, 8))
    {
        printf("serialize sample->ptsMs failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->keyFrame, 1))
    {
        printf("serialize sample->keyFrame failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->codec, sample->codec == NULL ? 0 : strlen(sample->codec) + 1))
    {
        printf("serialize sample->codec failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->width, 4))
    {
        printf("serialize sample->width failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->height, 4))
    {
        printf("serialize sample->height failed.");
        return -2;
    }

    return 0;
}

DDS_Long VideoFrameMetaV1Deserialize(
    VideoFrameMetaV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    DDS_ULong channelTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &channelTmpLen, 4))
    {
        sample->channel = NULL;
        sample->frameSeq = 0;
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->keyFrame = 0;
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (0 == channelTmpLen)
    {
        ZRDealloc(pool, sample->channel);
        sample->channel = NULL;
    }
    else
    {
        if (sample->channel == NULL)
        {
            sample->channel = (DDS_Char*) ZRMalloc(pool, channelTmpLen);
            if (sample->channel == NULL)
            {
                printf("malloc for sample->channel failed(%d).", channelTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->channel, channelTmpLen, 1))
        {
            printf("deserialize member sample->channel failed.");
            return -4;
        }
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameSeq, 4))
    {
        sample->frameSeq = 0;
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->keyFrame = 0;
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->keyFrame = 0;
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->ptsMs, 8))
    {
        sample->ptsMs = 0;
        sample->keyFrame = 0;
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->keyFrame, 1))
    {
        sample->keyFrame = 0;
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    DDS_ULong codecTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &codecTmpLen, 4))
    {
        sample->codec = NULL;
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (0 == codecTmpLen)
    {
        ZRDealloc(pool, sample->codec);
        sample->codec = NULL;
    }
    else
    {
        if (sample->codec == NULL)
        {
            sample->codec = (DDS_Char*) ZRMalloc(pool, codecTmpLen);
            if (sample->codec == NULL)
            {
                printf("malloc for sample->codec failed(%d).", codecTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->codec, codecTmpLen, 1))
        {
            printf("deserialize member sample->codec failed.");
            return -4;
        }
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->width, 4))
    {
        sample->width = 0;
        sample->height = 0;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->height, 4))
    {
        sample->height = 0;
        return 0;
    }
    return 0;
}

DDS_ULong VideoFrameMetaV1GetSerializedSampleSize(const VideoFrameMetaV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->channel == NULL ? 0 : strlen(sample->channel) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(1, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->codec == NULL ? 0 : strlen(sample->codec) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long VideoFrameMetaV1SerializeKey(const VideoFrameMetaV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long VideoFrameMetaV1DeserializeKey(
    VideoFrameMetaV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong VideoFrameMetaV1GetSerializedKeySize(const VideoFrameMetaV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* VideoFrameMetaV1LoanSampleBuf(VideoFrameMetaV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void VideoFrameMetaV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long VideoFrameMetaV1LoanDeserialize(VideoFrameMetaV1* sampleBuf,
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
DDS_Long VideoFrameMetaV1OnSiteDeserialize(CDRDeserializer* cdr,
    VideoFrameMetaV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean VideoFrameMetaV1NoSerializingSupported()
{
    return false;
}

DDS_ULong VideoFrameMetaV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
#define T AnnotationFrameV1
#define TSeq AnnotationFrameV1Seq
#define TINITIALIZE AnnotationFrameV1InitializeEx
#define TFINALIZE AnnotationFrameV1FinalizeEx
#define TCOPY AnnotationFrameV1CopyEx

#include "ZRSequence.cpp"
#include "ZRCPlusPlusSequence.cpp"

#undef TCOPY
#undef TFINALIZE
#undef TINITIALIZE
#undef TSeq
#undef T

DDS_Boolean AnnotationFrameV1Initialize(AnnotationFrameV1* self)
{
    return AnnotationFrameV1InitializeEx(self, NULL, true);
}

void AnnotationFrameV1Finalize(AnnotationFrameV1* self)
{
    AnnotationFrameV1FinalizeEx(self, NULL, true);
}

DDS_Boolean AnnotationFrameV1Copy(
    AnnotationFrameV1* dst,
    const AnnotationFrameV1* src)
{
    return AnnotationFrameV1CopyEx(dst, src, NULL);
}

AnnotationFrameV1* AnnotationFrameV1CreateSample(
    ZRMemPool* pool,
    DDS_Boolean allocMutable)
{
    AnnotationFrameV1* newSample = (AnnotationFrameV1*)ZRMalloc(pool, sizeof(AnnotationFrameV1));
    if (newSample == NULL)
    {
        printf("malloc for AnnotationFrameV1 failed.");
        return NULL;
    }
    if (!AnnotationFrameV1InitializeEx(newSample, pool, allocMutable))
    {
        printf("initial Sample failed.");
        AnnotationFrameV1DestroySample(pool, newSample);
        return NULL;
    }
    return newSample;
}

void AnnotationFrameV1DestroySample(ZRMemPool* pool, AnnotationFrameV1* sample)
{
    if (sample == NULL) return;
    AnnotationFrameV1FinalizeEx(sample, pool, true);
    ZRDealloc(pool, sample);
}

DDS_ULong AnnotationFrameV1GetSerializedSampleMaxSize()
{
    return 32821;
}

DDS_ULong AnnotationFrameV1GetSerializedKeyMaxSize()
{
    return 8;
}

DDS_Long AnnotationFrameV1GetKeyHash(
    const AnnotationFrameV1* sample,
    CDRSerializer* cdr,
    DDS::KeyHash_t* result)
{
    DDS_Long ret = AnnotationFrameV1SerializeKey(sample, cdr);
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

DDS_Boolean AnnotationFrameV1HasKey()
{
    return true;
}

TypeCodeHeader* AnnotationFrameV1GetInnerTypeCode()
{
#ifdef _ZRDDS_INCLUDE_TYPECODE
    DDS::TypeCode* userTypeCode = AnnotationFrameV1GetTypeCode();
    if (userTypeCode == NULL) return NULL;
    return userTypeCode->getImpl();
#else
    return NULL;
#endif
}

DDS_Boolean AnnotationFrameV1InitializeEx(
    AnnotationFrameV1* self,
    ZRMemPool* pool,
    DDS_Boolean allocateMemory)
{
    self->platID = 0;

    self->sensorID = 0;

    self->channel = NULL;

    self->frameSeq = 0;

    self->currentRound = 0;

    self->ptsMs = 0;

    self->json = NULL;

    if (allocateMemory)
    {
        self->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
        if (self->channel == NULL)
        {
            printf("Malloc for self->channel failed.");
            return false;
        }
        self->channel[0] = '\0';
        self->json = (DDS_Char*) ZRMalloc(pool, 32768 + 1);
        if (self->json == NULL)
        {
            printf("Malloc for self->json failed.");
            return false;
        }
        self->json[0] = '\0';
    }
    else
    {
        if (self->channel != NULL)
        {
            self->channel[0] = '\0';
        }
        if (self->json != NULL)
        {
            self->json[0] = '\0';
        }
    }
    return true;
}

void AnnotationFrameV1FinalizeEx(
    AnnotationFrameV1* self,
    ZRMemPool* pool,
    DDS_Boolean deletePointers)
{
    if (deletePointers)
    {
        ZRDealloc(pool, self->channel);
        self->channel = NULL;
        ZRDealloc(pool, self->json);
        self->json = NULL;
    }
}

DDS_Boolean AnnotationFrameV1CopyEx(
    AnnotationFrameV1* dst,
    const AnnotationFrameV1* src,
    ZRMemPool* pool)
{
    dst->platID = src->platID;
    dst->sensorID = src->sensorID;
    if (src->channel == NULL)
    {
        ZRDealloc(pool, dst->channel);
        dst->channel = NULL;
    }
    else
    {
        if (dst->channel == NULL)
        {
            dst->channel = (DDS_Char*) ZRMalloc(pool, 16 + 1);
            if (dst->channel == NULL)
            {
                printf("malloc for channel failed.");
                return false;
            }
        }
        strcpy(dst->channel, src->channel);
    }
    dst->frameSeq = src->frameSeq;
    dst->currentRound = src->currentRound;
    dst->ptsMs = src->ptsMs;
    if (src->json == NULL)
    {
        ZRDealloc(pool, dst->json);
        dst->json = NULL;
    }
    else
    {
        if (dst->json == NULL)
        {
            dst->json = (DDS_Char*) ZRMalloc(pool, 32768 + 1);
            if (dst->json == NULL)
            {
                printf("malloc for json failed.");
                return false;
            }
        }
        strcpy(dst->json, src->json);
    }
    return true;
}

void AnnotationFrameV1PrintData(const AnnotationFrameV1 *sample)
{
    if (sample == NULL)
    {
        printf("NULL\n");
        return;
    }
    printf("sample->platID: %d\n", sample->platID);
    printf("\n");

    printf("sample->sensorID: %d\n", sample->sensorID);
    printf("\n");

    if (sample->channel != NULL)
    {
        printf("sample->channel(%d): %s\n", strlen(sample->channel), sample->channel);
    }
    else
    {
        printf("sample->channel(0): NULL\n");
    }
    printf("\n");

    printf("sample->frameSeq: %u\n", sample->frameSeq);
    printf("\n");

    printf("sample->currentRound: %d\n", sample->currentRound);
    printf("\n");

    printf("sample->ptsMs: %lf\n", sample->ptsMs);
    printf("\n");

    if (sample->json != NULL)
    {
        printf("sample->json(%d): %s\n", strlen(sample->json), sample->json);
    }
    else
    {
        printf("sample->json(0): NULL\n");
    }
    printf("\n");

}

DDS::TypeCode* AnnotationFrameV1GetTypeCode()
{
    static DDS::TypeCode* s_typeCode = NULL;
    if (s_typeCode != NULL) return s_typeCode;
    DDS::TypeCodeFactory &factory = DDS::TypeCodeFactory::getInstance();

    s_typeCode = factory.createStructTC(
        "HwaSimIRDds::AnnotationFrameV1",
        DDS_EXTENSIBLE_EXTENSIBILITY);
    if (s_typeCode == NULL)
    {
        printf("create struct AnnotationFrameV1 typecode failed.");
        return s_typeCode;
    }
    DDS_Long ret = 0;
    DDS::TypeCode* memberTc = NULL;
    DDS::TypeCode* eleTc = NULL;

    memberTc = factory.getPrimitiveTC(DDS_TK_INT);
    if (memberTc == NULL)
    {
        printf("Get Member platID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        0,
        0,
        "platID",
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
        printf("Get Member sensorID TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        1,
        1,
        "sensorID",
        memberTc,
        true,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(16);
    if (memberTc == NULL)
    {
        printf("Get Member channel TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        2,
        2,
        "channel",
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

    memberTc = factory.getPrimitiveTC(DDS_TK_UINT);
    if (memberTc == NULL)
    {
        printf("Get Member frameSeq TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        3,
        3,
        "frameSeq",
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
        printf("Get Member currentRound TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        4,
        4,
        "currentRound",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.getPrimitiveTC(DDS_TK_DOUBLE);
    if (memberTc == NULL)
    {
        printf("Get Member ptsMs TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        5,
        5,
        "ptsMs",
        memberTc,
        false,
        false);
    if (ret < 0)
    {
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }

    memberTc = factory.createStringTC(32768);
    if (memberTc == NULL)
    {
        printf("Get Member json TypeCode failed.");
        factory.deleteTC(s_typeCode);
        s_typeCode = NULL;
        return NULL;
    }
    ret = s_typeCode->addMemberToStruct(
        6,
        6,
        "json",
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

DDS_Long AnnotationFrameV1Serialize(const AnnotationFrameV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->channel, sample->channel == NULL ? 0 : strlen(sample->channel) + 1))
    {
        printf("serialize sample->channel failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->frameSeq, 4))
    {
        printf("serialize sample->frameSeq failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        printf("serialize sample->currentRound failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->ptsMs, 8))
    {
        printf("serialize sample->ptsMs failed.");
        return -2;
    }

    if (!CDRSerializerPutString(cdr, (DDS_Char*) sample->json, sample->json == NULL ? 0 : strlen(sample->json) + 1))
    {
        printf("serialize sample->json failed.");
        return -2;
    }

    return 0;
}

DDS_Long AnnotationFrameV1Deserialize(
    AnnotationFrameV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    //has key :last key name:sensorID
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }
    //last key :sensorID has been deserialized
    DDS_ULong channelTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &channelTmpLen, 4))
    {
        sample->channel = NULL;
        sample->frameSeq = 0;
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->json = NULL;
        return 0;
    }
    if (0 == channelTmpLen)
    {
        ZRDealloc(pool, sample->channel);
        sample->channel = NULL;
    }
    else
    {
        if (sample->channel == NULL)
        {
            sample->channel = (DDS_Char*) ZRMalloc(pool, channelTmpLen);
            if (sample->channel == NULL)
            {
                printf("malloc for sample->channel failed(%d).", channelTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->channel, channelTmpLen, 1))
        {
            printf("deserialize member sample->channel failed.");
            return -4;
        }
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->frameSeq, 4))
    {
        sample->frameSeq = 0;
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->json = NULL;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->currentRound, 4))
    {
        sample->currentRound = 0;
        sample->ptsMs = 0;
        sample->json = NULL;
        return 0;
    }
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->ptsMs, 8))
    {
        sample->ptsMs = 0;
        sample->json = NULL;
        return 0;
    }
    DDS_ULong jsonTmpLen = 0;
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &jsonTmpLen, 4))
    {
        sample->json = NULL;
        return 0;
    }
    if (0 == jsonTmpLen)
    {
        ZRDealloc(pool, sample->json);
        sample->json = NULL;
    }
    else
    {
        if (sample->json == NULL)
        {
            sample->json = (DDS_Char*) ZRMalloc(pool, jsonTmpLen);
            if (sample->json == NULL)
            {
                printf("malloc for sample->json failed(%d).", jsonTmpLen);
                return -3;
            }
        }
        if (!CDRDeserializerGetUntypeArray(cdr, (DDS_Octet*)sample->json, jsonTmpLen, 1))
        {
            printf("deserialize member sample->json failed.");
            return -4;
        }
    }
    return 0;
}

DDS_ULong AnnotationFrameV1GetSerializedSampleSize(const AnnotationFrameV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->channel == NULL ? 0 : strlen(sample->channel) + 1, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(8, currentAlignment);

    currentAlignment += CDRSerializerGetStringSize(sample->json == NULL ? 0 : strlen(sample->json) + 1, currentAlignment);

    return currentAlignment - initialAlignment;
}

DDS_Long AnnotationFrameV1SerializeKey(const AnnotationFrameV1* sample, CDRSerializer *cdr)
{
    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("serialize sample->platID failed.");
        return -2;
    }

    if (!CDRSerializerPutUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("serialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_Long AnnotationFrameV1DeserializeKey(
    AnnotationFrameV1* sample,
    CDRDeserializer* cdr,
    ZRMemPool* pool)
{
    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->platID, 4))
    {
        printf("deserialize sample->platID failed.");
        return -2;
    }

    if (!CDRDeserializerGetUntype(cdr, (DDS_Octet*) &sample->sensorID, 4))
    {
        printf("deserialize sample->sensorID failed.");
        return -2;
    }

    return 0;
}

DDS_ULong AnnotationFrameV1GetSerializedKeySize(const AnnotationFrameV1* sample, DDS_ULong currentAlignment)
{
    DDS_ULong initialAlignment = currentAlignment;

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    currentAlignment += CDRSerializerGetUntypeSize(4, currentAlignment);

    return currentAlignment - initialAlignment;
}

#ifdef _ZRDDS_INCLUDE_NO_SERIALIZE_MODE
DDS_Char* AnnotationFrameV1LoanSampleBuf(AnnotationFrameV1* sample, DDS_Boolean takeBuffer)
{
    return NULL;
}

void AnnotationFrameV1ReturnSampleBuf(DDS_Char* sampleBuf)
{
    ;
}

DDS_Long AnnotationFrameV1LoanDeserialize(AnnotationFrameV1* sampleBuf,
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
DDS_Long AnnotationFrameV1OnSiteDeserialize(CDRDeserializer* cdr,
    AnnotationFrameV1* sample,
    DDS_ULong offset,
    DDS_ULong totalSize,
    DDS_Char* payload,
    DDS_ULong payloadLen,
    DDS_ULong fixedHeaderLen)
{
    return 0;
}

DDS_Boolean AnnotationFrameV1NoSerializingSupported()
{
    return false;
}

DDS_ULong AnnotationFrameV1FixedHeaderLength()
{
    return 0;
}

#endif/*_ZRDDS_INCLUDE_ONSITE_DESERILIZE*/
}
