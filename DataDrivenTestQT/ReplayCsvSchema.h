#pragma once
#include <QStringList>

// Exact column labels and declared units of the existing input format.
inline QStringList replayCsvSchema()
{
    return QStringLiteral("Time(ms),distanceDAM_RTState(m),RedLat(degree),RedLon(degree),RedAlt(m),RedYaw(degree),RedPitch(degree),RedRoll(degree),RedSpeedAir(km/h),RedFlyOverLoad,MissileLat(degree),MissileLon(degree),MissileAlt(m),MissileYaw(degree),MissilePitch(degree),MissileRoll(degree),MissileSpeedAir(km/h),MissileType,RedFlyOverLoadX,RedFlyOverLoadY,RedFlyOverLoadZ,distanceFF(m),AA,AA,AA,AA,RedPlatAttackAngle(degree),RedFlySideslipAng(degree),strickenFlag,damageFlag,AA,AA,AA,AA,AA,AA,AA,AA,AA,AA,AA,synAngRat(degree/s),thetaAngRat(degree/s),phiAngRat(degree/s),synAng_Acc(degree/s^2),thetaAng_Acc(degree/s^2),phiAng_Acc(degree/s^2),PlatOverLoadX,PlatOverLoadY,PlatOverLoadZ,coeT,AA,AA,ViewValid,TrackingState,HitState,OPDrms,AA,AA,TargetLatFake(degree),TargetLonFake(degree),TargetAltFake(m),ladarType").split(QLatin1Char(','));
}
