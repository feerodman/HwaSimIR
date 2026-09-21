#pragma once

#include "CommonData.h"

#include <cmath>

// P14 compatibility contract for the unchanged V1 wire layout.
//
// V1 has no independent platform-position-valid bit.  A completely zero
// business sample at source time zero is therefore treated as a legacy
// placeholder.  A real position at latitude/longitude/altitude 0 remains
// valid when the sender supplies a non-zero source time or any explicit
// business state.  ViewValid is intentionally absent from every test below:
// it controls target presentation, never platform position validity.
namespace HwaRealtimeValidity {

enum class Status
{
    Valid,
    PlaceholderAllZero,
    NonFiniteTime,
    NonFinitePlatformState,
    LatitudeOutOfRange,
    LongitudeOutOfRange,
    TargetCountOutOfRange,
    NonFiniteTargetState,
    TargetLatitudeOutOfRange,
    TargetLongitudeOutOfRange
};

struct Result
{
    Status status = Status::Valid;
    int targetIndex = -1;

    bool valid() const { return status == Status::Valid; }
};

inline bool IsFiniteSpatial(const BYHWICD::SpatialState& value)
{
    return std::isfinite(value.lat) && std::isfinite(value.lon) &&
        std::isfinite(value.alt) && std::isfinite(value.yaw) &&
        std::isfinite(value.pitch) && std::isfinite(value.roll) &&
        std::isfinite(value.speed);
}

inline bool IsZeroSpatial(const BYHWICD::SpatialState& value)
{
    return value.lat == 0.0 && value.lon == 0.0 && value.alt == 0.0 &&
        value.yaw == 0.0 && value.pitch == 0.0 && value.roll == 0.0 &&
        value.speed == 0.0;
}

inline bool IsLegacyAllZeroPlaceholder(const BYHWICD::DisplayC2cObjTrackingData& data)
{
    if (data.time != 0.0 || !IsZeroSpatial(data.platLoc) || data.targetNumValid != 0)
        return false;
    const BYHWICD::WeaponState& weapon = data.weaponState;
    if (weapon.targetType != 0 || weapon.targetPlatID != 0 || weapon.targetID != 0 ||
        weapon.xxOutAng[0] != 0.0 || weapon.xxOutAng[1] != 0.0 ||
        weapon.lookatEn || weapon.illuminatorEn ||
        weapon.offsetAng[0] != 0.0 || weapon.offsetAng[1] != 0.0 ||
        weapon.viewValid || weapon.damageFlag != 0 || weapon.strikeFlag ||
        weapon.strikePart != 0)
        return false;
    for (int index = 0; index < 5; ++index)
    {
        const BYHWICD::TargetState& target = data.targetState[index];
        if (target.targetType != 0 || target.targetPlatID != 0 || target.targetID != 0 ||
            target.engineState || target.viewValid || !IsZeroSpatial(target.targetLoc) ||
            target.targetState != 0)
            return false;
    }
    return true;
}

inline Result Classify(const BYHWICD::DisplayC2cObjTrackingData& data)
{
    Result result;
    if (IsLegacyAllZeroPlaceholder(data))
    {
        result.status = Status::PlaceholderAllZero;
        return result;
    }
    if (!std::isfinite(data.time))
    {
        result.status = Status::NonFiniteTime;
        return result;
    }
    if (!IsFiniteSpatial(data.platLoc))
    {
        result.status = Status::NonFinitePlatformState;
        return result;
    }
    if (data.platLoc.lat < -90.0 || data.platLoc.lat > 90.0)
    {
        result.status = Status::LatitudeOutOfRange;
        return result;
    }
    if (data.platLoc.lon < -180.0 || data.platLoc.lon > 180.0)
    {
        result.status = Status::LongitudeOutOfRange;
        return result;
    }
    if (data.targetNumValid < 0 || data.targetNumValid > 5)
    {
        result.status = Status::TargetCountOutOfRange;
        return result;
    }
    for (int index = 0; index < data.targetNumValid; ++index)
    {
        const BYHWICD::SpatialState& target = data.targetState[index].targetLoc;
        result.targetIndex = index;
        if (!IsFiniteSpatial(target))
        {
            result.status = Status::NonFiniteTargetState;
            return result;
        }
        if (target.lat < -90.0 || target.lat > 90.0)
        {
            result.status = Status::TargetLatitudeOutOfRange;
            return result;
        }
        if (target.lon < -180.0 || target.lon > 180.0)
        {
            result.status = Status::TargetLongitudeOutOfRange;
            return result;
        }
    }
    result.targetIndex = -1;
    return result;
}

inline const char* StatusText(Status status)
{
    switch (status)
    {
    case Status::Valid: return "valid";
    case Status::PlaceholderAllZero: return "legacy_all_zero_placeholder";
    case Status::NonFiniteTime: return "non_finite_source_time";
    case Status::NonFinitePlatformState: return "non_finite_platform_state";
    case Status::LatitudeOutOfRange: return "platform_latitude_out_of_range";
    case Status::LongitudeOutOfRange: return "platform_longitude_out_of_range";
    case Status::TargetCountOutOfRange: return "target_count_out_of_range";
    case Status::NonFiniteTargetState: return "non_finite_target_state";
    case Status::TargetLatitudeOutOfRange: return "target_latitude_out_of_range";
    case Status::TargetLongitudeOutOfRange: return "target_longitude_out_of_range";
    }
    return "unknown";
}

} // namespace HwaRealtimeValidity
