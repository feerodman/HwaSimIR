"""P20标准结束NAL回归：真实typed DDS，不依赖renderer私有Topic。"""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--frames',type=int,default=120)
ap.add_argument('--topic',choices=['HD264','LD264'],default='HD264')
ap.add_argument('--pause-ms',type=int,default=3000)
ap.add_argument('--name',required=True)
ap.add_argument('--rounds',type=int,default=1)
ap.add_argument('--last-au-multiple',action='store_true')
ap.add_argument('--truncate',action='store_true')
a=ap.parse_args()
out=ROOT/'logs/p20/local_tests'/a.name;out.mkdir(parents=True,exist_ok=False)
rxdir=ROOT/'build-p19-customer-receiver/release'
pubdir=ROOT/'build-p20-contract-test/release'
ffdir=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
fixture=out/'fixture.h264'
subprocess.run([str(ffdir/'ffmpeg.exe'),'-v','error','-f','lavfi','-i','testsrc2=size=320x320:rate=60',
 '-frames:v',str(a.frames),'-pix_fmt','yuv420p','-c:v','libx264','-preset','ultrafast','-tune','zerolatency',
 '-profile:v','baseline','-x264-params','repeat-headers=1:bframes=0:keyint=30','-f','h264',str(fixture)],check=True)
data=fixture.read_bytes()
if a.last_au_multiple:
 # Annex-B trailing_zero_8bits are legal padding, not another picture.
 if a.frames!=1:raise SystemExit('exact AU size test uses one real picture')
 data+=bytes((-len(data))%1400)
data=data[:-17] if a.truncate else data+bytes([0,0,0,1,10])
fixture.write_bytes(data*a.rounds)
env=os.environ.copy();env['PATH']=str(rxdir)+os.pathsep+env['PATH']
qos=str(rxdir/'ZRDDS_PROTOCOL_QOS.xml')
commands={
 'receiver':[str(rxdir/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--topic','TX/'+a.topic,'--codec','h264',
 '--pixel-format','rgb24','--width','320','--height','320','--fps','60','--domain','150','--qos',qos,
 '--output',str(out),'--display','0','--timeout-ms','12000' if a.truncate else str(30000+a.pause_ms),'--exit-after-ends',str(a.rounds),'--audit','1','--ffmpeg',str(ffdir/'ffmpeg.exe')],
 'sender':[str(pubdir/'customer_tx_contract_test.exe'),qos,str(fixture),'TX/'+a.topic,str(a.pause_ms)]}
(out/'commands.json').write_text(json.dumps(commands,indent=2))
with (out/'receiver.log').open('w') as log,(out/'sender.log').open('w') as publog:
 rx=subprocess.Popen(commands['receiver'],cwd=rxdir,env=env,stdout=log,stderr=subprocess.STDOUT)
 try:
  sender=subprocess.run(commands['sender'],cwd=pubdir,env=env,stdout=publog,stderr=subprocess.STDOUT,timeout=30+a.pause_ms/1000)
  rc=rx.wait(timeout=35+a.pause_ms/1000)
 finally:
  if rx.poll() is None:rx.terminate();rx.wait()
summary=json.loads((out/'summary.json').read_text())
if a.truncate:
 result=dict(status='PASS' if sender.returncode==0 and rc==2 and summary['unterminatedOrIncomplete'] and not summary['mp4Created'] and summary['completedStandardEnds']==0 else 'FAIL',case='deliberately_truncated_no_standard_end',senderExit=sender.returncode,receiverExit=rc,summary=summary)
 (out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2));raise SystemExit(0 if result['status']=='PASS' else 2)
probe=json.loads(subprocess.check_output([str(ffdir/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0',
 '-show_entries','stream=width,height,nb_read_frames','-of','json',str(out/'output.mp4')]))
(out/'ffprobe.json').write_text(json.dumps(probe,indent=2))
same=hashlib.sha256(fixture.read_bytes()).digest()==hashlib.sha256((out/'received.h264').read_bytes()).digest()
result={'status':'PASS' if sender.returncode==0 and rc==0 and same and summary['decodedFrames']==a.frames*a.rounds and int(probe['streams'][0]['nb_read_frames'])==a.frames*a.rounds else 'FAIL',
 'framesPerRound':a.frames,'rounds':a.rounds,'lastAuBytes':len(data)-5 if a.frames==1 else None,'standardEndCrossesSamples':True,'topic':a.topic,'pauseMs':a.pause_ms,'byteIdentical':same,'senderExit':sender.returncode,'receiverExit':rc,
 'endReceiveToLastDecodedMs':(summary['lastDecodedNs']-summary['endReceivedNs'])/1e6,
 'endReceiveToDrainMs':(summary['drainCompleteNs']-summary['endReceivedNs'])/1e6,
 'endReceiveToMp4Ms':(summary['mp4CompleteNs']-summary['endReceivedNs'])/1e6,'summary':summary}
(out/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
raise SystemExit(0 if result['status']=='PASS' else 2)
