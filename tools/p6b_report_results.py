"""Render the evidence table into the implementation report, preserving prose."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p6b'
e=json.loads((OUT/'evidence.json').read_text(encoding='utf8'))
p=ROOT/'docs/HwaSimIR_P6B_WorldCloud_Display_SensorReadiness.md'
s=p.read_text(encoding='utf8')
lines=['## 实机版本、回归及图像','',
    '代码正确性：Windows MSVC 构建与 aarch64 交叉构建通过；共享世界描述、JSON 路径读取、显示数值与原生深度分别验证。所有通过项都同时检查视频、元数据、标注、队列清空和旧帧拒发；保留早期失败日志，没有混入通过项。', '',
    '以下主要为 MWIR、原尺寸 800×800。FPS 是日志区间平均值再取均值，省略首末活动窗口，不是 GPU 查询或逐帧 P95。暂停/重新初始化项包含主动空闲，故不拿其平均 FPS 评吞吐。冷启动及诊断窗口会低于 60，未宣称每帧硬实时截止期。', '',
    '| 实测场景 | 接收对应输入数 | 帧率均值 | 输入/视频/元数据/标注守恒 |',
    '|---|---:|---:|---|']
cases=[('正常世界＋资产尾焰＋双云，60 秒','rk_release_normal_asset'),
       ('Auto 双云，无 PFM，60 秒','rk_release_auto60_runtime'),
       ('正常世界＋资产尾焰＋双云，600 秒，INI 加载中心','rk_release_normal_600s'),
       ('分离视角，8 秒','rk_release_separated'),('横移/进出视野，24 秒','rk_release_pan'),
       ('云前普通平板遮挡','rk_release_plate'),
       ('20 Hz 暂停＋重初始化','rk_release_sync_20'),('30 Hz 暂停＋重初始化','rk_release_sync_30'),('60 Hz 暂停＋重初始化','rk_release_sync_60'),
       ('默认 MWIR / 默认 Weather，8 秒，显式 PFM 诊断','rk_deployed_runtime'),('默认 NVG / 默认 Weather，6 秒，显式 PFM 诊断','rk_deployed_nvg')]
for label,key in cases:
    c=e['cases'].get(key)
    if not c or not c.get('counts',{}).get('rounds'):
        lines.append(f'| {label} | 待完成 | — | 待完成 |');continue
    counts='+'.join(x['acceptedRealtime'] for x in c['counts']['rounds'])
    fps=c['timing'].get('renderFps',{}).get('window_mean')
    value='—' if 'sync_' in key or fps is None else f'{fps:.3f}'
    lines.append(f"| {label} | {counts} | {value} | {'通过' if c['conservation_pass'] else '未通过，查原始日志'} |")
lines+=['','加载/剔除日志中的 visible 表示提交绘制选择，并不等于像素贡献。最终版本逐云隐藏的控制对照（cloudId 和其他云保持不变）为：','']
for name,data in e['cloud_contributions'].items():
    v=data['linear_rgb8.png'];lines.append(f"* {name}：编码前至少变化 2 灰阶的像素 {v['changed_pixels_ge2']}，占 {v['fraction_ge2']*100:.2f}%；DDS 原图也有相应变化。")
lines+=['','固定映射图表 Legacy/Game/Black 的反向步数均为 0，预编码预测 MAE 约 0.19–0.30 灰阶。小高亮片线性输入前后完全相同（1.5，1600 像素），编码前从 149 变为 182，非高亮像素差为 0；DDS 均值约从 148.98 变为 181。详见 `hdr_order_comparison.json`。','',
    '同一 20 Hz 双云输入、相同低频统计策略、无完整 PFM 诊断：一次抽样读取/转换/直方图平均 **0.516 ms**（4096 点，23 个去重更新）；旧全图参考平均 **56.768 ms**（640000 点，25 个更新）。这是每次统计调用耗时，GPU 抽样绘制计入渲染耗时，不能与每帧摊销数混称。`auto_statistics_update_cost.json` 保存原始摘要；最终 Auto 60 Hz 无 PFM 测试是实时使用资格依据，默认仍是 Legacy。','',
    '图像效果：原有周期层纹减轻，云轮廓更加不规则；Auto 下最明显的片云细条通过掠视淡化减轻。固定区域的 9 行低通残差均值由 1.958 降到 0.621 灰阶，P99 由 17.00 降到 2.67；该指标仅辅助定位片云细线，不是通用画质分数。有限步数体积仍偏软，近景内层不是完整散射模拟。俯视分离场景中片云保留，局部云与片云共存；云前平板遮挡经过实际接收图核验。','',
    '部署后的默认图也单独交付：Legacy MWIR 画面明显偏暗，NVG 的云与背景对比偏低，不能称为理想画质。依照本轮要求没有暗中更换默认显示，Game/Auto 图明确标为主动选择的预设。两组默认启动短测包含完整 PFM 诊断和冷启动，不能据其均值推断稳态 60 Hz 性能。','',
    '四云单独结果：正常同一视角把预算改为 4 仍只选到 2 朵，不能据此宣布正常世界四云通过。另一个固定 P6 测试入口短测实际选择 4 朵，约 59.91 FPS、10 秒输出守恒，但不是正常世界四云长测与画质验收。本轮发布双云预算，不发布四云质量保证。','',
    '失败与边界：早期屏幕相位造成斜向颗粒，已拒绝；一次 GLES 矩阵精度不匹配使该组图形验收失败，已修复；UV 高精度单独未消除片云细线，真正有效的后续改动是掠视淡化。原有资产导入的两条三角化告警仍存在，未通过重做资产或改变材料掩盖。没有重新引入 GL 0x506 路径。','',
    '无标注实际接收图及视频集中在 `logs/p6b/delivery/`，`manifest.json` 记录来源、操作及 SHA-256。独立原图为收到的像素原样复制；比较图只拼接并在图外标说明。MP4 独立视频从实际 DDS H.264 无损封装；并排比较视频单独标为重编码。','',
    '* [正常世界 Game 显示原图](../logs/p6b/delivery/normal_world_asset.png) / [尾焰与双云短视频](../logs/p6b/delivery/normal_world_asset.mp4)',
    '* [Auto 原图](../logs/p6b/delivery/normal_world_auto.png) / [24 秒相机横移与进出视野](../logs/p6b/delivery/normal_world_pan.mp4)',
    '* [片云细条改前/改后](../logs/p6b/delivery/sheet_before_after.png) / [同显示设置对照视频](../logs/p6b/delivery/sheet_before_after.mp4)',
    '* [旧公式参考/新云算法](../logs/p6b/delivery/cloud_algorithm_comparison.png) / [算法对照视频](../logs/p6b/delivery/cloud_algorithm_comparison.mp4)',
    '* [普通平板遮挡](../logs/p6b/delivery/ordinary_plate_occlusion.png) / [分离视角](../logs/p6b/delivery/separated_world_view.png)',
    '* [高亮映射顺序对照](../logs/p6b/delivery/display_highlight_order.png)',
    '* [部署后默认 MWIR 原图](../logs/p6b/delivery/deployed_default_mwir.png) / [实际接收视频](../logs/p6b/delivery/deployed_default_mwir.mp4)；[默认 NVG 原图](../logs/p6b/delivery/deployed_default_nvg.png) / [实际接收视频](../logs/p6b/delivery/deployed_default_nvg.mp4)',
    '', '真实两实例最终构建对照见 `world_two_release_instances.json`；最终 Windows/aarch64 正常渲染对照见 `world_render_release_windows_arm.json`，8 个共享描述最大数值差 `1.14e-13`。全部计数、耗时和编解码差异见 `logs/p6b/evidence.json`。', '']
start=s.index('## 实机版本、回归及图像');end=s.index('## 部署与回滚',start)
p.write_text(s[:start]+'\n'.join(lines)+'\n'+s[end:],encoding='utf8')
print(p)
