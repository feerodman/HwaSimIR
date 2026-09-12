"""Create a local review page from actual DDS evidence; never alters the frames."""
import html
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / 'logs' / 'p6'
data = json.loads((root / 'evidence.json').read_text(encoding='utf8'))
sections = [
    ('目标、尾焰和双云', ['rk_existing_targets_20', 'rk_existing_targets_30', 'rk_existing_targets_60']),
    ('不同模型喷口', ['rk_nozzle_f35_oblique', 'rk_nozzle_f22_end', 'rk_nozzle_aim120_oblique', 'rk_nozzle_aim9x_oblique']),
    ('喷口挂点对照', ['rk_nozzle_f35_before', 'rk_nozzle_f35_oblique']),
    ('普通喷口同显示设置对照', ['rk_plume_before_orbit', 'rk_plume_smoke2_orbit', 'rk_plume_before_side', 'rk_plume_smoke2_side']),
    ('多云数量', ['rk_release2_cloud1', 'rk_release2_cloud2', 'rk_release2_cloud4']),
    ('环境美术同显示设置对照', ['rk_release_legacy_art', 'rk_release2_cloud2']),
    ('最终版本云的独立像素贡献', ['rk_release_two_pixels', 'rk_release_two_off1', 'rk_release_two_off2']),
    ('空间关系', ['rk_cloud_overlap', 'rk_release_translate', 'rk_release_entry', 'rk_cloud_occluded', 'rk_cloud_behind']),
    ('原尺寸长测', ['rk_mixed_smoke2_60s', 'rk_mixed_smoke2_600s']),
    ('通用显示', ['rk_display_Legacy', 'rk_display_Game', 'rk_display_Black', 'rk_display_gain05', 'rk_display_NIR', 'rk_display_Auto_status_final']),
]
parts = ['''<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>P6A 实机图像验收</title>
<style>body{margin:0;background:#14191c;color:#e1e8e8;font:16px/1.65 "Microsoft YaHei",sans-serif}main{max-width:1400px;margin:auto;padding:24px}h1{font-size:28px}h2{margin-top:36px;font-size:22px}p{max-width:1100px}a{color:#8de0bf}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(290px,1fr));gap:20px}.card{background:#20282c;border:1px solid #374448;border-radius:8px;overflow:hidden}.card img,.card video{display:block;width:100%;aspect-ratio:1;object-fit:contain;background:#111}.info{padding:12px}code{overflow-wrap:anywhere;font-size:13px}.ui{max-width:100%;height:auto}.bad{color:#ffbe9c}.good{color:#8de0bf}small{color:#b7c1c5}</style>
<main><h1>P6A 实机图像验收</h1><p>无标注图片和短片来自 RK3588 → MPP → DDS → Windows。MP4 仅封装原接收 H264；播放标称帧率来自请求值，实测帧率见下方日志统计。长测短片只保留前 10 秒，600 秒结论来自完整计数。</p><p>选定普通双云档。四云未通过稳定 60 Hz；8 步云边缘仍可见采样纹理。艺术效果已实现，待用户确认。新云和普通喷口美术仅在 P6 测试入口生效；每模型喷口挂点、配置解析和接收端 UI 已进入正常运行。</p>''']
for title, names in sections:
    parts.append(f'<h2>{html.escape(title)}</h2><div class="grid">')
    for name in names:
        folder = root / name
        if not (folder / 'received.png').exists():
            continue
        case = data['cases'].get(name, {})
        timing = case.get('timing', {}).get('renderFps', {}).get('window_mean')
        perf = f'{timing:.2f} FPS' if timing is not None else '未记录稳态 FPS'
        conserved = case.get('conservation_pass', False)
        status = '全量计数闭合' if conserved else '计数未闭合／未完成'
        css = 'good' if conserved else 'bad'
        parts.append(f'<article class="card"><a href="{name}/received.png"><img loading="lazy" src="{name}/received.png" alt="{name} 无标注实际接收图"></a><div class="info"><code>{name}</code><br>{perf} · <span class="{css}">{status}</span><br><a href="{name}/board.log">板端日志</a> · <a href="{name}/request.json">测试条件</a>')
        if name in ['rk_release2_cloud4', 'rk_display_Auto_status_final']:
            parts.append('<p class="bad">请求帧率的性能验收未通过；数量闭合不代表达到请求速率。</p>')
        release = case.get('release', '').strip()
        if release:
            parts.append(f'<details><summary>该次程序与配置版本</summary><small><code>{html.escape(release)}</code></small></details>')
        if (folder / 'received.mp4').exists():
            parts.append(f'<details><summary>播放原接收短片</summary><video controls preload="none" src="{name}/received.mp4"></video></details>')
        parts.append('</div></article>')
    parts.append('</div>')
parts.append('<h2>改前 / 改后接收图对照</h2><div class="grid">')
for name, label in [('rk_environment_comparison.png', '同 Game 显示：环境美术'),
                    ('rk_plume_smoke2_comparison.png', '同 Game 显示：普通喷口美术'),
                    ('rk_nozzle_attachment_comparison.png', '同显示设置：模型喷口挂点'),
                    ('rk_legacy_migration_comparison.png', 'Legacy 显示迁移兼容性')]:
    if (root / name).exists():
        parts.append(f'<article class="card"><a href="{name}"><img loading="lazy" src="{name}" alt="{label}"></a><div class="info">{label}；标签仅在对照拼图，原图未加标注。</div></article>')
parts.append('</div>')
ui = root / 'rk_nozzle_f35_oblique/ui_800x600_tab1.png'
if not ui.exists():
    ui = root / 'win_nozzle_f35/ui_800x600_tab1.png'
if ui.exists():
    path = ui.relative_to(root).as_posix()
    parts.append(f'<h2>800×600 界面：平台与目标同页、实时 FPS</h2><p><code>{path}</code></p><p>FPS 统计最近 1 秒解码图像的 GUI 更新。截图期间缩放窗口会短时积压后集中更新，瞬时标签可超过请求 Hz；性能结论使用独立稳态场景。</p><a href="{path}"><img class="ui" src="{path}" alt="实际接收界面"></a>')
parts.append('<p><a href="../../docs/HwaSimIR_P6A_Display_Profiles_MultiCloud_GameVFX_Implementation.md">实施与验收记录</a> · <a href="evidence.json">完整机器可读证据</a> · <a href="source_manifest.json">源码版本清单</a> · <a href="migration_evidence.json">Legacy 迁移对照</a></p></main></html>')
(root / 'Review.html').write_text(''.join(parts), encoding='utf8')
print(root / 'Review.html')
