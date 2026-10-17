"""Per-stream measured rates, render stages, and same-Windows-clock decode endpoint."""
import argparse,bisect,csv,json,re,statistics
from pathlib import Path
root=Path(__file__).resolve().parents[1];ap=argparse.ArgumentParser();ap.add_argument('name');args=ap.parse_args()
p=root/'logs/p19/runs'/args.name;r=json.loads((p/'result.json').read_text());report={'case':args.name,'functionalResult':r['status'],'screenPresentation':'NOT_VERIFIED','targetFps':60,'targetLatencyMs':80,'streams':{}}
def csvs(d,pat):return [x for f in sorted(d.glob(pat)) for x in csv.DictReader(f.open(encoding='utf-8-sig'))]
def lines(f):return [json.loads(x) for x in f.read_text().splitlines()]
def stats(x):
 x=sorted(x);return dict(mean=statistics.mean(x),p50=statistics.median(x),p95=x[int(.95*(len(x)-1))],maximum=x[-1]) if x else None
def rate(x):return (len(x)-1)*1e9/(x[-1]-x[0]) if len(x)>1 and x[-1]>x[0] else None
roles=[x for x in ('coarse','precise') if x in r];stim=csvs(p,'input_sender*.csv')
for index,role in enumerate(roles):
 d=p/role;render=csvs(d/'board','stage_render*.csv');execute=csvs(d/'board','input_execute*.csv');rx=lines(d/'receive.jsonl');tx=[x for f in sorted((d/'board').glob('customer_round*.jsonl')) for x in lines(f)]
 frames=[x for x in rx if x['kind']=='frame'];samples=[x for x in rx if x['kind']=='sample'];warm=600 if len(render)>1200 else 0
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
 report['streams'][role]=s
report['sustainedBacklog']=any(s['maximumPendingInputFrames']>60 for s in report['streams'].values())
report['performanceStatus']='FAIL_SUSTAINED_BACKLOG' if report['sustainedBacklog'] else 'RECORDED'
(p/'performance.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
