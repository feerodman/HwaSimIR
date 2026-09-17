# HwaSimIR P11 SWIR / MWIR 物理链根因、修复与证据

<!-- P11_FINALIZER_STATUS_BEGIN -->

# P11 final evidence status

- Generated: `2026-09-17T02:52:20+08:00`
- Overall: **PARTIAL**

| Evidence | Status | Summary |
|---|---:|---|
| Prechange backup | `PASS` | sourceResult=PASS |
| Independent CPU / production CPU / WGL GPU | `PASS` | sourceResult=PASS; checks=194 |
| MODTRAN SWIR/MWIR humidity and five-component QC | `PASS` | sourceResult=PASS; checks=113 |
| MODTRAN camera-band LUT atomic publish | `PASS` | publishStatus=SUCCESS_ATOMIC_MERGE_PRESERVED_ALL_NON_HUMIDITY_AND_DEFAULT_ROWS; rows=1628; sha256=feb5f30ee6d4991e019185a3d2e31ae1440041ed0d21964099e0fd1c5433c57e |
| MODTRAN broadband solar-heating QC | `PASS` | sourceResult=PASS; checks=41 |
| MODTRAN solar-heating LUT atomic publish | `PASS` | publishStatus=SUCCESS_ATOMIC_MERGE_PRESERVED_LEGACY_45_ROWS; rows=93; sha256=1eee146191c0f353bef8408a116b115ca3948bb33fde10b006fc78646d8c6561 |
| Glass transmission static/CPU gate | `PASS` | sourceResult=PASS; checks=25 |
| Glass transmission WGL GPU probe | `PASS` | sourceResult=PASS |
| Protocol and formal-chain contracts | `PASS` | sourceResult=PASS; checks=65 |
| Active illumination component gate | `PASS` | rows=30; failed=0 |
| Thermal inertia component gate | `PASS` | sourceResult=PASS |
| Aero/material locality gate | `PASS` | sourceResult=PASS |
| Exhaust plume component gate | `PASS` | sourceResult=PASS |
| SWIR CPU-uniform-float pixel triplet | `PASS` | sourceResult=PASS |
| MWIR CPU-uniform-float pixel triplet | `PASS` | sourceResult=PASS |
| SWIR to MWIR to SWIR re-INIT | `PASS` | sourceResult=PASS |
| Traceable real target/sensor calibration | `NOT_VERIFIED_CALIBRATION` | Engineering assumptions are explicit; no traceable coupon/SRF/QE/optics/exposure truth was supplied. |
| VIS-SWIR | `UNSUPPORTED_REJECTED` | Unsupported protocol values are rejected and never aliased to SWIR. |
| Windows SWIR/MWIR image matrix | `HISTORICAL_DIAGNOSTIC` | historicalSourceStatus=FAIL; DDS-only nonblocking; expectedCases=86; pass=0; fail=45; missing=41; blockedCoverage=3 |
| Windows aerodynamic-heating matrix gate | `HISTORICAL_DIAGNOSTIC` | historicalSourceStatus=FAIL; DDS-only nonblocking; Aero summary is mandatory when --windows-matrix-root is supplied |
| Windows dual-band H.264 60 s | `HISTORICAL_DIAGNOSTIC` | historicalSourceStatus=FAIL; DDS-only nonblocking; seconds=60; bands=MWIR,SWIR; sourceResult=PASS |
| RK3588 cross-build | `PASS` | stage=p11-20260917-000705; elf=3a616afed1fc2d0984ce6d498e96000a8b9848bed670475b5b9fdd8297585862 |
| RK3588 atomic deployment | `PASS` | stage=p11-20260917-000705; changed=1; removed=0 |
| RK3588 rollback and return | `PASS` | stage=p11-20260917-000705; run=p11rb-20260917-003744 |
| RK3588 DDS/MPP dual-band 60 s | `PASS` | bands=SWIR,MWIR; sourceResult=PASS |
| RK3588 DDS lifecycle | `PASS` | scenarios=pause_mwir,pause_swir,receiver_restart,retained_reinit; sourceResult=PASS |
| RK3588 DDS SWIR/MWIR image matrix | `PARTIAL` | cases=20; blockers=3; sourceResult=PARTIAL; transport=DDS-only |
| NIR compatibility regression | `HISTORICAL_DIAGNOSTIC` | historicalSourceStatus=FAIL; DDS-only nonblocking; seconds=3; bands=NIR; sourceResult=PASS |
| RK build/deploy/DDS lifecycle/image/rollback identity | `PASS` | ELF/build/config/stage identities agree |

## Preserved failures and blockers

- **real-calibration / NOT_VERIFIED_CALIBRATION**
- **vis-swir / UNSUPPORTED_REJECTED**
- **windows-image-matrix / HISTORICAL_DIAGNOSTIC**
  - `missing_cases:41`
  - `failed_cases:45`
  - `matrix_root_marked_aborted`
- **windows-aero-matrix / HISTORICAL_DIAGNOSTIC**
  - `windows_aero_summary_not_supplied`
- **windows-h264-60s / HISTORICAL_DIAGNOSTIC**
  - `matrix_formal_program_identity_file_size_mismatch:HwaSim_IR`
  - `matrix_formal_program_identity_file_sha256_mismatch:HwaSim_IR`
  - `binary_preflight_identity_missing:DataDrivenTestQT`
  - `binary_preflight_formal_program_identity_bytes_invalid:HwaSim_IR`
  - `binary_preflight_formal_program_identity_file_sha256_mismatch:HwaSim_IR`
  - `binary_preflight_formal_program_identity_bytes_invalid:HwaSim_IR_VideoDisplay`
  - `binary_preflight_formal_program_identity_role_missing:DataDrivenTestQT`
  - `formal_program_identity_mismatch:HwaSim_IR:bytes`
  - `formal_program_identity_mismatch:HwaSim_IR:sha256`
  - `formal_program_identity_mismatch:HwaSim_IR_VideoDisplay:bytes`
- **rk3588-dds-image-matrix / PARTIAL**
- **nir-compatibility / HISTORICAL_DIAGNOSTIC**
  - `matrix_formal_program_identity_file_size_mismatch:HwaSim_IR`
  - `matrix_formal_program_identity_file_sha256_mismatch:HwaSim_IR`
  - `binary_preflight_identity_missing:DataDrivenTestQT`
  - `binary_preflight_formal_program_identity_bytes_invalid:HwaSim_IR`
  - `binary_preflight_formal_program_identity_file_sha256_mismatch:HwaSim_IR`
  - `binary_preflight_formal_program_identity_bytes_invalid:HwaSim_IR_VideoDisplay`
  - `binary_preflight_formal_program_identity_role_missing:DataDrivenTestQT`
  - `formal_program_identity_mismatch:HwaSim_IR:bytes`
  - `formal_program_identity_mismatch:HwaSim_IR:sha256`
  - `formal_program_identity_mismatch:HwaSim_IR_VideoDisplay:bytes`

This file is generated from the cited JSON/CSV artifacts. It does not override their gates.

<!-- P11_FINALIZER_STATUS_END -->

> 文档状态：**结构化证据驱动；标题下的 `P11_FINALIZER_STATUS` 块为动态最终状态**
> 初始叙述快照时间：2026-09-16
> 本文不以文字覆盖原始证据。按用户最终口径，正式传输验收只采用 RK3588 DDS/MPP→Windows 真解码证据；Windows UDP/TCP matrix/H.264 仅为历史诊断、非阻塞且不要求重跑。最终状态由 `tools/p11_finalize_delivery.py` 从显式指定的 DDS evidence roots 生成；没有生成块时应视为 DRAFT。

## 1. 结论先行

P11 已在公式与源码层找到并修复多个会直接改变正式像素的根因：SWIR 中心波长单点代替积分、无量纲反射项混入辐亮度、近零透过率被回退为 1、SWIR profile 范围冲突、把旧 Stage3/5 开关误当成 M1 启用门、MWIR 2.5 W/(m²·sr·µm) 公共窗口过早裁剪、主动光仅 NIR 语义，以及气动温升“计算了但未以合法局部性进入成像”。RK 的 `Failed to convert image` 也已定位为 Panda GLES 浮点 framebuffer/texture negotiation 将 half-float attachment 推回 4-byte RGBA，而 `copy_image` 缺少该 GL_RGBA 转换路径；Stage7/weather/material-ID 均为排除项，不再列作根因。

已有独立 CPU 参考与真实 Windows WGL 浮点 GPU 探针：194 项数值检查 PASS，GPU 对同语义 CPU 的最大相对误差为 0.001394865，低于预先冻结的 2% 阈值。湿度辐射 LUT 和宽谱太阳加热 LUT 已从真实 MODTRAN 5.2.1 输出生成并通过单位/Jacobian/查询检查。

但尚不能宣布 P11 整体完成：

- 早期 `validation-target-near-20260916` 的 MWIR 近景三变体曾因仿真时间与边界帧数门失败，该失败证据继续保留；后续 `validation-rgba32f-{swir,mwir}-20260916` 已在相同 sourceSeq、允许的 2 ms sim-time/边界帧容差内通过，并验证实际 RGBA32F 32/32/32/32。完整矩阵仍以最终显式传给 finalizer 的 evidence root 为准。
- 两个独立的受控黑体 fixed 轮已完成正式场景 CPU 参考—运行 uniform—RGBA32F 渲染目标/PFM 指定像素三方对照：SWIR 最大 GPU-vs-shader 误差 0.1001%，MWIR 为 0.0385%，均低于 2% 冻结门。该证据使用 rho=0 的理想黑体，不替代玻璃、非零反射表面或真实器件标定。
- 历史 Windows UDP/TCP 根（包括 `final-matrix-frame-contract-20260916`、后续未完成矩阵与 `h264-60s-frame-contract-20260916`）只保留作诊断，均为非阻塞项且不再要求重跑。`final-matrix-frame-contract-20260916` 的旧 85/86 仍标记为 **`SUPERSEDED_INVALID_FOR_AERO`**，不得迁移为 DDS 正式统计。三个 coverage 阻塞项仍须原样保留，除非有新增 metadata/真实 2 km 数据实际解阻。
- RK3588 当前 stage `p11-20260917-000705` 已完成交叉编译、原子部署、SWIR/MWIR 各 60 s DDS + RK MPP + Windows 解码验收和 v2 回滚恢复。SWIR 六级计数均为 3602，MWIR 均为 3600；正式回滚根 `rollback-p11-20260917-003800` 又分别完成旧版本 NIR 1021/1021/1021/1021 与恢复后 P11 SWIR 902/902/902/902 的 completed/sent/received/decoded 守恒。旧 `rollback-p11-20260916-202709` 为 **`SUPERSEDED_INVALID_ROLLBACK_RECEIPT`**。早期失败 pilot、Stage7/weather/material-ID 排除试验和 RGBA16F 初次失败迭代继续保留，不因最终通过而删除。
- 玻璃已接通按波段 tau 的单层直透合成，并通过静态 CPU 与 Windows WGL GPU 门；该模型仍明确不包含折射、Fresnel 角度项或内部多次反射，最终板端像素状态由 finalizer 读取实际 DDS 图片证据决定，不能称为实测玻璃标定。
- 民用车材料、温度、羽流及理想传感器参数是显式工程假设；没有与真实样片、真实目标或真实器件实测标定，验证等级必须保持 **NOT_VERIFIED_CALIBRATION**。

## 2. 状态与证据规则

| 状态 | 含义 |
|---|---|
| `PASS` | 命令实际运行，有可读结果、阈值和原始证据 |
| `PARTIAL` | 有部分有效证据，但未满足完整验收口径 |
| `FAIL` | 实际运行并触发门禁失败；保留不改 PASS |
| `PENDING` / `NOT_RUN` | 脚本或计划可能已存在，但未获得完整运行证据 |
| `BLOCKED_IMPLEMENTATION` | 明确缺少产品路径实现，不能靠配置或日志替代 |
| `NOT_VERIFIED_CALIBRATION` | 实现/现象可验证，但没有可追溯的实物测量比对 |

本文引用的相对路径均从仓库根 `D:\HwaSimIR` 起算。每个结论以相应 JSON/CSV/日志内的结果为准，不以文档文字覆盖原始失败。

## 3. 参考资料：原理—实现—差异—处理

| 原始资料 | 实际阅读位置 | 可用原理 | P11 处理与边界 |
|---|---|---|---|
| 李晨阳《基于三维场景的红外成像仿真系统及实现》 | PDF p25–27（正文 11–13）Planck/单位/Kirchhoff；p31（正文 17）太阳直射与天空；p37（正文 23）表面和传输公式；p44–45（正文 30–31）可见颜色仅作材料标识；p54–59（正文 40–45）局部热状态；p66（正文 52）探测器积分；p83–84（正文 69–70）太阳/热辐射与显示映射 | `Lsurface=εB+ρE/π`，`Lsensor=τLsurface+Lpath`；光谱量和显示映射分开 | 用于公式和系统分层依据，不把论文图像当定量真值 |
| `ondulus ir 红外图片示例/OndulusIR_Overview20210524.pptx` | s5 总链；s18/s23 辐射公式；s19/s21 SWIR 散射；s26/s31 传感器积分；s63–64 光谱；s65–66 SWIR 1.5–2.5 / MWIR 3–5 示例；s67–72 为 preliminary；s72 的 850 nm 主动光 | 波段相对应、大气散射、反射/自发射和传感器响应应分层 | 只作现象/架构参考；`preliminary` 图及 850 nm 不是 SWIR/MWIR 定量标定 |
| `MODTRAN_R_5.2.1.pdf` 与本机 MODOUT | PDF p16–20 只用于 CARD1A、`H2OSTR`、`H2OAER`、`AERRH` 输入卡；输出列以本机 [`MODOUT2.txt` 第 11 行](../logs/p11/modtran/mwir_ground_grid/MWIR_flux_tar0p001_vis6_sza20/MODOUT2.txt) 和同轮 [`MODOUT1.txt` 第 796–800 行](../logs/p11/modtran/mwir_ground_grid/MWIR_flux_tar0p001_vis6_sza20/MODOUT1.txt) 交叉核对 | 水柱缩放、气溶胶湿度联动、边界层 RH，以及本机 5.2.1 实际列头/单位 | 手册 p16–20 不冒充输出列页码；输出列页码尚未从本机手册定位时，明确使用同轮原始表头和数值对照，不编造页码、不伪装 MODTRAN6、不任意缩放追平 legacy |

## 4. 修复前基线与可回滚性

- 修改前 Git HEAD：`5784ec00b46540124f3138a020b9c4b3a2389fad`。
- 修改前工作区只见本 P11 prompt 为 untracked；基线捕获结果为 PASS。
- 基线本地清单 10,183 项，清单 SHA-256 `5ef8596a6698ba569383bbd31eebd173e9cff893e1cfbe181f193fed0359a059`。
- 证据：[`logs/p11/baseline/prechange_20260916_0126/baseline_summary.json`](../logs/p11/baseline/prechange_20260916_0126/baseline_summary.json)。
- 旧 SWIR 1.5–2.5 μm profile 已归档为 `HwaSim_IR/Bin/Config/SensorWave/Archive/P11/default_SWIR_legacy_1p5_2p5.json`，SHA-256 `91715e10541c46010df4ce111c97e3b007ca427c6e11d497a969bbdb9945ad3b`。
- GMC Van 原始 FLT 不改写；转换审计与中间产物仅留在 `logs/p11/work/gmc_conversion`，不进入正式配置、板端部署或可分发交付包。

## 5. 实际数据流与消费边界

```text
CommonData / INIT / 环境与目标帧
  -> 协议数值到内部波段映射（SWIR=0, NIR=1, MWIR=2）
  -> 全键 targetType + targetPlatID + targetID 解析模型和每帧状态
  -> 模型 material-ID XML / band optics / 分区温度 / 局部热区与羽流
  -> 环境、太阳位置、阴影、主动光两段路径
  -> MODTRAN SI LUT（tau, path thermal, direct solar, sky diffuse, path scattering）
  -> CPU IRRadianceModelV2 分量与正式 M1 有效性门
  -> shader uniform，目标片元上的材料/法线/热区掩码计算
  -> 映射前 SI 原始辐射（Windows/WGL 为 RGBA32F；RK/Linux GLES 为 RGBA16F；PFM 保存 RGB 通道）
  -> Stage6 固定映射或 AGC，然后 AA/噪声/标注
  -> H.264 编码，TCP v3 或 DDS Bytes
  -> VideoDisplay 解码、录像、frameSeq/sourceSeq 身份索引与 PNG
```

正式 M1 是独立门：`[M1NirMwirPhysics] EnableRuntime/EnableSWIRRuntime/EnableMWIRRuntime=true`。`[Stage3] UseModtranTauForAtmosphere=0` 以及 Stage5 的旧 path/sky/solar runtime 开关为 false，并不意味着 M1 没启用。本轮保持旧开关不乱开，避免同一路径项重复加入。正式像素状态由 `[Stage5 RadianceComponents] formalRuntimeAffectsImage=1 finalOutput=M1` 定义，超出 LUT 支持域时失败关闭，不默默退到 legacy 冒充物理结果。

## 6. 根因、触发条件、修复和数值

### RC-01：SWIR 用 1.80 μm 单点代替 1.1–2.5 μm 响应积分

**状态：PASS（公式/实现/GPU）；正式动态图像状态待 DDS image evidence root 实测收口。**

- 修复前触发：`band != MWIR` 的所有波段都调用 `planckRadiance(bandCenterUm)`，SWIR 中心固定为 1.80 μm。
- 正式修复：`IRRadianceModelV2.cpp:534-557` 对 SWIR 1.1–2.5 μm 做 10 子区间复合 Simpson 积分，除以波段宽度，输出响应加权平均谱辐亮度 W/(m²·sr·μm)；GPU 采用同等采样语义。
- 独立 16,384 子区间参考与生产 C++ 对比：300 K 为 `0.000622067668` 对 `0.000621201225`，相对误差 0.001394787；900 K 为 `930.666704` 对 `930.670454`，相对误差 4.03e-6。
- 修复前/after 数值差（同一 Planck 常数）：300 K 时 1.80 μm 单点 `1.69133e-5`，波段平均 `6.21201e-4`，旧值仅为新值 2.72%；500 K 时为 `0.719057` 对 `2.485182`。
- 证据：[`reference_results.json`](../logs/p11/reference/final_gpu/reference_results.json)、[`gpu_probe.json`](../logs/p11/reference/final_gpu/gpu_probe.json)。

### RC-02：无量纲经验反射项混入物理辐亮度

**状态：PASS（公式/生产探针/太阳方位图组）；正式板端图像状态待 DDS image evidence root 实测收口。**

- 修复前触发：`rho * solarStrength * NdotL * textureLuma * solarReflectanceWeight` 为无量纲，却与 W/(m²·sr·μm) 的 Planck 项相加；旧 M1 MWIR 也将这一 `reflectedRadiance` 带入表面辐亮度。
- 正式修复：`IRRadianceModelV2.cpp:265-273` 仅保留旧项为显式 legacy 诊断；`333-390` 用 `rho/pi * (E_direct*NdotL*Vsun + E_sky*Vsky)` 和主动光单独路径组成 M1，不再把旧项加入正式链。
- 独立基准例：太阳反射 `0.0636619772`，天空反射 `0.0318309886`，合计 `0.0954929659 W/(m²·sr·μm)`；分别关闭 direct/sky 后相应项精确归零。
- `production_*_legacy_dimensionless_excluded` 检查证明改动 legacy 无量纲参数不会改变正式输出。
- `solar_az_*` 五场景×双波段 10/10 聚合 PASS，`solar_morning/noon` 双波段 4/4 聚合 PASS。唯一 controlled solar-off MWIR 失败来自指定证据帧 AGC 低对比有效性门，不是反射公式或正式 M1 链失败。

### RC-03：合法 `tau <= 1e-6` 在无 reason 时被回退为 1

**状态：PASS（源码触发和数值）；已排除“仅因当前值小就回退”。**

- 修复前确认触发条件：`tauUp <= 1e-6 && tauFallbackReason == "none"`，会将合法不透明路径改成 1。它是真实源码缺陷，但没有证据说明 P10 那一张具体图必然触发过，两者分开陈述。
- 修复：`IRRadianceModelV2.cpp:220-234` 只对 non-finite 回退为 1，对越界值夹取并标 invalid，`0..1` 内的 0 和近零值原样保留。`HwaSimIR.cpp:14776-14807` 同样只处理 non-finite/越界。
- 数值：`tau=0` 时传感器仅保留 `Lpath=0.125`；`tau=1e-8` 时 SWIR 结果 `0.1250000000049696`，MWIR 为 `0.12500000746382484`，未变成全透明。`path_added_once=1.525` 与独立公式一致。

### RC-04：SWIR profile 1.5–2.5 μm 与生产模型 1.1–2.5 μm 冲突

**状态：PASS（profile/协议切换）。**

- 处理决策：在无可追溯实测 SRF 的条件下，采用当前代码的 1.1–2.5 μm 理想矩形响应，名称为理想参考，不声称是原始器件带宽。
- 新 profile：`default_SWIR.json`，800×800，`Response.Type=RectangularBand`，1.1–2.5 μm，SHA-256 `a8b40cd4d5e72128dd9cc36fbe53e4a11a1841050f0b8e85c5a7f457019d8ca3`。旧 1.5–2.5 μm 文件保留在 P11 archive。
- 同一进程实际切换为 SWIR 360 帧→MWIR 361 帧→SWIR re-INIT 361 帧；sent/accepted/captured/executed/output/received/written/MP4 各轮全等，overflow/overwrite/drop 均 0，profile/M1/MODTRAN/shader 均刷新。
- VIS-SWIR/LWIR/VIS 未伪装成 SWIR；值 3、4 及 `VIS-SWIR` 实际退出码 64。
- 证据：[`p11_band_switch_reinit_summary.json`](../logs/p11/band_switch/final17/p11_band_switch_reinit_summary.json)。

### RC-05：错把旧 MODTRAN 开关当成 M1 正式链开关

**状态：PASS（消费路径/切换日志）。**

- `UseModtranTauForAtmosphere=0` 是 Stage3 旧 tau-only 试验；`UseModtranPath/Sky/SolarRuntime=false` 是 Stage5 旧路径。
- 正式 M1 由 `EnableRuntime=true`, `EnableSWIRRuntime=true`, `EnableMWIRRuntime=true`, `CompareOnly=false` 控制，并以查询有效性门决定是否进入像素。
- 修复未采取“把旧开关全打开”；这避免了 path/sky/solar 再次叠加。日志现在输出 `formalRuntimeAffectsImage`, `finalOutput`, `finalTauUp`, `finalPathRadiance`, `finalSensorInputRadiance` 和 SI 单位。

### RC-06：MWIR 2.5 W/(m²·sr·μm) 公共窗口导致显示前动态范围丢失

**状态：PASS（范围审计/浮点附件/公式链）；正式动态呈现状态待 DDS image evidence root 实测收口。**

- 民用车 475 K、MWIR ε=0.88 的局部尾管辐亮度为 `51.0265866 W/(m²·sr·μm)`。旧 2.5 窗口的归一化值为 20.4106，必然裁剪，不是单纯“亮一点”的可逆曝光问题。
- 新公共窗口：SWIR 0–40，MWIR 0–64 W/(m²·sr·μm)。SWIR 上限依据完整湿度网格重算的民用目标保守包络 35.0474 留出余量；475 K 尾管对 MWIR 64 的归一化值为 0.797290，均保留未映射动态范围。
- shader 输出未映射 SI 辐亮度，固定映射/AGC 放在最后显示阶段。Windows/WGL 正式中间附件保持 RGBA32F；RK/Linux GLES 使用 RGBA16F，并以实际 `half_float`、2-byte component 和 16/16/16/16 framebuffer bitplane 门禁 fail closed。二者保持相同的 W/(m²·sr·μm) 物理域，不以格式差异改变辐亮度语义。
- 旧根 86/86 场景的 raw 均为 800×800×3、全 finite、非零并标记 `physicalRadiance=1`，旧 258 个变体的 `formalChainGates` 也通过；但该根的 aero 配置无效，故这些只是窗口/附件能力的历史证据，不能充当 DDS 正式 PASS。当前 DDS image root 必须独立报告 raw、正式链门和所有场景失败。
- 证据：[`logs/p11/display_window/current.json`](../logs/p11/display_window/current.json)。该审计是湿度 LUT 合并前快照，其源码保护与窗口结论有效，最终交付应重生输入哈希。

### RC-07：0.85 μm NIR 主动光不能作为 SWIR/MWIR 带内光

**状态：PASS（单元物理/正式图片矩阵）。**

- 修复前默认为 NIR 0.85 μm / 0.05 μm，且宣告支持不包含 SWIR；对 SWIR/MWIR 期待带内反射是错误前提。
- `IRActiveIlluminator` 现在支持 NIR/SWIR/MWIR/FollowSensor，检查光源标签、光谱交叠宽度、锥角、几何遮挡以及入射/出射两段 tau。正式配置为 SWIR 1.55 μm / 0.10 μm；是显式参考光源，不冒称实测发射谱。
- 30 项检查 PASS：0.85 μm 对 SWIR=0，NIR 标签却填 MWIR 波长被拒绝，带外=0，锥外=0，遮挡=0，1/2/5/10 km 两程透过均使用正确。
- `active_off`、`active_in_band_on`、`active_in_band_miss`、`active_out_of_band` 四场景×双波段 8/8 聚合 PASS；这只验证配置光源与正式链响应，不是实测灯源标定。
- 证据：`logs/l2-active-qc-20260916-051119/l2_active_illuminator_qc.csv`。

### RC-08：气动加热和发动机热源必须保持局部性

**状态：实现/独立数值检查 PASS；旧正式动态矩阵 `SUPERSEDED_INVALID_FOR_AERO`，新正式图像闭环 PENDING。**

- 当前 runner 已明确要求 `EnableAeroThermalModel=true`、`ApplyAeroToRadiance=true`、`AeroApplyOnlyBand=SWIR_MWIR`；CPU 整体机身偏移固定为 0 K，鼻/前缘/后部候选温升仅通过模型边界归一化 GPU 掩码作用于局部片元。该要求必须由运行时日志和原始浮点像素共同证明，不能以脚本字段代替动态证据。
- 民用车发动机舱和尾管是材料 ID 15/16；engine-off 均为 303 K，engine-on 分别是 345 K/475 K，不应用到整车。
- 热惯性模型是六法向箱的一阶表面能量平衡；反射立即变化，温度/MWIR 发射按状态渐变。这是工程代理，不是 CFD 或实测热标定。
- `aero locality` 和 `thermal inertia` 独立检查证明公式、局部候选温升和时间常数实现；但旧 Windows 速度场景运行时 `ApplyAeroToRadiance=0`、`aeroAppliedToRadiance=0`，不能据旧 `speed_dynamic_0_15_30mps` 图片声称正式图像已应用气动加热。
- 最终动态门必须绑定运行的三端 Windows 二进制哈希，要求全体速度 case 使用 `ApplyAeroToRadiance=1`、`AeroApplyOnlyBand=SWIR_MWIR`、`wholeBodyAeroDeltaK=0`；非零速度局部 effective 温升大于零，并在相同静态相机/目标几何下证明 raw target ROI 相对零速发生非平凡变化。证据由新矩阵根的 `aero_matrix_check.json` 给出。
- 独立证据：[`aero locality`](../logs/p11/aero_material_locality/20260916-051031/p11_aero_material_locality_summary.json)（7 项 PASS）、[`thermal inertia`](../logs/p11/thermal_inertia/20260916-051030/p11_thermal_inertia_summary.json)（10 项 PASS）。

### RC-09：羽流与固体尾管容易重复加入或重复乘 opacity

**状态：PASS（数值/模型局部性/多角度正式图）；SWIR 可见效应弱。**

- 固体尾管保留为模型材料分区；空间羽流由独立几何绘制，CPU 目标中心分量不把局部热峰或羽流当成整机面积再加一次。
- 源辐亮定义为 `epsilon * max(B_band(T)-B_band(Tambient),0)`，发射辐亮度再乘一次 opacity；检查明确 `opacityApplicationCount=1`。
- 62 项检查 PASS，其中 SWIR/MWIR 积分均与独立 16,384 子区间参考比较。民用车小型排气羽流的温度/几何是工程假设，不是特定发动机标定。
- rear/side 的 plume on/off 四场景×双波段 8/8 聚合 PASS。SWIR rear 帧均值只增加约 `0.003054%`，side 增加约 `0.008821%`；MWIR 对应约 `1.82394%` 与 `4.24900%`。因此只能证明正式像素存在可审计差异，不能声称 SWIR 羽流强可见或已经实测标定。
- 证据：[`p11_plume_physics_summary.json`](../logs/p11/plume_physics/20260916-051032/p11_plume_physics_summary.json)。

### RC-10：几何不等于红外材料，可见 RGB 不能当光谱参数

**状态：PASS（资产/分区/能量守恒）；实物标定 NOT_VERIFIED_CALIBRATION。**

- GMC Van FLT 直接 `flt2egg` 在未支持 opcode 135 和 `extra data at end of file` 失败；OSG 桥接只得到 0 texture/0 LOD/语义丢失的诊断几何，且源资产旁未找到可分发许可。因此它未接入生产，不因格式问题阻塞整阶段。
- 正式示例改用项目自建 CC0 民用箱式车：840 顶点、175 多边形、6 个红外分区（车漆/金属/玻璃/橡胶/发动机舱/尾管）。
- 受控样片架也为 CC0：250/300/350/475 K 黑体、300 K 灰体、高反射样片、玻璃、275 K 冷灰体，每个样片的 SWIR/MWIR `rho + epsilon + tau = 1`。
- 可见 PPM/MTL 只用于方向识别；所有红外物性均来自显式 CSV/XML/清单，并标注为理想参考或工程假设。

### RC-11：玻璃后背景不能按不透明表面或全量 path 重复合成

**状态：PASS（单层直透公式/静态 CPU/WGL GPU）；真实玻璃标定 NOT_VERIFIED_CALIBRATION。**

- 分波段材质 uniform 使用显式 `tau_material`；只有带独立 P11 材料 ID 且 tau>0 的几何进入预乘 alpha 透明合成，旧未标记几何失败关闭为不透明。
- 正式公式为 `Lout = tau_atm*(epsilon*B + rho*E/pi) + Lactive + (1-tau_material)*Lpath + tau_material*Lbehind`，alpha 为 `1-tau_material`，不会把完整前景 path 再叠加到已含后景大气的透射项上。
- SWIR 玻璃 tau=0.85 的算例得到 0.223733 W/(m²·sr·μm)，旧错误全量 path 重复加入为 0.257733、偏差 0.034；MWIR tau=0.05 得到 0.903082，旧错误为 0.915582、偏差 0.0125。
- 25 项静态/CPU 检查见 [`p11_glass_transmission_check.json`](../logs/p11/reference/glass_transmission/p11_glass_transmission_check.json)；真实 WGL 探针见同目录 `gpu/p11_glass_gpu_probe.json`。边界仍是直线单层模型，无折射、Fresnel 角依赖和内部多次反射。

### RC-12：RK Panda GLES 浮点 attachment negotiation 触发缺失的 RGBA32F 转换路径

**状态：PASS（GDB 根因、后端定向修复、实际 16/16/16/16、双波段 60 s DDS/MPP 和回滚闭环）。**

- 触发条件：RK3588 Mali OpenGL ES 正式双 pass 链创建映射前浮点 raw framebuffer。EGL 创建实际 half-float output 后，Panda `add_render_texture` / `rebuild_bitplanes` 又把 attachment texture 的 component type 从 half-float 推为 4-byte float；随后 `copy_image` 收到 `GL_RGBA`、4 components、component width=4，但该 Panda GLES 路径没有相应转换分支，于是断言 `Failed to convert image`。
- 根因边界：GDB 证据位于 `logs/p11/rk3588/diagnose-gsg-registers-20260916-183912/gdb_backtrace.txt`。关闭 Stage7 weather/cloud、material-ID 仍可复现，因此 Stage7 3-D RGBA、weather 和 material-ID 是已排除项；早期把断言归因于 Stage7 `set_ram_image_as` 的判断不成立。相关 Stage7/R8 兼容修改可以保留，但不冒称本根因修复。
- 正式修复：Windows/WGL 继续使用 RGBA32F。RK/Linux GLES 请求 RGBA16F；必须先让 EGL 以 `float_color=true` 创建实际浮点 output，创建后、attachment 前只把 Panda 后续 texture negotiation hint 窄化为非 float-color，避免 `rebuild_bitplanes` 覆盖 half-float；attachment 后再次显式恢复 `T_half_float/F_rgba16`，清除 RAM image，并在第一帧验证实际 attachment 仍为 `RTM_bind_or_copy`。该 workaround 不重建 EGL output，日志标记 `Panda_rebuild_bitplanes_component_override_avoided` / `Panda_float_color_component_override_avoided`。
- 精度边界：binary16 对正常数的 round-to-nearest 理论相对舍入界为 `2^-11 = 0.00048828125`，近零绝对 allowance 为 `2^-25 W/(m²·sr·μm)`，最大有限值为 65504；冻结的 GPU 一致性门仍为 2%。最终两波段日志均证明 texture component type=`half_float`、component width=2、components=4、framebuffer RGB/A bits=`16/16/16/16`，PFM 全 finite 样本均落在 binary16 lattice，无 overflow。
- 失败迭代继续保留：`pilot-p11-20260916-172419` 保留最初 assertion；`pilot-p11-20260916-181851` 保留 601/601 守恒但 fatal 的 GDB 前迭代；`pilot-p11-20260916-191207` 已请求 RGBA16F、两帧 PFM 也满足 half-lattice，却因 Panda negotiation 仍覆盖 texture、缺合格 dual-pass 日志且继续 assertion 而保持 FAIL。这些失败不能被最终 PASS 覆盖或删除。
- 最终闭环：build/deploy stage `p11-20260917-000705`，ELF SHA-256 `3a616afed1fc2d0984ce6d498e96000a8b9848bed670475b5b9fdd8297585862`，source manifest SHA-256 `1678e6172a00ba24e02d5ed7697574d0b72a9b1f170d7943237aab7024dd2550`，config manifest SHA-256 `efa7aa0b4b068b53e8640a4bbbccfc300dc672cfe2c2ce07814024af4d2c5ac5`。[`acceptance-p11-20260917-000705`](../logs/p11/rk3588/acceptance-p11-20260917-000705) 的 SWIR/MWIR 六级计数分别全为 3602/3600，输出 FPS 为 59.912050/59.881678，p95 延时为 49.196/44.388 ms，>80 ms 为 127/90；所有声明 gate 均 PASS。[`rollback_receipt.json`](../logs/p11/rk3588/rollback-p11-20260917-003800/rollback_receipt.json) 是正式 v2 DDS 闭环回执。旧 `rollback-p11-20260916-202709` 为 `SUPERSEDED_INVALID_ROLLBACK_RECEIPT`。

## 7. 光谱、单位与独立 CPU/GPU 基准

生产接口的定义为响应加权波段平均谱辐亮度：

```text
L_band = integral(R(lambda) * L_lambda d lambda) / integral(R(lambda) d lambda)
unit   = W/(m^2 sr um)
```

对不透明 Lambert 表面：

```text
L_surface(lambda) = epsilon(lambda) B(lambda,T)
                  + rho(lambda)/pi * [E_sun_normal(lambda) max(n dot s,0) V_sun
                                      + E_sky_surface(lambda)
                                      + E_active_surface(lambda)]

L_sensor(lambda) = tau_LOS(lambda) L_surface(lambda) + L_path(lambda)
```

天空与主动光项已是到达表面的辐照度，不再无条件乘同一余弦或同一段 tau。主动光将光源→表面与表面→传感器分开。`TOTAL_RAD` 不直接加到自渲染目标，避免把目标/地表贡献再加一次。

冻结阈值：生产 CPU 对独立 CPU 相对误差 1%，GPU 对同语义 CPU 相对误差 2%，近零绝对误差 `1e-8 W/(m²·sr·μm)`，tau 绝对误差 `1e-12`。本次 194/194 检查 PASS；10 个 WGL GPU 样本的最大相对误差 0.001394865。这是实现一致性，不等于现实测量精度。

## 8. MODTRAN 数据：复用与新生成

### 8.1 已复用

- 原有 MWIR 335 行仅复用其实际谱区和已定义的旧 tau/path-thermal 能力。它们从 3 km 同高、1 km 距离开始，缺 SZA/direct-solar/sky-diffuse/path-scattering，不进入需要五分量的正式太阳反射路径。
- 旧 `default` 类别保留为独立 legacy 类别，不伪装成 76.18% 数字湿度。

### 8.2 新生成

- 正式 `band_lut_si.csv`：1628 数据行，SHA-256 `feb5f30ee6d4991e019185a3d2e31ae1440041ed0d21964099e0fd1c5433c57e`。其中 SWIR 144，MWIR 479，NIR 1005；添加 RH30/60/85，海拔 0.001/1 km，距离 0.1/0.5/1 km，能见度 6/23 km，SZA 20/45/70°，五个分量。
- 湿度网格由 504 次真实 MODTRAN 分量执行组成 216 个五分量顶点；物理 QC 113/113 PASS，C++ query 29/29 PASS。RH 插值对 tau 在 optical depth 域线性，辐射/辐照度在值域线性，范围外 fail closed。
- 正式 `solar_heating_lut_si.csv`：93 数据行（45 legacy + 48 P11），SHA-256 `1eee146191c0f353bef8408a116b115ca3948bb33fde10b006fc78646d8c6561`。P11 数据是真正 0.30–2.50 μm 宽谱积分，来自 96 次真实运行；QC 41/41 PASS，C++ query 34/34 PASS。
- 单位转换显式使用面积 `1e4` 与波数→波长 Jacobian `1e4/lambda_um^2`；最大谱积分一致性误差为 1.201594e-6，不用任意倍数追平 legacy。
- 证据：[`P11_MODTRAN_HUMIDITY_AND_SOLAR_DELIVERY.md`](../logs/p11/modtran/P11_MODTRAN_HUMIDITY_AND_SOLAR_DELIVERY.md)及其列出的 raw input/output/publish manifest。

### 8.3 本机输出列、单位与排除政策

- 本机 MODTRAN 5.2.1 `MODOUT2` 实际表头同时列出 `TOT_TRANS`、`PTH_THRML`、`THRML_SCT`、`SURF_EMIS`、`SOL_SCAT`、`SING_SCAT`、`GRND_RFLT`、`DRCT_RFLT`、`TOTAL_RAD`、`REF_SOL` 和 `SOL@OBS`；证据为同轮原始 [`MODOUT2.txt` 第 11 行](../logs/p11/modtran/mwir_ground_grid/MWIR_flux_tar0p001_vis6_sza20/MODOUT2.txt)。本机输出使用 `PTH_THRML`，不是在报告中凭空改名为 `THRML_EM`。
- 同轮 `MODOUT1` 第 796–800 行明确给出 radiance 单位 `WATTS/CM2-STER-XXX`，并将 path thermal、surface emission、path-scattered solar（total/single）、ground reflected、total radiance 和 total trans 分列。正式转换按每波数原生量逐点执行面积换算和波数→波长 Jacobian 后再作波段响应积分；tau 保持无量纲。flux-table `DOWNWARD` 按其原始通量单位独立换算，不与 radiance 列混用。
- 正式五分量只取 `COMBIN TRANS`/`TOT_TRANS`、`PTH_THRML`、目标 `SOL TR`、flux-table `DOWNWARD` 和 LOS `SOL_SCAT`。`TOTAL_RAD`、`SURF_EMIS`、`GRND_RFLT`、`DRCT_RFLT` 不作为目标无关 path 加到自渲染目标；`THRML_SCT`/single-scatter 子项不在已选 `SOL_SCAT` 后重复相加；`REF_SOL` 与 `SOL@OBS` 只保留为诊断列，不混用参考位置与观测路径。
- 上述列选择和单位转换见 [`mwir_ground_grid/DELIVERY.md`](../logs/p11/modtran/mwir_ground_grid/DELIVERY.md) 第 10–12 行及原始 MODOUT。当前可确认的手册页 p16–20 只证明湿度输入卡语义；输出列若无已定位的本机手册页码，必须如实标为“由本机表头/同轮 MODOUT1 数值对照核对”，禁止编造页码。

## 9. 民用模型、样片与许可

| 资产 | 来源/许可 | 用途 | 边界 |
|---|---|---|---|
| P11 Civil Panel Van | 项目自建，CC0-1.0 | 正式民用车、材料分区、局部发动机/尾管/羽流 | 物性为工程假设，未实测标定 |
| P11 Controlled IR Sample Rack | 项目自建，CC0-1.0 | 理想黑体/灰体、高反射、玻璃的公式和分区基准 | 仅是数值标准，不是物理黑体炉测量 |
| GMC_Van_White FLT 及桥接派生物 | 本地来源，未找到可分发许可 | 仅本地转换兼容性审计 | 排除在交付 ZIP、公开仓库和板端部署外 |
| 论文、Ondulus PPTX | 仓库内参考文档 | 原理/现象参考 | 不作资产重分发，不作定量真值 |

完整条目见 [`asset_license_manifest.json`](../logs/p11/delivery/manifests/asset_license_manifest.json)。

## 10. Windows 入口、历史诊断与正式 DDS 接收边界

### 10.1 历史 Windows 60 s TCP 通过（仅诊断、非阻塞）

- 波段切换/re-INIT：同一 HwaSim_IR PID 19380、同一 VideoDisplay PID 42568 完成 SWIR→MWIR→SWIR，每轮输入—输出—接收—录像一一保存。
- 接收器已将 TCP v3 JSON 和 DDS AU SEI 的 sourceSeq 来源分开标记；本节 Windows 60 s 验证明确定义为 `TCP_PACKET_V3_JSON` + FFmpeg/libx264，不冒称 DDS/MPP。
- SWIR 60 s：sent/accepted/receiver/written/MP4 均为 3601（包含 60.000 s 端点），800×800、实测 MP4 60.014266 s、121 个关键帧、decodeErrors=0。input-to-display 平均/p95/p99/最大为 `39.759692/154.419/651.7449/820.672 ms`，>80 ms 为 `219/3601`（6.081644%）。
- MWIR 60 s：上述五级计数均为 3600，实测 MP4 59.9976 s、120 个关键帧、decodeErrors=0。input-to-display 平均/p95/p99/最大为 `14.084221/21.88092/34.865455/112.6832 ms`，>80 ms 为 `5/3600`（0.138889%）。
- 这组历史二进制的两轮 H.264、帧身份和计数守恒门均 PASS；80 ms 在此轮是全量报告阈值，不是 overall PASS 的硬门。因此不得把 H.264 PASS 改写为“全帧低于 80 ms”，尤其不能隐藏 SWIR 长尾。按 DDS-only 口径，它只作历史诊断，不是正式验收输入，也不再要求重跑。
- 证据：[`SWIR 60 s summary`](../logs/p11/windows/h264-60s-frame-contract-20260916/SWIR/p11_windows_h264_acceptance_summary.json)、[`MWIR 60 s summary`](../logs/p11/windows/h264-60s-frame-contract-20260916/MWIR/p11_windows_h264_acceptance_summary.json)。

### 10.2 旧失败证据与后续代表性通过

- [`validation-target-near-20260916/MWIR/target_near_100m/case.json`](../logs/p11/windows/validation-target-near-20260916/MWIR/target_near_100m/case.json) 是保留的早期 FAIL，不能事后删除或改写。
- 后续 [`validation-rgba32f-mwir-20260916`](../logs/p11/windows/validation-rgba32f-mwir-20260916/MWIR/target_near_100m/case.json) 与 [`validation-rgba32f-swir-20260916`](../logs/p11/windows/validation-rgba32f-swir-20260916/SWIR/target_near_100m/case.json) 的代表性近景 case 均为 PASS；它们和后来执行的 `final-matrix-frame-contract-20260916` 都只作为演进/历史证据，最终正式场景状态取当前板端 DDS image evidence root。
- 每个场景只有 fixed/agc/annotated 三次独立 replay。旧根 86/86 的 `received.png` 与该场景 AGC/auto 接收解码来自同一录制源且输出哈希相同；它是历史交付视图与接收完整性证据，不是第四次独立 replay。当前 DDS image root 必须按实际板端发送与 Windows 接收解码重新核对计数/哈希，不能照抄旧 86/86。

### 10.3 正式场景 CPU—uniform—像素三方对照

- SWIR 案例使用 sourceSeq 90、800×800 pre-display RGBA32F 渲染目标（PFM 保存 RGB），运行 uniform 为 `u_m1_physics_runtime_en=1`, `u_m1_raw_si_output=1`, `tau=0.861182`, `path=0.025149 W/(m²·sr·μm)`。250/300/350/475 K 四个 rho=0 黑体面板全部 PASS；最大 GPU-vs-shader 相对误差为 `0.001001258`（0.1001%）。
- MWIR 案例使用 sourceSeq 90、800×800 pre-display RGBA32F 渲染目标（PFM 保存 RGB），运行 uniform 为 `u_m1_physics_runtime_en=1`, `u_m1_raw_si_output=1`, `tau=0.819460`, `path=0.137830 W/(m²·sr·μm)`。四面板全部 PASS；最大 GPU-vs-shader 相对误差为 `0.000384964`（0.0385%）。
- 每个面板都从标注 bbox 内选择 5×5 中值像素，并与独立高精度 Planck 波段平均、生产 shader Simpson 方程和当帧 tau/path uniform 联合比较。这是正式场景实现一致性 PASS，仍不是真实目标/传感器标定。
- 证据：[`SWIR cpu_shader_pixel_check.json`](../logs/p11/windows/validation-controlled-swir-near-20260916/SWIR/controlled_sample_solar_off/cpu_shader_pixel_check.json)、[`MWIR cpu_shader_pixel_check.json`](../logs/p11/windows/validation-controlled-mwir-near-20260916/MWIR/controlled_sample_solar_off/cpu_shader_pixel_check.json)。

### 10.4 Windows UDP/TCP 历史根与非阻塞边界

- 历史根 `final-matrix-frame-contract-20260916` 预检选择 264 个计划变体并实际执行 258 个可运行变体；其旧聚合结果为 85 PASS/1 FAIL，唯一旧失败是 `MWIR/controlled_sample_solar_off` 的 AGC/annotated `agc_requested_source_seq_not_valid`。这些结果继续保留，但不能直接迁移为最终统计。
- 该根运行配置和日志证明 `ApplyAeroToRadiance=0`、`AeroApplyOnlyBand=MWIR`、`aeroAppliedToRadiance=0`，且旧 request 未绑定本轮要求的正式程序 SHA-256。因此整体状态为 **`SUPERSEDED_INVALID_FOR_AERO`**；旧 raw 非黑、AGC 失败或其余 case PASS 都不能豁免这一失效。
- 按用户指定的 DDS-only 口径，不再把 Windows UDP/TCP 全矩阵或 TCP H.264 重跑列为正式交付阻塞门。其 request、aero summary、aggregate/variant 统计和失败继续保留作历史诊断，不得用于替代板端 DDS/MPP→Windows 真接收解码证据。
- 三个 coverage 阻塞项为 `cloud_target_front_behind`（BLOCKED_METADATA）、`cloud_individual_id_disable`（BLOCKED_METADATA）和 `target_range_2km_physical_imagery`（BLOCKED_DATA）。普通 cloud、雨、雪、能见度、高度和速度场景的通过不能外推这三项。
- 历史证据根：[`final-matrix-frame-contract-20260916`](../logs/p11/windows/final-matrix-frame-contract-20260916)（`SUPERSEDED_INVALID_FOR_AERO`）、[`h264-60s-frame-contract-20260916`](../logs/p11/windows/h264-60s-frame-contract-20260916) 及后续未完成 UDP/TCP 矩阵。它们均是非阻塞历史诊断，不替代 RK3588 DDS/MPP 正式链。

## 11. RK3588 构建、部署与回滚快照

**当前结论：stage `p11-20260917-000705` 的 RK build/deploy、双波段各 60 s DDS/MPP 验收和 v2 回滚恢复均 PASS；早期失败迭代完整保留。**

- 当前隔离 VM 交叉构建为 [`build-p11-20260917-000705`](../logs/p11/rk3588/build-p11-20260917-000705)：ELF SHA-256 `3a616afed1fc2d0984ce6d498e96000a8b9848bed670475b5b9fdd8297585862`，Build ID `77deeddfc0b2db1a98025364579bceb844cee481`，source manifest SHA-256 `1678e6172a00ba24e02d5ed7697574d0b72a9b1f170d7943237aab7024dd2550`，source archive SHA-256 `c424a64f4c786d697c4fe71602d9df4adf9e2aaf4e8d6003abbc1c2d3dc23571`，并记录 `rkmpp=true`, `zrdds=true`, `avformat_mux=true`。对应 [`deploy-p11-20260917-000705`](../logs/p11/rk3588/deploy-p11-20260917-000705) 的 ELF/Build ID 相同，active config manifest SHA-256 为 `efa7aa0b4b068b53e8640a4bbbccfc300dc672cfe2c2ce07814024af4d2c5ac5`；只改动 1 个配置文件，delta 31,410 bytes，hard-link snapshot 保留。
- 正式 [`acceptance-p11-20260917-000705`](../logs/p11/rk3588/acceptance-p11-20260917-000705) 使用 DDS domain 150、RK MPP H.264→Windows FFmpeg 真解码和 800×800 OrderedQueue。SWIR 的 sender/accepted/execute/render/output/received 均为 `3602`，输出 FPS `59.912050`；MWIR 六级计数均为 `3600`，输出 FPS `59.881678`。
- RK steady-clock 端到端延时：SWIR mean/p95/p99/max=`35.410/49.196/172.248/292.916 ms`，>80 ms=`127/3602`，RESET/START/STOP 响应=`0.435/9.305/0.149 ms`；MWIR=`33.367/44.388/130.035/276.723 ms`，>80 ms=`90/3600`，控制响应=`0.279/7.283/0.156 ms`。最大输入队列深度分别为 16/15，未发生 overflow/overwrite/source gap。80 ms 是全量报告阈值，不把少数长尾隐藏成“全帧低于 80 ms”。
- 两波段均验证正式 M1、dual-pass、`RTM_bind_or_copy`、RGBA16F SI、每波段 5 个真实 800×800×3 PFM、真实 receiver PNG/Annex-B、PFM half-lattice、输入身份守恒、控制响应、排空和无覆盖/丢帧；acceptance overall 为 PASS。正式 v2 [`rollback-p11-20260917-003800`](../logs/p11/rk3588/rollback-p11-20260917-003800/rollback_receipt.json) 证明旧版本 NIR completed/sent/received/decoded 均为 `1021`，恢复后 P11 SWIR 均为 `902`，两阶段 DDS/decode error 均为 0；恢复后的 active ELF 和 config manifest 与当前部署哈希一致。旧 `rollback-p11-20260916-202709` 缺少该闭环计数合同，标记为 **`SUPERSEDED_INVALID_ROLLBACK_RECEIPT`**。
- 正式 [`dds-lifecycle-p11-20260917-000705-final-r2`](../logs/p11/rk3588/dds-lifecycle-p11-20260917-000705-final-r2/dds_lifecycle_overall.json) 四项均 PASS：SWIR/MWIR pause-resume 分别有约 3.005/3.005 s DDS 输出空窗且 `catchUpBurst=0`，接收端 PID `42720→14232` 后恢复 SPS/PPS/IDR 与真解码且生产 PID/session 不变，同进程 re-INIT 的波段/profile 为 `0/SWIR→2/MWIR→0/SWIR`、generation `1→2→3`，三轮 sender/accepted/execute/received 均为 1442。
- 正式 [`dds-image-p11-20260917-000705`](../logs/p11/rk3588/dds-image-p11-20260917-000705/dds_image_matrix_summary.json) 的 20 个 SWIR/MWIR 类别案例、60 个 fixed/AGC/annotated 独立 DDS 运行全部 PASS；每次 600–602 帧，FPS `59.805888–60.041787`，六阶段无身份错配。>80 ms 合计 `2509` 帧，最差 p95/max=`136.980/298.812 ms`，长尾如实保留。顶层为 `PARTIAL`，原因仅为三个冻结 coverage blocker，而不是把未覆盖项改成 PASS。
- 协议只定义 RESET/START/STOP，没有 PAUSE opcode。生命周期测试中的 pause/resume 是发送端暂时停止再恢复 realtime sample emission 的 emission gap，不得冒称新增协议控制语义。
- 演进失败不删除：`172419` 的初始 assertion、`181851` 的 GDB 定位轮和 `191207` 的初版 RGBA16F negotiation 失败轮均继续保留；最终通过只证明后续 workaround 闭环，不把历史 FAIL 改写为 PASS。

## 12. 明确缺口、BLOCKED 和不得声称的事项

| 项目 | 状态 | 影响 | 最小解阻/下一步 |
|---|---|---|---|
| 玻璃 GPU 后背景透射 | `PASS_STATIC_CPU_WGL` | 单层直透正式公式与 WGL 探针已通过；仍不是实测玻璃标定；本次 60-run DDS Representative 矩阵不含 controlled glass case | 保留现有 CPU/WGL/Windows 受控样片证据；不得把 DDS Representative 图片冒充玻璃样片板端证据 |
| 真实材料/目标/器件标定 | `NOT_VERIFIED_CALIBRATION` | 不能报告现实温度精度、NETD 或真实目标对比度精度 | 提供可追溯样片光谱、温度、SRF/QE/镜头/像元/曝光参数及实验图 |
| Windows UDP/TCP 全矩阵/H.264 | `HISTORICAL_DIAGNOSTIC_NONBLOCKING` | 旧 258 个 runnable variants 的 85/86 为 `SUPERSEDED_INVALID_FOR_AERO`；后续未完成矩阵与 TCP H.264 也不属于 DDS-only 正式口径 | 保留原始日志，不要求重跑，不进入正式 overall gate |
| 云前/云后 | `BLOCKED_METADATA` | 普通 cloud case 不能证明目标相对单个云体的世界空间前后关系 | 提供确定性的相机/目标/云体几何元数据，分别实际运行 clear、云前、云后并检查 raw 像素贡献 |
| 逐 cloudId 禁用 | `BLOCKED_METADATA` | 当前 fixture 没有稳定、可观测且与当轮绑定的 cloud-volume ID 集 | 使用当轮实际发出的稳定 cloudId 逐项 enable/disable，保存 raw 差分和身份清单 |
| 2 km 目标距离而 LUT 最大 1 km | `BLOCKED_DATA` | 双波段×三变体共 6 个计划行未运行；不应夹到 1 km 或外推冒充有效 | 使用真实 2 km MODTRAN 数据扩展 LUT 后再运行；当前保持 fail closed |
| Windows 代表性三变体 | `HISTORICAL_ONLY` | 旧近景代表 case 仍是有效演进证据，但不代表 DDS-only 最终状态 | 保留早期失败与后续代表性通过；正式结果只取当前板端 DDS image root |
| RK3588 SWIR/MWIR 各 60 s DDS/MPP | `PASS` | stage `p11-20260917-000705`、双波段 60 s、DDS/MPP/Windows 真解码、身份/延时/raw half-lattice 与 v2 回滚均已闭环；仍有如实报告的 >80 ms 长尾 | 以 `acceptance-p11-20260917-000705` 和 `rollback-p11-20260917-003800` 为正式 RK 根；旧 `rollback-p11-20260916-202709` 为 `SUPERSEDED_INVALID_ROLLBACK_RECEIPT` |
| NIR | `PASS_3S_TCP_COMPATIBILITY_ONLY` | 180/180 守恒、H.264/libx264、decodeErrors=0；>80 ms 为 7/180，但不是本轮主验收链 | 只作协议/profile 兼容回归，不升级为 SWIR/MWIR 或 RK 证据 |
| VIS-SWIR | `UNSUPPORTED_REJECTED` | 不能以 VIS 或 NIR 回退冒充 SWIR | 继续明确拒绝，直到有独立模型/数据/验收 |

## 13. 验证等级

| 等级 | 当前结论 | 主要证据 |
|---|---|---|
| 解析/公式基准 | `PASS` | 194 项独立参考，tau/path/反射/主动光/积分检查 |
| 实现一致性 | `PASS` | 独立 WGL 方程探针最大相对误差 0.001394865；正式场景黑体面板 CPU—uniform—PFM 像素三方对照 SWIR 最大 0.1001%、MWIR 最大 0.0385%，均 < 2% |
| 受控现象 | `PARTIAL` | DDS 60 s、v2 回滚、DDS lifecycle 与 20-case/60-run image matrix 已完成；3 个 coverage 阻塞、修复前图片缺口、DDS controlled-glass 缺口仍保留，且 SWIR plume 虽有正式差异但视觉效应弱 |
| 真实测量对比 | `NOT_VERIFIED_CALIBRATION` | 没有真实样片/目标/传感器标定数据 |

## 14. 关键证据索引

- 基线：`logs/p11/baseline/prechange_20260916_0126`
- 独立 CPU/生产 CPU/WGL GPU：`logs/p11/reference/final_gpu`
- MODTRAN 湿度和宽谱太阳：`logs/p11/modtran/P11_MODTRAN_HUMIDITY_AND_SOLAR_DELIVERY.md`
- 材料与气动局部性：`logs/p11/aero_material_locality/20260916-051031`
- 热惯性：`logs/p11/thermal_inertia/20260916-051030`
- 羽流：`logs/p11/plume_physics/20260916-051032`
- 波段切换：`logs/p11/band_switch/final17`
- Windows 历史矩阵（`SUPERSEDED_INVALID_FOR_AERO`，非阻塞）：`logs/p11/windows/final-matrix-frame-contract-20260916`
- Windows 双波段 60 s H.264/TCP（历史诊断、非阻塞）：`logs/p11/windows/h264-60s-frame-contract-20260916`
- NIR 3 s 兼容回归：`logs/p11/windows/nir-compat-frame-contract-20260916`
- Windows 历史短轮延时：`logs/p11/windows/smoke-mwir-agc-target13`
- Windows 保留的早期失败三变体：`logs/p11/windows/validation-target-near-20260916`
- Windows RGBA32F 双波段近景代表性通过：`logs/p11/windows/validation-rgba32f-swir-20260916`、`logs/p11/windows/validation-rgba32f-mwir-20260916`
- 正式场景三方数值对照：`logs/p11/windows/validation-controlled-swir-near-20260916`、`logs/p11/windows/validation-controlled-mwir-near-20260916`
- RK3588 正式构建/部署：`logs/p11/rk3588/build-p11-20260917-000705`、`logs/p11/rk3588/deploy-p11-20260917-000705`
- RK3588 正式双波段 60 s：`logs/p11/rk3588/acceptance-p11-20260917-000705`；正式 v2 回滚：`logs/p11/rk3588/rollback-p11-20260917-003800`；旧 `rollback-p11-20260916-202709` 为 `SUPERSEDED_INVALID_ROLLBACK_RECEIPT`
- RK3588 DDS 生命周期：`logs/p11/rk3588/dds-lifecycle-p11-20260917-000705-final-r2`（overall PASS；pause/resume 是 sender emission gap，不是协议 opcode）
- RK3588 DDS 图片矩阵：`logs/p11/rk3588/dds-image-p11-20260917-000705`（20/20 cases、60/60 variants PASS；overall PARTIAL 仅因 3 个冻结 blocker）
- RK3588 保留失败迭代：`logs/p11/rk3588/pilot-p11-20260916-172419`、`pilot-p11-20260916-181851`、`pilot-p11-20260916-191207`；静态合同：`logs/p11/tests/rk3588_frame_contract.json`
- 可机读交付状态：[`evidence_manifest.json`](../logs/p11/delivery/manifests/evidence_manifest.json)（仅在最终 finalizer 重生后作为当前入口）
- 离线图片页：[`logs/p11/delivery/index.html`](../logs/p11/delivery/index.html)（仅在最终 finalizer 重生后作为当前入口）

## 15. 最终收尾前必须重生的项目

1. DDS 双波段 60 s、v2 回滚、lifecycle 和 20-case/60-run image matrix 均已完成；使用上述显式 evidence roots 统一重生 evidence/config/data/assets/license manifest 哈希。
2. Windows UDP/TCP matrix/H.264 只保留历史诊断，不再要求重跑，也不进入 DDS-only overall gate。旧 85/86、唯一 AGC 失败和后续中止事实均不得改写。
3. 为每个关键根因加入实际修复前/后同因素图；当前没有有效 before image 的项目必须明确 `BEFORE_IMAGE_NOT_CAPTURED`，不能用占位图关闭。
4. 三个 coverage 阻塞继续原样保存；解阻需要新增 metadata/真实 2 km 数据。NIR 只作兼容回归，VIS-SWIR 保持 unsupported，真实材料/目标/器件仍为 `NOT_VERIFIED_CALIBRATION`，所以 overall 保持 **PARTIAL**。
5. 只在所有已完成 DDS 引用文件和 ZIP 内容核对后生成 `HwaSimIR_P11_Delivery.zip`；ZIP 的最终状态必须原样保留上述 PARTIAL/BLOCKED、before-image 缺口与延时长尾。
