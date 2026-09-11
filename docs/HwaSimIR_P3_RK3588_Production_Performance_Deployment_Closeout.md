# HwaSimIR P3：RK3588 生产性能与部署收口

日期：2026-09-10  
板卡：`root@192.168.1.116`，`aarch64`  
源码基线：`main`，`115566e570302812a8f6b45d14d784e5e65e7e74`  
结论：**PASS，M1/L1/L2 已正式开启为生产默认。**

> 板卡系统时钟仍停在 2026-08-26，因此板端日志文件名和 CSV 的绝对时间早于本报告日期。所有测试时长、频率变化和队列变化均使用单调递增的 `elapsed_ms`，不依赖错误的板端日历时间。

## 1. 直白结论

原始省电配置跑不满 60 Hz 的主要原因不是 MODTRAN、DDS、MPP 编码器或 Mali 驱动，也不是单独的 CPU 或 GPU 频率不足，而是：

- DMC（动态内存控制器，也就是 DDR 内存带宽控制器）在默认 `dmc_ondemand` 策略下长期停在 528 MHz；
- 800×800 红外后处理、GPU 回读、BGR→NV12 预处理、MPP 编码和 DDS 发布共享内存带宽，528 MHz 不能持续供给整条链；
- CPU/GPU 单独提到最高频仍会积压；DMC 提到 1068 MHz 后，渲染时间从约 16.5 ms 降到约 11–12 ms，输入队列不再持续增长；
- 在 DMC=1068 MHz 的前提下，CPU 仍需要适度最低频率保护；GPU 在本场景保持最低 300 MHz 已足够。

最终没有把 CPU/GPU 全部锁死在最高频，也不需要永久使用 `performance`（固定高性能）模式。生产策略为：

```text
CPU: ondemand（按需调频）+ 最低频率限制
GPU: simple_ondemand（按需调频）+ 最低 300 MHz
DMC: dmc_ondemand（按需调频）+ 最低 1068 MHz
HwaSim_IR nice: -20
程序退出后: 恢复板卡启动前的原始策略
```

## 2. 原始自动调频 90 秒证据

场景固定为完整 M1+L1+L2、MWIR、Cloudy、DDS、MPP、800×800、60 Hz。原始策略：

```text
CPU governor = ondemand
GPU governor = simple_ondemand
GPU minimum = 300 MHz
DMC governor = dmc_ondemand
DMC actual/minimum = 528 MHz
```

结果：

| 指标 | 结果 |
|---|---:|
| 最大输入队列 | 16（达到上限） |
| 输入反压次数 | 5168 |
| 累计反压等待 | 89503.578 ms |
| renderMs 平均 | 16.466 ms |
| renderMs 最大 | 25.540 ms |
| renderFps 采样平均 | 59.313 |
| 输入是否自行追平 | 否，停止发送后才排空 |

队列开始增长时不是 GPU 忙满：GPU 实测一直是 300 MHz，GPU 利用率较低。CPU 各簇频率会升高，但变化不同步，无法弥补内存控制器仍停在 528 MHz 的带宽缺口。

证据：

- `logs/p3-final-evidence/logs/p3/isolation_D_timeline_90s.csv`
- `logs/p3-final-evidence/logs/p3/isolation_D_both_auto_90s.log`

## 3. CPU/GPU 四组隔离

四组均使用相同场景和相同 ELF；DMC 仍为默认 528 MHz。`inputBackpressureCount` 表示应用输入 FIFO 已满、DDS 回调等待空位的次数，它不是 DDS 网络丢包。

| 组别 | CPU | GPU | 最大队列 | 输入反压次数 | 累计等待 | 结论 |
|---|---|---|---:|---:|---:|---|
| A | performance | simple_ondemand | 16 | 3397 | 58192.150 ms | FAIL |
| B | ondemand | performance | 16 | 3402 | 57796.256 ms | FAIL |
| C | performance | performance | 16 | 1106 | 17880.785 ms | FAIL |
| D | ondemand | simple_ondemand | 16 | 5168 | 89503.578 ms | FAIL |

明确结论：

- 只提高 CPU：不能解决；
- 只提高 GPU：不能解决；
- CPU/GPU 同时最高频但 DMC 仍为 528 MHz：仍不能稳定解决；
- 因此主要矛盾是 DDR 内存控制器频率，CPU 是最低稳定组合的次要约束，GPU 不是本场景瓶颈。

证据位于 `logs/p3-final-evidence/logs/p3/isolation_*`。

## 4. DMC 验证

板卡实际可用 DMC 频率：

```text
528 / 1068 / 1560 / 2112 MHz
```

相同场景结果：

| DMC | renderFps | renderMs | 最大队列 | 输入反压 | 结果 |
|---:|---:|---:|---:|---:|---|
| 528 MHz | 57.455 | 16.817 ms | 16 | 3459 / 58142.341 ms | FAIL |
| 1068 MHz | 72.626 | 11.350 ms | 3 | 0 | PASS |
| 2112 MHz | 75.040 | 10.218 ms | 3 | 0 | PASS，但不是最低方案 |

1068 MHz 已有充分余量，因此没有采用 1560 或 2112 MHz 作为生产最低频率。

## 5. 最低稳定频率搜索

频率候选全部从板端 `scaling_available_frequencies` / `available_frequencies` 自动读取，没有硬写不存在的档位。

CPU 可用档位：

```text
policy0: 408 600 816 1008 1200 1416 1608 1704 MHz
policy4: 408 600 816 1008 1200 1416 1608 1800 2016 MHz
policy6: 408 600 816 1008 1200 1416 1608 1800 2016 MHz
```

GPU 可用档位：

```text
300 400 500 600 700 800 900 1000 MHz
```

DMC 固定为已证明最低可用的 1068 MHz 后，逐级回退结果：

| 候选 | CPU policy0/4/6 最低 MHz | GPU 最低 MHz | renderFps | renderMs | 输入反压 | 结果 |
|---|---|---:|---:|---:|---:|---|
| K | 1608 / 1800 / 1800 | 900 | 70.263 | 11.812 ms | 0 | PASS |
| L | 1416 / 1608 / 1608 | 800 | 69.570 | 12.321 ms | 0 | PASS |
| M | 1200 / 1416 / 1416 | 700 | 70.076 | 12.534 ms | 0 | PASS |
| N | 1008 / 1200 / 1200 | 600 | 67.278 | 13.452 ms | 91 / 1447.385 ms | FAIL |
| O | 1200 / 1416 / 1416 | 600 | 69.486 | 12.566 ms | 0 | PASS |
| P | 1200 / 1416 / 1416 | 500 | 68.223 | 12.576 ms | 0 | PASS |
| Q | 1200 / 1416 / 1416 | 400 | 68.515 | 12.702 ms | 0 | PASS |
| R | 1200 / 1416 / 1416 | 300 | 68.435 | 13.020 ms | 0 | PASS |
| S | 816 / 1008 / 1008 | 300 | 59.794 | 15.593 ms | 644 / 16252.905 ms | FAIL |

因此最低稳定组合是：

```text
CPU policy0 minimum = 1200 MHz
CPU policy4 minimum = 1416 MHz
CPU policy6 minimum = 1416 MHz
GPU minimum = 300 MHz
DMC minimum = 1068 MHz
```

把上述最低值应用到原来的自动调频策略后，60 秒复验：3600/3600 输入全部消费，最大队列 3，输入反压 0，`renderFps=69.950`，`renderMs=12.279 ms`。

## 6. 正式板端性能策略

生产配置文件：

- `HwaSim_IR/Bin/Config/RK3588PerformancePolicy.conf`

正式值：

```ini
PolicyMode=auto_min
ProcessNice=-20
CpuGovernor=ondemand
CpuPolicy0MinKHz=1200000
CpuPolicy0MaxKHz=1704000
CpuPolicy4MinKHz=1416000
CpuPolicy4MaxKHz=2016000
CpuPolicy6MinKHz=1416000
CpuPolicy6MaxKHz=2016000
GpuGovernor=simple_ondemand
GpuMinHz=300000000
GpuMaxHz=1000000000
DmcGovernor=dmc_ondemand
DmcMinHz=1068000000
DmcMaxHz=2112000000
RestoreOnExit=true
```

这不是全局最高频锁定。10 分钟长测中：

- policy0 实际 1704 MHz；
- policy4 实际在 1416–2016 MHz 间变化；
- policy6 实际在 1416–2016 MHz 间变化；
- GPU 始终 300 MHz；
- DMC 始终 1068 MHz。

## 7. 启动流程集成

修改后的统一启动器：

- `tools/rk3588_run_hwasimir_precise.sh`
- 板端部署名：`/userdata/HwaSimIR/run_precise.sh`

正常流程：

```text
检查 Xorg :0 和 /dev/mali0
→ 校验完整 Config 清单
→ 校验 ELF、启动器、性能脚本的 SHA-256
→ 检查 DDS QoS 和 NetworkConfig
→ 应用 RK3588PerformancePolicy.conf
→ 回读 CPU/GPU/DMC governor、min/max/current
→ 以 nice=-20 启动 HwaSim_IR
→ HwaSim_IR 退出
→ 恢复启动前 CPU/GPU/DMC 状态
```

所有设置失败均输出 `[BoardPerformancePolicy][ERROR]` 并中止或报告；不会静默假装生效。正式 10 分钟测试结束后已看到 `action=restore result=PASS`，CPU/GPU/DMC 都恢复到板卡原始自动调频配置。

正常用户启动方式：

```sh
cd /userdata/HwaSimIR
./run_precise.sh
```

不需要现场人员手工向 sysfs 写频率。

## 8. 完整 Config 原子部署

新增：

- `tools/rk3588_deploy_atomic.ps1`

更新：

- `tools/codex_rk3588_pipeline.ps1`

部署不再只上传 ELF 和少量 DDS 文件，而是每次同步完整 `HwaSim_IR/Bin/Config/`，总计约 5.72 GB。流程：

1. 本机复制完整 Config 到部署暂存目录；
2. 检查 Runtime、DDS、材料、MODTRAN、太阳热 LUT、NIR/MWIR SensorWave、TargetLib、Weather、IRRadiance、IRPlume、Annotation 必需文件；
3. 为每个文件生成 SHA-256 清单；
4. 上传 ELF、启动器、性能脚本和压缩 Config 到板端临时路径；
5. 在 `Config.new_<timestamp>` 解包并逐文件校验；
6. 全部通过后才将旧 Config 改名备份，并一次性切换新 Config/ELF/启动器；
7. 失败时恢复旧 Config 和旧 ELF；
8. 切换成功后把旧 Config 压缩成可恢复的 `Config.before_<timestamp>.tgz`。

本轮生产切换：

```text
[DeploymentVerify] PASS
[DeploymentSwitch] PASS
[DeploymentFinal] PASS
[DeploymentBackup] PASS
```

旧备份已全部从临时内存盘移回 `/userdata/HwaSimIR/`，没有留在重启即丢失的 `/tmp`。

## 9. 运行版本身份

板端最终运行身份：

| 项目 | SHA-256 / ID |
|---|---|
| Git commit | `115566e570302812a8f6b45d14d784e5e65e7e74` |
| Source identity | `115566e570302812a8f6b45d14d784e5e65e7e74-dirty` |
| AArch64 ELF | `2e021f8688af418f82365e671e931df2f13221353a85dc29f755f6a2a4e67796` |
| ELF Build ID | `6870349d487fdc09fd0ee2c0e6b4d60d2923e2ba` |
| HwaSimIRRuntime.ini | `2e687614094071b6b177862d467dd1e3c180bff9f46f825997e2e6b231421130` |
| Config manifest | `df56561d2688989b381ac7246f29efbc5ae49f90d1df847e0720b0745b7e8fca` |
| run_precise.sh | `e878e7d0e533dd0da271aa383c7e1f853d13167a02460d7e4315171cf4502693` |
| performance tool | `248c1f33d826b7f4f2944810d9514338f705de01858a5d4bd34e6ee7994afe7d` |
| performance policy | `19f1ce9f4792b2a233df2ec1f43d06e54f249eacd5708bc97f1770b37de64e36` |

`dirty` 是因为 P3 的脚本、策略和生产开关尚未提交；它不是“版本未知”。启动器同时校验 commit 记录、ELF、Config 清单、启动器和性能脚本，可证明当前运行的 ELF 与 Config 是本轮同一次部署。

P3 没有修改 C++，因此继续使用 P2 已 clean build 并通过实机的 AArch64 Release ELF；P3 变更只涉及启动/部署脚本、性能策略和生产配置。

## 10. 正式配置 60 秒矩阵

正式部署后，不再用环境变量覆盖 M1/L1/L2 开关；物理门禁全部来自板端正式 `Config/HwaSimIRRuntime.ini`。

| 场景 | 输入 received/consumed | 队列峰值/最终 | renderFps 稳态 | renderMs | DDS 视频 | 结果 |
|---|---:|---:|---:|---:|---|---|
| NIR daylight Clear | 3599/3599 | 3/0 | 约 70–71 | 约 11.9–12.0 ms | 0 error/drop | PASS |
| NIR night Active off/on/off | 3600/3600 | 3/0 | 约 71 | 约 12.0 ms | 0 error/drop | PASS |
| MWIR Clear | 3599/3599 | 3/0 | 约 70–71 | 约 11.9 ms | 0 error/drop | PASS |
| MWIR Cloudy | 3599/3599 | 3/0 | 约 69–70 | 约 12.6 ms | 0 error/drop | PASS |

四组共同满足：

```text
inputOverwritten=0
inputQueueOverflow=0
sourceSeqGapCount=0
inputBackpressureCount=0
receivedMinusConsumedOverwrittenPending=0
outputOverwritten=0
DDS writeErrors=0
DDS droppedSamples=0
```

Windows VideoDisplay 实测：

```text
receiveFps ≈ 70–71
displayFps ≈ 70–71
decodeMsAvg ≈ 1.0–1.1 ms
sourceSeqContinuous=1
discontinuities=0
h264DecodeErrors=0
decoded geometry=800x800
aspectPreserved=1
```

NIR 夜间主动照明仍保持正确可逆：

```text
activeSensorRadiance = 0 → 0.008 → 0 W/(m²·sr·μm)
finalGray             = 0 → 0.008 → 0
tauOutbound = tauInbound ≈ 0.957
```

`active_display_monotonic_qc.py` 对最终生产日志检查 PASS。

## 11. 正式 10 分钟热稳定测试

条件：完整 M1+L1+L2、MWIR Cloudy、DDS、Mali、MPP、800×800、60 Hz 输入，使用正常 `/userdata/HwaSimIR/run_precise.sh`。

稳态统计（预热后约 9.7 分钟采样）：

| 指标 | 最小 | 平均 | 最大 |
|---|---:|---:|---:|
| renderFps | 60.044 | 70.305 | 71.414 |
| renderMs | 11.938 ms | 12.045 ms | 13.364 ms |
| irUpdateMs | 0.613 ms | 0.641 ms | 1.239 ms |
| readbackMs | 1.826 ms | 1.849 ms | 1.904 ms |
| SoC 温度 | 41.615°C | 42.516°C | 43.461°C |
| GPU 温度 | 40.692°C | 41.607°C | 41.615°C |
| HwaSim_IR RSS | 332012 KiB | 334779 KiB | 342092 KiB |

最终计数：

```text
DDS realtime received = 36000
app queued             = 36000
app consumed           = 36000
pending                = 0
max input queue        = 3
input overwrite        = 0
input overflow         = 0
source sequence gap    = 0
input backpressure     = 0
input backpressure ms  = 0.000

DDS H264 samples       = 42169
DDS video max queue    = 1
DDS write errors       = 0
DDS dropped samples    = 0
```

MPP（媒体处理平台硬件编码）保持：

```text
activeBackend=mpp
encoderName=rockchip_mpp_avc
codec=h264_annexb
keyFrame=true
spsPps=true
800x800
```

GPU 门禁保持：

```text
glVendor=ARM
glRenderer=Mali-LODX
hardwareGpu=1
```

没有出现温度持续上升、热降频、内存持续增长、输入队列持续增长或 DDS/MPP 错误。

## 12. M1/L1/L2 QC 与生产门禁

P3 不改红外物理公式。最终重新运行：

- `tools/m1_nir_mwir_physics_check.ps1`：PASS；
- `tools/l1_natural_solar_qc.py`：15 rows PASS；
- `tools/l2_active_illuminator_check.ps1`：24 rows PASS；
- `tools/active_display_monotonic_qc.py`：PASS。

正式配置：

```ini
[M1NirMwirPhysics]
CompareOnly=false
EnableRuntime=true
EnableNIRRuntime=true
EnableMWIRRuntime=true

[NaturalSolar]
Enable=true
EnableOpticalShadow=true
EnableSolarThermal=true

[ActiveIlluminator]
Enable=true
```

主动照明是否实际发光仍只由协议已有 `WeaponState.illuminatorEn` 实时控制。

DDS 和诊断默认保持：

```text
CommandTransport.Input=dds
DdsProtocol.Enable=true
DdsVideo.Enable=true
FrameScheduling.AsyncInputPolicy=OrderedQueue
TCP Packet v3 payload=false
Stage6Diagnostics.Enable=false
```

## 13. 修改文件

- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`
- `HwaSim_IR/Bin/Config/RK3588PerformancePolicy.conf`
- `tools/rk3588_hwasimir_performance_mode.sh`
- `tools/rk3588_run_hwasimir_precise.sh`
- `tools/rk3588_hwasimir_perf_sample.sh`
- `tools/rk3588_deploy_atomic.ps1`
- `tools/codex_rk3588_pipeline.ps1`
- `docs/HwaSimIR_P3_RK3588_Production_Performance_Deployment_Closeout.md`

## 14. 已知限制

1. 板卡系统时钟错误，需由部署系统或 NTP 单独修复；本轮未修改系统时间。
2. P2 的异步补帧/追赶调度在渲染有较大余量时会产生 `repeatedStateOutputFrames`，因此视频发布/显示可能约 70 FPS，而不是精确锁在 60 FPS。输入仍严格 60 Hz、有序且无合并/丢失，本轮验收条件是 `>=59 FPS`，故不构成 P3 阻塞；后续若需要严格 60.000 FPS，应单独调整输出节拍，不得恢复 Latest 或改变 OrderedQueue 语义。
3. Windows 到板卡的端到端绝对延迟仍因两机时钟未同步显示为不可用；本轮可验证队列、序号、解码和显示帧率，但不能用不一致的绝对时钟伪造 P95 延迟。
4. 本轮没有新增 VIS/SWIR/LWIR、探测器电子/QE、SRF 高保真或地形全阴影功能。

## 15. 证据位置

- `logs/p3-final-evidence/p3_prod_long10_mwir_cloudy.log`
- `logs/p3-final-evidence/p3_prod_long10_mwir_cloudy.csv`
- `logs/p3-final-evidence/p3_prod_nir_day_clear.log`
- `logs/p3-final-evidence/p3_prod_nir_night_active.log`
- `logs/p3-final-evidence/p3_prod_mwir_clear.log`
- `logs/p3-final-evidence/p3_prod_mwir_cloudy.log`
- `logs/p3-final-evidence/active_display_monotonic_qc.json`
- `logs/p3-final-evidence/l2-qc/l2_active_illuminator_qc.csv`
- `logs/p3-final-evidence/logs/p3/isolation_A_cpu_perf_gpu_auto.log`
- `logs/p3-final-evidence/logs/p3/isolation_B_cpu_auto_gpu_perf.log`
- `logs/p3-final-evidence/logs/p3/isolation_C_both_perf.log`
- `logs/p3-final-evidence/logs/p3/isolation_D_both_auto_90s.log`
- `logs/p3-deploy-production-gates/deployment_final.txt`

