"""Offline incremental deployment; existing Config/models remain hard-linked read-only.

HWASIMIR_SSH_PASSWORD is consumed by the existing SSH askpass helper, never logged.
All replacements use a new inode. Previous ELF and complete Config stay available.
"""
import argparse, datetime, hashlib, json, os, pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
FILES = ['GameVFX/sprite.vert', 'GameVFX/sprite.frag',
         'Weather/precipitation.vert', 'IRPlume/engine_plume_profiles.json',
         'SensorWave/default_SWIR.json', 'SensorWave/default_MWIR.json']

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--elf',required=True); ap.add_argument('--out',required=True)
    ap.add_argument('--include-cloud',action='store_true')
    a=ap.parse_args(); out=pathlib.Path(a.out).resolve(); out.mkdir(parents=True,exist_ok=False)
    elf=pathlib.Path(a.elf).resolve(); stamp=datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    env=os.environ.copy(); env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p17')
    if not env.get('HWASIMIR_SSH_PASSWORD'): raise RuntimeError('Set HWASIMIR_SSH_PASSWORD')
    args=['-o','BatchMode=no','-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password','-o','ConnectTimeout=10']
    def ssh(command):
        return subprocess.check_output(['ssh',*args,'root@192.168.1.116',command],env=env,text=True,encoding='utf-8')
    def scp(src,dst): subprocess.run(['scp',*args,str(src),str(dst)],env=env,check=True)
    board='/userdata/HwaSimIR'; staged=f'{board}/Config.p17_new_{stamp}'; old=f'{board}/Config.before_p17_{stamp}'
    oldelf=f'{board}/HwaSim_IR.before_p17_{stamp}'; newelf=f'{board}/HwaSim_IR.p17_new_{stamp}'
    pre=ssh(f"set -eu; cd {board}; test ! -e .deployment_in_progress; ! pgrep -x HwaSim_IR >/dev/null; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); sha256sum HwaSim_IR Config/deployment_manifest.sha256")
    (out/'preflight.txt').write_text(pre,encoding='utf-8')
    scp(f'root@192.168.1.116:{board}/Config/deployment_manifest.sha256',out/'previous_manifest.sha256')
    scp(f'root@192.168.1.116:{board}/Config/deployment_version.env',out/'previous_version.env')
    extra=[]
    if a.include_cloud:
        extra=['Weather/world_cloud_game.json','Weather/world_cloud_demo.json','Weather/Derived/cloud_manifest_p17.json']
        cloud=json.loads((ROOT/'HwaSim_IR/Bin/Config/Weather/Derived/cloud_manifest_p17.json').read_text())
        extra += [t['Path'] for t in cloud['Templates'].values()]
    deployment_files=FILES+extra
    hashes={name:sha(ROOT/'HwaSim_IR/Bin/Config'/name) for name in deployment_files}
    lines=(out/'previous_manifest.sha256').read_text(encoding='utf-8').splitlines(); found=set(); updated=[]
    for line in lines:
        digest,name=line.split('  ',1); normalized=name.removeprefix('./')
        if normalized in hashes: digest=hashes[normalized]; found.add(normalized)
        updated.append(digest+'  '+name)
    if not set(FILES).issubset(found): raise RuntimeError('Required resource missing from prior manifest')
    for name in sorted(set(extra)-found):updated.append(hashes[name]+'  ./'+name)
    manifest=out/'deployment_manifest.sha256'; manifest.write_text('\n'.join(updated)+'\n',encoding='utf-8',newline='\n')
    ssh(f'set -eu; cd {board}; mkdir .deployment_in_progress; cp -al Config {staged}; cp -p HwaSim_IR {oldelf}')
    for name in deployment_files:
        scp(ROOT/'HwaSim_IR/Bin/Config'/name,f'root@192.168.1.116:{staged}/{name}.p17_replacement')
        ssh(f"set -eu; test $(sha256sum '{staged}/{name}.p17_replacement'|cut -d' ' -f1) = {hashes[name]}; mv '{staged}/{name}.p17_replacement' '{staged}/{name}'")
    scp(elf,f'root@192.168.1.116:{newelf}')
    buildid=ssh(f"readelf -n {newelf} | awk '/Build ID:/ {{print $3;exit}}'").strip()
    version=dict(line.split('=',1) for line in (out/'previous_version.env').read_text().splitlines() if '=' in line)
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    version.update(GitCommit=head,WorkingTreeState='dirty',SourceIdentity=head+'-P17-dirty',ElfSha256=sha(elf),BuildId=buildid,ConfigManifestSha256=sha(manifest),CreatedUtc=datetime.datetime.now(datetime.timezone.utc).isoformat())
    vpath=out/'deployment_version.env'; vpath.write_text(''.join(k+'='+v+'\n' for k,v in version.items()),encoding='utf-8',newline='\n')
    for p in [manifest,vpath]:
        scp(p,f'root@192.168.1.116:{staged}/{p.name}.p17_replacement')
        ssh(f'mv {staged}/{p.name}.p17_replacement {staged}/{p.name}')
    command=f"""set -eu
cd {board}
! pgrep -x HwaSim_IR >/dev/null
test $(sha256sum {newelf}|cut -d' ' -f1) = {sha(elf)}
(cd {staged} && sha256sum -c deployment_manifest.sha256 >/dev/null)
mv Config {old}
if mv {staged} Config && mv {newelf} HwaSim_IR; then chmod 755 HwaSim_IR; else
  test ! -e Config || mv Config Config.failed_p17_{stamp}
  mv {old} Config; cp -p {oldelf} HwaSim_IR; exit 31
fi
(cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null)
sha256sum HwaSim_IR Config/deployment_manifest.sha256
rmdir .deployment_in_progress
"""
    (out/'switch.sh').write_text(command,encoding='utf-8',newline='\n')
    post=ssh(command); (out/'postflight.txt').write_text(post,encoding='utf-8')
    receipt=dict(schema='HwaSimIR.P17.Deployment.1',result='PASS',elfSha256=sha(elf),buildId=buildid,configManifestSha256=sha(manifest),resources=hashes,backupElf=oldelf,backupConfig=old)
    (out/'deployment_receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8'); print(json.dumps(receipt))

if __name__=='__main__': main()
