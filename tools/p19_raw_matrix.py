"""Local real SDK size/rate probes. Counts do not establish customer interoperability."""
import argparse,json,os,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--domain',type=int,default=1)
ap.add_argument('--transport',choices=['tcpv4','udpv4'],default='tcpv4');ap.add_argument('--quick',action='store_true');ap.add_argument('--board',action='store_true')
a=ap.parse_args();out=ROOT/'logs/p19/tests'/a.name;out.mkdir(parents=True,exist_ok=False)
rxdir=ROOT/'build-p19-minimal/release';txdir=ROOT/'build-p19-payload-probe/release'
qos=out/'qos.xml';qos.write_text((rxdir/'ZRDDS_PROTOCOL_QOS.xml').read_text().replace('tcpv4://default//0',a.transport+'://default//0'))
env=os.environ.copy();env['PATH']=str(rxdir)+os.pathsep+env['PATH']
if a.board:
 import shlex
 env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
 opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
 remote='/userdata/HwaSimIR/releases/P19/coarse'
 remoteQos=remote+'/logs/'+a.name+'_qos.xml'
 subprocess.run(['ssh',*opts,'root@192.168.1.116','! pgrep -x HwaSim_IR >/dev/null'],env=env,check=True)
 subprocess.run(['scp',*opts,str(qos),'root@192.168.1.116:'+remoteQos],env=env,check=True)
results=[]
sizes=[1399,4096,16384,65500,65536,65540,102400,307200,640000] if not a.quick else [102400,640000]
rates=[1,10,60] if not a.quick else [60]
def last_json(path):
 for line in reversed(path.read_text(errors='replace').splitlines()):
  if line.startswith('{'):
   return json.loads(line)
 return None
for size in sizes:
 for hz in rates:
  case=out/f'size{size}_hz{hz}';case.mkdir()
  count=1 if hz==1 else hz
  rxargs=[str(rxdir/'CustomerRawReceiverMinimal.exe'),'--domain',str(a.domain),'--qos',str(qos),
          '--probe-bytes',str(size),'--seconds','8']
  txargs=[str(txdir/'CustomerTxPayloadProbe.exe'),'--domain',str(a.domain),'--qos',str(qos),
          '--bytes',str(size),'--hz',str(hz),'--frames',str(count),'--discovery-seconds','3']
  if a.board:
   txargs=['ssh',*opts,'root@192.168.1.116','cd '+shlex.quote(remote)+' && chmod +x CustomerTxPayloadProbe && LD_LIBRARY_PATH=zrdds/lib ./CustomerTxPayloadProbe --domain '+str(a.domain)+' --qos '+shlex.quote(remoteQos)+' --bytes '+str(size)+' --hz '+str(hz)+' --frames '+str(count)+' --discovery-seconds 3']
  (case/'commands.json').write_text(json.dumps({'receiver':rxargs,'sender':txargs},indent=2))
  with (case/'receiver.log').open('w') as rl,(case/'sender.log').open('w') as tl:
   rx=subprocess.Popen(rxargs,cwd=rxdir,env=env,stdout=rl,stderr=subprocess.STDOUT)
   try:
    tx=subprocess.run(txargs,cwd=txdir,env=env,stdout=tl,stderr=subprocess.STDOUT,timeout=20)
    rcode=rx.wait(timeout=15)
   finally:
    if rx.poll() is None:rx.terminate();rx.wait()
  r=last_json(case/'receiver.log');t=last_json(case/'sender.log')
  ok=bool(r and t and tx.returncode==0 and rcode==0 and r['receivedSamples']==count and r['consumed']==count and r['bytes']==count*size and r['gaps']==0 and r['duplicates']==0)
  result={'status':'PASS' if ok else 'FAIL','domain':a.domain,'transport':a.transport,'senderHost':'RK3588' if a.board else 'Windows','bytes':size,'hz':hz,
          'senderExit':tx.returncode,'receiverExit':rcode,'sender':t,'receiver':r,'imageClaim':False}
  (case/'result.json').write_text(json.dumps(result,indent=2));results.append(result)
  (out/'matrix.json').write_text(json.dumps(results,indent=2));print(json.dumps(result),flush=True)
