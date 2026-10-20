# P20 MODTRAN 数据与 1020 成像异常核查

本报告对应 `/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm`、Windows 当前工作区和 `1020测试内容` 中的两组日志、三份 CSV。检查在 2026 年 10 月 20 日至 21 日进行。

结论：运行数据一致，正式 M1 链路已启用 MODTRAN。客户日志中出现正式 LUT 越界后目标置黑，同时存在相机目标引用未找到。太阳加热表和背景大气依赖也有独立限制。因此，打开旧 Stage3/Stage5 开关不能解决本次问题。

本次完成只读部署核查、代码路径审计及现有生产查询器复测；没有修改生产配置、替换板卡程序或生成新的物理数据。当前候选版本仍有下述问题，不能将本报告视为修复验收。

## 两端数据身份

已通过 SSH 读取板卡 coarse、precise 实际文件。Windows `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN` 与板卡两路均有 7427 个文件，总大小均为 5,614,151,638 字节，文件相对路径和大小没有差异。

以下三个运行数据及身份文件逐字节 SHA-256 一致：

| 文件 | SHA-256 |
|---|---|
| `processed/band_lut_si.csv` | `48432459ac56dbb6980f3ca25e2d1e4701763ba3674703051cb0fd45da8c7cd7` |
| `processed/solar_heating_lut_si.csv` | `1eee146191c0f353bef8408a116b115ca3948bb33fde10b006fc78646d8c6561` |
| `processed/p14_coverage_manifest.json` | `f5d27c304b663218b94c9e45bd2bbbb968de1a66695f551ba5c4633f9c4c5d7b` |

`build-p19-stim/release/Config` 内的正式 LUT 和 P14 manifest 也具有上述相同摘要。板卡两路 ELF 摘要均为 `da428e18b9781ed7a61876facc483665f0213da491b60c4a396f5d9d749d22e6`。

因此可以确认实际运行使用的是同组 MODTRAN 数据。其余历史、原始文件仅比对了路径和大小，不声称整个 5.6 GB 目录均完成内容哈希核验。

## 哪些文件被使用

| 文件或目录 | 当前用途 | 是否直接用于正式逐帧成像 |
|---|---|---|
| `processed/band_lut_si.csv` | M1 波段大气数据：视线透过率、路径热辐射、目标处太阳直射辐照度、下行天空漫射辐照度、路径散射辐射；旧 Stage3 也加载其中透过率信息 | 是，查询有效且对应 M1 开关开启时 |
| `processed/solar_heating_lut_si.csv` | NaturalSolar 的宽带太阳直射与漫射加热，单位 W/m²；影响材质温升 | 是，但本次客户几何下查询失败 |
| `processed/p14_coverage_manifest.json` | LUT 摘要、覆盖范围与发布身份校验 | 用于启动校验，不是辐射数值来源 |
| `processed/p13_coverage_manifest.json` | 代码保留的旧清单候选路径 | 当前实际选择 P14，未作为本次运行清单 |
| `processed/band_lut.csv`、`band_lut_si_candidate.csv`、各种 smoke/diagnosis LUT | 历史生成、转换或诊断产物 | 否，不是当前正式数据源 |
| `path_lut_spectral.csv`、`sky_lut_spectral.csv`、`solar_lut_spectral.csv` 及其它诊断光谱表 | 离线生成、积分与审计中间产物 | 当前渲染代码没有逐帧读取这些文件 |
| `raw`、`generated` | 原始 MODTRAN 输出、输入案例及生成证据 | 否 |
| `processed_snapshots`、`processed/Archive` | 历史快照和归档 | 否 |
| QC、manifest.csv、单位审计、失效诊断报告等 | 离线质量和来源审计 | 否；区别于启动实际读取的 P14 coverage manifest |

LUT 的 `source_files` 等列是来源追溯信息，不代表渲染时会重新打开其中所有原始文件。历史证据虽然不直接参与逐帧计算，仍有审计价值，不能据此直接删除。

正式 CSV 有 **2396 行数据**；生产加载器实际接受 **2061 行**。其中 335 行旧 MWIR 数据缺少正式链路要求的完整分量，被加载器跳过。文件中存在高空 MWIR 旧行，不等于这些行能供正式 M1 成像使用。

本次实际使用波段为客户 coarse 的 MWIR 和 precise 的 SWIR。NIR 数据虽然被加载，但不因此成为这两路的替代数据；NIR 也不能改名后作为 SWIR 使用。

## 配置开关与大气来源

用户列出的 Stage3 和 Stage5ModtranRadiance 开关确实为关闭状态。它们控制旧透过率替换、旧路径辐射实验接入及诊断，不能代表正式 M1 是否开启。

本机配置和板卡两路实际配置均包含：

```ini
[Stage5Radiance]
EnableIRPhysicalPipeline=true

[M1NirMwirPhysics]
CompareOnly=false
EnableRuntime=true
EnableNIRRuntime=true
EnableSWIRRuntime=true
EnableMWIRRuntime=true

[NaturalSolar]
Enable=true
EnableSolarThermal=true
```

两组日志均确认 `EnableRuntime=1`、`CompareOnly=0`、`ModtranRows=2061`。`DebugView=Off` 也只是关闭调试显示，不关闭物理计算。`Stage5ModtranCompareEffective=1` 可以由 M1 等正式功能触发，不与旧 CompareLegacy=false 矛盾。

当前应区分三种大气处理：

1. **正式目标链路**使用 `band_lut_si.csv`。SWIR/MWIR 按数值湿度查询 RH30/RH60/RH85 数据，而不是仅按配置字符串 `HumidityProfile=default` 查询。
2. **旧 Stage3/legacy 链路**尝试加载 `transmittance/transmittance_0.3_15.txt`。两组板卡日志明确显示它未加载，使用经验透过率；代码以默认参考透过率、路径长度和能见度进行经验计算。这些旧链路日志不能证明最终采用 legacy 图像。
3. **背景**使用配置中的温度、发射率、反射率等工程假设，并借用最新有效目标 LOS 的 MODTRAN 透过率、路径项和照明项。它不是实测地点天空/地面光谱数据库。

`PreferredSource=band_lut` 目前没有其它可用候选值。代码遇到其它字符串会警告并强制回到 `band_lut`；不存在配置为 spectral/raw 后自动改用原始光谱的功能。

证据代码：`HwaSimIR.cpp` 的资源选择约 11664 行、M1 配置约 11726 行、PreferredSource 约 13219 行、数值湿度查询约 15846 行；`IRSimulation.cpp` 的旧经验透过率实现。

## 两组日志揭示的差异

| 项目 | 135725 旧回放 | 163730 客户 DDS |
|---|---|---|
| 正式 LUT 身份校验 | PASS，同一摘要 | PASS，同一摘要 |
| 正式 M1 开关 | 已开启 | 已开启 |
| 首帧平台高度 | 约 11.000 km | 约 9.913 km |
| 太阳天顶角 | 约 41.427° | 约 73.318° |
| M1 FormalFailClosed 日志 | 两路均未发现 | 每路 366 条，不能当作完整失败帧数 |
| 相机目标未找到 | 未发现 | 每路 520 条 |
| 旧回放结束计数 | 每路接收并捕获 331 帧 | 不据现有日志宣称完成同样的正常停止验收 |

135725 没有开启完整分量日志，且首帧 `inputViewValid=0`。没有失败日志不等于全部红外物理和太阳加热均通过验收，更不能把该回放作为任意几何的证明。

客户 coarse 第 967 行及 precise 第 965 行明确记录：

```text
status=FAILED_CLOSED
fallbackReason=out_of_range
fallbackAxis=targetAltKm
query=12.7073 min=0.001 max=10.05
action=black_target_no_legacy legacyRendered=0
```

这说明正式计算所需数据不足，程序主动选择无效输出处理。此处 `tauUp=1` 是失败结果的默认字段值，不能当作实际使用了完全透明大气。`[M1 Compare] finalOutput=legacy` 是另一处日志的二分标签缺陷：其代码只区分 M1/legacy，漏写 formal_fail_closed。实际执行分支和专门失败日志说明没有退回旧目标图像。

客户日志还分别有：

- coarse 第 942 行、precise 第 940 行开始的 `lookat_target_not_found`：相机请求的完整目标引用没有找到。这个错误与 LUT 无关，可能影响观察方向；仅凭这些日志不能替客户决定期望观察的对象。
- coarse 第 965 行、precise 第 963 行开始的 `L1 SolarHeatingLut` 越界：该数值湿度分支的目标高度上界为 1 km，当前输入约 12.707 km，执行 `no_solar_heating`。
- 首个有效实时包已提交参考位置，客户日志为 `atmosphereCacheInvalidated=1`。不能直接把异常归咎于沿用初始化占位位置。

背景方面，`HwaSimIR.cpp` 约 16802 行仅在目标正式查询有效时更新环境大气；约 7178 行在没有可用环境大气时将背景所需透过率和照明项置零。因此“目标查询失败影响背景”具有明确代码依据。若先有有效查询再越界，还存在沿用旧环境值的风险。现有日志不足以量化每帧背景受影响程度。

## 三份 CSV 的现有查询器复测

复测使用当前 `build-p19-stim/release/NetworkConfig.ini` 的条件：UTC 2026-09-06 03:30、能见度 6 km、RH85；两波段均逐行调用现有 `IRModtranRadianceLut`。它是本机数据查询复测，不是新的板端视频或客户 DDS 回放验收。

距离按激励预检代码从两端经纬高重建，未直接使用文件 distance 列。

| 输入 | 数据行 | 平台高度 km | 目标高度 km | 重建斜距 km | SWIR+MWIR 查询结果 |
|---|---:|---|---|---|---|
| circleDataAll.csv | 3940 | 9.880–9.992 | 9.895–13.737 | 0.0557–44.5731 | 0/7880 有效 |
| circleDataAll2.csv | 319 | 10.972–10.997 | 9.997–10.050 | 26.6494–28.5606 | 638/638 有效 |
| circleDataAll3.csv | 2902 | 10.825–10.997 | 9.997–10.824 | 0.0525–28.5606 | 642/5804 有效 |

第一份第 2 行首先在 observerAltKm 失败：约 9.99161 km，相关高空单元允许 10.95–12 km。第三份第 323 行首次在 targetAltKm 失败：约 10.0517 km，上界 10.05 km。预检一旦遇到失败便拒发 INIT，外部 DDS 不经过该 UI 的整文件预检，所以会在板端运行期间失败。

**第二份在上述条件下通过，不能把它报告为已复现相同错误。** 如果当时选第二份也报错，需以弹窗/预检审计的具体 axis、value、bounds，以及当时日期、时间、湿度、能见度和所选通道为准。CSV 本身没有完整环境与绝对日期信息。

现有高空单元主要覆盖观察者 10.95–12 km、目标 9.7–10.05 km、斜距 2.4–50 km、能见度 6–23 km、太阳天顶角 20–45°和 RH30–85。低空数据属于另外的单元。不能把所有行的全局最小值和最大值合并为一个“任意组合都可查询”的长方体。

客户实际太阳天顶角约 73.318°也超出当前高空太阳角单元，因此单独处理高度不足不能保证查询恢复。现有程序仍使用 `FallbackUtcDate=2026-09-06`，再结合协议时间生成太阳位置；记录目录名 20260907 也不能作为本次真实测试日期，板卡本次读取的系统时钟仍为 9 月 7 日。

## 处理建议与边界

当前资料足以确定数据有效域和输入一致性问题，但没有证明一项配置调整能够解决全部异常。建议将后续工作分为以下独立事项：

| 事项 | 应解决的问题 | 判定依据 |
|---|---|---|
| 配置与日志可理解性 | 注明旧 Stage3/Stage5 开关的作用范围，明确显示 M1 是否有效；失败输出不再误标 legacy | 配置、有效状态、实际输出三者一致 |
| 输入契约与可观测性 | 对未找到的目标引用、时间解释、输入单位给出明确诊断 | 保留双方原始输入及解析结果，不靠画面猜测 |
| 数据适用范围 | 将多维覆盖条件作为能力声明，不将某条轨迹验证扩大为通用结论 | 所选波段、几何、湿度、能见度、太阳条件均有来源依据 |
| 失败呈现 | 将“不具备有效物理结果”与正常黑色画面区分，防止把失败帧当作有效仿真 | 有明确无效状态和原因 |
| 背景与目标依赖 | 对背景借用目标 LOS 及旧值保留作独立设计审查 | 背景结果不被未经说明的目标状态左右 |
| 太阳加热 | 区分宽带加热数据与成像波段数据，单独声明其有效范围 | 主 LUT 通过不再被误认为加热表也通过 |

对于“3–20 km、0–50 km、经纬度不限”，现有数据不能声明支持。0 km 也不属于当前加载器允许的正距离查询，其含义必须先明确。经纬度并非简单多填两列：位置和日期时间决定太阳条件，实际大气状态也随地域和天气变化。

不存在可以直接替换当前文件、自动适配全部地点时间天气的单一通用表。MODTRAN 是按大气状态、视线几何和照明条件计算的辐射传输模型；标准大气可作为明确假设的通用基线，但不是任意场景的实况数据。[MODTRAN 官方说明](https://www.spectral.com/our-software/modtran/)

新增有效数据与现有软件接口、插值有效域、单位、完整分量及质量验证是不同工作项。扩大文件数、改 manifest 上界、放宽检查、填零、固定 tau 或打开旧开关，都不能证明获得正确结果。

## 证据文件

- `logs/p20/1020_modtran_audit/audit_summary.json`：两端身份、LUT 分组、日志计数和首批证据行。
- `logs/p20/1020_modtran_audit/board_inventory.json`：板卡两路文件清单、配置快照与版本身份。
- `logs/p20/1020_modtran_audit/local_inventory.json`：Windows 文件清单。
- `logs/p20/1020_modtran_audit/input_summary.json`：三份 CSV 行数、几何范围及可见标记统计。
- `logs/p20/1020_modtran_audit/circleDataAll*_queries.csv` 及对应 `.log`：逐行查询条件与生产查询器结果。
- `tools/p20_modtran_readonly_audit.py`：只读部署及日志核查工具；认证由调用环境提供，未写入密码。

工程当前原有大量未提交改动，本次未改写这些生产源文件。本次新增上述核查工具、报告与证据。
