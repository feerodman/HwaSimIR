# HwaSimIR P2 DDS Realtime Ordered Input 与生产门禁收口

日期：2026-09-09  
工作区：`D:\HwaSimIR`  
真实板卡：`root@192.168.1.116`（RK3588 / aarch64）

## 1. 结论

P2 已完成 DDS Realtime 有序消费实现、低开销 ingress 审计、Latest 兼容、同步模式回归，以及 Stage6 生产日志总开关。`AsyncInputPolicy=OrderedQueue` 已成为 DDS 生产默认；旧 `Latest` 仍可显式恢复。

P2 没有开启 M1/L1/L2 生产物理 gate。原因不是 MODTRAN、DDS、MPP、Mali 驱动或数据单位错误，而是最终真实部署条件仍有一个可复现的吞吐 blocker：

- 在受控 `performance` governor 下，完整 M1/L1/L2 + Weather + DDS + Mali + MPP 的四组 65 s 测试均达到约 60 FPS，输入队列可恢复，零覆盖、零 overflow、零序号 gap。
- 恢复板卡原始 `CPU=ondemand`、`GPU=simple_ondemand` 后，用正式配置重跑 65 s，FIFO 长时间达到 16，`inputBackpressureCount=3678`、累计等待 `62642.203 ms`，只在发送停止后恢复。虽然最终数量守恒且没有丢弃，这不符合“steady-state queue <= 1、无持续 backpressure”的生产门禁。
- 对 catch-up 每 4 帧的 `yield()` 做了单因素去除 A/B；原始 governor 下仍为 `maxQueueDepth=16`、`inputBackpressureCount=3741`、累计等待 `63355.434 ms`，因此撤销该无效变更。

最终生产配置保持：DDS protocol/video ON、TCP payload OFF、OrderedQueue ON、Stage6Diagnostics OFF；M1/L1/L2 gate OFF。

## 2. `inputOverwritten` 的真实语义和来源

P1 的 `inputOverwritten=1643` 不是 DDS 网络 drop。旧 `AsyncInputPolicy=Latest` 在应用 ingress 内只保留最新状态；新 Realtime 到达时会替换尚未由 render thread 消费的旧状态，因此这是应用层有意合并：

```text
DDS reader callback
  -> adapter/copy
  -> single pending/latest application state
  -> newer sample replaces older pending state
```

P2 将 DDS 接收、应用排队和应用消费分开计数：

- `ddsRealtimeReceived`
- `ddsRealtimeCallbackCount`
- `appRealtimeQueued`
- `appRealtimeConsumed`
- `inputOverwritten`
- `inputQueueOverflow`
- `currentQueueDepth` / `maxQueueDepth`
- `firstSourceSeq` / `lastSourceSeq` / `consumedSourceSeq`
- `sourceSeqGapCount`
- `callbackInterArrivalUs` / `callbackInterArrivalUsMax`
- burst histogram
- backpressure、catch-up、repeat 和数量守恒

守恒式为：

```text
ddsRealtimeReceived - appRealtimeConsumed - inputOverwritten - pending = 0
```

`ddsRealtimeReceived` 与 enqueue 在同一审计边界提交，避免 callback 正处于 backpressure 时产生虚假的瞬时不守恒。DDS callback 不操作 Panda Scene Graph。

## 3. DDS burst histogram

每秒只打印一条 `[RealtimeIngress]` 汇总，不逐 sample 打日志。直方图字段为：

```text
arrivalGapLe1ms
burstSize1
burstSize2
burstSize3
burstSize4Plus
```

RK physics-off 正式样本窗口中的代表值：

```text
ddsRealtimeReceived=240
appRealtimeConsumed=231
currentQueueDepth=9
maxQueueDepth=9
callbackInterArrivalUs=16444.964
callbackInterArrivalUsMax=42759.212
arrivalGapLe1ms=7
burstSize1=46
burstSize2=7
burstSize3=0
burstSize4Plus=0
```

这证明 DDS 可以形成短 burst，因而验收不要求 `maxQueueDepth` 永远为 1；要求的是 FIFO 不持续增长并能自动回落。

## 4. Latest 兼容行为

`AsyncInputPolicy=Latest` 未删除。Windows 最终兼容 smoke：

`D:\HwaSimIR\logs\p2-windows-final-latest-smoke-20260909`

代表终态：

```text
received=548
queued=548
consumed=462
overwritten=86
pending=0
sourceSeqGapCount=85
conservation=0
```

这是预期的 latest-state 合并语义，不是 OrderedQueue 的验收结果。UDP legacy 可继续显式选用该策略。

## 5. OrderedQueue 设计

生产默认配置：

```ini
[RenderControl]
AsyncInputPolicy=OrderedQueue
AsyncInputQueueMaxFrames=16
AsyncInputBackpressureMaxWaitMs=250
AsyncCatchUpMaxBurst=4
```

实现直接复用 `m_pendingDisplayFrames` 和 `m_cvDisplayQueueSpace`：

```text
DDS callback: adapter -> immutable copy -> push_back -> return
render thread: pop_front -> apply one Realtime -> render one output
```

主要约束：

- FIFO 保留原始到达顺序；
- 每个接受的 Realtime 最多消费一次；
- 不 clear、不 overwrite、不 silent drop；
- render hot path 使用已 pop 的 immutable sample，避免第二次锁和 506-byte 重拷贝；
- `ProcessRealSimSceneDrivenData` 和相机控制使用该 sample，不在 callback 中修改场景；
- STOP 到达时若 FIFO 尚有 Realtime，则延后 STOP，先 drain 已接受输入；
- 命令重新插入保持原始顺序，并兼容 VS2015 STL。

同步模式回归日志：

`D:\HwaSimIR\logs\p2-windows-final-sync-smoke-20260909`

结果为 `received=queued=consumed=545`、pending 0、max depth 3、overwrite/overflow/gap 全 0，保持 `1 Realtime -> 1 rendered/output frame`。

## 6. Queue、backpressure、catch-up 和空输入

队列满时不退回 Latest。callback 使用 condition variable 等待空间并累计：

- `inputBackpressureCount`
- `inputBackpressureWaitMs`
- `maxInputBackpressureWaitMs`

如果一次等待超过受控期限，记录 `inputQueueOverflow`、输出 ERROR，并 fail-fast；不会覆盖旧帧把测试做绿。

当 `queueDepth > 1` 时，scheduler 不再添加完整 16.67 ms sleep；每帧仍只消费最老的一条。队列恢复到 `<=1` 后恢复 60 Hz absolute deadline。统计：

- `catchUpFrames`
- `maxCatchUpBurst`
- `queueRecoveryMs`

队列为空且达到输出 deadline 时，可复用最后消费状态输出，并统计 `repeatedStateOutputFrames`。一旦 FIFO 非空，最老输入优先，不能跳过中间状态。

## 7. Windows 纯 DDS 60 s

最终 Windows 日志：

`D:\HwaSimIR\logs\p2-windows-final-ordered-ffmpeg-20260909-190729`

无 transport CLI override，确认：

- DataDrivenTestQT：`ControlTransport=dds source=production_default`
- HwaSim_IR：`CommandTransport=dds`、DDS protocol runtime init count 1
- HwaSim_IR：DDS H264，shared runtime init count 1
- VideoDisplay：`VideoInput Transport=dds`
- Realtime active-window：`maxQueueDepth=5`，steady depth 1–2，backpressure 0
- `inputOverwritten=0`、`inputQueueOverflow=0`、`sourceSeqGapCount=0`
- 每个汇总窗口 `receivedMinusConsumedOverwrittenPending=0`
- DDS H264：`sentSamples=3901`、`writeErrors=0`、`droppedSamples=0`、video queue max 1
- VideoDisplay 尾部 receive/display：59.949–60.135 FPS
- H264 keyframe seen，decode errors 0，source sequence continuous

Windows Release x64 最终 HwaSim_IR 使用 FFmpeg SDK 完整 Rebuild；产物：

```text
D:\HwaSimIR\HwaSim_IR\Bin\HwaSim_IR.exe
SHA-256=C3EF4159A1DD1BD7AD75BA0833DF8D9CEC0F49C7A6BA0CE38B7EEF88BE56E138
```

一次未设置 `FFMPEG_ROOT` 的中间增量构建没有 H264 能力，已通过带 SDK 的完整 Rebuild 修正，不计作验收产物。

## 8. RK3588 physics-off OrderedQueue

最终 STOP drain 证明：

```text
board:   /userdata/HwaSimIR/logs/p2_final_drain_physics_off_20260909.log
Windows: D:\HwaSimIR\logs\p2-rk-final-drain-20260909-150842
received=1199
queued=1199
consumed=1199
pending=0
maxQueueDepth=3
inputOverwritten=0
inputQueueOverflow=0
sourceSeqGapCount=0
conservation=0
```

受控 performance governor 的 physics-off 运行达到约 60.0–60.65 FPS，`renderMs=15.98–16.10 ms`，队列由 burst staging 自动回落。

板端正式保留 ELF（撤销无效 no-yield A/B 后）：

```text
/userdata/HwaSimIR/HwaSim_IR
SHA-256=2e021f8688af418f82365e671e931df2f13221353a85dc29f755f6a2a4e67796
Build ID=6870349d487fdc09fd0ee2c0e6b4d60d2923e2ba
file=ELF 64-bit LSB pie executable, ARM aarch64
```

GPU/编码/通信门禁保持：

```text
glVendor=ARM
glRenderer=Mali-LODX
hardwareGpu=1
MPP encoder=rockchip_mpp_avc
H264=Annex-B
DDS writeErrors=0
DDS droppedSamples=0
```

## 9. 完整物理 Clear/Cloudy 矩阵

下列四组均使用真实 RK3588、Mali、MPP、DDS protocol/video、HeadlessOffscreen，65 s（预热后统计），临时开启 M1/L1/L2；测试完成恢复配置。正式性能矩阵在受控 performance governor 下通过：

| 场景 | 板端日志 | 输入守恒 | max queue | render/FPS | Windows display 尾部 |
|---|---|---:|---:|---|---|
| NIR daylight Clear | `/userdata/HwaSimIR/logs/p2_full_nir_day_clear_20260909.log` | 3899/3899/0 | 7 | 59.9–60.2 FPS, 15.99–16.14 ms | 60.213 FPS |
| NIR night Active off/on | `/userdata/HwaSimIR/logs/p2_full_nir_night_active_20260909.log` | 3900/3900/0 | 3 | 60.1–60.4 FPS, 15.90–15.91 ms | 60.404 FPS |
| MWIR Clear | `/userdata/HwaSimIR/logs/p2_full_mwir_clear_corrected_20260909.log` | 3900/3900/0 | 4 | 60.47–60.83 FPS, 15.82–15.86 ms | 60.047 FPS |
| MWIR Cloudy | `/userdata/HwaSimIR/logs/p2_full_mwir_cloudy_20260909.log` | 3899/3899/0 | 3 | 60.0–60.31 FPS, 15.98–16.00 ms | 60.304 FPS |

表中“输入守恒”格式为 `received/consumed/pending`；所有场景均为 overwrite 0、overflow 0、source gap 0、DDS video drop/error 0。Cloudy 使用 `StreamedWorld3D`，没有新增 render error。

物理语义抽查：

- NIR daylight：太阳高度 `56.18 deg`；Active off/on/off 为零、正值、零；outbound/inbound tau 约 0.957。
- NIR night：太阳高度 `-42.78 deg`，自然直射为零；Active 正辐亮度可逆，使用审计过的共址 reciprocity tau。
- MWIR Clear/Cloudy：协议 band 2；MODTRAN tau/path thermal 有效；Active `FollowSensor` 光谱重叠有效且不改变 thermal state。
- 一次 band 3 的中间运行被协议正确解释为 LWIR/unsupported，未冒充 MWIR；正式 MWIR 结果来自 corrected band 2 运行。

## 10. 物理和显示 QC

自动回归：

```text
tools/m1_nir_mwir_physics_check.ps1                         PASS (4 unit tests)
python tools/l1_natural_solar_qc.py                         PASS (15 rows)
python tools/modtran_solar_heating_qc.py                    PASS
tools/l2_active_illuminator_check.ps1                       PASS (24 rows)
python tools/active_display_monotonic_qc.py --self-test     PASS
```

NIR night monotonic self-test：

```text
activeSensorRadiance = 0 / 0.25 / 0
finalGray             = 0 / 0.5  / 0
```

没有修改 MODTRAN SI 单位、黄金 case、gain、tone map、body floor 或 illuminator power。

## 11. Stage6Diagnostics

新增生产总开关：

```ini
[Stage6Diagnostics]
Enable=false
```

为 false 时只关闭普通诊断打印，不关闭 Stage6 计算。受控的普通类别包括：

```text
[Stage6 Capture]
[Stage6 FrameDiag]
[Stage6 ViewportDiag]
[Stage6 Display]
[Stage6 SensorGeometry]
[Stage6 Resize]
[Stage6 FinalPipeline]
[Stage6 MTF]
[Stage6 Noise]
[Stage6 AGC]
```

FATAL、ERROR 和影响正确性的 WARN 不受该 gate 抑制。`MTFDebugLog`、`NoiseDebugLog`、`AGCDebugLog` 只有在 master gate 为 true 时才可输出。Stage6 smoke 脚本会临时设置 `Stage6DiagnosticsEnable=1`，并在 `finally` 恢复原环境。

## 12. 原始 governor 生产复验与 blocker

恢复板卡部署默认：

```text
CPU governor=ondemand
GPU governor=simple_ondemand
```

正式配置、完整 M1/L1/L2、MWIR Cloudy、DDS/MPP 65 s：

```text
/userdata/HwaSimIR/logs/p2_production_default_65s_valid_20260909.log

ddsRealtimeReceived=3826
appRealtimeQueued=3826
appRealtimeConsumed=3826
pending=0
inputOverwritten=0
inputQueueOverflow=0
sourceSeqGapCount=0
maxQueueDepth=16
inputBackpressureCount=3678
inputBackpressureWaitMs=62642.203
maxInputBackpressureWaitMs=25.832
queueRecoveryMs=66187.371
conservation=0
DDS video writeErrors=0
DDS video droppedSamples=0
```

结论：有序语义和最终数量守恒正确，但 callback 几乎整个发送窗口都在施加 backpressure，FIFO 未在 steady state 回落到 1。这不是允许的 DDS burst staging。

去除 catch-up quantum `yield()` 的单因素 A/B：

```text
/userdata/HwaSimIR/logs/p2_production_default_no_yield_65s_20260909.log
received/queued/consumed=3899/3899/3899
pending=0
maxQueueDepth=16
inputBackpressureCount=3741
inputBackpressureWaitMs=63355.434
queueRecoveryMs=67109.219
```

没有改善，因此源代码和板端 ELF均恢复为已验证版本。CPU/GPU governor 也已恢复原值。

## 13. 最终 production gate 状态

DDS 生产默认保持：

```text
CommandTransport.Input=dds
DdsProtocol.Enable=true
DdsVideo.Enable=true
TCP payload=false
AsyncInputPolicy=OrderedQueue
AsyncInputQueueMaxFrames=16
Stage6Diagnostics.Enable=false
```

由于原始 governor 下完整物理 steady-state FIFO 门禁失败，物理正式默认保持：

```ini
[M1NirMwirPhysics]
CompareOnly=false
EnableRuntime=false
EnableNIRRuntime=false
EnableMWIRRuntime=false

[NaturalSolar]
Enable=false
EnableOpticalShadow=false
EnableSolarThermal=false

[ActiveIlluminator]
Enable=false
```

板端已部署同一 OFF 配置：

```text
/userdata/HwaSimIR/Config/HwaSimIRRuntime.ini
SHA-256=17466a9b061ff99b221131970a0d39f24d41b6ee9c33885713823274aa769c66
```

这不是功能回退：M1/L1/L2 实现和临时 gate 验证均保留；只是不把尚未满足真实部署调频条件的组合标成生产默认。

## 14. 修改文件

- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`
- `HwaSim_IR/HwaSim_IR/HwaSimIR.h`
- `HwaSim_IR/HwaSim_IR/IR/IRPerfStats.cpp`
- `HwaSim_IR/HwaSim_IR/IR/IRPerfStats.h`
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`
- `tools/stage6_sensor_display_smoke.ps1`
- `tools/stage6_sensor_geometry_smoke.ps1`
- `docs/HwaSimIR_P2_DDS_Realtime_Ordered_Input_Production_Closeout.md`

## 15. Remaining blocker / 下一步边界

唯一未关闭的 P2 production blocker 是：在板卡部署默认 `ondemand/simple_ondemand` 下，完整物理 OrderedQueue 不能以无持续 backpressure 的方式吸收 60 Hz 输入。下一步应只围绕已测得的默认-DVFS吞吐差进行隔离，例如：

1. 记录每簇 CPU/GPU 实际频率与 queue depth 的同时间轴；
2. 判断是 DVFS ramp latency，还是 full-physics render 平均吞吐确实低于 60；
3. 优化被计时证明的具体 CPU/GPU热点，或建立经部署方批准且可恢复的 governor/min-frequency 服务策略；
4. 在原始生产启动方式下重新完成四组 60 s 矩阵后，才允许开启物理 gate。

P2 到此停止；未扩展 VIS/SWIR/LWIR、detector QE/electron、SRF 高保真、terrain full shadow 或通信协议。
