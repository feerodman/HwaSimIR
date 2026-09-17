# HwaSimIR P11 一页运行说明

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

> 状态：结构化证据驱动。按用户最终口径，正式传输验收只测试 DDS；Windows UDP/TCP matrix/H.264 仅为历史诊断、非阻塞且不再要求重跑。标题下存在 `P11_FINALIZER_STATUS` 块时以该块和对应 JSON 为最终状态，不存在时视为 DRAFT。旧 Windows 根 `final-matrix-frame-contract-20260916` 为 `SUPERSEDED_INVALID_FOR_AERO`，旧 85/86 不进入 DDS 正式统计。由于 3 个 coverage blocker、before-image 缺口和真实标定未完成，overall 保持 `PARTIAL`。

## Windows 正常启动

1. 启动接收/显示端：`HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe`。
2. 启动仿真端：`HwaSim_IR\Bin\HwaSim_IR.exe`。
3. 启动原控制 UI：`build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe`。
4. 在 INIT 中明确选择且只选择：
   - `0 = SWIR`，理想矩形响应 1.1–2.5 μm；
   - `2 = MWIR`，3–5 μm。
5. 正常验收时将 INIT 输出分辨率设为 800×800，然后按原流程 `INIT -> START -> STOP`。不要靠改输出分辨率或重复旧图来保 FPS。
6. 切换 SWIR/MWIR 时发新 INIT；不仅更改显示标签。正确日志应同时显示 profile、M1、MODTRAN 和 shader 刷新，且不出现 `previousBandCacheMarker`。

## 波段与不支持项

| 选择 | 用途 | 状态 |
|---:|---|---|
| 0 | SWIR 1.1–2.5 μm | P11 主链 |
| 1 | NIR 0.7–1.1 μm | 仅兼容回归 |
| 2 | MWIR 3–5 μm | P11 主链 |
| 3 | LWIR | 未支持，必须拒绝 |
| 4 / `VIS-SWIR` | VIS 或 VIS-SWIR | 未支持，不得回退为 SWIR/NIR |

## 正常物理链不需手工打开的东西

- 不要因 `UseModtranTauForAtmosphere=0` 就把 Stage3/Stage5 的旧 MODTRAN 开关全部改为 true。正式 SWIR/MWIR 由 `[M1NirMwirPhysics]` 控制，乱开旧开关会带来重复 path/sky/solar 风险。
- 日常运行不需要打开逐帧 PFM/大 CSV/详细物理日志。这些只在受控验收时使用。
- 正式默认数据是 `Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`、`solar_heating_lut_si.csv`和显式材料光学表；运行不需联网或实时调 MODTRAN。

## 显示选择

- 物理对比使用固定映射，SWIR 和 MWIR 各自保持同组公共窗口；不要用 AGC 证明物理大小关系。
- 日常观看可用 Auto/AGC。当前正式公共辐亮度窗口是 SWIR 0–40、MWIR 0–64 W/(m²·sr·μm)；映射前保持 SI 值，Windows/WGL raw 为 RGBA32F，RK/Linux GLES raw 为经实际 16/16/16/16 门验证的 RGBA16F。
- `continuous_low_contrast` 是合法的 AGC fallback，但场景若声明 `requireAgcValid=true`，所选证据帧仍必须 valid；不能因 raw 非黑而豁免场景门。
- 标注只在最终显示层叠加；物理检查请同时保存无标注图。

## 主动光和热源

- 正式参考主动光为 SWIR 1.55 μm，实时开关仍由原 `WeaponState.illuminatorEn`、锥角和指向字段控制。0.85 μm 是 NIR，不会被冒充为 SWIR/MWIR 带内信号。
- `engineState` 只控制局部发动机舱/尾管/后部热区和民用羽流，不应瞬间加热整机，也不创造损伤语义。
- `strikeFlag/strikePart` 仍只对 BrightSpot 路径负责，不控制后部 ThermalHotspot。

## 目标身份与输入顺序

- 目标必须用 `targetType + targetPlatID + targetID` 全键匹配，不能只按 `targetID`。
- `targetNumValid`、`TargetState.viewValid`、`WeaponState.viewValid`和 `lookatEn` 继续遵守原协议语义。
- 生产队列必须保持 `OrderedQueue`；不得改成 Latest、覆盖已接受输入或复制图像冒充新帧。

## STOP 和结果完成

- STOP 控制开始响应、队列排空、末帧到达、录像完成是四个不同时间口径。不要在刚收到 STOP 时强制杀进程。
- 正常结束后应看到 recorder flush 和总结文件，再关闭显示端。如果输出计数小于已接受输入，该轮是失败，不以视频能播放为 PASS。
- 允许包含端点的 60 s 轮出现 3601 帧，也允许恰为 3600 帧；判据是 sender→accepted→render/output→receiver→written→MP4 数量全等、sourceSeq 连续且不复用旧图，不是硬凑一个帧数。

## RK3588 正常启动

1. 使用板端原目录 `/userdata/HwaSimIR`，不移动系统库或修改系统时间。
2. 使用原启动器 `/userdata/HwaSimIR/run_precise.sh`。它负责现有 Mali/Xorg/headless、DDS 和 MPP 环境；普通用户不需要拼接长环境变量。
3. 波段仍通过原 INIT 消息选择 0=SWIR 或 2=MWIR，不通过改脚本文本假装切换。
4. 正式板端视频链应为 DDS domain 150 / `HwaSimIR.Video.precise.H264` / RK MPP H.264，Windows 接收器解码。如果使用 TCP 或 JPEG 诊断通道，必须明确标记，不能冒称这条 DDS/MPP 链通过。
5. 历史 Windows `h264-60s-frame-contract-20260916` 使用 `TCP_PACKET_V3_JSON` + FFmpeg/libx264，只证明旧 Windows TCP 链；它不能冒称板端 DDS/MPP。该根和 Windows UDP/TCP matrix 均为非阻塞历史诊断，不再要求重跑；正式状态只取独立 RK DDS evidence roots。
6. 若板端日志出现 `Failed to convert image`、GL fatal 或全零 raw，整轮必须 FAIL。Stage7/weather/material-ID 已通过禁用复现排除；实际根因是 Panda GLES 在 EGL 浮点 output 创建后把 half-float attachment negotiation 推为 4-byte RGBA，而 `copy_image` 缺少该转换路径。Windows 保持 RGBA32F；RK 在 output 创建后、attachment 前窄化 Panda negotiation hint，恢复 RGBA16F，并验证 actual texture=`half_float`、component width=2、framebuffer=`16/16/16/16`。
7. 当前 RK stage 为 `p11-20260917-000705`，ELF SHA-256 `3a616afed1fc2d0984ce6d498e96000a8b9848bed670475b5b9fdd8297585862`，Build ID `77deeddfc0b2db1a98025364579bceb844cee481`，source manifest/archive SHA-256 为 `1678e6172a00ba24e02d5ed7697574d0b72a9b1f170d7943237aab7024dd2550` / `c424a64f4c786d697c4fe71602d9df4adf9e2aaf4e8d6003abbc1c2d3dc23571`；部署 config manifest 为 `efa7aa0b4b068b53e8640a4bbbccfc300dc672cfe2c2ce07814024af4d2c5ac5`。
8. `acceptance-p11-20260917-000705`：SWIR 六级计数均为 3602，FPS 59.912050，mean/p95/p99/max=35.410/49.196/172.248/292.916 ms，>80 ms=127，RESET/START/STOP=0.435/9.305/0.149 ms；MWIR 六级计数均为 3600，FPS 59.881678，mean/p95/p99/max=33.367/44.388/130.035/276.723 ms，>80 ms=90，控制响应=0.279/7.283/0.156 ms。两波段均为 800×800、MPP H.264 DDS 发布和 Windows 真解码，overflow/overwrite/source gap 均为 0。
9. 正式 v2 `rollback-p11-20260917-003800`：旧版本 NIR completed/sent/received/decoded=`1021/1021/1021/1021`，恢复后 P11 SWIR=`902/902/902/902`，两阶段 DDS/decode error 均为 0，active ELF/config 与当前部署一致。旧 `rollback-p11-20260916-202709` 标记为 `SUPERSEDED_INVALID_ROLLBACK_RECEIPT`；`172419`、`181851`、`191207` 失败 pilot 仍须保留。
10. 协议没有 PAUSE opcode；生命周期测试中的 pause/resume 是发送端 realtime sample emission gap。不得把该动作记为新增控制码或改变原控制语义。正式 lifecycle 根为 `logs/p11/rk3588/dds-lifecycle-p11-20260917-000705-final-r2`（四项 PASS）；正式 image 根为 `logs/p11/rk3588/dds-image-p11-20260917-000705`（20/20 cases、60/60 variants PASS，顶层因 3 个 blocker 为 PARTIAL）。

## 版本与回滚

- Windows 仿真端、VideoDisplay 和原控制 UI 仍需按正式构建清单保存身份，但不再以 UDP/TCP matrix 作为验收门。RK 以同一 stage 的 build/deploy/DDS acceptance/lifecycle/image/rollback receipt 为准；finalizer 必须验证跨阶段 ELF/config 一致性并把相关证据文件自身 SHA-256 写入 manifest/HTML。
- 板端保留的回滚快照与详细步骤在 `logs/p11/rk3588/rollback-*`。回滚是维护操作；普通运行不要手动交换 ELF/Config。

## 目前必须知道的限制

- 玻璃的分波段 rho/epsilon/tau 与单层直透合成已有静态 CPU/WGL GPU 检查；它仍是工程近似（无折射、Fresnel 角度项和内部多次反射），且最终 Windows/RK 像素状态必须以交付清单中的实际运行证据为准，不能称为实测玻璃标定。
- 民用车、材料样片、羽流和理想传感器参数是可审计工程假设，未与真实目标/器件标定。
- 2 km 场景超出当前正式 1 km MODTRAN 路径网格时应 fail closed，不应夹到 1 km。
- `logs/p11/windows/final-matrix-frame-contract-20260916` 是 `SUPERSEDED_INVALID_FOR_AERO` 历史根；`h264-60s-frame-contract-20260916`、后续中止矩阵和 `nir-compat-frame-contract-20260916` 也只证明各自声明的 Windows TCP/NIR 范围。它们均不进入 DDS-only 正式 gate，也不再要求重跑；只有 finalizer 使用当前 RK DDS build/deploy/acceptance/lifecycle/image/rollback 根重生后，[`logs/p11/delivery/index.html`](../logs/p11/delivery/index.html) 与 `evidence_manifest.json` 才是最新入口。
- 旧 85/86、唯一 AGC 失败和 `received.png` 86/86 同源哈希只能作为旧 UDP/TCP 根事实，不进入正式 DDS aggregate。三个未解 coverage 必须分开保留：`cloud_target_front_behind`（BLOCKED_METADATA）、`cloud_individual_id_disable`（BLOCKED_METADATA）、`target_range_2km_physical_imagery`（BLOCKED_DATA）。旧 Windows SWIR TCP 60 s 虽 H.264/身份守恒 PASS，但 >80 ms 为 219/3601、p95 154.419 ms，只作历史诊断；当前 DDS 长尾按第 8 项如实报告。

## 最终交付生成

动态证据根必须显式给出，避免把旧时间戳目录误选为最终结果。DDS-only finalizer 先用 `--validate-only` 检查当前 RK build/deploy、双波段 60 s、lifecycle、image matrix 和 v2 rollback；Windows UDP/TCP 参数只允许作为历史非阻塞输入或省略。所有后台 DDS 测试结束后再使用 `--update-docs --replace-output --build-zip`。finalizer 会原样保留源 JSON 中的 FAIL，并在 PASS 声明缺图片、单位、尺寸或哈希不符时将其降为 FAIL。三个 coverage blocker、`BEFORE_IMAGE_NOT_CAPTURED` 和 `NOT_VERIFIED_CALIBRATION` 必须保留，所以当前 overall 是 `PARTIAL`。

```powershell
python tools/p11_finalize_delivery.py `
  --acceptance-mode dds-only `
  --rk-acceptance-root 'logs/p11/rk3588/acceptance-p11-20260917-000705' `
  --rk-build-receipt 'logs/p11/rk3588/build-p11-20260917-000705/build_receipt.json' `
  --rk-deployment-receipt 'logs/p11/rk3588/deploy-p11-20260917-000705/deployment_receipt.json' `
  --rk-dds-lifecycle-summary 'logs/p11/rk3588/dds-lifecycle-p11-20260917-000705-final-r2/dds_lifecycle_overall.json' `
  --rk-dds-image-root 'logs/p11/rk3588/dds-image-p11-20260917-000705' `
  --rk-rollback-receipt 'logs/p11/rk3588/rollback-p11-20260917-003800/rollback_receipt.json' `
  --validate-only
```

去掉 `--validate-only` 并增加下列参数才会生成最终目录和 ZIP；已有 `logs/p11/delivery` 会先原子移动为带时间戳的备份。

```powershell
  --update-docs --replace-output --build-zip `
  --output-dir logs/p11/delivery `
  --zip-path HwaSimIR_P11_Delivery.zip
```
