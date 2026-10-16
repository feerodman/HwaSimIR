"""Offline acceptance of identity/order, byte contracts and explicitly scoped timing."""
import argparse,csv,json,statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def csvrows(p):return list(csv.DictReader(p.open(encoding='utf-8-sig')))
def percentile(v,p):return sorted(v)[int((len(v)-1)*p)] if v else None
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory');a=ap.parse_args();base=Path(a.directory)
    sender=csvrows(next(base.glob('input_sender*.csv')));expected=[r['digestFNV1a64'] for r in sender]
    report={'expectedRows':4318,'senderRows':len(sender),'branches':{},'limits':{'fps':60,'latencyMs':80,'warmupRows':600},'endToEndLatency':'NOT_VERIFIED_no_cross_host_clock_calibration'}
    for role in ['coarse','precise']:
        audit=base/role/'sender_audit';accepted=csvrows(next(audit.glob('input_accepted*.csv')));executed=csvrows(next(audit.glob('input_execute*.csv')))
        contract=json.loads((base/'result.json').read_text()).get(role,{})
        result=dict(accepted=len(accepted),executed=len(executed),sourceSeqContinuous=[int(r['sourceSeq']) for r in accepted]==list(range(1,4319)),
            acceptedBusinessDigestMatchesSender=[r['digestFNV1a64'] for r in accepted]==expected,
            executedBusinessDigestMatchesSender=[r['digestFNV1a64'] for r in executed]==expected,
            receiverFrames=contract.get('decodedFrames'),savedFrames=contract.get('savedFramesVerified'))
        timings={}
        for stage in ['render','output']:
            rows=csvrows(next(audit.glob('stage_'+stage+'*.csv')));rows=[r for r in rows if int(r['sourceSeq'])>600]
            span=(int(rows[-1]['steadyNs'])-int(rows[0]['steadyNs']))/1e9;fps=(len(rows)-1)/span
            timings[stage]={'newFrames':len(rows),'spanSeconds':span,'fps':fps,'strict60FpsPass':fps>=60,'atLeast60Seconds':span>=60}
            for name in rows[0]:
                if name.endswith('Ms'):
                    v=[float(r[name]) for r in rows];timings[stage][name]={'p50':statistics.median(v),'p95':percentile(v,.95),'max':max(v)}
        timings['captureTaskCompletion']={k:timings['render'][k] for k in ['newFrames','spanSeconds','fps','strict60FpsPass','atLeast60Seconds']}
        timings['captureTaskCompletion']['timestampScope']='stage_render ledger is recorded after do_frame and capture_task return; same completed business-cycle timestamps, not an independent GPU clock'
        timings['render']['timestampScope']='completed render/capture business cycle; doFrameMs and captureTaskMs report separate durations'
        customer=[json.loads(line) for p in audit.glob('customer_round*.jsonl') for line in p.read_text().splitlines()]
        warm=[r for r in customer if r['sourceSeq']>600]
        if not warm:
            result['timings']=timings;report['branches'][role]=result;continue
        start=warm[0]['writeEndNs'];end=warm[-1]['writeEndNs'];fps=(len(warm)-1)*1e9/(end-start)
        timings['customer']={'apiAcceptedFrames':len(warm),'fps':fps,'spanSeconds':(end-start)/1e9,'strict60FpsPass':fps>=60}
        for name in ['queueWaitMs','convertMs','encodeMs','writeMs']:
            v=[r[name] for r in warm];timings['customer'][name]={'p50':statistics.median(v),'p95':percentile(v,.95),'max':max(v)}
        acceptNs={int(r['sourceSeq']):int(r['beginNs']) for r in accepted}
        latency=[(r['writeEndNs']-acceptNs[r['sourceSeq']])/1e6 for r in warm]
        timings['boardAcceptedToCustomerWriteCompleteMs']={'p95':percentile(latency,.95),'max':max(latency),'isEndToEnd':False}
        result['timings']=timings;report['branches'][role]=result
    report['businessIdentityResult']='PASS' if len(sender)==4318 and all(v['accepted']==v['executed']==4318 and v['sourceSeqContinuous'] and v['acceptedBusinessDigestMatchesSender'] and v['executedBusinessDigestMatchesSender'] for v in report['branches'].values()) else 'FAIL'
    (base/'identity_timing_analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
