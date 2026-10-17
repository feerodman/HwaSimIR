"""Exercise production UI configuration/source validation without sending INIT."""
import copy,json,os,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/ui'/('validation_'+sys.argv[1] if len(sys.argv)>1 else 'validation');out.mkdir(parents=True,exist_ok=False)
app=root/'build-p19-stim/release';base=json.loads((root/'ConfigExamples/P19/lab_domain150/dual_sensor.json').read_text(encoding='utf-8'))
source=(root/'DataDrivenTestQT/1.txt').read_text(encoding='utf-8-sig').splitlines();header=next(i for i,l in enumerate(source) if l.startswith('Time'))
valid=out/'renamed_17rows.csv';valid.write_text('\n'.join(source[header:header+18])+'\n',encoding='utf-8')
empty=out/'empty.csv';empty.write_text(source[header]+'\n',encoding='utf-8')
bad=out/'bad.csv';bad.write_text(source[header]+'\n1 2 invalid\n',encoding='utf-8')
base['source']=str(valid)
cases=[]
def case(name,edit,expected):
 data=copy.deepcopy(base);edit(data);path=out/(name+'.json');path.write_text(json.dumps(data));cases.append((name,str(path),expected))
case('valid_short_renamed',lambda j:None,0)
case('empty',lambda j:j.update(source=str(empty)),2)
case('bad_columns',lambda j:j.update(source=str(bad)),2)
case('duplicate_identity',lambda j:j['channels'][1].update(sensorID=1),2)
case('broadcast_255',lambda j:j['channels'][0].update(sensorID=255),2)
case('fractional_identity',lambda j:j['channels'][0].update(sensorID=1.5),2)
case('invalid_pixels',lambda j:j['channels'][0].update(pixelFormat='bgr24'),2)
case('invalid_domain',lambda j:j.update(domain=300),2)
case('split_domains',lambda j:j.update(domain=150,videoDomain=1,customerDomain=2),0)
case('inherit_customer_domain',lambda j:j.update(videoDomain=1,customerDomain=-1),0)
case('invalid_video_domain',lambda j:j.update(videoDomain=233),2)
case('fractional_customer_domain',lambda j:j.update(customerDomain=1.5),2)
case('string_customer_domain',lambda j:j.update(customerDomain='1'),2)
case('invalid_negative_customer_domain',lambda j:j.update(customerDomain=-2),2)
case('topic_conflict',lambda j:(j['channels'][1].update(role='coarse'),j['channels'][1]['init'].update(h264En=0)),2)
case('raw_over_capacity',lambda j:(j['channels'][1].update(pixelFormat='rgb24'),j['channels'][1]['init'].update(h264En=0)),2)
cases.append(('legacy_ini_migration',str(root/'DataDrivenTestQT/NetworkConfig.ini'),0))
results=[];env=os.environ.copy();env['QT_FORCE_STDERR_LOGGING']='1'
for name,path,expected in cases:
 with (out/(name+'.log')).open('wb') as log:
  run=subprocess.run([str(app/'DataDrivenTestQT.exe'),'--channels-json='+path,'--validate-dual'],cwd=app,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=25)
 results.append(dict(case=name,expectedExit=expected,actualExit=run.returncode,status='PASS' if run.returncode==expected else 'FAIL'))
(out/'result.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
raise SystemExit(0 if all(r['status']=='PASS' for r in results) else 2)
