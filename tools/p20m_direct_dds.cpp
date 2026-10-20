// Independent DDS sender for runtime rejection/recovery tests. It intentionally
// has no LUT dependency. Production stim preflight remains enabled.
#include "DdsStimClient.h"
#include "SensorFieldSchema.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <chrono>
#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>

struct Route
{
	BYHWICD::InitP2cObjectTrackingCmd init = {};
	std::vector<QStringList> rows;
	int model = 0x22;
};

int main(int argc, char** argv)
{
	QCoreApplication app(argc,argv);
	QString iniPath, jsonPath, pacingPath;
	bool p20m2 = false;
	for(const QString& arg:app.arguments())
	{
		if(arg.startsWith("--network-config=")) iniPath=arg.mid(17);
		if(arg.startsWith("--channels-json=")) jsonPath=arg.mid(16);
		if(arg.startsWith("--pacing-jsonl=")) pacingPath=arg.mid(15);
		if(arg=="--p20m2-60hz") p20m2=true;
	}
	QFile input(jsonPath); if(!input.open(QIODevice::ReadOnly)) return 2;
	const QJsonObject config=QJsonDocument::fromJson(input.readAll()).object();
	QSettings ini(iniPath,QSettings::IniFormat);ini.setIniCodec("UTF-8");
	const QDate date=QDate::fromString(ini.value("Demo/UtcDate").toString(),"yyyy-MM-dd");
	bool hourOk=false;double hour=ini.value("Demo/UtcHour").toDouble(&hourOk);
	if(!date.isValid()||!hourOk||!std::isfinite(hour)||hour<0||hour>=24) return 3;
	const qint64 epoch=QDateTime(date,QTime(0,0),Qt::UTC).toMSecsSinceEpoch()+qint64(hour*3600000);
	std::vector<Route> routes;
	for(const auto& item:config["channels"].toArray())
	{
		const QJsonObject channel=item.toObject();if(!channel["enabled"].toBool())continue;
		Route r;r.init.flag=0x36;r.init.JB=1;r.init.platID=channel["platID"].toInt();r.init.sensorID=channel["sensorID"].toInt();
		r.model=channel["followModel"].toBool()?config["model"].toInt():channel["model"].toInt();
		r.init.MissileMaxCount120=2;r.init.MissileMaxCount9=2;r.init.MissileMaxCountF35=2;
		r.init.MissileMaxCountF22=2;r.init.MissileMaxCountMMD=2;
		auto& env=r.init.trackingInit;env.enable=true;env.envTerrain=0;env.envSky=ini.value("WeatherInit/envSky",1).toInt();
		env.envHumidity=ini.value("WeatherInit/envHumidity",85).toDouble();env.envVisibility=ini.value("WeatherInit/envVisibility",6000).toDouble();
		env.envTemp=ini.value("WeatherInit/envTemp",15).toDouble();env.envRadScaleSky=1;env.envRadScaleTerrain=1;
		env.envMaxHeightRain=6000;env.envTransHeightRain=500;env.envMaxHeightSnow=6000;env.envTransHeightSnow=500;
		env.envRainSnowSpeedScale=1;env.envWindV=3;env.envWindDir=0;
		const auto initial=channel["init"].toObject();
		env.simMode=initial["simMode"].toInt(1);env.videoFps=initial["videoFps"].toInt(40);
		if(initial.contains("envSky"))env.envSky=initial["envSky"].toInt();
		auto& sensor=env.trackerSensor[0];sensor=HwaSensorFields::defaults();
		for(int f=0;f<HwaSensorFields::count;++f)
		{
			const auto& field=HwaSensorFields::fields()[f];
			if(initial.contains(field.name)&&!HwaSensorFields::set(sensor,field,initial[field.name].toDouble()))return 4;
		}
		const QString source=channel["followSource"].toBool()?config["source"].toString():channel["source"].toString();
		QFile csv(source);if(!csv.open(QIODevice::ReadOnly))return 5;
		for(const QByteArray& line:csv.readAll().split('\n'))
		{
			const QString text=QString::fromUtf8(line).trimmed();
			if(text.isEmpty()||text.startsWith("Time"))continue;
			const auto cells=text.split(',');if(cells.size()<54)return 6;
			for(const auto& cell:cells){bool ok=false;const double value=cell.toDouble(&ok);if(!ok||!std::isfinite(value))return 7;}
			r.rows.push_back(cells);
		}
		if(r.rows.empty())return 8;
		const auto& first=r.rows.front();
		r.init.platParamInit.id=r.init.platID;r.init.platParamInit.type=1;
		r.init.platParamInit.spatial.lat=first[2].toDouble();r.init.platParamInit.spatial.lon=first[3].toDouble();r.init.platParamInit.spatial.alt=first[4].toDouble();
		routes.push_back(r);
	}
	if(routes.empty())return 9;
	const double stepMs=config["stepMs"].toDouble();
	if(!std::isfinite(stepMs)||stepMs<=0)return 10;
	if(p20m2)
	{
		if(std::abs(stepMs-1000.0/60.0)>1e-9)return 19;
		for(const auto& r:routes)if(r.init.trackingInit.videoFps!=60||r.init.trackingInit.simMode!=1)return 19;
		if(pacingPath.isEmpty())return 19;
	}
	QFile pacingFile(pacingPath);
	if(!pacingPath.isEmpty() && (pacingFile.exists() || !pacingFile.open(QIODevice::WriteOnly))) return 20;
	QTextStream pacing(&pacingFile); pacing.setRealNumberPrecision(17);
	DdsStimClient client;DdsStimConfig dds;dds.domainId=config["domain"].toInt();dds.qosFile=config["qos"].toString().toStdString();dds.observeStopStatus=true;
	std::string error;if(!client.start(dds,error)){std::cerr<<error;return 11;}
	std::vector<DdsStimRoute> expected;
	for(const auto& r:routes){DdsStimRoute route;route.platID=r.init.platID;route.sensorID=r.init.sensorID;route.role=r.init.sensorID==1?"coarse":"precise";expected.push_back(route);}
	client.setExpectedRoutes(expected);
	std::this_thread::sleep_for(std::chrono::seconds(3));
	auto control=[&](int command)
	{
		std::vector<int> sent;
		for(const auto& r:routes)
		{
			if(std::find(sent.begin(),sent.end(),r.init.platID)!=sent.end())continue;
			sent.push_back(r.init.platID);
			BYHWICD::ControlP2cX1ObjTrackingCmd c={};c.flag=0x41;c.JB=1;c.platID=r.init.platID;c.simCommand=command;c.roundCut=1;c.currentRound=1;
			if(!client.sendControl(c,error))return false;
		}
		return true;
	};
	if(!control(1))return 12;
	for(const auto& r:routes)
	{
		if(!client.sendInit(r.init,error))return 13;
		BYHWICD::InitAckC2pObjectTrackingCmd ack={};
		if(!client.waitForInitAck(30000,ack)||ack.platID!=r.init.platID||ack.sensorID!=r.init.sensorID)return 14;
	}
	if(!control(2)||!client.waitForRunningStatus(30000,error))return 15;
	std::cout<<"[P20M DirectDDS] inputContract=UnixEpochMs replayStart="<<epoch
		<<" coveragePreflight=not_part_of_external_sender no_physics_success_claim=1"<<std::endl;
	size_t frames=routes.front().rows.size();
	for(const auto& r:routes)if(r.rows.size()!=frames)return 16;
	const auto start=std::chrono::steady_clock::now();
	for(size_t i=0;i<frames;++i)
	{
		const auto deadline=start+std::chrono::nanoseconds(static_cast<long long>(i*stepMs*1000000.0));
		std::this_thread::sleep_until(deadline);
		for(const auto& r:routes)
		{
			const auto& row=r.rows[i];BYHWICD::DisplayC2cObjTrackingData d={};
			d.flag=0x38;d.platID=r.init.platID;d.sensorID=r.init.sensorID;
			d.time=epoch+row[0].toDouble()-r.rows.front()[0].toDouble();
			auto& p=d.platLoc;p.lat=row[2].toDouble();p.lon=row[3].toDouble();p.alt=row[4].toDouble();p.yaw=row[5].toDouble();p.pitch=row[6].toDouble();p.roll=row[7].toDouble();p.speed=row[8].toDouble();
			d.targetNumValid=1;auto& t=d.targetState[0];t.targetType=r.model;t.targetPlatID=3;t.targetID=3;t.engineState=true;t.viewValid=row[53].toInt()!=0;t.targetState=1;
			auto& q=t.targetLoc;q.lat=row[10].toDouble();q.lon=row[11].toDouble();q.alt=row[12].toDouble();q.yaw=row[13].toDouble();q.pitch=row[14].toDouble();q.roll=row[15].toDouble();q.speed=row[16].toDouble();
			auto& w=d.weaponState;w.targetType=r.model;w.targetPlatID=3;w.targetID=3;w.lookatEn=true;w.viewValid=t.viewValid;w.strikeFlag=row[28].toInt()!=0;w.strikePart=2;
			if(config["diagnosticReferenceMismatch"].toBool()&&i>=30&&i<60)w.targetType=r.model==0x22?0x33:0x22;
			if(!client.sendRealtime(d,error)){control(3);return 17;}
			if(pacingFile.isOpen())
			{
				const double wallMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
				pacing<<"{\"schema\":\"HwaSimIR.SendPacing.1\",\"row\":"<<qulonglong(i)
					<<",\"platID\":"<<int(r.init.platID)<<",\"sensorID\":"<<int(r.init.sensorID)
					<<",\"protocolUtcMs\":"<<d.time<<",\"sourceRelativeMs\":"<<row[0].toDouble()-r.rows.front()[0].toDouble()
					<<",\"deadlineMs\":"<<i*stepMs<<",\"wallSendCompletedMs\":"<<wallMs<<"}\n";
			}
		}
	}
	if(pacingFile.isOpen()) { pacing.flush(); if(pacingFile.error()!=QFile::NoError)return 21; }
	client.waitForAcknowledgments(1000,error);
	if(!control(3)||!client.waitForStopStatus(120000,error)){std::cerr<<error;return 18;}
	std::cout<<"[P20M DirectDDS] packetsPerRoute="<<frames<<" stopObserved=1"<<std::endl;
	client.shutdown();return 0;
}
