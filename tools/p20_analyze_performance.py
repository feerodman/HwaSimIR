"""逐路实测帧率、渲染阶段和同一 Windows 时钟下的解码端延迟。"""
import argparse,bisect,csv,json,re,statistics
from pathlib import Path
root=Path(__file__).resolve().parents[1];ap=argparse.ArgumentParser();ap.add_argument('name');args=ap.parse_args()
p=root/'logs/p20/runs'/args.name;r=json.loads((p/'result.json').read_text());report={'case':args.name,'functionalResult':r['status'],'screenPresentation':'NOT_VERIFIED','targetFps':r.get('configuredVideoFps',60),'inputPacketHz':r.get('inputPacketHz',60),'targetLatencyMs':80,'streams':{}}
def csvs(d,pat):return [x for f in sorted(d.glob(pat)) for x in csv.DictReader(f.open(encoding='utf-8-sig'))]
def lines(f):return [json.loads(x) for x in f.read_text().splitlines()]
def stats(x):
 x=sorted(x);return dict(mean=statistics.mean(x),p50=statistics.median(x),p95=x[int(.95*(len(x)-1))],p99=x[int(.99*(len(x)-1))],maximum=x[-1],over80msRatio=sum(v>80 for v in x)/len(x)) if x else None
def rate(x):return (len(x)-1)*1e9/(x[-1]-x[0]) if len(x)>1 and x[-1]>x[0] else None
roles=[x for x in ('coarse','precise') if x in r];stim=csvs(p,'input_sender*.csv')
for index,role in enumerate(roles):
 d=p/role;render=csvs(d/'board','stage_render*.csv');execute=csvs(d/'board','input_execute*.csv');rx=lines(d/'receive.jsonl');tx=[x for f in sorted((d/'board').glob('customer_round*.jsonl')) for x in lines(f)]
 frames=[x for x in rx if x['kind']=='frame'];samples=[x for x in rx if x['kind']=='sample'];warm=0
 s={'renderFrames':len(render),'customerSentFrames':len(tx),'receivedSamples':len(samples),'decodedFrames':len(frames),'savedFrames':r[role].get('savedFramesVerified'),'warmupFramesExcluded':warm,'renderFps':rate([int(x['steadyNs']) for x in render[warm:]]),'decodeFps':rate([x['decodedNs'] for x in frames[warm:]]),'renderStagesMs':{k:stats([float(x[k]) for x in render[warm:]]) for k in ('executeToDrawBeginMs','doFrameMs','captureTaskMs')},'maximumPendingInputFrames':max([int(x['remainingInputDepth']) for x in render],default=0),'inputQueueWaitMs':stats([(int(x['endNs'])-int(x['beginNs']))/1e6 for x in execute[warm:]])}
 log=(d/'board/board.log').read_text(errors='replace');s['stopTotalMs']=[float(x) for x in re.findall(r'stopTotalMs=([\d.]+)',log)]
 ends=[];n=0
 for x in samples:n+=x['bytes'];ends.append(n)
 received=[];n=0
 for x in tx:
  n+=x['bytes'];j=bisect.bisect_left(ends,n)
  if j>=len(ends) or ends[j]!=n:raise RuntimeError('AU/Sample accounting mismatch')
  received.append(samples[j]['receivedNs'])
 s['receiveFrameCompletionFps']=rate(received[warm:])
 if len(frames)==len(tx):s['receiveCompleteToDecodeMs']=stats([(x['decodedNs']-stamp)/1e6 for x,stamp in zip(frames[warm:],received[warm:])])
 own=stim[index::len(roles)]
 if len(own)==len(execute)==len(frames) and [x['digestFNV1a64'] for x in own]==[x['digestFNV1a64'] for x in execute]:
  s['windowsStimulusToDecodedImageMs']=stats([(frame['decodedNs']-int(source['beginNs']))/1e6 for source,frame in zip(own[warm:],frames[warm:])]);s['latencyClock']='same Windows std::chrono::steady_clock; route order and canonical business digests verified; decoded QImage, not screen'
 else:s['windowsStimulusToDecodedImageMs']='NOT_VERIFIED_missing_or_mismatched_stimulus_audit'
 # 输出逐帧帧龄、队列及1秒窗口，不排除冷启动与结束排空。
 stamps=[int(x['steadyNs']) for x in render]
 if stamps:
  origin=stamps[0];windows={}
  for stamp in stamps:windows[int((stamp-origin)/1e9)]=windows.get(int((stamp-origin)/1e9),0)+1
  s['oneSecondWindows']=[dict(second=k,frames=v,partialLast=(k==max(windows))) for k,v in windows.items()]
  s['frameIntervalsMs']=stats([(b-a)/1e6 for a,b in zip(stamps,stamps[1:])])
  s['boardEnqueueToRenderedAgeMs']=stats([(stamp-int(e['beginNs']))/1e6 for stamp,e in zip(stamps,execute)]) if len(execute)==len(stamps) else None
  q=[int(x['remainingInputDepth']) for x in render]
  s['medianPendingLastQuarter']=statistics.median(q[max(0,len(q)*3//4):])
  peak=max(range(len(q)),key=lambda i:q[i]);s['queuePeakFrame']=peak+1
  s['queueGrowthUntilPeakFramesPerSecond']=(q[peak]-q[0])/max((stamps[peak]-stamps[0])/1e9,.000001)
  with (d/'performance_frames.csv').open('w',newline='',encoding='utf-8') as f:
   writer=csv.writer(f);writer.writerow(['frame','elapsedSeconds','remainingInputFrames','enqueueToRenderedAgeMs'])
   for i,(stamp,e) in enumerate(zip(stamps,execute)):writer.writerow([i+1,(stamp-origin)/1e9,q[i],(stamp-int(e['beginNs']))/1e6])
 summary=r[role]
 s['endTimingMs']={k:(summary[k]-summary['endReceivedNs'])/1e6 for k in ('lastParserOutputNs','lastDecodedNs','drainCompleteNs','mp4CompleteNs') if summary.get('endReceivedNs',0)>0 and k in summary}
 if summary.get('lastReceiveNs',0)>0 and summary.get('mp4CompleteNs',0)>0:
  s['lastSampleToMp4CompleteMs']=(summary['mp4CompleteNs']-summary['lastReceiveNs'])/1e6
 accepted=csvs(d/'board','input_accepted*.csv')
 rejected=csvs(d/'board','input_rejected*.csv')
 s['queueConservation']={'stimulusWrites':len(own),'accepted':len(accepted),'executed':len(execute),'completed':len(render),'sent':len(tx),'decoded':len(frames),'saved':summary.get('savedFramesVerified'),'explicitRejected':len(rejected),'pass':len(own)==len(accepted)==len(execute)==len(render)==len(tx)==len(frames)==summary.get('savedFramesVerified') and not rejected}
 report['streams'][role]=s
backlogRate=report['inputPacketHz']*.99
report['sustainedBacklogCriteria']=f'queue peak >8 AND (new-frame FPS <{backlogRate} OR median pending in last quarter >8); diagnostic threshold only, not FPS acceptance; all cold frames retained'
report['sustainedBacklog']=any(s['maximumPendingInputFrames']>8 and ((s['renderFps'] or 0)<backlogRate or s.get('medianPendingLastQuarter',0)>8) for s in report['streams'].values())
for s in report['streams'].values():
 s['strictConfiguredFpsMet']=s['renderFps'] is not None and s['renderFps']>=report['targetFps']
 latency=s.get('windowsStimulusToDecodedImageMs')
 s['p95Latency80msMet']=latency['p95']<=80 if isinstance(latency,dict) else None
report['performanceStatus']='FAIL_SUSTAINED_BACKLOG' if report['sustainedBacklog'] else 'RECORDED'
(p/'performance.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
