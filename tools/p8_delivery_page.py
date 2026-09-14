"""Local evidence page. Media pixels are unchanged; overlays are optional SVG."""
import csv,html,json,os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR',str(Path(__file__).resolve().parents[1]/'logs/p8/matplotlib_cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p8';OUT=BASE/'delivery'
def esc(value):return html.escape(str(value),quote=True)
def rel(path):return Path(os.path.relpath(path,OUT)).as_posix()
def figure(path,title):return f'<figure><a href="{esc(path)}"><img src="{esc(path)}" alt="{esc(title)}"></a><figcaption>{esc(title)}</figcaption></figure>'
def main():
    fig,axes=plt.subplots(2,1,figsize=(11,6),sharex=True)
    for case,label in [('P8_A6_legacy_current_rain192','Old readback + previous policy (181 inputs missing)'),('P8_final_NVG_rain600','Final NVG rain128 + selected policy')]:
        with (BASE/case/'per_second.csv').open(encoding='utf-8-sig') as f:rows=[r for r in csv.DictReader(f) if 0<=int(r['second'])<65]
        axes[0].plot([int(r['second']) for r in rows],[float(r['newFrames']) for r in rows],label=label)
        axes[1].plot([int(r['second']) for r in rows],[float(r['inputWaitMaxMs']) for r in rows])
    axes[0].axhline(60,color='gray',ls='--',lw=.8);axes[0].set_ylabel('Actual decoded frames / 1 s');axes[0].legend(fontsize=8)
    axes[1].set_ylabel('FIFO head age at execution / ms');axes[1].set_xlabel('Seconds since first successful sender call (local monotonic)')
    for ax in axes:ax.grid(alpha=.2)
    fig.tight_layout();fig.savefig(OUT/'queue_and_new_frames.png',dpi=130);plt.close(fig)
    pieces=['''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>P8 实机交付</title>
<style>body{margin:0;background:#101820;color:#e3ecf3;font:16px/1.65 "Microsoft YaHei",sans-serif}main{max-width:1400px;margin:auto;padding:30px}a{color:#8bd9ff}h1,h2{line-height:1.3}section{margin:34px 0;padding-top:12px;border-top:1px solid #385060}.grid{display:grid;grid-template-columns:1fr 1fr;gap:18px}figure{margin:0;background:#1c2b36;padding:12px}img,video,svg{display:block;max-width:100%;height:auto;margin:auto}video{width:100%}figcaption{padding:8px 0;font-size:14px}table{border-collapse:collapse;width:100%;font-size:14px}th,td{border:1px solid #455b68;padding:8px;text-align:left}code{word-break:break-all}.notice{background:#293d49;padding:16px;border-left:4px solid #edbe76}.scroll{overflow:auto}.box{display:none}body.showboxes .box{display:block}@media(max-width:850px){.grid{grid-template-columns:1fr}main{padding:18px}}</style><main>
<h1>P8 DDS 60Hz、录像标注与清理</h1><p>正式 RK3588 → MPP → DDS → Windows 接收。800×800，双云、降水、录像与真实数据页同时开启。</p>
<div class="notice">NVG雨 / MWIR雪各10分钟：60.01 / 60.00 新图FPS，分别36300条输入和实际MP4帧完整对应。全程估计延时&lt;80ms未通过：启动段仍有尖峰。近云偏软、MWIR整体低对比和设备模型语义未标定均保留。</div>
<p><a href="../../../docs/HwaSimIR_P8_DDS_Performance_Annotation_Closeout.md">完整实施报告</a> · <a href="../../../docs/HwaSimIR_P8_Log_Retention.md">实际清理与保留</a> · <a href="performance_and_files.csv">性能/文件CSV</a> · <a href="upstream_conservation.csv">上游守恒</a> · <a href="annotation_scope_and_checks.csv">标注范围</a> · <a href="latency_validity_and_outliers.csv">延时及有效比例</a> · <a href="local_stage_timings.csv">本机阶段P95/P99</a></p>
<section><h2>普通启动的无标注原图与短片</h2><p>从各完整长测中间选取；解码MD5与该帧全片独立验证结果一致。没有测试相机、固定云位置或临时显示增益。</p><div class="grid">''']
    for band,label in [('nvg','NVG 默认 Auto / 雨128'),('mwir','MWIR 默认 Game / 雪128')]:
        pieces.append(figure(f'normal_{band}.png',label+'，源800×800'))
    pieces.append('</div><div class="grid">')
    for band in ['nvg','mwir']:pieces.append(f'<figure><video controls preload="metadata" src="normal_{band}_6s.mp4"></video><figcaption>{band.upper()} 实际长测片段 · <a href="normal_{band}.json">选帧身份</a></figcaption></figure>')
    pieces.append('</div></section><section><h2>两段完整录像与逐帧证据</h2>')
    for case in ['P8_final_NVG_rain600','P8_final_MWIR_snow600']:
        folder=next((BASE/case/'recording').iterdir());v=json.loads((folder/'validation.json').read_text())
        pieces.append(f'<p><b>{case}</b>：{v["frames"]}帧全量解码/SEI/正文/PTS通过；SHA256 <code>{v["movieSha256"]}</code><br>')
        for name,title in [('output.mp4','完整MP4'),('producer_annotations.jsonl','生产标注正文'),('annotations.txt','完整实时输入记录'),('frame_index.jsonl','逐帧索引'),('all_frame_correspondence.csv','逐帧独立对应表'),('validation.json','文件验证结果')]:pieces.append(f'<a href="{esc(rel(folder/name))}">{title}</a> · ')
        pieces.append(f'<a href="{esc(rel(BASE/case/"accepted_product_correspondence.csv"))}">接受输入→保存帧</a></p>')
    pieces.append('</section><section><h2>192 → 128 粒子，同波段固定显示对照</h2><p>同一相机和显示设置，动画使用真实协议仿真时间。精确指定帧另外按完整输入摘要/时间匹配，整段墙钟驱动短片并非逐时刻相同。原图无标注。</p>')
    for weather,label in [('rain','雨，NVG固定gain=8 / offset=-2.571'),('snow','雪，MWIR固定gain=1 / offset=0')]:
        pieces.append(f'<h3>{label}</h3><div class="grid">')
        for n in [192,128]:pieces.append(figure(f'{weather}_{n}.png',f'{n}粒子，同时间接收图'))
        pieces.append('</div><div class="grid">')
        for n in [192,128]:pieces.append(f'<figure><video controls preload="metadata" src="{weather}_{n}_6s.mp4"></video><figcaption>{n}粒子实际接收短片</figcaption></figure>')
        pieces.append('</div>')
    pieces.append('<p><a href="weather_pixel_comparisons.csv">同时间双路/逐云像素影响</a> · <a href="exact_weather_pixel_coverage.csv">完整输入匹配三路覆盖</a> · <a href="precipitation_coverage.csv">指定线性帧观察（含时间差，不作严格因果通过）</a></p><p>雨的三路参考为sourceSeq223；雪仅有初始化三路共同输入，稳定段三路零基线未测。稳定双云图像与性能使用上面的正式长测，不能用初始化图代替。</p><div class="grid">')
    pieces.extend([figure('rain_hide_first.png','诊断：隐藏第一朵稳定身份云'),figure('rain_hide_second.png','诊断：隐藏第二朵稳定身份云')])
    pieces.append('</div></section><section><h2>通用标注：改前/改后，独立普通几何体</h2><p>同一输入时刻。原图像素不改；勾选仅在网页叠加已保存的框，绿色为人工点。模型语义没有标定。</p><label><input type="checkbox" onchange="document.body.classList.toggle(\'showboxes\',this.checked)">显示保存的诊断框/人工点</label><div class="grid">')
    for label,title in [('before','改前 A4'),('after','改后 A6')]:
        body=json.loads((OUT/f'annotation_{label}_body.json').read_text());svg=f'<svg viewBox="0 0 800 800"><image href="annotation_{label}.png" width="800" height="800"/>'
        for obj in body['targets']:
            p=obj['bboxCorners'];x,y=p[0]['x'],p[0]['y'];w,h=p[2]['x']-x,p[2]['y']-y
            svg+=f'<rect class="box" x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="#ff6658" stroke-width="2"/>'
            for point in obj['keyPoints']:svg+=f'<circle class="box" cx="{point["x"]}" cy="{point["y"]}" r="4" fill="none" stroke="#69ff99" stroke-width="2"/>'
        pieces.append(f'<figure>{svg}</svg><figcaption>{title} · <a href="annotation_{label}.png">无标注原图</a> · <a href="annotation_{label}_body.json">保存正文</a></figcaption></figure>')
    pieces.append('</div></section><section><h2>真实界面与虚拟布局范围</h2><div class="grid">')
    pieces.append(figure('../P8_ordinary5_windows_A6_retry/receiver_ui.png','真实2560×1440实屏，DPR2；1平台/5目标字段全可见'))
    pieces.append(figure('../P8_virtual_4k_layout_unclamped/receiver_ui.png','3840×2160实际Qt客户区的虚拟布局；不是4K实屏'))
    pieces.append('</div><p><a href="../P8_virtual_4k_layout_unclamped/virtual_layout_validation.json">虚拟布局/DPI/物理像素说明</a> · <a href="../P8_ordinary5_windows_A6_retry/receiver_ui.png.layout.json">真实2K布局数据</a></p></section><section><h2>吞吐和FIFO队首年龄</h2>')
    pieces.append(figure('queue_and_new_frames.png','对照多个实施因素的整体结果；不把帧均值当画质，不省略启动尖峰。'))
    pieces.append('<p>正式NVG/MWIR全程估计P99：137.45/46.72ms；最大230.17/106.12ms；超80ms为525/12个。稳态新图吞吐通过，不宣称零排队或全程低于80ms。</p><p><a href="../deployment_integrity_final.log">正式+回滚全量校验</a> · <a href="../windows_release_final_verification.json">Windows整包校验</a> · <a href="../source_identity/manifest.json">实际源码身份</a> · <a href="recording_files.csv">录像文件及核验状态</a></p></section></main></html>')
    (OUT/'index.html').write_text(''.join(pieces),encoding='utf-8')
    print('Wrote local evidence page and measured queue plot.')
if __name__=='__main__':main()
