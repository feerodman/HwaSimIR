"""Local review page with untouched DDS images and copy-remuxed videos."""
from pathlib import Path
import json,html,hashlib,zipfile
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    evidence=json.loads((OUT/'evidence.json').read_text())
    blocks=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">',
    '<title>P6D 世界云实机验收</title><style>body{font:16px/1.6 system-ui,sans-serif;margin:24px auto;padding:0 20px;max-width:1400px;background:#16191d;color:#eee}a{color:#9dd4ff}h1,h2{font-weight:600}section{margin:36px 0}table{border-collapse:collapse;width:100%;font-size:14px}td,th{padding:9px;border-bottom:1px solid #41474f;text-align:left}.pair,.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:18px}figure{margin:0 0 24px}img,video{display:block;width:100%;height:auto;background:#080808}figcaption{padding:8px 0;color:#ccc}code{word-break:break-all}.note{padding:16px;background:#252b32;border-left:4px solid #b9d5e8}@media(max-width:800px){.pair,.grid{grid-template-columns:1fr}}</style>',
    '<h1>P6D 正常世界云：实机图像与验证</h1><p class="note">原图为实际 RK3588 → MPP → DDS → Windows 接收图，无画内标注。MP4 由原始接收 H.264 直接封装，没有补帧。PFM 是公共缩放线性值，不是原始物理辐亮度。请分别查看数值正确性、实际吞吐和图像效果；近景柔软程度与代理深度近似仍有明确限制。</p>',
    '<p><a href="../../docs/HwaSimIR_P6D_Cloud_Compositing_Visual_Closeout.md">完整报告</a> · <a href="cause_matrix.csv">局部原因矩阵</a> · <a href="evidence.json">原始计数及耗时</a> · <a href="numeric_blend.json">GPU 数值合成</a> · <a href="zero_density_board_closed.json">零密度等价</a> · <a href="world_windows_arm.json">世界描述对照</a></p>']
    delivered=[]
    def figure(case,label,video=False):
        p=OUT/case/('received.mp4' if video else 'received.png')
        if not p.exists():return '<p>该项尚未生成：'+html.escape(case)+'</p>'
        delivered.append(dict(case=case,path=str(p.relative_to(ROOT)).replace('\\','/'),sha256=sha(p),kind='actual_DDS_mp4_copy_remux' if video else 'actual_DDS_unannotated_png'))
        url=case+'/'+p.name;tag=f'<video controls preload="metadata" src="{url}"></video>' if video else f'<a href="{url}"><img loading="lazy" src="{url}"></a>'
        return f'<figure>{tag}<figcaption>{html.escape(label)} — <a href="{case}/request.json">输入定义</a></figcaption></figure>'
    for band,name in [(1,'NVG'),(2,'MWIR')]:
        blocks.append(f'<section><h2>{name}：同帧、同冻结显示的算法对照</h2><div class="pair">')
        blocks.append(figure(f'isolate_b{band}_base','改前：P6B 挤出密度 / PNG 人工明暗'))
        blocks.append(figure(f'visual_b{band}_base','改后：三维云团 / 局部人工明暗 / 无圆形挖空'))
        blocks.append(f'</div><p>输入序号 90；普通相机读取 pair_mid.json；原 PNG 哈希与世界身份保持。此处为冻结显示的诊断比较。正式默认显示见下图。 <a href="comparison_b{band}.png">全尺寸对照图</a> · <a href="defect_comparison_b{band}.png">原黑纹区域裁剪对照</a></p>')
        blocks.append(figure(f'release_b{band}_60hz_60s',f'{name} 无临时覆盖的普通启动原图'))
        blocks.append('<div class="pair">'+figure(f'before_pan_b{band}','改前移动视频',True)+figure(f'visual_b{band}_pair_pan','改后，同冻结映射与同输入路径',True)+'</div>')
        blocks.append('<h2>分离、重叠、距离与遮挡</h2><div class="grid">')
        for view,label in [('near','单云中景（仅诊断隐藏另一 cloudId）'),('pair_separated','两云分离，与片云共存'),('pair_overlap','两云重叠'),('pair_far','远景'),('plate_front','普通前置平板，固定 Game 显示'),('plate_back','普通后置平板，固定 Game 显示')]:
            prefix='default' if band==1 and view in ('pair_separated','pair_overlap','pair_far') else 'visual'
            blocks.append(figure(f'{prefix}_b{band}_{view}',label))
        prefix='default' if band==1 else 'visual'
        blocks.append('</div><div class="pair">'+figure(f'{prefix}_b{band}_near_translate','近景横移；正式波段默认显示',True)+figure(f'{prefix}_b{band}_proxy_crossing','穿过代理边界；不等同于精确体积遮挡',True)+'</div></section>')
        if band==1:
            blocks.append('<p class="note">NVG 穿云场景画质未通过：动态 Auto 仍有明显明暗跳变和低值裁黑。<a href="default_b1_proxy_crossing_transitions.png">最大变化的相邻接收帧</a> · <a href="boundary_display_isolation.json">固定 8 步的冻结／动态隔离</a>。中景原黑带消除不代表所有距离画质通过。</p>')
            blocks.append('<div class="pair">'+figure('boundary_b1_auto8','固定 8 步，动态 Auto，序号 262')+figure('boundary_b1_frozen8','固定 8 步，冻结 Auto，序号 262')+'</div>')
            blocks.append('<div class="pair">'+figure('boundary_b1_auto8','固定 8 步，动态显示穿越视频',True)+figure('boundary_b1_frozen8','固定 8 步，冻结显示穿越视频',True)+'</div>')
    blocks.append('<section><h2>派生资源与局部原因证据</h2><p><a href="source/final_slices.png">实际 PNG、透明通道、改前改后体密度切片</a> · <a href="actual_cloud_pixel_impact.json">逐个移除 cloudId 的像素影响</a> · <a href="source_final.json">源码与资源版本</a></p></section>')
    blocks.append('<section><h2>普通启动回归结果</h2><p>FPS 为 Windows 实际解码后 GUI 更新的约 1 秒窗口。仅去掉首尾活动边界窗口，暂停和初始化之间的内部零窗口保留。CPU do_frame 含 GPU 等待，不是 GPU query，也不是 P95。NVG 长测有有界输入背压，具体计数与余量限制见报告。</p><table><tr><th>场景</th><th>输入 / 视频</th><th>对应关系</th><th>GUI FPS 均值 / 最低</th><th>CPU 调用均值 ms</th><th>温度峰值 °C</th></tr>')
    for name,c in evidence['cases'].items():
        if not name.startswith('release_'):continue
        counts=c['counts'];total=sum(int(x.get('acceptedRealtime',0)) for x in counts['rounds']);video=counts['sender'].get('sentSamples','?')
        rate=c['timing'].get('receiver_gui_fps',{});audit=c.get('cpu_render_call_audit',[]);cpu='/'.join(x.get('meanMs','?') for x in audit)
        blocks.append(f'<tr><td>{html.escape(name)}</td><td>{total} / {video}</td><td>{"通过" if c["conservation_pass"] else "待核查/未通过"}</td><td>{rate.get("window_mean",0):.2f} / {rate.get("window_min",0):.2f}</td><td>{cpu}</td><td>{c.get("thermal_peak_c")}</td></tr>')
    blocks.append('</table></section><p>四云、多重散射、全地形阴影、真实目标拟合和生产传感器接入均不在本轮交付中。</p></html>')
    (OUT/'Review.html').write_text('\n'.join(blocks),encoding='utf8')
    (OUT/'delivery_manifest.json').write_text(json.dumps(delivered,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
    # Keep the same relative layout so the review page also works after unzip.
    paths={ROOT/item['path'] for item in delivered}
    for item in delivered:paths.add(ROOT/item['path'].rsplit('/',1)[0]/'request.json')
    # Include the concrete camera inputs and raw diagnostic layers, not only
    # rendered previews. Exclude abandoned trial directories from this bundle.
    case_dirs={OUT/item['case'] for item in delivered}
    case_dirs.update(p for p in OUT.iterdir() if p.is_dir() and p.name.startswith(('isolate_','verified_','visual_','default_b1_','mapping_','boundary_','release_')) and 'failed' not in p.name)
    case_dirs={p for p in case_dirs if not(p.name.startswith('isolate_') and (OUT/p.name.replace('isolate_','verified_',1)/'linear.pfm').exists())}
    for folder in case_dirs:
        for name in ('request.json','effective_environment.json','release.sha256','board.log','linear.pfm','linear_rgb8.png','received.png','stim.err.log','video.err.log'):
            p=folder/name
            if p.exists():paths.add(p)
        request=folder/'request.json'
        if request.exists():
            camera=json.loads(request.read_text(encoding='utf-8-sig')).get('cameraInput')
            if camera:
                p=Path(camera);p=p if p.is_absolute() else ROOT/p
                assert p.resolve().is_relative_to(ROOT.resolve())
                paths.add(p)
    for name in ('Review.html','delivery_manifest.json','cause_matrix.csv','evidence.json','numeric_blend.json','zero_density_board_closed.json','world_windows_arm.json','world_actual_return.json','world_actual_windows_arm.json','actual_cloud_pixel_impact.json','final_compositing_validation.json','final_resource_validation.json','source_final.json','source_final.patch','source_changes.zip','rollback_verification.log','release_summary.json','video_review_manifest.json','video_transition_review.json','comparison_b1.png','comparison_b2.png','defect_comparison_b1.png','defect_comparison_b2.png','source/final_slices.png','source/build_evidence.json','deploy_budget/deployment_final.txt'):
        paths.add(OUT/name)
    paths.add(ROOT/'docs/HwaSimIR_P6D_Cloud_Compositing_Visual_Closeout.md')
    paths.add(OUT/'boundary_display_isolation.json')
    paths.add(OUT/'boundary_display_comparison.png')
    for p in OUT.glob('*_video_review.png'):paths.add(p)
    for p in OUT.glob('*_transitions.png'):paths.add(p)
    assert all(p.exists() for p in paths),'incomplete review package'
    with zipfile.ZipFile(OUT/'P6D_delivery.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
        for p in sorted(paths):z.write(p,str(p.relative_to(ROOT)))
    print('Review items:',len(delivered))
if __name__=='__main__':main()
