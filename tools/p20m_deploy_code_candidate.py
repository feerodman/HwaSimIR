"""Deploy an isolated P20M CODE VALIDATION candidate, retaining P14 data.

This command does not publish a new full-domain physics dataset.
Authentication uses HWASIMIR_SSH_PASSWORD and the existing askpass helper.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build", type=Path, required=True)
parser.add_argument("--name", default="P20_P20M_20261021_code_validation")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not re.fullmatch(r"P20_P20M_[A-Za-z0-9_]+", args.name):
    raise SystemExit("Invalid isolated candidate name")
build = json.loads((args.build/"build_receipt.json").read_text(encoding="utf-8-sig"))
binary = args.build/"HwaSim_IR.aarch64"
if hashlib.sha256(binary.read_bytes()).hexdigest() != build["elf_sha256"]:
    raise SystemExit("Build binary identity mismatch")
args.output.mkdir(parents=True, exist_ok=False)
env = os.environ.copy()
env.update(SSH_ASKPASS=str(ROOT/"tools/p5_ssh_askpass.cmd"),
           SSH_ASKPASS_REQUIRE="force", DISPLAY="p20m")
opts = ["-o", "ConnectTimeout=10", "-o", "UpdateHostKeys=no"]
host = "root@192.168.1.116"
target = "/userdata/HwaSimIR/candidates/"+args.name


def remote(code):
    p = subprocess.run(["ssh", *opts, host, "python3 -"], input=code, text=True,
                       encoding="utf-8", stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       env=env, check=True)
    return p.stdout


preflight = """
import pathlib,shutil,subprocess,json
p=pathlib.Path(TARGET)
assert not p.exists()
assert subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode!=0
assert shutil.disk_usage(p.parent).free > REQUIRED+32*1024*1024
p.mkdir()
print(json.dumps(dict(candidate=str(p),free=shutil.disk_usage(p.parent).free)))
""".replace("TARGET", repr(target)).replace("REQUIRED", str(binary.stat().st_size))
(args.output/"preflight.json").write_text(remote(preflight), encoding="utf-8")
subprocess.run(["scp", *opts, str(binary), host+":"+target+"/HwaSim_IR"], env=env, check=True)
code = r'''
import pathlib,shutil,os,hashlib,json,re,configparser
root=pathlib.Path(TARGET)
base=pathlib.Path('/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(root/'HwaSim_IR')==EXPECTED
os.chmod(root/'HwaSim_IR',0o755)
def ignore(path,names):
 p=pathlib.Path(path)
 if p.name=='MODTRAN':return [n for n in names if n!='processed']
 if p.name=='processed' and p.parent.name=='MODTRAN':
  return [n for n in names if n not in ('band_lut_si.csv','solar_heating_lut_si.csv','p14_coverage_manifest.json','p13_coverage_manifest.json')]
 return []
def atomic(path,text):
 temp=path.with_name(path.name+'.p20m-next')
 temp.write_text(text)
 os.replace(temp,path)
result={}
for role in ('coarse','precise'):
 src=base/role;dst=root/role;dst.mkdir()
 runtimeBefore=sha(src/'Config/HwaSimIRRuntime.ini')
 shutil.copytree(src/'Config',dst/'Config',copy_function=os.link,symlinks=True,ignore=ignore)
 shutil.copytree(src/'zrdds',dst/'zrdds',copy_function=os.link,symlinks=True)
 for name in ('network.ini','run_precise.sh','rk3588_hwasimir_performance_mode.sh','zrddslicence.lic'):
  shutil.copy2(src/name,dst/name)
 (dst/'logs').mkdir();os.link(root/'HwaSim_IR',dst/'HwaSim_IR')
 runtime=dst/'Config/HwaSimIRRuntime.ini'
 text=runtime.read_text()
 text=re.sub(r'(?m)^UtcTimeMode=.*\n?','',text)
 text=text.replace('[M1NirMwirPhysics]','[M1NirMwirPhysics]\nUtcTimeMode=UnixEpochMs')
 atomic(runtime,text)
 data={name:sha(dst/'Config/Atmosphere/MODTRAN/processed'/name)
       for name in ('band_lut_si.csv','solar_heating_lut_si.csv','p14_coverage_manifest.json')}
 for name,value in data.items():assert value==sha(src/'Config/Atmosphere/MODTRAN/processed'/name)
 metadata=dict(status='CODE_VALIDATION_ONLY',fullDomainPhysics='NOT_ACCEPTED',
               dataRelease='UNCHANGED_P14',dataHashes=data,elfSha256=EXPECTED,
               sourceBaseline=str(base),currentReleaseChanged=False)
 atomic(dst/'Config/p20m_candidate.json',json.dumps(metadata,indent=2))
 manifest=dst/'Config/deployment_manifest.sha256'
 files=sorted(p for p in (dst/'Config').rglob('*') if p.is_file() and p.name not in ('deployment_manifest.sha256','deployment_version.env'))
 atomic(manifest,''.join(sha(p)+'  '+str(p.relative_to(dst/'Config'))+'\n' for p in files))
 version=dst/'Config/deployment_version.env'
 values=dict(line.split('=',1) for line in version.read_text().splitlines() if '=' in line)
 values.update(ElfSha256=EXPECTED,SourceIdentity='P20M-code-validation',BuildId=BUILDID,GitCommit=GIT,
               RuntimeConfigSha256=sha(runtime),ConfigManifestSha256=sha(manifest))
 atomic(version,''.join(k+'='+v+'\n' for k,v in values.items()))
 assert sha(src/'Config/HwaSimIRRuntime.ini')==runtimeBefore
 cfg=configparser.ConfigParser(interpolation=None,strict=False);cfg.read(str(runtime))
 video=cfg.getint('DdsVideo','DomainId');customer=cfg.getint('CustomerDdsVideo','DomainId')
 result[role]=dict(root=str(dst),metadata=values,data=data,
  domains=dict(control=cfg.getint('DdsProtocol','DomainId'),oldVideo=video,customerVideo=video if customer==-1 else customer),
  runtimeSha256=sha(runtime),baselineRuntimeUnchanged=True)
print(json.dumps(result))
'''
for key, value in dict(TARGET=target, EXPECTED=build["elf_sha256"], BUILDID=build["build_id"], GIT=build["git_head"]).items():
    code = re.sub(r"\b"+key+r"\b", lambda match: repr(value), code)
(args.output/"deploy_remote.py").write_text(code, encoding="utf-8")
instances = json.loads(remote(code))
receipt = dict(build=build, instances=instances, currentReleaseChanged=False,
               fullDomainPhysics="NOT_ACCEPTED", candidatePurpose="CODE_VALIDATION_ONLY",
               dataRelease="UNCHANGED_P14", actualCustomerSoftware="NOT_RUN",
               calibration="NOT_VERIFIED_CALIBRATION")
(args.output/"receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
print(json.dumps(receipt, indent=2))
