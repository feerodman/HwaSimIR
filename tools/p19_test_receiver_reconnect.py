"""Actual typed publisher, reader joins late then is closed/reopened mid-stream."""
import json,os,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/tests/receiver_late_reconnect_v2';out.mkdir(parents=True,exist_ok=False)
rxdir=root/'build-p19-customer-receiver/release';pubdir=root/'build-p18-contract-test/release'
ff=root/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
fixture=out/'fixture.h264'
subprocess.run([str(ff/'ffmpeg.exe'),'-v','error','-f','lavfi','-i','testsrc2=size=320x320:rate=60','-frames:v','600','-c:v','libx264','-preset','ultrafast','-tune','zerolatency','-x264-params','bframes=0:keyint=30:repeat-headers=1','-f','h264',str(fixture)],check=True)
with fixture.open('ab') as f:f.write(bytes([0,0,0,1,10]))
env=os.environ.copy();env['PATH']=str(rxdir)+os.pathsep+env['PATH'];qos=str(rxdir/'ZRDDS_PROTOCOL_QOS.xml');files=[];processes=[]
def launch(args,cwd,name):
 f=(out/(name+'.log')).open('w');files.append(f);p=subprocess.Popen(args,cwd=cwd,env=env,stdout=f,stderr=subprocess.STDOUT);processes.append(p);return p
def receiver(name):
 path=out/name;path.mkdir()
 return launch([str(rxdir/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--topic','TX/HD264','--codec','h264','--pixel-format','rgb24','--width','320','--height','320','--fps','60','--domain','150','--qos',qos,'--output',str(path),'--display','1','--audit','1','--timeout-ms','90000','--exit-after-ends','1','--ffmpeg',str(ff/'ffmpeg.exe')],rxdir,name)
result={'status':'FAIL','scope':'real SDK local typed H264, late join and new receiver process; no replay of offline frames'}
try:
 tx=launch([str(pubdir/'customer_tx_contract_test.exe'),qos,str(fixture),'TX/HD264','5000'],pubdir,'publisher')
 time.sleep(6) # Publisher discovery is five seconds: joining after transmission began.
 first=receiver('late');deadline=time.monotonic()+15
 while time.monotonic()<deadline:
  wire=out/'late/received.h264'
  if wire.exists() and wire.stat().st_size>30000:break
  time.sleep(.05)
 else:raise RuntimeError('late receiver did not receive data')
 # Abrupt process loss is intentional; no claim of a complete first recording.
 first.terminate();first.wait(timeout=5);time.sleep(.25)
 second=receiver('reconnected');result['senderExit']=tx.wait(timeout=75);result['receiverExit']=second.wait(timeout=95)
 s=json.loads((out/'reconnected/summary.json').read_text());result['summary']=s
 probe=json.loads(subprocess.check_output([str(ff/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=nb_read_frames','-of','json',str(out/'reconnected/output.mp4')]))
 result['savedFrames']=int(probe['streams'][0]['nb_read_frames'])
 result['status']='PASS' if result['senderExit']==0 and result['receiverExit']==0 and 0<s['decodedFrames']<600 and s['decodedFrames']==result['savedFrames'] and s['completedStandardEnds']==1 and s['decodeErrors']==0 and s['displayUpdates']>0 else 'FAIL'
except Exception as e:result['error']=str(e)
finally:
 for p in processes:
  if p.poll() is None:p.terminate();p.wait(timeout=5)
 for f in files:f.close()
 (out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
raise SystemExit(0 if result['status']=='PASS' else 2)
