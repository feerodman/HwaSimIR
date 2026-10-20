"""独立板端光学夹具；不使用DDS和真实大气标定。"""
from pathlib import Path
import argparse,json,base64,subprocess,os,shlex,re
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--binary',default='logs/p20/p20_board_optics_fixture.aarch64');ap.add_argument('--verify-cloud-source-b',action='store_true');a=ap.parse_args();assert re.fullmatch('[a-zA-Z0-9_]+',a.name)
root=Path(__file__).resolve().parents[1];out=root/'logs/p20'/a.name;out.mkdir(exist_ok=False)
contract=dict(width=800,height=800,verifyCloudSourceB=a.verify_cloud_source_b,alphaAbsTolerance=.002,sourceAbsTolerance=.002,targetPixels=[1,3,7],relations=['front','inside','behind','camera_inside'],backend='RK3588_Mali_GLES_RGBA16F',boardResult='PENDING')
(out/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
puffs=json.loads((root/'logs/p20/puff_templates_24/templates.json').read_text())[0]
relations=[('front',10,-30,0,45),('inside',30,-30,0,45),('behind',50,-30,0,45),('camera_inside',5,-2.5,0,45),('edge_right6',30,-30,6,45),('edge_right10',30,-30,10,45),('edge_left6',30,-30,-6,45),('fov20',30,-30,0,20),('fov80',30,-30,0,80),('two_clouds',50,-30,0,45)]
cases=[dict(name=rel+'_'+str(px)+'px',distance=distance,cameraY=origin,cameraX=x,fov=fov,targetPixels=px,width=800,height=800,componentBytes=4,puffs=puffs+[[p[0]+.3,p[1]+1.5,*p[2:]] for p in puffs] if rel=='two_clouds' else puffs) for rel,distance,origin,x,fov in relations for px in [1,3,7]]
(out/'cases.json').write_text(json.dumps(cases,indent=2),encoding='utf-8')
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
args=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10'];host='root@192.168.1.116';remote='/tmp/hwasimir_p20_'+a.name
subprocess.run(['ssh',*args,host,'! pgrep -x HwaSim_IR >/dev/null && mkdir '+remote],env=env,check=True)
subprocess.run(['scp',*args,str(root/a.binary),host+':'+remote+'/fixture'],env=env,check=True)
script='''import os,pathlib,subprocess
p=pathlib.Path(REMOTE+'/fixture');p.chmod(0o755)
xorg=int(subprocess.check_output(['pgrep','-o','Xorg']))
args=pathlib.Path('/proc/'+str(xorg)+'/cmdline').read_bytes().decode().split('\\0')
env=os.environ.copy();env.update(DISPLAY=':0',XAUTHORITY=args[args.index('-auth')+1],PANDA3D_ROOT='/opt/panda3d-aarch64',PRC_DIR='/opt/panda3d-aarch64/etc',PANDA_PRC_DIR='/opt/panda3d-aarch64/etc',LD_LIBRARY_PATH='/opt/panda3d-aarch64/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu')
r=subprocess.run([str(p)],cwd=p.parent,env=env,timeout=90);raise SystemExit(r.returncode)
'''.replace('REMOTE',repr(remote))
command='python3 -c '+shlex.quote('import base64;exec(base64.b64decode("'+base64.b64encode(script.encode()).decode()+'"))')
with (out/'board.log').open('w',encoding='utf-8') as log:r=subprocess.run(['ssh',*args,host,command],env=env,stdout=log,stderr=subprocess.STDOUT)
print('fixtureExit',r.returncode)
if r.returncode==0:
 for suffix in ('.rgba32f','.depth.rgb8'):
  subprocess.run(['scp',*args,host+':'+remote+'/*'+suffix,str(out)],env=env,check=True)
(out/'execution.json').write_text(json.dumps(dict(exitCode=r.returncode,scope='standalone_optical_fixture_no_DDS_no_radiometric_calibration'),indent=2),encoding='utf-8')
raise SystemExit(r.returncode)
