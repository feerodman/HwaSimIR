# HwaSimIR ZRDDS 视频传输与板端本地录像集成方案

> 基线提交：`3528e02b12f3e3193a443a40a33b012a91b27544`（DDS许可修改_20260825_1547）  
> ZRDDS：2.4.5  
> Windows SDK：`F:\Programs\ZRDDS\ZRDDS-2.4.5`  
> RK3588 SDK：`/usr/ZRDDS/ZRDDS-2.4.5`  
> RK3588：`192.168.1.116`  
> Windows：`192.168.1.188` / `192.168.123.100`，板卡直连 DDS 优先使用 `192.168.1.188`  
> 交叉编译 VM：`192.168.203.128`  
> VM 工程：`/home/linaro/userdata/HwaSimIR`  
> 板卡禁止联网。

## 1. 硬约束

DDS 只传视频负载，不传标注、实时数据、初始化/控制数据，不封装 TCP Packet v2/v3，也不增加自定义视频头。DDS 使用 ZRDDS 内置 `DDS::Bytes`。

支持两种视频：

- H.264：一个 DDS Sample = 一个完整 H.264 Annex-B Access Unit，Sample 内只有码流字节。
- 未压缩：一个 DDS Sample = 一整帧原始像素，默认 `Gray8`，可保留 `BGR24` 配置能力；宽高/像素格式由收发端配置约定，Sample 内不携带元数据。

现有 TCP 必须保留，运行时至少支持 `TCP only / DDS only / TCP + DDS`。

## 2. Topic 约定

由于 DDS Sample 中没有 codec/尺寸等自定义头，不允许同一 Topic 无约定地混发不同格式。Topic 应可配置，建议默认：

```text
HwaSimIR.Video.precise.H264
HwaSimIR.Video.precise.RawGray8
HwaSimIR.Video.precise.RawBGR24
HwaSimIR.Video.coarse.H264
HwaSimIR.Video.coarse.RawGray8
HwaSimIR.Video.coarse.RawBGR24
```

客户最终 Topic 名可由 INI 覆盖，不能写死。

## 3. 可靠 DDS 方案

禁止生产使用 ZRDDS “UDP 大包零拷贝”，因为该特性只能 BEST_EFFORT。

生产方案：

```text
DDS Type          = DDS::Bytes
Underlying        = ZRDDS tcpv4
Reliability       = RELIABLE_RELIABILITY_QOS
History           = KEEP_ALL_HISTORY_QOS
Application drop  = 禁止
```

ZRDDS SDK 自带 `tcp_dp`，并自带 `non_zerocopy_reliable` 示例 QoS，可作为基线。性能不足时再评估 ZRDDS 2.4.5 TCP 多核传输。

“不丢帧”的工程定义：

- HwaSim_IR 不覆盖 DDS 待发送帧；
- 不使用 latest-only DDS 队列；
- Writer/Reader 使用 RELIABLE；
- Writer 使用 KEEP_ALL；
- 接收端不主动丢已收到 Sample；
- 验收时发送 Sample 数 = 接收 Sample 数；
- 队列达到硬上限时必须背压或显式报错，禁止静默丢帧。

说明：接收端永久离线、网络永久中断或内存耗尽时，VOLATILE DDS 不可能无限保存历史视频；这类情况必须明确失败，而不是静默丢帧。

## 4. Raw 带宽约束

800×800@60：

```text
Gray8  ≈ 38.4 MB/s ≈ 307.2 Mbit/s
BGR24  ≈ 115.2 MB/s ≈ 921.6 Mbit/s
```

BGR24 尚未计算 TCP/IP/DDS 开销，1GbE 不能安全承诺 800×800@60 且零丢帧。因此未压缩默认 Gray8；BGR24 可实现但必须按实测报告最大稳定 FPS。H.264 仍为生产优先模式。

## 5. HwaSim_IR 发送端

目标：

```text
Final Sensor Frame
       |
       +--> RawVideoFrame ---------------------> DDS Raw
       |
       +--> FFmpeg/MPP H264 --> EncodedVideoFrame.payload
                                  |--> Legacy TCP
                                  |--> DDS H264
                                  `--> Local MP4 mux
```

原则：

- DDS 不进入 Panda3D 渲染线程；
- DDS 不重新读取 Panda Texture；
- H.264 DDS 直接复用 `EncodedVideoFrame.payload`；
- TCP+DDS 同时开时，同一帧只编码一次；
- RELIABLE DDS 队列不能覆盖旧帧。

建议新增：

```text
Video/DdsVideoPublisher.h/.cpp
Video/LocalMp4Recorder.h/.cpp
```

首轮不要为了 DDS 大规模重构已稳定的 TcpCommThread。

## 6. DDS 配置

建议在 `HwaSimIRRuntime.ini` 增加：

```ini
[DdsVideo]
Enable=false
Transport=tcp
Reliability=reliable
Codec=auto
RawPixelFormat=gray8
DomainId=150
QosFile=Config/DDS/ZRDDS_QOS_PROFILES.xml

TopicH264Precise=HwaSimIR.Video.precise.H264
TopicRawGray8Precise=HwaSimIR.Video.precise.RawGray8
TopicRawBgr24Precise=HwaSimIR.Video.precise.RawBGR24
TopicH264Coarse=HwaSimIR.Video.coarse.H264
TopicRawGray8Coarse=HwaSimIR.Video.coarse.RawGray8
TopicRawBgr24Coarse=HwaSimIR.Video.coarse.RawBGR24

QueueMaxFrames=1024
BlockWhenQueueFull=true
EnablePerfLog=true
```

`Codec=auto`：

```text
trackerSensorParam.h264En=true  -> DDS H264
trackerSensorParam.h264En=false -> DDS Raw
```

## 7. 板端本地 MP4

协议已有 `trackerSensorParam::saveMP4En`，不新增协议字段。

增加：

```ini
[LocalRecording]
Enable=false
OutputDirectory=/home/linaro/HwaSimIR_Record
Container=mp4
FilePrefix=HwaSimIR
Encoder=auto
BitrateKbps=4000
GopFrames=30
QueueMaxFrames=600
BlockWhenQueueFull=true
EnablePerfLog=true
```

唯一有效门控：

```text
effectiveSaveMp4 =
    LocalRecording.Enable
    &&
    trackerSensorParam.saveMP4En
```

任意一个 false 时，不创建文件、不启动录像编码、不占用录像队列。

保存规则：

- 一回合一个 MP4；
- 开始命令后创建；
- 停止/复位/下一初始化时 flush + close；
- precise/coarse 独立目录/文件；
- 输出目录不存在自动创建；
- 磁盘不足或创建失败明确报错。

若已有 H.264 AU，直接 mux 到 MP4，不重复编码。若 DDS/旧 TCP 当前走 raw/JPEG、但本地录像开启，则本地录像单独请求 H.264：RK3588 用 MPP，Windows smoke 用 FFmpeg，但不重新读回图像。

## 8. HwaSim_IR_VideoDisplay

保留 TCP 接收，新增 DDS 接收模式。

最小配置：

```text
ReceiveTransport = TCP / DDS
DDS DomainId
DDS QoS File
DDS Topic
DDS Codec = H264 / RawGray8 / RawBGR24
Raw Width
Raw Height
```

H.264：

```text
DDS Bytes -> 现有 H264FfmpegDecoder -> QImage -> 原显示链
```

Raw：

```text
DDS Bytes -> sample size 校验 -> Gray8/BGR24 QImage -> 原显示链
```

DDS 模式下不等待 annotation/realtimeData，不解析 TcpVideoPacketV3。

## 9. 客户接收 Demo

新增独立目录：

```text
DDS/HwaSimIRVideoReceiverDemo/
  README.md
  CMakeLists.txt
  main.cpp
  Config/ZRDDS_QOS_PROFILES.xml
```

不依赖 HwaSimIR CommonData/IDL。

参数：

```text
--domain
--topic
--codec h264|raw_gray8|raw_bgr24
--width
--height
--qos
--output
--frames
```

H.264 Sample 原样写 `received.h264`；Raw Sample 校验尺寸后原样写 `received.raw`。输出 `samples/bytes/sampleBytesMinAvgMax/receiveFps/ddsErrors`。

客户只需和我方约定 DomainId、Topic、QoS、codec；raw 模式再约定 width/height/pixel format。

## 10. SDK / VM / CLion

Windows 已安装：

```text
F:\Programs\ZRDDS\ZRDDS-2.4.5
```

RK3588 已安装：

```text
/usr/ZRDDS/ZRDDS-2.4.5
```

VM 只需要目标开发文件，优先从板端实际安装目录复制 `include/` 和 `lib/` 到：

```text
/home/linaro/sysroots/zrdds-aarch64/
```

VM 不能直达板卡时使用 Windows 中转，禁止联网下载。

CMake 新增：

```text
HWASIMIR_ENABLE_ZRDDS
ZRDDS_ROOT
```

CLion Release-aarch64-rk3588-ssh CMake Options：

```text
-DHWASIMIR_ENABLE_RKMPP=ON
-DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp
-DHWASIMIR_ENABLE_ZRDDS=ON
-DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64
-DHWASIMIR_ENABLE_FFMPEG=OFF
-DPANDA3D_ROOT=/opt/panda3d-aarch64
-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
```

CLion 远程工程路径固定 `/home/linaro/userdata/HwaSimIR`。

## 11. 三阶段实施

### D1：ZRDDS 可靠链路、SDK、CLion、客户 Demo 基础

一次完成：

- 最新 licence 核验；
- Windows ZRDDS SDK/License smoke；
- RK3588 SDK/License/ABI smoke；
- VM 建 ZRDDS aarch64 sysroot；
- CMake/VS2015 ZRDDS 可选编译框架；
- CLion 远程交叉编译环境完成并实编译；
- 创建 ZRDDS TCP + RELIABLE + KEEP_ALL QoS；
- 通用 Bytes Publisher/Subscriber；
- 客户 Receiver Demo 第一版；
- Windows<->RK3588 RELIABLE Bytes；
- H264 文件作为纯 Bytes 发送/逐字节比对；
- Raw Gray8 大帧作为纯 Bytes 发送/逐字节比对；
- 统计吞吐/Sample/0 missing；
- 不改 HwaSim_IR 生产视频链。

门禁：

```text
Windows SDK PASS
RK3588 SDK PASS
License PASS
AArch64 link PASS
CLion build PASS
Windows<->RK3588 DDS discovery PASS
RELIABLE H264 Bytes 0 missing PASS
RELIABLE RawGray8 Bytes 0 missing PASS
Customer receiver demo PASS
```

### D2：HwaSim_IR + VideoDisplay + MP4 正式接入

一次完成：

- DdsVideoPublisher；
- H264/Raw DDS；
- TCP/DDS/both；
- h264En 驱动 DDS auto codec；
- VideoDisplay DDS H264/Raw；
- LocalRecording；
- `saveMP4En && config Enable`；
- Windows 端到端 smoke；
- TCP 完整回归；
- MP4 ffprobe；
- 验证 DDS payload 不含 header/annotation/realtime。

### D3：RK3588 生产闭环 + 客户交付

一次完成：

- VM 交叉编译/部署；
- MPP + DDS RELIABLE；
- Raw Gray8 DDS；
- TCP + DDS；
- 板端 MP4；
- precise/coarse；
- 30/60s soak；
- 发送/接收 Sample 数一致；
- 网络短时抖动/慢接收验证无应用层丢帧；
- CPU/内存/带宽/队列/blocking；
- 客户 Demo/README/QoS/ICD/CLion/现场联调文档。

## 12. 客户联调顺序

1. 双方确认 ZRDDS 2.4.5；
2. 检查 licence；
3. 确认 IP/网卡；
4. ping；
5. 放行测试防火墙；
6. 4 KB Bytes；
7. 1 MB Bytes；
8. H.264 文件；
9. Raw Gray8；
10. 再启动 HwaSim_IR；
11. 对照 Domain/Topic/QoS/codec/raw width-height-pixel format；
12. 对比发送/接收 Sample 数；
13. H.264 用 decoder/ffprobe 验证；
14. Raw 用固定帧 hash 验证；
15. 最后 precise/coarse 双进程。

多网卡时板卡 DDS 链路优先：

```text
Windows 192.168.1.188
RK3588  192.168.1.116
```

避免误选 `192.168.123.100` 或 VMware 网卡。

## 13. 最终原则

- 不改现有 UDP 控制协议；
- 不新增 IDL；
- DDS 只用 `DDS::Bytes`；
- DDS Sample 无自定义 header；
- ZRDDS 必须 RELIABLE；
- 不使用 BEST_EFFORT 大包零拷贝作为生产视频；
- 应用层不主动丢 DDS 视频帧；
- 保留现有 TCP；
- H264 尽量一次编码供 TCP/DDS/录像共享；
- 本地录像严格双门控；
- 网络/磁盘资源不足必须明确报警；
- Windows 结果不能替代 RK3588；
- 板卡禁止联网。

## 14. D1 execution record (2026-08-25/26)

D1 was executed against baseline
`3528e02b12f3e3193a443a40a33b012a91b27544`. No commit or push was made.

Results:

- Windows and RK3588 actual `DDSIF::Init` licence checks: PASS.
- Windows VS2015 Release x64 HwaSimIR Bytes pub/sub: PASS, 300/300.
- Unmodified ZRDDS official SimpleInterface Bytes pub/sub sources: build/run
  PASS. Its publisher does not wait for discovery, so the observed 19/20 per
  run is retained as connectivity evidence and is not the zero-missing gate.
- RK3588 AArch64 ABI/runtime: PASS.
- Offline board-to-Windows-to-VM ZRDDS sysroot: PASS.
- AArch64 minimal C++ cross-link/runtime: PASS.
- Customer Receiver Windows and AArch64 builds: PASS.
- RK3588 to Windows 4 KB: PASS, 1000/1000.
- RK3588 to Windows 1 MB: PASS, 100/100 after documented writer drain fix.
- RK3588 to Windows 800x800 Gray8: PASS, 120/120, SHA256 equal.
- RK3588 to Windows H.264 Annex-B AU: PASS, 30/30, SHA256 equal.
- Windows to RK3588 reverse 4 KB smoke: PASS, 500/500, SHA256 equal.
- HwaSim_IR and VideoDisplay Windows Release x64: PASS.
- HwaSim_IR AArch64 + RKMPP + ZRDDS skeleton: PASS.
- CLion Reload and Build using `Release-aarch64-rk3588-ssh`: PASS.

The generic QoS remains `tcpv4://default//0`; lab-only bound profiles select
Windows `192.168.1.188` and RK3588 `192.168.1.116`. All profiles used by D1 are
RELIABLE + KEEP_ALL. No runtime HwaSim_IR DDS publisher was added; that remains
D2 scope.

Open items before D2 are recorded in
`logs/dds-d1-20260825-181131/summary.txt`: runtime package/banner version
mismatch, vendor-supported reliable writer shutdown semantics, CAEP trial
licence mutation/concurrency, and explicit board service environment setup.

## 15. D2 execution record (2026-08-26)

D2 was executed against baseline
`a361ae051279fa0e389a554a0cd791db6e35d2da` (`DDS集成D1阶段_20260826_0030`).
No commit or push was made.

### Integrated components and data flow

- `Video/DdsVideoPublisher`: one process-lifetime `DDSIF::Init`, Participant and
  no-drop writer worker; one queued vector is written verbatim as one
  `DDS::Bytes` Sample.
- `Video/LocalMp4Recorder`: strict `LocalRecording.Enable && saveMP4En` gate,
  no-drop worker, lazy file creation on the first IDR, one file per round, and
  flush/close on STOP, RESET, next INIT, and process shutdown.
- `TcpCommThread` remains the existing video-output worker. It now builds a
  per-frame product plan and shares one H264 encode among TCP, DDS, and local
  MP4. DDS Raw is prepared from the existing final BGR readback and does not
  trigger a second Panda capture.
- `DdsVideoReceiverWorker` runs outside the Qt GUI thread and feeds the existing
  decoder/display path. DDS mode is explicitly video-only (`packetVersion=0`,
  no annotation, no realtime data).

TCP connection state no longer gates DDS or local recording. Upstream async
latest-overwrite is retained only when DDS and effective recording are both
disabled. When either independent output requires no-drop semantics, the
upstream and component queues block for space instead of clearing or replacing
old frames.

### Final configuration and QoS

`HwaSimIRRuntime.ini` now contains the production `[DdsVideo]` and
`[LocalRecording]` sections with 120-frame blocking queues. `Codec=auto` uses
only `trackerSensorParam.h264En`; the TCP codec cannot change DDS auto
selection. Topics are the six precise/coarse H264, RawGray8, and RawBGR24 names
specified in `docs/DDS_HwaSimIR_ICD.md`.

The D1 QoS remains unchanged: `tcpv4`, RELIABLE writer/reader, KEEP_ALL writer
history, and bounded resource limits. No BEST_EFFORT profile, UDP large-package
zero-copy, IDL, ACK topic, or custom video header was added.

### Windows functional results

- DDS-only H264 full round: 120 sent / 120 received, 9,764 bytes, 61.047
  Samples/s, writer/reader/drop errors all zero. Sender/receiver SHA256:
  `9629D4C5DC21B55F81A6DD95BD8AA88A3A714CFA415780FF2EA4D639B3EA7E1F`.
- DDS RawGray8 full round: 61/61, 39,040,000 bytes, every Sample exactly
  640,000 bytes, 60.582 Samples/s, errors/drop zero. SHA256:
  `C82A6F723D166F4FB2E03D72ED253ACA54879C7A415D7B4B67B87E623253B7A5`.
- DDS RawBGR24: 10/10, every Sample 1,920,000 bytes; VideoDisplay RGB conversion
  and orientation were inspected. Sender/receiver hash matched.
- DDS VideoDisplay decoded H264 continuously (about 60.2 FPS) and displayed
  RawGray8 and RawBGR24 without waiting for realtime/annotation data.
- TCP-only Packet v3 retained video, annotation, and realtime flags. R1 route,
  Packet v3, TCP reconnect, and reset/init IDR regressions passed.
- TCP+DDS H264 ran simultaneously; both consumers received continuous video and
  `[VideoOutputProducts]` recorded `h264EncodeCount=1`, `tcpH264=1`, and
  `ddsH264=1` for the same frame.

The H264, Gray8, and BGR24 test-only sender audit files matched customer
receiver output byte-for-byte. Audit output is disabled by default and is not
part of the production wire contract.

### Local MP4 results

The four gate combinations produced 0, 0, 0, and 1 MP4 respectively. The
enabled Windows case used `shared_h264_remux`, wrote 121/121 frames with zero
drops, and passed ffprobe/decode at 800x800, 60 FPS, 2.016667 seconds. Two
start/stop rounds plus a reset case produced three separate valid MP4 files;
each recorder session closed with inputFrames equal to writtenFrames and zero
drops.

### RK3588 short functional loop

An isolated native AArch64 build on the board passed and linked the target
`librockchip_mpp.so.1`, `libZRDDSCpp.so`, and `libavformat.so.58` libraries.
The production checkout binary was not overwritten.

- MPP+DDS H264: audit window 120/120, 10,219 bytes, errors/drop zero; SHA256
  `1A6ED98AB6A84E5B1277207E2624FBBE8A8B1424F7434440C148F9B2CEAF5617`;
  Windows FFmpeg decode passed.
- DDS RawGray8: audit window 10/10, 6,400,000 bytes, 640,000 bytes/Sample,
  errors/drop zero; SHA256
  `56E3CC9FBDFEDF1AB4F67FDE0EF1B9CD4ED01CC8CE7444B7F478C238EFB22C88`.
- Local MP4: `shared_h264_remux`, 70/70 frames, zero drops, 800x800 at 60 FPS,
  1.166667 seconds; ffprobe and decode passed after copying to Windows.
- TCP+DDS simultaneous: DDS audit window 30/30 with matching SHA256; TCP Packet
  v3 H264 plus annotation/realtime decoded concurrently; one shared MPP H264
  encode was recorded.

The installed directory is labelled ZRDDS 2.4.5 while the actual runtime banner
remains `2.4.4-r6873577`; D2 uses only APIs verified during D1. On 2026-08-26 the
Debian VM endpoint recovered and the documented `Release-aarch64-rk3588-ssh`
profile was rerun through its command-line equivalent. CMake Reload passed,
`HwaSim_IR` built 32/32 with exit code zero, and the result was verified as an
ELF64 AArch64 binary linked to `librockchip_mpp.so.1`, `libZRDDSCpp.so`, and
`libavformat.so.58`. The CLion Reload/Build gate is therefore PASS. Full
performance/soak remains D3 scope.
