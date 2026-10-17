"""Create isolated P19 instances from the verified P18 resource tree; no cleanup."""
import base64, hashlib, json, os, subprocess, shlex
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p19/deployment/initial';OUT.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def remote(command):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',command],env=env,text=True)
build=ROOT/'logs/p19/build/aarch64_03';receipt=json.loads((build/'build_receipt.json').read_text())
target='/userdata/HwaSimIR/releases/P19'
remote("set -eu; ! pgrep -x HwaSim_IR; test ! -e "+shlex.quote(target)+"; mkdir -p "+shlex.quote(target))
subprocess.run(['scp',*opts,str(build/'HwaSim_IR.aarch64'),'root@192.168.1.116:'+target+'/HwaSim_IR'],env=env,check=True)
script=r'''
import os, pathlib, subprocess, hashlib, configparser, json
root=pathlib.Path('/userdata/HwaSimIR/releases/P19')
def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
    return h.hexdigest()
def atomic(path,text):
    temp=path.with_name(path.name+'.p19tmp')
    with open(temp,'x') as f:f.write(text)
    os.replace(temp,path)
os.chmod(root/'HwaSim_IR',0o755)
results={}
for index,role in enumerate(('coarse','precise')):
    old=pathlib.Path('/userdata/HwaSimIR/p18/candidate4')/role
    dest=root/role;dest.mkdir()
    subprocess.run(['cp','-al',str(old/'Config'),str(dest/'Config')],check=True)
    for name in ('run_precise.sh','rk3588_hwasimir_performance_mode.sh','network.ini','zrddslicence.lic'):
        subprocess.run(['cp','-a',str(old/name),str(dest/name)],check=True)
    subprocess.run(['cp','-a',str(old/'zrdds'),str(dest/'zrdds')],check=True)
    os.link(root/'HwaSim_IR',dest/'HwaSim_IR');(dest/'logs').mkdir()
    cfg=dest/'Config/HwaSimIRRuntime.ini'
    # Preserve every imaging value and comment. Only communication fields change.
    lines=cfg.read_text().splitlines();section='';updated=[]
    values={('DdsProtocol','DomainId'):'1',('DdsVideo','DomainId'):'1',('CustomerDdsVideo','Enable'):'true',('CustomerDdsVideo','Mode'):'auto',('CustomerDdsVideo','PixelFormat'):'gray8'}
    for line in lines:
        if line.startswith('['):section=line.strip()[1:-1]
        if '=' in line and not line.lstrip().startswith((';','#')):
            key=line.split('=',1)[0].strip()
            if (section,key) in values:line=key+'='+values[(section,key)]
        updated.append(line)
    atomic(cfg,'\n'.join(updated)+'\n')
    net=dest/'network.ini';atomic(net,net.read_text().replace('acceptSensorBroadcast=true','acceptSensorBroadcast=false'))
    manifest=dest/'Config/deployment_manifest.sha256'
    files=sorted(p for p in (dest/'Config').rglob('*') if p.is_file() and p.name not in ('deployment_manifest.sha256','deployment_version.env'))
    atomic(manifest,''.join(sha(p)+'  '+str(p.relative_to(dest/'Config'))+'\n' for p in files))
    version=dest/'Config/deployment_version.env'
    metadata={'SchemaVersion':'1','SourceIdentity':'P19-initial','GitCommit':'fded9de2f0ff652aab3afdfee3d350910def5baf','WorkingTreeState':'dirty',
      'ElfSha256':sha(root/'HwaSim_IR'),'ConfigManifestSha256':sha(manifest),'LauncherSha256':sha(dest/'run_precise.sh'),
      'PerformanceToolSha256':sha(dest/'rk3588_hwasimir_performance_mode.sh'),'RuntimeConfigSha256':sha(cfg)}
    atomic(version,''.join(k+'='+v+'\n' for k,v in metadata.items()))
    results[role]={'root':str(dest),'platID':1001,'sensorID':index+1,'domain':1,'metadata':metadata}
print(json.dumps(results))
'''
# Keep the exact remote script in evidence; no passwords or license content.
(OUT/'deploy_remote.py').write_text(script,encoding='utf-8')
encoded=base64.b64encode(script.encode()).decode()
result=remote("python3 -c "+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))"))
instances=json.loads(result)
for item in instances.values():assert item['metadata']['ElfSha256']==receipt['elf_sha256']
(OUT/'receipt.json').write_text(json.dumps(dict(build=receipt,instances=instances,cleanupPerformed=False),indent=2))
print(result)
