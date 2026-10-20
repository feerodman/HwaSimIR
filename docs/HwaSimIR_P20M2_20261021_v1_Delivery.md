# P20M2 20261021_v1：执行记录与未完成项

**结论：INCOMPLETE_NOT_DELIVERED。本轮实际修改源码、运行官方 MODTRAN、复核 raw、构建 Windows/aarch64，并在 RK3588 上执行了单元验证；尚未交付新全域正式数据，未部署新成像候选，也未完成新数据 COMPLETE 路径的双路 60 FPS 验收。**

这份文件不是全域完成声明，也不是新的 P14 代码验证包。当前还有实现和物理验证工作；不能把原因写成板卡不可达或权限不足，也尚未证明 MODTRAN 5.2.1 整体无解。

## 工作身份与证据

- 工作目录：`D:\HwaSimIR`，起始 HEAD：`d5c1e07097a921c718856e1d3ec056650c46d386`。
- 用户已有的 P20M2 详细任务文档保留；没有 reset、clean 或丢弃用户修改。
- 新证据目录：`logs/p20m2/20261021_v1`。
- 官方程序：`F:\Programs\PcModWin5\Bin\Mod5.2.1.0.exe`。各任务的 `engine_database_identity.json` 保存引擎与数据库文件 SHA-256。
- 原始 P20M 三个目录的输入、输出未修改。G0 的 `raw_review.json` 保存 244 个旧 case 的逐项哈希复核，包括原 192 个光谱 PASS。
- 总验收以本目录的 `acceptance.json` 为准；构建身份见 `build/build_receipt.json`，逐 case 入口见各 `case_manifest.json`、`raw/<case>/checkpoint.json`、`qc_review.json`。

## 实际执行与发现

### G0：QC、身份与记录

`tools/p20m2_geometry_qc.py` 统一 `requested_observer/requested_range` 与 `observer/range`，检查必需几何字段、未知模式、实际几何摘要及地球相交状态。几何检查不再只看输入回显。打印精度、求解器路径残差和 WGS84 映射状态分别记录；实际打印几何通过不等于 ECEF/折射路径映射已验收。

原 244 个 case 的实际几何复核结果为：194 个打印几何通过、29 个失败、21 个未验证。`g0/failure_closure.csv/json` 保留原 52 个失败：

- 44 个近场 NaN 仍未完成生产算法闭合。
- 8 个高空直射 GEO IERROR 通过实际折射率廓线的 Bouguer 不变量复核，证明原输入“视太阳天顶角”下被地球遮挡。只闭合这些旧直射状态；不代表天文 SZA 映射、漫射、路径散射或全域发布通过。

`p20m_modtran.py` 将计算身份与 QC 身份分离，保存 `qc_identities`。已有 exit 0 的不可变 raw 即使此前 QC 失败，也只重新解析，不盲目重复运行相同确定性失败。新任务全部 820 个 case 已实际执行身份续跑，均复用 raw，尝试次数仍为 1。缺失/变化的输入或输出不能作为有效续跑。

新增 `IRPhysicalTrace.h` 将数值记录写入专用、互斥保护的 JSONL 文件，STOP 时 flush。环境变量为：

```text
HWASIMIR_RUN_ID=<本次运行身份>
HWASIMIR_PHYSICAL_TRACE=<必须尚不存在的文件>
```

记录包含 sourceSeq、generation、协议 UTC/相对时间、完整目标身份字段、band、数据哈希身份、查询来源、几何/气象输入及带单位/状态的分量。无有效值时写 null；不把未知值补零。当前旧查询的整体状态明确标为 `LEGACY_DIAGNOSTIC_NOT_PHYSICS_ACCEPTED`，cellId 为旧格式未声明状态，不能冒充新完整物理数据。

Windows 与板卡分别用 4 个线程写入 2,000 条记录并通过严格 JSON schema/重复/有限性检查。污染行、重复字段、缺值、异常 band 和非有限有效分量会拒绝。这是记录器单元证据，**尚不是正式渲染逐帧物理真值**。

### G1：真实官方实验

实际运行的开发 case 共 820 个，对应 784 份不同输入卡；各实验间有 36 个重叠重复执行。光谱 checkpoint 为 736 PASS、84 FAIL，不能将这些 PASS 解释为完整物理通过。分布如下。它们是开发/对照样本，不是独立 holdout，也没有被打包为正式 LUT。

| 目录 | case 数 | 用途 |
|---|---:|---|
| g1_v2 | 202 | 端点/角度/中心角近场表示、ISAACS/DISORT/SCALED_ISAACS 晨昏、solar-on/off |
| g1_directional | 96 | 上下视、斜向和竖直短路径 |
| g1_vertical | 48 | 竖直路径改用端点高度＋天顶角 |
| g1_clear | 96 | 修复隐藏卷云后的同环境晨昏及热源对照 |
| g1_nearfield | 336 | 1 cm⁻¹ 逐谱点局部输运开发与官方重叠参考 |
| g1_nearfield_p1 | 28 | 本机 p1_2009、0.1 cm⁻¹ 对照；保留 SWIR 端点包围失败 |
| g1_nearfield_p1_swir | 14 | 新输入补足真实端点后的 SWIR 对照 |

另有 `g1` 目录在输入换行导致哈希不一致时被拒绝，未启动引擎；随后在新目录 `g1_v2` 使用明确的 ASCII/LF 字节输入。

关键发现：

1. **有限光谱不代表正确路径。** 一组端点＋角度的等高短路径输出有限光谱，但实际 HRANGE/BETA 为零。这些结果被几何 QC 拒绝。
2. **竖直边界有可用的官方表示。** 初始斜距输入的 12 个竖直状态因浮点高度差检查被 FTRANG 拒绝；改用端点高度＋天顶角后，新 48 个算例全部通过光谱和实际打印几何检查，包括 0.1 m。没有将其推广为任意方向全域证明。
3. **旧 flux 模板暗含卷云。** CARD 2 的 `ICLD=18` 是标准卷云，不是输出层控制。旧传播模板为 ICLD=0，因此原“统一环境”并不一致。新有类型模板显式声明 `cloud_model=0`，气体/气溶胶基线与 P20 云雨雪职责分开。没有修改板端天气配置以获得性能 PASS。
4. **晨昏巨大尖峰与该环境差异相关。** SWIR、3 km、SZA=90° 的 ISAACS DOWNWARD 从约 12.139 降为 0.124677；90.5° 从约 76.105 降为 0.179173 W/(m²·μm)。无云复算仍有弱线太阳光学厚度 warning；完整晨昏有效性与插值预算尚未验收，没有未经论证平滑或填零。
5. **DOWNWARD 包含热分量。** solar-on/off 和深夜对照均保留了非零热辐照度；不能把深夜 MWIR DOWNWARD 清零。方向天空背景也不能用它直接替代。
6. **默认太阳源为 Kurucz 1997。** 早期实验元数据曾误写 2005，但实际 CARD 1A 的 LSUNFL=F。`source_audit.json` 将两个成像波段真实 TOA 输出逐谱点与本机 1997/2005 文件比对，1997 在两份文本各自打印分辨率预算内全部匹配，2005 不匹配。已纠正模板声明，未为元数据修正重复运行引擎。

模板保留 MLS/Rural、scaled_mls_surface_rh 语义，显式设置 CO2、地表边界、半径、日地归一、源选择。旧模板/旧 raw 继续保留用于追溯，不重新标成正确生产数据。

### 近场算法仍未获得批准

局部输运实验使用**逐谱点** tau 和匹配的热路径源项，再做波段积分，未对波段平均 tau 直接指数缩放。透明谱点使用有说明的连续极限，遇到不可靠参考或无法逆推的谱点会拒绝。

- 1 cm⁻¹：288 个官方重叠分量比较；tau 最大绝对差 0.0024019346；热路径最大相对差约 51.53%。
- 0.1 cm⁻¹、20 km 的 MWIR 对照：热路径最大相对差仍约 43.33%。
- 0.1 cm⁻¹、补足端点后的 SWIR 对照：热路径最大相对差约 14.13%。

最坏相对差与最坏绝对差不是同一个状态，完整结果见三个 `overlap_report.json`。这些结果不能证明近场方案满足最终误差预算；没有事后增加绝对容差来换取 PASS。还需解决有限谱区间的非线性输运误差，并审查极短路径参考本身的数值精度。当前没有形成 0–1 m、所有方向、天气中间值的正式连续支持域。

### 时间、缓存与运行代码

- 新 `IRSimulationClock` 使用协议时间推进热历史，区分序号重复/倒退、时间重复、跨日及需要 RESET 的大跳变。重复时间保持 dt=0；不再使用 Panda 墙钟积分材质和 Stage4 热状态。
- 材质积分去掉 1 秒截断，按稳定性要求细分完整 dt；热源/气动平滑去掉原有时间截断，并拒绝非有限物理输入。
- 每包平台/目标几何、相机引用及 ViewValid 变化触发 IR 更新，避免只按仿真时间间隔更新导致移动几何使用旧分量。
- 正式调用的两个 1 m 隐式夹距已删除。**解析零路径和完整正短距生产查询仍未接通，不能宣称零路径已正式通过。**
- 时钟、跨日、重复/倒退、大跳变、热积分分段一致性及非有限输入检查已做 C++ 单元验证；未完成生产 DDS 回放的变速一致性矩阵。

## 构建、板卡与 60 Hz 范围

- Windows：VS2015 v140，`build-p20m2-renderer/bin/HwaSim_IR.exe`。
- Qt5.12.12：`build-p20m2-stim/release/DataDrivenTestQT.exe` 及独立 DDS 发端。
- aarch64 最终构建：`build/aarch64_geometry_cache`；构建收据绑定源码清单、ELF SHA-256 和 Build ID。
- RK3588 实际单元运行目录：`/userdata/HwaSimIR/p20m2_validation_20261021_v1`。这不是正式渲染候选。

初次常规 Windows 构建曾写入默认 `HwaSim_IR/Bin`。该试验 EXE/PDB 已保存在任务 build 目录，默认 EXE 已恢复为旧 `code_validation_01` 清单验证的 P20M 二进制；见 `build/windows_default_restore.json`。未记录任务开始前默认 EXE 的哈希，因此不声称与开始前字节完全相同。后续 P20M2 构建均使用隔离目录。

独立 DDS 发端增加 `--p20m2-60hz`：要求 input-hz=60、videoFps=60、simMode=1；采用累计纳秒 deadline，保留 CSV 原始时间偏移。`--pacing-jsonl` 单独记录协议时间、源相对时间、发送 deadline 与墙钟发送完成时间。错误速率/视频 FPS 的拒绝测试已实际运行，在 DDS 初始化前退出。

三份 CSV 的全部行已检查并保存原文件哈希：

| 文件 | 行数 | 原始仿真跨度 | 60 Hz 首末发送 deadline 跨度 |
|---|---:|---:|---:|
| circleDataAll.csv | 3940 | 39.390 s | 65.650 s |
| circleDataAll2.csv | 319 | 3.180 s | 5.300 s |
| circleDataAll3.csv | 2902 | 29.010 s | 48.350 s |

第二份确认为第三份前 319 行，旧审计通过事实保留。以上是完整性与时轴清单，**不是新数据正式查询/渲染回归结果**。

本轮没有执行 100 Hz 测试，也不把历史 100 Hz 积压作为本轮失败条件。新数据 COMPLETE 成像路径尚不存在，因此双路 60 FPS、逐帧 ≤80 ms、队列守恒、正常视频与 AGC 前 ROI 验收均为 NOT_RUN，不能由单元测试、编译成功或传输 exit 0 推导 PASS。

## 未交付项及发布状态

未完成：A/B/C/D/E 生产 schema 与独立状态、有效单元/邻接与连续覆盖证明、正式加载与 Shader 接通、独立背景、0–180° 全太阳域、冻结分量绝对误差预算、至少 200 状态独立 holdout、全量三 CSV 正式回归、状态矩阵、AGC 前图像证据和双路 60 FPS。旧 SZA45 传播替代、太阳高度关断及目标/背景生命周期耦合仍需后续实现，当前代码不能视为全域成像修复完成。

`coverage_manifest.json` 的接受单元为空，明确 `NOT_RUNTIME_DATASET`；不存在可发布的新全域运行包。没有把 P14 数据包改名发布，没有创建 P20M2 成像候选。客户实际软件为 NOT_RUN，真实标定为 NOT_VERIFIED_CALIBRATION。

未切换运行入口。已只读确认原基线和既有启动器仍在；需要启动原基线时，板端原有命令为：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M_20261021_code_validation/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm
```

## 已实际运行的复核/续跑入口

在 `D:\HwaSimIR` 执行：

```powershell
# 验证身份并复用已有 raw；不消耗新的引擎尝试次数
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p20m2_resume_evidence.ps1 -RunEngine

# 只复核证据、重新分析及聚合
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p20m2_resume_evidence.ps1

# 独立回归
F:\Programs\anaconda3\python.exe -m unittest discover -s tools -p "test_p20m*.py" -v
```

总验收器在当前未完成状态有意返回 **2**。它不是一键全域生成/发布入口；缺失的生产实现不能靠再次执行此脚本自动补齐。输入物理卡改变必须创建新任务目录；只改解析/验收规则则复核现存 raw。

各次构建和实验的控制台记录保留。总聚合曾遇到最终 receipt 尚未生成，以及 Windows 默认 GBK 解码 UTF-8 源码清单失败；已等待构建结束并显式使用 UTF-8 后重新执行，不将这些暂态/脚本问题当成工具不可用。
