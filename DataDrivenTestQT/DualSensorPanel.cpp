#include "DualSensorPanel.h"
#include "../DDS/Protocol/SensorFieldSchema.h"
#include <QtWidgets>
#include <cmath>
#include <set>
#include <chrono>
#include <limits>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
struct InitField { const char* name; const char* label; double initial, low, high; int decimals; };
const InitField extraFields[] = {
    {"enable","挂载 enable (0/1)",1,0,1,0},
    {"envTerrain","地形 (0戈壁/1山区/2海面)",0,0,2,0},
    {"envSky","天气 (0晴/1云/2雨/3雪/4雾/5阴)",1,0,5,0},
    {"envMaxHeightRain","降雨上限 (m)",4500,0,20000,2},
    {"envTransHeightRain","降雨过渡 (m)",800,0,20000,2},
    {"envMaxHeightSnow","降雪上限 (m)",4500,0,20000,2},
    {"envTransHeightSnow","降雪过渡 (m)",800,0,20000,2},
    {"envRainSnowSpeedScale","雨雪速度系数",1,.1,5,3},
    {"envRadScaleTerrain","地形辐射系数",1,0,100,3},
    {"envRadScaleSky","天空辐射系数",1,0,100,3},
    {"envTemp","气温 (°C)",25,-100,100,3},
    {"envHumidity","湿度 (%)",85,0,100,3},
    {"envVisibility","能见度 (m)",6000,100,100000,2},
    {"envWindV","风速 (m/s)",8,0,1000,3},
    {"envWindDir","风向 (°)",30,-360,360,3},
    {"simMode","模式 (1同步/2异步)",1,1,2,0},
    {"videoFps","成像 FPS (0不限)",60,0,240,0},
    {"JB","军别 (1红/2蓝)",1,1,2,0},
    {"platformType","平台 type",1,1,2,0},
    {"lat","平台纬度 (°)",0,-90,90,8},
    {"lon","平台经度 (°)",0,-180,180,8},
    {"alt","平台高度 (m)",0,-1000,1000000,3},
    {"yaw","平台航向 (°)",0,-360,360,4},
    {"pitch","平台俯仰 (°)",0,-360,360,4},
    {"roll","平台滚转 (°)",0,-360,360,4},
    {"speed","平台速度 (km/h)",0,0,100000,3}
};
QComboBox* modelBox() {
    auto* box=new QComboBox;
    const char* names[]={"0x11 · F35","0x12 · F22","0x22 · 雷达导弹","0x33 · 红外导弹","0x44 · MMD","0x55 · 预留模型1","0x66 · 预留模型2"};
    const int ids[]={0x11,0x12,0x22,0x33,0x44,0x55,0x66};
    for(int i=0;i<7;++i)box->addItem(QString::fromUtf8(names[i]),ids[i]);
    box->setCurrentIndex(2); return box;
}
QString sourceKey(const QString& path) {
    const QFileInfo file(path);
    return file.exists()?file.canonicalFilePath():file.absoluteFilePath();
}
void putFile(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())
        throw std::runtime_error((path+": "+file.errorString()).toStdString());
}
}

DualSensorPanel::DualSensorPanel(DdsStimClient* client,const QString& config,const QString& input,Coverage validator,QWidget* parent)
    : QWidget(parent),dds(client),network(config),coverage(validator)
{
    setObjectName("dualSensorPanel");
    QSettings ini(config,QSettings::IniFormat); ini.setIniCodec("UTF-8");
    const QString dateText=ini.value("Demo/UtcDate").toString();
    const QDate date=QDate::fromString(dateText,"yyyy-MM-dd");
    bool hourOk=false;const double hour=ini.value("Demo/UtcHour").toDouble(&hourOk);
    if(date.isValid()&&date.toString("yyyy-MM-dd")==dateText&&hourOk&&std::isfinite(hour)&&hour>=0.0&&hour<24.0)
        epoch=QDateTime(date,QTime(0,0),Qt::UTC).addMSecs(qint64(hour*3600000));
    auto* root=new QVBoxLayout(this);
    auto* title=new QLabel("双传感器 DDS 测试 · P19"); title->setStyleSheet("font-size:20px;font-weight:bold"); root->addWidget(title);
    auto* warning=new QLabel("发送勾选只控制 INIT / Realtime；START / STOP / RESET 按 platID 联动同平台实例。独立控制请使用不同 platID。\nID、role、像素格式、Domain 是部署配置：导出后按说明启动 renderer；INIT 不会修改其本地身份。无事务号 ACK 不能区分延迟的旧应答。");
    warning->setWordWrap(true); root->addWidget(warning);
    settings=new QWidget; auto* shared=new QGridLayout(settings); shared->setContentsMargins(0,0,0,0);
    commonSource=new QLineEdit(input); commonSource->setObjectName("commonSource");
    auto* browse=new QPushButton("选择公共数据文件");
    connect(browse,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"选择符合原格式的数据文件",commonSource->text());if(!path.isEmpty())commonSource->setText(path);});
    commonModel=modelBox(); commonModel->setObjectName("commonModel");
    stepMs=new QDoubleSpinBox;stepMs->setRange(.1,100000);stepMs->setDecimals(6);stepMs->setValue(1000./60.);
    domain=new QSpinBox;domain->setRange(0,232);domain->setValue(ini.value("DdsProtocol/DomainId",150).toInt());domain->setObjectName("exportDomain");
    videoDomain=new QSpinBox;videoDomain->setRange(0,232);videoDomain->setValue(domain->value());videoDomain->setObjectName("exportVideoDomain");
    customerDomain=new QSpinBox;customerDomain->setRange(-1,232);customerDomain->setValue(-1);customerDomain->setSpecialValueText("跟随原视频域");customerDomain->setObjectName("exportCustomerDomain");
    qos=new QLineEdit(ini.value("DdsProtocol/QosFile","Config/DDS/ZRDDS_PROTOCOL_QOS.xml").toString());
    observeStatus=new QCheckBox("等待原 VideoStatus（无需 VideoDisplay；关闭原视频时取消）");observeStatus->setChecked(true);
    shared->addWidget(commonSource,0,0,1,3);shared->addWidget(browse,0,3);
    shared->addWidget(new QLabel("公共模型"),1,0);shared->addWidget(commonModel,1,1);shared->addWidget(new QLabel("每 tick 间隔 (ms)"),1,2);shared->addWidget(stepMs,1,3);
    shared->addWidget(new QLabel("控制 / 激励域（导出后重启）"),2,0);shared->addWidget(domain,2,1);shared->addWidget(new QLabel("共享 QoS 文件"),2,2);shared->addWidget(qos,2,3);
    shared->addWidget(new QLabel("原视频 / VideoDisplay 域"),3,0);shared->addWidget(videoDomain,3,1);shared->addWidget(new QLabel("客户 TX 视频域"),3,2);shared->addWidget(customerDomain,3,3);
    shared->addWidget(observeStatus,4,0,1,4);root->addWidget(settings);
    auto* effective=new QLabel(QString("当前进程：Domain %1 · QoS %2\n配置：%3 · DDS 全局初始化次数 %4")
        .arg(domain->value()).arg(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(qos->text()),network).arg(dds->runtimeInitCount()));
    effective->setWordWrap(true);effective->setTextInteractionFlags(Qt::TextSelectableByMouse);root->addWidget(effective);
    tabs=new QTabWidget;
    channels.resize(2);
    for(int index=0;index<channels.size();++index) {
        auto& c=channels[index];
        auto* head=new QHBoxLayout;
        c.selected=new QCheckBox(QString("发送路 %1").arg(index+1));c.selected->setChecked(true);c.selected->setObjectName(QString("route%1Enabled").arg(index+1));
        c.role=new QComboBox;c.role->addItems({"coarse","precise"});c.role->setCurrentIndex(index);c.role->setObjectName(QString("route%1Role").arg(index+1));
        c.role->setMinimumContentsLength(7);c.role->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        c.plat=new QSpinBox;c.plat->setRange(1,2147483647);c.plat->setValue(1001);
        c.sensor=new QSpinBox;c.sensor->setRange(1,254);c.sensor->setValue(index+1);
        c.status=new QLabel("待 RESET / INIT");c.status->setMinimumWidth(180);c.status->setWordWrap(true);
        head->addWidget(c.selected);head->addWidget(c.role);head->addWidget(new QLabel("platID"));head->addWidget(c.plat);
        head->addWidget(new QLabel("sensorID"));head->addWidget(c.sensor);head->addWidget(c.status,1);root->addLayout(head);
        c.form=new QWidget;auto* form=new QVBoxLayout(c.form);
        auto* controls=new QGridLayout;
        c.followSource=new QCheckBox("跟随公共数据源");c.followSource->setChecked(true);
        c.source=new QLineEdit(input);auto* ownBrowse=new QPushButton("选择本路数据");
        connect(ownBrowse,&QPushButton::clicked,this,[this,index]{auto& c=channels[index];const auto path=QFileDialog::getOpenFileName(this,"选择本路数据",c.source->text());if(!path.isEmpty()){c.source->setText(path);c.followSource->setChecked(false);}});
        c.followModel=new QCheckBox("跟随公共模型");c.followModel->setChecked(true);c.model=modelBox();
        c.pixel=new QComboBox;c.pixel->addItems({"gray8","rgb24"});
        c.platformFromSource=new QCheckBox("平台 INIT 使用所选数据首行（取消后编辑平台字段）");c.platformFromSource->setChecked(true);
        c.forceVisible=new QCheckBox("演示强制显示目标（默认遵循源 viewValid）");
        controls->addWidget(c.followSource,0,0);controls->addWidget(c.source,0,1);controls->addWidget(ownBrowse,0,2);
        controls->addWidget(c.followModel,1,0);controls->addWidget(c.model,1,1);controls->addWidget(c.pixel,1,2);
        controls->addWidget(c.platformFromSource,2,0,1,3);controls->addWidget(c.forceVisible,3,0,1,3);
        form->addLayout(controls);
        auto* grid=new QGridLayout;int count=0;
        auto add=[&](QString key,QString label,double value,double low,double high,int decimals){
            auto* number=new QDoubleSpinBox;number->setRange(low,high);number->setDecimals(decimals);number->setValue(value);
            number->setObjectName(QString("route%1_%2").arg(index+1).arg(key));number->setToolTip(key);
            number->setMinimumWidth(100);c.values[key]=number;
            grid->addWidget(new QLabel(label),count/2,(count%2)*2);grid->addWidget(number,count/2,(count%2)*2+1);++count;
        };
        for(int i=0;i<HwaSensorFields::count;++i){const auto& f=HwaSensorFields::fields()[i];
            double initial=ini.value("SensorInit/"+QString(f.name),f.initial).toDouble();
            if(QString(f.name)=="trackerSensorWidth"||QString(f.name)=="trackerSensorHeight")initial=index?800:320;
            if(QString(f.name)=="h264En")initial=index?1:0;
            if(QString(f.name)=="saveMP4En")initial=0;
            add(f.name,QString::fromUtf8(f.label)+" "+QString::fromUtf8(f.unit),initial,f.minimum,f.maximum,f.decimals);
        }
        for(const auto& f:extraFields){double value=ini.value("WeatherInit/"+QString(f.name),f.initial).toDouble();
            if(QString(f.name)=="envSky")value=ini.value("Demo/envSky",f.initial).toDouble();
            if(QString(f.name)=="simMode"||QString(f.name)=="videoFps")value=ini.value("RenderControl/"+QString(f.name),f.initial).toDouble();
            add(f.name,QString::fromUtf8(f.label),value,f.low,f.high,f.decimals);
        }
        form->addLayout(grid);
        auto* copy=new QPushButton(QString("复制本路参数到路 %1（保留对方身份、role 和编码模式）").arg(2-index));
        connect(copy,&QPushButton::clicked,this,[this,index]{copyChannel(index,1-index);});form->addWidget(copy);
        auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setWidget(c.form);tabs->addTab(scroll,QString("路 %1 · 完整初始化").arg(index+1));
    }
    root->addWidget(tabs,1);
    auto* actions=new QHBoxLayout;
    auto button=[&](const QString& text,std::function<void()> fn){auto* b=new QPushButton(text);connect(b,&QPushButton::clicked,this,fn);actions->addWidget(b);return b;};
    button("RESET / 解锁",[this]{reset();});button("分别 INIT",[this]{initialize();});button("START",[this]{start();});button("STOP / 排空",[this]{stop();});
    button("读取配置",[this]{if(phase!=Editable){fail("请先 STOP / RESET，再读取配置");return;}auto p=QFileDialog::getOpenFileName(this,"读取双路 JSON 或迁移单路 INI",{},"配置 (*.json *.ini)");if(!p.isEmpty())loadConfig(p);});
    button("保存配置",[this]{auto p=QFileDialog::getSaveFileName(this,"保存双路配置","dual_sensor.json");if(!p.isEmpty())saveConfig(p);});
    button("导出部署",[this]{auto p=QFileDialog::getExistingDirectory(this,"导出两路配置与启动说明");if(!p.isEmpty())exportDeployment(p);});
    root->addLayout(actions);message=new QLabel("就绪。先确认 renderer 部署身份与当前 Domain，再 RESET → INIT → 两份 ACK → START。");message->setWordWrap(true);root->addWidget(message);
    tick.setSingleShot(true);tick.setTimerType(Qt::PreciseTimer);connect(&tick,&QTimer::timeout,this,&DualSensorPanel::pump);
    poll.setInterval(50);connect(&poll,&QTimer::timeout,this,&DualSensorPanel::pollState);poll.start();
}

bool DualSensorPanel::busy() const {return phase==Running||phase==Starting||phase==Stopping||phase==AwaitingAck;}
void DualSensorPanel::fail(const QString& error) {
    message->setText("错误："+error);message->setStyleSheet("color:#b42318");qCritical().noquote()<<"[P19UI][ERROR]"<<error;emit operationFailed(error);
}
void DualSensorPanel::freeze(bool value){settings->setEnabled(!value);for(auto& c:channels){c.form->setEnabled(!value);c.selected->setEnabled(!value);c.role->setEnabled(!value);c.plat->setEnabled(!value);c.sensor->setEnabled(!value);}}
void DualSensorPanel::status(int index,const QString& state){auto& c=channels[index];c.status->setText(QString("%1 · %2 · 已发 %3\n%4").arg(state,c.topic).arg(c.sent).arg(c.replay?QString("%1 · 行 %2/%3").arg(QFileInfo(c.replay->path).fileName()).arg(c.sent).arg(c.replay->rows.size()):QString()));}
void DualSensorPanel::setSensorFieldAll(const QString& key,double value){for(auto& c:channels)if(c.values.contains(key))c.values[key]->setValue(value);}
void DualSensorPanel::setEnvironmentAll(const QString& key,double value){setSensorFieldAll(key,value);}
void DualSensorPanel::setSource(const QString& path){commonSource->setText(path);}
void DualSensorPanel::setStepMs(double value){stepMs->setValue(value);}
void DualSensorPanel::setStatusBarrier(bool value){observeStatus->setChecked(value);dds->setVideoStatusObservation(value);}

bool DualSensorPanel::collect(QString& error,bool loadSources) {
    std::set<std::pair<int,int>> identities;std::set<QString> topics;
    QMap<QString,std::shared_ptr<Source>> sources;int selected=0;
    for(int i=0;i<channels.size();++i){auto& c=channels[i];c.enabled=c.selected->isChecked();if(!c.enabled){status(i,"未选择");continue;}++selected;
        if(!identities.insert({c.plat->value(),c.sensor->value()}).second){error="选中路的 platID/sensorID 重复";return false;}
        c.init={};auto& cmd=c.init;cmd.flag=0x36;cmd.platID=c.plat->value();cmd.sensorID=c.sensor->value();
        auto v=[&](const char* name){return c.values[name]->value();};
        cmd.JB=int(v("JB"));cmd.platParamInit.id=cmd.platID;cmd.platParamInit.type=int(v("platformType"));
        auto& init=cmd.trackingInit;
#define SET_INIT(name) init.name=decltype(init.name)(v(#name))
        SET_INIT(enable);SET_INIT(envTerrain);SET_INIT(envSky);SET_INIT(envMaxHeightRain);SET_INIT(envTransHeightRain);SET_INIT(envMaxHeightSnow);SET_INIT(envTransHeightSnow);
        SET_INIT(envRainSnowSpeedScale);SET_INIT(envRadScaleTerrain);SET_INIT(envRadScaleSky);SET_INIT(envTemp);SET_INIT(envHumidity);SET_INIT(envVisibility);SET_INIT(envWindV);SET_INIT(envWindDir);SET_INIT(simMode);SET_INIT(videoFps);
#undef SET_INIT
        auto& sensor=init.trackerSensor[0];for(int j=0;j<HwaSensorFields::count;++j){const auto& f=HwaSensorFields::fields()[j];if(!HwaSensorFields::set(sensor,f,v(f.name))){error=QString("无效传感器字段 %1").arg(f.name);return false;}}
        if(sensor.trackerSensorViewMin>=sensor.trackerSensorViewMax){error="最近距离须小于最远距离";return false;}
        const quint64 bytes=quint64(sensor.trackerSensorWidth)*sensor.trackerSensorHeight*(c.pixel->currentText()=="rgb24"?3:1);
        if(!sensor.h264En&&bytes>640000){error=QString("raw %1 字节超过 640000；拒绝，不缩放/截断").arg(bytes);return false;}
        c.effectiveRole=c.role->currentText();c.topic="TX/"+QString(c.effectiveRole=="precise"?"H":"L")+(sensor.h264En?"D264":"Decode");
        if(!topics.insert(c.topic).second){error="同 Domain/Partition 固定客户 Topic 冲突："+c.topic;return false;}
        c.target=(c.followModel->isChecked()?commonModel:c.model)->currentData().toInt();
        cmd.MissileMaxCount120=cmd.MissileMaxCount9=cmd.MissileMaxCountF35=cmd.MissileMaxCountF22=3;
        cmd.MissileMaxCountMMD=c.target==0x44?3:0;cmd.MissileMaxCountResv1=c.target==0x55?3:0;cmd.MissileMaxCountResv2=c.target==0x66?3:0;
        auto& spatial=cmd.platParamInit.spatial;
#define SET_SPATIAL(name) spatial.name=v(#name)
        SET_SPATIAL(lat);SET_SPATIAL(lon);SET_SPATIAL(alt);SET_SPATIAL(yaw);SET_SPATIAL(pitch);SET_SPATIAL(roll);SET_SPATIAL(speed);
#undef SET_SPATIAL
        if(loadSources){const QString path=sourceKey(c.followSource->isChecked()?commonSource->text():c.source->text());
            if(!sources.contains(path)){auto source=std::make_shared<Source>();source->path=path;if(!loadReplaySource(path,source->rows,error)){error=QString("路 %1：%2").arg(i+1).arg(error);return false;}sources[path]=source;}
            c.replay=sources[path];
            if(c.platformFromSource->isChecked()){const auto& row=c.replay->rows.first();spatial.lat=row.platPos.lat;spatial.lon=row.platPos.lon;spatial.alt=row.platPos.alt;spatial.yaw=row.platEul.yaw;spatial.pitch=row.platEul.pitch;spatial.roll=row.platEul.roll;spatial.speed=row.platSpeed;}
            if(!coverage(cmd,c.replay->rows,path,error)){error=QString("路 %1：%2").arg(i+1).arg(error);return false;}
        }
    }
    if(!selected){error="至少勾选一路";return false;}return true;
}

bool DualSensorPanel::control(int command){std::set<int> platforms;bool ok=true;for(const auto& c:channels)if(c.enabled)platforms.insert(c.init.platID);
    for(int plat:platforms){BYHWICD::ControlP2cX1ObjTrackingCmd cmd={};cmd.flag=0x41;cmd.JB=1;cmd.platID=plat;cmd.simCommand=command;cmd.currentRound=round;cmd.roundCut=rounds;std::string error;
        if(!dds->sendControl(cmd,error)){fail(QString::fromStdString(error));ok=false;}else qInfo()<<"[P19UIControl] platID="<<plat<<"command="<<command<<"round="<<round;}
    return ok;
}
void DualSensorPanel::reset(){if(busy()){fail("请先 STOP 并等待排空，再 RESET；当前操作仍在进行");return;}
    // 先复位冻结的旧身份，再解锁编辑，避免修改 ID 后错误地复位另一个实例。
    if(phase==Editable){QString error;if(!collect(error,false)){fail(error);return;}}
    if(!control(1))return;
    phase=Editable;freeze(false);for(int i=0;i<channels.size();++i){auto& c=channels[i];c.pending=c.ready=c.eof=false;c.sent=0;status(i,"已 RESET / 可编辑");}
    message->setText("已发送平台级 RESET；可修改设置，再分别 INIT。");message->setStyleSheet("");
}
void DualSensorPanel::initialize(){if(phase!=Editable){fail("INIT 前请 STOP / RESET，避免覆盖仍有效的回合");return;}
    if(!epoch.isValid()){fail("CSV回放需要显式有效的Demo/UtcDate和Demo/UtcHour；INIT未发送");return;}QString error;
    if(!collect(error,true)){fail(error);return;}
    QSettings actual(network,QSettings::IniFormat);
    if(domain->value()!=actual.value("DdsProtocol/DomainId",150).toInt()||qos->text()!=actual.value("DdsProtocol/QosFile","Config/DDS/ZRDDS_PROTOCOL_QOS.xml").toString()){
        fail("Domain/QoS 是部署设置，尚未在当前进程生效。请导出配置并重启控制端与 renderer。");return;}
    std::vector<DdsStimRoute> routes;for(auto& c:channels)if(c.enabled){DdsStimRoute route;route.platID=c.init.platID;route.sensorID=c.init.sensorID;route.role=c.effectiveRole.toStdString();routes.push_back(route);c.pending=true;c.ready=false;c.eof=false;c.sent=0;}
    dds->setExpectedRoutes(routes);dds->setVideoStatusObservation(observeStatus->isChecked());phase=AwaitingAck;phaseClock.restart();freeze(true);
    for(int i=0;i<channels.size();++i){auto& c=channels[i];if(!c.enabled)continue;std::string why;
        if(!dds->sendInit(c.init,why)){c.pending=false;phase=Failed;status(i,"INIT 发送失败");fail(QString::fromStdString(why));return;}
        status(i,"等待真实 ACK");qInfo()<<"[P19UIInit] route="<<i+1<<"platID="<<c.init.platID<<"sensorID="<<c.init.sensorID<<"role="<<c.effectiveRole<<"topic="<<c.topic<<"width="<<c.init.trackingInit.trackerSensor[0].trackerSensorWidth<<"source="<<c.replay->path<<"rows="<<c.replay->rows.size();
    }message->setText("已分别发送 INIT；等待每个选中身份的真实 ACK（30 秒超时）。");
}
void DualSensorPanel::receiveAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ack){if(phase!=AwaitingAck)return;
    for(int i=0;i<channels.size();++i){auto& c=channels[i];if(!c.enabled||!c.pending||ack.platID!=c.init.platID||ack.sensorID!=c.init.sensorID)continue;
        c.pending=false;c.ready=ack.trackingReady;status(i,c.ready?"ACK 就绪":"ACK 拒绝");qInfo()<<"[P19UIAck] platID="<<ack.platID<<"sensorID="<<ack.sensorID<<"ready="<<ack.trackingReady;
        if(!c.ready){phase=Failed;fail("renderer 拒绝 INIT；检查对应日志，再 RESET");return;}}
    for(const auto& c:channels)if(c.enabled&&!c.ready)return;
    phase=Ready;message->setText("选中路 ACK 全部就绪，可共同 START。");emit allInitialized();
}
void DualSensorPanel::start(){if(phase!=Ready){fail("必须等待所有选中路的 INIT ACK 后再 START");return;}if(!control(2)){phase=Failed;return;}phase=Starting;phaseClock.restart();message->setText(observeStatus->isChecked()?"START 已发送，异步等待 renderer 运行状态":"START 已发送；原状态关闭，等待 2 秒准备窗口（不宣称已观察应用就绪）");}
void DualSensorPanel::stop(){if(phase==Stopping||phase==Stopped||phase==Editable)return;const bool neverStarted=phase==AwaitingAck||phase==Ready;tick.stop();stopReplayWorker();if(!control(3)){phase=Failed;return;}if(neverStarted){phase=Stopped;message->setText("已发送 STOP；本地未 START、未发送实时帧。RESET 后可重试。");emit roundStopped();return;}phase=Stopping;phaseClock.restart();message->setText("STOP 已发，等待选中实例排空；平台级控制会联动同平台实例。");}
void DualSensorPanel::pollState(){
    if(phase==AwaitingAck&&phaseClock.elapsed()>30000){for(int i=0;i<channels.size();++i)if(channels[i].pending)status(i,"ACK 超时");phase=Failed;fail("INIT ACK 超时。核对部署身份、Domain、QoS 和 renderer 日志");}
    if(phase==Starting){if((observeStatus->isChecked()&&dds->runningStatusObserved())||(!observeStatus->isChecked()&&phaseClock.elapsed()>=2000)){phase=Running;ticks=0;pacing.restart();beginReplayWorker();}
        else if(phaseClock.elapsed()>10000){phase=Failed;fail("START 状态超时；未发送实时数据，可 STOP / RESET");}}
    if(phase==Running){
        for(int i=0;i<channels.size();++i)if(channels[i].enabled){channels[i].sent=replaySent[i].load();status(i,"运行");}
        if(replayDone.load(std::memory_order_acquire)){
            stopReplayWorker();
            if(!replayError.isEmpty())fail(replayError);
            for(int i=0;i<channels.size();++i)if(channels[i].enabled){channels[i].eof=true;status(i,"数据 EOF");}
            stop();
        }
    }
    if(phase==Stopping){const bool observed=dds->stoppedStatusObserved();if((observeStatus->isChecked()&&observed)||(!observeStatus->isChecked()&&phaseClock.elapsed()>=5000)){
            phase=Stopped;for(int i=0;i<channels.size();++i)if(channels[i].enabled)status(i,observed?"已停止 / 排空":"STOP 已发 / 排空未观测");
            qInfo()<<"[P19UIStop] round="<<round<<"applicationStopObserved="<<observed<<"elapsedMs="<<phaseClock.elapsed();message->setText(observed?"已观察全部选中实例停止。RESET 后可开始下一回合。":"未订阅原状态；STOP 后等待已结束，排空须由 renderer/客户日志确认。RESET 后可再 INIT。");emit roundStopped();
        }else if(phaseClock.elapsed()>30000){phase=Failed;fail("STOP 排空状态超时；保留日志，不伪造完成");}}
}
DualSensorPanel::~DualSensorPanel(){stopReplayWorker();}
void DualSensorPanel::stopReplayWorker(){replayCancel.store(true);if(replayWorker.joinable())replayWorker.join();}
void DualSensorPanel::beginReplayWorker(){
    stopReplayWorker();replayCancel.store(false);replayDone.store(false);replayError.clear();
    struct Packet {BYHWICD::DisplayC2cObjTrackingData value;int channel,row;double sourceTime;};
    using Tick=std::vector<Packet>;auto packets=std::make_shared<std::vector<Tick>>();
    int count=std::numeric_limits<int>::max();
    for(int i=0;i<channels.size();++i){replaySent[i].store(0);const auto& c=channels[i];if(c.enabled)count=std::min(count,c.replay->rows.size()-c.replay->cursor);}
    if(count==std::numeric_limits<int>::max()||count<=0){replayDone.store(true);return;}
    packets->reserve(count);
    // Snapshot only after INIT/ACK and START. The worker never touches widgets,
    // source cursors, protocol clock anchors, or mutable channel configuration.
    for(int n=0;n<count;++n){Tick batch;
        for(int i=0;i<channels.size();++i){const auto& c=channels[i];if(!c.enabled)continue;
            const int index=c.replay->cursor+n;const auto& row=c.replay->rows[index];
            Packet packet={};packet.channel=i;packet.row=index;packet.sourceTime=row.sourceTimeMs;
            auto& data=packet.value;data.flag=0x38;data.platID=c.init.platID;data.sensorID=c.init.sensorID;
            data.time=epoch.toMSecsSinceEpoch()+row.sourceTimeMs-c.replay->rows.first().sourceTimeMs;
            auto& p=data.platLoc;p.lat=row.platPos.lat;p.lon=row.platPos.lon;p.alt=row.platPos.alt;p.yaw=row.platEul.yaw;p.pitch=row.platEul.pitch;p.roll=row.platEul.roll;p.speed=row.platSpeed;
            const bool visible=c.forceVisible->isChecked()||row.viewValid;
            auto& w=data.weaponState;w.targetType=c.target;w.targetPlatID=3;w.targetID=3;w.lookatEn=true;w.viewValid=visible;w.strikeFlag=row.strikeFlag;w.strikePart=2;
            data.targetNumValid=1;auto& t=data.targetState[0];t.targetType=c.target;t.targetPlatID=3;t.targetID=3;t.engineState=true;t.viewValid=visible;t.targetState=1;
            auto& loc=t.targetLoc;loc.lat=row.tarPos.lat;loc.lon=row.tarPos.lon;loc.alt=row.tarPos.alt;loc.yaw=row.tarEul.yaw;loc.pitch=row.tarEul.pitch;loc.roll=row.tarEul.roll;loc.speed=c.target==0x11?row.platSpeed:row.tarSpeed;
            batch.push_back(packet);
        }packets->push_back(std::move(batch));
    }
    const long long periodNs=std::llround(stepMs->value()*1.e6);
    qInfo()<<"[P20M3 Pacing] owner=dedicated_worker periodNs="<<periodNs<<"ticks="<<count<<"sourceTimes=unchanged deadline=cumulative noDrop=1";
    replayWorker=std::thread([this,packets,periodNs]{
#if defined(_WIN32)
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
        typedef HANDLE (WINAPI *CreateTimer)(LPSECURITY_ATTRIBUTES,LPCWSTR,DWORD,DWORD);
        const auto createTimer=reinterpret_cast<CreateTimer>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"CreateWaitableTimerExW"));
        struct TimerHandle {HANDLE value;~TimerHandle(){if(value)CloseHandle(value);}} timer={createTimer?createTimer(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS):nullptr};
        // Windows 10+ high-resolution waitable timer. sleep_for(1ms) can round
        // to the process's 15.6ms timer quantum and create 15/30ms packet gaps.
        if(!timer.value){replayError="High-resolution pacing timer unavailable";replayDone.store(true,std::memory_order_release);return;}
#endif
        const auto start=std::chrono::steady_clock::now();
        for(size_t n=0;n<packets->size()&&!replayCancel.load();++n){
            const auto deadline=start+std::chrono::nanoseconds(periodNs*static_cast<long long>(n));
            while(!replayCancel.load()){
                const auto remaining=deadline-std::chrono::steady_clock::now();if(remaining<=std::chrono::steady_clock::duration::zero())break;
                if(remaining>std::chrono::microseconds(200)){
#if defined(_WIN32)
                    LARGE_INTEGER due;due.QuadPart=-std::max<long long>(1,(std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count()-100000)/100);
                    if(!SetWaitableTimer(timer.value,&due,0,nullptr,nullptr,FALSE)||WaitForSingleObject(timer.value,INFINITE)!=WAIT_OBJECT_0){replayError="High-resolution pacing wait failed";replayDone.store(true,std::memory_order_release);return;}
#else
                    std::this_thread::sleep_until(deadline-std::chrono::microseconds(100));
#endif
                }else std::atomic_signal_fence(std::memory_order_seq_cst);
            }
            if(replayCancel.load())break;
            for(const auto& packet:(*packets)[n]){
                std::string error;
                if(!dds->sendRealtime(packet.value,error)){replayError=QString::fromStdString(error);replayDone.store(true,std::memory_order_release);return;}
                const auto sent=replaySent[packet.channel].fetch_add(1)+1;
                if(sent<=2||sent%600==0)qInfo()<<"[P19UIRealtime] route="<<packet.channel+1<<"platID="<<packet.value.platID<<"sensorID="<<packet.value.sensorID<<"sourceRow="<<packet.row<<"sourceTimeMs="<<packet.sourceTime<<"sent="<<sent<<"model="<<packet.value.targetState[0].targetType;
            }
        }
        replayDone.store(true,std::memory_order_release);
    });
}
void DualSensorPanel::pump(){if(phase!=Running)return;
    std::set<Source*> advanced;std::set<int> endedPlatforms;
    for(auto& c:channels)if(c.enabled&&!c.eof&&c.replay->cursor>=c.replay->rows.size())endedPlatforms.insert(c.init.platID);
    // 本阶段一个调度器；任一路 EOF 时停止本回合，明确显示平台联动而不重复末行。
    if(!endedPlatforms.empty()){for(int i=0;i<channels.size();++i)if(channels[i].enabled){channels[i].eof=channels[i].replay->cursor>=channels[i].replay->rows.size();status(i,channels[i].eof?"数据 EOF":"另一源 EOF，共同 STOP");}stop();return;}
    for(int i=0;i<channels.size();++i){auto& c=channels[i];if(!c.enabled)continue;const auto& row=c.replay->rows[c.replay->cursor];
        BYHWICD::DisplayC2cObjTrackingData data={};data.flag=0x38;data.platID=c.init.platID;data.sensorID=c.init.sensorID;
        data.time=epoch.toMSecsSinceEpoch()+row.sourceTimeMs-c.replay->rows.first().sourceTimeMs;
        auto& p=data.platLoc;p.lat=row.platPos.lat;p.lon=row.platPos.lon;p.alt=row.platPos.alt;p.yaw=row.platEul.yaw;p.pitch=row.platEul.pitch;p.roll=row.platEul.roll;p.speed=row.platSpeed;
        const bool visible=c.forceVisible->isChecked()||row.viewValid;
        auto& w=data.weaponState;w.targetType=c.target;w.targetPlatID=3;w.targetID=3;w.lookatEn=true;w.viewValid=visible;w.strikeFlag=row.strikeFlag;w.strikePart=2;
        data.targetNumValid=1;auto& t=data.targetState[0];t.targetType=c.target;t.targetPlatID=3;t.targetID=3;t.engineState=true;t.viewValid=visible;t.targetState=1;
        auto& loc=t.targetLoc;loc.lat=row.tarPos.lat;loc.lon=row.tarPos.lon;loc.alt=row.tarPos.alt;loc.yaw=row.tarEul.yaw;loc.pitch=row.tarEul.pitch;loc.roll=row.tarEul.roll;loc.speed=c.target==0x11?row.platSpeed:row.tarSpeed;
        std::string error;if(!dds->sendRealtime(data,error)){status(i,"Realtime 写失败");fail(QString::fromStdString(error));stop();return;}++c.sent;advanced.insert(c.replay.get());
        if(c.sent<=2||c.sent%600==0)qInfo()<<"[P19UIRealtime] route="<<i+1<<"platID="<<data.platID<<"sensorID="<<data.sensorID<<"sourceRow="<<c.replay->cursor<<"sourceTimeMs="<<row.sourceTimeMs<<"sent="<<c.sent<<"model="<<c.target;
    }
    // 相同规范路径只推进一次；不同源各自一行，均以同一个 tick 起点调度。
    for(auto* source:advanced)++source->cursor;
    ++ticks;if(ticks%12==0||ticks==1)for(int i=0;i<channels.size();++i)if(channels[i].enabled)status(i,"运行");
    const double deadline=ticks*stepMs->value();tick.start(int(std::max(0.,std::ceil(deadline-pacing.nsecsElapsed()/1.e6))));
}

QJsonObject DualSensorPanel::snapshot() const {QJsonObject root;root["version"]=19;root["source"]=commonSource->text();root["model"]=commonModel->currentData().toInt();root["stepMs"]=stepMs->value();root["domain"]=domain->value();root["videoDomain"]=videoDomain->value();root["customerDomain"]=customerDomain->value();root["qos"]=qos->text();root["observeStatus"]=observeStatus->isChecked();QJsonArray routes;
    for(const auto& c:channels){QJsonObject j;j["enabled"]=c.selected->isChecked();j["role"]=c.role->currentText();j["platID"]=c.plat->value();j["sensorID"]=c.sensor->value();j["source"]=c.source->text();j["followSource"]=c.followSource->isChecked();j["model"]=c.model->currentData().toInt();j["followModel"]=c.followModel->isChecked();j["pixelFormat"]=c.pixel->currentText();j["platformFromSource"]=c.platformFromSource->isChecked();j["forceVisible"]=c.forceVisible->isChecked();QJsonObject values;for(auto it=c.values.begin();it!=c.values.end();++it)values[it.key()]=it.value()->value();j["init"]=values;routes.append(j);}root["channels"]=routes;return root;
}
bool DualSensorPanel::saveConfig(const QString& path){try{putFile(path,QJsonDocument(snapshot()).toJson());message->setText("配置已保存："+path);return true;}catch(const std::exception& e){fail(e.what());return false;}}
bool DualSensorPanel::loadConfig(const QString& path){if(phase!=Editable){fail("读取配置前须 STOP / RESET");return false;}
    if(path.endsWith(".ini",Qt::CaseInsensitive)){QSettings old(path,QSettings::IniFormat);old.setIniCodec("UTF-8");auto& c=channels[tabs->currentIndex()];
        // 迁移实际存在的旧键；不将本地身份误称为 INIT 可修改的 renderer 身份。
        const int plat=old.value("Identity/platID",1001).toInt(),sensor=old.value("Identity/sensorID",2).toInt();
        if(plat<1||sensor<1||sensor>254){fail("旧 INI 身份无效");return false;}
        QMap<QString,double> migrated;
        for(auto it=c.values.begin();it!=c.values.end();++it){QString key;
            for(const auto& section:{QString("SensorInit/"),QString("WeatherInit/"),QString("PlatformInit/"),QString("RenderControl/")})if(old.contains(section+it.key()))key=section+it.key();
            if(it.key()=="envSky"&&old.contains("Demo/envSky"))key="Demo/envSky";
            if(key.isEmpty())continue;bool ok=false;const double value=old.value(key).toDouble(&ok);
            if(!ok||!std::isfinite(value)||value<it.value()->minimum()||value>it.value()->maximum()||(it.value()->decimals()==0&&value!=std::floor(value))){fail("旧 INI 字段无效："+key);return false;}
            migrated[it.key()]=value;
        }
        c.plat->setValue(old.value("Identity/platID",1001).toInt());c.sensor->setValue(old.value("Identity/sensorID",2).toInt());
        for(auto it=migrated.begin();it!=migrated.end();++it)c.values[it.key()]->setValue(it.value());
        const QString role=old.value("Identity/channel",c.role->currentText()).toString();if(role=="coarse"||role=="precise")c.role->setCurrentText(role);
        bool modelOk=false;const int model=old.value("Demo/TargetType","0x22").toString().toInt(&modelOk,0);
        if(modelOk&&c.model->findData(model)>=0){c.model->setCurrentIndex(c.model->findData(model));c.followModel->setChecked(false);}
        c.source->setText(QDir(QFileInfo(path).absolutePath()).absoluteFilePath(old.value("Demo/InputFile","1.txt").toString()));c.followSource->setChecked(false);
        c.selected->setChecked(true);channels[1-tabs->currentIndex()].selected->setChecked(false);
        message->setText("旧单路 INI 已迁移到当前标签页，仅勾选本路；请核对部署 role，再 INIT。重新启用另一路时检查 ID / Topic 冲突。");return true;
    }
    QFile file(path);if(!file.open(QIODevice::ReadOnly)){fail(file.errorString());return false;}QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);const auto root=doc.object();const auto routes=root["channels"].toArray();
    if(error.error!=QJsonParseError::NoError||routes.size()!=channels.size()){fail("双路配置 JSON 无效，需要两条 channels");return false;}
    const double step=root.value("stepMs").toDouble(1000./60),domainValue=root.value("domain").toDouble(150);
    for(const auto& key:{QString("domain"),QString("videoDomain"),QString("customerDomain")}) {
        if(!root.contains(key))continue;
        const auto value=root.value(key);const double number=value.toDouble();
        if(!value.isDouble()||!std::isfinite(number)||number!=std::floor(number)||number<(key=="customerDomain"?-1:0)||number>232){fail("无效 DDS 域："+key);return false;}
    }
    if(!std::isfinite(step)||step<stepMs->minimum()||step>stepMs->maximum()||domainValue!=std::floor(domainValue)||domainValue<0||domainValue>232||commonModel->findData(root.value("model").toInt(0x22))<0){fail("无效调度间隔、Domain 或公共模型");return false;}
    // 先检查全部值，防止 Qt 控件自动夹值把无效配置伪装为有效。
    for(int i=0;i<routes.size();++i){const auto j=routes[i].toObject();const auto values=j["init"].toObject();
        if((j["role"].toString()!="coarse"&&j["role"].toString()!="precise")||j["platID"].toInt()<=0||j["sensorID"].toInt()<1||j["sensorID"].toInt()>254){fail("无效路由身份或 role");return false;}
        const QString pixels=j.value("pixelFormat").toString("gray8");
        if((pixels!="gray8"&&pixels!="rgb24")||channels[i].model->findData(j.value("model").toInt(0x22))<0||j["platID"].toDouble()!=j["platID"].toInt()||j["sensorID"].toDouble()!=j["sensorID"].toInt()){fail("无效像素格式、模型或非整数身份");return false;}
        for(auto it=values.begin();it!=values.end();++it){auto* box=channels[i].values.value(it.key(),nullptr);double v=it.value().toDouble();if(!box||!it.value().isDouble()||!std::isfinite(v)||v<box->minimum()||v>box->maximum()||(box->decimals()==0&&v!=std::floor(v))){fail("无效 INIT 字段："+it.key());return false;}}
    }
    commonSource->setText(root.value("source").toString(commonSource->text()));commonModel->setCurrentIndex(commonModel->findData(root.value("model").toInt(0x22)));stepMs->setValue(root.value("stepMs").toDouble(1000./60));domain->setValue(root.value("domain").toInt(150));videoDomain->setValue(root.value("videoDomain").toInt(domain->value()));customerDomain->setValue(root.value("customerDomain").toInt(-1));qos->setText(root.value("qos").toString(qos->text()));observeStatus->setChecked(root.value("observeStatus").toBool(true));
    for(int i=0;i<routes.size();++i){auto& c=channels[i];const auto j=routes[i].toObject();c.selected->setChecked(j["enabled"].toBool());c.role->setCurrentText(j["role"].toString());c.plat->setValue(j["platID"].toInt());c.sensor->setValue(j["sensorID"].toInt());c.source->setText(j["source"].toString());c.followSource->setChecked(j.value("followSource").toBool(true));c.followModel->setChecked(j.value("followModel").toBool(true));c.model->setCurrentIndex(c.model->findData(j.value("model").toInt(0x22)));c.pixel->setCurrentText(j.value("pixelFormat").toString("gray8"));c.platformFromSource->setChecked(j.value("platformFromSource").toBool(true));c.forceVisible->setChecked(j.value("forceVisible").toBool());const auto values=j["init"].toObject();for(auto it=values.begin();it!=values.end();++it)c.values[it.key()]->setValue(it.value().toDouble());}
    message->setText("已读取双路配置；导出 Domain 与当前进程不同须重启。");return true;
}
void DualSensorPanel::copyChannel(int from,int to){if(phase!=Editable){fail("复制前须 RESET 解锁");return;}auto& a=channels[from];auto& b=channels[to];
    for(auto it=a.values.begin();it!=a.values.end();++it)if(it.key()!="h264En"&&it.key()!="trackerSensorWidth"&&it.key()!="trackerSensorHeight")b.values[it.key()]->setValue(it.value()->value());
    b.source->setText(a.source->text());b.followSource->setChecked(a.followSource->isChecked());b.model->setCurrentIndex(a.model->currentIndex());b.followModel->setChecked(a.followModel->isChecked());b.platformFromSource->setChecked(a.platformFromSource->isChecked());b.forceVisible->setChecked(a.forceVisible->isChecked());
    message->setText("参数已复制；保留接收路的 ID、role、分辨率、编码及像素格式，避免制造重复生产者。");
}

bool DualSensorPanel::exportDeployment(const QString& directory){QString error;if(!collect(error,false)){fail(error);return false;}
    try{QDir out(directory);if(!out.exists()&&!QDir().mkpath(directory))throw std::runtime_error("无法创建导出目录");
        putFile(out.filePath("dual_sensor.json"),QJsonDocument(snapshot()).toJson());
        QString instructions="# P19 双路部署\n\n以下是本地部署配置，不会由 INIT 修改已运行 renderer。复制匹配版本的 Config 资源和许可到每个独立工作目录；控制端匹配控制域，VideoDisplay 匹配原视频域，客户 Demo 匹配客户域；每个 renderer 只初始化一次共享 DDS 工厂。\n\n";
        instructions+=QString("控制域=%1，原视频域=%2，客户域=%3（-1 跟随原视频）。环境变量可覆盖 INI；启动时核对 [DdsDomains] 日志。修改后重启对应进程，RESET 不会切域。\n\n").arg(domain->value()).arg(videoDomain->value()).arg(customerDomain->value());
        for(int i=0;i<channels.size();++i){const auto& c=channels[i];const QString name=QString("route%1").arg(i+1);out.mkpath(name);QDir dir(out.filePath(name));
            QSettings dst(dir.filePath("NetworkConfig.ini"),QSettings::IniFormat);QSettings src(network,QSettings::IniFormat);for(const auto& k:src.allKeys())dst.setValue(k,src.value(k));
            dst.setValue("Identity/channel",c.role->currentText());dst.setValue("Identity/platID",c.plat->value());dst.setValue("Identity/sensorID",c.sensor->value());dst.setValue("DdsProtocol/DomainId",domain->value());dst.setValue("DdsProtocol/QosFile",qos->text());dst.setValue("UDP/localIp","192.168.1.116");dst.setValue("UDP/localPort",8888+i);dst.setValue("Identity/acceptSensorBroadcast",false);dst.setValue("TCP/serverIp","192.168.1.188");dst.setValue("TCP/serverPort",5554+i);dst.sync();
            QSettings runtime(dir.filePath("HwaSimIRRuntime.ini"),QSettings::IniFormat);
            const QString baseRuntime=QDir(QCoreApplication::applicationDirPath()).filePath("Config/HwaSimIRRuntime.ini");
            if(!QFileInfo::exists(baseRuntime))throw std::runtime_error("Missing complete Config/HwaSimIRRuntime.ini export template");
            QSettings baseline(baseRuntime,QSettings::IniFormat);
            for(const auto& key:baseline.allKeys())runtime.setValue(key,baseline.value(key));
            runtime.setValue("DdsProtocol/DomainId",domain->value());runtime.setValue("DdsVideo/DomainId",videoDomain->value());runtime.setValue("CustomerDdsVideo/DomainId",customerDomain->value());runtime.setValue("DdsProtocol/QosFile",qos->text());runtime.setValue("DdsVideo/QosFile",qos->text());runtime.setValue("CustomerDdsVideo/Enable",true);runtime.setValue("CustomerDdsVideo/Mode",c.values["h264En"]->value()!=0?"h264":"raw");runtime.setValue("CustomerDdsVideo/PixelFormat",c.pixel->currentText());runtime.setValue("DdsProtocol/Enable",true);runtime.setValue("DdsVideo/Enable",true);runtime.sync();
            instructions+=QString("## %1\n\nrole=%2 platID=%3 sensorID=%4，选择发送=%5。将导出的 HwaSimIRRuntime.ini 放入本路 Config，NetworkConfig.ini 放到独立工作目录。启动：HWASIMIR_ROOT=$PWD HWASIMIR_CHANNEL=%2 HWASIMIR_NETWORK_CONFIG=$PWD/NetworkConfig.ini HWASIMIR_RUNTIME_CONFIG=$PWD/Config/HwaSimIRRuntime.ini ./run_precise.sh。核对日志中的实际 identity/domain/topic。\n\n").arg(name,c.role->currentText()).arg(c.plat->value()).arg(c.sensor->value()).arg(c.selected->isChecked());
        }
        QSettings sender(out.filePath("SenderNetworkConfig.ini"),QSettings::IniFormat);QSettings base(network,QSettings::IniFormat);for(const auto& k:base.allKeys())sender.setValue(k,base.value(k));sender.setValue("DdsProtocol/DomainId",domain->value());sender.setValue("DdsProtocol/QosFile",qos->text());sender.sync();
        instructions+="启动控制端：DataDrivenTestQT --network-config=SenderNetworkConfig.ini --channels-json=dual_sensor.json。QoS、LUT 路径及原始数据文件须可读；SDK 许可由部署方提供。平台级控制会联动同平台其它实例。固定 TX Topic 不能靠不同 platID 区分两个生产者。\n";
        putFile(out.filePath("README.md"),instructions.toUtf8());message->setText("已导出部署设置："+directory);return true;
    }catch(const std::exception& e){fail(e.what());return false;}
}
