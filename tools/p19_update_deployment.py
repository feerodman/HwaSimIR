"""Update only the P19 ELF, preserving P18 and atomically refreshing integrity metadata."""
import argparse, base64, json, os, shlex, subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
ap = argparse.ArgumentParser()
ap.add_argument('--build', required=True)
ap.add_argument('--deployment-name', help='Unique receipt directory when redeploying a previously verified build')
a = ap.parse_args()
build = root / 'logs/p19/build' / a.build
receipt = json.loads((build / 'build_receipt.json').read_text())
import re
deployment_name=a.deployment_name or a.build
if not re.fullmatch(r'[A-Za-z0-9_]+',deployment_name):raise SystemExit('invalid deployment name')
out = root / 'logs/p19/deployment' / deployment_name
out.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'), SSH_ASKPASS_REQUIRE='force', DISPLAY='p19')
opts = ['-o', 'UpdateHostKeys=no', '-o', 'ConnectTimeout=10']
def ssh(command):
    return subprocess.check_output(['ssh', *opts, 'root@192.168.1.116', command], env=env, text=True)
target = '/userdata/HwaSimIR/releases/P19'
ssh('! pgrep -x HwaSim_IR >/dev/null')
subprocess.run(['scp', *opts, str(build/'HwaSim_IR.aarch64'), 'root@192.168.1.116:'+target+'/HwaSim_IR.next'], env=env, check=True)
script = '''
import pathlib, hashlib, os, json, configparser
root=pathlib.Path('/userdata/HwaSimIR/releases/P19')
expected=EXPECTED
assert root.resolve()==root
new=root/'HwaSim_IR.next'
assert hashlib.sha256(new.read_bytes()).hexdigest()==expected
os.chmod(new,0o755)
os.replace(new,root/'HwaSim_IR')
result={}
for role in ('coarse','precise'):
    dest=root/role
    assert dest.resolve()==dest
    temp=dest/'HwaSim_IR.next'
    os.link(root/'HwaSim_IR',temp)
    os.replace(temp,dest/'HwaSim_IR')
    version=dest/'Config/deployment_version.env'
    values=dict(line.split('=',1) for line in version.read_text().splitlines() if '=' in line)
    values['ElfSha256']=expected
    values['SourceIdentity']='P19-'+BUILD
    runtime=dest/'Config/HwaSimIRRuntime.ini'
    values['RuntimeConfigSha256']=hashlib.sha256(runtime.read_bytes()).hexdigest()
    temp=version.with_name('deployment_version.env.next')
    temp.write_text(''.join(k+'='+v+'\\n' for k,v in values.items()))
    os.replace(temp,version)
    config=configparser.ConfigParser(interpolation=None,strict=False)
    config.read(str(runtime))
    protocolDomain=config.getint('DdsProtocol','DomainId',fallback=150)
    videoDomain=config.getint('DdsVideo','DomainId',fallback=150)
    customerDomain=config.getint('CustomerDdsVideo','DomainId',fallback=-1)
    result[role]={'root':str(dest),'metadata':values,'domain':protocolDomain,
        'domains':{'control':protocolDomain,'oldVideo':videoDomain,'customerVideo':videoDomain if customerDomain==-1 else customerDomain},
        'domainSource':'runtime_ini_without_environment_overrides'}
print(json.dumps(result))
'''.replace('EXPECTED', repr(receipt['elf_sha256'])).replace('BUILD', repr(a.build))
(out/'update_remote.py').write_text(script, encoding='utf-8')
encoded=base64.b64encode(script.encode()).decode()
instances=json.loads(ssh('python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")))
result=dict(build=receipt,instances=instances,cleanupPerformed=False)
(out/'receipt.json').write_text(json.dumps(result,indent=2))
(root/'logs/p19/deployment/current.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
