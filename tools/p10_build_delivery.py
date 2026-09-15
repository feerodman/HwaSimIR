"""Portable P10 evidence: actual reception media, small tables and local report."""
import csv,hashlib,html,json,re,shutil,subprocess,zipfile
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p10';OUT=BASE/'delivery';OUT.mkdir(exist_ok=True)
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'

def copy(source,name):shutil.copy2(source,OUT/name)
def extract(case,frames,prefix):
    movie=next((case/'recording').glob('*/output.mp4'));folder=movie.parent
    assert json.loads((folder/'validation.json').read_text())['result']=='PASS'
    rows=list(csv.DictReader((folder/'all_frame_correspondence.csv').open(encoding='utf-8-sig')))
    selected=[]
    for number in frames:
        name=f'{prefix}_{number:04}.png'
        subprocess.run([str(FF),'-hide_banner','-loglevel','error','-y','-i',str(movie),'-vf',f'select=eq(n\\,{number})','-frames:v','1','-fps_mode','passthrough',str(OUT/name)],check=True)
        digest=hashlib.md5(Image.open(OUT/name).convert('RGB').tobytes()).hexdigest()
        assert digest==rows[number]['rgbMd5']
        selected.append(dict(file=name,**rows[number]))
    return movie,selected

normal=BASE/'P10_final_ui_verified60';graphics=BASE/'P10_motion_edge60';final=BASE/'P10_A9_edge60'
normal_results=json.loads((normal/'p9_results.json').read_text())
selected=[]
movie,sel=extract(normal,[179,1799,3479],'normal_nir');selected+=sel
gm,sel=extract(graphics,[179],'ordinary_motion');selected+=sel
for src,dst in [(movie,'normal_received_12s.mp4'),(gm,'ordinary_motion_12s.mp4')]:
    subprocess.run([str(FF),'-hide_banner','-loglevel','error','-y','-i',str(src),'-t','12','-map','0:v:0','-c','copy','-movflags','+faststart',str(OUT/dst)],check=True)
for mode in ['off','msaa2','msaa4','edge']:
    _,sel=extract(BASE/f'P10_A7_{mode}' if mode!='edge' else final,[179],f'aa_{mode}_received');selected+=sel
for name in ['labels_0_before.png','labels_0_after.png','labels_2_before.png','labels_2_after.png']:copy(BASE/'labels'/name,name)
for name in ['labels_0_original_coordinates.txt','labels_2_original_coordinates.txt']:copy(BASE/'labels'/name,name)
for name in ['sphere_off_left_edge_right_2x.png','checker_off_left_edge_right_2x.png']:copy(BASE/'aa'/name,name)
copy(normal/'receiver_ui.png','normal_receiver_ui.png')
copy(next((BASE/'synthetic_mwir/frame_0000').glob('*preview*.png')),'synthetic_mwir_preview.png')
for src,name in [(BASE/'aa/comparison.json','aa_actual.json'),(BASE/'aa/comparison.csv','aa_comparison.csv'),
    (BASE/'labels/label_tests.csv','label_coordinate_tests.csv'),(BASE/'profiles/tests.json','profile_tests.json'),
    (BASE/'coverage/coverage.json','atmosphere_coverage.json'),(normal/'p9_results.json','normal_results.json'),
    (normal/'control_timing.csv','control_timing.csv'),(normal/'accepted_product_correspondence.csv','normal_input_to_product.csv'),
    (BASE/'retention/retention_result.json','retention_result.json'),(BASE/'deploy_A9/deployment_final.txt','deployment_final.txt'),
    (BASE/'post_retention_board.txt','post_retention_board.txt'),(BASE/'acceptance_receipt.json','acceptance_receipt.json'),
    (final/'p8_metrics.json','final_graphics_metrics.json'),(final/'input_product_validation.json','final_input_product_validation.json'),
    (BASE/'synthetic_mwir/manifest.json','synthetic_manifest.json'),(BASE/'source_final.json','source_final.json'),
    (ROOT/'HwaSim_IR/Bin/Config/TargetLib/portable_models.json','portable_models.json')]:copy(src,name)
for band in ['VIS-SWIR','SWIR','MWIR']:copy(BASE/f'profiles/default_{band}.result.json',f'profile_{band}.json')
copy(ROOT/'docs/HwaSimIR_P10_Profile_Audit_Label_AA_Retention.md','P10_Closeout.md')
(OUT/'selected_frames.json').write_text(json.dumps(selected,indent=2),encoding='utf-8')
captions={
 'labels_0_before.png':'Windows 普通字体测试：五组重合点，修改前', 'labels_0_after.png':'同帧修改后：仅文字位移与引线',
 'labels_2_before.png':'Windows 普通字体测试：边缘，修改前','labels_2_after.png':'同帧边缘修改后',
 'aa_off_received_0179.png':'RK3588 普通几何 seq 180：Off，实际 DDS 解码图',
 'aa_msaa2_received_0179.png':'实际 DDS：请求 2×，附件 0，未生效', 'aa_msaa4_received_0179.png':'实际 DDS：请求 4×，附件 0，未生效',
 'aa_edge_received_0179.png':'相同线性输入：最终 A9 EdgeAA，实际 DDS 解码图',
 'sphere_off_left_edge_right_2x.png':'局部 2 倍最近邻放大：左 Off，右 EdgeAA',
 'checker_off_left_edge_right_2x.png':'局部 2 倍最近邻放大：左 Off，右 EdgeAA',
 'normal_nir_0179.png':'正常启动 NIR 接收图，视频第 180 帧', 'normal_nir_1799.png':'正常启动 NIR 接收图，第 1800 帧',
 'normal_nir_3479.png':'正常启动 NIR 接收图，第 3480 帧', 'ordinary_motion_0179.png':'A7 普通几何移动/遮挡；边角采样另由 A9 修正',
 'normal_receiver_ui.png':'实际 Windows 接收器界面；屏幕/DPI 见 normal_results.json',
 'synthetic_mwir_preview.png':'独立 800×800 人工 MWIR 实验预览，非生产输入，非器件标定'}
identities={row['file']:row for row in selected}
image_metadata=[]
for name,caption in captions.items():
    item={'file':name,'caption':caption,'storedSize':list(Image.open(OUT/name).size),
          'frameIdentity':identities.get(name),'sourceSize':[800,800]}
    if name.startswith('labels_'):
        item.update(source='Windows Panda actual font fixture',profile='not applicable: direct text fixture',aa='not applicable',display='direct font rendering',frameIdentity={'fixtureCase':int(name.split('_')[1]),'inputCoordinatesFile':f"labels_{name.split('_')[1]}_original_coordinates.txt"})
    elif name.startswith('normal_'):
        item.update(source='RK3588 A9 ordinary startup / Windows DDS',profile='default_NVG.json',rangeUm=[0.7,1.1],display='Auto, normal dynamic mapping',aa='EdgeAA, native samples 0',perFrameMapping='not exported in normal run')
        if name=='normal_receiver_ui.png':item['frameIdentity']='live widget capture; not identified as a selected MP4 frame'
    elif name.startswith('synthetic_'):
        item.update(source='ordinary_sensor_lab synthetic reference',profile='synthetic_mwir_image.json',rangeUm=[3,5],display='fixed DN/16383 to RGB8, no gamma/AGC',aa='not applicable',frameIdentity={'syntheticFrame':0,'seed':20260915})
    else:
        item.update(source='RK3588 ordinary generated graphics',profile='default_MWIR.json',rangeUm=[3,5],display='Game; gain 3, offset 0, gamma 2.2, Reinhard, white hot; Auto off',aa='EdgeAA' if ('edge' in name or name.startswith('ordinary_')) else 'Off',nativeSamples=0)
        if name.startswith(('sphere_','checker_')):item.update(source='pre-encode RGB8 crop, 2x nearest-neighbor',aa='left Off / right A9 EdgeAA',frameIdentity={'sourceSeq':180,'cases':['P10_A7_off','P10_A9_edge60']})
        elif name.startswith('aa_'):item['case']='P10_A9_edge60' if 'edge' in name else f"P10_A7_{name.split('_')[1]}"
        else:item['case']='P10_motion_edge60'
    image_metadata.append(item)
(OUT/'image_manifest.json').write_text(json.dumps(image_metadata,ensure_ascii=False,indent=2),encoding='utf-8')
retention=json.loads((BASE/'retention/retention_result.json').read_text())
gallery=''.join(f'<figure><a href="{name}"><img src="{name}" loading="lazy"></a><figcaption>{html.escape(caption)}</figcaption></figure>' for name,caption in captions.items())
links=''.join(f'<li><a href="{p.name}">{p.name}</a></li>' for p in sorted(OUT.iterdir()) if p.suffix in ('.json','.csv','.md','.txt'))
page=f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>HwaSimIR P10 实际交付</title>
<style>body{{margin:24px auto;padding:0 20px;max-width:1300px;font:16px/1.6 system-ui,sans-serif;color:#243244;background:#edf1f5}}h1{{font-size:28px}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(310px,1fr));gap:18px}}figure{{margin:0;padding:12px;background:white;border-radius:8px}}img,video{{max-width:100%;height:auto}}figcaption{{padding-top:8px}}a{{color:#175bb1}}</style>
<h1>HwaSimIR P10：配置、文字、AA、保留策略</h1>
<p>原始接收图没有后加说明或修图。比较裁图另列；生成几何、Windows 字体测试、人工光谱与正常运行清楚区分。SWIR / VIS-SWIR 未启用生产链，2× / 4× MSAA 未生效。正常启动使用 EdgeAA；旧预设可恢复。</p>
<p>正常启动新图 {normal_results['newImageFpsFirst60Seconds']:.2f} FPS；输入 {normal_results['actualInputHz']:.3f} Hz。结果含启动阶段；录像逐帧校验单列，不能用帧数代替画质。</p>
<p>{normal_results['savedProducts']} 条输入与保存产品逐项对应；STOP 开始执行响应 0.158375 ms，另有 52.184 ms 排空。估计输出延时最大 114.18 ms，15 帧超过 80 ms，均保留。</p>
<p>配置核验：VIS-SWIR 实际 0.5–1.7 μm，范围不兼容；SWIR 实际 1.5–2.5 μm，与代码 1.1–2.5 冲突；MWIR 3–5 μm 通过。NIR 文件保持原状。</p>
<p>删除 5 套完整中间快照，实际释放 {retention['actualFreedBytes']/1048576:.2f} MiB；保留当前 A9 与完整 P9 回滚，未完成及不完整目录明确排除。回滚完整性通过，回滚启动本轮未测。</p>
<p><a href="P10_Closeout.md">完整报告与限制</a></p>
<div class="grid"><figure><video controls preload="metadata" src="normal_received_12s.mp4"></video><figcaption>RK3588 → MPP → DDS → Windows，正常运行 12 秒；保留实际画内标注。</figcaption></figure>
<figure><video controls preload="metadata" src="ordinary_motion_12s.mp4"></video><figcaption>A7 普通生成几何移动/遮挡；最终 A9 另修边角采样，内部 AA 算法相同。</figcaption></figure></div><br><div class="grid">{gallery}</div><h2>检查数据</h2><ul>{links}</ul></html>'''
(OUT/'index.html').write_text(page,encoding='utf-8')
manifest=[]
for p in sorted(OUT.iterdir()):
    if p.is_file() and p.name!='sha256.txt':manifest.append(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name)
(OUT/'sha256.txt').write_text('\n'.join(manifest)+'\n',encoding='ascii')
archive=BASE/'HwaSimIR_P10_Delivery.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(OUT.iterdir()):
        if p.is_file():z.write(p,p.name)
print(json.dumps({'zip':str(archive),'bytes':archive.stat().st_size,'images':len(captions),'videos':2,'relativeDependencies':True}))
