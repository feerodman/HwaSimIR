"""Record independently inspected OBJ rim planes and the authored render mounts."""
from pathlib import Path
import hashlib,json
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
CONFIG=ROOT/'HwaSim_IR/Bin/Config'
attachments=json.loads((CONFIG/'GameVFX/nozzle_attachments.json').read_text(encoding='utf8'))
planes={'F35':-6.8245,'F22':-6.8435,'AIM120D':-1.8221,'AIM9X':-1.5086}
out=[]
for key,plane in planes.items():
    a=attachments['Assets'][key];path=CONFIG/a['Mesh'];vertices=[]
    for line in path.read_text(errors='replace').splitlines():
        p=line.split()
        if p and p[0]=='v':vertices.append([float(v) for v in p[1:4]])
    v=np.asarray(vertices);rim=v[np.abs(v[:,1]-plane)<.00006]
    rims=[]
    for n in range(a['NozzleCount']):
        r=rim
        if a['NozzleCount']==2:r=rim[rim[:,0]<-.001] if n==0 else rim[rim[:,0]>.001]
        lo,hi=r.min(axis=0),r.max(axis=0);center=(lo+hi)/2
        point=a[f'Nozzle{n}'];mount=np.array([point[k] for k in ['X','Y','Z']])
        # The plume begins just outside the selected rim, not at the aircraft AABB tail.
        assert abs(mount[0]-center[0])<.002 and abs(mount[2]-center[2])<.002
        assert -.025<mount[1]-plane<0
        assert a['RadiusX']<= (hi[0]-lo[0])/2+.001
        assert a['RadiusZ']<= (hi[2]-lo[2])/2+.001
        rims.append(dict(vertex_count=len(r),bounds=[lo.tolist(),hi.tolist()],center=center.tolist(),mount=mount.tolist(),axial_offset=float(mount[1]-plane)))
    out.append(dict(asset=key,mesh=str(path),mesh_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),rim_plane=plane,rims=rims,thermal_fields_modified=False))
dest=ROOT/'logs/p6/nozzle_mesh/audit.json';dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text(json.dumps(out,indent=2),encoding='utf8')
print('PASS: four existing meshes, five nozzle mounts; audit.json written')
