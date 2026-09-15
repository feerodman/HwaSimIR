"""Build path-only OBJ/MTL derivatives without modifying user assets or geometry."""
import hashlib, json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'HwaSim_IR/Bin/Config/TargetLib'
SOURCES = ['f35/F35C.obj', 'f22/f22.obj', 'aim120/AIM120.obj']

def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64)-1)
    return format(value, '016x')

def prepare():
    records = []
    for name in SOURCES:
        source = ROOT / 'models' / name
        obj = source.read_bytes()
        match = re.search(rb'(?m)^mtllib\s+([^\r\n]+)', obj)
        if not match: raise ValueError('Missing MTL: ' + name)
        material = source.parent / match[1].decode('ascii').strip()
        mtl = material.read_bytes()
        stem = source.name + '.portable.' + fnv(obj) + '-' + fnv(mtl)
        output, mtl_output = source.parent / (stem + '.obj'), source.parent / (stem + '.mtl')
        replacements = []
        def resolve_map(m):
            old = m[2].strip().replace(b'\\', b'/').split(b'/')[-1].decode('ascii')
            candidates = [p for p in source.parent.iterdir() if p.is_file() and p.name.lower() == old.lower()]
            if len(candidates) != 1: raise ValueError('Unresolved/ambiguous texture: ' + old)
            replacements.append({'originalPathHex':m[2].hex(), 'relativePath':candidates[0].name})
            return m[1] + candidates[0].name.encode('ascii')
        portable_mtl = re.sub(rb'(?m)^(\s*map_\w+\s+)([^\r\n]+)', resolve_map, mtl)
        portable_obj = obj[:match.start(1)] + mtl_output.name.encode('ascii') + obj[match.end(1):]
        for path, data in [(output,portable_obj),(mtl_output,portable_mtl)]:
            if not path.exists() or path.read_bytes() != data:
                tmp = path.with_name(path.name + '.tmp')
                tmp.write_bytes(data); tmp.replace(path)
        # Every non-mtllib byte must remain identical; this includes all geometry and UVs.
        invariant = re.sub(rb'(?m)^mtllib[^\r\n]*',b'',obj) == re.sub(rb'(?m)^mtllib[^\r\n]*',b'',portable_obj)
        assert invariant
        records.append({'source':str(source.relative_to(ROOT)).replace('\\','/'),
            'sourceSha256':hashlib.sha256(obj).hexdigest(),'materialSha256':hashlib.sha256(mtl).hexdigest(),
            'derived':str(output.relative_to(ROOT)).replace('\\','/'),'derivedSha256':hashlib.sha256(portable_obj).hexdigest(),
            'geometryAndUVBytesUnchanged':invariant,'texturePaths':replacements})
    manifest = ROOT / 'portable_models.json'
    data = json.dumps({'schema':1,'purpose':'path-only portable imports of current user models','models':records},indent=2)+'\n'
    tmp=manifest.with_suffix('.tmp');tmp.write_text(data,encoding='utf-8');tmp.replace(manifest)
    print(json.dumps({'models':len(records),'geometryAndUVBytesUnchanged':True}))

if __name__ == '__main__': prepare()
