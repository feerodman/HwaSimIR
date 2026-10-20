"""隔离编译/运行RGB读回夹具，不改部署程序或SDK。"""
from pathlib import Path
import argparse,os,json,subprocess,shlex,base64,re
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);a=ap.parse_args();assert re.fullmatch('[A-Za-z0-9_]+',a.name)
out=root/'logs/p20'/a.name;out.mkdir(exist_ok=False)
(out/'contract.json').write_text(json.dumps(dict(size=[800,800],format='RGB8',comparison='all_bytes_and_image_modified_sequence',framesPerMode=120,scope='独立同步读回夹具，不代表主程序性能'),ensure_ascii=False,indent=2),encoding='utf-8')
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20');opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def remote(host,script):
 encoded=base64.b64encode(script.encode()).decode();result=subprocess.run(['ssh',*opts,host,'python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")],env=env,text=True,encoding='utf-8',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 if result.returncode:print(result.stdout,flush=True);raise RuntimeError('remote failed: '+str(result.returncode))
 return result.stdout
vm='linaro@192.168.203.128';board='root@192.168.1.116';remoteDir='/tmp/p20_'+a.name
env['HWASIMIR_SSH_PASSWORD']=os.environ['P11_VM_PASSWORD'];remote(vm,'from pathlib import Path\nPath('+repr(remoteDir)+').mkdir()')
files=[root/'tools/p20_rgb_readback_fixture.cpp',root/'tools/p20_board_optics_fixture.cpp']+[root/'HwaSim_IR/HwaSim_IR/IR'/n for n in ['IRNativeRgbCopy.h','IRCloudPuffGeometry.h','IRCloudPuffTemplates.h','IRDepthPrecision.h']]
for p in files:subprocess.run(['scp',*opts,str(p),vm+':'+remoteDir+'/'+p.name],env=env,check=True)
source=json.loads((root/'logs/p20/build/aarch64_35_prefix_gather/source_build_plan.json').read_text(encoding='utf-8-sig'))['remote_source_root']
script=r'''import pathlib,subprocess,re,shlex
p=pathlib.Path(SOURCE);rules=(p/'cmake-build-release/CMakeFiles/rules.ninja').read_text();compiler=re.search(r'command = (\S+) \$DEFINES',rules).group(1)
includes=shlex.split(re.search(r'^  INCLUDES = (.*)$',(p/'cmake-build-release/build.ninja').read_text(),re.M).group(1))
subprocess.run([compiler,'-O2','-std=c++14','-I'+DIR]+includes+[DIR+'/p20_rgb_readback_fixture.cpp','-L/opt/panda3d-aarch64/lib','-Wl,-rpath,/opt/panda3d-aarch64/lib','-lp3framework','-lpanda','-lpandaexpress','-lp3dtoolconfig','-lp3dtool','-lpthread','-ldl','-o',DIR+'/fixture'],cwd=p/'cmake-build-release',check=True)
'''.replace('SOURCE',repr(source)).replace('DIR',repr(remoteDir))
(out/'compile.log').write_text(remote(vm,script),encoding='utf-8');binary=out/'fixture.aarch64';subprocess.run(['scp',*opts,vm+':'+remoteDir+'/fixture',str(binary)],env=env,check=True)
env['HWASIMIR_SSH_PASSWORD']=os.environ['HWASIMIR_SSH_PASSWORD'];remote(board,"import subprocess,pathlib\nassert subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==1\npathlib.Path("+repr(remoteDir)+").mkdir()")
subprocess.run(['scp',*opts,str(binary),board+':'+remoteDir+'/fixture'],env=env,check=True)
script='''import pathlib,subprocess,os
p=pathlib.Path(REMOTE_DIRECTORY+'/fixture');p.chmod(0o755);xorg=int(subprocess.check_output(['pgrep','-o','Xorg']));args=pathlib.Path('/proc/'+str(xorg)+'/cmdline').read_bytes().decode().split('\\0')
env=os.environ.copy();env.update(DISPLAY=':0',XAUTHORITY=args[args.index('-auth')+1],PANDA3D_ROOT='/opt/panda3d-aarch64',PRC_DIR='/opt/panda3d-aarch64/etc',PANDA_PRC_DIR='/opt/panda3d-aarch64/etc',LD_LIBRARY_PATH='/opt/panda3d-aarch64/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu')
subprocess.run([str(p)],cwd=p.parent,env=env,check=True,timeout=120)
'''.replace('REMOTE_DIRECTORY',repr(remoteDir))
(out/'board.log').write_text(remote(board,script),encoding='utf-8');print(out)
