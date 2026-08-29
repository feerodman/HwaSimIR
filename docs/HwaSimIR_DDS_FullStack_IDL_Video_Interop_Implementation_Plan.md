# HwaSimIR DDS 全栈通信、IDL、VideoStatus 与视频互操作实施方案

> 当前仓库参考基线：`7383ef8653ab495b8a8cf071d8027b2a774f7a3f`（DDS集成dds_pub_sub_20260828_1809）  
> 目标：保留现有 TCP+UDP 完整通信链，同时新增一套可独立运行的 ZRDDS 完整通信链；HwaSim_IR、HwaSim_IR_VideoDisplay、DataDrivenTestQT 同步支持两套传输。  
> ZRDDS：Windows VS2015 x64、Windows MinGW 7.3.0 x64、Linux/AArch64 三套 SDK。  
> 视频主协议继续优先使用 `DDS::Bytes`；控制/初始化/实时激励/应答/视频状态使用 IDL 强类型 Topic。

## 1. 两套完整通信

### Legacy：TCP + UDP

```text
DataDrivenTestQT / 客户激励
   | UDP 0x41/0x36/0x38
   v
HwaSim_IR
   | UDP 0x37 InitAck
   +------------------> 激励端
   |
   | TCP Packet v3: video + annotation + realtime + init/control forward
   v
HwaSim_IR_VideoDisplay
```

旧 UDP struct 和 TCP Packet v3 wire format 均不改，必须可完全脱离 DDS 独立工作。

### DDS：完整 ZRDDS 链

```text
DataDrivenTestQT / 客户激励
  +--> HwaSimIR.Control  / ControlCommandV1
  +--> HwaSimIR.Init     / InitCommandV1
  `--> HwaSimIR.Realtime / RealtimeDataV1
             |                  |
             v                  v
         HwaSim_IR      HwaSim_IR_VideoDisplay

HwaSim_IR
  +--> HwaSimIR.InitAck     / InitAckV1
  +--> HwaSimIR.VideoStatus / VideoStatusV1
  `--> HwaSimIR.Video.*     / DDS::Bytes
```

DDS 的 fan-out 让 HwaSim_IR 和 VideoDisplay 同时订阅 Control/Init/Realtime，不需要 HwaSim_IR 再转发一份 DDS 控制数据。

## 2. 两个 Transport，一套业务状态机

禁止复制业务逻辑。

```text
UdpCommThread -------------------+
                                 |
DdsProtocolReceiver -> Adapter --+--> ProtocolIngress
                                        |
                                        +--> handleControlCmd()
                                        +--> handleInitCmd()
                                        `--> handleDisplayData()
```

RESET / INIT / START / Realtime / STOP 无论从 UDP 还是 DDS 来，都进入同一状态机。STOP 后必须保持现有语义：停止生成新视频/标注、停止本地录像并 flush/close；VideoDisplay 可冻结最后一帧。

## 3. DDS Runtime 共享

加入业务 Topic 后必须将当前视频 Publisher 自己的 DDS 生命周期抽成：

```text
DdsRuntimeManager
  +-- DDSIF::Init() once
  +-- tcpv4 DomainParticipant
  +-- Control Reader
  +-- Init Reader
  +-- Realtime Reader
  +-- InitAck Writer
  +-- VideoStatus Writer
  +-- Video Bytes Writer
  `-- DDSIF::Finalize() once
```

每个进程只初始化一次 DDS。HwaSim_IR、VideoDisplay、DataDrivenTestQT 是不同进程，各自有自己的 RuntimeManager。

## 4. Topic

建议 DomainId=150。

```text
HwaSimIR.Control
HwaSimIR.Init
HwaSimIR.Realtime
HwaSimIR.InitAck
HwaSimIR.VideoStatus

HwaSimIR.Video.precise.H264
HwaSimIR.Video.precise.RawGray8
HwaSimIR.Video.precise.RawBGR24
HwaSimIR.Video.coarse.H264
HwaSimIR.Video.coarse.RawGray8
HwaSimIR.Video.coarse.RawBGR24
```

## 5. IDL V1

新建：

```text
DDS/IDL/HwaSimIRProtocolV1.idl
```

推荐：

```idl
module HwaSimIRDds
{
    struct SpatialStateV1
    {
        double lat;
        double lon;
        double alt;
        double yaw;
        double pitch;
        double roll;
        double speed;
    };

    struct TrackerSensorParamV1
    {
        boolean h264En;
        boolean noiseEn;
        double trackerSensorNoise;
        boolean realtimeAnnotation;
        boolean saveMP4En;

        long trackerSensorBand;
        long trackerSensorWidth;
        long trackerSensorHeight;
        long trackerSensorViewMin;
        long trackerSensorViewMax;
        double trackerSensorPixelAngle;

        double trackerX;
        double trackerY;
        double trackerZ;
        double trackerPitch;
        double trackerYaw;
        double trackerRoll;

        double illuminatorX;
        double illuminatorY;
        double illuminatorZ;
        double illuminatorPitch;
        double illuminatorYaw;
        double illuminatorRoll;

        double illuminatorAngle;
        double illuminatorSpotRad;

        long emitterSpotRadius;
        double emitterSpotRad;
    };

    struct InitObjectTrackingParamV1
    {
        boolean enable;
        long envTerrain;
        long envSky;

        double envMaxHeightRain;
        double envTransHeightRain;
        double envMaxHeightSnow;
        double envTransHeightSnow;
        double envRainSnowSpeedScale;

        double envRadScaleTerrain;
        double envRadScaleSky;
        double envTemp;
        double envHumidity;
        double envVisibility;
        double envWindV;
        double envWindDir;

        long simMode;
        long videoFps;

        TrackerSensorParamV1 trackerSensor[1];
    };

    struct PlatParamPakV1
    {
        long id;
        long type;
        SpatialStateV1 spatial;
    };

    struct WeaponStateV1
    {
        long targetType;
        long targetPlatID;
        long targetID;
        double xxOutAng[2];
        boolean lookatEn;
        boolean illuminatorEn;
        double offsetAng[2];
        boolean viewValid;
        long damageFlag;
        boolean strikeFlag;
        long strikePart;
    };

    struct TargetStateV1
    {
        long targetType;
        long targetPlatID;
        long targetID;
        boolean engineState;
        boolean viewValid;
        SpatialStateV1 targetLoc;
        long targetState;
    };

    struct ControlCommandV1
    {
        long flag;
        long JB;
        long platID; //@key
        long simCommand;
        long roundCut;
        long currentRound;
    };

    struct InitCommandV1
    {
        long flag;
        long JB;
        long platID;   //@key
        long sensorID; //@key

        PlatParamPakV1 platParamInit;
        InitObjectTrackingParamV1 trackingInit;

        long MissileMaxCount120;
        long MissileMaxCount9;
        long MissileMaxCountMMD;
        long MissileMaxCountF35;
        long MissileMaxCountF22;
        long MissileMaxCountResv1;
        long MissileMaxCountResv2;
    };

    struct RealtimeDataV1
    {
        long flag;
        long platID;   //@key
        long sensorID; //@key
        double time;

        SpatialStateV1 platLoc;
        WeaponStateV1 weaponState;

        long targetNumValid;
        TargetStateV1 targetState[5];
    };

    struct InitAckV1
    {
        long flag;
        long JB;
        long platID;   //@key
        long sensorID; //@key
        boolean trackingReady;
    };

    struct VideoStatusV1
    {
        long platID;   //@key
        long sensorID; //@key
        string<16> channel;

        boolean running;

        string<24> codec;
        string<24> pixelFormat;
        string<128> videoTopic;

        long width;
        long height;
        long fps;

        long bitrateKbps;
        long gopFrames;

        boolean compressed;
        long currentRound;
    };
};
```

IDL `long` 固定按 DDS 32-bit 语义使用；C++ `bool` 与 IDL `boolean` 必须逐字段转换，禁止 memcpy。

## 6. Adapter

新增：

```text
DDS/Protocol/CommonDataDdsAdapter.h/.cpp
```

将四类现有 BYHWICD struct 与四类 IDL type 双向逐字段转换。

必须有 deterministic semantic round-trip test：

```text
CommonData -> DDS type -> CommonData
```

所有字段一致。

## 7. VideoStatus 必须现在加入

H.264 真实解码尺寸可由 SPS/decoder 得到，所以 H.264 解码本身不依赖 Status；但整个系统仍应增加 VideoStatus，因为：

- Raw 没有 SPS，必须知道 width/height/pixelFormat；
- 接收端不应再手工填 `--dds-width/height/fps/codec`；
- 可自动知道实际 Topic；
- 可知道 precise/coarse；
- 可知道 START/STOP；
- 可纠正 GUI expected geometry；
- 客户与中间 Gateway 可自动配置。

发布时机：

```text
DDS ready
INIT accepted
START
STOP
codec/topic/resolution/fps changed
低频 1 Hz 状态重发
```

VideoDisplay H264 模式：

```text
decoded width/height = 真实尺寸
VideoStatus width/height = expected config
```

若不一致打印 warning。GUI 按实际 QImage 尺寸保持宽高比。

Raw 模式则严格以 Status geometry 校验 Sample size。

## 8. 当前 50 FPS / 分辨率

VideoStatus 能解决配置和分辨率同步，但不能提高 FPS。

必须分开统计：

```text
sourceOutputFps
encodedFps
ddsAcceptedFps
ddsWriteFps
receiverSampleFps
decodeFps
guiPresentFps
```

新增：

```text
[VideoGeometry]
statusWidth=
statusHeight=
decodedWidth=
decodedHeight=
labelWidth=
labelHeight=
aspectPreserved=
```

## 9. 当前视频方案

H264：

```text
1 Annex-B AU = 1 DDS::Bytes Sample
```

Raw：

```text
1 complete frame = 1 DDS::Bytes Sample
```

优点是 AU/frame 边界天然保留、Sample 数少、客户代码简单。

缺点是 Raw 大 Sample，并且当前 `DDSIF::BytesWrite` 属于 ZRDDS 非零拷贝 Bytes API；HwaSimIR 自己的 queue 也有一次 payload copy。

## 10. 同事 dds_pub_sub 方案评价

同事当前 `ShapeType.idl`：

```idl
struct ShapeType
{
    long x; //@key
    long type;
    long sn;
    long cmd;
    long len;
    long crc;
    sequence<octet,32768> data;
};
```

当前测试链：

```text
A h264_pub
 -> H264 连续流分片
 -> 当前 H264_PACKET_MAX 实际为 1024 bytes
 -> 每包 write 后 sleep 1 ms

B dds_dec
 -> 订阅 H264 chunk
 -> MPP decode
 -> NV12 Y
 -> Gray
 -> 再按 32768 bytes 分片发布

C gray_sub
 -> 收 chunk
 -> 写 raw gray
```

它适合验证 DDS+MPP 网关，但不应直接冻结成客户生产协议：

1. H264 AU 边界被打散；
2. 当前 H264 pub/sub 使用 default QoS，和我们的 RELIABLE+KEEP_ALL 不是同条件；
3. Decode 极端重试存在丢包路径；
4. Raw 一帧被拆成多个 Sample，但协议没有完整 frameSeq/chunkIndex/chunkCount；
5. 客户端不能仅从当前 wire data 稳定恢复真实 frame boundary；
6. `crc` 尚未形成完整校验；
7. 1KB chunk 会制造大量 DDS Sample。

## 11. 推荐融合

直接客户：

```text
HwaSim_IR
 -> HwaSimIR.Video.* / DDS::Bytes
 -> 客户
```

同事 Gateway：

```text
HwaSim_IR
 -> HwaSimIR.Video.precise.H264 / DDS::Bytes, 1 AU/sample
 -> Gateway B
 -> MPP decode
 -> HwaSimIR.Decoded.precise.RawGray8 / DDS::Bytes, 1 frame/sample
 -> 客户
```

Gateway 同时发布 decoded VideoStatus。

不要让 HwaSim_IR 为同事额外维护一套 1KB ShapeType H264 协议，除非公平基准证明必须这样做。

## 12. 只有性能实测证明必要时才加 Chunk

如果 whole-frame Raw 明显受 ZRDDS 大 Sample 影响，再增加可选：

```idl
struct VideoChunkV1
{
    long platID;   //@key
    long sensorID; //@key
    unsigned long frameSeq;
    long chunkIndex;
    long chunkCount;
    long payloadBytes;
    sequence<octet,65536> data;
};
```

必须带 frameSeq/chunkIndex/chunkCount，不能依靠隐含连续分片。

默认 H264 仍不 chunk。

## 13. 为什么 DDS 看起来可能比 TCP 慢

当前 DDS 路径多出：

```text
output worker
 -> publishBytes
 -> app vector copy
 -> deque
 -> DDS worker
 -> DDSIF::BytesWrite
 -> serialization
 -> RELIABLE history
 -> ZRDDS TCP fragmentation/ACK
```

而普通 TCP socket send 更轻。

但 D3 数据已经显示 H264 DDS write 约 0.1 ms、RawGray8 约 1.2 ms；当时低 FPS 的主因是 renderer 105–127 ms。因此必须用脱离 renderer 的同条件 benchmark 再判断。

## 14. DDS 视频性能计时

发送端：

```text
captureMs
readbackMs
rawPrepMs
h264EncodeMs
ddsPublishEnqueueMs
ddsAppCopyMs
ddsAppQueueWaitMs
ddsWriteMs
ddsFrameTotalMs
ddsSamplesPerSec
ddsMiBPerSec
```

接收端：

```text
ddsCallbackFps
ddsCallbackBytesPerSec
ddsCallbackCopyMs
h264DecodeMs
guiPresentFps
```

## 15. 性能优化优先级

1. 去掉 HwaSimIR 自己不必要的 payload copy：
   - Raw 用 `publishOwned(std::vector&&)`；
   - H264 TCP/DDS/MP4 共用时用 shared ownership。
2. Raw buffer pool / 预分配。
3. 同 QoS 比较 WholeFrame 640KB vs Chunk64K vs Chunk32K。
4. 仅在官方支持且实测必要时研究 ZRDDS TCP concurrent transmission。
5. 不允许为了速度把正式链改成 BEST_EFFORT 或 silent drop。

## 16. 三套工具链

### HwaSim_IR_VideoDisplay

VS2015 x64 -> 必须链接 VS2015 ZRDDS。

### DataDrivenTestQT

Qt MinGW 7.3.0 -> 必须链接 MinGW7.3.0 ZRDDS，禁止链接 VS2015 C++ ZRDDS 库。

### RK3588 / 板端 Demo

使用板端 `/usr/ZRDDS/ZRDDS-2.4.5`，VM sysroot 来自板端实际 include/lib。

Windows 多版本不能共用一个模糊 `ZRDDS_HOME` 来决定编译库，建议构建参数明确：

```text
ZRDDS_VS2015_ROOT
ZRDDS_MINGW730_ROOT
ZRDDS_AARCH64_ROOT
```

运行时 licence 按 D3.1 已验证的“每进程独立可写副本”处理 Trial 并发写入。

## 17. zrddsgen

Canonical：

```text
DDS/IDL/HwaSimIRProtocolV1.idl
DDS/Generated/HwaSimIRProtocolV1/
```

新增：

```text
tools/dds_generate_protocol.ps1
```

用 ZRDDSGen 生成 C++ TypeSupport；generated 文件不手工修改。

同一 generated source 必须实编译：

```text
VS2015
MinGW7.3.0
AArch64
```

## 18. 客户激励 DDS Demo

新增一个共享源码树：

```text
DDS/HwaSimIRStimDdsDemo/
```

功能：

```text
RESET -> ControlCommandV1
INIT -> InitCommandV1
WAIT InitAckV1
START -> ControlCommandV1
60Hz RealtimeDataV1
STOP -> ControlCommandV1
```

交付三个构建：

```text
VS2015 x64 Release
MinGW 7.3.0 x64 Release
AArch64 Release
```

不是复制三套协议源码，而是一套源码 + 三套已验证构建产物/构建说明。

## 19. 客户板端 Receiver Demo

升级现有 receiver 或新增：

```text
DDS/HwaSimIRCustomerReceiverDemo/
```

支持：

```text
subscribe VideoStatus
auto subscribe actual Video Topic
H264 save / optional MPP decode
Raw save
```

输出：

```text
statusReceived
videoSamples
videoBytes
receiveFps
decodeFps
width/height
codec/pixelFormat
ddsErrors
```

## 20. DataDrivenTestQT

新增：

```text
ControlTransport=udp|dds|both
```

DDS 模式使用 MinGW ZRDDS。

内部仍先构造 BYHWICD struct，再 ToDds() 发布；Ack 从 InitAckV1 转回原状态逻辑。

## 21. HwaSim_IR

新增：

```ini
[CommandTransport]
Input=udp
Ack=match_input

[DdsProtocol]
Enable=false
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml
TopicControl=HwaSimIR.Control
TopicInit=HwaSimIR.Init
TopicRealtime=HwaSimIR.Realtime
TopicInitAck=HwaSimIR.InitAck
TopicVideoStatus=HwaSimIR.VideoStatus
DeduplicateWhenBoth=true
DeduplicateWindowMs=1000
```

支持：

```text
Input=udp|dds|both
Ack=match_input|udp|dds|both
```

## 22. both 去重

Control：

```text
platID + simCommand + currentRound + roundCut
```

Init：

```text
semantic hash
```

Realtime：

```text
platID + sensorID + time
```

同一短窗口只执行一次。

## 23. VideoDisplay

最终：

```text
Transport=legacy|dds
```

legacy 使用现有 TCP worker。

dds 同时订阅：

```text
Control
Init
Realtime
InitAck(optional)
VideoStatus
Video
```

从而 DDS 模式左侧 Init/Realtime 数据不再为空。

## 24. Annotation / 帧同步

为了做到 DDS 与 TCP 最终完全对齐，第二阶段增加：

```text
HwaSimIR.VideoMeta.precise
HwaSimIR.Annotation.precise
```

视频仍保持纯 Bytes。

推荐：

```idl
struct VideoFrameMetaV1
{
    long platID; //@key
    long sensorID; //@key
    unsigned long frameSeq;
    double ptsMs;
    boolean keyFrame;
};

struct AnnotationFrameV1
{
    long platID; //@key
    long sensorID; //@key
    unsigned long frameSeq;
    string<32768> json;
};
```

通过 frameSeq 对齐。

## 25. 四种最终拓扑

### A 自己闭环

```text
DataDrivenTestQT -> HwaSim_IR -> HwaSim_IR_VideoDisplay
```

Legacy 和 DDS 都要完整闭环。

### B 客户激励 + 我方显示

```text
客户激励 DDS -> HwaSim_IR -> VideoDisplay DDS
```

### C 客户激励 + 同事 Gateway + 客户板端

```text
客户激励 -> HwaSim_IR -> H264 DDS -> Gateway B -> Raw DDS -> 客户 Receiver
```

### D 客户直接接收

```text
客户激励 -> HwaSim_IR -> H264 或 Raw DDS -> 客户 Receiver
```

## 26. 公平视频 benchmark

新增：

```text
DDS/HwaSimIRVideoTransportBenchmark
```

相同 QoS：

```text
tcpv4 + RELIABLE + KEEP_ALL
```

H264：

```text
DirectBytes-AU
ShapeType-1KB
ShapeType-32KB
Chunk64KB
```

RawGray8 800x800：

```text
WholeFrameBytes-640KB
Chunk64KB
Chunk32KB
```

记录：

```text
appCopyMs
writeMs
samples/s
MiB/s
CPU/RSS
received samples
reassembled frames
queue depth
drops/errors
```

最终用数字决定 Direct/Chunk，而不是“同事看起来快”。

## 27. 两阶段实施

### F1：完整 DDS 控制平面 + VideoStatus + 三工具链 Demo + 视频 A/B

一次完成：

1. `HwaSimIRProtocolV1.idl`
2. zrddsgen + generated source
3. VS2015/MinGW/AArch64 三工具链编译
4. CommonDataDdsAdapter
5. DdsRuntimeManager
6. HwaSim_IR 支持 UDP/DDS/both
7. DDS InitAck
8. DataDrivenTestQT 支持 UDP/DDS/both
9. VideoDisplay DDS 订阅 Control/Init/Realtime
10. VideoStatusV1
11. VideoDisplay 自动 codec/topic/geometry
12. H264 GUI actual geometry
13. 客户 Stim Demo 三种 build
14. 客户板端 Receiver Demo
15. 同事 dds_pub_sub 协议审计
16. TransportBenchmark
17. WholeFrame vs chunk 公平 A/B
18. DDS 视频发送耗时统计
19. Legacy TCP/UDP 回归

F1 结束必须可跑：

```text
DataDrivenTestQT DDS -> HwaSim_IR -> VideoDisplay DDS
CustomerStimDemo DDS -> HwaSim_IR -> CustomerReceiverDemo
```

### F2：Gateway + Annotation/FrameMeta + 四拓扑最终验收

一次完成：

1. HwaSimIRDecodeGatewayDemo
2. H264 direct -> Gateway MPP -> Raw direct
3. 仅 benchmark 证明必要时实现 VideoChunkV1
4. VideoFrameMeta/Annotation DDS
5. STOP/RESET/round 保存生命周期完整对齐
6. precise/coarse
7. 四种拓扑闭环
8. 客户三工具链交付
9. ICD/现场调试文档
10. 性能和可靠性最终收口

## 28. 推荐最终视频协议

HwaSim_IR -> 客户 H264：

```text
DDS::Bytes
1 AU = 1 Sample
+ VideoStatusV1
```

HwaSim_IR -> 客户 Raw：

```text
DDS::Bytes
1 frame = 1 Sample
+ VideoStatusV1
```

只有 A/B 证明 whole-frame 是 ZRDDS 性能瓶颈时，再启用 VideoChunkV1。

HwaSim_IR -> 同事 B：让 B 直接订阅标准 H264 Bytes Topic，不为 B 额外维护 1KB ShapeType 协议。

## 30. F1 final gate closeout（2026-08-29）

DataDrivenTestQT MinGW F1 二进制已补跑纯 DDS 自动流程：RESET、INIT、DDS InitAck、
START、约 60.484 Hz Realtime、STOP 全部进入 HwaSimIR 的共用业务入口；三个进程角色
仍各自只初始化一次 DDS runtime。VideoDisplay 全 DDS 的最终短闭环为 sender/receiver
180/180、60.796 FPS、800x800 decoded geometry、aspectPreserved=1、DDS errors=0。

F1 最终补跑 RK3588 到 Windows 的同 NIC 公平矩阵后为 46 PASS / 1 FAIL。
Direct/Shape1K/Shape32K/Chunk64K 与 Raw whole/chunk 均为 600/600、0 drop/error；
唯一 FAIL 是历史约 50 FPS 现象未在本轮 60.796 FPS 闭环中复现，不能伪称精确定位。
F1 没有进入 F2 的 Annotation、VideoFrameMeta 或生产 Gateway。

## 31. F1 cross-host benchmark closeout（2026-08-29）

RK3588 Publisher 到 Windows Receiver 已按同一 tcpv4/RELIABLE/KEEP_ALL QoS
补齐七组 600 帧公平测试。H264-like Direct/Shape1K/Shape32K/Chunk64K 与 RawGray8
Whole/Chunk64K/Chunk32K 全部 600/600、0 drop、0 error。Shape1K 的 60,000
Samples 显著提高 CPU/write 时间；Raw 分片同样没有提高 36.6 MiB/s payload 吞吐。
最终推荐 H264 DirectBytes-AU、Raw WholeFrameBytes。

## 29. F1 execution record（2026-08-29）

F1 已在基线 `7383ef8653ab495b8a8cf071d8027b2a774f7a3f` 上实现，未 commit/push。
正式代码包括 frozen IDL/canonical generated TypeSupport、CommonDataDdsAdapter、
DdsRuntimeManager、HwaSimIR protocol endpoint、DataDriven DDS client、VideoDisplay typed
Readers、VideoStatus、Customer Stim/Receiver 和 TransportBenchmark。

实际通过：VS2015/MinGW/AArch64 compile，VS/MinGW semantic roundtrip，legacy size
24/385/506/17，VS/MinGW Stim DDS 控制闭环，DataDriven both 去重，HwaSimIR 单 runtime，
RK3588 Customer Receiver VideoStatus 自动 H264（30 Samples）与 RawGray8（10 帧）接收。
正式 sender timing 已输出 `[DdsVideoTiming]`。

保留失败/未完成：历史约 50 FPS 现场现象未精确复现、Customer Receiver optional
MPP decode、以及 D3.1 完整长时生产矩阵。跨 NIC 公平 Direct/ShapeType/Chunk benchmark
已补齐。VideoDisplay 全 DDS 已实测 180/180，约 60.8 FPS，
actual geometry/aspect 正确；F1 相关 legacy/Local MP4 回归已补跑。详细数字和门禁见
`docs/HwaSimIR_DDS_F1_Execution_Report.md`；F1 不进入 Annotation/VideoFrameMeta/Gateway。
