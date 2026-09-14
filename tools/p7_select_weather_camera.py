"""Select camera inputs from actual C++ world descriptors; never place clouds."""
import argparse,itertools,json,math
from pathlib import Path
import numpy as np

def geodetic(c):
    lat,lon=map(math.radians,(40,116));a=6378137.;e2=6.6943799901413165e-3
    n=a/math.sqrt(1-e2*math.sin(lat)**2)
    origin=np.array([n*math.cos(lat)*math.cos(lon),n*math.cos(lat)*math.sin(lon),n*(1-e2)*math.sin(lat)])
    rotation=np.array([[-math.sin(lon),-math.sin(lat)*math.cos(lon),math.cos(lat)*math.cos(lon)],
        [math.cos(lon),-math.sin(lat)*math.sin(lon),math.cos(lat)*math.sin(lon)],[0,math.cos(lat),math.sin(lat)]])
    xyz=origin+rotation@c;longitude=math.atan2(xyz[1],xyz[0]);p=math.hypot(xyz[0],xyz[1]);latitude=math.atan2(xyz[2],p*(1-e2))
    for _ in range(12):
        n=a/math.sqrt(1-e2*math.sin(latitude)**2);h=p/math.cos(latitude)-n
        latitude=math.atan2(xyz[2],p*(1-e2*n/(n+h)))
    return [math.degrees(latitude),math.degrees(longitude),h]

def select(source,output):
    rows=[json.loads(x) for x in source.read_text().splitlines()];positions=np.array([r['position'] for r in rows]);best=[]
    for i,j in itertools.combinations(range(len(rows)),2):
        a,b=positions[i],positions[j];separation=np.linalg.norm((a-b)[:2])
        if separation<900 or separation>2800:continue
        mid=(a+b)*.5
        if np.max(np.abs(mid[:2]))>6000:continue
        # Mostly along the pair axis, with a small offset to show distinct lobes.
        pairAngle=math.atan2((a-b)[0],(a-b)[1])
        for angle in [pairAngle+x for x in (-.22,-.12,.12,.22,math.pi-.22,math.pi+.22)]:
            for distance in (3300,3900,4500):
                camera=mid-np.array([math.sin(angle)*distance,math.cos(angle)*distance,0]);camera[2]=min(3300,max(2400,mid[2]-300))
                distances=np.linalg.norm((positions-camera)[:,:2],axis=1)
                # Renderer selects the nearest eight around the nearby game asset.
                forward=mid-camera;forward/=np.linalg.norm(forward)
                asset=camera+forward*250;nearest=np.argsort(np.linalg.norm((positions-asset)[:,:2],axis=1))[:8]
                if i not in nearest or j not in nearest or max(distances[i],distances[j])>5700:continue
                right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
                projected=[];radii=[]
                for k in (i,j):
                    delta=positions[k]-camera;depth=float(delta@forward)
                    if depth<100:break
                    projected.append(np.array([delta@right,delta@up])/depth)
                    r=rows[k];rot=math.radians(r['rotation']);basis=np.array([[math.cos(rot)*r['radius'][0]*.65,math.sin(rot)*r['radius'][0]*.65,0],
                        [-math.sin(rot)*r['radius'][1]*.43,math.cos(rot)*r['radius'][1]*.43,0],[0,0,r['radius'][2]*.68]])
                    radii.append(np.array([np.linalg.norm(basis@right),np.linalg.norm(basis@up)])/depth)
                if len(projected)!=2:continue
                centers=np.array(projected);radii=np.array(radii)
                if np.max(abs(centers))>.065:continue
                gap=abs(centers[0,0]-centers[1,0]);extent=np.max(abs(centers)+radii)
                score=gap-.6*max(0,extent-.08)-.25*abs(radii[0,0]-radii[1,0])
                best.append((score,i,j,camera,forward,centers,radii))
    if not best:raise RuntimeError('No ordinary camera candidate')
    best.sort(key=lambda x:x[0],reverse=True);score,i,j,camera,forward,centers,radii=best[0]
    yaw=math.degrees(math.atan2(forward[0],forward[1]));pitch=math.degrees(math.asin(forward[2]));asset=camera+forward*250
    value=dict(Schema='ordinary_weather_camera_1',SimulationEpochMs=21600000,
        Keyframes=[[0,*geodetic(camera),0,yaw,pitch]],GameAssetPose=[*geodetic(asset),yaw+65,0,0],
        PublicCameraENU=camera.tolist(),ExpectedNearbyCloudIds=[rows[i]['cloudId'],rows[j]['cloudId']],
        InitializationWeather=dict(envMaxHeightRain=4500,envTransHeightRain=800,envMaxHeightSnow=4500,envTransHeightSnow=800,envRainSnowSpeedScale=1),
        Description='Camera and existing game-asset pose chosen from actual world descriptors; no cloud placement or budget override.')
    output.write_text(json.dumps(value,indent=2)+'\n')
    output.with_suffix('.selection.json').write_text(json.dumps(dict(score=score,clouds=[rows[i],rows[j]],projectedCenters=centers.tolist(),projectedRadii=radii.tolist(),pixelVisibility='pending_actual_render'),indent=2))
    print(output)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args();select(a.source,a.output)
