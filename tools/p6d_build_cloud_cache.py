"""Normal-world artistic cloud cache from existing silhouette assets.

Source alpha constructs density. Source RGB is inspected but is not a lighting
measurement and is no longer extruded into the volume. R stores 3D puff density;
G stores a local artificial light coefficient / 1.4, derived only from density.
"""
from pathlib import Path
import hashlib,json,sys
import numpy as np
import scipy,platform
from scipy.ndimage import gaussian_filter, map_coordinates
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]/'HwaSim_IR/Bin/Config'
OUT=ROOT/'Weather/Derived'
def fnv(data):
    h=1469598103934665603
    for v in data:h=((h^v)*1099511628211)&((1<<64)-1)
    return f'{h:016x}'
def atomic(path,data):
    tmp=path.with_name(path.name+'.new');tmp.write_bytes(data);tmp.replace(path)
def main():
    original=json.loads((OUT/'cloud_manifest.json').read_text())
    n=original['Size'];axis=(np.arange(n)+.5)/n
    report=[]
    result=dict(original,BuildVersion='P6D-density-light-1',LightMeaning='G times 1.4 = synthetic local source multiplier; density-weighted mean 1',
                DensityMeaning='R: 150 deterministic 3D puffs selected by source alpha; not a 2D extrusion',
                BuildEnvironment=dict(Python=platform.python_version(),NumPy=np.__version__,SciPy=scipy.__version__),Templates={})
    for key,t in original['Templates'].items():
        data=(ROOT/t['Path']).read_bytes()
        assert hashlib.sha256(data).hexdigest()==t['SHA256']
        rgba=np.frombuffer(data,np.uint8).reshape(n,n,n,4).copy()
        original_density=rgba[...,0].astype(np.float64)/255
        old=.42+.62*np.clip((rgba[...,1]/255-.60)/.35,0,1)+.18*axis[:,None,None]
        old_mean=float(np.sum(old*original_density)/np.sum(original_density))
        # A reproducible cluster of 3D puffs, selected by the supplied silhouette.
        # Source alpha selects lobe centers; it is not extruded into depth sheets.
        assert hashlib.sha256((ROOT/t['Source']).read_bytes()).hexdigest()==t['SourceSHA256']
        alpha=np.asarray(Image.open(ROOT/t['Source']).convert('RGBA'),float)[...,3]/255
        rng=np.random.Generator(np.random.PCG64(601304+int(key[1:])));
        z,y,x=np.meshgrid(axis*2-1,axis*2-1,axis*2-1,indexing='ij')
        density=np.zeros_like(original_density);centers=[]
        for attempt in range(12000):
            cx,cz=rng.uniform(-.75,.75,2);u=.5+cx*.57;v=.5+cz*.60
            mask=float(map_coordinates(alpha,np.array([[(1-v)*(alpha.shape[0]-1)],[u*(alpha.shape[1]-1)]]),order=1,mode='constant')[0])
            if rng.random()>mask or cx*cx+cz*cz>.62:continue
            cy=rng.uniform(-.24,.24);r=rng.uniform(.09,.19)
            rx,ry,rz=r*rng.uniform(.85,1.35,3)
            distance=((x-cx)/rx)**2+((y-cy)/ry)**2+((z-cz)/rz)**2
            edge=np.clip((1-distance)/.55,0,1)
            puff=edge*edge*(3-2*edge)*rng.uniform(.75,1.)
            density=1-(1-density)*(1-puff)
            centers.append([cx,cy,cz,rx,ry,rz])
            if len(centers)>=150:break
        assert len(centers)==150
        density*=np.clip((.95-x*x-y*y-z*z)/.10,0,1)
        rgba[...,0]=np.floor(np.clip(density*1.7,0,1)*255+.5).astype(np.uint8)
        density=rgba[...,0]/255
        smooth=gaussian_filter(density,.8)
        # A fixed artistic overhead direction, common to every renderer. This is
        # not multiple scattering, real weather illumination, or physical radiance.
        dz,dy,dx=np.gradient(smooth,2/n)
        normal_length=np.maximum(np.sqrt(dx*dx+dy*dy+dz*dz),.1)
        directional=-(.6*dz+.8*dx)/normal_length
        # Use local 3D lobe orientation, not accumulated vertical columns. The
        # latter trial made a new broad lower-cloud shadow and was rejected.
        # This is source art contrast, never a floor/clamp on displayed pixels.
        # Normalize this new artificial lighting model to the Weather nominal
        # source (unit mean). PNG background gray has no defined light units.
        light=1.0+.20*(directional-np.sum(directional*density)/np.sum(density))
        assert light.min()>=0 and light.max()<1.4
        rgba[...,1]=np.floor(light/1.4*255+.5).astype(np.uint8)
        packed=rgba.tobytes();path=OUT/(Path(t['Path']).stem+'_p6d.rgba');atomic(path,packed)
        result['Templates'][key]=dict(t,Path='Weather/Derived/'+path.name,SHA256=hashlib.sha256(packed).hexdigest(),FNV64=fnv(packed))
        report.append(dict(template=t['Key'],density_sha256=hashlib.sha256(rgba[...,0].tobytes()).hexdigest(),density_identical=False,puffs=centers,seed=601304+int(key[1:]),
                           previous_density_weighted_art_mean=old_mean,new_density_weighted_art_mean=float(np.sum(rgba[...,1]/255*1.4*density)/np.sum(density)),
                           source_rgb_used_for_light=False,coefficient_range=[float(light.min()),float(light.max())]))
    atomic(OUT/'cloud_manifest_p6d.json',(json.dumps(result,indent=2)+'\n').encode())
    dest=Path('logs/p6d/source');dest.mkdir(parents=True,exist_ok=True)
    (dest/'build_evidence.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
