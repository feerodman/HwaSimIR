"""Metamorphic fixtures exercise the real C++ loader, not a second JSON parser."""
from pathlib import Path
import copy,json,os,subprocess
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p10/profiles';OUT.mkdir(parents=True,exist_ok=True)
os.environ['PATH']=str(ROOT/'HwaSim_IR/HwaSim_IR/opencv2-440/opencv_x64/vc14/bin')+os.pathsep+os.environ['PATH']
exe=ROOT/'logs/p10/bin/p10_profile_check.exe';base=ROOT/'HwaSim_IR/Bin/Config/SensorWave'
def check(name,value,expected,band=2):
    path=OUT/(name+'.json');path.write_text(value if isinstance(value,str) else json.dumps(value),encoding='utf-8')
    result=OUT/(name+'.result.json');p=subprocess.run([str(exe),str(path),str(band),str(result)],capture_output=True,text=True)
    assert p.returncode==expected,(name,p.returncode,p.stdout,p.stderr)
    return dict(case=name,exit=p.returncode,expected=expected,result='PASS')
rows=[]
# P11 production profile contract: protocol 0/1/2 are SWIR/NIR/MWIR and all
# three shipped profiles must load.  VIS-SWIR remains a non-production profile;
# protocol 4 is VIS and must not be used to activate it.
for name,profile_name,band,code in [
    ('SWIR','default_SWIR.json',0,0),
    ('NIR','default_NVG.json',1,0),
    ('MWIR','default_MWIR.json',2,0),
    ('VIS-SWIR-unsupported','default_VIS-SWIR.json',4,2),
]:
    result=OUT/(name+'.result.json');p=subprocess.run([str(exe),str(base/profile_name),str(band),str(result)],capture_output=True,text=True)
    assert p.returncode==code,(name,p.stdout,p.stderr);rows.append(dict(case=name,exit=p.returncode,expected=code,result='PASS'))
for protocol,profile_name in [(-1,'default_MWIR.json'),(3,'default_LWIR.json'),(4,'default_LLLTV.json'),(5,'default_MWIR.json')]:
    name=f'protocol_{protocol}_unsupported';result=OUT/(name+'.result.json')
    p=subprocess.run([str(exe),str(base/profile_name),str(protocol),str(result)],capture_output=True,text=True)
    assert p.returncode==2,(name,p.stdout,p.stderr);rows.append(dict(case=name,exit=p.returncode,expected=2,result='PASS'))
good=json.loads((base/'default_MWIR.json').read_text());s='SensorConfigurationSystem'
def variant(name,edit,code):
    v=copy.deepcopy(good);edit(v);rows.append(check(name,v,code))
variant('explicit_um',lambda v:v['HwaSimIR'].update(SpectralUnit='um'),0)
variant('explicit_nm_rejected',lambda v:v['HwaSimIR'].update(SpectralUnit='nm'),2)
variant('tolerance_inside',lambda v:v['Systems'][s].update(SpectralResponseRangeLow=3.00009),0)
variant('tolerance_outside',lambda v:v['Systems'][s].update(SpectralResponseRangeLow=3.00011),2)
variant('wrong_level_ignored',lambda v:v.update(SpectralResponseRangeLow=99,Width=1),0)
variant('missing_real_low_wrong_level',lambda v:(v['Systems'][s].pop('SpectralResponseRangeLow'),v.update(SpectralResponseRangeLow=3)),2)
variant('missing_width',lambda v:v['Systems'][s].pop('Width'),2)
variant('wrong_type',lambda v:v['Systems'][s].update(Width='800'),2)
variant('profile_width_kept_as_default',lambda v:v['Systems'][s].update(Width=1024),0)
variant('missing_default_preset',lambda v:v['HwaSimIR']['Display'].update(DefaultPreset='absent'),2)
variant('legacy_fxaa_does_not_enable',lambda v:v.update(FXAAEnabled=True,ReflectionsEnabled=True,TemporalAAEnabled=True),0)
for name,raw in [('bad_json','{"x":'),('duplicate','{"a":1,"a":2}'),('trailing','{} garbage'),('nonfinite','{"a":1e999}')]:rows.append(check(name,raw,2))
(OUT/'tests.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(dict(result='PASS',tests=len(rows))))
