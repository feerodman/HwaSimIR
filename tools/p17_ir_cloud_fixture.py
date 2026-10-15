"""Ordinary SWIR/MWIR cloud views inside the existing measured LUT domain.

Camera stays above the existing cloud, rather than claiming unsupported
in-cloud atmospheric queries. Cloud descriptors and all radiometry are unchanged.
"""
import csv
import json
import math
from datetime import date
from pathlib import Path
from p14_generate_ground_weather_fixture import geodetic_los_m, solar_zenith

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'logs/p17/ir_retest/fixtures'
OUT.mkdir(parents=True, exist_ok=True)
lat, lon = map(math.radians, (40, 100))
a, e2 = 6378137., 6.69437999014e-3
n = a/math.sqrt(1-e2*math.sin(lat)**2)
origin = (n*math.cos(lat)*math.cos(lon), n*math.cos(lat)*math.sin(lon), n*(1-e2)*math.sin(lat))


def geodetic(x, y, z):
    xx = origin[0]-math.sin(lon)*x-math.sin(lat)*math.cos(lon)*y+math.cos(lat)*math.cos(lon)*z
    yy = origin[1]+math.cos(lon)*x-math.sin(lat)*math.sin(lon)*y+math.cos(lat)*math.sin(lon)*z
    zz = origin[2]+math.cos(lat)*y+math.sin(lat)*z
    p = math.hypot(xx, yy)
    phi = math.atan2(zz, p*(1-e2))
    for _ in range(10):
        nn = a/math.sqrt(1-e2*math.sin(phi)**2)
        h = p/math.cos(phi)-nn
        phi = math.atan2(zz, p*(1-e2*nn/(nn+h)))
    return [math.degrees(phi), math.degrees(math.atan2(yy, xx)), h]


center = (1827.813, -1496.287, 9676.007)
target = geodetic(center[0], center[1], 9850)
poses = [(0,-1500,-11000,11200), (6,-1500,-11000,11200),
         (15,1827.813,-7000,11200), (24,4000,-5000,11200), (30,4000,-5000,11200)]
frames = []
for t,x,y,z in poses:
    dx,dy,dz = (center[0]-x,center[1]-y,center[2]-z)
    frames.append([t,*geodetic(x,y,z),0,math.degrees(math.atan2(dx,dy)),
                   math.degrees(math.atan2(dz,math.hypot(dx,dy)))])
fixture = dict(Schema='ordinary_weather_camera_1', SimulationEpochMs=1788665400000,
    Keyframes=frames, SyntheticTelemetryTargets=[[0x12,100,1,*target,90,0,0,0,0]],
    SyntheticTelemetryKeyframes=[[t,*target,90,0,0,0,0] for t in (0,31)],
    InitializationWeather=dict(envVisibility=6000,envHumidity=85),
    Description=__doc__, ReferenceCloudENU=center, ExpectedNearbyCloudIds=['D4CB564594352F28'],
    Scope='Above-cloud approach and lateral parallax; actual F22 near a reference descriptor; runtime streaming and occlusion require separate verification; no in-cloud camera claim',
    Bands=['SWIR','MWIR'])
(OUT/'cloud_above.json').write_text(json.dumps(fixture,indent=2)+'\n',encoding='utf-8')
(OUT/'audit.json').write_text(json.dumps({'WorldCloudAudit':'1'})+'\n',encoding='utf-8')
rows=[]
for index in range(1861):
    t=index/60
    k=max(i for i,f in enumerate(frames) if f[0]<=t)
    f,g=frames[k],frames[min(k+1,len(frames)-1)]
    blend=min(1,(t-f[0])/(g[0]-f[0])) if g[0]>f[0] else 0
    obs=[f[j]+blend*(g[j]-f[j]) for j in (1,2,3)]
    distance=geodetic_los_m(obs,target)[0]/1000
    for band in ('SWIR','MWIR'):
        rows.append(dict(rowIndex=index,sourceLine=index+2,sourceTimeMs=t*1000,
            environment='cloud_above',envSky=1,band=band,observerAltKm=obs[2]/1000,
            targetAltKm=target[2]/1000,productionPreflightLosKm=distance,visibilityKm=6,
            relativeHumidityPercent=85,solarZenithDeg=solar_zenith(obs[0],obs[1],date(2026,9,6),3.5+t/3600)))
with (OUT/'cloud_queries.csv').open('w',newline='',encoding='utf-8') as stream:
    writer=csv.DictWriter(stream,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
print(OUT/'cloud_above.json')
