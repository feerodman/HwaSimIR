import csv,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p18/runs/dual_two_rounds_v2'
def csvrows(p):return list(csv.DictReader(p.open(encoding='utf-8-sig')))
def jsonrows(p):return [json.loads(s) for s in p.read_text().splitlines()]
sender=csvrows(next(BASE.glob('input_sender*.csv')));digests=[r['digestFNV1a64'] for r in sender]
report=dict(senderRows=len(sender),twoIdenticalOriginalReplays=digests[:4318]==digests[4318:] and len(digests)==8636,branches={})
for role in ['coarse','precise']:
    audit=BASE/role/'sender_audit';accepted=csvrows(next(audit.glob('input_accepted*.csv')));executed=csvrows(next(audit.glob('input_execute*.csv')))
    r=dict(accepted=len(accepted),executed=len(executed),acceptedDigestsEqualSender=[x['digestFNV1a64'] for x in accepted]==digests,executedDigestsEqualSender=[x['digestFNV1a64'] for x in executed]==digests,rounds={})
    for rnd in [1,2]:
        rows=jsonrows(audit/f'customer_round{rnd}.jsonl');r['rounds'][rnd]=dict(frames=len(rows),allRoundCorrect=all(x['round']==rnd for x in rows),logicalSeqResetAndContinuous=[x['frameSeq'] for x in rows]==list(range(1,4319)),sourceSeqFirst=rows[0]['sourceSeq'],sourceSeqLast=rows[-1]['sourceSeq'])
    log=(BASE/(role+'_board.out.log')).read_text(errors='replace')
    r['runtimeInitCount1']=log.count('[DdsRuntime] initialized=1 initCount=1')==1
    r['drains']=[s for s in log.splitlines() if '[CustomerTXDrain]' in s]
    if role=='coarse':
        frames=[x for x in jsonrows(BASE/role/'receive.jsonl') if x['kind']=='sample']
        r['rawFrameIdsResetAndContinuous']=[x['frameId'] for x in frames]==list(range(1,4319))*2
    report['branches'][role]=r
report['result']='PASS' if report['twoIdenticalOriginalReplays'] and all(r['accepted']==r['executed']==8636 and r['acceptedDigestsEqualSender'] and r['executedDigestsEqualSender'] and r['runtimeInitCount1'] and all(t['frames']==4318 and t['allRoundCorrect'] and t['logicalSeqResetAndContinuous'] for t in r['rounds'].values()) for r in report['branches'].values()) else 'FAIL'
(BASE/'round_identity_analysis.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
