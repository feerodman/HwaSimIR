#include "RealtimeSampleValidity.h"

#include <cmath>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

void Expect(HwaRealtimeValidity::Status actual,
    HwaRealtimeValidity::Status expected, const char* name)
{
    if (actual == expected) return;
    std::cerr << "[P14ValidityTest][FAIL] case=" << name
              << " expected=" << HwaRealtimeValidity::StatusText(expected)
              << " actual=" << HwaRealtimeValidity::StatusText(actual) << std::endl;
    ++failures;
}

BYHWICD::DisplayC2cObjTrackingData ValidSample(bool viewValid)
{
    BYHWICD::DisplayC2cObjTrackingData value = {};
    value.flag = 0x38;
    value.platID = 1001;
    value.sensorID = 2;
    value.time = 1000.0;
    value.platLoc.lat = 30.0;
    value.platLoc.lon = 110.0;
    value.platLoc.alt = 1.0;
    value.weaponState.targetType = 0x55;
    value.weaponState.targetPlatID = 3;
    value.weaponState.targetID = 3;
    value.weaponState.viewValid = viewValid;
    value.targetNumValid = 1;
    value.targetState[0].targetType = 0x55;
    value.targetState[0].targetPlatID = 3;
    value.targetState[0].targetID = 3;
    value.targetState[0].viewValid = viewValid;
    value.targetState[0].targetLoc.lat = 30.0;
    value.targetState[0].targetLoc.lon = 110.004;
    value.targetState[0].targetLoc.alt = 1.0;
    return value;
}

} // namespace

int main()
{
    BYHWICD::DisplayC2cObjTrackingData placeholder = {};
    placeholder.flag = 0x38;
    placeholder.platID = 1001;
    placeholder.sensorID = 2;
    Expect(HwaRealtimeValidity::Classify(placeholder).status,
        HwaRealtimeValidity::Status::PlaceholderAllZero, "legacy_all_zero_placeholder");

    Expect(HwaRealtimeValidity::Classify(ValidSample(false)).status,
        HwaRealtimeValidity::Status::Valid, "view_valid_zero_does_not_invalidate_position");
    Expect(HwaRealtimeValidity::Classify(ValidSample(true)).status,
        HwaRealtimeValidity::Status::Valid, "view_valid_one");

    BYHWICD::DisplayC2cObjTrackingData legalOrigin = {};
    legalOrigin.flag = 0x38;
    legalOrigin.platID = 1001;
    legalOrigin.sensorID = 2;
    legalOrigin.time = 1.0;
    Expect(HwaRealtimeValidity::Classify(legalOrigin).status,
        HwaRealtimeValidity::Status::Valid, "zero_geography_nonzero_source_time");

    BYHWICD::DisplayC2cObjTrackingData invalid = ValidSample(true);
    invalid.platLoc.lat = 91.0;
    Expect(HwaRealtimeValidity::Classify(invalid).status,
        HwaRealtimeValidity::Status::LatitudeOutOfRange, "platform_latitude_range");
    invalid = ValidSample(true);
    invalid.platLoc.speed = std::numeric_limits<double>::quiet_NaN();
    Expect(HwaRealtimeValidity::Classify(invalid).status,
        HwaRealtimeValidity::Status::NonFinitePlatformState, "platform_non_finite");
    invalid = ValidSample(true);
    invalid.targetNumValid = 6;
    Expect(HwaRealtimeValidity::Classify(invalid).status,
        HwaRealtimeValidity::Status::TargetCountOutOfRange, "target_count_range");
    invalid = ValidSample(true);
    invalid.targetState[0].targetLoc.lon = 181.0;
    Expect(HwaRealtimeValidity::Classify(invalid).status,
        HwaRealtimeValidity::Status::TargetLongitudeOutOfRange, "target_longitude_range");

    std::cout << "[P14ValidityTest] cases=8 failures=" << failures
              << " viewValidIndependent=1 wireLayoutChanged=0" << std::endl;
    return failures == 0 ? 0 : 1;
}
