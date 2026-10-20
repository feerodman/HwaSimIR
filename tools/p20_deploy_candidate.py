"""隔离部署 P20；共享只读资源硬链接，配置修改必须原子替换，保留 P19。"""
import argparse,base64,hashlib,json,os,shlex,subprocess,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--build',required=True);ap.add_argument('--direct-rgb-readback',action='store_true');a=ap.parse_args()
if not re.fullmatch('[a-zA-Z0-9_]+',a.build):raise SystemExit('非法候选名称')
build=ROOT/'logs/p20/build'/a.build
receipt=json.loads((build/'build_receipt.json').read_text(encoding='utf-8-sig'))
out=ROOT/'logs/p20/deployment'/a.build;out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def ssh(cmd):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',cmd],env=env,text=True,encoding='utf-8')
ssh('! pgrep -x HwaSim_IR >/dev/null')
target='/userdata/HwaSimIR/candidates/P20_'+a.build
ssh('mkdir -p /userdata/HwaSimIR/candidates && mkdir '+shlex.quote(target))
subprocess.run(['scp',*opts,str(build/'HwaSim_IR.aarch64'),'root@192.168.1.116:'+target+'/HwaSim_IR'],env=env,check=True)
script=r'''
import pathlib,hashlib,os,json,shutil,configparser,re
root=pathlib.Path(TARGET)
assert root.parent==pathlib.Path('/userdata/HwaSimIR/candidates') and root.resolve()==root
expected=EXPECTED
assert hashlib.sha256((root/'HwaSim_IR').read_bytes()).hexdigest()==expected
os.chmod(root/'HwaSim_IR',0o755)
result={}
for role in ('coarse','precise'):
 src=pathlib.Path('/userdata/HwaSimIR/releases/P19')/role
 dest=root/role;dest.mkdir()
 # 资源只读共享；不共享运行日志、网络配置和版本文件。
 shutil.copytree(src/'Config',dest/'Config',copy_function=os.link,symlinks=True)
 shutil.copytree(src/'zrdds',dest/'zrdds',copy_function=os.link,symlinks=True)
 for name in ('network.ini','run_precise.sh','rk3588_hwasimir_performance_mode.sh','zrddslicence.lic'):
  shutil.copy2(src/name,dest/name)
 (dest/'logs').mkdir();os.link(root/'HwaSim_IR',dest/'HwaSim_IR')
 version=dest/'Config/deployment_version.env'
 values=dict(line.split('=',1) for line in version.read_text().splitlines() if '=' in line)
 values.update(ElfSha256=expected,SourceIdentity='P20-candidate-'+BUILD,BuildId=BUILDID,GitCommit=GIT)
 temp=version.with_name('deployment_version.env.next')
 temp.write_text(''.join(k+'='+v+'\n' for k,v in values.items()));os.replace(temp,version)
 runtime=dest/'Config/HwaSimIRRuntime.ini'
 # 仅修改候选自己的云模式与协议QoS；原子替换打断资源硬链接，P19不变。
 text=runtime.read_text()
 def set_key(section,key,value):
  global text
  pattern=r'(?ms)(^\['+re.escape(section)+r'\]\s*\n)(.*?)(?=^\[|\Z)'
  match=re.search(pattern,text)
  if not match:text+='\n['+section+']\n'+key+'='+value+'\n';return
  body=match.group(2)
  if re.search(r'(?m)^'+re.escape(key)+r'\s*=',body):body=re.sub(r'(?m)^'+re.escape(key)+r'\s*=.*$',key+'='+value,body)
  else:body+=key+'='+value+'\n'
  text=text[:match.start(2)]+body+text[match.end(2):]
 set_key('Stage7Weather','CloudAlgorithm','PuffHybrid')
 set_key('Performance','RenderStateCacheMaintenanceFrames','60')
 if DIRECT_RGB:set_key('RenderBackend','DirectRgbReadback','true')
 temp=runtime.with_name(runtime.name+'.next');temp.write_text(text);os.replace(temp,runtime)
 qos=dest/'Config/DDS/ZRDDS_PROTOCOL_QOS.xml'
 temp=qos.with_name(qos.name+'.next');temp.write_text(QOSTEXT);os.replace(temp,qos)
 # 派生配置完成后重建清单，不能继承P19的旧摘要冒充候选有效配置。
 manifest=dest/'Config/deployment_manifest.sha256'
 files=sorted(p for p in (dest/'Config').rglob('*') if p.is_file() and p.name not in ('deployment_manifest.sha256','deployment_version.env'))
 temp=manifest.with_name(manifest.name+'.next')
 temp.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(dest/'Config'))+'\n' for p in files));os.replace(temp,manifest)
 values.update(RuntimeConfigSha256=hashlib.sha256(runtime.read_bytes()).hexdigest(),ConfigManifestSha256=hashlib.sha256(manifest.read_bytes()).hexdigest())
 temp=version.with_name(version.name+'.next');temp.write_text(''.join(k+'='+v+'\n' for k,v in values.items()));os.replace(temp,version)
 cfg=configparser.ConfigParser(interpolation=None,strict=False);cfg.read(str(runtime))
 control=cfg.getint('DdsProtocol','DomainId');video=cfg.getint('DdsVideo','DomainId');customer=cfg.getint('CustomerDdsVideo','DomainId')
 result[role]=dict(root=str(dest),metadata=values,domains=dict(control=control,oldVideo=video,customerVideo=video if customer==-1 else customer),cloudAlgorithm=cfg.get('Stage7Weather','CloudAlgorithm'),directRgbReadback=cfg.getboolean('RenderBackend','DirectRgbReadback',fallback=False),runtimeSha256=hashlib.sha256(runtime.read_bytes()).hexdigest(),qosSha256=hashlib.sha256(qos.read_bytes()).hexdigest())
print(json.dumps(result))
'''
for k,v in dict(TARGET=target,EXPECTED=receipt['elf_sha256'],BUILD=a.build,BUILDID=receipt.get('build_id',''),GIT=receipt.get('git_head',''),DIRECT_RGB=a.direct_rgb_readback,QOSTEXT=(ROOT/'HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml').read_text(encoding='utf-8-sig')).items():
    script=re.sub(r"\b"+k+r"\b",lambda match:repr(v),script)
(out/'deploy_remote.py').write_text(script,encoding='utf-8')
encoded=base64.b64encode(script.encode()).decode()
instances=json.loads(ssh('python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")))
result=dict(build=receipt,instances=instances,currentReleaseChanged=False,sharedReadOnlyAssets='hardlinks_to_P19',visualAcceptance='PENDING')
(out/'receipt.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
