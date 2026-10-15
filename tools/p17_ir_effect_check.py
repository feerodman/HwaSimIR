"""Check rerun identity, formal controls, and signed pre-display engine differences."""
import json
import argparse
import numpy as np
from p17_collect_evidence import ROOT, read_json, read_pfm

BASE=ROOT/'logs/p17/ir_retest/after'
parser=argparse.ArgumentParser();parser.add_argument('--suffix',default='ir_v8');args=parser.parse_args()
results=[]
for band in ('swir','mwir'):
    for detail in ('','_detail'):
        paths=[BASE/f'{band}_plume{engine}{detail}_{args.suffix}' for engine in (0,1)]
        plans=[read_json(p/'case_plan.json') for p in paths]
        assert plans[0]['inputSha256']==plans[1]['inputSha256']
        assert plans[0]['pixelAngleUrad']==plans[1]['pixelAngleUrad']
        checks=[]
        for engine,p in enumerate(paths):
            log=(p/'board.log').read_text(encoding='utf-8',errors='replace')
            assert plans[engine]['engineStateOverride']==engine
            lines=[line for line in log.splitlines() if '[Stage5 Plume]' in line
                   and f'engineState={engine}' in line and f'band={band.upper()}' in line]
            if engine==1:
                assert any('formalTauReady=1' in s and 'coreVisible=1' in s for s in lines),p
            else:
                lines=[line for line in log.splitlines() if '[Stage5 PlumePerf]' in line]
                assert lines and all('visiblePlumeCount=0' in s for s in lines),p
            checks.append(lines[-1])
        samples=[]
        for seq in (900,1200,3600,4200):
            arrays=[read_pfm(next((p/'diagnostic_linear').glob(f'*_seq{seq}.pfm'))) for p in paths]
            assert all(np.isfinite(a).all() for a in arrays)
            delta=arrays[1].mean(axis=2)-arrays[0].mean(axis=2)
            samples.append(dict(sourceSeq=seq,minimum=float(delta.min()),maximum=float(delta.max()),
                positivePixels=int((delta>1e-5).sum()),negativePixels=int((delta<-1e-5).sum()),
                offMaximum=float(arrays[0].max()),onMaximum=float(arrays[1].max())))
        assert any(s['maximum']>1 and s['positivePixels']>0 for s in samples if s['sourceSeq']>=3600)
        results.append(dict(band=band.upper(),pixelAngleUrad=plans[0]['pixelAngleUrad'],
            result='PASS_CONTROL_AND_FINITE_SIGNED_RESPONSE',cases=[p.name for p in paths],
            plumeLogs=checks,samples=samples,unit='W/(m^2 sr um)',
            scope='Engine On minus Off includes engine-linked body heat; not isolated plume-only radiometric validation'))
out=ROOT/f'logs/p17/ir_retest/effect_check_{args.suffix}.json'
out.write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print(out)
