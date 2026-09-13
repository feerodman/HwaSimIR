"""Build shareable artistic volumes from existing repository alpha assets.

No commercial installation files are copied. RGBA bytes are prebuilt once so
Windows/aarch64 use identical samples. R=density, G=appearance, B=0, A=255.
"""
from pathlib import Path
import hashlib,json
import numpy as np
from PIL import Image
from scipy.ndimage import map_coordinates, gaussian_filter, zoom

root=Path(__file__).resolve().parents[1]/'HwaSim_IR/Bin/Config'
out=root/'Weather/Derived';out.mkdir(parents=True,exist_ok=True)
size=96
axis=(np.arange(size,dtype=np.float64)+.5)/size*2-1
pz,py,px=np.meshgrid(axis,axis,axis,indexing='ij')
def fnv(b):
    h=1469598103934665603
    for v in b:h=((h^v)*1099511628211)&((1<<64)-1)
    return f'{h:016x}'

for improved in (False,True):
    version='P6B-art-density-1' if improved else 'P6A-depth-lobes-reference'
    manifest=dict(BuildVersion=version,Size=size,Count=4,AxisOrder='Z,Y,X,RGBA',Templates={})
    for index,number in enumerate((1,2,14,20)):
        source=f'Weather/Textures/cloud3d_{number:03d}.png'
        raw=(root/source).read_bytes();im=np.asarray(Image.open(root/source).convert('LA'),dtype=np.float64)/255
        u=.5+px*.57+.055*np.sin(py*4+index)*py
        v=.5+pz*.60+.035*np.cos(py*5+index)*py
        coords=np.array([(1-v)*(im.shape[0]-1),u*(im.shape[1]-1)])
        alpha=map_coordinates(im[:,:,1],coords,order=1,mode='constant',cval=0)
        shade=map_coordinates(im[:,:,0],coords,order=1,mode='constant',cval=0)
        depth=np.maximum(0,1-(py/(.28+.27*alpha))**2)
        boundary=np.clip((.98-px*px-py*py-pz*pz)/.12,0,1)
        if improved:
            rng=np.random.Generator(np.random.PCG64(20260913+index))
            coarse=zoom(rng.uniform(-1,1,(9,9,9)),size/9,order=3)
            structure=np.clip(.90+.12*gaussian_filter(coarse,1),.72,1.08)
        else:
            structure=.78+.22*np.sin(px*11+py*8+index)*np.cos(pz*9-py*7)
        density=np.clip(alpha*depth*boundary*structure*1.7,0,1)
        volume=np.zeros((size,size,size,4),np.uint8)
        volume[:,:,:,0]=np.floor(density*255+.5).astype(np.uint8)
        volume[:,:,:,1]=np.floor(shade*255+.5).astype(np.uint8);volume[:,:,:,3]=255
        data=volume.tobytes();name=f'cloud_{number:03d}'+('' if improved else '_p6a')+'.rgba'
        path=out/name;temp=path.with_suffix('.new');temp.write_bytes(data);temp.replace(path)
        manifest['Templates'][f'T{index}']=dict(Key=f'cloud-alpha-{number:03d}',Path='Weather/Derived/'+name,
            SHA256=hashlib.sha256(data).hexdigest(),FNV64=fnv(data),Source=source,SourceSHA256=hashlib.sha256(raw).hexdigest())
        print(version,number,len(data),manifest['Templates'][f'T{index}']['SHA256'])
    path=out/('cloud_manifest.json' if improved else 'cloud_manifest_p6a.json')
    path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf8')
