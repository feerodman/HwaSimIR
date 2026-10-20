# P20M 实际执行记录与未通过门槛（2026-10-21）

本次已修改代码、真实调用官方 MODTRAN、构建 Windows/aarch64、部署新隔离候选并运行板卡双路回归。**P20M 全域目标尚未完成，不能作为正式物理数据发布。** 本记录是可复核的开发 checkpoint，不能替代连续覆盖、独立精度和物理成像验收。

依据为本机 `docs/HwaSimIR_MODTRAN_FullDomain_Solution_and_Codex_Prompt_20261021.md`、1020 审计及客户附件。开始时 HEAD 为 `fe26dd8677382f0ceac606930005005dc4e0b160`；保留用户未提交的任务文档，没有 reset/clean。旧 Stage3 开关和局部热源职责未作为本次修复捷径。

## 当前交付状态

| 项目 | 状态 | 实际证据及限制 |
|---|---|---|
| 官方数据生成 | PARTIAL / DEVELOPMENT_ONLY | 244 个真实分量开发 case；192 个当前光谱 QC 通过、52 个失败。不是 244 个独立物理状态，更不是 200 个独立 holdout |
| 连续支持域 | NOT_ACCEPTED | 尚未实现覆盖全部合法几何的新版参数化、有效单元清单与结构证明 |
| 插值精度 | NOT_VERIFIED | 旧表回归通过；新版 A_x 未冻结，独立 holdout 未执行 |
| 物理合成 | NOT_ACCEPTED | 未完成 A/B/C/D/E 分离及晨昏、天空/地面背景完整性；没有用补零或增益宣称通过 |
| Windows | BUILD_PASS / STARTUP_PASS | VS2015 v140、Qt5.12.12 新目录构建通过；两程序实际启动存活；Windows 端物理视频验收尚未完成 |
| RK3588 双路 | TRANSPORT_PASS / PERFORMANCE_PARTIAL | 40/60 FPS 普通回放及三 CSV 直接 DDS 帧计数守恒；100 Hz precise 有持续积压 |
| 客户实际软件现场互通 | NOT_RUN | 使用项目 stim 和独立模拟 DDS 发端，未启动客户真实软件 |
| 真实标定 | NOT_VERIFIED_CALIBRATION | 无实测标定数据 |

## 已落地的源码修改

1. `tools/modtran_flux.py` 共用真实 flux 层解析：精确层优先，只允许真实相邻层内插值；缺少上下界即失败。P11 湿度、solar heating 和 P13 QC 已共用，删除高空继续套用 0/1 km 两层外推的实现。
2. `modtran_convert_to_si.py` 保留 NaN/Inf/`***********` 所占的原始列位置，避免正则丢列后把别的分量读进来。必需分量非有限即失败；非必需诊断分量无值用空字段及独立 valid 标识，不填零。P20M flux 的必需列是 DOWNWARD，直射仍独立读取 SOL TR。
3. `IRSimulationUtc.h` 和正式 M1 使用明确的 `UnixEpochMs` / `UtcMillisecondsOfDay` 契约；日内毫秒要求 UTC 日期锚点，识别跨日并在 RESET 清除累计日数。日期不再由旧的 2026-09-06 加绝对时间取模得到。日志保留 `DisplayC2cObjTrackingData.time`、sourceSeq 和来源。
4. stim 预检和双路回放要求显式 UTC 起点；预检按每行时间计算日期，跨日不会固定旧日期。正式覆盖预检仍存在。
5. `IRLutAxisIndex.h` 在加载时建立分类别前缀索引。正式 radiance 查询最多访问 32 个叶节点、heating 最多 8 个；替代每次查询对整张表扫描、构造轴和排序。缺失、重复叶节点拒绝查询。旧容差由相邻节点间距及正距离限制，不把亚米正距离吸附到零或旧端点。**这不是新域无空洞证明。**
6. 错误相机引用记录原始完整键、输入目标完整键和累计次数，限频输出；拒绝仅按 ID 匹配其它类型。未全局替换 0x33/0x22。
7. 正式 M1 失败关闭的日志由误导的 `finalOutput=legacy` 改为 `formal_fail_closed`。
8. 板端测试工具分别记录配置视频 FPS、输入 Hz 和同步出帧计数；精确报告测得 FPS，不把 59 FPS 视作 60 FPS。

## 官方 MODTRAN 执行与尚未解决的物理门槛

工具为 `F:\Programs\PcModWin5\Bin\Mod5.2.1.0.exe`，真实 tape6 回显 5.2.1。已读取本机 MODTRAN/PcModWin 手册、数据库和成功模板，保存 engine/数据库文件 SHA-256。

采用原安装程序 + 每 case 独立工作目录，通过已验证的 CARD1A4 数据库路径读安装 Data；全局跨进程锁串行运行。没有修改共享 Bin 的 modin/tape。输入不可变，运行输出按 `attempt_###` 保留；每 case 原子 checkpoint、输入/输出摘要、耗时、QC、独立引擎身份历史。默认总尝试上限 3，超时 120 秒。实际执行过首轮和续跑；详见 `evidence/resume_execution.log`。

| 开发任务目录（logs/p20m 下） | case 数 | 光谱 QC 通过 / 失败 | 用途 |
|---|---:|---:|---|
| `20261021_full_domain` | 116 | 96 / 20 | 官方例子、项目成功例子、高空/短距/上下视、太阳角/相对方位、宽带加热 |
| `20261021_geometry_repair` | 96 | 64 / 32 | 水平 ITYPE1 与球形 angle/range 短距求解对照 |
| `20261021_solar_repair` | 32 | 32 / 0 | 高空/晨昏/深夜 ISAACS 的 DOWNWARD 光谱探索 |

这些 PASS 只表示必需光谱完整、有限、端点包围及输入检查通过；QC 明确标为 `SPECTRUM_QC_PASS_GEOMETRY_REVIEW_REQUIRED`，不表示完整物理正确。

关键实测失败与边界：

- 部分球形近等高短距求解在官方 tape6 中出现 NaN；高精度 CARD3 和 angle/range 试验仍有失败。水平 ITYPE1 的 48 个分量 case 通过，但只能证明这些水平样本，不能推广到任意斜向/竖直及全 0–1 m 域。
- 部分直射太阳路径返回 `GEO: IERROR NE 0`（地球相交），没有有效 SOL TR。尚未完成带高度、球形遮挡与折射语义的独立物理边界分类，未直接填零发布。
- 原 flux 输出存在其它高度或 DIRECT 诊断列溢出。解析已防止列移位；所需 DOWNWARD 有效不代表丢失的 DIRECT 有效。
- 历史模板的 CO2、日序、边界反照率、散射求解选项不一致。修复试验尚未构成经过统一验收的生产模板；未据此无限扩张真实全批次。
- 到 100° 的初始太阳样本不等于覆盖 0–180°。已计算部分 120°/180°开发样本，但晨昏/深夜全域、方位对称性与太阳源距离缩放仍未验收。
- `scaled_mls_surface_rh` 沿用 MLS 地表湿度缩放与气溶胶湿度语义，不代表沿高空局地恒定 RH。现阶段未扩充正式气象域。

每模式实际耗时、总磁盘量、失败明细在 `evidence/official_development_summary.json` 和三个 `pilot_qc.json`。**完整批次的有效单元/模板未通过，当前没有可信的完整任务数与 ETA，不能将开发样本数量冒充完整批次。**

## 构建、数据身份与隔离部署

新目录：

- 本机 Qt：`build-p20m-stim`；独立 DDS 发端 `build-p20m-stim/release/P20MDirectDds.exe`。
- VM：`/home/linaro/userdata/HwaSimIR-p11-20261021-013000`。
- 板卡：`/userdata/HwaSimIR/candidates/P20_P20M_20261021_code_validation`。
- 原入口保留：`/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm`，未切换 current。

aarch64 ELF SHA-256：`1e38e8dbaabb6a959ee93a33efb132c695df3ae068194955d81614167933a1ff`。

该候选标记为 `CODE_VALIDATION_ONLY`，**正式数据仍是 UNCHANGED_P14**。两路及本机包按实际 SHA-256 验证：

| 文件 | SHA-256 |
|---|---|
| band_lut_si.csv | `48432459ac56dbb6980f3ca25e2d1e4701763ba3674703051cb0fd45da8c7cd7` |
| solar_heating_lut_si.csv | `1eee146191c0f353bef8408a116b115ca3948bb33fde10b006fc78646d8c6561` |
| p14_coverage_manifest.json | `f5d27c304b663218b94c9e45bd2bbbb968de1a66695f551ba5c4633f9c4c5d7b` |

旧表历史 335 行未被接受，不能算新发布有效行；旧 radiance 正式接受 2061 行。NIR 数据未改名或覆盖。没有修改 P14 域上界。

初次代码候选的版本文件曾继承旧 GitCommit 标签；已只对新候选原子修正为当前 HEAD，两路 ELF 身份始终为上面的新构建摘要。修正前日志保留原样；三份全量直接 DDS 日志及最新 receipt 记录修正后的身份。

## 已运行的回归与性能

旧审计条件全量查询，在旧探针和新索引探针结果一致：

| 客户文件 | 行数 | SWIR+MWIR 有效查询 / 总数 |
|---|---:|---:|
| circleDataAll.csv | 3940 | 0 / 7880 |
| circleDataAll2.csv | 319 | 638 / 638 |
| circleDataAll3.csv | 2902 | 642 / 5804 |

第二份仍全部通过，并为第三份前 319 行前缀；没有报告其复现同类失败。CSV 原文件 SHA-256 保留在证据中。

板端同为 coarse MWIR 320×320 / precise SWIR 800×800，raw/H264，实际天气配置未关闭。如下旧日期仅用于**明确重现旧条件及同配置性能对照**，不作新域覆盖：

| 测试 | 输入/视频设置 | 每路帧数 | coarse / precise 渲染 FPS | coarse / precise P95 解码延迟 ms | precise 最大 ms |
|---|---|---:|---:|---:|---:|
| 新候选 ordinary，60 秒 | 40 Hz / 40 FPS | 2400 | 40.011 / 40.034 | 19.636 / 64.044 | 128.091 |
| 新候选 ordinary，全量 | 60 Hz / 60 FPS | 4318 | 60.017 / 60.045 | 15.482 / 54.207 | 119.450 |
| 原 P20 基线，同全量同配置 | 60 Hz / 60 FPS | 4318 | 60.017 / 60.046 | 15.794 / 54.498 | 118.847 |

没有排除冷启动帧。两组 60 FPS 比较未见可归因的大幅退化，但单次对照不足以给出统计显著性结论；原基线和新候选都存在 >80 ms 的帧。P95 小于 80 ms 不等于每帧达标。

当前 UTC `2026-10-21T03:30:00Z` 的三份全量直接 DDS：

| 文件 | 每路输入/接受/渲染/发送/解码/保存 | coarse / precise FPS | precise 队列峰值 | precise P95 ms |
|---|---:|---:|---:|---:|
| circleDataAll | 全部 3940 | 100.108 / 75.011 | 1017 | 12463.313 |
| circleDataAll2 | 全部 319 | 101.566 / 71.813 | 92 | 1249.431 |
| circleDataAll3 | 全部 2902 | 100.153 / 72.748 | 797 | 10313.647 |

这些运行输入为 100 Hz，INIT 视频配置为 40 FPS，当前同步契约是每个输入包产生一帧；不把配置 FPS、真实处理 FPS、视频播放时长混为一谈。没有用丢帧/重采样通过计数。三个测试均为 `TRANSPORT PASS` 且 `FAIL_SUSTAINED_BACKLOG`，不是物理或实时验收 PASS。

直接 DDS 120 帧测试还在第 31 帧注入了错误完整键。两路实际记录 `targetType=0x33 ... inputTargets=0x22/3/3 action=reject_reference_no_id_alias`，之后合法键恢复。当前 UTC 的真实太阳状态约 SZA 55.22°，日志记录协议绝对时间来源；正式主表越界标为 `formal_fail_closed`。

新 stim 以当前日期对客户 CSV2 预检时也真实拒绝 SZA 55.2205 超出旧 20–45°域，未发送 INIT。没有通过绕过正式 stim 来宣称物理成功；独立 DDS 发端只用于验证实时接收路径。

所有运行均完成策略恢复。视频、raw、逐帧队列和接收记录位于 `logs/p20/runs/p20m_20261021_*`。`physical_components_sampled.csv` 来自限频的 M1 分量日志，保留有效状态；它不是逐帧 AGC 前标定真值。尚未完成固定显示参数 ROI/目标背景差误差验收、加载/查询耗时和内存全量仪表。

## 验证记录

- Python 新解析用例 4/4，既有单位转换用例 4/4。
- C++ UTC 契约、真实日期、闰日跨日、RESET、倒退拒绝、极点有限向量通过。
- C++ 亚米索引不吸附零、合法区间权重、缺单元、重复节点检查通过。
- 旧等高查询 22/22、旧 solar heating 查询 34/34，保持已有严格检查。
- VS2015 v140 和 aarch64 编译成功；Qt 新构建成功。
- Windows 启动 smoke 只确认两进程存活；有既存 shader `gl_Position.w` 警告，不能写成已完成 Windows 物理成像验收。

## 可执行入口与续跑

在 `D:\HwaSimIR` PowerShell 中运行；身份变化需要新的任务目录，不得覆盖既有输入。官方计算运行失败门槛仍会返回非零：

```powershell
# 实际运行过：生成不可变输入、逐 case 续跑、全部 QC
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p20m_resume_pilots.ps1 -Run
# 只重做现存证据 QC
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p20m_resume_pilots.ps1
# 独立任务有上限的续跑；默认累计最多三次
python tools/p20m_modtran.py run --resume --root logs/p20m/20261021_geometry_repair --max-attempts 3
# 重建直接 DDS 发端
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p20m_build_direct_dds.ps1
# 全量三 CSV：使用新前缀保留本轮证据，SSH 密码用环境变量传入
python tools/p20m_customer_regression.py --prefix p20m_20261021_repeat02
# 汇总并打包；输出目录必须不存在，需板卡只读身份核验
python tools/p20m_collect_evidence.py
python tools/p20m_package_checkpoint.py --output logs/p20m/20261021_full_domain/delivery/code_validation_02
```

构建/隔离部署入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/stage0_build.ps1 -QtBuildDirectory build-p20m-stim
# 后续构建必须使用新的 StageId 和输出目录，保留当前 build receipt
powershell -NoProfile -ExecutionPolicy Bypass -File tools/p11_rk3588_source_build.ps1 -Mode Build -ConfirmSourceStable -StageId p11-20261021-030000 -OutputDirectory logs/p20m/rebuild_030000
python tools/p20m_deploy_code_candidate.py --build logs/p20m/rebuild_030000 --name P20_P20M_20261021_code_validation_02 --output logs/p20m/deployment_02
```

板端启动（现有启动器已复制到新隔离候选，原入口未改）：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M_20261021_code_validation/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M_20261021_code_validation
# Ctrl+C 仅结束此启动器拥有的两路并恢复策略；随后回到旧入口：
python3 /userdata/HwaSimIR/candidates/P20_P20M_20261021_code_validation/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm
```

新 `UtcTimeMode=UnixEpochMs` 已实现解析与行为；只有选择 `UtcMillisecondsOfDay` 时才使用显式 `FallbackUtcDate` 日期锚点。CSV 起点由发端 `Demo/UtcDate`/`UtcHour` 明确指定，CSV 时间列作相对偏移；接收封装时间、系统时间不充当协议日期。没有新增仅有 INI 字面而无行为的开关。

## 继续工作必须解决的事项

1. 统一生产模板及实际 tape6 几何/廓线/太阳状态验证；完成可证实的任意方向近场算法和解析零路径边界，解决球形短距失败。
2. 落实全域合法单元参数化及 A/B/C/D/E 独立数据 schema、生成和 loader；保留有效传播分量的部分有效状态。当前正式太阳不足仍可能连带使组合查询失败。
3. 修复背景与目标生命周期耦合、天空方向查询、近地平线/地面域；完成太阳相对方位和深夜/晨昏契约、地日缩放、天气重复衰减审计。
4. 冻结分波段/分量/日照状态绝对误差门槛与最终辐亮度预算，再独立运行至少 200 状态 × SWIR/MWIR holdout，不能复用开发加密点冒充独立样本。
5. 完成无目标/混合目标/隐藏删除/跨日/极区等完整板端状态矩阵、接收端物理无效状态以及固定显示的物理 ROI 证据。
6. 验收新数据加载时间、内存、查询耗时及实时性能；目前 100 Hz 双路 precise 明确未达实时要求。完成同配置实际客户 INIT 天气的逐字段对照，当前模拟发端不能替代客户软件。

这些是未完成的技术工作，不是板卡不可达或权限阻塞。板卡、VM、本机官方工具均可达。由于前述物理与覆盖门槛未通过，本轮没有发布或切换正式全域数据包。
