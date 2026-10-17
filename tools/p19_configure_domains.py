"""Set three P19 domains while stopped; preserve INI text and audit both backups."""
import argparse, base64, hashlib, json, os, re, shlex, subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--control',type=int,required=True)
ap.add_argument('--video',type=int,required=True)
ap.add_argument('--customer',type=int,required=True)
ap.add_argument('--name',required=True)
a=ap.parse_args()
assert all(0<=x<=232 for x in (a.control,a.video)) and -1<=a.customer<=232
assert re.fullmatch('[A-Za-z0-9_]+',a.name)
out=root/'logs/p19/resumed_domains'/a.name;out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
def ssh(command):
    return subprocess.check_output(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116',command],env=env,text=True,encoding='utf-8')
ssh('! pgrep -x HwaSim_IR >/dev/null')
paths={role:'/userdata/HwaSimIR/releases/P19/'+role for role in ('coarse','precise')}
for role,remote in paths.items():
    (out/(role+'_before.ini')).write_text(ssh('cat '+shlex.quote(remote+'/Config/HwaSimIRRuntime.ini')),encoding='utf-8')
script='''
import pathlib,re,hashlib,json,os
result={}
for role in ('coarse','precise'):
 root=pathlib.Path('/userdata/HwaSimIR/releases/P19')/role
 assert root.resolve()==root
 runtime=root/'Config/HwaSimIRRuntime.ini'
 text=runtime.read_text()
 for section,value in DOMAINS.items():
  pattern=r'(?ms)^\\['+re.escape(section)+r'\\]\\s*\\n(.*?)(?=^\\[|\\Z)'
  match=re.search(pattern,text)
  if match:
   body=match.group(1)
   if re.search(r'(?m)^DomainId\\s*=',body):body=re.sub(r'(?m)^DomainId\\s*=.*$', 'DomainId='+str(value),body)
   else:body='DomainId='+str(value)+'\\n'+body
   text=text[:match.start(1)]+body+text[match.end(1):]
  else:text+='\\n['+section+']\\nDomainId='+str(value)+'\\n'
 temp=runtime.with_name('HwaSimIRRuntime.ini.domains-next');temp.write_text(text);os.replace(temp,runtime)
 version=root/'Config/deployment_version.env'
 values=dict(line.split('=',1) for line in version.read_text().splitlines() if '=' in line)
 values['RuntimeConfigSha256']=hashlib.sha256(runtime.read_bytes()).hexdigest()
 temp=version.with_name('deployment_version.env.domains-next');temp.write_text(''.join(k+'='+v+'\\n' for k,v in values.items()));os.replace(temp,version)
 result[role]={'metadata':values,'sha256':values['RuntimeConfigSha256']}
print(json.dumps(result))
'''.replace('DOMAINS',repr(dict(DdsProtocol=a.control,DdsVideo=a.video,CustomerDdsVideo=a.customer)))
(out/'remote_update.py').write_text(script,encoding='utf-8')
result=json.loads(ssh('python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+base64.b64encode(script.encode()).decode()+"'))")))
receiptPath=root/'logs/p19/deployment/current.json';receipt=json.loads(receiptPath.read_text())
for role,remote in paths.items():
    (out/(role+'_after.ini')).write_text(ssh('cat '+shlex.quote(remote+'/Config/HwaSimIRRuntime.ini')),encoding='utf-8')
    receipt['instances'][role].update(metadata=result[role]['metadata'],domain=a.control,domains=dict(control=a.control,oldVideo=a.video,customerVideo=a.video if a.customer==-1 else a.customer),domainSource='runtime_ini_without_environment_overrides')
receiptPath.write_text(json.dumps(receipt,indent=2))
(out/'receipt.json').write_text(json.dumps(dict(status='PASS',requested=vars(a),instances=result),indent=2))
print(json.dumps(result,indent=2))
