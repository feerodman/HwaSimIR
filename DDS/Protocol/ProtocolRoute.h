#pragma once

// Pure transport-independent routing used by both legacy UDP and DDS ingress.
// Control has no sensor identity and therefore fans out to every sensor on the
// addressed platform. Init/Realtime use exact sensor identity or the explicit
// sensorID=255 broadcast convention.
enum class ProtocolRouteResult
{
    ExactMatch,
    SensorBroadcast,
    PlatMismatch,
    SensorMismatch
};

inline ProtocolRouteResult EvaluateProtocolRoute(
    int localPlatID,
    int localSensorID,
    bool acceptSensorBroadcast,
    int packetPlatID,
    int packetSensorID,
    bool packetHasSensorID)
{
    if (packetPlatID != localPlatID)
        return ProtocolRouteResult::PlatMismatch;
    if (!packetHasSensorID || packetSensorID == localSensorID)
        return ProtocolRouteResult::ExactMatch;
    if (packetSensorID == 255 && acceptSensorBroadcast)
        return ProtocolRouteResult::SensorBroadcast;
    return ProtocolRouteResult::SensorMismatch;
}

inline bool ProtocolRouteAccepted(ProtocolRouteResult result)
{
    return result == ProtocolRouteResult::ExactMatch ||
        result == ProtocolRouteResult::SensorBroadcast;
}

inline const char* ProtocolRouteReason(ProtocolRouteResult result)
{
    switch (result)
    {
    case ProtocolRouteResult::ExactMatch: return "exact_match";
    case ProtocolRouteResult::SensorBroadcast: return "sensor_broadcast";
    case ProtocolRouteResult::PlatMismatch: return "plat_mismatch";
    case ProtocolRouteResult::SensorMismatch: return "sensor_mismatch";
    }
    return "unknown";
}
