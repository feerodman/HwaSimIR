#pragma once
#include "../math_algorithm.h"

// Weather occupies one fixed ENU tangent plane. Rendering may use another ENU
// origin; both transforms use the same existing WGS84/ECEF project functions.
// Z is metres above the configured tangent plane, not terrain-relative height.
struct IRCloudWorldFrame {
    ICD::Position world{}, local{};
    ICD::CartesianCoordinate worldEcef{}, localEcef{};
    void setWorld(double lat,double lon,double alt) {
        world.lat=lat;world.lon=lon;world.alt=alt;worldEcef=LLA2ECEF(world);
    }
    void setLocal(double lat,double lon,double alt) {
        local.lat=lat;local.lon=lon;local.alt=alt;localEcef=LLA2ECEF(local);
    }
    ICD::CartesianCoordinate toLocal(const ICD::CartesianCoordinate& p) const {
        return ECEF2ENU(localEcef,local,ENU2ECEF(worldEcef,world,p));
    }
    ICD::CartesianCoordinate toWorld(const ICD::CartesianCoordinate& p) const {
        return ECEF2ENU(worldEcef,world,ENU2ECEF(localEcef,local,p));
    }
    ICD::CartesianCoordinate worldVectorToLocal(const ICD::CartesianCoordinate& p) const {
        ICD::CartesianCoordinate zero={0,0,0};
        return ECEF2ENU(zero,local,ENU2ECEF(zero,world,p));
    }
};
