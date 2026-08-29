#pragma once

#include "CommonData.h"
#include "HwaSimIRProtocolV1.h"

namespace HwaSimIRDdsAdapter {

HwaSimIRDds::ControlCommandV1 ToDds(
    const BYHWICD::ControlP2cX1ObjTrackingCmd& source);
BYHWICD::ControlP2cX1ObjTrackingCmd FromDds(
    const HwaSimIRDds::ControlCommandV1& source);

HwaSimIRDds::InitCommandV1 ToDds(
    const BYHWICD::InitP2cObjectTrackingCmd& source);
BYHWICD::InitP2cObjectTrackingCmd FromDds(
    const HwaSimIRDds::InitCommandV1& source);

HwaSimIRDds::RealtimeDataV1 ToDds(
    const BYHWICD::DisplayC2cObjTrackingData& source);
BYHWICD::DisplayC2cObjTrackingData FromDds(
    const HwaSimIRDds::RealtimeDataV1& source);

HwaSimIRDds::InitAckV1 ToDds(
    const BYHWICD::InitAckC2pObjectTrackingCmd& source);
BYHWICD::InitAckC2pObjectTrackingCmd FromDds(
    const HwaSimIRDds::InitAckV1& source);

} // namespace HwaSimIRDdsAdapter
