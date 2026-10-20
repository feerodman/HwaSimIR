"""Prepare explicit production GUI configuration using this candidate's binaries/parameters."""
import configparser, json
from pathlib import Path
R=Path(__file__).resolve().parents[1];d=R/'build-p20m3-stim/release'
c=configparser.ConfigParser(interpolation=None);c.optionxform=str;c.read(d/'NetworkConfig.ini',encoding='utf-8-sig')
c['DdsProtocol']['DomainId']='150'
with (d/'P20M3_Board_NetworkConfig.ini').open('w',encoding='utf-8') as f:c.write(f)
source=R/'1020测试内容/circleDataAll3.csv';channels=[]
for role,band,width,sensor in [('coarse',2,320,1),('precise',0,800,2)]:
    channels.append(dict(enabled=True,role=role,platID=1001,sensorID=sensor,source=str(source),followSource=True,model=34,followModel=True,pixelFormat='gray8',platformFromSource=True,
        init=dict(trackerSensorWidth=width,trackerSensorHeight=width,h264En=int(role=='precise'),saveMP4En=0,simMode=1,videoFps=60,trackerSensorBand=band,envSky=1,envVisibility=6000,envHumidity=85)))
(d/'P20M3_Board_Dual.json').write_text(json.dumps(dict(version=19,domain=150,source=str(source),model=34,stepMs=1000/60,qos=c['DdsProtocol']['QosFile'],observeStatus=True,channels=channels),indent=2),encoding='utf-8')
print(d/'P20M3_Board_NetworkConfig.ini');print(d/'P20M3_Board_Dual.json')
