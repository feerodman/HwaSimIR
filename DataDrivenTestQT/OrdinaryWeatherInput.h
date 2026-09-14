#pragma once
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QDebug>
#include <cmath>
#include <QSettings>

static void ApplyOrdinaryWeatherInitialization(BYHWICD::InitObjectTrackingParam& init,const QString& configPath) {
    QSettings settings(configPath,QSettings::IniFormat);settings.setIniCodec("UTF-8");
    QJsonObject fixture;
    const QString path=qEnvironmentVariable("WeatherCameraInput");
    if(!path.isEmpty()) {
        QFile file(path);if(!file.open(QIODevice::ReadOnly))qFatal("Weather initialization fixture unavailable");
        fixture=QJsonDocument::fromJson(file.readAll()).object().value("InitializationWeather").toObject();
    }
    struct Field {const char* name;double BYHWICD::InitObjectTrackingParam::*member;double initial,minimum,maximum;};
    const Field fields[]={
        {"envMaxHeightRain",&BYHWICD::InitObjectTrackingParam::envMaxHeightRain,4500,0,20000},
        {"envTransHeightRain",&BYHWICD::InitObjectTrackingParam::envTransHeightRain,800,0,20000},
        {"envMaxHeightSnow",&BYHWICD::InitObjectTrackingParam::envMaxHeightSnow,4500,0,20000},
        {"envTransHeightSnow",&BYHWICD::InitObjectTrackingParam::envTransHeightSnow,800,0,20000},
        {"envRainSnowSpeedScale",&BYHWICD::InitObjectTrackingParam::envRainSnowSpeedScale,1,0.1,5}};
    for(const auto& f:fields) {
        const QString name=QString::fromLatin1(f.name),key="WeatherInit/"+name;
        bool valid=true;double value=settings.value(key,f.initial).toDouble(&valid);
        const QString source=fixture.contains(name)?"explicit_camera_file":settings.contains(key)?"NetworkConfig.ini":"documented_default";
        if(fixture.contains(name)) {valid=fixture[name].isDouble();value=fixture[name].toDouble();}
        if(!valid || !std::isfinite(value) || value<f.minimum || value>f.maximum)qFatal("Invalid initialization weather field: %s",f.name);
        init.*(f.member)=value;
        qInfo().noquote()<<"[WeatherInitField]"<<name<<"value="<<value<<"source="<<source;
    }
}

// Explicit stimulus fixture only. Sends ordinary camera positions through the
// existing interface; the renderer has no fixture/camera override for this path.
static void ApplyOrdinaryWeatherInput(BYHWICD::DisplayC2cObjTrackingData& data,double elapsedSec){
    struct Fixture {
        bool enabled=false;QJsonArray frames,assetPose,telemetryTargets,illumination;qint64 epoch=0;double start=0;
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
            telemetryTargets=root.value("SyntheticTelemetryTargets").toArray();
            illumination=root.value("OrdinaryIlluminationEnableSteps").toArray();
            double previousIllumination=-1;
            for(auto value:illumination){const auto row=value.toArray();
                if(row.size()!=2 || !row[0].isDouble() || !row[1].isBool() || !std::isfinite(row[0].toDouble()) || row[0].toDouble()<0 || row[0].toDouble()<=previousIllumination)qFatal("Invalid ordinary illumination steps");
                previousIllumination=row[0].toDouble();
            }
            if(telemetryTargets.size()>5)qFatal("Telemetry fixture exceeds protocol capacity");
            for(auto value:telemetryTargets){const auto target=value.toArray();
                if(target.size()!=11)qFatal("Telemetry target needs type, platform, ID, seven spatial fields and status");
                for(auto x:target)if(!x.isDouble()||!std::isfinite(x.toDouble()))qFatal("Invalid telemetry target value");
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
    if(!fixture.illumination.isEmpty()) {
        bool enabled=false;
        for(auto value:fixture.illumination){const auto row=value.toArray();if(row[0].toDouble()>time)break;enabled=row[1].toBool();}
        data.weaponState.illuminatorEn=enabled;
    }
    data.targetNumValid=fixture.assetPose.isEmpty()?0:1;
    if(!fixture.assetPose.isEmpty()){
        // Test stimulus changes pose only. Keep the existing type/key/state and
        // engine flag; no renderer fixture or temperature/material override.
        auto& pose=data.targetState[0].targetLoc;
        pose.lat=fixture.assetPose[0].toDouble();pose.lon=fixture.assetPose[1].toDouble();pose.alt=fixture.assetPose[2].toDouble();
        pose.yaw=fixture.assetPose[3].toDouble();pose.pitch=fixture.assetPose[4].toDouble();pose.roll=fixture.assetPose[5].toDouble();
    }
    if(!fixture.telemetryTargets.isEmpty()){
        // Explicit synthetic UI/recording fixture. These values travel through
        // the existing DDS input; they are never injected into receiver widgets.
        data.targetNumValid=fixture.telemetryTargets.size();
        for(int n=0;n<data.targetNumValid;++n){const auto a=fixture.telemetryTargets[n].toArray();auto& t=data.targetState[n];
            t={};t.targetType=a[0].toInt();t.targetPlatID=a[1].toInt();t.targetID=a[2].toInt();
            t.targetLoc.lat=a[3].toDouble();t.targetLoc.lon=a[4].toDouble();t.targetLoc.alt=a[5].toDouble();
            t.targetLoc.yaw=a[6].toDouble();t.targetLoc.pitch=a[7].toDouble();t.targetLoc.roll=a[8].toDouble();t.targetLoc.speed=a[9].toDouble();
            t.targetState=a[10].toInt();t.viewValid=true;
        }
    }
}
