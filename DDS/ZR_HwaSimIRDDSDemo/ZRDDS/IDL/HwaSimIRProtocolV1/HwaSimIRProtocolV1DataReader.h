#ifndef HwaSimIRProtocolV1DataReader_h__
#define HwaSimIRProtocolV1DataReader_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "HwaSimIRProtocolV1.h"
#include "ZRDDSDataReader.h"

namespace HwaSimIRDds
{
typedef struct SpatialStateV1Seq SpatialStateV1Seq;

typedef DDS::ZRDDSDataReader<SpatialStateV1, SpatialStateV1Seq> SpatialStateV1DataReader;

typedef struct TrackerSensorParamV1Seq TrackerSensorParamV1Seq;

typedef DDS::ZRDDSDataReader<TrackerSensorParamV1, TrackerSensorParamV1Seq> TrackerSensorParamV1DataReader;

typedef struct InitObjectTrackingParamV1Seq InitObjectTrackingParamV1Seq;

typedef DDS::ZRDDSDataReader<InitObjectTrackingParamV1, InitObjectTrackingParamV1Seq> InitObjectTrackingParamV1DataReader;

typedef struct PlatParamPakV1Seq PlatParamPakV1Seq;

typedef DDS::ZRDDSDataReader<PlatParamPakV1, PlatParamPakV1Seq> PlatParamPakV1DataReader;

typedef struct WeaponStateV1Seq WeaponStateV1Seq;

typedef DDS::ZRDDSDataReader<WeaponStateV1, WeaponStateV1Seq> WeaponStateV1DataReader;

typedef struct TargetStateV1Seq TargetStateV1Seq;

typedef DDS::ZRDDSDataReader<TargetStateV1, TargetStateV1Seq> TargetStateV1DataReader;

typedef struct ControlCommandV1Seq ControlCommandV1Seq;

typedef DDS::ZRDDSDataReader<ControlCommandV1, ControlCommandV1Seq> ControlCommandV1DataReader;

typedef struct InitCommandV1Seq InitCommandV1Seq;

typedef DDS::ZRDDSDataReader<InitCommandV1, InitCommandV1Seq> InitCommandV1DataReader;

typedef struct RealtimeDataV1Seq RealtimeDataV1Seq;

typedef DDS::ZRDDSDataReader<RealtimeDataV1, RealtimeDataV1Seq> RealtimeDataV1DataReader;

typedef struct InitAckV1Seq InitAckV1Seq;

typedef DDS::ZRDDSDataReader<InitAckV1, InitAckV1Seq> InitAckV1DataReader;

typedef struct VideoStatusV1Seq VideoStatusV1Seq;

typedef DDS::ZRDDSDataReader<VideoStatusV1, VideoStatusV1Seq> VideoStatusV1DataReader;

typedef struct VideoFrameMetaV1Seq VideoFrameMetaV1Seq;

typedef DDS::ZRDDSDataReader<VideoFrameMetaV1, VideoFrameMetaV1Seq> VideoFrameMetaV1DataReader;

typedef struct AnnotationFrameV1Seq AnnotationFrameV1Seq;

typedef DDS::ZRDDSDataReader<AnnotationFrameV1, AnnotationFrameV1Seq> AnnotationFrameV1DataReader;

}
#endif

