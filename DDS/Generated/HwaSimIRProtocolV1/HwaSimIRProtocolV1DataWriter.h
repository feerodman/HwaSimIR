#ifndef HwaSimIRProtocolV1DataWriter_h__
#define HwaSimIRProtocolV1DataWriter_h__
/*************************************************************/
/*           此文件由编译器生成，请勿随意修改                */
/*************************************************************/

#include "HwaSimIRProtocolV1.h"
#include "ZRDDSDataWriter.h"

namespace HwaSimIRDds
{

typedef DDS::ZRDDSDataWriter<SpatialStateV1> SpatialStateV1DataWriter;


typedef DDS::ZRDDSDataWriter<TrackerSensorParamV1> TrackerSensorParamV1DataWriter;


typedef DDS::ZRDDSDataWriter<InitObjectTrackingParamV1> InitObjectTrackingParamV1DataWriter;


typedef DDS::ZRDDSDataWriter<PlatParamPakV1> PlatParamPakV1DataWriter;


typedef DDS::ZRDDSDataWriter<WeaponStateV1> WeaponStateV1DataWriter;


typedef DDS::ZRDDSDataWriter<TargetStateV1> TargetStateV1DataWriter;


typedef DDS::ZRDDSDataWriter<ControlCommandV1> ControlCommandV1DataWriter;


typedef DDS::ZRDDSDataWriter<InitCommandV1> InitCommandV1DataWriter;


typedef DDS::ZRDDSDataWriter<RealtimeDataV1> RealtimeDataV1DataWriter;


typedef DDS::ZRDDSDataWriter<InitAckV1> InitAckV1DataWriter;


typedef DDS::ZRDDSDataWriter<VideoStatusV1> VideoStatusV1DataWriter;

}
#endif

