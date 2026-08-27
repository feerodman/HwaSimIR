# 1. D2 现在能不能直接 DDS 发送不压缩视频？

**能，而且已经是正式代码，不只是 Demo。**

当前逻辑在：

```
HwaSim_IR/HwaSim_IR/TcpCommThread.cpp
HwaSim_IR/HwaSim_IR/TcpCommThread_Linux.cpp
```

核心函数：

```
TcpCommThread::resolvedDdsCodec()
TcpCommThread::resolvedDdsTopic()
TcpCommThread::sendFrameThreadFunc()
```

当前 `resolvedDdsCodec()` 的代码明确规定：

```
DdsVideo.Codec=auto

trackerSensorParam.h264En=true
    -> h264

trackerSensorParam.h264En=false
    -> RawPixelFormat

RawPixelFormat=gray8
    -> raw_gray8

RawPixelFormat=bgr24
    -> raw_bgr24
```

也可以直接显式配置 `Codec=raw_gray8` 或 `Codec=raw_bgr24`。

发送链实际是：

```
Final Sensor
    ↓
唯一一次 readback
    ↓
PendingFrame.pixels（BGR24）
    ↓
sendFrameThreadFunc()
    ├── raw_gray8：BGR → Gray8
    └── raw_bgr24：直接整理方向
            ↓
DdsVideoPublisher::publishBytes()
            ↓
DDS::Bytes
```

没有：

```
Raw → H264 → 解码 → Raw → DDS
```

这种绕路。

`DdsVideoPublisher` 的职责也非常纯粹：传进去多少字节，就把这些字节作为一个 `DDS::Bytes` Sample 排队并可靠发布，不做编码、不加 header。

D2 已实测：

| 模式                 | 结果                      |
| -------------------- | ------------------------- |
| Windows RawGray8     | 61/61，640,000 B/Sample   |
| Windows RawBGR24     | 10/10，1,920,000 B/Sample |
| RK3588 RawGray8      | 10/10，640,000 B/Sample   |
| DDS dropped          | 0                         |
| sender/receiver Hash | 一致                      |

所以这个能力已经闭环。

例如当前要发 **800×800 Gray8 未压缩图像**：

```
[DdsVideo]
Enable=true
Codec=auto
RawPixelFormat=gray8
DomainId=150
```

初始化：

```
trackerSensorParam.h264En = false;
trackerSensorParam.trackerSensorWidth = 800;
trackerSensorParam.trackerSensorHeight = 800;
```

实际 Topic 就是：

```
HwaSimIR.Video.precise.RawGray8
```

每 Sample：

```
800 × 800 = 640000 bytes
```

如果：

```
RawPixelFormat=bgr24
```

则：

```
HwaSimIR.Video.precise.RawBGR24
```

每 Sample：

```
800 × 800 × 3 = 1,920,000 bytes
```

------

# 2. 你明天和客户调 DDS，应该怎么调

这里建议你不要一上来就解释 DDS 原理。你只需要跟客户先统一 **5 个东西**：

```
1. ZRDDS 版本
2. Domain ID
3. Topic
4. QoS
5. 视频格式
```

当前你们约定：

```
ZRDDS 安装包：2.4.5
实际 runtime banner：2.4.4-r6873577

Domain = 150

Transport = tcpv4

Reliability = RELIABLE

History = KEEP_ALL
```

当前正式视频 ICD 已经明确这些约定，而且视频 Sample 内不含 frameSeq、PTS、宽高、codec、platID/sensorID、TCP 头、标注或实时数据。

## 明天现场最稳妥的调试顺序

### 第一步：先不要启动 HwaSim_IR

先让客户确认：

```
ZRDDS_HOME
zrddslicence.lic
ZRDDSCpp 库
```

Windows 客户端如果也是同一套 SDK，应该类似：

```
echo $env:ZRDDS_HOME
```

然后确认客户 PC 和板卡：

```
客户PC   <CUSTOMER_IP>
RK3588   192.168.1.116
```

能互相 ping。

如果客户 PC 多网卡，尤其有：

```
Wi-Fi
VMware
办公网
板卡直连网卡
```

一定确认 ZRDDS 用的是**和 192.168.1.116 同网段的网卡**。

发现不了对方时，第一优先怀疑：

```
网卡选错
Domain 不同
Topic 不同
防火墙
QoS
licence
```

而不是先改 HwaSimIR。

------

### 第二步：先用客户 Demo，不启动客户正式软件

你已经有：

```
DDS/HwaSimIRVideoReceiverDemo/
```

这是目前最适合现场定位问题的工具。

客户不需要：

```
CommonData.h
IDL
Panda3D
HwaSimIR 源码
```

只需要 ZRDDS。

先启动客户 Receiver，再启动 HwaSim_IR。

H264：

```
HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.H264 `
  --codec h264 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received.h264 `
  --frames 0 `
  --idle-exit-ms 2000 `
  --timeout-sec 90
```

这个 Demo 是当前 D2 正式提供的客户用法。

Raw Gray8：

```
HwaSimIRVideoReceiverDemo.exe `
  --domain 150 `
  --topic HwaSimIR.Video.precise.RawGray8 `
  --codec raw_gray8 `
  --width 800 `
  --height 800 `
  --qos Config\ZRDDS_QOS_PROFILES.xml `
  --output received.gray8 `
  --frames 0 `
  --idle-exit-ms 2000 `
  --timeout-sec 90
```

------

### 第三步：再启动 HwaSim_IR

目前 **D2 只有视频走 DDS**。

这一点明天必须跟客户讲清楚：

> 当前版本初始化、开始/停止/复位、60 Hz 实时激励、InitAck 仍然走原 UDP；DDS 当前只负责视频。

也就是说当前真实流程还是：

```
客户/DataDrivenTestQT
       │
       │ UDP
       ├── RESET
       ├── INIT
       ├── START
       └── 60Hz Realtime
                 │
                 ▼
             HwaSim_IR
                 │
                 ├── TCP（保留）
                 │
                 └── DDS Video
                         │
                         ▼
                       客户
```

不要让客户明天误以为：

```
“我只启动 DDS Receiver，
为什么 HwaSimIR 不自动开始出视频？”
```

因为 HwaSim_IR 还是需要 UDP 初始化和 START。

------

### 第四步：H264 先调通

HwaSim_IR：

```
[DdsVideo]
Enable=true
Codec=auto
DomainId=150
QosFile=Config/DDS/ZRDDS_QOS_PROFILES.xml
```

初始化：

```
h264En=true
```

客户订阅：

```
HwaSimIR.Video.precise.H264
```

然后你看 HwaSimIR：

```
[DdsVideo]
[DdsVideoPerf]
```

客户看：

```
receivedSamples
receivedBytes
ddsErrors
```

最后要求：

```
sender sentSamples
==
customer receivedSamples

droppedSamples=0
writeErrors=0
ddsErrors=0
```

------

### 第五步：再试不压缩 Gray8

初始化改：

```
h264En=false
```

同时：

```
RawPixelFormat=gray8
```

客户 Topic 改成：

```
HwaSimIR.Video.precise.RawGray8
```

800×800 必须严格：

```
640000 bytes/Sample
```

如果客户报：

```
raw size mismatch
```

优先检查：

```
width
height
Topic
codec
```

------

### 第六步：最后才让客户换成他们自己的程序

客户自己的代码逻辑，本质和：

```
DDS/HwaSimIRVideoReceiverDemo/main.cpp
```

一样。

所以明天最有用的一句话是：

> “先保证你们的软件和我们的 HwaSimIRVideoReceiverDemo 对相同 Topic 的接收结果一致；Demo 能收而客户软件收不到，问题在客户 DDS 接收端；Demo 也收不到，再查双方 ZRDDS/QoS/网络。”

这会非常省时间。

------

# 当前所有视频输出代码在哪里

你可以把这张表保存下来：

| 功能                          | 代码位置                                          |
| ----------------------------- | ------------------------------------------------- |
| 最终传感器图像产生/readback   | `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`                |
| DDS/TCP/录像配置加载          | `HwaSimIR.cpp / HwaSimIR.h`                       |
| 视频总输出后台线程            | `TcpCommThread.cpp`、`TcpCommThread_Linux.cpp`    |
| DDS codec 决策                | `TcpCommThread::*resolvedDdsCodec()`              |
| DDS Topic 决策                | `TcpCommThread::*resolvedDdsTopic()`              |
| TCP/DDS/MP4 一帧共享          | `TcpCommThread::*sendFrameThreadFunc()`           |
| DDS Publisher                 | `Video/DdsVideoPublisher.h/.cpp`                  |
| H264 公共接口                 | `Video/VideoEncoder.h`                            |
| Windows H264                  | `Video/H264FfmpegEncoder.cpp`                     |
| RK3588 H264                   | `Video/H264MppEncoder.cpp`                        |
| JPEG                          | `Video/JpegFrameEncoder.cpp`                      |
| 本地 MP4                      | `Video/LocalMp4Recorder.h/.cpp`                   |
| DDS 参数                      | `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`        |
| DDS QoS                       | `HwaSim_IR/Bin/Config/DDS/ZRDDS_QOS_PROFILES.xml` |
| VideoDisplay DDS Receiver     | `DdsVideoReceiverWorker.h/.cpp`                   |
| VideoDisplay H264 解码        | `Video/H264FfmpegDecoder.cpp`                     |
| VideoDisplay TCP Receiver     | `TcpServerWorker.h/.cpp`                          |
| VideoDisplay DDS/TCP 模式选择 | `HwaSim_IR_VideoDisplay.cpp/.h`、`main.cpp`       |
| 客户最小 DDS Receiver         | `DDS/HwaSimIRVideoReceiverDemo/main.cpp`          |
| 客户说明                      | `DDS/HwaSimIRVideoReceiverDemo/README.md`         |
| 视频 DDS ICD                  | `docs/DDS_HwaSimIR_ICD.md`                        |

其中现在最核心的是：

```
TcpCommThread_Linux.cpp
    sendFrameThreadFunc()
```

它已经会决定：

```
tcpH264
ddsH264
ddsRaw
recording
needH264
needJpeg
```

然后确保同一帧 H264 最多编码一次。

DDS 发送器内部则是：

```
publishBytes
    ↓
no-drop deque
    ↓
worker
    ↓
DDSIF::BytesWrite
```

队列满时会背压；不会清旧帧。

VideoDisplay DDS 接收入口则非常直接：

```
DdsVideoReceiverWorker
     ↓
DDSIF::SubTopic
     ↓
DDS::Bytes
     ↓
processSample()
     ├── H264FfmpegDecoder
     ├── Gray8 QImage
     └── BGR24 -> RGB -> QImage
```