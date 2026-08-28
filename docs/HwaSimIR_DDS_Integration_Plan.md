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

## 16. D3 execution record (2026-08-27)

D3 was executed against baseline
`b46458b781e9943667305a4e7341c7f3acd8789d`
(`DDS集成D2阶段_20260826_2301`). No commit or push was made. The unrelated
untracked future Control/Init architecture plan was preserved and no DDS
Control, Init, Realtime, InitAck, annotation, metadata, custom header, or IDL
was added.

### Code and automation changes

- `DdsVideoPublisher::endRound` now emits explicit application queue-drain,
  ACK-call, bounded-drain, and total-drain timings. The existing bounded drain
  remains; ACK success is not treated as endpoint delivery proof.
- The customer Receiver Demo adds test-only `--sample-delay-ms` and
  `--sample-delay-samples`; both default to zero. The callback copies/owns the
  Sample before the artificial delay.
- `tools/dds_d3_acceptance.ps1` records baseline/build/deploy/evidence gates,
  reuses `codex_rk3588_pipeline.ps1` for the established Windows-to-VM-to-board
  path, parses exact sender/receiver counts, and summarizes `/proc`/NIC resource
  TSV files. It never embeds passwords or licence contents.
- Customer field debug, field checklist, performance report, ICD, CLion guide,
  and Receiver README were updated.

### Production reliability results

- precise DDS H264 only: 689/689.
- precise TCP + DDS H264: 716/716, TCP Packet v3 decoded concurrently.
- precise TCP + DDS H264 + local MP4: 684/684; MP4 684/684, zero drop.
- Sync H264: 623/623; Async DDS output overwrite remained zero.
- Multi-reader: customer Demo received 492/492 while VideoDisplay received and
  decoded the same Writer continuously.
- Twenty START/STOP rounds: 20/20 exact sender/receiver count matches.
- RawGray8: 622/622 at exactly 640,000 bytes/Sample, 12.878 Samples/s.
- RawBGR24: 618/618 at target 30 and 640/640 at target 45, exactly 1,920,000
  bytes/Sample. Maximum observed reliable rate was 28.180 Samples/s.
- Dual DDS H264: precise 377/377 and coarse 372/372 after using a 25-second
  receiver idle window.
- Dual maximum load (TCP + DDS + record): precise 261/261 and coarse 222/222;
  both local MP4 streams had inputFrames equal to writtenFrames and zero drop.

All normal-consumer cases used tcpv4 + RELIABLE + KEEP_ALL with zero writer,
reader, and application-drop errors. H264 remained one encode per frame and
the recorder backend remained `shared_h264_remux`.

### Performance and retained failure

The production renderer baseline was below 59 FPS and varied materially across
the A/B sequence. DDS H264 write average remained about 0.09--0.16 ms and the
application queue maximum was one, but D3 does not claim a precise `<1 FPS`
transport delta from non-comparable renderer intervals. RawGray8 was reliable
at 12.878 Samples/s in this production scene; RawBGR24's maximum observed
reliable rate was 28.180 Samples/s. No frame dropping, resolution reduction, or
quality reduction was used to raise those numbers.

A deliberately severe slow callback test (`1000 ms` for the first 50 Samples)
ended with 749 writer Samples versus 328 reader Samples inside the acceptance
window, even though the writer reported zero error/drop and ACK returned
quickly. This is retained as a **FAIL** for the D3 slow-reader backpressure gate
and as a vendor/runtime issue. A lighter 100 ms/sample test drained 436/436.

### Local recording and resources

The 60-second wall-clock soak produced 684/684 shared-H264-remux frames, an
800x800 H264 MP4 at 60/1 with 11.4 seconds of media time, and passed ffprobe plus
full decode. Five further START/STOP rounds produced five independent valid MP4
files, all with exact input/written counts and zero drops. After one-time
Panda/MPP initialization, thread/fd counts stabilized; the apparent initial fd
increase was traced to MPP dmabufs/devices, loaded model files and existing
sockets, not a per-round recorder leak.

Per-second resource monitoring captured CPU, VmRSS/VmHWM, Threads, fd count,
`/proc/<pid>/io`, and NIC Tx/Rx. Normal DDS application queues did not grow
without bound. Detailed measurements and the exact PASS/FAIL table are in
`docs/DDS_Video_D3_Performance_Report.md`; customer commands and fault isolation
are in `docs/DDS_Customer_Video_Debug_Guide.md` and
`docs/DDS_Field_Debug_Checklist.md`.

### Known vendor issues after D3

1. SDK path label 2.4.5 versus runtime banner `2.4.4-r6873577`.
2. `wait_for_acknowledgments()` does not prove that the receiving application
   has drained the final Samples.
3. CAEP Trial runtime modifies a writable licence copy.
4. Severe callback blocking did not produce observable application-writer
   backpressure before a large middleware tail accumulated; vendor guidance is
   required.

## 17. D3.1 manual-start black-screen closure (2026-08-28)

D3.1 was performed on the preserved local D3 checkout at HEAD
`881441087826c7dbfab6c9281d524c862a7adf2a`. The pre-change status, empty diff,
and untracked-file hashes are retained under
`logs/dds-d31-20260828-012343/`; no reset, checkout, clean, commit, or push was
used.

The reported symptoms were separated into independent faults instead of being
classified as one DDS black screen:

1. a stale `HwaSim_IR` could own UDP `192.168.1.116:8888`; the new process used
   to continue rendering and publishing after bind failure;
2. the simplified manual environment selected Mesa llvmpipe instead of the
   board Xorg/Mali stack and produced GL 0x502 errors;
3. the deployment path omitted `Config/DDS/ZRDDS_QOS_PROFILES.xml`;
4. the Windows host has multiple NICs, so discovery needed an isolated
   default-versus-bound QoS test;
5. the former GUI evidence proved decode return status but did not prove that
   decoded pixels were non-black.

The original historical PID was no longer present when D3.1 began and cannot
be reconstructed honestly. There was no systemd/init auto-restart service. A
controlled reproduction recorded PID 3331 as the sole `HwaSim_IR` owner of UDP
8888, then proved that a second process exits with code 3 and logs
`[StartupFatal] component=UDP ... reason=bind_failed`. Missing DDS QoS now exits
with code 4 after logging the requested and resolved absolute path. The
validated launcher also refuses Xorg absence, missing deployment assets, and an
occupied UDP port before launching.

The restored board GPU line was:

```text
[GpuBackend] presentationMode=HeadlessOffscreen graphicsPipe=OpenGL ES gsgType=eglGraphicsStateGuardian glVendor=ARM glRenderer=Mali-LODX glVersion=OpenGL ES 3.2 v1.g6p0-01eac0.efb75e2978d783a80fe78be1bfb0efc1 hardwareGpu=1
```

No llvmpipe line or GL 0x502 error occurred in the corrected environment. The
new `run_precise.sh` exports the validated Panda3D, Xorg, ZRDDS, Mali and loader
paths and performs the deployment/UDP/Xorg preflight.

### Network, payload, GUI, and regression evidence

- Isolated bound QoS: board-to-Windows 200/200 and Windows-to-board 200/200.
- Isolated default QoS: board-to-Windows 200/200 and Windows-to-board 200/200.
  Therefore multi-NIC default discovery was not the reproduced root cause on
  this host. Manual/field tests nevertheless select Windows `192.168.1.188`
  and board `192.168.1.116` explicitly for deterministic routing.
- Real HwaSim_IR to customer Receiver: H264 575/575, zero errors/drop; ffprobe
  and decode passed at 800x800. A decoded scene frame had min 8, max 194.005,
  mean 80.076, stddev 2.828 and nonzero ratio 1.0.
- Real HwaSim_IR to VideoDisplay: H264 decode and pixel diagnostics passed; the
  user screenshot `videodisplay_gui_user_screenshot.png` proves a visible gray
  IR scene and target overlay. A stopped run intentionally retains its last
  frame.
- Two simultaneous Readers: writer 499, customer Receiver 499, VideoDisplay
  499, with zero writer, reader, or application errors/drop. Each Windows
  process used its own writable Trial-licence copy to avoid concurrent file
  mutation/locking.
- RawGray8 regression: 168/168 at exactly 640000 bytes/Sample, zero errors/drop.
- DDS H264 plus local MP4: 226/226 DDS; shared-H264 remux wrote 226/226 with zero
  drop. The 800x800, 60 FPS, 3.766667-second MP4 passed ffprobe and full decode.
- R1 routing, TCP Packet v3 H264, TCP reconnect, reset and IDR recovery all
  passed.

VideoDisplay now has test-only first-frame PNG/pixel diagnostics and a bounded
acceptance exit option. Production defaults are unchanged. Deployment copies
the DDS directory recursively and includes the IP-bound board QoS. The D3
remote path now fails on stale processes, UDP bind failure, missing QoS,
non-Mali GPU, GL 0x502, absent DDS Samples, decode failure, non-varying pixels,
or mismatched final counts. DDS remains video-only; no Control, Init, Realtime,
InitAck, annotation, metadata, custom header, or IDL was added.

After all functional runs, code review moved the diagnostic string
`socket_create_failed` from the Linux InitAck invalid-socket branch to the
actual `socket()` failure branch. The VM then became unreachable on
`192.168.203.128:22`, so this final diagnostic-only one-line delta was not
rebuilt/deployed. The validated board binary already contains and passed the
required UDP bind-fatal behavior; refresh this final source/binary parity when
the VM is reachable.
