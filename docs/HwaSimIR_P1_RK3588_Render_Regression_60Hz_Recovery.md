# HwaSimIR P1：RK3588 Render Regression Isolation & 60 Hz Recovery

日期：2026-09-09  
源码基线：`main`（开始时 `d437d87`）  
板卡：真实 RK3588，`192.168.1.116`，`aarch64`  
最终部署 ELF SHA-256：`66d808637c7d00f9ccc19c7ad3b887624c8a2b9d66ba2c2060867be2c9d2497d`  
最终 Build ID：`1bd224efb7178248cf9bc05db882dd9e1a2feaa9`

## 1. 结论

本轮已经把 RK3588 的主体渲染从 A1 初始的约 `44–48 FPS / 20–22 ms` 恢复到完整 NIR M1+L1+L2、Cloudy、DDS H264 下预热后 `59.637 FPS / 16.295 ms`。Mali、MPP 和 DDS 均保持正常。

回归不是 MODTRAN SI 数据、DDS serialization、MPP 编码器或 Mali 驱动单独造成。主要原因是：

1. M1/L1/L2 关闭时仍执行太阳环境构造、目标 bounds 遍历和 L1/L2 visibility 工作；
2. L1/L2 打开后反复调用 `calc_tight_bounds()` 遍历模型；
3. Stage7 生产配置回到了三张全域 `Layered2_5D` 云切片；
4. `StreamedWorld3D` 又错误沿用三张远场切片，而设计语义应为“一张远场切片 + 流式局部体积云”；
5. `sourceSeq=0` 被 Stage6 日志判断为“前 3 帧”，导致无新业务输入的重复渲染帧每帧输出 MTF/Noise/AGC 三条长日志，并重复触发 Annotation/IR 更新。

渲染 60 Hz 主体恢复，但严格 production gate 仍未全部满足：异步 `AsyncInputPolicy=Latest` 在 DDS 批量到达时仍主动合并未消费的 Realtime 样本，最终 60 s Cloudy 记录 `inputOverwritten=1643`。这是真实输入合并，不是 silent clamp 或计数伪造。因此本轮不把 M1/L1/L2 生产开关改为 true。

## 2. MODTRAN 是否是回归源

不是。

同一最新 ELF、相同几何/天气/传感器，physics 全关时 MODTRAN lookup 为 `0 ms`，但修复前仍只有约 `47.8–48.7 FPS`、`renderMs=20.0–20.4 ms`。M1 单独打开后的 LUT lookup 约 `0.14–0.30 ms`，量级不足以解释约 5 ms 回归。

| Feature | 观测 | 结论 |
|---|---:|---|
| physics 全 OFF，修复前 | 47.8–48.7 FPS；20.0–20.4 ms | 回归已存在 |
| M1 only | lookup 0.14–0.30 ms | 亚毫秒增量 |
| M1+L1（bounds cache 前） | NIR 约 55 FPS | L1 几何 bounds 是明显 CPU 热点 |
| M1+L1（bounds cache 后） | NIR 16.45–16.77 ms；MWIR 16.03–16.40 ms | 精度不变，遍历开销被移除 |
| M1+L1+L2 NIR active | 16.22–16.62 ms；active sensor radiance `0.008231 W/(m²·sr·µm)` | L2 物理本身不是 5 ms 根因 |
| M1+L1+L2 MWIR active | 14.90–15.00 ms；active sensor radiance `0.001767 W/(m²·sr·µm)` | 同上 |

没有修改 MODTRAN 单位、黄金 case、插值或 LUT 精度。

## 3. old/new ELF 同配置 A/B

所有 ELF 使用同一板端 `Config`、相同 Xorg/Mali、相同 governor 和相同输入。

| ELF | SHA-256 | physics OFF 结果 |
|---|---|---:|
| A1 前备份 `HwaSim_IR.before_a1_final_20260908` | `f061232401330bd275907ace5c986154033f3b46e989f3663c722904ba106851` | 40.0–42.9 FPS；22.6–24.2 ms |
| P1 开始时 `HwaSim_IR.pre_p1_20260909` | `268f88f227e2742131e922af93773344a45fd889eb9c61b008e604d154da3a73` | 47.8–48.7 FPS；20.0–20.4 ms |
| 源码级 known-good `fbac7275558b004498a7fa6c036c3007e5abd871` | `7a68751fe527cff43115de1ab7cf456d84365d7e579a0493e16de16c3e669f3c` | 输入有效窗口 56.663 FPS；16.338 ms |
| P1 最终 clean | `66d808637c7d00f9ccc19c7ad3b887624c8a2b9d66ba2c2060867be2c9d2497d` | 完整物理 Cloudy 59.637 FPS；16.295 ms |

历史报告中的两次基准为 `59.028 FPS / 16.383 ms` 和 `58.761 FPS / 16.454 ms`。源码级 known-good 的 `renderMs=16.338` 重现了历史 GPU/scene 时间，但旧 deadline 调度本身没有优于最终 P1。

## 4. DDS / legacy / render-only 隔离

使用同一 P1 开始时最新 ELF、physics 全 OFF：

| 输出模式 | FPS | renderMs | 其他 |
|---|---:|---:|---|
| DDS production | 47.8–48.7 | 20.0–20.4 | DDS write 约 0.17 ms；queue=1；无 DDS error/drop |
| legacy UDP + TCP H264 | 45.95–46.46 | 20.8–21.2 | NV12 约 8.0 ms；MPP 约 2.03 ms |
| render-only，无 DDS/TCP/local record | 59.702 | 14.911 | MPP 不启动 |

DDS 与 legacy 都慢、render-only 能到 60，说明视频 worker 会消耗 CPU 预算，但 `renderMs` 回归仍在 Panda/scene 路径；DDS 并非特有根因。最终实现继续保持 DDS production 默认，没有回退通信架构。

## 5. governor、频率与温度

正式测试均在板卡默认 governor 下完成：

- CPU：`ondemand`；负载时观测 policy0 约 1.608 GHz、policy4/6 约 2.016 GHz；测试结束恢复/确认 `ondemand`。
- GPU：`simple_ondemand`；可用最高频率 1 GHz；测试结束空闲为 300 MHz。
- 正式测试温度约 42.5 °C；最终停止后最高约 39.8 °C。
- 未发现 thermal throttling。
- 最终没有遗留 HwaSim_IR 进程，也没有把 performance governor 留在板卡上。

## 6. renderMs 细分

P1 增加/使用的低开销聚合字段包括：

`sceneUpdateMs`、`annotationMs`、`irUpdateMs`、`irEnvBuildMs`、`stage7SkyGroundMs`、`platformRadianceMs`、`targetRadianceMs`、`stage4HotspotMs`、`stage5PlumeMs`、`stage5RadianceComponentMs`、`stage5AeroThermalMs`、`stage5ModtranLookupMs`、`shaderInputApplyMs`、`renderMs`、`readbackMs`。

最终 clean Cloudy 60 s（去掉前 5 s）平均：

| 项目 | 平均 |
|---|---:|
| renderFps | 59.637 FPS |
| renderMs | 16.295 ms |
| irUpdateMs | 0.802 ms |
| readbackMs | 1.955 ms |
| DDS VideoMeta/Annotation | 3561 / 3561 |
| DDS frame-product writeErrors | 0 |

`renderMs` 仍是 `framework.do_frame()` 上界，细分 CPU 项独立记录。P95 未在板端 interval 日志中持久化，不能把 max 或平均伪报为 P95。

## 7. Stage7 / annotation A/B

修复前、physics OFF、render-only：

| A/B | renderMs / FPS | 解释 |
|---|---:|---|
| 完整 `Layered2_5D` + annotation | 17.30–17.48 ms；约 55.8 FPS | 回归状态 |
| annotation overlay OFF | 16.70–16.92 ms；最高约 58.6 FPS | annotation 有增量，但不是全部 |
| CloudLayer OFF | 16.61–16.80 ms；约 58.7 FPS | 三张全域云切片有明显增量 |
| `StreamedWorld3D`, MaxVisible=1 | 16.187–16.215 ms；59.94–60.15 FPS | 保留天气主体并恢复基线 |

Cloudy 实测又发现 `visibleCloudVolumes=0` 时仍绘制三张远场切片。修正 `StreamedWorld3D` 为一张远场切片后：

- 修正前 Cloudy：`57.885 FPS / 16.807 ms`；
- 修正后 Cloudy：`59.373–59.637 FPS / 16.295–16.379 ms`。

没有关闭 CloudLayer、WeatherEffects 或流式体积云；只移除了 hybrid 模式中两个重复的全域切片。

## 8. shader variant A/B

增加仅用于 P1 诊断的 `P1IRShaderVariant=LegacyMinimal`：通过 GLSL compile-time define 真正编译掉 M1/L1/L2 varyings 和 fragment 分支，而不是仅把 uniform 设为 0。

真实 Mali A/B：

- `Full`：约 16.17–16.23 ms；
- `LegacyMinimal`：约 16.17–16.23 ms。

差异在噪声内，因此新增 NIR solar/active shader 数学不是本次 5 ms 主因。生产默认仍是 `Full`；没有用 LegacyMinimal 作为通过手段。

## 9. 修改文件

- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`
  - disabled M1/L1/L2 早返回；
  - L1/L2 immutable local bounds cache；
  - stable shader-input cache 路径；
  - P1 compile-time shader variant 诊断；
  - `StreamedWorld3D` 单远场切片；
  - quiet audit log 降频；
  - `sourceSeq=0` 不再误触发 Stage6/Annotation/IR 周期工作。
- `HwaSim_IR/HwaSim_IR/HwaSimIR.h`
  - target local bounds cache 和 helper 声明。
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`
  - `QuietPerfMode=true`；
  - `CloudRenderMode=StreamedWorld3D`；
  - volumetric cloud enabled，`MaxVisibleVolumes=1`；
  - M1/L1/L2 production gate 保持 false。
- `DataDrivenTestQT/mainwindow.cpp`
  - INIT 使用已有 `m_protocolEnvSky`，修复 `--env-sky` 被硬编码 5 覆盖的问题；协议布局未变。

## 10. 修改前后

| 状态 | FPS | renderMs | IR update | 日志量 |
|---|---:|---:|---:|---:|
| A1/P1 初始 DDS，physics OFF | 47.8–48.7 | 20.0–20.4 | 4.3–4.6 ms | 高频 |
| guard + bounds + Stage7 修复，Clear | 59.52 | 16.29–16.34 | 0.81–0.90 ms | 降频前 |
| Cloud hybrid slice 修复 | 59.37–59.64 | 16.30–16.38 | 0.80–0.85 ms | 降频前 |
| `sourceSeq=0` 修复后最终 clean Cloudy | 59.637 | 16.295 | 0.802 ms | 约 0.30 MB/60 s |

修复前一份 60 s Cloudy 日志约 2.8 MB，其中 Stage6 MTF/Noise/AGC 各约 2385 行；修复后这些 zero-sequence 洪泛消失。

## 11. M1/L1/L2 单独性能增量

固定场景的短时板端 isolation：

| Feature | renderMs / FPS | IR/lookup 说明 |
|---|---:|---|
| baseline physics OFF | 16.19–16.22 ms；约 60 FPS | MODTRAN lookup=0 |
| M1 NIR | 约 60 FPS | lookup 0.14–0.30 ms；tau=0.957283 |
| M1 MWIR | 约 60 FPS | tau=0.707717；pathThermal=0.014974 |
| M1+L1 NIR daylight | 16.45–16.77 ms；约 58–59（短窗口） | bounds cache 后 |
| M1+L1 MWIR solar thermal | 16.03–16.40 ms；约 59–60 | directional thermal 保留 |
| M1+L1+L2 NIR active | 16.22–16.62 ms；约 58.9–60.1 | active radiance 为正 |
| M1+L1+L2 MWIR active | 14.90–15.00 ms；约 60 | active 不改变 thermal state |

最终 60 s Cloudy 使用完整 M1+L1+L2 NIR active，并达到 59.637 FPS。由于单项矩阵中部分只有短窗口、且输入 overwrite 未清零，不能把整张 production gate 写成 PASS。

## 12. Clear / Cloudy 持续测试

均为真实 RK3588、Mali-LODX、HeadlessOffscreen、MPP H264、DDS、800×800、完整 M1+L1+L2 NIR active；预热 5 s 后统计。

| Case | FPS | renderMs | DDS frames | DDS errors |
|---|---:|---:|---:|---:|
| Clear-01 | 59.522 | 16.340 | 3571 | 0 |
| Clear-02 | 59.519 | 16.289 | 3571 | 0 |
| Cloudy-01（cloud slice fix clean） | 59.463 | 16.374 | 3544 | 0 |
| Cloudy-02（cloud slice fix clean） | 59.373 | 16.379 | 3549 | 0 |
| Cloudy-final（zero-seq fix final clean） | 59.637 | 16.295 | 3561 | 0 |

日志确认：

- `glVendor=ARM`；
- `glRenderer=Mali-LODX`；
- `hardwareGpu=1`；
- MPP encoder selected；首个成功 AU 含 SPS/PPS 和 IDR，Annex-B；
- DDS frame-product `writeErrors=0`；
- `sourceSeqLag=0`；
- queue depth 观测为 0–1；
- 无 llvmpipe/softpipe。

板端完整日志归档：`logs/p1-rk3588-baseline-20260908/p1-rk3588-final-20260909.tgz`。

## 13. 构建与自动回归

通过：

- HwaSim_IR Windows Release x64；
- HwaSim_IR_VideoDisplay Windows Release x64；
- DataDrivenTestQT Release MinGW；
- aarch64 `--clean-first --config Release`；
- DDS protocol route test：7 cases；
- DDS video topic resolver：5 cases；
- DDS protocol roundtrip：PASS，布局 `24/385/506/17`；
- M1 NIR/MWIR strict QC；
- L1 natural solar QC；
- L2 active illuminator QC（24 rows）；
- NIR active display monotonic self-test；
- `git diff --check`。

## 14. production gate

通信默认保持：

```ini
[CommandTransport]
Input=dds
Ack=match_input

[DdsProtocol]
Enable=true

[DdsVideo]
Enable=true
```

TCP production payload 继续关闭；未回退 DDS。

物理默认保持：

```ini
[M1NirMwirPhysics]
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

阻塞原因：最终完整物理 60 s 虽达到 59.637 FPS、queue 0–1、sourceSeqLag=0、DDS/MPP 零错误，但 `AsyncInputPolicy=Latest` 仍在 DDS reader 批量交付时合并 pending Realtime，`inputOverwritten=1643`。验收条件明确要求 steady-state overwrite=0，因此不能开启正式物理 gate。

下一步只应处理这一已隔离的输入消费/调度问题：让 DDS Realtime batch 与 60 Hz render cadence 一一消费，或建立有界、可证明不增长的单帧 staging；不得通过不计数、改名或继续丢 Latest 数据来伪造 PASS。

## 15. 已知限制

- interval 日志未提供真实 P95，本文未伪造 P95；
- VideoDisplay 的端到端 latency 在 DDS H264 路径缺少可用 source wall timestamp，显示为 unavailable，不写成 0；
- sync 模式探针没有形成可靠 DDS 60 Hz/STOP 闭环，不能用它替代 production async 结果；
- `P1IRShaderVariant=LegacyMinimal` 只用于诊断，不是生产配置；
- 本轮没有修改 MODTRAN 物理、分辨率、CommonData、DDS IDL、天气主体、自然太阳或主动照明功能。

