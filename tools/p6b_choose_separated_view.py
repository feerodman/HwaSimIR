import json,math,numpy as np,itertools
from pathlib import Path
rows=[json.loads(x) for x in Path('logs/p6b/world_windows_a.jsonl').read_text().splitlines()]
pos=np.array([r['position'] for r in rows]); best=[]; fov=math.radians(9.1673247)
for i,j in itertools.combinations(range(len(rows)),2):
 a,b=pos[i],pos[j]; mid=(a+b)/2
 if np.linalg.norm((a-b)[:2])>5000:continue
 for ang in np.arange(0,360,6):
  for dist in (3800,4500,5200,5700):
   for alt in (1600,2400,3200,4000,4800):
    c=mid.copy();c[:2]-=dist*np.array([math.sin(math.radians(ang)),math.cos(math.radians(ang))]);c[2]=alt
    ds=np.linalg.norm((pos-c)[:,:2],axis=1)
    if max(ds[i],ds[j])>5850 or max(abs(c[:2]))>6000:continue
    nearest=np.argsort(ds)[:8]
    if i not in nearest or j not in nearest:continue
    forward=(mid-c);forward/=np.linalg.norm(forward);right=np.cross(forward,[0,0,1]);right/=np.linalg.norm(right);up=np.cross(right,forward)
    centers=[];sizes=[]
    for k in (i,j):
     delta=pos[k]-c;depth=np.dot(delta,forward);centers.append(np.array([np.dot(delta,right),np.dot(delta,up)])/depth)
     r=rows[k];rot=math.radians(r['rotation']);x=np.array([math.cos(rot),math.sin(rot),0])*r['radius'][0]*.72;y=np.array([-math.sin(rot),math.cos(rot),0])*r['radius'][1]*.43;z=np.array([0,0,r['radius'][2]*.78]);M=np.array([x,y,z]);sizes.append(np.array([np.linalg.norm(M@right),np.linalg.norm(M@up)])/depth)
    centers=np.array(centers);sizes=np.array(sizes);gap=np.abs(centers[0]-centers[1])-sizes.sum(axis=0);edge=np.max(np.abs(centers)+sizes)
    if edge<.3 and np.max(np.abs(centers))<.11:
     score=max(gap)-max(0,edge-math.tan(fov/2))
     best.append((float(score),i,j,c.tolist(),math.degrees(math.atan2(forward[0],forward[1])),math.degrees(math.asin(forward[2])),gap.tolist()))
best.sort(reverse=True);Path('logs/p6b/separated_view_search.json').write_text(json.dumps(best[:20],indent=2));print(best[:3])
if best:
 score,i,j,c,yaw,pitch,gap=best[0]
 lat=math.radians(40);lon=math.radians(116);aa=6378137.;ee=6.6943799901413165e-3;N=aa/math.sqrt(1-ee*math.sin(lat)**2);orig=np.array([N*math.cos(lat)*math.cos(lon),N*math.cos(lat)*math.sin(lon),N*(1-ee)*math.sin(lat)]);R=np.array([[-math.sin(lon),-math.sin(lat)*math.cos(lon),math.cos(lat)*math.cos(lon)],[math.cos(lon),-math.sin(lat)*math.sin(lon),math.cos(lat)*math.sin(lon)],[0,math.cos(lat),math.sin(lat)]]);e=orig+R@c;lon2=math.atan2(e[1],e[0]);p=math.hypot(e[0],e[1]);lat2=math.atan2(e[2],p*(1-ee))
 for _ in range(10):N=aa/math.sqrt(1-ee*math.sin(lat2)**2);h=p/math.cos(lat2)-N;lat2=math.atan2(e[2],p*(1-ee*N/(N+h)))
 obj={'Schema':'ordinary_weather_camera_1','SimulationEpochMs':21600000,'Keyframes':[[0,math.degrees(lat2),math.degrees(lon2),h,0,yaw,pitch]],'PublicCameraENU':c,'ExpectedNearbyCloudIds':[rows[i]['cloudId'],rows[j]['cloudId']],'Description':'Camera chosen from real deterministic world descriptors; no cloud placement override.'};Path('tools/p6b_inputs/pair_separated.json').write_text(json.dumps(obj,indent=2)+'\n');print(json.dumps(obj,indent=2))
