#include "CommonDataDdsAdapter.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int g_failures = 0;

template <typename T, typename U>
void CheckEqual(const T& actual, const U& expected, const char* field)
{
    if (!(actual == expected)) {
        std::cerr << "FAIL field=" << field << " actual=" << actual
                  << " expected=" << expected << std::endl;
        ++g_failures;
    }
}

#define CHECK_FIELD(ACTUAL, EXPECTED, FIELD) \
    CheckEqual((ACTUAL).FIELD, (EXPECTED).FIELD, #FIELD)

void FillSpatial(BYHWICD::SpatialState& value, double base)
{
    value.lat = base + 0.1;
    value.lon = base + 0.2;
    value.alt = base + 0.3;
    value.yaw = base + 0.4;
    value.pitch = base + 0.5;
    value.roll = base + 0.6;
    value.speed = base + 0.7;
}

void CheckSpatial(const BYHWICD::SpatialState& actual,
                  const BYHWICD::SpatialState& expected)
{
    CHECK_FIELD(actual, expected, lat);
    CHECK_FIELD(actual, expected, lon);
    CHECK_FIELD(actual, expected, alt);
    CHECK_FIELD(actual, expected, yaw);
    CHECK_FIELD(actual, expected, pitch);
    CHECK_FIELD(actual, expected, roll);
    CHECK_FIELD(actual, expected, speed);
}

void FillTracker(BYHWICD::trackerSensorParam& value)
{
    value.h264En = true;
    value.noiseEn = true;
    value.trackerSensorNoise = 1.125;
    value.realtimeAnnotation = true;
    value.saveMP4En = true;
    value.trackerSensorBand = 2;
    value.trackerSensorWidth = 801;
    value.trackerSensorHeight = 799;
    value.trackerSensorViewMin = 123;
    value.trackerSensorViewMax = 45678;
    value.trackerSensorPixelAngle = 0.03125;
    value.trackerX = 11.1;
    value.trackerY = 12.2;
    value.trackerZ = 13.3;
    value.trackerPitch = 14.4;
    value.trackerYaw = 15.5;
    value.trackerRoll = 16.6;
    value.illuminatorX = 21.1;
    value.illuminatorY = 22.2;
    value.illuminatorZ = 23.3;
    value.illuminatorPitch = 24.4;
    value.illuminatorYaw = 25.5;
    value.illuminatorRoll = 26.6;
    value.illuminatorAngle = 27.7;
    value.illuminatorSpotRad = 28.8;
    value.emitterSpotRadius = 29;
    value.emitterSpotRad = 30.5;
}

void CheckTracker(const BYHWICD::trackerSensorParam& actual,
                  const BYHWICD::trackerSensorParam& expected)
{
    CHECK_FIELD(actual, expected, h264En);
    CHECK_FIELD(actual, expected, noiseEn);
    CHECK_FIELD(actual, expected, trackerSensorNoise);
    CHECK_FIELD(actual, expected, realtimeAnnotation);
    CHECK_FIELD(actual, expected, saveMP4En);
    CHECK_FIELD(actual, expected, trackerSensorBand);
    CHECK_FIELD(actual, expected, trackerSensorWidth);
    CHECK_FIELD(actual, expected, trackerSensorHeight);
    CHECK_FIELD(actual, expected, trackerSensorViewMin);
    CHECK_FIELD(actual, expected, trackerSensorViewMax);
    CHECK_FIELD(actual, expected, trackerSensorPixelAngle);
    CHECK_FIELD(actual, expected, trackerX);
    CHECK_FIELD(actual, expected, trackerY);
    CHECK_FIELD(actual, expected, trackerZ);
    CHECK_FIELD(actual, expected, trackerPitch);
    CHECK_FIELD(actual, expected, trackerYaw);
    CHECK_FIELD(actual, expected, trackerRoll);
    CHECK_FIELD(actual, expected, illuminatorX);
    CHECK_FIELD(actual, expected, illuminatorY);
    CHECK_FIELD(actual, expected, illuminatorZ);
    CHECK_FIELD(actual, expected, illuminatorPitch);
    CHECK_FIELD(actual, expected, illuminatorYaw);
    CHECK_FIELD(actual, expected, illuminatorRoll);
    CHECK_FIELD(actual, expected, illuminatorAngle);
    CHECK_FIELD(actual, expected, illuminatorSpotRad);
    CHECK_FIELD(actual, expected, emitterSpotRadius);
    CHECK_FIELD(actual, expected, emitterSpotRad);
}

void FillInit(BYHWICD::InitP2cObjectTrackingCmd& value)
{
    value.flag = 0x36;
    value.JB = 2;
    value.platID = 101;
    value.sensorID = 202;
    value.platParamInit.id = 303;
    value.platParamInit.type = 404;
    FillSpatial(value.platParamInit.spatial, 10.0);
    value.trackingInit.enable = true;
    value.trackingInit.envTerrain = 11;
    value.trackingInit.envSky = 12;
    value.trackingInit.envMaxHeightRain = 13.1;
    value.trackingInit.envTransHeightRain = 14.2;
    value.trackingInit.envMaxHeightSnow = 15.3;
    value.trackingInit.envTransHeightSnow = 16.4;
    value.trackingInit.envRainSnowSpeedScale = 17.5;
    value.trackingInit.envRadScaleTerrain = 18.6;
    value.trackingInit.envRadScaleSky = 19.7;
    value.trackingInit.envTemp = 20.8;
    value.trackingInit.envHumidity = 21.9;
    value.trackingInit.envVisibility = 22000.0;
    value.trackingInit.envWindV = 23.1;
    value.trackingInit.envWindDir = 24.2;
    value.trackingInit.simMode = 2;
    value.trackingInit.videoFps = 60;
    FillTracker(value.trackingInit.trackerSensor[0]);
    value.MissileMaxCount120 = 31;
    value.MissileMaxCount9 = 32;
    value.MissileMaxCountMMD = 33;
    value.MissileMaxCountF35 = 34;
    value.MissileMaxCountF22 = 35;
    value.MissileMaxCountResv1 = 36;
    value.MissileMaxCountResv2 = 37;
}

void CheckInit(const BYHWICD::InitP2cObjectTrackingCmd& actual,
               const BYHWICD::InitP2cObjectTrackingCmd& expected)
{
    CHECK_FIELD(actual, expected, flag);
    CHECK_FIELD(actual, expected, JB);
    CHECK_FIELD(actual, expected, platID);
    CHECK_FIELD(actual, expected, sensorID);
    CHECK_FIELD(actual.platParamInit, expected.platParamInit, id);
    CHECK_FIELD(actual.platParamInit, expected.platParamInit, type);
    CheckSpatial(actual.platParamInit.spatial, expected.platParamInit.spatial);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, enable);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envTerrain);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envSky);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envMaxHeightRain);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envTransHeightRain);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envMaxHeightSnow);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envTransHeightSnow);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envRainSnowSpeedScale);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envRadScaleTerrain);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envRadScaleSky);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envTemp);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envHumidity);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envVisibility);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envWindV);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, envWindDir);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, simMode);
    CHECK_FIELD(actual.trackingInit, expected.trackingInit, videoFps);
    CheckTracker(actual.trackingInit.trackerSensor[0], expected.trackingInit.trackerSensor[0]);
    CHECK_FIELD(actual, expected, MissileMaxCount120);
    CHECK_FIELD(actual, expected, MissileMaxCount9);
    CHECK_FIELD(actual, expected, MissileMaxCountMMD);
    CHECK_FIELD(actual, expected, MissileMaxCountF35);
    CHECK_FIELD(actual, expected, MissileMaxCountF22);
    CHECK_FIELD(actual, expected, MissileMaxCountResv1);
    CHECK_FIELD(actual, expected, MissileMaxCountResv2);
}

void FillRealtime(BYHWICD::DisplayC2cObjTrackingData& value)
{
    value.flag = 0x38;
    value.platID = 501;
    value.sensorID = 502;
    value.time = 503.125;
    FillSpatial(value.platLoc, 500.0);
    value.weaponState.targetType = 0x11;
    value.weaponState.targetPlatID = 601;
    value.weaponState.targetID = 602;
    value.weaponState.xxOutAng[0] = 603.1;
    value.weaponState.xxOutAng[1] = 604.2;
    value.weaponState.lookatEn = true;
    value.weaponState.illuminatorEn = true;
    value.weaponState.offsetAng[0] = 605.3;
    value.weaponState.offsetAng[1] = 606.4;
    value.weaponState.viewValid = true;
    value.weaponState.damageFlag = 607;
    value.weaponState.strikeFlag = true;
    value.weaponState.strikePart = 608;
    value.targetNumValid = 5;
    for (int i = 0; i < 5; ++i) {
        BYHWICD::TargetState& target = value.targetState[i];
        target.targetType = 0x11 + i;
        target.targetPlatID = 700 + i;
        target.targetID = 800 + i;
        target.engineState = (i % 2) == 0;
        target.viewValid = true;
        FillSpatial(target.targetLoc, 900.0 + i * 10.0);
        target.targetState = 0x01 + i;
    }
}

void CheckRealtime(const BYHWICD::DisplayC2cObjTrackingData& actual,
                   const BYHWICD::DisplayC2cObjTrackingData& expected)
{
    CHECK_FIELD(actual, expected, flag);
    CHECK_FIELD(actual, expected, platID);
    CHECK_FIELD(actual, expected, sensorID);
    CHECK_FIELD(actual, expected, time);
    CheckSpatial(actual.platLoc, expected.platLoc);
    CHECK_FIELD(actual.weaponState, expected.weaponState, targetType);
    CHECK_FIELD(actual.weaponState, expected.weaponState, targetPlatID);
    CHECK_FIELD(actual.weaponState, expected.weaponState, targetID);
    for (int i = 0; i < 2; ++i) {
        CheckEqual(actual.weaponState.xxOutAng[i], expected.weaponState.xxOutAng[i], "weaponState.xxOutAng");
        CheckEqual(actual.weaponState.offsetAng[i], expected.weaponState.offsetAng[i], "weaponState.offsetAng");
    }
    CHECK_FIELD(actual.weaponState, expected.weaponState, lookatEn);
    CHECK_FIELD(actual.weaponState, expected.weaponState, illuminatorEn);
    CHECK_FIELD(actual.weaponState, expected.weaponState, viewValid);
    CHECK_FIELD(actual.weaponState, expected.weaponState, damageFlag);
    CHECK_FIELD(actual.weaponState, expected.weaponState, strikeFlag);
    CHECK_FIELD(actual.weaponState, expected.weaponState, strikePart);
    CHECK_FIELD(actual, expected, targetNumValid);
    for (int i = 0; i < 5; ++i) {
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], targetType);
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], targetPlatID);
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], targetID);
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], engineState);
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], viewValid);
        CheckSpatial(actual.targetState[i].targetLoc, expected.targetState[i].targetLoc);
        CHECK_FIELD(actual.targetState[i], expected.targetState[i], targetState);
    }
}

void CheckLegacySizes()
{
    CheckEqual(sizeof(BYHWICD::ControlP2cX1ObjTrackingCmd), size_t(24), "legacy.Control.size");
    CheckEqual(sizeof(BYHWICD::InitP2cObjectTrackingCmd), size_t(385), "legacy.Init.size");
    CheckEqual(sizeof(BYHWICD::DisplayC2cObjTrackingData), size_t(506), "legacy.Realtime.size");
    CheckEqual(sizeof(BYHWICD::InitAckC2pObjectTrackingCmd), size_t(17), "legacy.Ack.size");
}

} // namespace

int main()
{
    CheckLegacySizes();

    BYHWICD::ControlP2cX1ObjTrackingCmd control = {};
    control.flag = 0x41;
    control.JB = 2;
    control.platID = 101;
    control.simCommand = 3;
    control.roundCut = 20;
    control.currentRound = 7;
    const BYHWICD::ControlP2cX1ObjTrackingCmd controlResult =
        HwaSimIRDdsAdapter::FromDds(HwaSimIRDdsAdapter::ToDds(control));
    CHECK_FIELD(controlResult, control, flag);
    CHECK_FIELD(controlResult, control, JB);
    CHECK_FIELD(controlResult, control, platID);
    CHECK_FIELD(controlResult, control, simCommand);
    CHECK_FIELD(controlResult, control, roundCut);
    CHECK_FIELD(controlResult, control, currentRound);

    BYHWICD::InitP2cObjectTrackingCmd init = {};
    FillInit(init);
    CheckInit(HwaSimIRDdsAdapter::FromDds(HwaSimIRDdsAdapter::ToDds(init)), init);

    BYHWICD::DisplayC2cObjTrackingData realtime = {};
    FillRealtime(realtime);
    CheckRealtime(HwaSimIRDdsAdapter::FromDds(HwaSimIRDdsAdapter::ToDds(realtime)), realtime);

    BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
    ack.flag = 0x37;
    ack.JB = 2;
    ack.platID = 901;
    ack.sensorID = 902;
    ack.trackingReady = true;
    const BYHWICD::InitAckC2pObjectTrackingCmd ackResult =
        HwaSimIRDdsAdapter::FromDds(HwaSimIRDdsAdapter::ToDds(ack));
    CHECK_FIELD(ackResult, ack, flag);
    CHECK_FIELD(ackResult, ack, JB);
    CHECK_FIELD(ackResult, ack, platID);
    CHECK_FIELD(ackResult, ack, sensorID);
    CHECK_FIELD(ackResult, ack, trackingReady);

    if (g_failures != 0) {
        std::cerr << "dds_protocol_roundtrip_test FAIL failures=" << g_failures << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "dds_protocol_roundtrip_test PASS"
              << " sizes=24/385/506/17"
              << " flags=0x41/0x36/0x38/0x37" << std::endl;
    return EXIT_SUCCESS;
}
