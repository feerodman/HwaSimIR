"""Package verified P9 actual reception media with a portable, relative-link page."""
import csv,hashlib,html,json,re,shutil,subprocess,zipfile
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
CASE=ROOT/'logs/p9/final60'; OUT=ROOT/'logs/p9/delivery'
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
OUT.mkdir(parents=True,exist_ok=True)
results=json.loads((CASE/'p9_results.json').read_text(encoding='utf-8'))
movie=next((CASE/'recording').glob('*/output.mp4'));folder=movie.parent
assert json.loads((folder/'validation.json').read_text())['result']=='PASS'
correspondence=list(csv.DictReader((folder/'all_frame_correspondence.csv').open(encoding='utf-8-sig')))
selected=[]
for frame,name in [(179,'scene_3s.png'),(1799,'scene_30s.png'),(3479,'scene_58s.png')]:
    subprocess.run([str(FF),'-hide_banner','-loglevel','error','-y','-i',str(movie),'-vf',f'select=eq(n\\,{frame})','-frames:v','1','-fps_mode','passthrough',str(OUT/name)],check=True)
    with Image.open(OUT/name) as im: digest=hashlib.md5(im.convert('RGB').tobytes()).hexdigest()
    row=correspondence[frame];assert digest==row['rgbMd5'],(frame,digest,row['rgbMd5'])
    selected.append(dict(file=name,**row))
subprocess.run([str(FF),'-hide_banner','-loglevel','error','-y','-i',str(movie),'-t','12','-map','0:v:0','-c','copy','-movflags','+faststart',str(OUT/'demo_12s.mp4')],check=True)
for src,dst in [(CASE/'receiver_ui.png','receiver_running.png'),(CASE/'receiver_stopped.png','receiver_stopped.png'),(CASE/'sender_ui.png','sender.png'),(CASE/'control_timing.csv','control_timing.csv'),(CASE/'p9_results.json','results.json'),(CASE/'receiver_ui.png.layout.json','layout.json'),(folder/'validation.json','recording_validation.json'),(CASE/'input_product_validation.json','input_product_validation.json'),(CASE/'accepted_product_correspondence.csv','accepted_product_correspondence.csv'),(folder/'all_frame_correspondence.csv','all_frame_correspondence.csv'),(ROOT/'logs/p9/reference_results.txt','local_tests.txt'),(ROOT/'logs/p9/qt_pixels_results.txt','qt_pixels_tests.txt')]:shutil.copy2(src,OUT/dst)
(OUT/'selected_frames.json').write_text(json.dumps(selected,indent=2),encoding='utf-8')
shutil.copy2(ROOT/'docs/HwaSimIR_P9_CustomerDemo_QuickFix.md',OUT/'P9_Closeout.md')
shutil.copy2(ROOT/'logs/p9/deployment_and_restore_check.txt',OUT/'deployment_check.txt')
with (OUT/'runtime_statistics.csv').open('w',newline='',encoding='utf-8-sig') as f:
    w=csv.writer(f);w.writerow(['metric','value','definition'])
    for k in ['actualInputHz','newImageFpsFirst60Seconds','savedProducts','queueDepthMax','inputBackpressureCountMax','controlUnder10ms','controlResponseHeld','stopVisibleWithoutVideo','visibleCloudBudgetObservedMax']:w.writerow([k,results[k],'actual final60 evidence'])
    for k,v in results['latency']['statsMs'].items():w.writerow(['outputLatency_'+k,v,'application clock estimate; all valid frames; includes startup'])
controls=''.join(f'<tr><td>{ {1:"复位",2:"开始",3:"停止"}[int(r["command"])] }</td><td>{float(r["responseMs"]):.6f} ms</td><td>{float(r["equivalentHz"]):.1f} Hz</td></tr>' for r in results['controls'])
page=f'''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P9 客户演示交付</title><style>body{{margin:0;background:#101923;color:#e1e9f1;font:16px/1.65 system-ui,sans-serif}}main{{max-width:1160px;margin:auto;padding:28px}}h1{{font-size:28px}}h2{{font-size:21px;margin-top:30px}}a{{color:#90d4ff}}table{{border-collapse:collapse;width:100%;background:#172633}}td,th{{text-align:left;padding:8px 12px;border-bottom:1px solid #344555}}img,video{{display:block;max-width:100%;height:auto;background:#080d12}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px}}figure{{margin:0}}figcaption{{font-size:14px;color:#b4c6d4;margin:7px 0}}.pass{{color:#91e1ad}}code{{overflow-wrap:anywhere}}p{{max-width:95ch}}</style>
<main><h1>HwaSimIR P9 · 客户演示</h1><p class="pass">最终 60 秒：实际输入 {results['actualInputHz']:.3f} Hz，接收新图 {results['newImageFpsFirst60Seconds']:.2f} FPS；{results['savedProducts']} 个输入与实际 MP4、标注和索引逐帧验证通过。</p>
<p>普通启动：原始 1.txt、F35、近红外（NIR）/Auto、同步 60 Hz、800×800、静态世界多云（同屏预算 2）、既有标注。视频按解码尺寸显示，只缩小。录像含板端一次绘制的画内标注；原始标注坐标未变。</p>
<h2>控制指令响应</h2><table><tr><th>指令</th><th>接收→业务开始</th><th>响应倒数（等效）</th></tr>{controls}</table>
<p>STOP 的独立控制执行通路实际建立排空屏障；渲染线程继续完成安全排空。本次排空阶段另计 37.4 ms 左右（精确日志见结果 JSON），没有截断响应值，也不代表每秒可完成同等数量的指令。Realtime、INIT 和视频不覆盖最近结果，停止无视频后仍显示。</p>
<h2>实际接收视频 · 12 秒</h2><video controls preload="metadata" width="800" height="800" src="demo_12s.mp4" poster="scene_3s.png"></video>
<p>片段直接截取实际 RK3588→MPP→DDS→Windows 保存视频，保持 800×800，无再绘制标注。PNG 与整片逐帧 RGB 摘要一致。</p>
<div class="grid"><figure><img src="scene_3s.png"><figcaption>约 3 秒：体积云和片云进入普通画面。</figcaption></figure><figure><img src="scene_30s.png"><figcaption>约 30 秒：原始轨迹继续运行。</figcaption></figure><figure><img src="scene_58s.png"><figcaption>约 58 秒：同一静态世界，云不跟随目标。</figcaption></figure></div>
<h2>实际 Qt 界面</h2><img src="receiver_running.png"><p>实际屏幕 2560×1440，DPR=2；窗口客户区 2560×1334。左右逻辑宽度均 614，视频为 800×800 物理像素。平台 2 行、目标 7 行完整可见，无表格滚动。</p><img src="receiver_stopped.png"><p>停止后实际窗口截图：视频 FPS 为 0，Control 响应仍保留。</p><details><summary>发送器界面</summary><img src="sender.png"></details>
<h2>边界与证据</h2><p>输出延时是应用层时基估计：P99 {results['latency']['statsMs']['p99']:.3f} ms，最大 {results['latency']['statsMs']['maximum']:.3f} ms，超过 80 ms 共 {results['latency']['over80ms']} 帧；全部保留。队列峰值 {results['queueDepthMax']}，满队列回压 {results['inputBackpressureCountMax']}。近云偏软、代理深度近似和真实模型语义未标定等 P8 限制仍保留。本轮没有重跑十分钟矩阵，也不宣称物理/材料模型已验证。</p>
<p><a href="control_timing.csv">Control 原始计时</a> · <a href="runtime_statistics.csv">运行统计</a> · <a href="results.json">完整短测结果</a> · <a href="all_frame_correspondence.csv">实际视频逐帧对应</a> · <a href="accepted_product_correspondence.csv">输入到保存帧对应</a> · <a href="recording_validation.json">文件验证</a> · <a href="layout.json">实际布局</a> · <a href="selected_frames.json">PNG 帧身份</a></p>
<p>部署：Windows P9_A4；板端 P9_A5，ELF SHA256 <code>dcc6d04cb578cda1bcd39b683820a14c96a78ed17d3fb1babc6b9515f003acad</code>。<a href="P9_Closeout.md">完整交付报告</a> · <a href="deployment_check.txt">部署与回滚哈希</a>。页面与媒体的相对链接均在本包内，可解压后打开，或整目录上传。报告中列出的全量日志路径用于本机追溯，不作为页面依赖。</p></main></html>'''
# Never hard-code a previous run's drain duration into this final page.
phase=' '.join(results['stopPhases']);m=re.search(r'drainWaitMs=([0-9.]+)',phase)
if m:page=page.replace('37.4 ms 左右',m.group(1)+' ms')
(OUT/'index.html').write_text(page,encoding='utf-8')
files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.iterdir() if p.is_file() and p.name!='manifest.json'}
(OUT/'manifest.json').write_text(json.dumps(files,indent=2),encoding='utf-8')
archive=OUT.parent/'HwaSimIR_P9_CustomerDemo.zip'
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(OUT.iterdir()):
        if p.is_file():z.write(p,p.name)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for target in re.findall(r'(?:src|href)="([^"]+)"',page):assert target in z.namelist(),target
print(json.dumps(dict(package=str(archive),bytes=archive.stat().st_size,files=len(files)+1,mediaReferences='PASS',selectedFrameRgbHashes='PASS')))
