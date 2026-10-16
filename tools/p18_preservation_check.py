"""Compare protected inputs/models/weather/display/old IDL to captured P17 hashes."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
baseline=json.loads((ROOT/'logs/p18/baseline/identity.json').read_text(encoding='utf-8'))
allowed={'HwaSim_IR/Bin/Config/GameVFX/sprite.vert','HwaSim_IR/Bin/Config/GameVFX/sprite.frag','HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml','HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini',
 'HwaSim_IR/Bin/HwaSim_IR.exe','build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe'}
changes=[];missing=[];unchanged=0
for name,old in baseline['files'].items():
    p=ROOT/name
    if not p.is_file():missing.append(name);continue
    h=hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
    if h.hexdigest()!=old['sha256']:changes.append(dict(path=name,before=old['sha256'],after=h.hexdigest(),allowed=name in allowed))
    else:unchanged+=1
result=dict(head=baseline['head'],unchangedFiles=unchanged,changed=changes,missing=missing,result='PASS' if not missing and all(c['allowed'] for c in changes) else 'FAIL')
(ROOT/'logs/p18/protected_preservation.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
