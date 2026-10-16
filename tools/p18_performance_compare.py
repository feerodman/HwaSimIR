"""Strict, explicitly scoped off/on performance; no inferred cross-host latency."""
import argparse,bisect,csv,json,re,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def lines(p):return [json.loads(s) for s in p.read_text().splitlines()]
def stats(v):return dict(p50=statistics.median(v),p95=sorted(v)[int((len(v)-1)*.95)],maximum=max(v)) if v else None
def rate(stamps):
    span=(stamps[-1]-stamps[0])/1e9;fps=(len(stamps)-1)/span
    return dict(newFrames=len(stamps),spanSeconds=span,fps=fps,atLeast60Seconds=span>=60,strict60FpsPass=fps>=60)
def memory(base,role,start,end):
    samples=[];t=0;current='';ticks=0
    for line in (base/'memory_monitor.out.log').read_text().splitlines():
        if line.startswith('SAMPLE '):t=float(line.split()[1])
        elif line.startswith('PID '):current=line.split()[-1].rsplit('/',1)[-1]
        elif '(HwaSim_IR)' in line:
            p=line.split();ticks=int(p[13])+int(p[14])
        elif line.startswith('VmRSS:') and current==role and start<=t<=end:samples.append(dict(timeSeconds=t,rssKiB=int(line.split()[1]),cpuTicks=ticks))
    result=dict(samples=len(samples),maximumRssMiB=max(r['rssKiB'] for r in samples)/1024 if samples else None)
    if samples:result.update(firstRssMiB=samples[0]['rssKiB']/1024,lastRssMiB=samples[-1]['rssKiB']/1024,cpuTicksDelta=samples[-1]['cpuTicks']-samples[0]['cpuTicks'])
    return result
ap=argparse.ArgumentParser();ap.add_argument('--off-run',default='dual_perf_off_v2');ap.add_argument('--on-run',default='dual_perf_on_v2_retry');ap.add_argument('--output',default='performance_comparison.json');args=ap.parse_args()
report=dict(limits={'fps':60,'latencyMs':80},warmupRows=600,originalRows=4318,runs={'off':args.off_run,'on':args.on_run},comparison={},crossHostEndToEnd='NOT_VERIFIED_clock_not_calibrated',screenPresentation='NOT_VERIFIED_no_display_timestamp',actualCustomerInterop='NOT_VERIFIED')
for mode,name in [('off',args.off_run),('on',args.on_run)]:
    base=ROOT/'logs/p18/runs'/name;identity=json.loads((base/'identity_timing_analysis.json').read_text());result={}
    for role in ['coarse','precise']:
        path=base/role;timing=identity['branches'][role]['timings'];result[role]={'boardStages':timing}
        render=list(csv.DictReader(next((path/'sender_audit').glob('stage_render*.csv')).open(encoding='utf-8-sig')))
        start=int(render[600]['steadyNs'])/1e9;end=int(render[-1]['steadyNs'])/1e9
        result[role]['boardMemory']=memory(base,role,start,end)
        text=(base/(role+'_board.out.log')).read_text(errors='replace')
        result[role]['customerDrainLog']=[s for s in text.splitlines() if '[CustomerTXDrain]' in s]
        result[role]['oldDrainLog']=[s for s in text.splitlines() if '[DdsVideoDrain]' in s or '[SyncRoundConservation]' in s]
        if mode=='on':
            stimulus=list(csv.DictReader(next(base.glob('input_sender*.csv')).open(encoding='utf-8-sig')))
            recv=lines(path/'receive.jsonl');samples=[r for r in recv if r['kind']=='sample'];decoded=[r for r in recv if r['kind']=='frame']
            sent=lines(path/'sender_audit/customer_round1.jsonl');ends=[];offset=0
            for s in samples:offset+=s['bytes'];ends.append(offset)
            frameReceive=[];offset=0
            for frame in sent:
                offset+=frame['bytes'];index=bisect.bisect_left(ends,offset)
                if ends[index]!=offset:raise RuntimeError('AU boundary does not align with sample stream')
                frameReceive.append(samples[index]['receivedNs'])
            if len(decoded)!=len(sent):raise RuntimeError('incomplete decode cannot yield a full-frame performance claim')
            result[role]['receiveFrameCompletion']=rate(frameReceive[600:])
            result[role]['decodeSteadyBeforeIdleFlush']=rate([r['decodedNs'] for r in decoded[600:-1]])
            result[role]['decodeIncludingFinalIdleFlush']=rate([r['decodedNs'] for r in decoded[600:]])
            result[role]['sameHostReceiveToDecodeMs']=stats([(r['decodedNs']-stamp)/1e6 for r,stamp in zip(decoded[600:-1],frameReceive[600:-1])])
            result[role]['finalFrameIdleFlushDelayMs']=(decoded[-1]['decodedNs']-frameReceive[-1])/1e6
            # Both input_sender and Receiver use Windows std::steady_clock.
            # This closed-loop interval needs no Linux/Windows wall-clock offset,
            # but its endpoint is decoded QImage, not physical screen presentation.
            if len(stimulus)!=len(decoded):raise RuntimeError('stimulus/decode count mismatch')
            loop=[(r['decodedNs']-int(s['beginNs']))/1e6 for s,r in zip(stimulus,decoded)]
            result[role]['windowsStimulusToDecodedImageMs']={
                'endpoint':'Windows DDS stimulus write begins -> Windows decoded QImage',
                'matching':'verified identical ordered business digests, complete AU/hash stream and all 4318 products',
                'clock':'same Windows std::chrono::steady_clock; no board clock subtraction',
                'isScreenPresentation':False,'steadyAfterWarmupExcludingIdleFlush':stats(loop[600:-1]),
                'allFramesIncludingIdleFlush':stats(loop),'lastFrameMs':loop[-1],
                'allFramesAtMost80Ms':all(0<=v<=80 for v in loop)}
            result[role]['receiverSummary']=json.loads((path/'summary.json').read_text())
            mem=[r for r in lines(base/'windows_memory.jsonl') if r['role']==role and frameReceive[600]<=r['monotonicNs']<=frameReceive[-1]]
            result[role]['windowsMemory']=dict(samples=len(mem),maximumRssMiB=max((r['rssBytes']/1048576 for r in mem),default=None))
    report['comparison'][mode]=result
passes=[]
for mode in report['comparison'].values():
    for r in mode.values():
        passes += [r['boardStages'][s]['strict60FpsPass'] and r['boardStages'][s]['atLeast60Seconds'] for s in ['render','output']]
        if 'receiveFrameCompletion' in r:passes += [r[s]['strict60FpsPass'] and r[s]['atLeast60Seconds'] for s in ['receiveFrameCompletion','decodeSteadyBeforeIdleFlush']]
report['strictFpsResult']='PASS' if all(passes) else 'FAIL'
report['overallPerformance']='NOT_PASSED' # 80ms end-to-end is not calibrated even if local rates pass.
report['copyTimingScope']='Customer frame copy occurs after queuedNs and before workerNs. queueWaitMs includes copy/enqueue/wait and bounds copy cost; no independent copy-only timer is claimed.'
(ROOT/'logs/p18'/args.output).write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({'strictFpsResult':report['strictFpsResult'],'overallPerformance':report['overallPerformance']},indent=2))
