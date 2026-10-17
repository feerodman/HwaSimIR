"""Use the actual dual UI export path, then package communication-only profiles."""
import json, os, re, shutil, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'ConfigExamples/P19';out.mkdir(parents=True,exist_ok=True)
app=root/'build-p19-stim/release'
env=os.environ.copy();env['QT_FORCE_STDERR_LOGGING']='1'
for name,domain,videoDomain,customerDomain in [('lab_domain150',150,150,-1),('customer_domain1',1,1,-1),('control150_video1',150,1,1)]:
 target=out/name;target.mkdir(exist_ok=True)
 network=target/'SenderNetworkConfig.ini'
 network.write_text(re.sub(r'(?m)^DomainId=.*$','DomainId='+str(domain),(root/'DataDrivenTestQT/NetworkConfig.ini').read_text(encoding='utf-8-sig')),encoding='utf-8')
 # Use the real UI load/export path to retain all three independent domains.
 data=json.loads((out/'lab_domain150/dual_sensor.json').read_text(encoding='utf-8-sig'))
 data.update(domain=domain,videoDomain=videoDomain,customerDomain=customerDomain)
 inputJson=root/'logs/p19'/('export_input_'+name+'.json');inputJson.write_text(json.dumps(data),encoding='utf-8')
 with (root/'logs/p19'/('export_'+name+'.log')).open('wb') as log:
  subprocess.run([str(app/'DataDrivenTestQT.exe'),'--network-config='+str(network),'--channels-json='+str(inputJson),'--export-dual='+str(target),'--export-only'],cwd=app,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=20,check=True)
 for directory in [target,target/'route1',target/'route2']:
  dest=directory/'Config/DDS';dest.mkdir(parents=True,exist_ok=True)
  shutil.copy2(root/'HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml',dest/'ZRDDS_PROTOCOL_QOS.xml')
 vd=(root/'HwaSim_IR_VideoDisplay/x64/Release/NetworkConfig.ini').read_text(encoding='utf-8-sig')
 (target/'VideoDisplayNetworkConfig.ini').write_text(re.sub(r'(?m)^DomainId=.*$','DomainId='+str(videoDomain),vd),encoding='utf-8')
 for role in ('route1','route2'):
  path=target/role/'HwaSimIRRuntime.ini';text=path.read_text(encoding='utf-8-sig')
  text=text.replace('[TcpOutput]','[TcpOutput]\nH264Profile=baseline\nH264Level=auto\nH264RateControl=backend_default\nH264ReferenceFrames=1\nH264RepeatHeaders=true\nH264Aud=false\nH264StandardEnd=true')
  path.write_text(text,encoding='utf-8')
compat=out/'customer_hw_compat';compat.mkdir(exist_ok=True)
text=(out/'customer_domain1/route2/HwaSimIRRuntime.ini').read_text(encoding='utf-8')
text=text.replace('H264Level=auto','H264Level=3.2').replace('H264RateControl=backend_default','H264RateControl=cbr').replace('H264Aud=false','H264Aud=true')
(compat/'HwaSimIRRuntime.ini').write_text(text,encoding='utf-8')
(compat/'README.md').write_text('候选：800×800@60，Baseline/CAVLC、Level 3.2、B=0、refs=1、GOP30、CBR 4Mbps、重复 SPS/PPS、AUD、标准结束 NAL。须配套 customer_domain1 的精路 network/QoS 和完整冻结资源。未知客户硬件 NOT_VERIFIED；1024×1024@60 不得沿用 3.2。修改完整性受控部署时需重新生成清单，不能绕过 launcher 校验。\n',encoding='utf-8')
print('exported lab_domain150, customer_domain1, customer_hw_compat')
