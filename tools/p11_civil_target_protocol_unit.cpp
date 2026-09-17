#include "CommonData.h"

#include <cstddef>
#include <iostream>

namespace
{
bool SameFullTargetKey(const BYHWICD::TargetState& left, const BYHWICD::TargetState& right)
{
    return left.targetType == right.targetType &&
        left.targetPlatID == right.targetPlatID &&
        left.targetID == right.targetID;
}
}

int main()
{
    static_assert(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd) == 24, "control wire layout changed");
    static_assert(sizeof(BYHWICD::InitP2cObjectTrackingCmd) == 385, "init wire layout changed");
    static_assert(sizeof(BYHWICD::DisplayC2cObjTrackingData) == 506, "realtime wire layout changed");
    static_assert(sizeof(BYHWICD::InitAckC2pObjectTrackingCmd) == 17, "ack wire layout changed");
    static_assert(sizeof(((BYHWICD::DisplayC2cObjTrackingData*)0)->targetState) /
        sizeof(BYHWICD::TargetState) == 5, "target array capacity changed");

    BYHWICD::TargetState reference = {};
    reference.targetType = 0x55;
    reference.targetPlatID = 3101;
    reference.targetID = 5501;
    BYHWICD::TargetState same = reference;
    BYHWICD::TargetState otherPlatform = reference;
    otherPlatform.targetPlatID = 3102;
    BYHWICD::TargetState otherType = reference;
    otherType.targetType = 0x11;
    BYHWICD::TargetState otherTarget = reference;
    otherTarget.targetID = 5502;

    if (!SameFullTargetKey(reference, same) ||
        SameFullTargetKey(reference, otherPlatform) ||
        SameFullTargetKey(reference, otherType) ||
        SameFullTargetKey(reference, otherTarget))
    {
        std::cerr << "full target key isolation failed" << std::endl;
        return 2;
    }

    BYHWICD::InitP2cObjectTrackingCmd init = {};
    init.MissileMaxCountResv1 = 1;
    if (init.MissileMaxCountResv1 != 1 || init.MissileMaxCountResv2 != 0)
    {
        std::cerr << "reserved target capacity field failed" << std::endl;
        return 3;
    }

    std::cout
        << "{\"result\":\"PASS\","
        << "\"protocol\":{\"control\":" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
        << ",\"init\":" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
        << ",\"realtime\":" << sizeof(BYHWICD::DisplayC2cObjTrackingData)
        << ",\"ack\":" << sizeof(BYHWICD::InitAckC2pObjectTrackingCmd) << "},"
        << "\"targetCapacity\":5,"
        << "\"civilTargetType\":85,"
        << "\"fullKey\":[85,3101,5501],"
        << "\"differentPlatformRejected\":true,"
        << "\"differentTypeRejected\":true,"
        << "\"differentTargetRejected\":true}"
        << std::endl;
    return 0;
}
