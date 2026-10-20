"""矩阵结束后核验候选清单，实测成对启动/退出，导出三域配置及交接回执。

只修改所指定P20候选的派生manifest/version和启动器；不切换release。
禁止在存在renderer时运行。测试仅停止本脚本拥有的启动器进程。
"""
import argparse,base64,json,os,pathlib,shlex,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--build',required=True);a=ap.parse_args()
folder=root/'logs/p20/deployment'/a.build
receipt=json.loads((folder/'receipt.json').read_text(encoding='utf-8'))
candidate=pathlib.PurePosixPath(receipt['instances']['precise']['root']).parent
assert candidate.parent==pathlib.PurePosixPath('/userdata/HwaSimIR/candidates') and candidate.name.startswith('P20_')
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
script=r'''
import pathlib,hashlib,os,json,subprocess,time,signal,configparser
root=pathlib.Path(CANDIDATE)
assert subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==1
assert root.resolve()==root and root.parent==pathlib.Path('/userdata/HwaSimIR/candidates')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def atomic(p,text):
 t=p.with_name(p.name+'.p20-next');t.write_text(text);os.replace(t,p)
records={}
for role in ('coarse','precise'):
 base=root/role;cfg=base/'Config';version=cfg/'deployment_version.env';manifest=cfg/'deployment_manifest.sha256'
 before=dict(line.split('=',1) for line in version.read_text().splitlines() if '=' in line)
 assert sha(base/'HwaSim_IR')==EXPECTED
 files=sorted(p for p in cfg.rglob('*') if p.is_file() and p.name not in ('deployment_manifest.sha256','deployment_version.env'))
 atomic(manifest,''.join(sha(p)+'  '+str(p.relative_to(cfg))+'\n' for p in files))
 after=before.copy();after.update(RuntimeConfigSha256=sha(cfg/'HwaSimIRRuntime.ini'),ConfigManifestSha256=sha(manifest))
 atomic(version,''.join(k+'='+v+'\n' for k,v in after.items()))
 records[role]=dict(previousMetadata=before,metadata=after,elfSha256=sha(base/'HwaSim_IR'),runtime=(cfg/'HwaSimIRRuntime.ini').read_text(),network=(base/'network.ini').read_text(),qos=(cfg/'DDS/ZRDDS_PROTOCOL_QOS.xml').read_text())
starter=root/'p20_start_candidate_pair.py';atomic(starter,STARTER)
log=open('/tmp/p20-pair-startup-smoke.log','w')
p=subprocess.Popen(['python3','-u',str(starter),str(root)],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
try:
 for _ in range(30):
  time.sleep(1)
  if p.poll() is not None:break
  text=pathlib.Path('/tmp/p20-pair-startup-smoke.log').read_text()
  if '两路已启动' in text and _>=12:break
finally:
 if p.poll() is None:p.send_signal(signal.SIGTERM)
 p.wait(timeout=25);log.close()
text=pathlib.Path('/tmp/p20-pair-startup-smoke.log').read_text()
dirs=sorted(pathlib.Path('/tmp').glob('hwasimir-p20-pair-*'),key=lambda p:p.stat().st_mtime)
smoke=dirs[-1];out=json.loads((smoke/'result.json').read_text())
logs={role:(smoke/(role+'.log')).read_text(errors='replace') for role in ('coarse','precise')}
remaining=subprocess.run(['pgrep','-ax','HwaSim_IR'],stdout=subprocess.PIPE,text=True).stdout.strip()
releases={}
for name in ('P19','P18'):
 base=pathlib.Path('/userdata/HwaSimIR/releases')/name
 releases[name]={role:sha(base/role/'HwaSim_IR') for role in ('coarse','precise')}
result=dict(candidate=str(root),instances=records,pairLauncher=out,pairStartupLog=text,rendererLogs=logs,
 remainingRendererProcesses=remaining,releaseHashes=releases,
 current=str(pathlib.Path('/userdata/HwaSimIR/current').resolve()),previous=str(pathlib.Path('/userdata/HwaSimIR/previous').resolve()),
 startupDomainsSeen={role:'[DdsDomains]' in data for role,data in logs.items()},
 scope='两路入口启动与退出烟测；真实收发另见完整矩阵',currentReleaseChanged=False)
print(json.dumps(result))
'''
for key,value in dict(CANDIDATE=str(candidate),EXPECTED=receipt['build']['elf_sha256'],STARTER=(root/'tools/p20_start_candidate_pair.py').read_text(encoding='utf-8')).items():
 script=script.replace(key,repr(value))
(folder/'handoff_remote.py').write_text(script,encoding='utf-8')
encoded=base64.b64encode(script.encode()).decode()
response=subprocess.check_output(['ssh',*opts,'root@192.168.1.116','python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")],env=env,text=True,encoding='utf-8',timeout=240)
result=json.loads(response)
for role,item in result['instances'].items():
 out=root/'logs/p20/configs'/a.build/role;out.mkdir(parents=True,exist_ok=True)
 for key,name in [('runtime','HwaSimIRRuntime.ini'),('network','network.ini'),('qos','ZRDDS_PROTOCOL_QOS.xml')]:
  (out/name).write_text(item.pop(key),encoding='utf-8')
for role,log in result.pop('rendererLogs').items():(folder/(role+'_startup.log')).write_text(log,encoding='utf-8')
(root/'logs/p20/deployment/final_state.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps({k:result[k] for k in ['candidate','remainingRendererProcesses','current','previous','startupDomainsSeen']},ensure_ascii=False))
assert result['pairLauncher']['systemPolicyRestored'] and not result['remainingRendererProcesses']
assert all(result['startupDomainsSeen'].values())
