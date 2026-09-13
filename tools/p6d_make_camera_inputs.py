"""Ordinary protocol camera paths through an existing cloud; never places clouds."""
from pathlib import Path
import json,math
import numpy as np
OUT=Path('tools/p6d_inputs')
lat,lon=map(math.radians,(40,116));a=6378137.;e=6.6943799901413165e-3
N=a/math.sqrt(1-e*math.sin(lat)**2)
origin=np.array([N*math.cos(lat)*math.cos(lon),N*math.cos(lat)*math.sin(lon),N*(1-e)*math.sin(lat)])
R=np.array([[-math.sin(lon),-math.sin(lat)*math.cos(lon),math.cos(lat)*math.cos(lon)],
            [math.cos(lon),-math.sin(lat)*math.sin(lon),math.cos(lat)*math.sin(lon)],[0,math.cos(lat),math.sin(lat)]])
def lla(c):
    x,y,z=origin+R@c;long=math.atan2(y,x);p=math.hypot(x,y);phi=math.atan2(z,p*(1-e))
    for _ in range(12):
        nn=a/math.sqrt(1-e*math.sin(phi)**2);h=p/math.cos(phi)-nn;phi=math.atan2(z,p*(1-e*nn/(nn+h)))
    return [math.degrees(phi),math.degrees(long),h]
def write(name,rows,description):
    (OUT/(name+'.json')).write_text(json.dumps(dict(Schema='ordinary_weather_camera_1',SimulationEpochMs=21600000,
        Keyframes=rows,Description=description+' No renderer camera or cloud placement override.',ExpectedNearbyCloudIds=['D73EEA5ECB3EA0B8','198EE8338358C182']),indent=2)+'\n')
near=json.loads(Path('tools/p6b_inputs/pair_near.json').read_text());pos=np.array(near['PublicCameraENU']);yaw,pitch=near['Keyframes'][0][-2:]
right=np.array([math.cos(math.radians(yaw)),-math.sin(math.radians(yaw)),0])
write('near_translate',[[t,*lla(pos+right*d),0,yaw,pitch] for t,d in [(0,0),(3,0),(10,240),(17,-240),(24,0)]],
      'Near camera moves 240 m across the view and returns; ordinary world clouds remain unchanged.')
center=np.array([3931.8070168456352,-8357.746162303416,2661.767198383478])
forward=np.array([math.sin(math.radians(yaw)),math.cos(math.radians(yaw)),0])
write('proxy_crossing',[[t,*lla(center+forward*d),0,yaw,0] for t,d in [(0,-800),(3,-800),(9,-150),(12,0),(15,150),(21,800),(24,800)]],
      'Camera enters and leaves the existing proxy; also exposes the native proxy-depth approximation.')
for band in (1,2):
    frozen=json.loads((OUT/f'b{band}_base.json').read_text())
    for name,d in [('plate_front',1000),('plate_back',6500)]:
        (OUT/f'b{band}_{name}.json').write_text(json.dumps(dict(frozen,WorldCloudOrdinaryPlate='1',P6DPlateRangeM=str(d)),indent=2)+'\n')
