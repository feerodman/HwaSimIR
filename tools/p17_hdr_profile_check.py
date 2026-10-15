"""Exercise the production profile parser and fixed HDR numeric boundaries."""
import copy
import json
import os
import subprocess
import numpy as np
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p17/ir_retest/profile'
EXE=OUT/'bin/p10_profile_check.exe'
os.environ['PATH']=str(ROOT/'HwaSim_IR/HwaSim_IR/opencv2-440/opencv_x64/vc14/bin')+os.pathsep+os.environ['PATH']
base=json.loads((ROOT/'HwaSim_IR/Bin/Config/SensorWave/default_MWIR.json').read_text())
results=[]
for name,knee,expected in [('current',273.15,0),('old_omitted',None,0),('equal_white',1800,2),
                            ('above_white',1801,2),('below_range',179,2),('wrong_type','cold',2)]:
    value=copy.deepcopy(base)
    hdr=value['HwaSimIR']['Display']['Presets']['HDR']
    if knee is None: hdr.pop('KneeReferenceTemperatureK')
    else: hdr['KneeReferenceTemperatureK']=knee
    path=OUT/(name+'.json');path.write_text(json.dumps(value))
    result=OUT/(name+'.result.json')
    process=subprocess.run([str(EXE),str(path),'2',str(result)],capture_output=True,text=True)
    assert process.returncode==expected,(name,process.stdout,process.stderr)
    results.append(dict(case=name,exitCode=process.returncode,result='PASS'))
current=json.loads((OUT/'current.result.json').read_text())
field=next(f for f in current['fieldConsumption'] if f['path'].endswith('HDR.KneeReferenceTemperatureK'))
assert field['eligibleForThisProfileRequest']==1 and field['contract']=='public_display'

# Independent SI integration and monotonic/end-point checks precede GPU A/B.
w=np.linspace(3,5,16385)*1e-6
def planck(temp):
    v=1.191042972e-16/w**5/np.expm1(.01438776877/(w*temp))*1e-6
    return float(np.trapezoid(v,w)/(w[-1]-w[0]))
knee,white=planck(273.15),planck(1800)
x=np.concatenate(([0],np.geomspace(1e-9,white,10000)))
y=(np.log1p(x/knee)/np.log1p(white/knee))**(1/2.2)
assert np.isfinite(y).all() and (np.diff(y)>0).all() and y[0]==0 and abs(y[-1]-1)<1e-12
check=dict(parser=results,kneeRadiance=knee,whiteRadiance=white,curve='PASS_FINITE_MONOTONIC_BLACK_WHITE',
    limitation='Analytic display contract; final validation requires actual GPU raw/preencode/decoded recordings')
(OUT/'check.json').write_text(json.dumps(check,indent=2)+'\n')
print(json.dumps(check,indent=2))
