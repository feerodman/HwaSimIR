"""Deterministic cloud light-column cache; preserve P7 density and originals.

R=density, G/B=up/down integrated density divided by two local units,
A=prior legacy appearance. No measured optical coefficients are introduced.
"""
import hashlib,json,pathlib
import numpy as np
ROOT=pathlib.Path(__file__).resolve().parents[1]/'HwaSim_IR/Bin/Config'
def sha(b):return hashlib.sha256(b).hexdigest()
def fnv(b):
    h=1469598103934665603
    for v in b:h=((h^v)*1099511628211)&0xffffffffffffffff
    return f'{h:016x}'
def main():
    source=ROOT/'Weather/Derived/cloud_manifest_p7.json'
    manifest=json.loads(source.read_text()); size=manifest['Size']
    for t in manifest['Templates'].values():
        old=(ROOT/t['Path']).read_bytes(); assert sha(old)==t['SHA256']
        a=np.frombuffer(old,np.uint8).reshape(size,size,size,4)
        density=a[...,0].astype(np.float64)/255
        upward=(np.cumsum(density[::-1],axis=0)[::-1]-.5*density)/size
        downward=(np.cumsum(density,axis=0)-.5*density)/size
        out=np.empty_like(a);out[...,0]=a[...,0];out[...,3]=a[...,1]
        out[...,1]=np.rint(np.clip(upward,0,1)*255).astype(np.uint8)
        out[...,2]=np.rint(np.clip(downward,0,1)*255).astype(np.uint8)
        b=out.tobytes();t['ParentPath']=t['Path'];t['ParentSHA256']=t['SHA256']
        t['Path']=t['Path'].replace('_p7.rgba','_p17.rgba');t['SHA256']=sha(b);t['FNV64']=fnv(b)
        (ROOT/t['Path']).write_bytes(b)
    manifest.update(BuildVersion='P17-density-columns-1',LightMeaning=__doc__.strip(),ParentManifestSHA256=sha(source.read_bytes()))
    (ROOT/'Weather/Derived/cloud_manifest_p17.json').write_text(json.dumps(manifest,indent=2)+'\n')
    for name in ['world_cloud_demo.json','world_cloud_game.json']:
        p=ROOT/'Weather'/name; d=json.loads(p.read_text());d['Revision']='P17-density-columns-1'
        d['Appearance']['Manifest']='Weather/Derived/cloud_manifest_p17.json'
        d['Appearance']['ArtLightEncoding']='DensityColumns'
        p.write_text(json.dumps(d,indent=2)+'\n')
    print('4 cloud caches: density preserved, physical column integration added, original caches unchanged')
if __name__=='__main__':main()
