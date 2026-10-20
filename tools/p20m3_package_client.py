"""Create a relocatable Windows x64 client bundle, with no renderer/MODTRAN dependency."""
import argparse,configparser,hashlib,json,shutil
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--output',required=True);a=ap.parse_args();out=Path(a.output).resolve();out.mkdir(parents=True,exist_ok=False)
for source,dest,exe in [('build-p20m3-stim/release','DataDrivenTestQT','DataDrivenTestQT.exe'),('build-p20m3-customer-receiver/release','customer-receiver','HwaSimIRCustomerTxVideoReceiverDemo.exe')]:
 src=R/source;dst=out/dest;dst.mkdir()
 for p in src.iterdir():
  if p.suffix.lower()=='.dll' or p.name in (exe,'ffmpeg.exe','zrddslicence.lic'):shutil.copy2(p,dst/p.name)
 for name in ('platforms','styles','imageformats','iconengines','bearer','translations'):
  if (src/name).exists():shutil.copytree(src/name,dst/name)
 (dst/'qt.conf').write_text('[Paths]\nPlugins=.\nTranslations=translations\n',encoding='ascii')
 shutil.copy2(R/'HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml',dst/'ZRDDS_PROTOCOL_QOS.xml')
shutil.copy2(R/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe',out/'customer-receiver/ffprobe.exe')
stim=out/'DataDrivenTestQT';par=stim/'Config/Atmosphere';par.mkdir(parents=True)
for name in ('parameters.txt','provenance.json','quality.json'):shutil.copy2(R/'HwaSim_IR/Bin/Config/Atmosphere/Engineering/P20M3_GREY_GAS_1'/name,par/name)
c=configparser.ConfigParser(interpolation=None);c.optionxform=str;c.read(R/'build-p20m3-stim/release/P20M3_Board_NetworkConfig.ini',encoding='utf-8-sig')
c['DdsProtocol']['QosFile']='ZRDDS_PROTOCOL_QOS.xml';c['UDP']['localIp']='0.0.0.0';c['M1NirMwirPhysics']['EngineeringParameters']='Config/Atmosphere/parameters.txt';c['Demo']['InputFile']='../Data/circleDataAll3.csv'
c['SensorInit'].update(trackerSensorBand='0',saveMP4En='0');c.remove_section('Atmosphere')
with (stim/'NetworkConfig.ini').open('w',encoding='utf-8') as f:c.write(f)
j=json.loads((R/'build-p20m3-stim/release/P20M3_Board_Dual.json').read_text(encoding='utf-8'));j.update(source='../Data/circleDataAll3.csv',qos='ZRDDS_PROTOCOL_QOS.xml',videoDomain=1,customerDomain=1)
for x in j['channels']:x['source']=j['source']
(stim/'P20M3_Board_Dual.json').write_text(json.dumps(j,indent=2),encoding='utf-8')
(out/'Data').mkdir()
for n in ('circleDataAll.csv','circleDataAll2.csv','circleDataAll3.csv'):shutil.copy2(R/'1020测试内容'/n,out/'Data'/n)
(out/'Start-Stim.cmd').write_text('@echo off\r\ncd /d "%~dp0DataDrivenTestQT"\r\nDataDrivenTestQT.exe --network-config=NetworkConfig.ini --channels-json=P20M3_Board_Dual.json\r\n',encoding='ascii')
for role,topic,codec,width,idle,ends in [('Coarse','TX/LDecode','raw',320,4000,0),('Precise','TX/HD264','h264',800,0,1)]:
 (out/('Start-'+role+'.cmd')).write_text('@echo off\r\ncd /d "%~dp0customer-receiver"\r\nset "RX_OUT=..\\Output\\'+role.lower()+'_%RANDOM%_%RANDOM%"\r\nmkdir "%RX_OUT%"\r\nHwaSimIRCustomerTxVideoReceiverDemo.exe --topic '+topic+' --codec '+codec+' --pixel-format gray8 --width '+str(width)+' --height '+str(width)+' --fps 60 --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml --output "%RX_OUT%" --display 1 --audit 1 --timeout-ms 0 --idle-ms '+str(idle)+' --exit-after-ends '+str(ends)+' --ffmpeg ffmpeg.exe\r\n',encoding='ascii')
(out/'Start-Receivers.ps1').write_text("$ErrorActionPreference='Stop'\nforeach ($role in @('Coarse','Precise')) {\n  $script=Join-Path $PSScriptRoot ('Start-'+$role+'.cmd')\n  Start-Process -FilePath $env:ComSpec -ArgumentList ('/c \"\"'+$script+'\"\"') -WorkingDirectory $PSScriptRoot -WindowStyle Normal\n}\n",encoding='utf-8')
(out/'Start-Receivers.cmd').write_text('@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Receivers.ps1"\r\n',encoding='ascii')
(out/'Board-Start.txt').write_text('python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6\n',encoding='ascii')
readme=R/'docs/HwaSimIR_P20M3_Client_Run_Environment.md'
if readme.exists():shutil.copy2(readme,out/'README.md')
files={str(p.relative_to(out)).replace('\\','/'):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file()}
(out/'manifest.json').write_text(json.dumps(dict(schema='P20M3.PortableClient.1',boardCandidate='P20_P20M3_SWIR_MWIR_Engineering_20261021_v6',platform='Windows x64',quality='ENGINEERING_APPROX',files=files),indent=2),encoding='utf-8')
print(out)
