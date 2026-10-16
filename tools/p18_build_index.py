"""Local review index for real P18 evidence; no invented measurements."""
import html, json, os
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p18'
MAIN_RUN='dual_perf_on_v4'
main=json.loads((BASE/'runs'/MAIN_RUN/'result.json').read_text(encoding='utf-8'))
def link(path,label=None):
    p=ROOT/path if isinstance(path,str) else path
    if not p.exists():return '<span class="muted">'+html.escape(label or str(path))+' — 未形成文件</span>'
    href=Path(os.path.relpath(p,BASE)).as_posix()
    return f'<a href="{html.escape(href)}">{html.escape(label or p.name)}</a>'
def video(path):
    p=ROOT/path if isinstance(path,str) else path
    if not p.exists():return ''
    return f'<video controls preload="metadata" src="{Path(os.path.relpath(p,BASE)).as_posix()}"></video>'
def image(path,alt):
    p=ROOT/path
    return f'<a href="{Path(os.path.relpath(p,BASE)).as_posix()}"><img loading="lazy" src="{Path(os.path.relpath(p,BASE)).as_posix()}" alt="{html.escape(alt)}"></a>' if p.exists() else ''
parts=['''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P18 · 实际验收证据</title><style>
:root{color-scheme:dark}*{box-sizing:border-box}body{font:16px/1.65 system-ui,sans-serif;background:#10141c;color:#e4ebf3;max-width:1280px;margin:auto;padding:32px}a{color:#84c9ff}h1,h2{line-height:1.25}h2{margin-top:44px}p{max-width:1040px}.box,article{border:1px solid #364355;border-radius:10px;padding:20px;background:#18212e;margin:16px 0}.notice{border-left:5px solid #e6b260}.grid{display:grid;grid-template-columns:1fr 1fr;gap:20px}video,img{width:100%;max-height:850px;object-fit:contain;background:#080b11}table{border-collapse:collapse;width:100%;font-size:14px}td,th{text-align:left;border-bottom:1px solid #344354;padding:9px;overflow-wrap:anywhere}summary{cursor:pointer;font-weight:600}.muted{color:#b0bbc9}.bad{color:#ffae9e}button,input{padding:10px;font:inherit}nav{display:flex;gap:18px;flex-wrap:wrap}code{overflow-wrap:anywhere}@media(max-width:800px){.grid{grid-template-columns:1fr}body{padding:16px}}
</style><h1>HwaSimIR P18 · 实际验收证据</h1><p>尾焰局部恢复 · 原 DDS 链保留 · 四个客户 typed Topic · 同 RK3588 双实例</p>
<div class="box notice"><b>请分别阅读功能、视觉、性能与真实客户互通结论。</b><p>严格60 FPS / 80 ms门槛保持不变；本阶段整体未通过性能验收。已测 Windows激励→Windows解码闭环时延，跨主机单向及屏幕呈现时延仍未验证。自写Demo通过不代表客户实机互通。完整状态和失败以结项记录及机器结果为准。</p></div><nav>''']
for name,label in [('docs/HwaSimIR_P18_Closeout.md','结项记录'),('docs/HwaSimIR_DDS_EndToEnd_Guide.md','DDS端到端'),('docs/HwaSimIR_Customer_TX_DDS_ICD.md','客户ICD'),('docs/HwaSimIR_P18_DualChannel_Runbook.md','命令与配置'),('logs/p18/acceptance.json','机器验收'),('logs/p18/delivery_manifest.json','文件manifest')]:parts.append(link(name,label))
parts.append('</nav><h2>同板双路 · 完整4318行原始激励</h2><p>主计数证据 '+MAIN_RUN+'：两个INIT、两份真实ACK、一次sensor255业务广播。粗路原生320，精路原生800，视频按各自本地帧顺序保存。点击同步播放仅对齐录像时间，不是网络时延或实时桌面证据。</p><div class="grid" id="dual">')
for role,title in [('coarse','1001/1 · TX/LDecode · 320×320 Gray8'),('precise','1001/2 · TX/HD264 · 800×800 H264灰度')]:
    r=main[role]
    parts.append('<article><h3>'+title+'</h3>'+video(f'logs/p18/runs/{MAIN_RUN}/{role}/output.mp4')+f'<p>{r["senderFrames"]}发送 / {r["decodedFrames"]}解码 / {r["savedFramesVerified"]}保存；{r["receivedSamples"]} '+('raw Samples' if role=='coarse' else '压缩分片')+'</p></article>')
parts.append('</div><button onclick="pairPlay()">从当前共同位置播放两路</button> <button onclick="pairPause()">两路暂停</button> <label>跳到逻辑帧 <input id="seq" type="number" min="1" max="4318" value="4200" style="width:110px"></label> <button onclick="pairSeek()">定位</button>')
parts.append('<p>'+link('logs/p18/visual/dual_simultaneous_recordings.mp4','并排完整回放（实际同期接收录像，粗路仅补黑边）')+' · '+link('logs/p18/visual/dual_media_provenance.json','来源与处理说明')+'</p>')
parts.append(image('logs/p18/visual/dual_4200_paired.png','同期双路录像第4200帧；不是桌面截图'))
parts.append('<details open><summary>同一实际双路运行的原生 GUI 窗口</summary><p>分别通过系统窗口捕获取得，采集时间见JSON；两张截图并非同一时刻的整桌面截图，也不用于推导屏幕时延。</p><div class="grid">'+image('logs/p18/visual/dual_live_coarse.png','实际粗路接收窗口')+image('logs/p18/visual/dual_live_precise.png','实际精路接收窗口')+'</div>'+link('logs/p18/visual/dual_live_windows.json','窗口与采集时间')+'</details>')
parts.append('<h2>尾焰 · 同轨迹固定显示 before / after</h2><p>before来自已验证P17v12；不使用初次发现的陈旧板端部署。普通生产轨迹、engine=1、10µrad，后段3600/4200均提供全帧。尾焰参考图只规定外形，不代表辐射标定。</p>')
for band in ['swir','mwir']:
    parts.append('<article><h3>'+band.upper()+' · sourceSeq 4200</h3>'+image(f'logs/p18/visual/{band}_10_4200_comparison.png',band+' full comparison'))
    before=list((BASE/f'before/p18_before_{band}_10urad_p17v12/recording').glob('*/output.mp4'))
    parts.append('<div class="grid"><div><h4>P17v12 before</h4>'+ (video(before[0]) if before else '')+'</div><div><h4>P18 after</h4>'+video(f'logs/p18/runs/plume_{band}_old_graceful_v3/customer/output.mp4')+'</div></div><p>')
    for seq in [900,1200,2400,3600,4200]:parts.append(link(f'logs/p18/visual/{band}_10_{seq}_comparison.png',f'{seq}全帧')+' · '+link(f'logs/p18/visual/{band}_10_{seq}_crop.png','标明ROI裁剪')+'　')
    parts.append('</p></article>')
parts.append('<details><summary>原始两张尾焰参考</summary><div class="grid">'+image('docs/HwaSimIR_P18_Codex_Package/inputs/plume_reference_1.png','原始参考1')+image('docs/HwaSimIR_P18_Codex_Package/inputs/plume_reference_2.png','原始参考2')+'</div></details>')
parts.append('<h2>天气保留与尾焰受控视角</h2><p>以下卡片只列实际生成且可解码媒体。是否满足视觉目标须结合结项中的人工检查；诊断core/halo/mask与色卡不作为普通红外生产画面。</p><div class="grid">')
media=json.loads((BASE/'media_audit.json').read_text(encoding='utf-8'))
for m in media:
    if '/customer/output.mp4' not in m['path'] or not m['readable']:continue
    case=Path(m['path']).parts[3]
    if case.startswith(('nozzle_','diagnostic_')) and case.endswith('_v2') and '_320_' not in case:continue
    if not any(case.startswith(p) for p in ['retain_','nozzle_','diagnostic_','plume_swir_engine','plume_mwir_engine']):continue
    parts.append('<article><h3>'+html.escape(case)+'</h3>'+video(m['path'])+f'<p>{m["width"]}×{m["height"]}，独立解码计数 {m["decodedSavedFrames"]} 帧。</p></article>')
parts.append('</div><details><summary>实际客户窗口截图（SWIR雨，独立原生GUI）</summary>'+image('logs/p18/visual/customer_live_weather_window.png','实际原生客户窗口截图')+'</details>')
parts.append('<details><summary>后段运动抽帧、分层与遮挡诊断</summary><p>运动预览每60帧取一帧，完整800图另存，不代表已逐帧排除所有亚秒闪烁。v2遮挡诊断混用深度，因此只以v3判定遮挡。</p>')
for band in ['swir','mwir']:
    for suffix in ['motion_contact','views_contact','layers_contact','occlusion_depth_comparison']:
        parts.append(image(f'logs/p18/visual/{band}_{suffix}.png',f'{band} {suffix}'))
parts.append(link('logs/p18/visual_review.json','人工视觉检查与局限')+'</details>')
parts.append('<h2>所有实际运行记录</h2><p>executionCompleted表示编排完成，不等于整项验收通过。迟加入/重连只覆盖加入后的帧；sender API成功不代表客户最终收到。所有失败目录均保留。</p><table><tr><th>案例</th><th>编排/计数状态</th><th>结果与审计</th></tr>')
for folder in sorted((BASE/'runs').iterdir()):
    result=folder/'result.json';status='结果未形成／查看日志'
    if result.exists():
        r=json.loads(result.read_text(encoding='utf-8'));status=r.get('functionalCountResult') or ('预期容量拒绝已观察' if r.get('expectedRejection') else ('编排完成；按分项审计' if r.get('executionCompleted') else r.get('error','非成功／查看结果')))
    links=[link(result,'result.json')]
    for filename in ['byte_pixel_analysis.json','input_identity_analysis.json','round_identity_analysis.json','old_sei_check.json']:
        if (folder/filename).exists():links.append(link(folder/filename,filename))
    parts.append('<tr><td>'+html.escape(folder.name)+'</td><td>'+html.escape(str(status))+'</td><td>'+' · '.join(links)+'</td></tr>')
parts.append('</table><h2>每份媒体的独立读取与计数</h2><p>保存帧数由ffprobe逐帧读取，不能由文件标称60/120FPS推算。损坏原录像明确列为失败，不作为可用交付视频。</p><table><tr><th>实际文件</th><th>尺寸 / 读出帧数</th><th>读取结果</th></tr>')
for m in media:
    parts.append('<tr><td>'+link(m['path'],m['path'].removeprefix('logs/p18/'))+'</td><td>'+str(m.get('width','—'))+'×'+str(m.get('height','—'))+' / '+str(m.get('decodedSavedFrames','—'))+'</td><td>'+('可读取' if m['readable'] else '<b class="bad">失败：'+html.escape(m['stderr'][:120])+'</b>')+'</td></tr>')
parts.append('</table><p>'+link('logs/p18/media_audit.json','媒体审计JSON')+' · '+link('logs/p18/performance_comparison.json','60秒性能窗口与全部计时')+' · '+link('logs/p18/protected_preservation.json','原输入/模型/天气/旧协议保护')+'</p>')
parts.append('''<script>
const pair=()=>Array.from(document.querySelectorAll('#dual video'));
function pairPause(){pair().forEach(v=>v.pause());}
function pairSeek(){pairPause();const t=(Math.min(4318,Math.max(1,Number(document.getElementById('seq').value)))-1)/60;pair().forEach(v=>v.currentTime=t);}
async function pairPlay(){const vs=pair();if(vs.length!==2)return;vs[1].currentTime=vs[0].currentTime;await Promise.allSettled(vs.map(v=>v.play()));}
</script></html>''')
(BASE/'index.html').write_text('\n'.join(parts),encoding='utf-8')
print('Wrote',BASE/'index.html')
