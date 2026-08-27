# HwaSimIR 控制/初始化/实时激励 DDS 化方案（保留 UDP 可选）

> 当前视频 DDS 保持 D2 方案不变：视频使用 `DDS::Bytes`，不把视频塞进 IDL。
> 当前仓库基线：`b46458b781e9943667305a4e7341c7f3acd8789d`（DDS集成D2阶段_20260826_2301）。
> 本文仅做后续架构设计，当前不修改生产代码。

## 1. 总体架构

建议最终拆成两个平面：

```text
视频大数据平面
  HwaSimIR.Video.* -> DDS::Bytes

控制/状态小数据平面
  HwaSimIR.Control     -> ControlCommandV1
  HwaSimIR.Init        -> InitCommandV1
  HwaSimIR.Realtime    -> RealtimeDataV1
  HwaSimIR.InitAck     -> InitAckV1
  HwaSimIR.VideoStatus -> VideoStatusV1
```

视频继续保持：

```text
HwaSimIR.Video.precise.H264
HwaSimIR.Video.precise.RawGray8
HwaSimIR.Video.precise.RawBGR24
HwaSimIR.Video.coarse.H264
HwaSimIR.Video.coarse.RawGray8
HwaSimIR.Video.coarse.RawBGR24
```

H.264：一个 DDS Sample = 一个完整 Annex-B AU。  
Raw：一个 DDS Sample = 一整帧原始像素。  
视频 Topic 内仍不加自定义头。

## 2. 为什么控制数据要分 Topic

不要把初始化、控制、实时激励、应答都塞进一个万能 Topic 再靠 flag 手工拆包。

推荐：

```text
HwaSimIR.Control
HwaSimIR.Init
HwaSimIR.Realtime
HwaSimIR.InitAck
HwaSimIR.VideoStatus
```

每个 Topic 对应一个明确 IDL Type，代码、QoS、客户接口都更清晰。

## 3. UDP 必须保留

未来运行配置建议：

```ini
[CommandTransport]
InputTransport=udp
AckTransport=match_input

DdsEnable=false
DomainId=150
QosFile=Config/DDS/ZRDDS_CONTROL_QOS.xml

TopicControl=HwaSimIR.Control
TopicInit=HwaSimIR.Init
TopicRealtime=HwaSimIR.Realtime
TopicInitAck=HwaSimIR.InitAck
TopicVideoStatus=HwaSimIR.VideoStatus

AcceptSensorBroadcast=true
DeduplicateWhenBoth=true
DeduplicateWindowMs=1000
EnablePerfLog=true
```

支持：

```text
InputTransport=udp
InputTransport=dds
InputTransport=both
```

Ack 支持：

```text
AckTransport=match_input
AckTransport=udp
AckTransport=dds
AckTransport=both
```

推荐 `match_input`：

```text
Init 从 UDP 收到 -> Ack 回 UDP
Init 从 DDS 收到 -> Ack 回 DDS
```

## 4. UDP/DDS 必须共用同一套业务入口

现有业务入口应继续复用：

```cpp
HwaSimIR::handleControlCmd(...)
HwaSimIR::handleInitCmd(...)
HwaSimIR::handleDisplayData(...)
```

目标：

```text
UdpCommThread ------------------+
                                |
                                v
                         ProtocolIngress
                                |
                                +--> HwaSimIR::handleControlCmd
                                +--> HwaSimIR::handleInitCmd
                                +--> HwaSimIR::handleDisplayData
                                ^
                                |
DdsCommandReceiver -> Adapter --+
```

禁止写成：

```text
UDP 一套初始化逻辑
DDS 另一套初始化逻辑
```

## 5. 未来建议新增模块

```text
HwaSim_IR/HwaSim_IR/DDS/
    DdsRuntimeManager.h/.cpp
    DdsCommandReceiver.h/.cpp
    DdsAckPublisher.h/.cpp
    CommonDataDdsAdapter.h/.cpp
    ProtocolIngress.h/.cpp
```

### DdsRuntimeManager

未来扩展控制 DDS 时，这是最先要做的重构。

D2 当前视频 Publisher 自己执行 DDS Init/Finalize。未来同一 HwaSim_IR 进程会有：

```text
Video Writer
Control Reader
Init Reader
Realtime Reader
InitAck Writer
VideoStatus Writer
```

因此必须改为：

```text
一个 HwaSim_IR 进程
=
一次 DDSIF::Init
+
共享 DomainParticipant
+
一次最终 DDSIF::Finalize
```

所有 DDS 模块共用 Runtime，禁止各自 Init/Finalize。

## 6. DDS 类型与 CommonData 隔离

不要让 zrddsgen 生成类型扩散进 Panda3D/IR 模块。

做显式 Adapter：

```cpp
bool FromDds(
    const HwaSimIRDds::InitCommandV1& src,
    BYHWICD::InitP2cObjectTrackingCmd& dst);

bool FromDds(
    const HwaSimIRDds::ControlCommandV1& src,
    BYHWICD::ControlP2cX1ObjTrackingCmd& dst);

bool FromDds(
    const HwaSimIRDds::RealtimeDataV1& src,
    BYHWICD::DisplayC2cObjTrackingData& dst);

void ToDds(
    const BYHWICD::InitAckC2pObjectTrackingCmd& src,
    HwaSimIRDds::InitAckV1& dst);
```

结果：

```text
UDP 收到 -> BYHWICD
DDS 收到 -> IDL Type -> BYHWICD

后续业务完全共用
```

## 7. IDL V1 推荐设计

新建：

```text
DDS/IDL/HwaSimIRControlV1.idl
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
        string<24> codec;
        string<128> topic;

        long width;
        long height;
        long fps;

        boolean compressed;
        boolean running;
    };
};
```

## 8. flag 为什么保留

DDS Topic 本身已经区分类型，所以 `0x41/0x36/0x38/0x37` 不再用于反序列化。

V1 仍建议保留，便于：

- 与现有 UDP 字段逐项对照；
- A/B 调试；
- Adapter 检查；
- 客户理解。

## 9. Key 与路由

推荐：

```text
Control:  platID
Init:     platID + sensorID
Realtime: platID + sensorID
InitAck:  platID + sensorID
```

ZRDDSGen 支持：

```idl
long platID; //@key
```

但 `sensorID=255` 仍是 HwaSimIR 业务广播规则，不是 DDS 自动广播语义。

当前应用路由必须继续保留：

```text
Control:
packet.platID == localPlatID

Init / Realtime:
platID match
&&
(
  sensorID == localSensorID
  ||
  (AcceptSensorBroadcast && sensorID == 255)
)
```

## 10. both 模式去重

如果 `InputTransport=both`，客户可能 UDP 和 DDS 同发同一消息。

必须在 `ProtocolIngress` 去重。

推荐：

```text
Control:
platID + simCommand + currentRound + roundCut

Init:
对所有业务字段做 hash

Realtime:
platID + sensorID + time
```

在短窗口内同一业务键只处理一次。

推荐 `AckTransport=match_input`，避免同一 Init 返回两份 Ack。

## 11. QoS

### Control / Init / InitAck

```text
tcpv4
RELIABLE_RELIABILITY_QOS
KEEP_ALL_HISTORY_QOS
VOLATILE
```

消息很少，可从：

```text
max_samples=256
```

开始。

### Realtime 60 Hz

第一版推荐：

```text
tcpv4
RELIABLE
KEEP_ALL
resource_limits >= 4096
```

这样兼容同步模式“一组输入对应一帧”。

如果以后客户明确异步模式允许跳过历史状态，再额外提供：

```text
hwasimir_realtime_latest
RELIABLE + KEEP_LAST depth=1
```

不能把它作为默认。

## 12. VideoStatus 的价值

D2 视频 Topic 故意没有：

```text
codec
width
height
fps
platID
sensorID
```

未来 `HwaSimIR.VideoStatus` 可以发布：

```text
platID
sensorID
channel
codec
topic
width
height
fps
compressed
running
```

客户先订阅 VideoStatus，再自动决定订阅哪个裸视频 Topic。

这样仍满足：

```text
真正的视频 DDS Sample 只有视频字节
```

第一版建议 VideoStatus：

```text
RELIABLE
KEEP_LAST depth=1
VOLATILE
```

状态变化立即发布，并可 1 Hz 重发。

## 13. ZRDDS IDL 生成

Windows：

```text
ZRDDS_HOME=F:\Programs\ZRDDS\ZRDDS-2.4.5
```

生成器：

```text
%ZRDDS_HOME%\bin\ZRDDSGen\zrddsgen.exe
```

ZRDDSGen 基本用法：

```text
zrddsgen
  -input_idl <path>
  -output_dir <directory>
  -language <C|C++|C#|java>
  [-example <normal|xml-ex|xml-sim>]
  [-project <Win64VS2015|LinuxMakefile|...>]
```

生产生成：

```powershell
$gen = Join-Path $env:ZRDDS_HOME 'bin\ZRDDSGen\zrddsgen.exe'

& $gen `
  -input_idl D:\HwaSimIR\DDS\IDL\HwaSimIRControlV1.idl `
  -output_dir D:\HwaSimIR\DDS\Generated\HwaSimIRControlV1 `
  -language C++
```

为理解官方 Publisher/Subscriber 用法，单独生成示例：

```powershell
& $gen `
  -input_idl D:\HwaSimIR\DDS\IDL\HwaSimIRControlV1.idl `
  -output_dir D:\HwaSimIR\DDS\GeneratedExample\HwaSimIRControlV1 `
  -language C++ `
  -example xml-sim `
  -project Win64VS2015
```

Linux Makefile 示例：

```powershell
& $gen `
  -input_idl D:\HwaSimIR\DDS\IDL\HwaSimIRControlV1.idl `
  -output_dir D:\HwaSimIR\DDS\GeneratedLinuxExample\HwaSimIRControlV1 `
  -language C++ `
  -example xml-sim `
  -project LinuxMakefile
```

ZRDDS 手册给出的 LinuxMakefile 构建形式：

```bash
make -f pubMakefile CONFIG=RELEASE
make -f subMakefile CONFIG=RELEASE
```

HwaSimIR 正式工程仍建议：

```text
Windows zrddsgen.exe
    -> 生成 TypeSupport C++ 源码
    -> 源码纳入仓库
    -> VS2015 编译
    -> Debian VM 用 aarch64-linux-gnu-g++ 交叉编译
```

不需要在 x86 VM 执行 ARM 版 zrddsgen。

## 14. sequence_bound

ZRDDSGen 手册说明默认：

```text
string_bound=255
sequence_bound=255
```

也支持：

```text
-sequence_bound
-enable_unbounded
```

本 V1 使用：

```text
trackerSensor[1]
targetState[5]
double[2]
```

等固定数组，不需要 unbounded sequence。

视频仍然使用 `DDS::Bytes`，不改成 IDL `sequence<octet>`。

## 15. Generated code 管理

建议：

```text
DDS/
  IDL/
    HwaSimIRControlV1.idl

  Generated/
    HwaSimIRControlV1/
      <zrddsgen 自动生成文件>

  ControlDemo/
    CustomerControlPublisher
    CustomerAckSubscriber
```

不要手工修改 generated 文件。

将生成类型编成独立库：

```text
HwaSimIRDdsTypes
```

然后：

```text
HwaSim_IR
  -> HwaSimIRDdsTypes
  -> ZRDDSCpp

客户 Demo / DataDrivenTestQT
  -> HwaSimIRDdsTypes
  -> ZRDDSCpp
```

## 16. 客户最终通信流程

业务时序不变，只替换传输：

```text
1. Publish HwaSimIR.Control : RESET
2. Publish HwaSimIR.Init
3. Subscribe HwaSimIR.InitAck
4. 收到 trackingReady=true
5. Publish HwaSimIR.Control : START
6. 60 Hz Publish HwaSimIR.Realtime
7. 同时 Subscribe HwaSimIR.Video.*
8. Publish HwaSimIR.Control : STOP
```

## 17. DDS callback 不能直接改场景

正确：

```text
DDS callback
  -> copy generated sample
  -> command/data queue
  -> HwaSimIR 主线程
  -> ProcessPendingNetworkCommands / realtime 调度
```

不要在 ZRDDS callback 线程直接操作 Panda3D Scene Graph。

## 18. 当前 UDP 与未来 DDS 映射

| 当前结构 | flag | DDS Topic | IDL Type | 方向 |
|---|---:|---|---|---|
| `ControlP2cX1ObjTrackingCmd` | `0x41` | `HwaSimIR.Control` | `ControlCommandV1` | 客户→HwaSimIR |
| `InitP2cObjectTrackingCmd` | `0x36` | `HwaSimIR.Init` | `InitCommandV1` | 客户→HwaSimIR |
| `DisplayC2cObjTrackingData` | `0x38` | `HwaSimIR.Realtime` | `RealtimeDataV1` | 客户→HwaSimIR |
| `InitAckC2pObjectTrackingCmd` | `0x37` | `HwaSimIR.InitAck` | `InitAckV1` | HwaSimIR→客户 |
| 无旧 UDP 对应 | - | `HwaSimIR.VideoStatus` | `VideoStatusV1` | HwaSimIR→客户 |

## 19. precise / coarse

两个 HwaSim_IR 进程可订阅同样的：

```text
HwaSimIR.Control
HwaSimIR.Init
HwaSimIR.Realtime
```

然后继续根据本地 platID/sensorID 过滤。

因此客户可以：

```text
sensorID=255 -> 两个通道都接收
```

也可：

```text
精确 sensorID -> 单独一个实例
```

不必为 precise/coarse 再复制 Control Topic。

## 20. 推荐联调顺序

不要第一次就同时迁移全部链路。

```text
Step 1: 现有 DDS Video Demo
Step 2: DDS Control RESET
Step 3: DDS Init -> DDS InitAck
Step 4: DDS START/STOP
Step 5: Realtime 1 Hz
Step 6: Realtime 60 Hz
Step 7: Control + Realtime + Video 全 DDS
Step 8: UDP + DDS both，验证去重
```

## 21. 客户最终应获得

```text
HwaSimIRControlV1.idl
ZRDDS_CONTROL_QOS.xml
zrddsgen Generated TypeSupport
DDS Control/Init/Realtime Publisher Demo
DDS InitAck Subscriber Demo
HwaSimIR_DDS_Control_ICD.md

现有：
HwaSimIRVideoReceiverDemo
DDS_HwaSimIR_ICD.md
```

客户不需要 Panda3D、MPP 或 HwaSimIR 源码。

## 22. 版本管理

IDL 一旦交给客户应冻结：

```text
HwaSimIRControlV1.idl
```

未来不兼容修改用：

```text
ControlCommandV2
InitCommandV2
RealtimeDataV2
```

不要继续依赖双方 C++ `#pragma pack(1)` 和 `sizeof()` 恰好一致。

## 23. 后续实施建议

不要混进当前视频 D3 性能收口。

### C1：IDL + 独立 Demo

完成：

```text
IDL V1
zrddsgen
Generated TypeSupport
Customer Control Demo
Windows <-> RK3588：
Control
Init/Ack
Realtime 1Hz/60Hz
```

### C2：正式接 HwaSimIR

完成：

```text
DdsRuntimeManager
DdsCommandReceiver
DdsAckPublisher
CommonDataDdsAdapter
ProtocolIngress

udp/dds/both
match_input Ack
deduplicate
precise/coarse
60Hz sync/async
DDS Control + DDS Video 并行
```

## 24. 最终推荐架构

```text
                          HwaSimIR
                             |
             +---------------+---------------+
             |                               |
       Control/Data Plane               Video Plane
             |                               |
       +-----+-----+                   +-----+-----+
       |           |                   |           |
      UDP         DDS                 TCP         DDS
       |           |                   |           |
       +----> ProtocolIngress           |      DDS::Bytes
                   |                    |
                   v                    |
           HwaSimIR handlers            |
                                        v
                                   H264 / Raw
```

DDS 内部最终为：

```text
DdsRuntimeManager
  +-- ControlCommandV1 Reader
  +-- InitCommandV1 Reader
  +-- RealtimeDataV1 Reader
  +-- InitAckV1 Writer
  +-- VideoStatusV1 Writer
  +-- Video DDS::Bytes Writer
```

原则：

```text
业务小数据 = IDL 强类型
视频大数据 = DDS::Bytes
UDP 保留可选
DDS 与 UDP 共用业务逻辑
```
