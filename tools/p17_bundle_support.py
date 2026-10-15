"""Copy actual binaries, overlay, diagnostics and reproduction tools into the P17 kit.

Run after p17_collect_evidence.py; re-run that collector last for the full manifest.
No models or credentials are copied. Existing source evidence is never edited.
"""
import hashlib,json,pathlib,shutil,subprocess,html
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p17/deliverables/HwaSimIR_P17'
def copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
def main():
    # Preserve earlier generated candidates under an explicit failed-attempt
    # directory. These are our own known output files, never user source files.
    for rel in ('inputs/p17_weather_performance_120s.txt','inputs/p17_weather_performance_120s.txt.json','tools/p17_generate_perf_fixture.py'):
        old=OUT/rel
        if old.exists():
            dest=OUT/'diagnostics/unadmitted_fixture'/('earlier_bundle_'+old.name)
            dest.parent.mkdir(parents=True,exist_ok=True)
            if not dest.exists():old.rename(dest)
    for rel in ('logs/p17/build/candidate4/HwaSim_IR.aarch64','HwaSim_IR/Bin/HwaSim_IR.exe'):
        p=ROOT/rel;copy(p,OUT/'program'/p.name)
    receipt=json.loads((ROOT/'logs/p17/deployment/candidate4/deployment_receipt.json').read_text())
    for rel in receipt['resources']:
        path='HwaSim_IR/Bin/Config/'+rel
        copy(ROOT/path,OUT/'overlay/Config'/rel)
        old=OUT/'rollback/windows/Config'/rel;old.parent.mkdir(parents=True,exist_ok=True)
        old.write_bytes(subprocess.check_output(['git','show','HEAD:'+path],cwd=ROOT))
    copy(ROOT/'logs/p17/backups/HwaSim_IR.pre_p17.exe',OUT/'rollback/windows/HwaSim_IR.exe')
    for folder in ('baseline','checks','build/candidate4','deployment/candidate4','ui'):
        src=ROOT/'logs/p17'/folder
        for p in src.rglob('*'):
            if p.is_file() and p.suffix in ('.json','.log','.txt','.png','.env','.sha256','.csv','.tgz'):
                copy(p,OUT/'support'/folder/p.relative_to(src))
    for name in ('windows_build_candidate4.log',):copy(ROOT/'logs/p17'/name,OUT/'support'/name)
    copy(ROOT/'logs/p17/performance_summary.json',OUT/'performance_summary.json')
    for p in (ROOT/'docs').glob('HwaSimIR_P17*'):
        if p.is_file():copy(p,OUT/'docs'/p.name)
    for pattern in ('p17*','p5_run_case.ps1','p16_production_flicker_compare.py','p7_validate_recording.py','p5_ssh_askpass.cmd','p11_plume_physics*'):
        for p in (ROOT/'tools').glob(pattern):
            if p.is_file() and p.name!='p17_generate_perf_fixture.py':copy(p,OUT/'tools'/p.name)
    for p in (ROOT/'DataDrivenTestQT').glob('p14_ground_truck_weather_30s.txt*'):copy(p,OUT/'inputs'/p.name)
    for p in (ROOT/'DataDrivenTestQT').glob('p17_weather_performance_120s.txt*'):copy(p,OUT/'diagnostics/unadmitted_fixture'/p.name)
    copy(ROOT/'tools/p17_generate_perf_fixture.py',OUT/'diagnostics/unadmitted_fixture/generator.py')
    copy(ROOT/'logs/p17/performance_fixture_rejected.log',OUT/'diagnostics/unadmitted_fixture/rejected_before_init.log')
    copy(ROOT/'DataDrivenTestQT/1.txt',OUT/'inputs/1.txt')
    (OUT/'source').mkdir(exist_ok=True)
    (OUT/'source/changes.patch').write_bytes(subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT))
    diagnostics=OUT/'diagnostics/flicker'
    for folder in ('model_clock_audit','model_reprojection','encoding_alignment'):
        src=ROOT/'logs/p17/diagnostics'/folder
        for p in src.rglob('*'):
            if p.is_file():copy(p,diagnostics/folder/p.relative_to(src))
    for name in ('hdr_mapping_ab.json','aim120_duplicates.json','f35_duplicates.json','exact_face_audit.json'):
        p=ROOT/'logs/p17/diagnostics'/name
        if p.exists():copy(p,OUT/'diagnostics'/name)
    for case in ('p17_aim120_static_texture','p17_aim120_moving_texture','p17_aim120_moving_ids','p17_aim120_moving_radiance'):
        src=ROOT/'logs/p5'/case
        for p in src.glob('*'):
            if p.is_file() and p.suffix in ('.json','.png','.log','.h264'):copy(p,diagnostics/'source'/case/p.name)
    plume=OUT/'diagnostics/plume_comparison';plume.mkdir(parents=True,exist_ok=True)
    entries=[]
    for key,folder,label in (
        ('before','before/p17_before_mwir_narrow_fov_retry','P16 geometry + Game display (before)'),
        ('geometry','after/p17_after2_mwir_narrow_fov','P17 geometry + same Game display'),
        ('hdr','after/p17_hdr_mwir_narrow_fov','P17 geometry + fixed HDR display (separate mapping experiment)')):
        src=ROOT/'logs/p17/diagnostics'/folder
        png=next(src.glob('diagnostic_linear/*seq1200_rgb8.png'))
        copy(png,plume/(key+'.png'))
        copy(next(src.glob('diagnostic_linear/*seq1200.pfm')),plume/(key+'.pfm'))
        copy(src/'case_plan.json',plume/(key+'_case_plan.json'))
        entries.append(f'<h2>{html.escape(label)}</h2><p>sourceSeq=1200; explicit 0.5 microradian diagnostic; pre-encode Stage6 RGB, no per-image stretching.</p><a href="{key}.png"><img src="{key}.png" width="600"></a> <a href="{key}.pfm">float PFM</a> <a href="{key}_case_plan.json">identity</a>')
    (plume/'index.html').write_text('<!doctype html><meta charset="utf-8"><h1>Plume geometry / display experiments</h1><p>Not an ordinary FOV scene. Not a user visual acceptance.</p>'+''.join(entries),encoding='utf-8')
    before=ROOT/'logs/p17/diagnostics/before/p17_before_mwir_narrow_fov_retry'
    copy(before/'received.h264',plume/'before_received.h264')
    original=next(before.glob('recording/*/output.mp4'))
    copy(original,plume/'before_original.mp4.incomplete')
    derived=plume/'before_received_derived_60fps.mp4'
    if not derived.exists():
        subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-r','60','-i',str(before/'received.h264'),'-c:v','copy',str(derived)],check=True)
    (plume/'before_recovery.json').write_text(json.dumps(dict(originalRecorderFinalized=False,originalRetainedAs='before_original.mp4.incomplete',source='before_received.h264',derived=derived.name,frameRateSource='requested production 60 Hz; elementary H264 has no container PTS',timingScope='derived nominal playback timeline, not real-time performance evidence'),indent=2))
    with (plume/'index.html').open('a',encoding='utf-8') as f:
        f.write('<h2>Before source recovery</h2><p>Recorder did not finalize. Original incomplete container and received elementary stream are retained. This full-length MP4 is a derived remux at the requested 60 Hz; it is not evidence of measured FPS.</p><video width="600" controls src="before_received_derived_60fps.mp4"></video><p><a href="before_recovery.json">Recovery identity</a> | <a href="../p17_after2_mwir_narrow_fov/output.mp4">P17 geometry / same Game mapping full video</a> | <a href="../p17_hdr_mwir_narrow_fov/output.mp4">P17 HDR full video</a></p>')
    movies=list((diagnostics/'model_clock_audit').glob('*.mp4'))
    (diagnostics/'index.html').write_text('<!doctype html><meta charset="utf-8"><h1>Flicker diagnostic evidence - unresolved</h1><p>6-second controlled P5 clips, not ordinary production acceptance. Formal radiance black clip is invalid, not a stable result. Texture and ID motion are real; before/after rendering repair is NOT established.</p>'+''.join(f'<h2>{html.escape(p.name)}</h2><video width="600" controls src="model_clock_audit/{p.name}"></video>' for p in movies)+'<p><a href="model_reprojection/p17_aim120_moving_texture/reprojection.json">Known geometry reprojection</a></p>',encoding='utf-8')
    print(OUT)
if __name__=='__main__':main()
