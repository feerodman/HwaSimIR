"""Run the existing independent MP4/SEI/body/PTS/RGB verifier after timing.

Each result belongs to one actual recording; aggregate case rows are not added
once per recording. Failed historical evidence is retained, never relabelled.
"""
import argparse,csv,json,subprocess,sys,time
from collections import Counter
from pathlib import Path
from p7_validate_conservation import audit
from p8_latency_report import report as latency
from p8_case_metrics import ordered_ledger

ROOT=Path(__file__).resolve().parents[1]
FFMPEG=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'

def input_products(case):
    accepted=ordered_ledger(case,'accepted')
    if not accepted:return
    bytime={int(r['beginNs']):r for r in accepted}
    assert len(bytime)==len(accepted),'Non-unique accepted timestamps'
    products=[];errors=[];times=[]
    for path in sorted((case/'recording').glob('*/frame_index.jsonl')):
        for line in path.read_text(encoding='utf-8').splitlines():
            p=json.loads(line);key=int(p['acceptedSteadyNs']);source=bytime.get(key)
            if source is None:errors.append(dict(recording=path.parent.name,storageIndex=p['storageIndex'],reason='unknown accepted timestamp'));continue
            if int(source['sourceSeq'])!=int(p['sourceSeq']):errors.append(dict(reason='sourceSeq differs from accepted input',time=key))
            times.append(key)
            products.append(dict(recording=path.parent.name,storageIndex=p['storageIndex'],session=p['session'],generation=p['generation'],run=p['run'],frameSeq=p['frameSeq'],sourceSeq=p['sourceSeq'],acceptedSteadyNs=key,acceptedInputOrdinal=source['ordinal'],canonicalInputDigest=source['digestFNV1a64']))
    req=json.loads((case/'request.json').read_text(encoding='utf-8-sig'))
    multiplicity=Counter(times)
    if req['simMode']==1 and (len(times)!=len(accepted) or any(multiplicity[k]!=1 for k in bytime)):
        errors.append(dict(reason='sync inputs do not each have exactly one saved product'))
    if times!=sorted(times):errors.append(dict(reason='accepted input order went backwards'))
    value=dict(result='PASS' if products and not errors else 'FAIL',accepted=len(accepted),savedProducts=len(products),uniqueAcceptedInputsInProducts=len(multiplicity),errors=errors,scope='Join every independently verified MP4 index product to exact producer acceptance time and source sequence; repeated accepted states allowed only in async mode')
    (case/'input_product_validation.json').write_text(json.dumps(value,indent=2),encoding='utf-8')
    if products:
        with (case/'accepted_product_correspondence.csv').open('w',newline='',encoding='utf-8-sig') as f:
            w=csv.DictWriter(f,fieldnames=products[0]);w.writeheader();w.writerows(products)

def main(cases):
    records=[]
    for case in cases:
        for movie in sorted((case/'recording').glob('*/output.mp4')):
            folder=movie.parent;start=time.monotonic()
            cmd=[sys.executable,str(ROOT/'tools/p7_validate_recording.py'),str(folder),'--ffmpeg',str(FFMPEG)]
            with (folder/'independent_validation.log').open('w',encoding='utf-8') as out:
                result=subprocess.run(cmd,stdout=out,stderr=subprocess.STDOUT)
            row=dict(case=case.name,recording=folder.name,exitCode=result.returncode,elapsedSeconds=time.monotonic()-start,result='PASS' if result.returncode==0 else 'FAIL')
            if result.returncode:
                (folder/'validation.json').write_text(json.dumps(dict(result='FAIL',reason='Independent validator failed; see independent_validation.log')),encoding='utf-8')
            records.append(row);print(json.dumps(row),flush=True)
        audit(case)
        try:
            input_products(case)
        except (TypeError,ValueError,AssertionError) as error:
            # Historical pre-flush audit files can be incomplete even when the
            # actual MP4/body/index verification passes. Never infer missing
            # input evidence from matching video counts.
            (case/'input_product_validation.json').write_text(json.dumps(dict(result='NOT_MEASURED',reason='Input ledger cannot support a complete accepted-product join',detail=str(error))),encoding='utf-8')
        if list((case/'recording').glob('*/frame_index.jsonl')):latency(case)
    (ROOT/('logs/p8/saved_validation_batch_'+cases[0].name+'.json')).write_text(json.dumps(records,indent=2),encoding='utf-8')
    return int(any(r['exitCode'] for r in records))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('cases',nargs='+',type=Path);sys.exit(main(p.parse_args().cases))
