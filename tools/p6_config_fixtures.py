from pathlib import Path
import json,shutil,subprocess,os
root=Path(__file__).resolve().parents[1]
good=root/'HwaSim_IR/Bin/Config/SensorWave';out=root/'logs/p6/config_fixtures';out.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['PATH']=str(root/'HwaSim_IR/Bin')+';'+env.get('PATH','')
exe=root/'logs/p6/p6_config_check.exe'
log=[]
def run(args):
    r=subprocess.run([str(exe),*map(str,args)],env=env,capture_output=True,text=True)
    log.append(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(log[-1])
run([good,out/'invalid.json'])
original=json.loads((good/'default_MWIR.json').read_text(encoding='utf-8-sig'))
for name in ['wrong_type','missing_required','bad_version','wrong_band','spectral_conflict','missing_file','invalid_json']:
    folder=out/name;folder.mkdir(exist_ok=True)
    shutil.copy2(good/'default_NVG.json',folder/'default_NVG.json')
    d=json.loads(json.dumps(original))
    if name=='wrong_type':d['HwaSimIR']['Display']['Presets']['Legacy']['Gamma']=True
    if name=='missing_required':del d['Systems']['SensorConfigurationSystem']['Width']
    if name=='bad_version':d['HwaSimIR']['SchemaVersion']=2
    if name=='wrong_band':d['HwaSimIR']['Band']='NIR'
    if name=='spectral_conflict':d['Systems']['SensorConfigurationSystem']['SpectralResponseRangeHigh']=6
    file=folder/'default_MWIR.json'
    if name=='missing_file':file.unlink(missing_ok=True)
    else:file.write_text('{' if name=='invalid_json' else json.dumps(d),encoding='utf8')
    run([folder,good,name])
(root/'logs/p6/config_check.log').write_text('\n'.join(log),encoding='utf8')
print('PASS: JSON syntax/type/path, geometry, seven invalid-profile/root isolation cases')
