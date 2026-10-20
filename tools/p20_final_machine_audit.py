"""交付前只读复核：没有遗留P20测试进程，实际策略与恢复回执相同。"""
from pathlib import Path
import json,os,subprocess,base64,shlex,datetime
root=Path(__file__).resolve().parents[1];folder=root/'logs/p20/deployment';handoff=json.loads((folder/'final_state.json').read_text(encoding='utf-8'))
script=r'''
import pathlib,json,re,base64,os
expected=EXPECTED
policy={k:pathlib.Path(k).read_text().strip() for k in expected}
left=[]
for p in pathlib.Path('/proc').iterdir():
 if not p.name.isdigit() or int(p.name)==os.getpid():continue
 try:
  comm=(p/'comm').read_text().strip();args=(p/'cmdline').read_bytes().decode(errors='replace').replace('\0',' ')
 except OSError:continue
 owned=comm=='HwaSim_IR' or '/tmp/p20_' in args or 'p20_start_candidate_pair.py' in args
 if comm.startswith('python'):
  match=re.search(r"b64decode\('([A-Za-z0-9+/=]+)'\)",args)
  if match:
   decoded=base64.b64decode(match.group(1)).decode(errors='replace')
   owned=owned or ('range(900)' in decoded and 'thermal_zone' in decoded and 'time.monotonic_ns' in decoded)
 if owned:left.append(dict(pid=int(p.name),name=comm))
print(json.dumps(dict(remainingTestProcesses=left,actualPolicy=policy,policyMatchesRestoreReceipt=policy==expected,freeUserdataBytes=os.statvfs('/userdata').f_bavail*os.statvfs('/userdata').f_frsize)))
'''.replace('EXPECTED',repr(handoff['pairLauncher']['actualPolicy']))
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
encoded=base64.b64encode(script.encode()).decode()
data=subprocess.check_output(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116','python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")],env=env,text=True,encoding='utf-8')
r=json.loads(data);r['checkedAt']=datetime.datetime.now().astimezone().isoformat();r['result']='PASS' if not r['remainingTestProcesses'] and r['policyMatchesRestoreReceipt'] else 'FAIL'
(folder/'final_machine_audit.json').write_text(json.dumps(r,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(r,ensure_ascii=False))
raise SystemExit(0 if r['result']=='PASS' else 2)
