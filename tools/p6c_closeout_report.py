"""Build the display closeout report from captured evidence, not intended rates."""
from pathlib import Path
import json
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6c'
e=json.loads((OUT/'evidence.json').read_text());v=json.loads((OUT/'display_validation.json').read_text())
lines=['# P6C 公共显示定稿与实机验收','',
'本轮基线 `7a55fb545785a01951ad0c0af53ea263d5374206`，工作树修改未提交。实施前唯一未跟踪文件是用户的 P6C 指令附件，已保留原字节；源码清单、原状态与附件快照在 `logs/p6c/baseline/`。本报告只验收公共显示。独立二维人工实验见 [单独报告](HwaSimIR_Sensor_ImageLab_Validation.md)。','',
'## 修正的完成状态','',
'P6B 的 `normal_world_asset.png` 已经采用 Game；其偏暗不能全部归因于 Legacy 默认。本轮用普通数学测试输入定位显示链，并没有证明业务温度、材料、尾焰或绝对辐射链正确。P6B 已修好的低频 64×64 GPU 抽样继续保留，没有恢复高频整幅浮点读回。','',
'## 实际显示修改','',
'* 4096 个真实像元样本在固定 Gain/Offset 之后、gamma/极性/色调映射之前统计。去掉 CPU 统计中的 0–1 提前裁剪和 256 桶范围限制，改为浮点样本排序；保留负值和 HDR。分位索引严格定义为 `floor(p/100*(N-1)+0.5)`，是抽样总体的次序统计量。',
'* Auto 先限制正增益，再由受限增益计算偏移；否则窄输入范围的黑点会被错误放置。每波段偏移范围改为 ±32 个公共线性单位；3.00–3.05 的人工 HDR 平板实测需要约 −23.97 的偏移。超出范围仍受限，不声称任意动态范围均可展开。',
'* 样本 2%–98% 范围小于 0.02 时，向单位 AGC 映射平滑回归；不放大无差异输入。更新最高 5 Hz，指数平滑 α=0.15；首次有效统计直接初始化。RESET/INIT 清空增益、偏移和统计帧身份。',
'* 加入一个明确的 Reinhard 色调映射选项，使用 `z=max(x,0); z/(1+z)` 保留 HDR 亮部层次，然后只施加一次 gamma，最后施加极性。Legacy 的 LinearClamp 路径保留。最终片元与顶点坐标、采样器统一 highp；首个 mediump 候选出现两处 1 灰阶反向步进，已拒绝并修正。',
'* JSON 按明确路径解析、存储并消费。六个旧 INI AGC 映射常量改为过渡注释，避免覆盖每波段参数。P4 原生深度、缓冲路径及 P6B 世界云、纹理、模板身份均未修改。','',
'固定流程（白热）：`x=Gain*scene+OffsetGray/255`；Auto 启用时 `x=agcGain*x+agcOffset`；色调映射；`RGB=clamp(x,0,1)^(1/Gamma)`；黑热再作 `1-RGB`。动态 Auto 的映射随整幅样本变化；固定或冻结映射的正输入顺序必须保持。CPU 统计使用 `.299R+.587G+.114B` 的公共线性亮度，最终用同一仿射系数逐 RGB 通道映射；本轮为单色通用测试，没有扩大到彩色相机色度标定。','',
'## 默认与参数来源','',
'正式 NVG/NIR 默认由 Legacy 改为 **Auto**；MWIR 默认由 Legacy 改为 **Game**。版本 `P6C-display-release-1`。两者先经普通数值图、候选正常双云 60 Hz 验证，再修改 DefaultPreset；发布后重新验证无覆盖普通启动。NVG 的新 Game 候选会把正常云背景压到约 190–203 灰阶，画质未通过，已恢复该波段原 Game/Black 参数。MWIR 的固定 Game 提升暗部并保留 HDR 层次。Legacy 保留作固定诊断与恢复选项。','',
'| 参数路径（均在各自波段 JSON） | NVG 普通默认 Auto | MWIR 普通默认 Game | 消费与有效来源 |',
'|---|---|---|---|',
'| `HwaSimIR.Display.DefaultPreset` | Auto | Game | 普通 INIT 选择；日志 `source=band_profile` |',
'| `HwaSimIR.Display.Presets.<name>.Mode` | Auto | Fixed | 启用或关闭动态映射 |',
'| `.Gain / .OffsetGray` | 1 / 0 | 3 / 0 | 最终仿射各一次；OffsetGray 是灰阶单位 |',
'| `.Gamma / .WhiteHot / .ToneMap` | 2.2 / true / Reinhard | 2.2 / true / Reinhard | gamma 一次、末端极性、HDR 曲线 |',
'| `.Statistics.Method / Size / UpdateHz` | stratified_pixel_centers / 64 / 5 | 固定模式不统计 | Method 明确验证支持值；Size/Hz 消费于 GPU 小缓冲和低频调度 |',
'| `.Statistics.LowPercentile / HighPercentile` | 2 / 98 | 固定模式不消费 | 抽样排序索引，非完整图百分位 |',
'| `.Statistics.SmoothingAlpha / MinimumInputSpan` | 0.15 / 0.02 | 固定模式不消费 | 更新平滑、低对比回退 |',
'| `.Mapping.TargetLow / TargetHigh` | 0.05 / 0.95 | 固定模式不消费 | 色调映射前线性目标，并非最终 8 位百分比 |',
'| `.Mapping.MinGain / MaxGain` | 0.25 / 8 | 固定模式不消费 | AGC 增益限幅 |',
'| `.Mapping.MinOffset / MaxOffset` | −32 / 32 | 固定模式不消费 | 公共线性偏移限幅 |',
'| Legacy 预设 | Gain=1, Offset=0, Gamma=1, LinearClamp, 白热固定 | 同左 | 诊断恒等区间 0–1，区间外饱和；单调非递减 |',
'| `Systems.SensorConfigurationSystem.Width/Height/FOVH` | 仅接口几何无效时缺省 | 同左 | 有效 800×800 与 200 μrad 优先，未被 profile 640 宽覆盖 |','',
'旧兼容优先级仍是环境变量 > INI > 波段 profile。旧键冲突打印该字段唯一有效来源与 `legacyConflict=1`；本次普通回归无这些覆盖，`envOverrideCount=0`。固定模式的统计参数只保留解析与日志，不会执行统计。其余旧器件参数的解析/仅日志/未实现状态沿用 P6B，未因显示修改获得新的传感器功能。恢复 Legacy 可显式选择已有 `SensorWave.DisplayPreset=Legacy`，再重新初始化；不需要新覆盖开关。','',
'## 同序号通用测试证据','',
'所有图为已知数学场：灰阶、相邻平板、球形明暗、小高亮片、大面积亮区、HDR、常量与近常量。每组保存 `defined_linear.npy`、`linear.pfm`、实际 `display_parameters.json`、`linear_rgb8.png`（编码前）及 `received.png`（DDS 解码）。完整 PFM 只在显式测试入口指定序号导出；它是公共缩放线性值，没有绝对光谱单位。','',
'| 实测测试 | 定义→显示前最大差 | 映射公式→GPU MAE（灰阶） | GPU→DDS MAE（灰阶） | GPU 固定/当帧映射反向步数 |',
'|---|---:|---:|---:|---:|']
for name in ('before_b2_Game','precision_b2_Game','hdr_Game','hdr_Auto','flat_Auto','nearflat_Auto','offset_domain_7','offset_domain_8','polarity_Black'):
    if name not in e['displays'] or name not in v:continue
    a=e['displays'][name];b=v[name]
    rev=sum(b.get('fixed_or_frozen_row_inversions',{}).values())
    lines.append(f"| {name} | {b['definition_to_float_max']:.8g} | {a['prediction_mae_gray8']:.4f} | {a['decoded_mae_gray8']:.4f} | {rev} |")
lines+=['',
'定位结论：0.25–0.2501 的近常量输入在 RGB16F 存储后已全部变成 0.25，显示不能恢复这些差异；普通 HDR 0–4 在新 Game 中不提前裁到 1。小面积亮区低于高百分位阈值时，不改变 Auto 背景映射；大面积亮区改变整幅统计，背景随之调整，冻结映射对照保持不变。这是动态统计行为，不能作为固定映射错误。',
'64×64 精确选择每层中心像元，实测抽样值与原图对应像元最大差为 0。周期细线反例中，完整图 98% 分位是 2、最大值 8，而抽样全部约为 0.1：**固定格点可能漏掉小亮片和周期结构，不能宣称完整图百分位等价**。Auto 默认是明确的抽样显示策略，不是计量输出；要求稳定比较时选择固定 Legacy 或 MWIR Game。','',
'当前 MPP RGB/BGR24 路径按 BT.601 整数式映射为有限范围 Y（约 16–235），UV 中性约 128；接收端 FFmpeg 转回 RGB24。实测灰阶误差小于 1 个灰阶的均值量级，没有出现整幅 16–235 当作 0–255 的压缩。码流未明确标注 color_range/color_space，现有 Windows 解码路径通过，但本轮不声称任意第三方播放器的色彩元数据一致性。旧未选用的 Gray8 编码分支不是本轮验收对象。损耗编码可能产生局部 1 灰阶反向步进（HDR Auto 接收梯度出现 1 处），因此 GPU 单调性与 DDS 有损误差分别报告。','',
'## 图像与视频','',
'[旧 Game / 新 MWIR Game 同输入图](../logs/p6c/delivery/display_game_before_after.png) · [同输入短视频](../logs/p6c/delivery/display_game_before_after.mp4) · [动态 Auto / 冻结 Auto 图](../logs/p6c/delivery/auto_dynamic_frozen.png) · [动态/冻结短视频](../logs/p6c/delivery/auto_dynamic_frozen.mp4) · [小亮区/大亮区](../logs/p6c/delivery/auto_small_large.png) · [数学曲线与实际适应过程](../logs/p6c/delivery/mapping_and_adaptation.png)。','',
'无标注普通启动：[NVG 原图](../logs/p6c/delivery/normal_nvg_default.png) / [NVG 实际接收视频](../logs/p6c/delivery/normal_nvg_default.mp4)；[MWIR 原图](../logs/p6c/delivery/normal_mwir_default.png) / [MWIR 实际接收视频](../logs/p6c/delivery/normal_mwir_default.mp4)。独立图原字节复制；独立 MP4 仅封装实际 DDS H.264；并排视频单独标为重编码，标签在原画面外。来源与 SHA-256 见 [交付清单](../logs/p6c/delivery/manifest.json)。','',
'画质判断：普通灰階暗阶、平板相邻差异、球形明暗和 HDR 高亮层次均能检查；MWIR 云和背景比旧固定映射更明亮，NVG Auto 显出了云内结构。云仍偏软，NVG 中继承的内部暗纹变得明显；本轮没有改云的密度或有限步数积分，不能宣布云画质已完全完成。提升显示可见性不提供真实目标温度、材料、尾焰或器件标定证据。','',
'## 正常启动与实机稳定性','',
'测试入口与普通结果明确分离：`P6Scene=display`、`P6CDisplayCase`、映射冻结和浮点导出仅用于普通数学测试图。正式回归使用 `run_precise.sh`，清空所有临时渲染环境覆盖；相机经既有正常协议输入，800×800、200 μrad、原生约 9.17° 视场。云仍由 IRWorldCloudStreaming 正常网格生成，既有自然同屏双云，未摆固定测试云或注入 35° 相机。普通场景没有业务目标注入；P6B 的资产与尾焰工作保持原状。','',
'正常默认关闭渲染 Perf 日志；下面使用 Windows 实际接收/显示的区间速率，不把请求频率或捕获 MP4 的帧率标签当实测。长测保留所有输入及视频/元数据/标注计数，短视频只保存前 10 秒。GUI 首尾局部窗口被剔除，中间停顿仍保留。暂停与重初始化组有意暂停，不能用其全组均值评价稳态。','',
'| 普通发布回归 | 实际对应输入数 | 接收 FPS 均值 | 显示 FPS 均值 | 云可见提交上限 | 完整守恒 |',
'|---|---:|---:|---:|---:|---|']
for name,a in e['cases'].items():
    if not name.startswith('release_'):continue
    count=sum(int(x.get('acceptedRealtime',0)) for x in a['counts']['rounds'])
    t=a['timing'];rx=t.get('receiver_receiveFps',{}).get('window_mean');gui=t.get('receiver_displayFps',{}).get('window_mean')
    if 'pause_init' in name:rx=gui=None
    fmt=lambda n:'—' if n is None else f'{n:.3f}'
    lines.append(f"| {name} | {count} | {fmt(rx)} | {fmt(gui)} | {a['visible_max']} | {'通过' if a['conservation_pass'] else '未通过/尚未完成'} |")
lines+=['',
'守恒验收要求：发送成功输入=板端接收=有序消费=捕获=视频发布=Windows 接收；每轮元数据/标注与视频数相等；STOP 后队列归零，旧帧发布、序号缺口、覆盖、溢出、DDS 写错和关联错误为 0。两次 INIT 各从新序号开始，暂停不补发突发输入。普通长测的双云提交与实际图像同时保留；逐 cloudId 像素贡献沿用 P6B 的逐云隐藏证据，云实现和数据哈希未变。完整细项见 [机器可读证据](../logs/p6c/evidence.json)。','',
'同输入 20 Hz、5 秒普通图（双方均含一次指定序号 PFM 诊断）耗时对照：Game 的 renderMs 区间均值由 24.995 到 23.346 ms；Auto 由 24.475 到 24.118 ms；Auto 统计每帧摊销均值由 0.117 到 0.158 ms。只有各两个日志窗口，包含诊断与冷启动影响，不能据此宣称曲线更快或代表稳态 60 Hz 预算。Windows 独立 GPU 测试的映射公式 MAE 为 Game 0.301、Auto 0.227 灰阶；Windows x264 编码后的误差约 2.75–2.77 灰阶，与板端 MPP 单列。','',
'候选同质量双云 60 Hz（无完整 PFM）：MWIR Game 渲染区间平均 59.983 FPS；NVG Auto 60.004 FPS。NVG 每次 4096 点读回/转换/排序平均 0.698 ms、最大 0.884 ms（261 次去重更新）；GPU 小缓冲绘制包含在渲染时长内，不能再称为完整统计 GPU 查询耗时或每帧耗时。P6B 4096 点旧桶统计平均约 0.516 ms，当前增加的浮点排序避免 HDR 域错误；两组为不同测试时段，不能据差值声称严格微基准。原整幅约 51–57 ms 的代价没有重新成为默认路径。跨机时钟未校准，不报告跨机端到端时延。','',
'## 部署和回滚身份','',
'最终 ELF SHA-256 `fe42e0f75e15484c6464b4bd6d4191b7903ee5e63d1ea845b606cae7cc602373`，Build ID `11bb36e910fd2af943e9346ffbe10fb5b66aadb8`；完整 Config manifest SHA-256 `6e2206cd8a3867a9403384f148ba2b06e48b6ebb44190ac47eddba1ffee44264`；运行 INI SHA-256 `b7c242de57550aeb3aae5581882c30228a3bc2eb61b55a3d1888fd2acee3c1cf`。','',
'使用现有原子部署器：先校验旧完整 Config，新版本通过独立文件加 rename 替换变化文件，保留未变的硬链接资源；核对约 5.75 GB 全部配置与缓存、ELF、两个启动/性能脚本，停进程成套切换后再次完整校验。正常启动仍只执行快速明确资源检查。最终备份时间戳 `20260913-123927`，目录 `/userdata/HwaSimIR/Config.before_20260913-123927`；对应 ELF、run_precise.sh 与性能脚本同后缀。部署原始证据见 [日志](../logs/p6c/deploy_final.log) 及 [版本](../logs/p6c/deploy_final/stage/Config/deployment_version.env)。独立传感器程序未部署进生产启动或 SensorWave。','',
'源码、原用户修改和最终交付文件校验见 `logs/p6c/final_integrity.json`；回滚快照检查见 `logs/p6c/rollback_verify.log`：本轮前 P6B、最终切换前候选和当前发布的完整资源、ELF、启动器身份均通过；修改的 profile 使用独立 inode。这里只验收备份完整性，没有切回旧版运行。保留回滚文件，禁止原地编辑共享 inode；恢复时必须先停进程，复制到独立候选目录并校验，再成套切换。没有修改系统时钟、系统网络、许可证、DDS 协议、P3 性能策略、业务目标参数或控制链。','',
'## 分开验收','',
'* **代码与数值**：Windows/aarch64 编译、严格 JSON 路径与非法配置隔离、有效 800×800 几何、同输入定义/浮点/GPU/DDS 对照通过；固定 GPU 单调。首个 mediump 候选与 NVG Game 外观候选未通过，保留失败证据并替换。',
'* **实机稳定性**：以上普通发布表与完整守恒结果为准；Auto 小尺寸统计继续保持实时路径，完整 PFM 仅作诊断。日志的可见数量与 FPS 均不能单独构成像素或画质证明。',
'* **图像效果**：公共显示的暗部层次与 HDR 呈现得到改善；动态 Auto 的适应行为、固定采样漏检、RGB16F 极小差异损失和继承云暗纹均明确保留。未验收真实目标物理正确性、彩色相机或真实器件标定。','',
'人工二维模型的数值、运行、图像与真实器件缺口在单独报告中验收。P6C 到此结束，不向生产目标链导入人工传感器参数。']
(ROOT/'docs/HwaSimIR_P6C_Display_Closeout.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
print('Display closeout report written from current captured evidence')
