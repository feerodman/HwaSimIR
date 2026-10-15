"""Reproject sparse pre-encode/float sequences with captured ordinary model/camera matrices.

Only a single visible original model is accepted. Reject silhouette/annotation
neighbours and visibility changes. No optical-flow or zero-frame dilution.
"""
import argparse,json,pathlib,re
import numpy as np
from scipy.ndimage import binary_erosion,map_coordinates
from PIL import Image
import p17_reproject_model as geometry
from p17_collect_evidence import read_pfm
ROOT=pathlib.Path(__file__).resolve().parents[1]
MODELS={17:'f35/F35C.obj',18:'f22/f22.obj',34:'aim120/AIM120.obj',51:'aim9x/aim9x.obj'}
def transform(points,m):return points@m[:3,:3]+m[3,:3]
def state(path):
    d=json.loads(path.read_text());assert len(d['models'])==1,'single visible-model diagnostic required'
    m=d['models'][0];world=np.array(m['toWorld']).reshape(4,4)
    camera=np.array(d['cameraToWorld']).reshape(4,4)
    local=np.array(m['toCamera']).reshape(4,4) if 'toCamera' in m else world@np.linalg.inv(camera)
    return d,m,local
def percentile(values):
    if not values:return None
    a=np.concatenate(values);return dict(samples=int(a.size),p95=float(np.percentile(a,95)),p99=float(np.percentile(a,99)),mean=float(a.mean()),over2Fraction=float(np.mean(a>2)),over8Fraction=float(np.mean(a>8)))
def main():
    ap=argparse.ArgumentParser();ap.add_argument('case',type=pathlib.Path);a=ap.parse_args();src=a.case.resolve()
    files=sorted((src/'diagnostic_linear').glob('*_geometry.json'),key=lambda p:int(re.search(r'_seq(\d+)',p.name)[1]))
    assert files
    cache={};prev=None;pairs=[];gray_values=[];raw_values=[];edge_values=[];repeats=0
    output=src/'reprojection';output.mkdir(exist_ok=True)
    for path in files:
        d,m,local=state(path);seq=d['sourceSeq'];kind=m['type']
        if kind not in cache:cache[kind]=geometry.mesh(ROOT/'HwaSim_IR/Bin/Config/TargetLib/models'/MODELS[kind])[1]
        tris=transform(cache[kind],local)
        geometry.TAN=np.tan(np.deg2rad(d['fovDeg'][0]*.5))
        cam=(np.zeros(3),np.array([[1,0,0],[0,0,1],[0,1,0]]))
        depth,xyz=geometry.raster(tris,cam);mask=np.isfinite(depth)
        # Store points in original model-local coordinates for moving-object reprojection.
        xyz=transform(xyz,np.linalg.inv(local))
        base=path.with_name(path.name.replace('_geometry.json',''))
        rgb=np.asarray(Image.open(str(base)+'_rgb8.png').convert('RGB'))
        gray=np.sum(rgb.astype(float)*[.299,.587,.114],axis=2)
        raw=read_pfm(path.with_name(path.name.replace('_geometry.json','.pfm')))[...,0]
        neutral=binary_erosion(np.ptp(rgb.astype(int),axis=2)<=2,iterations=3)
        interior=binary_erosion(mask,iterations=3)&neutral
        Image.fromarray((mask*255).astype('uint8')).save(output/f'seq{seq}_geometry_mask.png')
        # Disjoint capture windows must not become fictitious adjacent frames.
        if prev is not None and seq == prev[0] + 1:
            oldseq,oldlocal,olddepth,oldgray,oldraw,oldneutral=prev
            q=transform(xyz[mask],oldlocal);xy,z=geometry.project(q,cam)
            coords=np.array([xy[:,1]-.5,xy[:,0]-.5])
            oldz=map_coordinates(olddepth,coords,order=1,mode='constant',cval=np.inf)
            sampled=map_coordinates(oldgray,coords,order=1,mode='constant',cval=np.nan)
            sampled_raw=map_coordinates(oldraw,coords,order=1,mode='constant',cval=np.nan)
            neutral_old=map_coordinates(oldneutral.astype(float),coords,order=0,mode='constant',cval=0)>.5
            valid=np.isfinite(oldz)&np.isfinite(sampled)&(np.abs(oldz-z)<.02)&neutral_old&neutral[mask]
            inside=valid&interior[mask];edge=valid&~interior[mask]
            r=np.abs(gray[mask]-sampled);raw_r=np.abs(raw[mask]-sampled_raw)
            if inside.any():gray_values.append(r[inside]);raw_values.append(raw_r[inside])
            if edge.any():edge_values.append(r[edge])
            repeated=bool(np.array_equal(gray,oldgray));repeats+=repeated
            pairs.append(dict(sourceSeq=seq,previousSourceSeq=oldseq,validInterior=int(inside.sum()),rejected=int((~valid).sum()),repeated=repeated,matrixChanged=not np.array_equal(local,oldlocal)))
        prev=seq,local,depth,gray,raw,neutral
        print(src.name,seq,'geometryPixels',int(mask.sum()),flush=True)
    report=dict(scope='ordinary original model; submitted matrices; sparse late continuous sequence; no encoder error included',calibration='NOT_VERIFIED_CALIBRATION',method='original OBJ perspective raster; 3px silhouette/annotation exclusion; 20mm visibility depth agreement; model-local reprojection',frames=len(files),pairs=pairs,effectiveMovingPairs=sum(p['matrixChanged'] for p in pairs),repeatFraction=repeats/max(1,len(pairs)),preencodeInterior=percentile(gray_values),preencodeSilhouette=percentile(edge_values),rawInterior=percentile(raw_values),limitations='CPU OBJ fan triangulation can differ from Assimp for malformed polygons. Masks are exported for inspection; these metrics alone do not constitute visual acceptance.')
    (output/'temporal.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k!='pairs'},indent=2))
if __name__=='__main__':main()
