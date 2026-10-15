"""Collect only SWIR/MWIR reruns; acquisition and appearance remain separate."""
import json
import html
import re
import shutil
from p17_collect_evidence import ROOT, inspect_case, read_json, sha
from p17_review_delivery import inspect_camera_case

OUT=ROOT/'logs/p17/deliverables/HwaSimIR_P17_SWIR_MWIR'
SRC=ROOT/'logs/p17/ir_retest'


def formal_check(src, band):
    log=(src/'board.log').read_text(encoding='utf-8',errors='replace')
    assert '[M1 FormalFailClosed][ERROR]' not in log, src
    assert not re.search(r'error occurred while compiling GLSL|Error linking|Segmentation fault',log),src
    samples=[line for line in log.splitlines() if '[M1 Compare]' in line and f'band={band}' in line
             and 'valid=1' in line and 'finalOutput=M1' in line]
    assert samples, ('no successful formal radiance',src)
    deployment=next(line for line in log.splitlines() if line.startswith('[DeploymentVersion] result=PASS'))
    elf=re.search(r'elfSha256=([0-9a-f]{64})',deployment).group(1)
    profile=next(line for line in log.splitlines() if line.startswith(f'[SensorProfile] Band={band} '))
    return dict(result='PASS_RUNTIME_FORMAL_SAMPLES',validLoggedSamples=len(samples),
        first=samples[0],last=samples[-1],failClosedErrors=0,
        rendererElfSha256=elf,deployment=deployment,effectiveBoardProfile=profile,
        scope='Runtime logs are sampled; not a per-product radiometric calibration')


def validate_index(dest,row):
    frames=[json.loads(s) for s in (dest/'frame_index.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    for key in ('sourceSeq','frameSeq','storageIndex'):
        assert [int(f[key]) for f in frames]==list(range(1,len(frames)+1)),(dest,key)
    status=read_json(dest/'recording_status.json')
    assert status['muxerFinalized'] and not status['fileError']
    assert len(frames)==int(status['completeProducts'])==int(row['mp4']['nb_read_frames'])


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    expected_elf=read_json(SRC/'deploy/hdr_v8/deployment_receipt.json')['elfSha256']
    rows=[]
    for band in ('swir','mwir'):
        names=['clear','rain','snow','plume0','plume1','plume0_detail','plume1_detail']
        for name in names:
            case=f'{band}_{name}_ir_v8';src=SRC/'after'/case;dest=OUT/'cases'/case
            row=inspect_case(src,dest,make_excerpt=False)
            assert row and row['band']==band.upper()
            shutil.copytree(src/'diagnostic_linear',dest/'diagnostic_linear',dirs_exist_ok=True)
            validate_index(dest,row)
            row['formalCheck']=formal_check(src,band.upper())
            assert row['formalCheck']['rendererElfSha256']==expected_elf
            row['reviewScope']=(f'原始 4318 行，{row["pixelAngleUrad"]} µrad/像素；尾焰'+('关闭' if name.startswith('plume0') else '开启')
                if name.startswith('plume') else '独立有效降水区输入，相机约 1 m；1800 帧，25 µrad/像素')
            if 'detail' in name: row['reviewScope']+='；明确标注窄视场细节诊断，未改变模型大小或尾焰源项'
            row['visualAcceptance']='ENGINEERING_REVIEW_SEE_REPORT'
            rows.append(row)
            print(case,'FULL_DECODE_AND_FORMAL_PASS',flush=True)
        for weather in ('clear','cloudy','overcast'):
            case=f'p17_{band}_{weather}_above_ir_v8';src=ROOT/'logs/p7'/case;dest=OUT/'cases'/case
            row=inspect_camera_case(src,dest)
            validate_index(dest,row)
            row['formalCheck']=formal_check(src,band.upper())
            assert row['formalCheck']['rendererElfSha256']==expected_elf
            row['reviewScope']='独立普通协议云上接近/横向视差轨迹；200 µrad/像素；实际 F22，尾焰关闭。相机始终处于 LUT 支持域。参考云被流式调度移除，不声称目标入云遮挡或穿云验收；MWIR 云纹理仍较弱。'
            row['visualAcceptance']='ENGINEERING_REVIEW_SEE_REPORT'
            rows.append(row)
            print(case,'FULL_DECODE_AND_FORMAL_PASS',flush=True)
    for name in ('clear','rain','snow','plume1'):
        case=f'mwir_{name}_ir_v7';src=SRC/'after'/case;dest=OUT/'cases'/case
        row=inspect_case(src,dest,make_excerpt=False)
        validate_index(dest,row)
        shutil.copytree(src/'diagnostic_linear',dest/'diagnostic_linear',dirs_exist_ok=True)
        row['reviewScope']='修复前 MWIR 默认 HDR 对照；雨雪外观对比不足，不作为最终外观通过证据'
        row['visualAcceptance']='BEFORE_DISPLAY_FIX'
        row['formalCheck']=formal_check(src,'MWIR')
        rows.append(row)
    case='p17_mwir_cloudy_above_ir_v7';src=ROOT/'logs/p7'/case
    row=inspect_camera_case(src,OUT/'cases'/case)
    validate_index(OUT/'cases'/case,row)
    row['reviewScope']='修复前 MWIR 默认 HDR 云上轨迹；只作显示问题基线'
    row['visualAcceptance']='BEFORE_DISPLAY_FIX';rows.append(row)
    row['formalCheck']=formal_check(src,'MWIR')
    row['provenanceNote']='Baseline request.profileSha256 describes the controller file edited during the isolated v8 build; actual rendered v7 profile and ELF are bound by the preserved board log above.'
    for row in rows:
        (OUT/'cases'/row['caseId']/'evidence.json').write_text(json.dumps(row,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    shutil.copytree(SRC/'fixtures',OUT/'fixtures',dirs_exist_ok=True)
    shutil.copytree(SRC/'query_check',OUT/'query_check',dirs_exist_ok=True)
    for filename in ('effect_check_ir_v8.json','weather_display_check.json','hdr_gpu_check.json',
                     'resource_delta.json','swir_unchanged_check.json','cloud_scope_check.json','windows_renderer_v8_check.log','stage6_v8_check.log'):
        shutil.copy2(SRC/filename,OUT/filename)
    shutil.copytree(SRC/'profile',OUT/'profile_check',dirs_exist_ok=True)
    shutil.copy2(ROOT/'docs/HwaSimIR_P17_SWIR_MWIR_Retest.md',OUT)
    shutil.copy2(SRC/'deploy/hdr_v8/deployment_receipt.json',OUT/'deployment_receipt.json')
    shutil.copy2(SRC/'build/hdr_v8/build_receipt.json',OUT/'build_receipt.json')
    for name in ('SWIR','MWIR'):
        shutil.copy2(ROOT/f'HwaSim_IR/Bin/Config/SensorWave/default_{name}.json',OUT)
    (OUT/'validation.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    write_index(rows)
    write_manifest()


def write_index(rows):
    content=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>SWIR / MWIR 复测</title>',
        '<style>body{font:16px system-ui;background:#18202a;color:#eee;margin:30px}a{color:#93d8ff}article{padding:20px;background:#253140;margin:20px 0}video{width:640px;max-width:100%}img{width:190px}</style>',
        '<h1>SWIR / MWIR · 尾焰、云、雨、雪</h1><p>最终 v8 使用固定 HDR；MWIR 已加入固定冷场景参考点。末尾保留 v7 修复前对照。无逐图增强。',
        '雨雪为有效降水区；云为有效大气查表域内的云上轨迹。采集通过不等于标定或性能验收。</p>',
        '<p>MWIR 编码前采样：雨的最大亮差由 +1 提高到 +2～+4 灰度；雪的最大暗差由 −1 提高到 −2 灰度。雪仍是弱暗信号。相同采样的背景区域原始辐射逐值不变。</p>',
        '<p>云层复测限制：MWIR 云纹理仍弱；参考云在运行中被卸载，本组不作为目标入云遮挡或穿云验收。</p>',
        '<p><a href="HwaSimIR_P17_SWIR_MWIR_Retest.md">复测结论及限制</a> · <a href="validation.json">完整性及正式辐射检查</a> · <a href="manifest.sha256">文件摘要</a></p>']
    content.append('<table cellpadding="10"><tr><th>效果</th><th>SWIR</th><th>MWIR</th></tr>')
    for label,key in [('雨','rain'),('雪','snow'),('尾焰开启','plume1'),('尾焰细节','plume1_detail'),('多云','cloudy'),('阴天','overcast')]:
        content.append('<tr><td>'+label+'</td>')
        for band in ('swir','mwir'):
            case=f'p17_{band}_{key}_above_ir_v8' if key in ('cloudy','overcast') else f'{band}_{key}_ir_v8'
            content.append(f'<td><a href="#{case}">{band.upper()} {label}</a></td>')
        content.append('</tr>')
    content.append('</table><p>MWIR 修复前对照位于页面末尾。雨雪的明暗方向由原始辐射决定；雪可比背景更暗。</p>')
    for row in rows:
        rel='cases/'+row['caseId']
        poster=f' poster="{rel}/{row["frameIdentity"][0]["file"]}"' if row['frameIdentity'] else ''
        content += [f'<article id="{row["caseId"]}"><h2>{html.escape(row["caseId"])}</h2><p>{row["reviewScope"]} {row["frames"]} 帧。</p>',
            f'<video controls preload="none"{poster} src="{rel}/output.mp4"></video><p><a href="{rel}/evidence.json">参数与检查记录</a></p>']
        for f in row['frameIdentity']:
            content.append(f'<a href="{rel}/{f["file"]}"><img loading="lazy" src="{rel}/{f["file"]}" title="sourceSeq={f["sourceSeq"]}"></a>')
        content.append('</article>')
    (OUT/'index.html').write_text('\n'.join(content)+ '</html>',encoding='utf-8')


def write_manifest():
    (OUT/'manifest.sha256').write_text(''.join(sha(p)+'  '+p.relative_to(OUT).as_posix()+'\n'
        for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='manifest.sha256'),encoding='utf-8')


if __name__=='__main__':
    main()
