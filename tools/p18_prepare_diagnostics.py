"""Isolated core/halo/opacity shader instruments; never replace production resources."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];env=os.environ.copy()
env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('credential environment required')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def ssh(c):return subprocess.check_output(['ssh','-o','ConnectTimeout=10','root@192.168.1.116',c],env=env,text=True)
ap=argparse.ArgumentParser();ap.add_argument('--base-deployment',default='candidate4');ap.add_argument('--suffix',default='');args=ap.parse_args()
baseDeployment=ROOT/'logs/p18/deployment'/args.base_deployment
base=json.loads((baseDeployment/'receipt.json').read_text())['instances']['precise']['root']
original=(ROOT/'HwaSim_IR/Bin/Config/GameVFX/sprite.frag').read_text()
for mode in ['core','halo','mask']:
    name='diagnostic_'+mode+args.suffix;out=ROOT/'logs/p18/deployment'/name
    out.mkdir(exist_ok=False);target='/userdata/HwaSimIR/p18/'+name+'/precise'
    extra='    if(u_plume_layer!='+('1' if mode=='core' else '2')+')discard;\n' if mode!='mask' else ''
    shader=original.replace('void main() {','// P18 DIAGNOSTIC ONLY: '+mode+'; not production or radiometric evidence.\nvoid main() {\n'+extra)
    if mode=='mask':shader=shader.replace('fragColor=vec4(vec3(source),alpha);','fragColor=vec4(vec3(alpha),1.0); // opacity visualization only')
    path=out/'sprite.frag';path.write_text(shader,encoding='utf-8',newline='\n')
    ssh(f"set -eu; test ! -e '{target}'; mkdir -p '{target}'; cp -al '{base}/Config' '{target}/Config'; ln '{base}/HwaSim_IR' '{target}/HwaSim_IR'; cp '{base}/network.ini' '{target}/network.ini'; cp '{base}/run_precise.sh' '{target}/run_precise.sh'; cp '{base}/rk3588_hwasimir_performance_mode.sh' '{target}/rk3588_hwasimir_performance_mode.sh'; cp '{base}/zrddslicence.lic' '{target}/zrddslicence.lic'; cp -a '{base}/zrdds' '{target}/zrdds'; mkdir '{target}/logs'")
    manifest=(baseDeployment/'precise/deployment_manifest.sha256').read_text()
    manifest='\n'.join((sha(path)+'  '+line.split('  ',1)[1] if line.split('  ',1)[1].removeprefix('./')=='GameVFX/sprite.frag' else line) for line in manifest.splitlines())+'\n'
    mp=out/'deployment_manifest.sha256';mp.write_text(manifest,encoding='utf-8',newline='\n')
    version=(baseDeployment/'precise/deployment_version.env').read_text()
    version='\n'.join(('ConfigManifestSha256='+sha(mp) if s.startswith('ConfigManifestSha256=') else 'SourceIdentity=P18-diagnostic-'+mode if s.startswith('SourceIdentity=') else s) for s in version.splitlines())+'\n'
    vp=out/'deployment_version.env';vp.write_text(version,encoding='utf-8',newline='\n')
    for local,remote in [(path,'GameVFX/sprite.frag'),(mp,mp.name),(vp,vp.name)]:
        subprocess.run(['scp',str(local),f'root@192.168.1.116:{target}/Config/{remote}.p18'],env=env,check=True)
        ssh(f"mv '{target}/Config/{remote}.p18' '{target}/Config/{remote}'")
    verify=ssh(f"cd '{target}/Config' && sha256sum -c deployment_manifest.sha256 >/dev/null && sha256sum GameVFX/sprite.frag deployment_manifest.sha256")
    (out/'verification.log').write_text(verify)
    (out/'receipt.json').write_text(json.dumps(dict(diagnosticOnly=True,mode=mode,instances={'precise':{'root':target}},shaderSha256=sha(path),configManifestSha256=sha(mp)),indent=2))
    print(name,'prepared',flush=True)
