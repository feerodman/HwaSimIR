# HwaSimIR DDS 视频客户现场调试指南

本文面向第一次接触 DDS 的现场工程师。当前交付中的 DDS **只传视频**；RESET、INIT、START、STOP、Realtime 和 InitAck 仍走原有 UDP。不要等待 DDS 上出现控制、初始化、实时数据或标注，也不要为这些数据创建 IDL。

## 1. 双方分别做什么

我方 HwaSim_IR：

- 从 FinalSensor 完成一次最终图像 readback；
- 根据初始化参数选择 H264 或 Raw；
- 通过 ZRDDS `DDS::Bytes` 发布一个完整视频 Sample；
- 保留原 TCP Packet v3 视频、annotation 和 realtime 链路；
- 可在板端把共享 H264 AU 复用为本地 MP4。

客户接收端：

- 安装可用的 ZRDDS 和 licence；
- 选择正确网卡、Domain、Topic、QoS、codec；
- Raw 模式额外配置 width、height 和像素格式；
- 一个 Sample 当作一整个 AU 或一整帧处理，不能解析 TCP Packet v3；
- 用发送/接收 Sample 数和错误数验收，不能只看“有画面”。

客户不需要 IDL，不需要 `CommonData.h`，也不需要链接 HwaSim_IR。最小依赖只有 ZRDDS SDK、QoS XML 和本接口约定。

## 2. 固定接口约定

- 默认 Domain：`150`
- 类型：`DDS::Bytes`
- 传输：`tcpv4`
- Writer/Reader：`RELIABLE_RELIABILITY_QOS`
- Writer history：`KEEP_ALL_HISTORY_QOS`
- 禁止：BEST_EFFORT、UDP 大包零拷贝、自定义视频头
- Windows 板卡直连 IP：`192.168.1.188`
- RK3588 IP：`192.168.1.116`
- 不应绑定：`192.168.123.100`、VMware 或其他无关网卡

Topic：

| 通道 | 格式 | Topic |
|---|---|---|
| precise | H264 | `HwaSimIR.Video.precise.H264` |
| coarse | H264 | `HwaSimIR.Video.coarse.H264` |
| precise | Gray8 | `HwaSimIR.Video.precise.RawGray8` |
| coarse | Gray8 | `HwaSimIR.Video.coarse.RawGray8` |
| precise | BGR24 | `HwaSimIR.Video.precise.RawBGR24` |
| coarse | BGR24 | `HwaSimIR.Video.coarse.RawBGR24` |

H264 的一个 Sample 是一个完整 Annex-B Access Unit，首字节就是原 H264 数据。RawGray8 的 Sample 大小严格为 `width*height`；RawBGR24 严格为 `width*height*3`，B、G、R 紧密排列且无行填充。

## 3. 上电后的最短检查顺序

1. Windows 执行 `echo %ZRDDS_HOME%`，确认指向 `F:\Programs\ZRDDS\ZRDDS-2.4.5`。
2. 确认 SDK 根目录 licence 副本存在且可写。不要把 licence 内容或 Signature 发到群里。
3. `ping 192.168.1.116`，再用 `ipconfig` 确认 `192.168.1.188` 位于板卡直连网卡。
4. 临时放行 Windows 防火墙，或为接收程序和 ZRDDS tcpv4 建立入站/出站规则。
5. 确认双方 Domain 都是 150，Topic 字符逐个一致，大小写一致。
6. 先运行客户 Receiver Demo，看到 `receiverReady=1` 后再启动/START HwaSim_IR。
7. STOP 后等待 Receiver 的 idle-exit，再比较 `sentSamples` 与 `receivedSamples`。

## 4. Windows 客户 Receiver 完整命令

以下命令在 `DDS\HwaSimIRVideoReceiverDemo\x64\Release` 或包含对应 exe/DLL/licence 的可写运行目录执行。QoS 可使用随 Demo 提供的 `Config\ZRDDS_QOS_PROFILES.xml`；多网卡现场应改用明确绑定 `192.168.1.188` 的测试配置。

### precise H264

```powershell
$env:ZRDDS_HOME='F:\Programs\ZRDDS\ZRDDS-2.4.5'
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.H264 `
  --codec h264 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received_precise.h264 `
  --frames 0 `
  --idle-exit-ms 10000
```

### coarse H264

```powershell
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.coarse.H264 `
  --codec h264 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received_coarse.h264 `
  --frames 0 `
  --idle-exit-ms 10000
```

### precise RawGray8（800x800）

```powershell
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawGray8 `
  --codec raw_gray8 `
  --width 800 --height 800 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received_precise.gray8 `
  --frames 0 `
  --idle-exit-ms 10000
```

每个 Sample 必须恰好 `640000` 字节。

### precise RawBGR24（800x800）

```powershell
.\HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawBGR24 `
  --codec raw_bgr24 `
  --width 800 --height 800 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received_precise.bgr24 `
  --frames 0 `
  --idle-exit-ms 10000
```

每个 Sample 必须恰好 `1920000` 字节。coarse 只需把 Topic 中 `precise` 改为 `coarse`。

## 5. VideoDisplay DDS 模式

可通过 `NetworkConfig.ini` 设置 `[VideoInput] Transport=dds`，也可使用 CLI：

```powershell
.\HwaSim_IR_VideoDisplay.exe `
  --receive-transport dds `
  --dds-topic HwaSimIR.Video.precise.H264 `
  --dds-codec h264 `
  --dds-width 800 --dds-height 800 --dds-fps 60 `
  --dds-qos Config\DDS\ZRDDS_QOS_PROFILES.xml
```

DDS 模式日志中的 `packetVersion=0`、`hasRealtimeData=0`、`hasAnnotation=0` 是正常状态，界面可显示 `DDS video-only`。H264 Reader 中途加入后可能先等待下一个 IDR；当前 GOP 为 30，Writer/新回合首帧会请求 IDR。

## 6. 板端启动环境

板端不联网安装软件。启动前：

```bash
export ZRDDS_HOME=/usr/ZRDDS/ZRDDS-2.4.5
export LD_LIBRARY_PATH=$ZRDDS_HOME/lib:$LD_LIBRARY_PATH
cd /userdata/HwaSimIR
./HwaSim_IR --channel precise --network-config Config/NetworkConfig.ini
```

不要永久 `chdir` 到 ZRDDS 安装目录，否则 HwaSim_IR 的 Config、Shader、Model、Texture 和 Weather 相对资产会失效。Trial licence 运行时会写 licence，实际使用的副本必须可写。

## 7. 数字验收

发送端检查最后一条 `[DdsVideoPerf]`：

```text
sentSamples=N
writeErrors=0
droppedSamples=0
queueDepth=0
```

接收端检查最终汇总：

```text
receivedSamples=N
ddsErrors=0
timedOut=0
```

必须满足 `sentSamples == receivedSamples`。H264 可对发送 audit elementary stream 与接收 `.h264` 做 SHA256；Raw 可在相同固定窗口比较源和接收文件 SHA256。audit 只在验收时启用，生产默认关闭。

## 8. 慢消费者和 STOP 注意事项

客户 Demo 的测试参数 `--sample-delay-ms N` 可模拟慢处理；`--sample-delay-samples M` 表示只延迟前 M 个 Sample，0 表示全部延迟。正常客户模式必须保持默认 0。

可靠零丢帧意味着消费者长期慢于生产者时必须发生背压；不可能同时无限维持 60 FPS、无限缓存且永不丢帧。当前 Trial runtime 的 `wait_for_acknowledgments()` 不能单独证明尾帧到达，必须保留 bounded drain，并以端点计数为准。现场 STOP 后不要立即杀接收器。

## 9. 常见故障表

| 现象 | 排查步骤 |
|---|---|
| `DDSIF::Init` failure | 检查 `ZRDDS_HOME`、SDK 根目录 licence 是否存在/可写、DLL 搜索路径；从 HwaSim_IR 正常工作目录运行，禁止打印 licence 正文。 |
| 无法 discovery | 确认双方 tcpv4、Domain、网段和防火墙；核对测试 QoS 是否绑定 192.168.1.188/192.168.1.116。 |
| `receivedSamples=0` | 先确认 `receiverReady=1`，再核对 HwaSim_IR `[DdsVideo] Enable=true`、初始化已完成、START 已发送。 |
| Topic 错误 | 字符串和大小写必须完全一致；precise/coarse、H264/Raw 不能混用。 |
| Domain 错误 | 双方统一为 150，排除其他测试进程使用了不同 Domain。 |
| 网卡错误 | Windows 必须走 192.168.1.188，不得选 192.168.123.100 或 VMware；必要时使用测试专用绑定 QoS。 |
| Windows 防火墙 | 临时隔离验证后，为程序/tcpv4 建立明确规则，不建议长期关闭防火墙。 |
| Raw size mismatch | Gray8 检查 `800*800=640000`；BGR24 检查 `800*800*3=1920000`；确认 codec、width、height。 |
| H264 一直等 IDR | 先启动 Reader 再 START；确认新回合/Writer 创建触发 IDR，检查 Annex-B SPS/PPS/IDR。 |
| 最后几帧缺失 | 增大 Receiver `--idle-exit-ms`，保留 Sender bounded drain；同时记录 sent/received，不以 ACK 返回值单独判定。 |
| sent/received 不一致 | 保存双方完整 stdout/stderr；检查 writer/reader error、进程是否被提前杀死、慢 callback 是否阻塞中间件。禁止补发/重复最后一帧伪造一致。 |
| licence read-only | 复制 licence 到实际可写运行副本；只记录路径、哈希和返回状态，不记录正文或 Signature。 |

## 10. 代码定位（内部联调）

- `HwaSimIR.cpp`：FinalSensor capture/readback。
- `TcpCommThread.cpp`、`TcpCommThread_Linux.cpp`：Video Output Worker、`sendFrameThreadFunc`、TCP/DDS/MP4 分发、`resolvedDdsCodec`、`resolvedDdsTopic`。
- `Video/VideoEncoder.h`：公共编码器接口。
- `Video/H264FfmpegEncoder.cpp`：Windows H264。
- `Video/H264MppEncoder.cpp`：RK3588 H264。
- `Video/JpegFrameEncoder.cpp`：JPEG。
- `Video/DdsVideoPublisher.cpp`：DDS Writer、no-drop queue、drain 和统计。
- `Video/LocalMp4Recorder.cpp`：板端/Windows 本地 MP4。
- `HwaSim_IR_VideoDisplay/DdsVideoReceiverWorker.cpp`：DDS Reader。
- `HwaSim_IR_VideoDisplay/Video/H264FfmpegDecoder.cpp`：H264 解码。
- `HwaSim_IR_VideoDisplay/TcpServerWorker.cpp`：legacy TCP Reader。
- `DDS/HwaSimIRVideoReceiverDemo/main.cpp`：客户最小接收器。

## 11. 当前已知厂商问题

1. SDK 安装目录标称 2.4.5，但实际 runtime banner 为 `2.4.4-r6873577`。
2. 当前 runtime 的 `wait_for_acknowledgments()` 可能在接收端尚未完全 drain 时立即返回。
3. CAEP Trial licence 在运行时会修改 licence 副本，因此副本必须可写，多进程应各用独立运行副本。

发现上述问题时保留原始命令、stdout/stderr 和双方计数，提交厂商确认；不要自行替换 SDK。

## 12. D3.1 现场手工启动（已验证）

### Windows：VideoDisplay

在 PowerShell 中执行：

```powershell
cd D:\HwaSimIR\HwaSim_IR_VideoDisplay\x64\Release
$env:ZRDDS_HOME='F:\Programs\ZRDDS\ZRDDS-2.4.5'
.\HwaSim_IR_VideoDisplay.exe `
  --receive-transport dds `
  --dds-topic HwaSimIR.Video.precise.H264 `
  --dds-codec h264 `
  --dds-width 800 `
  --dds-height 800 `
  --dds-fps 60 `
  --dds-qos D:\HwaSimIR\tools\dds_d1_qos\ZRDDS_QOS_WINDOWS_192.168.1.188.xml
```

看到 `[DdsVideoReceiver] ready=1` 后，再启动板端和 DataDrivenTestQT。运行中
应连续出现 `[DdsVideoReceiverSample]`、`[H264DecodeSuccess]` 和
`[DdsFrameDiag]`。若 DataDrivenTestQT 已发送 STOP，界面保留最后一帧并停止
变化是正常行为。

DDS 模式左侧初始化/实时数据栏为空也是当前设计：DDS 只传视频，INIT、START、
STOP、Realtime 和 InitAck 仍走原 UDP。本阶段不要把它当 DDS 黑屏。

### RK3588：precise

正式部署后只需使用启动脚本，不要手工省略 Xorg/Mali 环境：

```bash
cd /userdata/HwaSimIR
export HwaSimIRDdsVideoEnable=true
export HwaSimIRDdsVideoCodec=auto
export HwaSimIRDdsVideoQosFile=Config/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml
export HwaSimIRLocalRecordingEnable=false
export TcpSendVideo=false
export RenderPresentationMode=HeadlessOffscreen
./run_precise.sh
```

启动后硬检查：

```text
[DdsVideoConfig] ... exists=1
[TcpPayloadConfigSource] SendVideo=0 source=env:TcpSendVideo
[GpuBackend] ... glVendor=ARM glRenderer=Mali-LODX ... hardwareGpu=1
```

若看到 `llvmpipe`、`hardwareGpu=0`、`Address already in use` 或
`[StartupFatal]`，立即停止测试，不能继续把后续 Sample 数当成有效验收。

### 多个 Windows DDS 进程

CAEP Trial runtime 会修改 licence。VideoDisplay 与客户 Receiver Demo 同时运行
时，每个工作目录必须有各自独立、可写的 `zrddslicence.lic` 副本；不要让多个
进程并发写同一个 SDK 根目录 licence。只记录副本是否存在、是否可写和 SHA256，
禁止输出 licence 正文或 Signature。

### 黑屏快速分层

1. `ss -lunp | grep ':8888'`：确认只有预期 HwaSim_IR 占用 UDP。
2. 检查 ARM/Mali/hardwareGpu=1；llvmpipe 不是生产验收环境。
3. 检查双方 bound QoS：Windows 192.168.1.188，板端 192.168.1.116。
4. 检查 Writer `sentSamples` 与 Reader `receivedSamples`。
5. 检查 H264 解码与 `[DdsFrameDiag] max > min`、`stddev > 0`。
6. 测试时可加 `--dds-dump-first-frame D:\logs\dds_first.png` 保存首帧证据。
