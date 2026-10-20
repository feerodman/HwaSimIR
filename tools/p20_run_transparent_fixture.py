"""构建并运行原生Mali透明前缀夹具；固定容差先写入，保存未增强float读回。"""
from pathlib import Path
import argparse,os,json,subprocess,shlex,base64,re
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--layer',type=int,choices=[1,2],default=1);ap.add_argument('--gather',action='store_true');ap.add_argument('--copies',type=int,default=1);a=ap.parse_args();assert re.fullmatch('[A-Za-z0-9_]+',a.name) and 1<=a.copies<=193
out=root/'logs/p20'/a.name;out.mkdir(exist_ok=False)
contract=dict(width=800,height=800,layer=a.layer,gather=a.gather,copies=a.copies,sourceAbsTolerance=.03,alphaAbsTolerance=.001,cloudOffRgbAbsTolerance=.001,positions=[10,30,50],backend='RK3588_Mali_GLES_RGBA16F',scope='实际P18 shader扩展与生产C++前缀函数；独立夹具不代表主程序集成或视觉验收')
(out/'contract.json').write_text(json.dumps(contract,indent=2,ensure_ascii=False),encoding='utf-8')
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def remote(host,script):
 encoded=base64.b64encode(script.encode()).decode()
 result=subprocess.run(['ssh',*opts,host,'python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+encoded+"'))")],env=env,text=True,encoding='utf-8',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 if result.returncode:print(result.stdout,flush=True);raise RuntimeError('remote fixture operation failed: '+str(result.returncode))
 return result.stdout
vm='linaro@192.168.203.128';board='root@192.168.1.116';vmDir='/tmp/p20_'+a.name;boardDir='/tmp/p20_'+a.name
env['HWASIMIR_SSH_PASSWORD']=os.environ['P11_VM_PASSWORD']
remote(vm,'from pathlib import Path\nPath('+repr(vmDir)+').mkdir()')
for name in ['p20_board_transparent_fixture.cpp','p20_board_optics_fixture.cpp']:
 subprocess.run(['scp',*opts,str(root/'tools'/name),vm+':'+vmDir+'/'+name],env=env,check=True)
subprocess.run(['scp',*opts,str(root/'HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h'),vm+':'+vmDir+'/IRCloudTransparentTransport.h'],env=env,check=True)
subprocess.run(['scp',*opts,str(root/'HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h'),vm+':'+vmDir+'/IRCloudPuffGeometry.h'],env=env,check=True)
plan=json.loads((root/'logs/p20/build/aarch64_25_transparent_mask/source_build_plan.json').read_text(encoding='utf-8-sig'))
source=plan['remote_source_root']
script=r"""import pathlib,subprocess,re,shlex
p=pathlib.Path(SOURCE)
rules=(p/'cmake-build-release/CMakeFiles/rules.ninja').read_text()
compiler=re.search(r'command = (\S+) \$DEFINES',rules).group(1)
includes=shlex.split(re.search(r'^  INCLUDES = (.*)$',(p/'cmake-build-release/build.ninja').read_text(),re.M).group(1))
cmd=[compiler,'-O2','-std=c++14','-I'+DIR]+includes+[DIR+'/p20_board_transparent_fixture.cpp','-L/opt/panda3d-aarch64/lib','-Wl,-rpath,/opt/panda3d-aarch64/lib','-lp3framework','-lpanda','-lpandaexpress','-lp3dtoolconfig','-lp3dtool','-lpthread','-ldl','-o',DIR+'/fixture']
print(cmd,flush=True);subprocess.run(cmd,cwd=p/'cmake-build-release',check=True)
""".replace('SOURCE',repr(source)).replace('DIR',repr(vmDir))
(out/'compile.log').write_text(remote(vm,script),encoding='utf-8')
binary=out/'fixture.aarch64';subprocess.run(['scp',*opts,vm+':'+vmDir+'/fixture',str(binary)],env=env,check=True)
env['HWASIMIR_SSH_PASSWORD']=os.environ['HWASIMIR_SSH_PASSWORD']
remote(board,"import subprocess,pathlib\nassert subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==1\npathlib.Path("+repr(boardDir)+").mkdir()")
subprocess.run(['scp',*opts,str(binary),board+':'+boardDir+'/fixture'],env=env,check=True)
script="""import pathlib,subprocess,os
p=pathlib.Path(REMOTE_DIRECTORY+'/fixture');p.chmod(0o755)
xorg=int(subprocess.check_output(['pgrep','-o','Xorg']));args=pathlib.Path('/proc/'+str(xorg)+'/cmdline').read_bytes().decode().split('\\0')
env=os.environ.copy();env.update(DISPLAY=':0',XAUTHORITY=args[args.index('-auth')+1],PANDA3D_ROOT='/opt/panda3d-aarch64',PRC_DIR='/opt/panda3d-aarch64/etc',PANDA_PRC_DIR='/opt/panda3d-aarch64/etc',LD_LIBRARY_PATH='/opt/panda3d-aarch64/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu')
subprocess.run([str(p),'/userdata/HwaSimIR/candidates/P20_aarch64_24_transparent_prefix/precise/Config',LAYER_VALUE,GATHER_VALUE,COPIES_VALUE],cwd=p.parent,env=env,check=True,timeout=120)
""".replace('REMOTE_DIRECTORY',repr(boardDir)).replace('LAYER_VALUE',repr(str(a.layer))).replace('GATHER_VALUE',repr('gather' if a.gather else 'scalar')).replace('COPIES_VALUE',repr(str(a.copies)))
(out/'board.log').write_text(remote(board,script),encoding='utf-8')
subprocess.run(['scp',*opts,board+':'+boardDir+'/*.rgba32f',str(out)],env=env,check=True)
print(out)
