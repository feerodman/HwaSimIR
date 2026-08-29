#include "CommonDataDdsAdapter.h"

namespace {

HwaSimIRDds::SpatialStateV1 ToDds(const BYHWICD::SpatialState& source)
{
    HwaSimIRDds::SpatialStateV1 target = {};
    target.lat = source.lat;
    target.lon = source.lon;
    target.alt = source.alt;
    target.yaw = source.yaw;
    target.pitch = source.pitch;
    target.roll = source.roll;
    target.speed = source.speed;
    return target;
}

BYHWICD::SpatialState FromDds(const HwaSimIRDds::SpatialStateV1& source)
{
    BYHWICD::SpatialState target = {};
    target.lat = source.lat;
    target.lon = source.lon;
    target.alt = source.alt;
    target.yaw = source.yaw;
    target.pitch = source.pitch;
    target.roll = source.roll;
    target.speed = source.speed;
    return target;
}

HwaSimIRDds::TrackerSensorParamV1 ToDds(const BYHWICD::trackerSensorParam& source)
{
    HwaSimIRDds::TrackerSensorParamV1 target = {};
    target.h264En = source.h264En;
    target.noiseEn = source.noiseEn;
    target.trackerSensorNoise = source.trackerSensorNoise;
    target.realtimeAnnotation = source.realtimeAnnotation;
    target.saveMP4En = source.saveMP4En;
    target.trackerSensorBand = source.trackerSensorBand;
    target.trackerSensorWidth = source.trackerSensorWidth;
    target.trackerSensorHeight = source.trackerSensorHeight;
    target.trackerSensorViewMin = source.trackerSensorViewMin;
    target.trackerSensorViewMax = source.trackerSensorViewMax;
    target.trackerSensorPixelAngle = source.trackerSensorPixelAngle;
    target.trackerX = source.trackerX;
    target.trackerY = source.trackerY;
    target.trackerZ = source.trackerZ;
    target.trackerPitch = source.trackerPitch;
    target.trackerYaw = source.trackerYaw;
    target.trackerRoll = source.trackerRoll;
    target.illuminatorX = source.illuminatorX;
    target.illuminatorY = source.illuminatorY;
    target.illuminatorZ = source.illuminatorZ;
    target.illuminatorPitch = source.illuminatorPitch;
    target.illuminatorYaw = source.illuminatorYaw;
    target.illuminatorRoll = source.illuminatorRoll;
    target.illuminatorAngle = source.illuminatorAngle;
    target.illuminatorSpotRad = source.illuminatorSpotRad;
    target.emitterSpotRadius = source.emitterSpotRadius;
    target.emitterSpotRad = source.emitterSpotRad;
    return target;
}

BYHWICD::trackerSensorParam FromDds(const HwaSimIRDds::TrackerSensorParamV1& source)
{
    BYHWICD::trackerSensorParam target = {};
    target.h264En = source.h264En != 0;
    target.noiseEn = source.noiseEn != 0;
    target.trackerSensorNoise = source.trackerSensorNoise;
    target.realtimeAnnotation = source.realtimeAnnotation != 0;
    target.saveMP4En = source.saveMP4En != 0;
    target.trackerSensorBand = source.trackerSensorBand;
    target.trackerSensorWidth = source.trackerSensorWidth;
    target.trackerSensorHeight = source.trackerSensorHeight;
    target.trackerSensorViewMin = source.trackerSensorViewMin;
    target.trackerSensorViewMax = source.trackerSensorViewMax;
    target.trackerSensorPixelAngle = source.trackerSensorPixelAngle;
    target.trackerX = source.trackerX;
    target.trackerY = source.trackerY;
    target.trackerZ = source.trackerZ;
    target.trackerPitch = source.trackerPitch;
    target.trackerYaw = source.trackerYaw;
    target.trackerRoll = source.trackerRoll;
    target.illuminatorX = source.illuminatorX;
    target.illuminatorY = source.illuminatorY;
    target.illuminatorZ = source.illuminatorZ;
    target.illuminatorPitch = source.illuminatorPitch;
    target.illuminatorYaw = source.illuminatorYaw;
    target.illuminatorRoll = source.illuminatorRoll;
    target.illuminatorAngle = source.illuminatorAngle;
    target.illuminatorSpotRad = source.illuminatorSpotRad;
    target.emitterSpotRadius = source.emitterSpotRadius;
    target.emitterSpotRad = source.emitterSpotRad;
    return target;
}

HwaSimIRDds::InitObjectTrackingParamV1 ToDds(
    const BYHWICD::InitObjectTrackingParam& source)
{
    HwaSimIRDds::InitObjectTrackingParamV1 target = {};
    target.enable = source.enable;
    target.envTerrain = source.envTerrain;
    target.envSky = source.envSky;
    target.envMaxHeightRain = source.envMaxHeightRain;
    target.envTransHeightRain = source.envTransHeightRain;
    target.envMaxHeightSnow = source.envMaxHeightSnow;
    target.envTransHeightSnow = source.envTransHeightSnow;
    target.envRainSnowSpeedScale = source.envRainSnowSpeedScale;
    target.envRadScaleTerrain = source.envRadScaleTerrain;
    target.envRadScaleSky = source.envRadScaleSky;
    target.envTemp = source.envTemp;
    target.envHumidity = source.envHumidity;
    target.envVisibility = source.envVisibility;
    target.envWindV = source.envWindV;
    target.envWindDir = source.envWindDir;
    target.simMode = source.simMode;
    target.videoFps = source.videoFps;
    target.trackerSensor[0] = ToDds(source.trackerSensor[0]);
    return target;
}

BYHWICD::InitObjectTrackingParam FromDds(
    const HwaSimIRDds::InitObjectTrackingParamV1& source)
{
    BYHWICD::InitObjectTrackingParam target = {};
    target.enable = source.enable != 0;
    target.envTerrain = source.envTerrain;
    target.envSky = source.envSky;
    target.envMaxHeightRain = source.envMaxHeightRain;
    target.envTransHeightRain = source.envTransHeightRain;
    target.envMaxHeightSnow = source.envMaxHeightSnow;
    target.envTransHeightSnow = source.envTransHeightSnow;
    target.envRainSnowSpeedScale = source.envRainSnowSpeedScale;
    target.envRadScaleTerrain = source.envRadScaleTerrain;
    target.envRadScaleSky = source.envRadScaleSky;
    target.envTemp = source.envTemp;
    target.envHumidity = source.envHumidity;
    target.envVisibility = source.envVisibility;
    target.envWindV = source.envWindV;
    target.envWindDir = source.envWindDir;
    target.simMode = source.simMode;
    target.videoFps = source.videoFps;
    target.trackerSensor[0] = FromDds(source.trackerSensor[0]);
    return target;
}

HwaSimIRDds::PlatParamPakV1 ToDds(const BYHWICD::PlatParamPak& source)
{
    HwaSimIRDds::PlatParamPakV1 target = {};
    target.id = source.id;
    target.type = source.type;
    target.spatial = ToDds(source.spatial);
    return target;
}

BYHWICD::PlatParamPak FromDds(const HwaSimIRDds::PlatParamPakV1& source)
{
    BYHWICD::PlatParamPak target = {};
    target.id = source.id;
    target.type = source.type;
    target.spatial = FromDds(source.spatial);
    return target;
}

HwaSimIRDds::WeaponStateV1 ToDds(const BYHWICD::WeaponState& source)
{
    HwaSimIRDds::WeaponStateV1 target = {};
    target.targetType = source.targetType;
    target.targetPlatID = source.targetPlatID;
    target.targetID = source.targetID;
    for (int i = 0; i < 2; ++i) {
        target.xxOutAng[i] = source.xxOutAng[i];
        target.offsetAng[i] = source.offsetAng[i];
    }
    target.lookatEn = source.lookatEn;
    target.illuminatorEn = source.illuminatorEn;
    target.viewValid = source.viewValid;
    target.damageFlag = source.damageFlag;
    target.strikeFlag = source.strikeFlag;
    target.strikePart = source.strikePart;
    return target;
}

BYHWICD::WeaponState FromDds(const HwaSimIRDds::WeaponStateV1& source)
{
    BYHWICD::WeaponState target = {};
    target.targetType = source.targetType;
    target.targetPlatID = source.targetPlatID;
    target.targetID = source.targetID;
    for (int i = 0; i < 2; ++i) {
        target.xxOutAng[i] = source.xxOutAng[i];
        target.offsetAng[i] = source.offsetAng[i];
    }
    target.lookatEn = source.lookatEn != 0;
    target.illuminatorEn = source.illuminatorEn != 0;
    target.viewValid = source.viewValid != 0;
    target.damageFlag = source.damageFlag;
    target.strikeFlag = source.strikeFlag != 0;
    target.strikePart = source.strikePart;
    return target;
}

HwaSimIRDds::TargetStateV1 ToDds(const BYHWICD::TargetState& source)
{
    HwaSimIRDds::TargetStateV1 target = {};
    target.targetType = source.targetType;
    target.targetPlatID = source.targetPlatID;
    target.targetID = source.targetID;
    target.engineState = source.engineState;
    target.viewValid = source.viewValid;
    target.targetLoc = ToDds(source.targetLoc);
    target.targetState = source.targetState;
    return target;
}

BYHWICD::TargetState FromDds(const HwaSimIRDds::TargetStateV1& source)
{
    BYHWICD::TargetState target = {};
    target.targetType = source.targetType;
    target.targetPlatID = source.targetPlatID;
    target.targetID = source.targetID;
    target.engineState = source.engineState != 0;
    target.viewValid = source.viewValid != 0;
    target.targetLoc = FromDds(source.targetLoc);
    target.targetState = source.targetState;
    return target;
}

} // namespace

namespace HwaSimIRDdsAdapter {

HwaSimIRDds::ControlCommandV1 ToDds(
    const BYHWICD::ControlP2cX1ObjTrackingCmd& source)
{
    HwaSimIRDds::ControlCommandV1 target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.simCommand = source.simCommand;
    target.roundCut = source.roundCut;
    target.currentRound = source.currentRound;
    return target;
}

BYHWICD::ControlP2cX1ObjTrackingCmd FromDds(
    const HwaSimIRDds::ControlCommandV1& source)
{
    BYHWICD::ControlP2cX1ObjTrackingCmd target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.simCommand = source.simCommand;
    target.roundCut = source.roundCut;
    target.currentRound = source.currentRound;
    return target;
}

HwaSimIRDds::InitCommandV1 ToDds(const BYHWICD::InitP2cObjectTrackingCmd& source)
{
    HwaSimIRDds::InitCommandV1 target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.platParamInit = ::ToDds(source.platParamInit);
    target.trackingInit = ::ToDds(source.trackingInit);
    target.MissileMaxCount120 = source.MissileMaxCount120;
    target.MissileMaxCount9 = source.MissileMaxCount9;
    target.MissileMaxCountMMD = source.MissileMaxCountMMD;
    target.MissileMaxCountF35 = source.MissileMaxCountF35;
    target.MissileMaxCountF22 = source.MissileMaxCountF22;
    target.MissileMaxCountResv1 = source.MissileMaxCountResv1;
    target.MissileMaxCountResv2 = source.MissileMaxCountResv2;
    return target;
}

BYHWICD::InitP2cObjectTrackingCmd FromDds(const HwaSimIRDds::InitCommandV1& source)
{
    BYHWICD::InitP2cObjectTrackingCmd target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.platParamInit = ::FromDds(source.platParamInit);
    target.trackingInit = ::FromDds(source.trackingInit);
    target.MissileMaxCount120 = source.MissileMaxCount120;
    target.MissileMaxCount9 = source.MissileMaxCount9;
    target.MissileMaxCountMMD = source.MissileMaxCountMMD;
    target.MissileMaxCountF35 = source.MissileMaxCountF35;
    target.MissileMaxCountF22 = source.MissileMaxCountF22;
    target.MissileMaxCountResv1 = source.MissileMaxCountResv1;
    target.MissileMaxCountResv2 = source.MissileMaxCountResv2;
    return target;
}

HwaSimIRDds::RealtimeDataV1 ToDds(const BYHWICD::DisplayC2cObjTrackingData& source)
{
    HwaSimIRDds::RealtimeDataV1 target = {};
    target.flag = source.flag;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.time = source.time;
    target.platLoc = ::ToDds(source.platLoc);
    target.weaponState = ::ToDds(source.weaponState);
    target.targetNumValid = source.targetNumValid;
    for (int i = 0; i < 5; ++i) {
        target.targetState[i] = ::ToDds(source.targetState[i]);
    }
    return target;
}

BYHWICD::DisplayC2cObjTrackingData FromDds(const HwaSimIRDds::RealtimeDataV1& source)
{
    BYHWICD::DisplayC2cObjTrackingData target = {};
    target.flag = source.flag;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.time = source.time;
    target.platLoc = ::FromDds(source.platLoc);
    target.weaponState = ::FromDds(source.weaponState);
    target.targetNumValid = source.targetNumValid;
    for (int i = 0; i < 5; ++i) {
        target.targetState[i] = ::FromDds(source.targetState[i]);
    }
    return target;
}

HwaSimIRDds::InitAckV1 ToDds(const BYHWICD::InitAckC2pObjectTrackingCmd& source)
{
    HwaSimIRDds::InitAckV1 target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.trackingReady = source.trackingReady;
    return target;
}

BYHWICD::InitAckC2pObjectTrackingCmd FromDds(const HwaSimIRDds::InitAckV1& source)
{
    BYHWICD::InitAckC2pObjectTrackingCmd target = {};
    target.flag = source.flag;
    target.JB = source.JB;
    target.platID = source.platID;
    target.sensorID = source.sensorID;
    target.trackingReady = source.trackingReady != 0;
    return target;
}

} // namespace HwaSimIRDdsAdapter
