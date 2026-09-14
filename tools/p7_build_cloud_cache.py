"""Offline asymmetric game-cloud density from existing alpha assets.

No source RGB lighting extrusion, world descriptor changes, or runtime noise.
Candidate outputs remain outside production Config until visually accepted.
"""
from pathlib import Path
import argparse,hashlib,json,platform
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter,distance_transform_edt,map_coordinates,label,binary_dilation
from p6d_build_cloud_cache import fnv,atomic

root=Path(__file__).resolve().parents[1]
config=root/'HwaSim_IR/Bin/Config'

def build(output):
    if output.exists() and any(output.iterdir()):raise SystemExit('Keep previous candidate evidence; use a new output directory.')
    output.mkdir(parents=True,exist_ok=True)
    source=json.loads((config/'Weather/Derived/cloud_manifest_p6d.json').read_text())
    n=source['Size'];axis=(np.arange(n)+.5)/n*2-1
    z,y,x=np.meshgrid(axis,axis,axis,indexing='ij')
    evidence=[];manifest=dict(source,BuildVersion='P7-asymmetric-alpha-puffs-2',Templates={},
        DensityMeaning='R: asymmetric multiscale 3D puffs selected by source alpha, wholly inside the existing sphere proxy',
        BuildEnvironment=dict(Python=platform.python_version(),NumPy=np.__version__))
    for key,template in source['Templates'].items():
        image=Image.open(config/template['Source']).convert('RGBA')
        assert hashlib.sha256((config/template['Source']).read_bytes()).hexdigest()==template['SourceSHA256']
        alpha=np.array(image.getchannel('A').resize((n,n),Image.Resampling.BOX),float)/255
        alpha=np.flipud(gaussian_filter(alpha,1.1))
        edgeDistance=distance_transform_edt(alpha>.12)*2/n
        rng=np.random.Generator(np.random.PCG64(7070914+int(key[1:])))
        density=np.zeros((n,n,n),np.float64);centers=[]
        phase=rng.uniform(-1,1)
        for attempt in range(40000):
            cx,cz=rng.uniform(-.85,.85,2)
            ix,iz=np.clip(np.floor((np.array([cx,cz])+.999)*n/2).astype(int),0,n-1)
            # Broad, unequal lobes avoid the former circular sampling footprint.
            macro=(.95*np.exp(-((cx+.28)/.50)**2-((cz+.23)/.39)**2)
                  +.90*np.exp(-((cx-.28)/.40)**2-((cz+.08)/.30)**2)
                  +.75*np.exp(-((cx+.02)/.30)**2-((cz-.39)/.40)**2))
            if rng.random()>min(1,macro)*alpha[iz,ix]**1.2:continue
            cy=.16*np.sin(2.4*cx+1.2*cz+phase)+rng.uniform(-.13,.13)
            cx=1.08*cx+.12*cz;cz=.84*cz+.09*np.sin(2.8*cx)
            room=.975-np.linalg.norm([cx,cy,cz])
            if room<.065:continue
            radius=min(rng.uniform(.09,.24),max(.07,edgeDistance[iz,ix]*.8))
            rx,rz=radius*rng.uniform(.8,1.25,2);ry=rng.uniform(.19,.34)
            rx,ry,rz=np.minimum([rx,ry,rz],room)
            ry=min(ry,.535-abs(cy))
            # Restrict computation to each compact support; no 2D alpha slab is
            # multiplied through the finished density field.
            lo=np.maximum(0,np.floor((np.array([cz-rz,cy-ry,cx-rx])+1)*n/2).astype(int))
            hi=np.minimum(n,np.ceil((np.array([cz+rz,cy+ry,cx+rx])+1)*n/2).astype(int))
            region=tuple(slice(a,b) for a,b in zip(lo,hi))
            d=((x[region]-cx)/rx)**2+((y[region]-cy)/ry)**2+((z[region]-cz)/rz)**2
            edge=np.clip((1-d)/.65,0,1)
            amplitude=rng.uniform(.65,1.)*min(1.,(min(rx,rz)/.16)**.8)
            puff=edge*edge*(3-2*edge)*amplitude
            density[region]=1-(1-density[region])*(1-puff)
            centers.append([float(v) for v in [cx,cy,cz,rx,ry,rz]])
            if len(centers)==140:break
        assert len(centers)==140
        # Smooth 3D coordinate displacement bends the elementary lobe surfaces;
        # source shadows are never used. Low-frequency structure survives the
        # existing eight-step-per-volume quality budget.
        coordinates=np.indices(density.shape,dtype=float)
        for dim,amplitude in enumerate([3.4,1.6,4.0]):
            warp=gaussian_filter(rng.standard_normal(density.shape),n/10)
            coordinates[dim]+=warp/max(warp.std(),1.e-8)*amplitude
        density=map_coordinates(density,coordinates,order=1,mode='constant',cval=0.)
        notch=np.exp(-((x-.28)/.28)**2-((y+.04)/.35)**2-((z-.18)/.26)**2)
        density*=1-.8*notch
        components,count=label(density>.025);sizes=np.bincount(components.ravel())
        keep=sizes>=max(350,int(sizes[1:].max()*.008));keep[0]=False
        retained=gaussian_filter(binary_dilation(keep[components],iterations=2).astype(float),.8)
        density*=retained
        density*=np.clip((.975-np.sqrt(x*x+y*y+z*z))/.06,0,1)*np.clip((.55-np.abs(y))/.025,0,1)
        rgba=np.zeros((n,n,n,4),np.uint8);rgba[...,0]=np.floor(np.clip(density*1.7,0,1)*255+.5).astype(np.uint8)
        density=rgba[...,0].astype(float)/255
        assert not np.any(rgba[[0,-1],:,:,0]) and not np.any(rgba[:,[0,-1],:,0]) and not np.any(rgba[:,:,[0,-1],0])
        dz,dy,dx=np.gradient(gaussian_filter(density,.8),2/n)
        direction=-(.6*dz+.8*dx)/np.maximum(np.sqrt(dx*dx+dy*dy+dz*dz),.1)
        light=1+.20*(direction-np.sum(direction*density)/density.sum())
        assert light.min()>0 and light.max()<1.4
        rgba[...,1]=np.floor(light/1.4*255+.5).astype(np.uint8);rgba[...,3]=255
        packed=rgba.tobytes();filename=Path(template['Path']).stem.replace('_p6d','_p7')+'.rgba'
        atomic(output/filename,packed)
        manifest['Templates'][key]=dict(template,Path='Weather/Derived/'+filename,
            SHA256=hashlib.sha256(packed).hexdigest(),FNV64=fnv(packed))
        evidence.append(dict(template=key,seed=7070914+int(key[1:]),puffs=centers,
            sourceRgbUsedForLight=False,proxyBoundaryDensityZero=True,
            centerOfMass=[float((density*v).sum()/density.sum()) for v in [x,y,z]],
            occupiedFraction=float(np.mean(density>0)),
            densityWeightedLightMean=float((rgba[...,1]/255*1.4*density).sum()/density.sum())))
        for name,dim in [('front',1),('side',2),('top',0)]:
            projection=1-np.prod(1-density*.08,axis=dim)
            Image.fromarray(np.floor(projection*255+.5).astype(np.uint8)).save(output/f'{key}_{name}_density_projection.png')
    atomic(output/'cloud_manifest_p7.json',(json.dumps(manifest,indent=2)+'\n').encode())
    atomic(output/'build_evidence.json',(json.dumps(evidence,indent=2)+'\n').encode())
    print(json.dumps(dict(output=str(output),templates=len(evidence),result='BUILT_PENDING_VISUAL_ACCEPTANCE')))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,default=root/'logs/p7/C_cloud_candidate')
    args=parser.parse_args();build(args.output)
