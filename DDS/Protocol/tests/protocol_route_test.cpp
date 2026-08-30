#include "../ProtocolRoute.h"

#include <iostream>

namespace
{
int failures = 0;

void Check(ProtocolRouteResult actual, ProtocolRouteResult expected, const char* name)
{
    if (actual != expected)
    {
        ++failures;
        std::cerr << "[ProtocolRouteTest][FAIL] case=" << name
                  << " actual=" << ProtocolRouteReason(actual)
                  << " expected=" << ProtocolRouteReason(expected) << std::endl;
    }
}
}

int main()
{
    Check(EvaluateProtocolRoute(1001, 2, true, 1001, -1, false),
        ProtocolRouteResult::ExactMatch, "control_same_platform");
    Check(EvaluateProtocolRoute(1001, 2, true, 1002, -1, false),
        ProtocolRouteResult::PlatMismatch, "control_wrong_platform");
    Check(EvaluateProtocolRoute(1001, 2, true, 1001, 2, true),
        ProtocolRouteResult::ExactMatch, "sensor_exact");
    Check(EvaluateProtocolRoute(1001, 2, true, 1001, 1, true),
        ProtocolRouteResult::SensorMismatch, "sensor_mismatch");
    Check(EvaluateProtocolRoute(1001, 2, true, 1002, 2, true),
        ProtocolRouteResult::PlatMismatch, "sensor_platform_mismatch");
    Check(EvaluateProtocolRoute(1001, 2, true, 1001, 255, true),
        ProtocolRouteResult::SensorBroadcast, "broadcast_enabled");
    Check(EvaluateProtocolRoute(1001, 2, false, 1001, 255, true),
        ProtocolRouteResult::SensorMismatch, "broadcast_disabled");
    std::cout << "[ProtocolRouteTest] cases=7 failures=" << failures
              << " status=" << (failures == 0 ? "PASS" : "FAIL") << std::endl;
    return failures == 0 ? 0 : 1;
}
