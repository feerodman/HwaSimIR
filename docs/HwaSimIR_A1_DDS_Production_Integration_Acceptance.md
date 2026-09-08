# HwaSimIR A1 DDS 生产集成验收报告

日期：2026-09-08  
基线：`main`，开始时 `82dd847ecf033c6b993217f12e1353125a8b90d1`；本轮修改未提交。  
结论：**DDS 已完成三程序生产默认切换；A1 综合硬门禁未全部通过，因此 M1/L1/L2 物理生产 gate 保持关闭。**

## 1. 范围与 F1 历史结论

F1 已经完成且本轮没有回退的能力包括 DDS RESET、INIT、InitAck、START、60 Hz Realtime、STOP、VideoStatus、H264 `DDS::Bytes`、VideoDisplay DDS H264 解码显示以及 DataDrivenTestQT pure DDS。F1 的实现证据见 `docs/HwaSimIR_DDS_F1_Execution_Report.md`。

A1 的新增工作不是重新实现 F1，而是：把 DDS 改成三个程序的无参数生产默认；禁止默认同时运行旧 UDP/TCP 业务链；保留显式 legacy 恢复；在真实 RK3588 验证 Mali、MPP 和 pure DDS；并把 M1/L1/L2 production gate 绑定到综合硬门禁。

`docs/DDS调试.md` 和 `docs/DDS_Customer_Video_Debug_Guide.md` 已加历史版本警告。二者原有“D2 只有视频 DDS、控制仍是 UDP”的说法只描述 2026-08-26 D2，已经被 F1 full DDS supersede。

## 2. 三程序默认通信修改

| 程序 | 修改前默认 | A1 默认 | legacy 恢复 |
|---|---|---|---|
| DataDrivenTestQT | `udp` | `dds`，日志 `ControlTransport=dds source=production_default` | `--control-transport=udp` 或 `both` |
| HwaSim_IR | UDP ingress、TCP Packet v3 常开 | `CommandTransport.Input=dds`、`Ack=match_input`、DDS protocol/video 开、TCP payload 全关 | 临时配置 `Input=udp`、`DdsProtocol.Enable=false`、`DdsVideo.Enable=false`、TCP payload 开 |
| HwaSim_IR_VideoDisplay | 缺省回退 `tcp` | 配置与代码缺省均为 `dds` | `--receive-transport=tcp` |

DataDriven DDS 模式不创建 UDP sender，也不发送 UDP counterpart；若构建不含 ZRDDS 则 fail-fast。HwaSim_IR 的 `dds` 模式不启动 UDP ingress，协议 DDS 与视频 DDS 共享同一个 `DdsRuntimeManager`。TCP worker 类名保留，但 DDS/录像工作不再要求建立 TCP 视频连接。VideoDisplay 的 DDS 初始化失败也 fail-fast，不静默回退 TCP。

默认启动不需要 transport 参数：

```text
DataDrivenTestQT.exe
HwaSim_IR.exe --channel precise
HwaSim_IR_VideoDisplay.exe
```

## 3. DDS 契约与部署

- Domain：`150`
- QoS：`Config/DDS/ZRDDS_PROTOCOL_QOS.xml`
- discovery：QoS 中 `tcpv4://default//0`，未写死网卡
- typed Topics：`HwaSimIR.Control`、`HwaSimIR.Init`、`HwaSimIR.Realtime`、`HwaSimIR.InitAck`、`HwaSimIR.VideoStatus`
- identity frame Topics：`HwaSimIR.Video.{platID}.{sensorID}.{codec}`、`HwaSimIR.VideoMeta.{platID}.{sensorID}`、`HwaSimIR.Annotation.{platID}.{sensorID}`
- 视频契约：一个 Annex-B AU 对应一个 `DDS::Bytes` Sample，不分片
- 协议布局保持 `24/385/506/17` bytes；未改 IDL 和 `CommonData.h`

Windows 到板卡使用的实际直连 NIC 是 `192.168.1.188/24`，板卡为 `192.168.1.116`。ZRDDS 自身使用 TCP discovery/transport 是 DDS 基础设施，不属于 legacy TCP Packet v3。

## 4. Windows pure DDS 默认回归

证据目录：`logs/a1-20260907-230826/windows-default-dds-pass/` 和最新复测
`logs/a1-final/windows-default-dds-current-pass/`、
`logs/a1-final/windows-default-dds-current-pass-logged/`。

三个程序均在**没有 transport CLI 参数**的条件下启动。结果：

- DataDriven：默认 DDS，RESET/INIT/DDS InitAck/START/Realtime/STOP 完整；Stim runtime init count 为 1。
- HwaSim_IR：`CommandTransport input=dds`，protocol `runtimeInitCount=1`，video `runtimeInitCount=1 sharedRuntime=1`。
- TCP payload：Video/Annotation/Realtime/ForwardInitControl 均为 0，没有 TCP Packet v3 业务输出。
- identity Topic：`HwaSimIR.Video.1001.2.H264`。
- H264/Meta/Annotation：发送和接收均为 `481/481/481`，pending 为 0，mismatch 为 0。
- DDS writer：481 Samples，39,218 bytes，write errors 0，drops 0，max queue depth 1。
- VideoDisplay：H264 解码几何 800x800，接收/显示约 59.4–60.2 Hz，DDS errors 0。
- DDS PTS 当前是流内相对时间，不是跨主机 wall clock，因此现有日志的 latency 为 `-1`；这不是已通过的端到端时延测量。

2026-09-08 曾出现 DataDriven MinGW ZRDDS runtime 在 `DDSIF::Init` 失败，证据为
`logs/a1-20260908-nir-night-dds/stim.err.log`。根因是 DataDriven Release 目录中的部署副本与本机已安装、且被 HwaSim_IR/VideoDisplay 正常使用的合法 ZRDDS licence 文件不一致。旧副本保存在
`build-DataDrivenTestQT-codex-mingw73_64-Release/release/zrddslicence.lic.before_a1_20260908`，随后仅把本机安装目录中的原始合法文件原样同步到该 Release 目录；未修改、破解或输出 licence 内容。同步后的默认 DDS 启动日志
`logs/a1-final/stim-license-sync.out.log` 记录 `initialized=1 initCount=1` 和 `ready=1`，该 blocker 已解除。

最新 Windows 默认 DDS 复测仍未传任何 transport CLI 参数。DataDriven 日志明确打印
`ControlTransport=dds source=production_default`，DDS InitAck 成功，实时发送瞬时频率为
`60.466/60.022/59.985 Hz`。HwaSim_IR 收到 DDS RESET/INIT/START、480 个 realtime ingress 和 STOP；FFmpeg/libx264 输出 Annex-B H264，DDS 最终发送 `478/478/478` 个 H264/VideoMeta/Annotation，write errors 0、dropped samples 0、max queue depth 1。VideoDisplay 明确打印 `Transport=dds`，FFmpeg 首个 key frame 解码成功，800x800 几何与 VideoStatus 一致，稳定接收/显示约 `59.46–59.83 Hz`、H264 decode errors 0、source sequence discontinuities 0。一次完整落盘复测的 `output.mp4` 经 ffprobe 核对为 H264、800x800、60/1、477 帧、7.949983 s，两个 annotation 文件也各为 477 行。帧数相差 1 是停止边界上的录像落盘边界，不是 DDS 产品错配；HwaSim_IR 的同轮三种 DDS 产品计数严格一致。带完整 Qt 日志的第二次复测因 recorder 在 VideoStatus 切换后才开始，落盘 447 帧，但 DDS receiver 已收到 477 帧且无不连续。

DataDriven 仍会对默认 `1.txt` 的边界行打印两条既有 `OpaPg data error`，随后明确加载 4318 rows 并完成全套 DDS 状态机；它不是 DDS 初始化、发送或协议布局错误，未作为传输 blocker 处理。

## 5. Legacy 显式回归

证据：`logs/phase2a-final-20260908-032153/`。测试脚本临时写入 UDP/TCP 配置并在退出时恢复：

- DataDriven：`--control-transport=udp`
- VideoDisplay：`--receive-transport=tcp`
- HwaSim_IR：`Input=udp`、DDS protocol/video 关闭、TCP Packet v3 payload 开

结果：发送 60.092 Hz、输入 60.190 Hz、render 60.276 Hz、output 60.189 Hz、display 60.168 Hz；latency average 12.286 ms、P95 16.894 ms；source sequence lag 0、drops 0、overwrites 0。由此证明“默认 DDS”没有删除旧通信能力。

## 6. NIR 夜间 active WhiteHot 数值链

问题不是 active 物理辐亮度符号。固定目标、WhiteHot=1、AGC disabled、显示参数不变时，新增 trace 为：

```text
activeSensorRadiance
 -> totalSensorRadiance
 -> Stage5 shader input
 -> Stage5 physical output
 -> Stage6 pre-AGC
 -> Stage6 post-AGC
 -> WhiteHot
 -> final gray
```

证据：`logs/phase2a-final-20260908-035006/`。

- off/on/off active sensor radiance：`0 / 0.008229 / 0 W/(m^2 sr um)`
- 上述各数值阶段：`0 / 0.008229 / 0`
- WhiteHot=1，AGC=0，gain=1，offset=0
- `tools/active_display_monotonic_qc.py` 自测和日志检查 PASS

像素实现中还修正了一个独立单位错误：CPU 已按米计算 inverse-square、双程 MODTRAN、材料反射和光谱宽度，shader 以前又拿带模型缩放的 object-local 距离重复做 `(Rref/R)^2`。现在 CPU 传入目标中心的 SI active sensor radiance，shader 只施加 per-fragment cone factor 与 `NdotL`，不再把局部模型单位冒充米。

稳定 on 与 off-after 的 8-bit/JPEG ROI 均值同为约 `76.269`。QC 使用 `off_after_stable` 为像素基准且只保留 `1e-6 gray` 浮点容差，因此已证明“不反向变暗”和 pre-display 单调，但没有证明这个 22.3 km、仅数像素目标在当前 8-bit/JPEG 量化下有可见增亮。off-before 的 `76.341` 含启动/目标加载过渡，只作为 `startupDeltaGray` 诊断，不用来声称 active 造成变暗。没有修改 DisplayGain、offset、tone map、body floor 或 illuminator power。

## 7. 真实 RK3588

板卡：`192.168.1.116`，`uname -m=aarch64`。密码未写入仓库或报告。

### 7.1 最新 clean Release 与部署

最新源码在 VM 新目录 `/home/linaro/userdata/HwaSimIR/cmake-build-a1-final-20260908` clean configure/build PASS：

- ELF：64-bit ARM aarch64
- NEEDED：`librockchip_mpp.so.1`、`libZRDDSCpp.so`
- ELF SHA-256：`268F88F227E2742131E922AF93773344A45FD889EB9C61B008E604D154DA3A73`
- 板端 `/userdata/HwaSimIR/HwaSim_IR` SHA-256 与上述完全一致
- 替换前产物保存为 `/userdata/HwaSimIR/HwaSim_IR.before_a1_final_20260908`

板端 runtime、DDS QoS、MaterialDatabase、MaterialBandOptics 的 SHA-256 均与仓库 `HwaSim_IR/Bin/Config` 对应文件一致。

### 7.2 Mali 与 MPP

最新 ELF 的 2026-09-08 HeadlessOffscreen smoke：

```text
glVendor=ARM
glRenderer=Mali-LODX
hardwareGpu=1
```

没有 llvmpipe/softpipe。Xorg `:0` 存在。DDS protocol/video 均 ready，且共享 `runtimeInitCount=1`。

A1 pure DDS 实机日志 `logs/a1-20260907-230826/rk3588-pure-dds-performance/` 证明 MPP 被选中，首个 H264 AU `keyFrame=true`、`spsPps=true`、Annex-B，Windows VideoDisplay FFmpeg 解码成功且几何与 VideoStatus 一致。没有把软件 FFmpeg/JPEG 回退记录为 MPP PASS。

### 7.3 pure DDS 60 Hz 硬门禁

60 秒与 performance governor 复测均没有使用 legacy UDP ingress 或 TCP Packet v3。Windows DataDriven DDS realtime 约 60.0 Hz；RK3588 渲染和 DDS H264 输出稳定约 44–46 Hz，performance 复测累计 input overwritten 443，Windows VideoDisplay 最终接收 893 Samples、DDS errors 0。

主要时序证据是 RK `renderMs` 约 21.1–21.7 ms，再加读回/MPP 前处理，无法达到 16.67 ms。MPP 编码本身成功，DDS writer/Windows 解码也没有报错，但完整链没有达到 60 Hz。因此该硬门禁判定 **FAIL**，不能写成 RK3588 pure DDS 60 Hz PASS。

测试结束后板端 HwaSim_IR 已停止，CPU governor 已从临时 performance 恢复为 `ondemand`，GPU governor 为 `simple_ondemand`。

## 8. M1/L1/L2 综合门禁

本轮自动回归：

- `tools/m1_nir_mwir_physics_check.ps1`：PASS
- `python tools/l1_natural_solar_qc.py`：PASS（15 rows）
- `tools/l2_active_illuminator_check.ps1`：PASS（24 rows，材料部署 hash 一致）
- `python tools/active_display_monotonic_qc.py --self-test`：PASS
- DDS adapter roundtrip：PASS，sizes `24/385/506/17`、flags `0x41/0x36/0x38/0x37`
- DDS protocol route：PASS（7 cases）；identity video topic resolver：PASS（5 cases）
- Windows NIR night 数值 trace：PASS；8-bit/JPEG 可见增亮：未证明
- Windows pure DDS 默认闭环：2026-09-07 PASS；2026-09-08 合法 licence 部署同步后复测 PASS（478 个对齐 DDS 产品，0 write error/drop）
- RK3588 Mali：PASS
- RK3588 MPP H264/DDS：PASS
- RK3588 pure DDS 60 Hz：FAIL（44–46 Hz）
- NIR daylight/night、MWIR、Clear/Cloudy 的最新 M1+L1+L2 pure DDS 实机综合矩阵：因 RK3588 60 Hz 硬门禁失败而未完成
- precise identity Topic 已验证；coarse/双进程并发本轮未完成

因此生产配置保留：

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

DDS 通信默认不回退：`CommandTransport.Input=dds`、`DdsProtocol.Enable=true`、`DdsVideo.Enable=true`、TCP payload 全 false。

## 9. 构建结果与产物 hash

- DataDrivenTestQT Release MinGW：PASS；SHA-256 `A6E53F411F324AA3D91C428C3AB1F67D6F64CC49EA458FAD827EA3A1EC3B0928`
- HwaSim_IR Windows Release x64（FFmpeg enabled）：PASS；SHA-256 `2E03001AF5315214AB0AD2D26E13AEB55B6C864746433376345694E5996DDC11`
- VideoDisplay Windows Release x64：PASS；SHA-256 `8CC833B5541BF87590B65D6589FD8207B5671A6E13437BCF29233525D994A3F4`
- HwaSim_IR AArch64 clean Release：PASS；SHA-256 `268F88F227E2742131E922AF93773344A45FD889EB9C61B008E604D154DA3A73`

Windows HwaSim_IR 的正式运行副本位于 `HwaSim_IR/Bin/HwaSim_IR.exe`。本轮确认只运行嵌套 MSBuild 输出会使相对资源根错误地落到 `HwaSim_IR/HwaSim_IR/Bin/Config`；复制到正式 Bin 后，TargetLib、material XML/TIF 和材料数据库均从统一运行目录解析。

## 10. Remaining limitations / blockers

1. RK3588 HeadlessOffscreen 当前 44–46 Hz，未达到 A1 60 Hz 硬门禁；瓶颈首先在约 21 ms render，而非 DDS writer 或 Windows decoder。
2. DDS VideoMeta PTS 是流相对时间，尚不能直接给出跨主机 average/P95 wall-clock latency。
3. NIR night active 的物理与 pre-display 数值单调已通过，但当前远距离小目标在 8-bit/JPEG 下未形成可测可见增量。
4. coarse identity 与双进程 Topic 隔离、最新 M1/L1/L2 实机完整矩阵尚未完成。
5. 不在 A1 范围内且未实现：VIS/SWIR/LWIR、detector electron/QE、高保真 SRF、terrain full shadow、FEM/CFD。

A1 到此停止；未开启 M1/L1/L2 production gate，也未进入后续波段或 detector 阶段。
