"""Aggregate actual evidence with scoped verdicts; never promote whole P18 to PASS."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p18'
def read(path):
    p=BASE/path
    return json.loads(p.read_text(encoding='utf-8')) if p.exists() else None
main=read('runs/dual_perf_on_v4/result.json')
perf=read('performance_comparison.json');media=read('media_audit.json') or []
def case(name):
    r=read(f'runs/{name}/result.json')
    if not r:return {'case':name,'execution':'NOT_VERIFIED','reason':'No completed result file'}
    return {'case':name,'execution':'EXPECTED_REJECTION_OBSERVED' if r.get('expectedRejection') else 'COMPLETED' if r.get('executionCompleted') or r.get('functionalCountResult')=='PASS' else 'FAILED_OR_PARTIAL',
            'resultFile':f'logs/p18/runs/{name}/result.json','result':r,
            'bytePixelAnalysis':read(f'runs/{name}/byte_pixel_analysis.json')}
coverage={
 'TX/HD264':['precise_mwir_old_and_customer_v2','precise_h264_rgb_shared_v2','precise_h264_gray_colorcard_v2'],
 'TX/LD264':['coarse_h264_rgb_customer_only_v2','coarse_h264_rgb_colorcard_v2','customer_gray_encoder_override_v2'],
 'TX/HDecode':['precise_raw800_gray_v2'],
 'TX/LDecode':['coarse_raw_rgb_colorcard_v2','resize_raw320_to640_v2']}
weather=[f'retain_{band}_weather{w}_v2' if not (band=='swir' and w==3) else 'retain_swir_weather3_retry' for band in ['swir','mwir'] for w in [0,2,3]]
weather += [f'retain_{band}_cloud{w}_v2' for band in ['swir','mwir'] for w in [1,5]]
views=[f'nozzle_{band}_{v}_v3' for band in ['swir','mwir'] for v in ['side','oblique','end','near','far','occluded']]+[f'nozzle_{band}_320_v2' for band in ['swir','mwir']]
layers=[f'diagnostic_{band}_{mode}_v3' for band in ['swir','mwir'] for mode in ['core','halo','mask']]
lifecycle=['customer_absent_graceful_v3','customer_slow_v2','late_idr_recovery_v2','reconnect_idr_recovery_v2',
 'old_reopen_graceful_v4','paused_connected_customer_v4','held_dds_history_v4','resize_raw320_to640_v2','customer_h264_1024_v2','dual_two_rounds_v2','dual_final_mwir_v4','raw800_receiver_v4','ld264_rgb_receiver_v4','late_idr_recovery_v4','reconnect_idr_recovery_v4']
required=weather+views+layers+lifecycle+['plume_swir_engine_off_v2','plume_mwir_engine_off_v2','plume_swir_old_graceful_v3','plume_mwir_old_graceful_v3']
unexecuted=[n for n in required if case(n)['execution']!='COMPLETED']
report={
 'schema':'HwaSimIR.P18.Acceptance.1','overall':'NOT_PASSED','limits':{'fps':60,'latencyMs':80,'warmupRows':600,'minimumMeasurementSeconds':60},
 'requirements':read('requirements_results.json'),
 'browserIndexReview':read('browser_review.json'),
 'rawRunnerMarkersMeaning':'Embedded run-level performance/visual markers are preserved original harness placeholders; final scoped verdicts are in this acceptance report and linked analysis files.',
 'boundedFailure':{'receiverQueueOverflow':'OBSERVED_EXPLICIT_FAILURE_WITH_OLD_CHAIN_CONTINUING','sdkWriterTimeout':'NOT_TRIGGERED','sdkTimeoutIsolation':'NOT_VERIFIED','heldReaderHistoryActuallyFull':'NOT_VERIFIED_no_occupancy_counter','evidence':'runs/held_dds_history_v4/board.out.log'},
 'finalEnvironment':read('final_environment.json'),
 'functional':{'dualMainCounts':main.get('functionalCountResult') if main else 'NOT_VERIFIED','fullMatrix':'EXECUTED_REVIEW_SCOPED_RESULTS' if not unexecuted else 'INCOMPLETE',
               'mainRun':'dual_perf_on_v4','main':main,'twoRoundIdentity':read('runs/dual_two_rounds_v2/round_identity_analysis.json'),
               'topics':{topic:[case(n) for n in names] for topic,names in coverage.items()},'lifecycle':[case(n) for n in lifecycle],
               'capacityRejection':[case(n) for n in ['reject_raw800_rgb_v2','reject_raw1024_gray_v2']],
               'old1024CustomerDisabled':case('old1024_customer_disabled_v2')},
 'visual':{'verdict':'SCOPED_ENGINEERING_APPEARANCE_ONLY','manualReview':read('visual_review.json'),
           'weather':[case(n) for n in weather],'views':[case(n) for n in views],'layers':[case(n) for n in layers],
           'weatherPixelChecks':read('weather_retention.json'),'calibration':'NOT_VERIFIED_CALIBRATION'},
 'performance':{'strictFps':perf['strictFpsResult'] if perf else 'NOT_VERIFIED','details':'logs/p18/performance_comparison.json',
                'crossHostOneWay':'NOT_VERIFIED_no_clock_calibration','screenPresentation':'NOT_VERIFIED_no_present_timestamp',
                'windowsStimulusToDecodedImage':{r:perf['comparison']['on'][r].get('windowsStimulusToDecodedImageMs') for r in ['coarse','precise']} if perf else None},
 'actualCustomerInterop':'NOT_VERIFIED_self_written_Demo_is_not_customer_acceptance',
 'protectedBaseline':read('protected_preservation.json'),
 'media':{'audited':len(media),'unreadable':[m for m in media if not m['readable']],
          'countMismatches':[m for m in media if m.get('countMatchesReceiver') is False or m.get('countMatchesOldIndex') is False]},
 'notCompletedCases':unexecuted,
 'retainedFailedAttempts':['dual_swir_original_02','customer_overload_v2','dual_perf_on_v2','retain_swir_weather3_v2','controlled_callback_timeout_v2','old_reopen_graceful_v3','paused_customer_transport_v3'],
 'note':'COMPLETED is execution, not blanket acceptance. Read byte counts, all media, faults, visual limits and strict performance separately.'}
(BASE/'acceptance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({'overall':report['overall'],'notCompletedCases':unexecuted,'unreadableMedia':len(report['media']['unreadable'])},indent=2))
