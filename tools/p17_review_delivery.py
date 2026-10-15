"""Package real P17 follow-up recordings, with explicit scope and no tone changes."""
import argparse
import html
import json
import shutil
from pathlib import Path
from p17_collect_evidence import ROOT, inspect_case, sha, run, read_json


def inspect_camera_case(src, dest):
    """Validate an ordinary protocol camera run without inventing a DDS-case PASS."""
    request = read_json(src/'request.json')
    conservation = read_json(src/'conservation.json')
    assert conservation['result'] == 'PASS'
    assert request['normal'] and request['productionDefaults'] and request['cameraInput']
    assert not request['volumeOff'] and not request['sheetOff']
    movies = list(src.glob('recording/*/output.mp4'))
    assert len(movies) == 1
    recording = movies[0].parent
    status = read_json(recording/'recording_status.json')
    assert status['muxerFinalized'] and not status['fileError']
    frames = [json.loads(line) for line in (recording/'frame_index.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    assert len({(f['session'], f['generation'], f['run']) for f in frames}) == 1
    for key in ('sourceSeq', 'frameSeq', 'storageIndex'):
        assert [int(f[key]) for f in frames] == list(range(1, len(frames)+1))
    assert len(frames) == int(status['completeProducts']) == conservation['boardAcceptedCounts'][0]
    board = (src/'board.log').read_text(encoding='utf-8', errors='replace')
    assert 'Segmentation fault' not in board and 'Program received signal' not in board
    dest.mkdir(parents=True, exist_ok=True)
    for p in src.iterdir():
        if p.is_file() and p.suffix in ('.json', '.log', '.sha256'):
            shutil.copy2(p, dest/p.name)
    for p in recording.iterdir():
        if p.is_file():
            shutil.copy2(p, dest/p.name)
    shutil.copy2(ROOT/request['cameraInput'], dest/'camera_input.json')
    movie = dest/'output.mp4'
    probe = json.loads(run(['ffprobe', '-v', 'error', '-count_frames', '-select_streams', 'v:0',
        '-show_entries', 'stream=width,height,nb_read_frames,duration,avg_frame_rate', '-of', 'json', str(movie)]))['streams'][0]
    assert int(probe['nb_read_frames']) == len(frames) and (probe['width'], probe['height']) == (800, 800)
    run(['ffmpeg', '-nostdin', '-xerror', '-v', 'error', '-i', str(movie), '-f', 'null', '-'])
    picks = []
    for seq in (180, 720, 1200, 1440, 1740):
        if seq > len(frames):
            continue
        row = frames[seq-1]
        name = f'sourceSeq_{seq}_decoded.png'
        run(['ffmpeg', '-nostdin', '-v', 'error', '-y', '-i', str(movie), '-vf',
             f'select=eq(n\\,{seq-1})', '-frames:v', '1', str(dest/name)])
        picks.append(dict(file=name, **row))
    band = {0:'SWIR', 1:'NIR', 2:'MWIR'}[request['band']]
    return dict(caseId=src.name, band=band, weather={0:'Clear',1:'Cloudy',5:'Overcast'}[request['weather']],
        frames=len(frames), pixelAngleUrad=request['pixelAngle'], frameIdentity=picks,
        physicsMode=('legacy_NIR' if band == 'NIR' else 'formal_M1_'+band) +
                    '; independent ordinary protocol camera path; NOT_VERIFIED_CALIBRATION',
        reviewScope=('独立普通协议遮挡检查：实际 F22 在云前、云中、云后；未修改原 1.txt。'
                     if 'occlusion' in request['cameraInput'] else
                     '独立普通协议相机轨迹：云下、云上、接近、穿云及离开；未修改原 1.txt。'),
        mp4=probe, harnessResult='PASS_CONSERVATION_AND_FULL_DECODE',
        parameterFile='request.json')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', default='logs/p17/deliverables/HwaSimIR_P17_Review')
    parser.add_argument('--camera-case', action='append', default=[])
    parser.add_argument('cases', nargs='+')
    args = parser.parse_args()
    assessments = read_json(ROOT/'logs/p17/review/inspection/review_assessments.json')
    out = (ROOT / args.output).resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    for name in args.cases:
        src = (ROOT / name).resolve()
        dest = out / 'cases' / src.name
        info = inspect_case(src, dest, make_excerpt=False)  # full MP4, no needless re-encode
        if not info:
            raise RuntimeError(f'No complete recording: {src}')
        for subdir in ('diagnostic_linear', 'reprojection'):
            if (src / subdir).exists():
                shutil.copytree(src / subdir, dest / subdir, dirs_exist_ok=True)
        plan = json.loads((src / 'case_plan.json').read_text(encoding='utf-8-sig'))
        note = ('原轨迹高空案例；不作为近场降水可见性证据。' if plan['inputMode'] == 'Original'
                and plan['weather'] in ('Rain', 'Snow') else
                '独立近地轨迹，使用普通渲染与生产传输；不是原 1.txt 场景。' if plan['inputMode'] == 'P14GroundWeather'
                else '完整原 1.txt，普通渲染；视场与深度隔离参数见 case_plan.json。')
        info['reviewScope'] = note
        rows.append(info)
        print(src.name, 'DECODE_VERIFIED', flush=True)
    for name in args.camera_case:
        src = (ROOT/name).resolve()
        row = inspect_camera_case(src, out/'cases'/src.name)
        env = read_json(src/'effective_environment.json')
        if env.get('EnableAGC') == 'false':
            row['reviewScope'] += ' 固定映射诊断：统一关闭 AGC，保持原增益、伽马和源项；不冒充默认 AGC。'
        rows.append(row)
        print(src.name, 'CAMERA_DECODE_VERIFIED', flush=True)
    for row in rows:
        for category, assessment in assessments.items():
            if isinstance(assessment, dict) and row['caseId'] in assessment.get('cases', []):
                row['followupEngineeringReview'] = dict(category=category, **assessment)
                row['visualAcceptance'] = assessment['result']
        (out/'cases'/row['caseId']/'evidence.json').write_text(
            json.dumps(row, indent=2, ensure_ascii=False)+'\n', encoding='utf-8')
    content = ['<!doctype html><html lang="zh-CN"><meta charset="utf-8">',
               '<title>P17 回看修复证据</title>',
               '<style>body{font:16px system-ui;background:#18202a;color:#eee;margin:30px}'
               'a{color:#93d8ff}article{padding:20px;background:#253140;margin:20px 0}'
               'video{width:640px;max-width:100%}img{width:190px}code{overflow-wrap:anywhere}</style>',
               '<h1>P17 回看修复证据</h1><p>均为未增强的实际输出。原高空录像保留；近地雨雪、'
               '普通云层和深度隔离分开标注。采集/解码通过不等于所有视觉、性能或标定通过。</p>',
               '<p><a href="HwaSimIR_P17_Followup.md">修复结论与剩余边界</a> · '
               '<a href="checks/inspection/review_assessments.json">工程验收范围</a> · '
               '<a href="manifest.sha256">文件哈希</a> · NOT_VERIFIED_CALIBRATION</p>']
    for row in rows:
        rel = 'cases/' + row['caseId']
        content += [f'<article><h2>{html.escape(row["caseId"])}</h2>',
                    f'<p>{html.escape(row["reviewScope"])} {row["band"]} / {row["weather"]}; '
                    f'{row["frames"]} 帧，{row["pixelAngleUrad"]} µrad/像素。</p>',
                    f'<p>{html.escape(row["physicsMode"])}</p>',
                    f'<video controls preload="none" src="{rel}/output.mp4"></video>',
                    f'<p><a href="{rel}/output.mp4">完整 MP4</a> · '
                    f'<a href="{rel}/{row.get("parameterFile", "case_plan.json")}">输入与参数</a> · '
                    f'<a href="{rel}/frame_index.jsonl">逐帧身份</a></p>']
        for frame in row['frameIdentity']:
            content.append(f'<a href="{rel}/{frame["file"]}"><img loading="lazy" '
                           f'src="{rel}/{frame["file"]}" title="sourceSeq={frame["sourceSeq"]}"></a>')
        content.append('</article>')
    content.append('</html>')
    (out / 'index.html').write_text('\n'.join(content), encoding='utf-8')
    (out / 'validation.json').write_text(json.dumps(rows, indent=2, ensure_ascii=False), encoding='utf-8')
    shutil.copy2(ROOT / 'docs/HwaSimIR_P17_Followup.md', out)
    for relative in ('windows_build_v7.log', 'stage7_v7_check.log', 'world_cloud_v7_check.log',
                     'inspection/cloud_cache_check.json', 'inspection/depth_comparison.json',
                     'inspection/cloud_runtime_check.json', 'inspection/review_assessments.json',
                     'inspection/cloud_occlusion_check.json',
                     'deploy/weather_depth_v4/deployment_receipt.json',
                     'build/weather_depth_v4/build_receipt.json',
                     'deploy/cloud_alpha_v5/deployment_receipt.json',
                     'build/cloud_alpha_v5/build_receipt.json',
                     'deploy/cloud_attachment_v6/deployment_receipt.json',
                     'build/cloud_attachment_v6/build_receipt.json',
                     'deploy/cloud_mips_v7/deployment_receipt.json',
                     'build/cloud_mips_v7/build_receipt.json'):
        src = ROOT / 'logs/p17/review' / relative
        if src.exists():
            dest = out / 'checks' / relative
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
    (out / 'manifest.sha256').write_text(''.join(sha(p) + '  ' + p.relative_to(out).as_posix() + '\n'
        for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'manifest.sha256'), encoding='utf-8')


if __name__ == '__main__':
    main()
