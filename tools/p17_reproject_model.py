"""Known-geometry reprojection of the explicit AIM120 P5 camera diagnostic.

Read production identities from H.264 SEI, use the source-time camera equation,
including the ordinary camera parent's HPR, and rasterize the unchanged OBJ
to exclude occlusion/disocclusion. This is not
an optical-flow estimate and never reports a rendering-FPS from pixel hashes.
Only applicable to the declared fixture, not arbitrary ordinary UI footage.
"""
import argparse,csv,json,re,pathlib,sys
import numpy as np
from scipy.ndimage import binary_erosion, map_coordinates
from PIL import Image
from p7_validate_recording import product
from p16_production_flicker_compare import decode

ROOT=pathlib.Path(__file__).resolve().parents[1]
SIZE=800
TAN=np.tan(np.deg2rad(17.5))

def mesh(path):
    vertices=[];faces=[]
    for line in path.read_text(errors='replace').splitlines():
        f=line.split()
        if not f:continue
        if f[0]=='v':vertices.append([float(x) for x in f[1:4]])
        elif f[0]=='f' and len(f)>=4:
            ids=[int(x.split('/')[0]) for x in f[1:]]
            ids=[x-1 if x>0 else len(vertices)+x for x in ids]
            for i in range(1,len(ids)-1): faces.append([ids[0],ids[i],ids[i+1]])
    v=np.array(vertices);t=v[np.array(faces)]
    area=np.linalg.norm(np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0]),axis=1)
    return v,t[area>1e-12]

def camera(vertices,time,view,parentHpr):
    focus=(vertices.min(0)+vertices.max(0))*.5
    distance=np.linalg.norm(vertices.max(0)-vertices.min(0))*2
    offset=np.array([distance*.65,-distance,distance*.3])
    if view=='parallax':offset[0]+=np.sin(time*.001)*distance*.25
    eye=focus+offset;forward=-offset/np.linalg.norm(offset)
    # P5 camera remains parented to the ordinary platform. NodePath look_at
    # uses parent-space up; ignoring the unchanged platform attitude invents
    # approximately one-degree image rotations and false temporal residuals.
    from scipy.spatial.transform import Rotation
    worldUp=Rotation.from_euler('ZXY',parentHpr,degrees=True).apply([0,0,1])
    right=np.cross(forward,worldUp);right/=np.linalg.norm(right)
    up=np.cross(right,forward)
    return eye,np.array([right,up,forward])

def project(points,cam):
    q=(points-cam[0])@cam[1].T
    xy=q[...,:2]/q[...,2,None]/TAN*(SIZE*.5)
    xy=xy*np.array([1,-1])+SIZE*.5
    return xy,q[...,2]

def raster(tris,cam):
    depth=np.full((SIZE,SIZE),np.inf,dtype=np.float32)
    xyz=np.zeros((SIZE,SIZE,3),dtype=np.float32)
    for tri in tris:
        if np.dot(np.cross(tri[1]-tri[0],tri[2]-tri[0]),cam[0]-tri.mean(0))<=0:continue
        xy,z=project(tri,cam)
        if np.min(z)<=.1:continue
        lo=np.maximum(0,np.floor(xy.min(0)-.5).astype(int));hi=np.minimum(SIZE-1,np.ceil(xy.max(0)-.5).astype(int))
        if np.any(hi<lo):continue
        a,b,c=xy;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-8:continue
        yy,xx=np.mgrid[lo[1]:hi[1]+1,lo[0]:hi[0]+1];x=xx+.5;y=yy+.5
        wa=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
        wb=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den
        wc=1-wa-wb;inside=(wa>=0)&(wb>=0)&(wc>=0)
        iz=wa/z[0]+wb/z[1]+wc/z[2];d=1/np.maximum(iz,1e-12)
        use=inside&(d<depth[yy,xx])
        px=(wa[...,None]*tri[0]/z[0]+wb[...,None]*tri[1]/z[1]+wc[...,None]*tri[2]/z[2])/iz[...,None]
        depth[yy[use],xx[use]]=d[use];xyz[yy[use],xx[use]]=px[use]
    return depth,xyz

def identities(path):
    rows=[]
    for nal in re.split(b'\x00\x00(?:\x00)?\x01',path.read_bytes()):
        for row in product(nal):
            body=json.loads(row.pop('body'));row['simTimeMs']=body['simTimeMs'];rows.append(row)
    return rows

def main():
    ap=argparse.ArgumentParser();ap.add_argument('case');ap.add_argument('--stride',type=int,default=1);a=ap.parse_args()
    src=ROOT/'logs/p5'/a.case;req=json.loads((src/'request.json').read_text(encoding='utf-8-sig'))
    if req['scene']!='aim120' or req['view'] not in ('parallax','oblique'):raise RuntimeError('unsupported reprojection fixture')
    video=src/'received.h264';rows=identities(video)
    with (ROOT/'DataDrivenTestQT/1.txt').open() as f: sourceRows=list(csv.reader(f))[1:]
    v,tris=mesh(ROOT/'HwaSim_IR/Bin/Config/TargetLib/models/aim120/AIM120.obj')
    output=ROOT/'logs/p17/diagnostics/model_reprojection'/a.case;output.mkdir(parents=True,exist_ok=True)
    stats=[];prev=None;ious=[];all_res=[];edge_res=[];motion=0;repeat=0
    for ordinal,(frame,row) in enumerate(zip(decode(video,SIZE,SIZE),rows),1):
        if ordinal<61:continue # explicit encoder/startup exclusion
        if ordinal%a.stride:continue
        source=sourceRows[int(row['sourceSeq'])-1 if req.get('advanceInputTime') else 0]
        hpr=[-float(source[5]),float(source[6]),float(source[7])]
        cam=camera(v,row['simTimeMs'],req['view'],hpr);depth,xyz=raster(tris,cam)
        mask=np.isfinite(depth);actual=frame>4;interior=binary_erosion(mask,iterations=2)
        iou=float(np.sum(mask&actual)/max(1,np.sum(mask|actual)));ious.append(iou)
        if prev is not None:
            old,oldcam,olddepth,oldrow=prev
            ys,xs=np.nonzero(mask);xy,z=project(xyz[mask],oldcam)
            coords=np.array([xy[:,1]-.5,xy[:,0]-.5])
            oldz=map_coordinates(olddepth,coords,order=1,mode='constant',cval=np.inf)
            sampled=map_coordinates(old.astype(float),coords,order=1,mode='constant',cval=np.nan)
            valid=np.isfinite(oldz)&np.isfinite(sampled)&(np.abs(oldz-z)<.002)
            residual=np.abs(frame[mask].astype(float)-sampled)
            inner=valid&interior[mask];edge=valid&~interior[mask]
            r=residual[inner];e=residual[edge]
            if len(r):all_res.append(r.astype(np.float32))
            if len(e):edge_res.append(e.astype(np.float32))
            moved=float(np.linalg.norm(cam[0]-oldcam[0]));motion+=moved>1e-9;repeat+=np.array_equal(frame,old)
            stats.append(dict(sourceSeq=row['sourceSeq'],previousSourceSeq=oldrow['sourceSeq'],cameraDisplacementM=moved,interiorSamples=len(r),edgeSamples=len(e),occlusionRejected=int(np.sum(~valid)),residualP95=float(np.percentile(r,95)) if len(r) else None))
        if ordinal in (120,240,360):
            Image.fromarray((mask*255).astype('uint8')).save(output/f'seq{row["sourceSeq"]}_geometry_mask.png')
            Image.fromarray(frame).save(output/f'seq{row["sourceSeq"]}_decoded.png')
        prev=frame,cam,depth,row
        if ordinal%60==0:print(a.case,ordinal,'maskIoU',round(iou,4),flush=True)
    def summary(arr):
        if not arr:return None
        arr=np.concatenate(arr);return dict(samples=int(arr.size),p95=float(np.percentile(arr,95)),p99=float(np.percentile(arr,99)),over2Fraction=float(np.mean(arr>2)))
    report=dict(caseId=a.case,scope='explicit P5 camera-motion diagnostic, decoded domain only',frameIdentitySource='actual H264 SEI v2 + annotation simTimeMs',decodedProducts=len(rows),sampledPairs=len(stats),effectiveMovingPairs=motion,exactRepeatPairs=repeat,geometryMaskIoUMin=min(ious) if ious else 0,interior=summary(all_res),silhouette=summary(edge_res),method='unchanged OBJ perspective raster + source-time P5 camera matrices; 2px interior erosion; 2mm visibility depth agreement; startup frames1..60 excluded',geometryValidated=bool(ious and min(ious)>.95),acceptance='NOT_ORDINARY_FLICKER_ACCEPTANCE',frames=stats)
    (output/'reprojection.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='frames'},indent=2))

if __name__=='__main__':main()
