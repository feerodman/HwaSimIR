"""Prepare isolated P18 instances without replacing the validated P17 installation."""
import argparse,configparser,datetime,hashlib,json,os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--elf',required=True);a=ap.parse_args()
    if not re.fullmatch('[a-z0-9_]+',a.name):raise RuntimeError('unsafe deployment name')
    out=ROOT/'logs/p18/deployment'/a.name;out.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
    if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('missing SSH credential environment')
    opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
    def ssh(command):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',command],env=env,text=True,encoding='utf-8')
    def scp(src,dst):subprocess.run(['scp',*opts,str(src),str(dst)],env=env,check=True)
    base='/userdata/HwaSimIR';remote=f'{base}/p18/{a.name}'
    ssh(f"set -eu; test ! -e '{remote}'; mkdir -p '{remote}'")
    elf=Path(a.elf).resolve();scp(elf,f'root@192.168.1.116:{remote}/HwaSim_IR');ssh(f"chmod 755 '{remote}/HwaSim_IR'")
    for name in ['HwaSimIRRuntime.ini','deployment_manifest.sha256','deployment_version.env']:
        scp(f'root@192.168.1.116:{base}/Config/{name}',out/('base_'+name))
    receipt=dict(schema='HwaSimIR.P18.IsolatedDeployment.1',elfSha256=sha(elf),remote=remote,instances={})
    for role,sensor in [('coarse',1),('precise',2)]:
        local=out/role;local.mkdir();target=f'{remote}/{role}'
        ssh(f"set -eu; mkdir '{target}'; cp -al '{base}/Config' '{target}/Config'; cp '/usr/ZRDDS/ZRDDS-2.4.5/zrddslicence.lic' '{target}/zrddslicence.lic'; mkdir '{target}/zrdds'; cp '/usr/ZRDDS/ZRDDS-2.4.5/zrddslicence.lic' '{target}/zrdds/zrddslicence.lic'; ln -s /usr/ZRDDS/ZRDDS-2.4.5/lib '{target}/zrdds/lib'; ln '{remote}/HwaSim_IR' '{target}/HwaSim_IR'; cp '{base}/run_precise.sh' '{target}/run_precise.sh'; cp '{base}/rk3588_hwasimir_performance_mode.sh' '{target}/rk3588_hwasimir_performance_mode.sh'; mkdir '{target}/logs'")
        runtime=(out/'base_HwaSimIRRuntime.ini').read_text(encoding='utf-8-sig')
        runtime+='\n[CustomerDdsVideo]\nEnable=true\nMode=auto\nPixelFormat=gray8\nQueueMaxFrames=32\nDrainMs=5000\nDiscoverySettleMs=1000\nAuditPrefix=logs/customer\n'
        (local/'HwaSimIRRuntime.ini').write_text(runtime,encoding='utf-8',newline='\n')
        network=f'[Identity]\nchannel={role}\nplatID=1001\nsensorID={sensor}\nacceptSensorBroadcast=true\nallowDynamicRemote=false\n\n[UDP]\nlocalIp=192.168.1.116\nlocalPort={8887+sensor}\nremoteIp=192.168.1.188\nremotePort={9996+sensor}\n\n[TCP]\nserverIp=192.168.1.188\nserverPort={5553+sensor}\n'
        (local/'network.ini').write_text(network,encoding='utf-8',newline='\n');scp(local/'network.ini',f'root@192.168.1.116:{target}/network.ini')
        resources={'HwaSimIRRuntime.ini':local/'HwaSimIRRuntime.ini'}
        for f in ['DDS/ZRDDS_PROTOCOL_QOS.xml','GameVFX/sprite.vert','GameVFX/sprite.frag']:resources[f]=ROOT/'HwaSim_IR/Bin/Config'/f
        hashes={name:sha(p) for name,p in resources.items()}
        for name,p in resources.items():
            scp(p,f'root@192.168.1.116:{target}/Config/{name}.p18')
            ssh(f"set -eu; test $(sha256sum '{target}/Config/{name}.p18'|cut -d' ' -f1) = {hashes[name]}; mv '{target}/Config/{name}.p18' '{target}/Config/{name}'")
        lines=[];found=set()
        for line in (out/'base_deployment_manifest.sha256').read_text().splitlines():
            digest,name=line.split('  ',1);key=name.removeprefix('./')
            if key in hashes:digest=hashes[key];found.add(key)
            lines.append(digest+'  '+name)
        for name in set(hashes)-found:lines.append(hashes[name]+'  ./'+name)
        manifest=local/'deployment_manifest.sha256';manifest.write_text('\n'.join(lines)+'\n',encoding='utf-8',newline='\n')
        version=dict(line.split('=',1) for line in (out/'base_deployment_version.env').read_text().splitlines() if '=' in line)
        version.update(ElfSha256=sha(elf),ConfigManifestSha256=sha(manifest),SourceIdentity='P18-'+a.name,WorkingTreeState='dirty',
            BuildId=ssh(f"readelf -n '{target}/HwaSim_IR' | awk '/Build ID:/ {{print $3;exit}}'").strip())
        vp=local/'deployment_version.env';vp.write_text(''.join(k+'='+v+'\n' for k,v in version.items()),encoding='utf-8',newline='\n')
        for p in [manifest,vp]:
            scp(p,f'root@192.168.1.116:{target}/Config/{p.name}.p18');ssh(f"mv '{target}/Config/{p.name}.p18' '{target}/Config/{p.name}'")
        verify=ssh(f"set -eu; cd '{target}'; test $(sha256sum HwaSim_IR|cut -d' ' -f1) = {sha(elf)}; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); sha256sum HwaSim_IR Config/deployment_manifest.sha256 network.ini; stat -c '%i %n' zrddslicence.lic")
        (local/'verification.log').write_text(verify,encoding='utf-8')
        receipt['instances'][role]=dict(root=target,platID=1001,sensorID=sensor,resources=hashes,configManifestSha256=sha(manifest),networkSha256=sha(local/'network.ini'))
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8');print(json.dumps(receipt))
if __name__=='__main__':main()
