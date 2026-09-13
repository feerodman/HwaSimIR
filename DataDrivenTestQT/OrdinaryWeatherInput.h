#pragma once
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDebug>
#include <cmath>

// Explicit stimulus fixture only. Sends ordinary camera positions through the
// existing interface; the renderer has no fixture/camera override for this path.
static void ApplyOrdinaryWeatherInput(BYHWICD::DisplayC2cObjTrackingData& data,double elapsedSec){
    struct Fixture {
        bool enabled=false;QJsonArray frames,assetPose;qint64 epoch=0;double start=0;
        Fixture(){
            const QString path=qEnvironmentVariable("WeatherCameraInput");if(path.isEmpty())return;
            QFile f(path);if(!f.open(QIODevice::ReadOnly))qFatal("Weather camera fixture unavailable");
            const auto bytes=f.readAll();QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
            if(error.error!=QJsonParseError::NoError||!doc.isObject())qFatal("Invalid weather camera JSON");
            const auto root=doc.object();if(root.value("Schema").toString()!="ordinary_weather_camera_1")qFatal("Invalid weather camera schema");
            frames=root.value("Keyframes").toArray();if(frames.isEmpty())qFatal("Empty weather camera keyframes");
            double previous=-1;
            for(auto value:frames){const auto frame=value.toArray();if(frame.size()!=7)qFatal("Weather keyframe needs 7 values");
                for(auto x:frame)if(!x.isDouble()||!std::isfinite(x.toDouble()))qFatal("Nonfinite weather keyframe");
                if(frame[0].toDouble()<=previous||std::abs(frame[1].toDouble())>89||std::abs(frame[2].toDouble())>180)qFatal("Invalid weather keyframe order/position");
                previous=frame[0].toDouble();
            }
            if(frames[0].toArray()[0].toDouble()!=0)qFatal("Weather keyframes must start at zero");
            if(root.contains("GameAssetPose")){
                assetPose=root.value("GameAssetPose").toArray();if(assetPose.size()!=6)qFatal("Game asset pose needs 6 values");
                for(auto v:assetPose)if(!v.isDouble()||!std::isfinite(v.toDouble()))qFatal("Nonfinite game asset pose");
                if(std::abs(assetPose[0].toDouble())>89||std::abs(assetPose[1].toDouble())>180)qFatal("Invalid game asset position");
            }
            epoch=qint64(root.value("SimulationEpochMs").toDouble());
            bool ok=false;start=qEnvironmentVariable("WeatherStartSec").toDouble(&ok);if(!ok)start=0;
            if(!std::isfinite(start)||start<0)qFatal("Invalid weather start time");
            enabled=true;qInfo().noquote()<<"[WeatherCameraInput] file="+path<<"sha256="+QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex()
                <<"ordinary_protocol_only=1 renderer_test_scene=0 startSec="<<start;
        }
    };
    static Fixture fixture;if(!fixture.enabled)return;
    const double time=std::max(0.0,elapsedSec)+fixture.start;
    int i=0;while(i+1<fixture.frames.size()&&fixture.frames[i+1].toArray()[0].toDouble()<=time)++i;
    const auto a=fixture.frames[i].toArray(),b=fixture.frames[std::min(i+1,fixture.frames.size()-1)].toArray();
    const double dt=b[0].toDouble()-a[0].toDouble();const double t=dt>0?std::min(1.0,(time-a[0].toDouble())/dt):0;
    auto at=[&](int n){return a[n].toDouble()+(b[n].toDouble()-a[n].toDouble())*t;};
    data.time=fixture.epoch+qint64(time*1000);
    data.platLoc.lat=at(1);data.platLoc.lon=at(2);data.platLoc.alt=at(3);
    data.platLoc.yaw=at(4);data.platLoc.pitch=0;data.platLoc.roll=0;
    data.weaponState.lookatEn=false;data.weaponState.xxOutAng[0]=at(5);data.weaponState.xxOutAng[1]=at(6);
    data.weaponState.offsetAng[0]=data.weaponState.offsetAng[1]=0;
    data.targetNumValid=fixture.assetPose.isEmpty()?0:1;
    if(!fixture.assetPose.isEmpty()){
        // Test stimulus changes pose only. Keep the existing type/key/state and
        // engine flag; no renderer fixture or temperature/material override.
        auto& pose=data.targetState[0].targetLoc;
        pose.lat=fixture.assetPose[0].toDouble();pose.lon=fixture.assetPose[1].toDouble();pose.alt=fixture.assetPose[2].toDouble();
        pose.yaw=fixture.assetPose[3].toDouble();pose.pitch=fixture.assetPose[4].toDouble();pose.roll=fixture.assetPose[5].toDouble();
    }
}
