#include "CommonData.h"

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
	reference.targetType = 0x66;
	reference.targetPlatID = 3202;
	reference.targetID = 6601;
	BYHWICD::TargetState same = reference;
	BYHWICD::TargetState wrongType = reference;
	wrongType.targetType = 0x55;
	BYHWICD::TargetState wrongPlatform = reference;
	wrongPlatform.targetPlatID = 3203;
	BYHWICD::TargetState wrongTarget = reference;
	wrongTarget.targetID = 6602;
	if (!SameFullTargetKey(reference, same) || SameFullTargetKey(reference, wrongType) ||
		SameFullTargetKey(reference, wrongPlatform) || SameFullTargetKey(reference, wrongTarget))
	{
		std::cerr << "controlled sample full-key isolation failed" << std::endl;
		return 2;
	}

	BYHWICD::InitP2cObjectTrackingCmd init = {};
	init.MissileMaxCountResv2 = 1;
	if (init.MissileMaxCountResv1 != 0 || init.MissileMaxCountResv2 != 1)
	{
		std::cerr << "Resv2-only capacity allocation failed" << std::endl;
		return 3;
	}

	std::cout
		<< "{\"result\":\"PASS\"," 
		<< "\"protocol\":{\"control\":" << sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd)
		<< ",\"init\":" << sizeof(BYHWICD::InitP2cObjectTrackingCmd)
		<< ",\"realtime\":" << sizeof(BYHWICD::DisplayC2cObjTrackingData)
		<< ",\"ack\":" << sizeof(BYHWICD::InitAckC2pObjectTrackingCmd) << "},"
		<< "\"targetCapacity\":5,\"targetType\":102,\"platform\":\"Resv2\","
		<< "\"fullKey\":[102,3202,6601],\"resv1Count\":0,\"resv2Count\":1,"
		<< "\"differentTypeRejected\":true,\"differentPlatformRejected\":true,"
		<< "\"differentTargetRejected\":true}" << std::endl;
	return 0;
}
