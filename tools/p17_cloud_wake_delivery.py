"""Package decoded board recordings without changing their tone or frame identity."""
import argparse
import html
import shutil
from p17_collect_evidence import ROOT, inspect_case, read_json, save_json, sha
from p17_review_delivery import inspect_camera_case
from p17_ir_delivery import formal_check, validate_index

OUT=ROOT/'logs/p17/deliverables/HwaSimIR_P17_Cloud_Wake'
WORK=ROOT/'logs/p17/cloud_rework'

def specifications():
    for band in ('mwir','swir'):
        for name,label in (('wake_full','完整尾流 · 10 µrad'),('plume1','尾流开启 · 25 µrad'),('plume0','发动机关闭对照 · 25 µrad')):
            yield WORK/'after'/f'{band}_{name}_v11', f'{band.upper()} {label}', 'wake', 'v11'
        yield ROOT/f'logs/p17/ir_retest/after/{band}_plume1_ir_v8',f'{band.upper()} 修改前尾焰 · 25 µrad','baseline','v8'
        for weather,label in (('cloudy','白色团云与层云'),('overcast','阴天层云'),('clear','晴天对照')):
            yield ROOT/f'logs/p7/p17_{band}_{weather}_white_v12',f'{band.upper()} {label}','cloud','v12'
        for mode,label in (('white','白云显示'),('radiometric','纯辐射显示')):
            yield ROOT/f'logs/p7/p17_{band}_cloud_{mode}_frozen_v12',f'{band.upper()} 固定风场隔离：{label}','isolation','v12'
    for weather,label in (('clear','晴天'),('rain','雨'),('snow','雪')):
        yield WORK/'after'/f'mwir_{weather}_v11',f'MWIR 有效降水区：{label}','weather','v11'

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--available',action='store_true');a=ap.parse_args()
    OUT.mkdir(parents=True,exist_ok=True);rows=[];missing=[]
    for src,title,group,version in specifications():
        dest=OUT/'cases'/src.name
        camera=src.parent.name=='p7'
        ready=(src/('conservation.json' if camera else 'case_result.json')).exists()
        if not ready:
            missing.append(src.name)
            if a.available:continue
            raise RuntimeError(f'Incomplete case: {src}')
        cache=dest/'evidence.json'
        if cache.exists() and read_json(cache).get('packageValidation')=='FULL_DECODE_AND_IDENTITY_VERIFIED':
            row=read_json(cache)
        else:
            row=inspect_camera_case(src,dest) if camera else inspect_case(src,dest,make_excerpt=False)
            validate_index(dest,row)
            row['formalCheck']=formal_check(src,row['band'])
            receipt=(ROOT/'logs/p17/ir_retest/deploy/hdr_v8/deployment_receipt.json' if version=='v8' else WORK/f'deploy/{version}/deployment_receipt.json')
            assert row['formalCheck']['rendererElfSha256']==read_json(receipt)['elfSha256']
            if camera:
                for file in ('linear.pfm','linear_rgb8.png','received.png'):
                    if (src/file).exists():shutil.copy2(src/file,dest/file)
            else:
                shutil.copytree(src/'diagnostic_linear',dest/'diagnostic_linear',dirs_exist_ok=True)
            row['packageValidation']='FULL_DECODE_AND_IDENTITY_VERIFIED'
        row.update(title=title,group=group,rendererVersion=version)
        row['reviewScope']={
            'wake':'原始 4318 行输入；发动机对照与 10 µrad 完整尾流细节。局部喷流，不是持久凝结尾迹。',
            'baseline':'保留 v8 修改前输出，仅用于同为 25 µrad 的尾焰外观对照。',
            'cloud':'独立普通协议云上接近及横向观察轨迹；200 µrad；相机约 11.2 km；不作为穿云验收。白云为显示外观。',
            'isolation':'同一静态阶段 sourceSeq 180；仅将层云 UV 风速缩放设为零，用于白云显示与 SI 原始附件隔离。',
            'weather':'独立有效降水区轨迹；相机约 1 m；1800 帧；保留 MWIR HDR 修正后的雨雪对照。'
        }[group]
        row['visualAcceptance']='ENGINEERING_REVIEW_SCOPE_IN_REPORT'
        save_json(dest/'evidence.json',row);rows.append(row)
        print(src.name,'FULL_DECODE_AND_IDENTITY_VERIFIED',flush=True)
    save_json(OUT/'validation.json',dict(cases=rows,missing=missing,complete=not missing,calibration='NOT_VERIFIED_CALIBRATION',performance='DIAGNOSTIC_RAW_EXPORT_NOT_60_HZ_ACCEPTANCE'))
    for name in ('fixtures_v2','query_check_v2','plume_physics'):
        shutil.copytree(WORK/name,OUT/name,dirs_exist_ok=True)
    for version in ('v11','v12'):
        (OUT/'receipts'/version).mkdir(parents=True,exist_ok=True)
        for kind in ('build','deploy'):
            file=WORK/kind/version/('build_receipt.json' if kind=='build' else 'deployment_receipt.json')
            if file.exists():
                d=OUT/'receipts'/version;d.mkdir(parents=True,exist_ok=True);shutil.copy2(file,d/file.name)
        for name in ('source_manifest.sha256',):
            f=WORK/'build'/version/name
            if f.exists():shutil.copy2(f,OUT/'receipts'/version/name)
    for name in ('depth_gate_visual_check.json','cloud_wake_checks.json','version_scope.json','stage7_check.log','windows_build_v12_final.log'):
        if (WORK/name).exists():shutil.copy2(WORK/name,OUT/name)
    for name in ('HwaSimIR_P17_Cloud_Wake_Rework.md',):shutil.copy2(ROOT/'docs'/name,OUT/name)
    config=OUT/'config';config.mkdir(exist_ok=True)
    for name in ('GameVFX/sprite.vert','GameVFX/sprite.frag','Weather/world_cloud_demo.json','Weather/world_cloud_game.json','SensorWave/default_SWIR.json','SensorWave/default_MWIR.json'):
        d=config/name;d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/'HwaSim_IR/Bin/Config'/name,d)
    render_index(rows,missing)
    files=sorted(p for p in OUT.rglob('*') if p.is_file() and p.name!='manifest.sha256')
    (OUT/'manifest.sha256').write_text(''.join(f'{sha(p)}  {p.relative_to(OUT).as_posix()}\n' for p in files),encoding='utf-8')

def render_index(rows,missing):
    parts=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">',
        '<title>P17 白云与尾流</title><style>body{font:16px/1.6 system-ui;background:#161d27;color:#eef3f8;max-width:1250px;margin:auto;padding:28px}a{color:#9dddff}nav{display:flex;gap:20px;flex-wrap:wrap}article{background:#242f3e;padding:24px;margin:24px 0;border-radius:12px}video{width:620px;max-width:100%}.frames{display:flex;overflow:auto;gap:12px}.frames img{width:180px}h1,h2{line-height:1.25}.note{color:#bacada}code{overflow-wrap:anywhere}</style>',
        '<h1>白色团云、层云与柔化尾流</h1><p>RK3588 实际输出 · SWIR / MWIR · 视频与采样图未经亮度增强。</p>',
        '<nav><a href="#wake">尾流</a><a href="#cloud">白云</a><a href="#weather">MWIR 雨雪回归</a><a href="#baseline">修改前尾焰</a><a href="#isolation">辐射隔离</a></nav>',
        '<p><a href="HwaSimIR_P17_Cloud_Wake_Rework.md">改动与验收范围</a> · <a href="validation.json">完整解码及帧身份检查</a> · <a href="cloud_wake_checks.json">采样对照</a> · <a href="manifest.sha256">文件哈希</a></p>',
        '<p class="note">白云亮度为独立显示外观，不代表红外温度。尾流是随喷口运动的扩散气体。10 µrad 为细节视场；与旧版对比请使用 25 µrad。含原始辐射导出的录像不作为 60 Hz 性能验收。</p>']
    if missing:parts.append('<p>采集中：'+html.escape(', '.join(missing))+'</p>')
    for group,label in (('wake','新增尾流'),('cloud','团云与层云'),('weather','MWIR 雨雪回归'),('baseline','修改前尾焰'),('isolation','白云显示隔离')):
        parts.append(f'<h2 id="{group}">{label}</h2>')
        for row in (r for r in rows if r['group']==group):
            rel='cases/'+row['caseId'];picks=row['frameIdentity'];seq=4200 if group in ('wake','baseline') else 180 if group in ('cloud','isolation') else 1200
            poster=next((p['file'] for p in picks if p['sourceSeq']==seq),picks[0]['file'])
            parts.append(f'<article><h3>{html.escape(row["title"])}</h3><p>{html.escape(row["reviewScope"])}</p><video controls preload="none" poster="{rel}/{poster}" src="{rel}/output.mp4"></video><p>{row["frames"]} 帧 · {row["rendererVersion"]} · <a href="{rel}/output.mp4">完整原视频</a> · <a href="{rel}/evidence.json">证据及配置</a></p><div class="frames">')
            for p in picks:
                parts.append(f'<a href="{rel}/{p["file"]}"><img loading="lazy" src="{rel}/{p["file"]}" alt="sourceSeq {p["sourceSeq"]}"><br>帧 {p["sourceSeq"]}</a>')
            parts.append('</div></article>')
    parts.append('</html>');(OUT/'index.html').write_text('\n'.join(parts),encoding='utf-8')

if __name__=='__main__':main()
