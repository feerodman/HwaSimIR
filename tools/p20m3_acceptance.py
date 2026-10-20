"""Evidence-driven P20M3 acceptance. Runtime, performance and certification stay separate."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
from p20m3_evidence import trace, pfm
R=Path(__file__).resolve().parents[1];E=R/'logs/p20m3/20261021_engineering'
ap=argparse.ArgumentParser();ap.add_argument('--candidate-version',default='v5');ap.add_argument('--run-prefix',default='p20m3_v5_hires');ap.add_argument('--native-run',default=None);a=ap.parse_args()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
report=dict(schema='HwaSimIR.P20M3.Acceptance.1',quality='ENGINEERING_APPROX',
    policy='EngineeringContinuous',modelVersion='P20M3_GREY_GAS_1',
    modtran_full_domain_accuracy='PENDING',calibration='NOT_VERIFIED_CALIBRATION',actual_customer_software='NOT_RUN',
    sourceHead=subprocess.check_output(['git','rev-parse','HEAD'],cwd=R,text=True).strip(),
    sourceDiffSha256=hashlib.sha256(subprocess.check_output(['git','diff','HEAD'],cwd=R)).hexdigest())
report['deployment']=read(E/('deployment_'+a.candidate_version)/'receipt.json')
report['parameterIdentity']=read(E/'parameter_provenance.json')
report['providerScan']=read(E/'provider_scan.json')
report['planckTableAccuracy']=read(E/'planck_check.json')
report['boundaryScan']=(E/'provider_boundary_scan.txt').read_text(encoding='utf-8-sig').strip()
report['numericalRangeStatus']='PASS' if report['providerScan']['failures']==0 and 'failures=0' in report['boundaryScan'] else 'FAIL'
report['windowsBuilds']={str(p.relative_to(R)):dict(sha256=digest(p),bytes=p.stat().st_size) for p in [R/'build-p20m3-renderer/bin/HwaSim_IR.exe',R/'build-p20m3-stim/release/DataDrivenTestQT.exe',R/'build-p20m3-customer-receiver/release/HwaSimIRCustomerTxVideoReceiverDemo.exe']}
report['windowsRuns']={}
for name in ['windows_swir_formal_02','windows_mwir_formal_02','windows_swir_halfk','windows_mwir_halfk','windows_missing_lut_fixed','windows_corrupt_lut_fixed','windows_lifecycle_swir','windows_lifecycle_mwir','windows_single_reset_fixed','windows_mwir_temperature350','windows_swir_night','windows_mwir_night','windows_wrong_reference','windows_worker_reset','windows_ordinary_final']:
    d=E/name
    v=dict(execution=read(d/'result.json'),trace=trace(d/'physical.jsonl'))
    if (d/'linear.pfm').exists():v['gpuLinearFrame']=pfm(d/'linear.pfm')
    v['runtimeStatus']='PASS' if v['execution']['qt_exit']==0 and v['trace']['forcedBlackDueToAtmosphereMiss']==0 else 'FAIL'
    report['windowsRuns'][name]=v
report['customerCsvPreflight']=read(E/'preflight/results.json')
report['schedulerPayloadEquivalence']=read(E/'scheduler_payload_equivalence.json')
report['finalNumericalVerification']=read(E/'final_verify/verification.json')
report['windowsPackage']=read(E/'windows_package.json')
report['boardRuns']={}
for name in [a.native_run or a.run_prefix+'_native',*[a.run_prefix+'_'+c for c in ('csv1','csv2','csv3')]]:
    d=R/'logs/p20/runs'/name
    if not (d/'result.json').exists():report['boardRuns'][name]={'status':'NOT_RUN'};continue
    v=dict(transport=read(d/'result.json'),performance=read(d/'performance_p20m3.json'))
    if (d/'source_identity.json').exists():v['sourceIdentity']=read(d/'source_identity.json')
    v['traces']={};v['counterTotals']={}
    for role in ('coarse','precise'):
        p=d/role/'board/physical.jsonl'
        if p.exists():v['traces'][role]=trace(p)
        log=(d/role/'board/board.log').read_text(encoding='utf-8',errors='replace')
        counts=re.findall(r'\[P20M3 AtmosphereCounters\] evaluated=(\d+) forcedBlack=(\d+)',log)
        v['counterTotals'][role]=[dict(evaluated=int(x),forcedBlack=int(y)) for x,y in counts]
        assert 'P20M3_GREY_GAS_1' in log and report['parameterIdentity']['parameter_sha256'] in log
        assert report['deployment']['build']['elf_sha256'] in log
        assert report['deployment']['instances'][role]['root'] in log
        assert v['performance']['streams'][role]['hardwareGpu']
    v['runtimeStatus']='PASS' if all(v['transport'][role]['countPass'] and v['counterTotals'][role] and all(c['forcedBlack']==0 for c in v['counterTotals'][role]) for role in ('coarse','precise')) else 'FAIL'
    report['boardRuns'][name]=v
report['oldNearfieldErrors']={}
capture=R/'logs/p20/runs/p20m3_v5_lifecycle_capture'
if (capture/'physical_evidence_summary.json').exists():report['finalBoardGpuAndLifecycle']=read(capture/'physical_evidence_summary.json')
for p in (R/'logs/p20m2/20261021_v1').glob('*/overlap_report.json'):
    data=read(p);summary={}
    for band in ('SWIR','MWIR'):
        for mode in ('Transmittance','ThermalRadiance'):
            rows=[x for x in data['results'] if x.get('band')==band and x.get('mode')==mode and 'absolute_error' in x]
            if rows:summary[band+'_'+mode]=dict(maxAbsolute=max(rows,key=lambda x:x['absolute_error']),maxRelative=max(rows,key=lambda x:x['relative_error']),unit='1' if mode=='Transmittance' else 'W/(m^2 sr um)',status='OLD_DEVELOPMENT_ERROR_NOT_FINAL_IMAGE_ERROR')
    report['oldNearfieldErrors'][str(p.relative_to(R))]=summary
report['runtime']='PASS' if all(v.get('runtimeStatus')=='PASS' for v in report['boardRuns'].values()) and all(v['runtimeStatus']=='PASS' for v in report['windowsRuns'].values()) else 'INCOMPLETE_OR_FAIL'
report['dual60fpsAnd80ms']='PASS' if all(v.get('performance',{}).get('performancePass',False) for v in report['boardRuns'].values()) else 'NOT_PASSED'
report['overall']='ENGINEERING_RUNTIME_DELIVERED_PERFORMANCE_NOT_PASSED' if report['runtime']=='PASS' and report['dual60fpsAnd80ms']!='PASS' else report['runtime']
(E/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:report[k] for k in ('runtime','quality','dual60fpsAnd80ms','overall','modtran_full_domain_accuracy','actual_customer_software','calibration')},indent=2))
