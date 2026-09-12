"""Read-only resource audit. Previews are diagnostics, never replacement assets."""
import csv
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'HwaSim_IR/Bin/Config'
OUT = ROOT / 'logs/p5/assets'
OUT.mkdir(parents=True, exist_ok=True)

def file_info(path):
    return dict(path=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest())

with (CONFIG / 'Materials/MaterialDatabase.csv').open(encoding='utf-8-sig') as f:
    next(f)
    database = {r['Name']: r for r in csv.DictReader(f)}
assets = []
for name, obj, base in [('f22','f22.obj','f22.rgb'), ('aim120','AIM120.obj','aim120.jpg'),
                        ('aim9x','aim9x.obj','TX_AIM9X_Diffuse.png')]:
    folder = CONFIG / 'TargetLib/models' / name
    id_path = next(folder.glob('*_mat.tif'))
    xml_path = Path(str(id_path) + '.xml')
    im = Image.open(id_path)
    ids, counts = np.unique(np.asarray(im), return_counts=True)
    entries = []
    for item in ET.parse(xml_path).getroot():
        surface = item.find('Surface_Substrate')
        if surface is None: surface = item.find('Primary_Substrate')
        mat = surface.find('.//Name').text
        entries.append(dict(id=int(item.attrib['index']), material=mat, database_match=mat in database))
    vertices, normals, uvs, faces = [], [], [], []
    for line in (folder / obj).read_text(errors='replace').splitlines():
        p = line.split()
        if not p: continue
        if p[0] == 'v': vertices.append([float(x) for x in p[1:4]])
        if p[0] == 'vn': normals.append([float(x) for x in p[1:4]])
        if p[0] == 'vt': uvs.append([float(x) for x in p[1:3]])
        if p[0] == 'f': faces.append(p[1:])
    v, n, uv = np.array(vertices), np.array(normals), np.array(uvs)
    known = {r['id'] for r in entries}
    asset = dict(name=name, files=[file_info(folder/obj),file_info(folder/base),file_info(id_path),file_info(xml_path)],
        material_entries=entries, xml_count=len(entries), shader_capacity=8, truncated=len(entries)>8,
        id_mode=im.mode, id_bit_depth=8, id_channel='red/luminance',
        texture_occupancy={str(int(i)):float(c/sum(counts)) for i,c in zip(ids,counts)},
        unknown_fraction=float(sum(c for i,c in zip(ids,counts) if i not in known)/sum(counts)),
        visible_pixel_occupancy='requires GPU capture; not inferred from texture occupancy',
        obj_vertices=len(v), obj_normals=len(n), obj_uvs=len(uv), obj_faces=len(faces),
        bounds_obj=[v.min(axis=0).tolist(),v.max(axis=0).tolist()],
        degenerate_normals=int(np.sum(np.linalg.norm(n,axis=1)<1e-6)) if len(n) else None,
        corners_missing_uv=sum(len(x.split('/'))<2 or not x.split('/')[1] for f in faces for x in f),
        corners_missing_normal=sum(len(x.split('/'))<3 or not x.split('/')[2] for f in faces for x in f))
    assets.append(asset)
    im.save(OUT / (name+'_material_ids.png'))
weather=[]
for key in ['cloud_scattered','cloud_overcast','cloud_cumulus']:
    path=CONFIG / 'Weather/Textures' / (key+'.png')
    im=Image.open(path)
    weather.append(dict(file_info(path), key=key, mode=im.mode,size=im.size,extrema=im.getextrema()))
    im.getchannel('A').save(OUT/(key+'_alpha.png'))
    im.convert('L').save(OUT/(key+'_luminance.png'))
(OUT/'audit.json').write_text(json.dumps(dict(assets=assets,weather=weather),indent=2),encoding='utf-8')
print(json.dumps(dict(assets=[{k:a[k] for k in ['name','xml_count','truncated','unknown_fraction','degenerate_normals','corners_missing_uv']} for a in assets],output=str(OUT)),indent=2))
