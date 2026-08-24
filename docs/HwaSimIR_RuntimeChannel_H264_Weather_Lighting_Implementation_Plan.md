# HwaSimIR 运行模式、双通道、H.264、纹理天气与照明系统实施方案

> 版本：v1.1，需求纠错后的总体实施规划  
> 更新时间：2026-07-16  
> 基线提交：`2f17f9cbd599fb7d7473a7e00e5ceff620ec4b77`（`修改通信接口后版本备份_20260716_1631`）  
> 目标平台：Windows Release x64 功能验证；RK3588 / Debian 11 / aarch64 HeadlessOffscreen 生产部署  
> 主体工程：`HwaSim_IR`、`DataDrivenTestQT`、`HwaSim_IR_VideoDisplay`  
> 性能目标：单通道实时输出稳定达到 60 FPS；双进程并发时分别统计，禁止以静默降帧代替性能收口  
> 红外边界：保持 Stage3～Stage7 物理链路语义；天气纹理和 Panda3D 灯光不得直接覆盖 MWIR/LWIR 辐射计算结果  

---

## 0. 本版纠错与硬约束

本版替换上一版规划中的以下错误方向。

### 0.1 不修改现有数据协议

本轮不得为了 ID 路由修改协议结构体，不得给：

```cpp
ControlP2cX1ObjTrackingCmd
```

增加 `sensorID`。

正确路由规则为：

```text
0x41 ControlP2cX1ObjTrackingCmd
  只检查 platID。
  cmd.platID == localPlatID 时接收。
  同一平台下 coarse/precise 两个传感器进程都接收该平台控制命令。

0x36 InitP2cObjectTrackingCmd
  检查 platID + sensorID。
  cmd.platID == localPlatID 且
  (cmd.sensorID == localSensorID 或 cmd.sensorID == 255) 时接收。

0x38 DisplayC2cObjTrackingData
  检查 platID + sensorID。
  data.platID == localPlatID 且
  (data.sensorID == localSensorID 或 data.sensorID == 255) 时接收。
```

`sensorID=255` 只适用于协议中本来包含 `sensorID` 的初始化和实时数据，不适用于控制命令。

### 0.2 三个程序的协议定义必须同步

本轮涉及的三个主体程序为：

```text
HwaSim_IR
DataDrivenTestQT
HwaSim_IR_VideoDisplay
```

活动协议头为：

```text
HwaSim_IR/HwaSim_IR/Common/CommonData.h
DataDrivenTestQT/CommonData.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/CommonData.h
```

以 `HwaSim_IR/HwaSim_IR/Common/CommonData.h` 为当前基准，将三个活动头文件同步为相同定义。同步时只统一现有结构体定义、顺序、注释和作用域，不增加、删除或重排协议字段。

后续任何经正式 ICD 确认的数据协议修改，也必须一次性同步三个程序并同时完成三工程构建验证，不允许只改发送端或只改接收端。

### 0.3 天气首阶段不使用粒子特效

当前天气完善阶段不使用 Panda3D Particle Effects，不创建逐粒子雨滴或雪花，也不引入体积云。

首阶段只使用：

```text
HwaSim_IR/Bin/Config/Weather
```

中的现有纹理和 profile，通过少量固定纹理层生成云、雨、雪：

```text
云：天空穹顶/大尺寸云层 Card 的纹理混合与 UV 缓慢滚动。
雨：全屏或相机前固定雨纹理层，按风向和速度滚动 UV。
雪：全屏或相机前固定雪纹理层，使用 1～2 层不同缩放/速度的 UV 滚动。
雾：距离相关 shader 混合，不使用粒子。
```

该方案优先保证红外合理性、Headless 可用和 60 FPS，再评估后续体积云或粒子效果。

---

## 1. 总体结论

近期双通道继续使用两个独立 `HwaSim_IR` 进程，但不再通过修改 `LoadNetworkConfig()` 并重复构建两次实现。

目标运行方式：

```text
同一套源码 + 同一个 HwaSim_IR 可执行文件 + 两份独立配置
```

Windows 示例：

```bat
HwaSim_IR.exe --channel precise
HwaSim_IR.exe --channel coarse
```

Linux/RK3588 示例：

```bash
./HwaSim_IR --channel precise
./HwaSim_IR --channel coarse
```

也支持直接指定配置：

```bash
./HwaSim_IR --network-config Config/NetworkConfig_precise.ini
./HwaSim_IR --network-config Config/NetworkConfig_coarse.ini
```

如果部署交付必须保留两个程序名，可以复制同一个构建产物：

```text
HwaSim_IR_precise.exe
HwaSim_IR_coarse.exe
```

但两者的差异仍由参数或配置决定，不允许再由源码注释决定。

总体实施顺序：

```text
R1  通道配置、三端协议一致性、ID 路由、simMode 调度闭环
R2  双进程启动与同步/异步 60 FPS 基线验收
V1  视频编码器抽象和现有 TCP 包兼容改造
V2  Windows H.264 编码/发送与 VideoDisplay 解码/显示
V3  RK3588 MPP 硬件 H.264 编码和板端性能收口
W1  纹理云层和红外云辐射/透射
W2  纹理雨雪、距离雾和天气综合效果
L1  24 小时太阳/月亮自然照明
L2  平台主动照明器及红外波段耦合
A1  双通道 + Headless + H.264 + 天气 + 照明综合验收
```

阶段门禁：

1. 每阶段先完成 Windows 构建和 smoke，再进行 RK3588 实测。
2. 新功能默认可关闭，失败时必须有明确日志和安全回退。
3. 视频编码、解码、文件写入不得进入 Panda3D 渲染线程。
4. 天气纹理在初始化或状态变化时加载，禁止每帧读取图片。
5. 单通道达不到 60 FPS 时暂停后续功能叠加，先定位瓶颈。
6. 双通道仍是双进程，不在本轮改造成单进程双相机。
7. 不改变现有红外目标热状态、尾喷、辐射、大气、MTF、噪声和 AGC 的物理语义。

---

## 2. 最新代码现状

### 2.1 数据接口现状

当前 `CommonData.h` 已包含：

```text
InitObjectTrackingParam.simMode
InitObjectTrackingParam.videoFps
trackerSensorParam.trackerSensorPixelAngle
InitP2cObjectTrackingCmd.platParamInit
F35/F22/预留目标最大数量
InitP2cObjectTrackingCmd.platID/sensorID
DisplayC2cObjTrackingData.platID/sensorID
ControlP2cX1ObjTrackingCmd.platID
```

`simMode` 定义：

```text
1：同步模式
2：异步模式，使用 videoFps
```

当前 HwaSimIR 已有：

```text
SetRenderMode(bool isSync, double targetFPS)
m_bSyncRenderMode
m_syncFrameActive
m_targetVideoFps
同步 TCP 队列等待
异步 TCP 队列覆盖旧帧
```

但 `trackingInit.simMode` 尚未形成完整的配置仲裁、模式切换、队列清理和验收闭环。

### 2.2 协议副本现状

当前：

```text
HwaSim_IR CommonData.h
HwaSim_IR_VideoDisplay CommonData.h
```

内容已经基本一致。

`DataDrivenTestQT/CommonData.h` 仍使用部分嵌套结构体定义，虽然字段目标相近，但长期存在类型作用域、结构体尺寸和后续修改不同步风险。

应在 R1 中完成一次三端同步，并增加自动检查。

### 2.3 网络配置现状

已有：

```text
NetworkConfig_precise.ini
NetworkConfig_coarse.ini
```

两份配置的 UDP/TCP 端口独立，但当前 `LoadNetworkConfig()` 通过注释一组路径、启用另一组路径选择通道。

缺少：

```text
本进程 channel
本进程 platID
本进程 sensorID
广播接收开关
配置选择参数
配置来源日志
```

### 2.4 当前 UDP 路由隐患

当前 UDP 线程在完成包身份判断前，会用最近一次发包地址更新远端地址，然后才解析数据。

在双通道、多发送端或误发包环境下，可能发生：

```text
不属于本进程的数据包进入业务处理；
不属于本进程的发包者覆盖 ACK 远端地址；
错误初始化/实时包进入队列；
coarse/precise 状态相互污染。
```

需要把“包长度验证、flag 解析、ID 过滤、业务入队、远端地址更新”重新排序。

### 2.5 H.264 当前状态

当前只完成：

```text
h264En 请求字段
Codec/Encoder/Bitrate/GOP/LowLatency 配置
H.264 可用性探测占位
JPEG 安全回退
annotation JSON 中的 codec 元数据
```

实际发送端仍然每帧执行：

```cpp
cv::imencode(".jpg", ...)
```

Linux 明确返回 H.264 未实现，VideoDisplay 收到 `h264_annexb` 时也会因解码器未集成而拒绝该帧。

因此必须完整补齐：

```text
编码器 -> H.264 Access Unit -> TCP payload -> 接收端 codec 分流
       -> 持久解码器 -> QImage -> 显示/录像
```

### 2.6 天气当前状态

现有模块已经具备：

```text
IRWeatherEffects
0～5 六类天气 profile
分波段 sky/ground 灰度缩放
云温、云量、云透明度
雾密度、能见度
雨雪密度、速度、风速、风向
weather_profiles.json
weather_textures.json
云/雨/雪/太阳/月亮纹理资源
Stage7 天气状态和纹理缓存框架
```

当前实际问题：

```text
EnableCloudLayer=0
EnablePrecipitation=0
CloudLayerMaxCards=0
PrecipitationMaxParticles=0
```

效果没有真正启用，并且此前的 `PrecipitationMaxParticles` 命名容易把实现引向粒子系统。本轮应改为固定纹理层，不使用粒子。

### 2.7 照明当前状态

已有输入：

```text
trackerSensorParam.illuminatorX/Y/Z
trackerSensorParam.illuminatorPitch/Yaw/Roll
trackerSensorParam.illuminatorAngle
trackerSensorParam.illuminatorSpotRad
WeaponState.illuminatorEn
```

已有红外计算基础：

```text
太阳方向
太阳高度/方位
太阳反射权重
天气 sunDirectScale/skyDiffuseScale
Stage5 reflectedRadiance
```

当前没有完整的 Panda3D 自然灯光节点，也没有主动照明器节点和统一波段规则。

本轮不新增协议字段。主动照明波段先通过运行配置设置，默认可跟随 `trackerSensorBand`。

---

## 3. 目标运行架构

```text
                        启动参数/环境变量
                     --channel / --network-config
                                 │
                                 v
                   ┌──────────────────────────┐
                   │ RuntimeInstanceConfig    │
                   │ channel/platID/sensorID  │
                   │ UDP/TCP/config source    │
                   └─────────────┬────────────┘
                                 │
UDP 0x41/0x36/0x38               v
────────────────────> ┌──────────────────────────┐
                      │ PacketRouteFilter        │
                      │ 0x41: platID             │
                      │ 0x36/0x38: plat+sensor   │
                      │ sensorID=255 broadcast   │
                      └─────────────┬────────────┘
                                    │ accepted only
                                    v
                      ┌──────────────────────────┐
                      │ RenderControlArbiter     │
                      │ external simMode         │
                      │ local config fallback    │
                      └─────────────┬────────────┘
                                    │
                ┌───────────────────┴───────────────────┐
                v                                       v
       ┌─────────────────┐                    ┌─────────────────┐
       │ Sync Scheduler  │                    │ Async Scheduler │
       │ 1 data = 1 frame│                    │ latest + FPS    │
       └────────┬────────┘                    └────────┬────────┘
                └───────────────────┬───────────────────┘
                                    v
                      ┌──────────────────────────┐
                      │ Scene + IR + Weather     │
                      │ + Natural/Active Light   │
                      └─────────────┬────────────┘
                                    v
                      ┌──────────────────────────┐
                      │ Stage6 final_sensor      │
                      │ Visible / Headless       │
                      └─────────────┬────────────┘
                                    v
                      ┌──────────────────────────┐
                      │ Async Encoder            │
                      │ JPEG / H.264             │
                      └─────────────┬────────────┘
                                    v
                      ┌──────────────────────────┐
                      │ TCP existing frame packet│
                      │ tracking + JSON + payload│
                      └─────────────┬────────────┘
                                    v
                      ┌──────────────────────────┐
                      │ VideoDisplay             │
                      │ JPEG/H264 decode/display │
                      └──────────────────────────┘
```

---

## 4. 配置设计

### 4.1 双通道网络配置

`NetworkConfig_precise.ini`：

```ini
[Identity]
channel=precise
platID=1001
sensorID=2
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=192.168.1.189
localPort=8888
remoteIp=192.168.1.188
remotePort=9998

[TCP]
serverIp=192.168.1.189
serverPort=5555
```

`NetworkConfig_coarse.ini`：

```ini
[Identity]
channel=coarse
platID=1001
sensorID=1
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=192.168.1.189
localPort=8889
remoteIp=192.168.1.188
remotePort=9999

[TCP]
serverIp=192.168.1.189
serverPort=5556
```

`platID/sensorID` 示例值必须由实际系统配置填写，不在代码中硬编码业务值。

配置选择优先级：

```text
--network-config <path>
  > --channel precise|coarse
  > 环境变量 HWASIMIR_NETWORK_CONFIG / HWASIMIR_CHANNEL
  > 默认配置通道
```

启动必须打印：

```text
[RuntimeInstance]
channel=precise
configPath=...
platID=...
sensorID=...
udpLocal=...
udpRemote=...
tcpServer=...
configSource=cli/env/default
```

### 4.2 simMode 配置

在 `HwaSimIRRuntime.ini` 增加：

```ini
[RenderControl]
ModePolicy=ExternalPreferred
ConfiguredSimMode=2
ConfiguredVideoFps=60
MinRealtimeFps=60
EnforceMinRealtimeFps=true
AsyncInputPolicy=Latest
ClearQueuesOnModeApply=true
```

含义：

```text
ModePolicy=ExternalPreferred
  初始化包 simMode 为 1/2 时使用外部值；
  外部值非法时回退 ConfiguredSimMode。

ModePolicy=ConfigOnly
  忽略外部 simMode，使用 ConfiguredSimMode。

ConfiguredSimMode
  1=同步，2=异步。

ConfiguredVideoFps
  外部 videoFps 无效或 ConfigOnly 时使用。
```

目标帧率规则：

```text
同步模式：输出节奏由有效 0x38 数据包驱动，不用时钟重复生成帧。
异步模式：按有效 videoFps 渲染，场景使用最近一次有效实时数据。
videoFps=0：不限帧，只作为诊断模式；正式 60 FPS 验收不得使用 0。
videoFps<60 且 EnforceMinRealtimeFps=true：提升到 60 并告警。
videoFps>设备能力：保留请求值并通过性能日志暴露未达标，不静默修改协议数据。
```

### 4.3 天气配置

保留 `[Stage7Weather]`，调整为纹理层语义：

```ini
[Stage7Weather]
EnableWeatherEffects=true
UseWeatherUdpInput=true
WeatherProfilePath=Config/Weather/weather_profiles.json
WeatherTextureConfig=Config/Weather/weather_textures.json

EnableCloudLayer=true
CloudRenderMode=LayeredCards
CloudLayerCount=2
CloudUpdateHz=10
CloudUvSpeedScale=1.0

EnableFog=true
FogRenderMode=DistanceShader

EnablePrecipitation=true
PrecipitationRenderMode=TextureOverlay
RainTextureLayerCount=1
SnowTextureLayerCount=2
PrecipitationUpdateHz=30
PrecipitationOpacityScale=1.0

EnableParticleEffects=false
EnableVolumetricCloud=false
```

原有：

```text
CloudLayerMaxCards
PrecipitationMaxParticles
```

可保留兼容读取，但新实现不创建粒子；建议逐步替换为明确的 `LayerCount` 配置。

### 4.4 照明配置

```ini
[NaturalLighting]
EnableNaturalLighting=true
TimeSource=SimulationData
FallbackHour=12.0
UpdateHz=2
EnableSun=true
EnableMoon=true
PandaLightAffects=VIS_NIR_SWIR
ApplyToIRRadiance=true

[ActiveIlluminator]
EnableActiveIlluminator=true
BandMode=FollowSensor
ConfiguredBand=2
UseProtocolMount=true
UseProtocolAngle=true
UseProtocolIntensity=true
UpdateHz=30
PandaSpotlightEnable=true
ApplyToIRRadiance=true
MaxRangeM=50000
```

本轮不增加 `illuminatorBand` 协议字段：

```text
BandMode=FollowSensor：照明波段跟随 trackerSensorBand。
BandMode=Configured：使用 ConfiguredBand。
```

等正式 ICD 明确独立照明波段字段后，再统一修改三个程序协议头。

---

## 5. R1：统一实例配置、协议一致性、ID 路由和 simMode

### 5.1 目标

完成本轮最基础、最优先的运行控制闭环：

```text
一套构建产物运行 coarse/precise 两个进程；
三个程序使用一致的现有协议定义；
不同实例只处理属于本实例的数据；
外部 simMode 和本地配置均可控制同步/异步；
不改变任何协议结构体布局。
```

### 5.2 实施内容

#### 5.2.1 统一协议头

以：

```text
HwaSim_IR/HwaSim_IR/Common/CommonData.h
```

为基准，将以下两个活动头同步：

```text
DataDrivenTestQT/CommonData.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/CommonData.h
```

要求：

1. 三份文件中结构体字段、顺序、类型、`#pragma pack(1)` 一致。
2. 不增加 `ControlP2cX1ObjTrackingCmd.sensorID`。
3. 不增加、删除或重排其他协议字段。
4. 修改因结构体作用域统一导致的编译引用。
5. 增加 `tools/check_commondata_sync.ps1`，对三份活动头做内容或规范化 hash 比较。
6. 三个程序启动或测试时记录关键结构体 `sizeof`：

```text
ControlP2cX1ObjTrackingCmd
InitP2cObjectTrackingCmd
DisplayC2cObjTrackingData
InitAckC2pObjectTrackingCmd
```

#### 5.2.2 通道选择

新增通道选择器：

```text
--channel precise
--channel coarse
--network-config <path>
```

删除 `LoadNetworkConfig()` 中靠注释切换 coarse/precise 的逻辑。

同一二进制可同时启动两个进程，分别绑定各自 UDP 端口和连接各自 TCP 端口。

#### 5.2.3 ID 路由

新增统一路由方法，例如：

```cpp
bool ShouldAcceptControl(const ControlP2cX1ObjTrackingCmd& cmd) const;
bool ShouldAcceptInit(const InitP2cObjectTrackingCmd& cmd) const;
bool ShouldAcceptDisplay(const DisplayC2cObjTrackingData& data) const;
```

规则固定为：

```cpp
controlAccepted = cmd.platID == localPlatID;

initAccepted =
    cmd.platID == localPlatID &&
    (cmd.sensorID == localSensorID ||
     (acceptSensorBroadcast && cmd.sensorID == 255));

displayAccepted =
    data.platID == localPlatID &&
    (data.sensorID == localSensorID ||
     (acceptSensorBroadcast && data.sensorID == 255));
```

拒绝包必须：

```text
不进入 pending command/display queue；
不改变 m_initSceneData/m_realTimeSceneData；
不改变仿真运行状态；
不转发到 TCP；
不发送初始化 ACK；
不更新 UDP 动态远端地址。
```

若收到 `sensorID=255` 的初始化包并通过过滤，初始化 ACK 中：

```text
platID = localPlatID
sensorID = localSensorID
```

用于明确应答来自哪个传感器实例，不改变 ACK 结构体。

#### 5.2.4 UDP 地址更新顺序

当前逻辑需改为：

```text
recvfrom
  -> 验证长度
  -> 解析 flag
  -> 解析对应结构体
  -> ID 过滤
  -> accepted 后才允许更新动态 remote
  -> accepted 后业务入队
```

`allowDynamicRemote=false` 时永远使用配置文件中的远端地址，不因接收源变化而修改 ACK 目标。

#### 5.2.5 simMode 仲裁

初始化命令在主线程处理时执行：

```text
读取 trackingInit.simMode/videoFps
  -> 根据 ModePolicy 选择有效模式
  -> 合法性检查和回退
  -> 清理旧输入队列/TCP 帧队列
  -> 重置 sourceSeq/outputOrdinal/telemetry
  -> SetRenderMode
  -> TCP setSyncMode
  -> 输出 RenderControl 日志
```

同步模式：

```text
每个通过 ID 过滤的 0x38 数据包最多触发一帧；
不得使用旧数据重复渲染额外帧；
不得覆盖尚未输出的有效同步帧；
编码/发送速度不足时通过 queueWait/latency 暴露问题。
```

异步模式：

```text
实时数据只保留最新状态；
渲染按 videoFps 时钟执行；
输入队列不得持续堆积；
允许覆盖未消费的旧实时状态，但记录 overwrite 数量。
```

### 5.3 日志

新增低频日志：

```text
[RuntimeInstance]
[ProtocolLayout]
[PacketRoute]
[PacketRouteReject]
[RenderControl]
[RenderModeApply]
```

示例：

```text
[PacketRoute] flag=0x38 accepted=1 localPlatID=1001 localSensorID=2 packetPlatID=1001 packetSensorID=2 reason=exact_match

[PacketRouteReject] flag=0x36 accepted=0 localPlatID=1001 localSensorID=2 packetPlatID=1001 packetSensorID=1 reason=sensor_mismatch

[RenderControl] externalSimMode=2 externalVideoFps=60 policy=ExternalPreferred effectiveSimMode=2 effectiveVideoFps=60 source=udp_init
```

高频 0x38 拒绝日志必须采样，避免控制台拖慢 RK3588。

### 5.4 R1 验收

构建：

```text
HwaSim_IR Windows Release x64 通过
DataDrivenTestQT Qt 5.12.12 Release 通过
HwaSim_IR_VideoDisplay Release x64 通过
HwaSim_IR aarch64 交叉编译通过（工具链可用时）
```

功能：

```text
同一个 HwaSim_IR 构建产物可分别加载 precise/coarse 配置。
两个进程可同时运行且端口不冲突。
Control 同 platID 时两个传感器进程均接收。
Init/Display 只被匹配 sensorID 的进程接收。
sensorID=255 的 Init/Display 被同 platID 的两个进程接收。
错误 platID 的所有包均拒绝。
拒绝包不改变 UDP ACK 远端。
外部 simMode=1 生效为同步。
外部 simMode=2 + videoFps=60 生效为异步 60 FPS。
ConfigOnly 可覆盖外部 simMode。
三个程序协议 sizeof 完全一致。
check_commondata_sync.ps1 通过。
```

性能：

```text
ID 过滤平均耗时应接近不可测量量级，不引入每帧动态分配。
单通道基础 smoke 不低于修改前基线。
异步输入队列不长期满。
同步 sourceSeq 保持连续。
```

---

## 6. R2：双进程与 60 FPS 基线收口

### 6.1 目标

在未开启真实 H.264、纹理天气和照明前，先证明新的实例配置和调度不会破坏已有基线。

### 6.2 测试矩阵

```text
A：precise / VisibleWindow / Sync / JPEG
B：precise / VisibleWindow / Async 60 / JPEG
C：coarse / VisibleWindow / Sync / JPEG
D：coarse / VisibleWindow / Async 60 / JPEG
E：precise + coarse 双进程 / Async 60 / JPEG
F：precise + coarse 双进程 / HeadlessOffscreen / Async 60 / JPEG
```

每组至少输出：

```text
udpFps
renderFps
outputFps
displayFps
pandaDoFrameMs
readbackMs
jpegMs
tcpSendMs
inputQueueDepth
outputQueueDepth
sourceSeqLag
latencyAvgMs
latencyP95Ms
dropped/overwritten
```

验收边界：

```text
单通道目标 60 FPS。
双进程分别统计，任何一个通道不得被另一个通道的包污染。
同步模式不丢有效输入帧。
异步模式允许覆盖旧状态，但不得出现持续累计延迟。
```

---

## 7. V1：视频编码器抽象

### 7.1 目标

先重构编码链路，不立即改变默认 JPEG 行为。

新增统一接口，例如：

```cpp
struct EncodedVideoFrame {
    std::string codec;
    std::vector<uint8_t> payload;
    bool keyFrame;
    int64_t ptsMs;
    int width;
    int height;
};

class IVideoEncoder {
public:
    virtual bool configure(const VideoEncoderConfig&) = 0;
    virtual bool encode(const RawVideoFrame&, EncodedVideoFrame&) = 0;
    virtual void requestKeyFrame() = 0;
    virtual void reset() = 0;
};
```

实现：

```text
JpegFrameEncoder
H264FrameEncoder（先占位）
```

要求：

1. 编码仍在 TCP/编码后台线程。
2. JPEG 输出必须与现有图像兼容。
3. `TcpCommThread` 不再直接硬编码 `cv::imencode`。
4. Windows/Linux 共用相同接口，后端实现可不同。
5. 复用 RGB/YUV/编码输出缓冲，减少逐帧分配。
6. 编码器切换和新连接必须重置状态。

### 7.2 TCP 包兼容

不修改 `CommonData.h`。

继续使用现有帧包结构：

```text
[总长度]
[tracking 长度][DisplayC2cObjTrackingData]
[annotation JSON 长度][annotation JSON]
[payload 长度][JPEG 或 H.264 数据]
```

由 annotation JSON 中现有字段标识：

```json
{
  "packetVersion": 2,
  "payloadCodec": "jpeg",
  "keyFrame": false,
  "ptsMs": 0
}
```

H.264 时：

```json
{
  "packetVersion": 2,
  "payloadCodec": "h264_annexb",
  "keyFrame": true,
  "ptsMs": 0
}
```

因此 H.264 传输不需要修改 UDP 数据协议结构体。

---

## 8. V2/V3：H.264 端到端闭环

### 8.1 编码格式

采用：

```text
H.264 Annex-B
每个 TCP frame payload 对应一个完整 Access Unit
```

关键帧要求：

```text
编码器启动第一帧为 IDR；
TCP 重连后请求 IDR；
初始化/开始新回合后请求 IDR；
按 GOP 周期产生 IDR；
IDR payload 携带或可获得 SPS/PPS。
```

### 8.2 Windows 后端

功能验证优先使用 FFmpeg/libavcodec 内存编码：

```text
RGB/BGR -> YUV420P/NV12 -> H.264 Annex-B
```

要求低延迟：

```text
B 帧关闭；
低延迟 preset/tune；
编码队列有界；
输出单 Access Unit；
不使用 VideoWriter 文件接口冒充实时传输编码。
```

### 8.3 RK3588 后端

生产编码优先使用 RK MPP 硬件 H.264：

```text
CPU RGB readback -> RGB/NV12 转换 -> MPP 编码 -> Annex-B
```

后续优化方向：

```text
减少 RGB 到 NV12 转换开销；
复用 MPP buffer；
评估 RGA 转换；
评估 GPU/DRM buffer 到 MPP 的低拷贝路径。
```

首轮先完成稳定闭环，不在同一阶段强制零拷贝。

### 8.4 VideoDisplay 解码

新增持久 H.264 解码器：

```text
按 payloadCodec 分流；
JPEG 继续 QImage/loadFromData；
H.264 送入 FFmpeg decoder；
解码输出转换为 QImage；
解码器跨帧保持；
收到新 SPS/PPS、重连或回合复位时 reset；
在收到可解码 IDR 前不显示破碎 P/B 帧。
```

### 8.5 H.264 回退

```text
h264En=false：JPEG。
h264En=true 且编码器可用：H.264。
h264En=true 且编码器不可用、FallbackToJpeg=true：JPEG + 明确告警。
h264En=true 且编码器不可用、FallbackToJpeg=false：停止视频输出并明确报错，不伪装为 H.264。
```

### 8.6 H.264 验收

```text
activeCodec=h264_annexb
发送端不再执行 JPEG 编码
VideoDisplay 能连续解码和显示
重连后在 IDR 恢复图像
初始化/开始新回合后图像正常
tracking/annotation/图像仍一帧一一对应
800x800@60 FPS 持续 30 秒
无持续编码队列增长
无持续 sourceSeqLag 增长
记录 h264EncodeMs/h264DecodeMs/encodedBytes/keyFrame
```

---

## 9. W1：纹理云层与红外云效果

### 9.1 目标

使用现有云纹理生成可见云层，不使用体积云和粒子系统。

### 9.2 云层实现

优先方案：固定数量的大尺寸纹理层。

```text
CloudLayer 0：较低层，大尺度纹理，较慢 UV 滚动。
CloudLayer 1：较高层，不同缩放/偏移，较快或不同方向滚动。
```

实现载体可选：

```text
天空穹顶内层云纹理；
围绕相机的水平/弧形大 Card；
少量固定云层 Card。
```

选择原则：

```text
节点数量固定；
不逐云团创建对象；
相机移动时云层跟随传感器参考位置，避免飞出云层范围；
纹理只在初始化或资源变化时加载；
每帧只更新 UV 偏移和少量 shader uniform。
```

### 9.3 云的红外处理

云纹理只提供空间 alpha/密度：

```text
cloudMask = textureAlphaOrLuma
```

云的红外灰度不能直接使用 PNG 的 RGB 颜色，应由：

```text
云温 cloudTemperatureK
当前 band
背景天空辐射
云层透过率
云自身热辐射
天气 profile
```

共同计算。

简化链路：

```text
L_cloud_pixel = tau_cloud * L_background
              + (1 - tau_cloud) * L_cloud_emit

tau_cloud = exp(-cloudOpticalDepth * cloudMask)
```

波段策略：

```text
VIS/NIR/SWIR：纹理反射和太阳/天空照明占主要作用。
MWIR：云温热辐射 + 对背景和目标的遮蔽/衰减。
LWIR：云自身热辐射更明显，按云温映射，不固定为白云。
```

云层不直接改变目标表面温度。

### 9.4 性能边界

```text
CloudLayerCount 默认 2，最大先限制为 4。
云更新 10 Hz，UV 时间可由 shader 每帧推进。
不创建体积纹理、不做 ray marching。
不使用动态阴影贴图作为首阶段要求。
```

---

## 10. W2：纹理雨、雪和距离雾

### 10.1 雨

使用：

```text
rain_shaft.png / rain.rgba
```

建立 1 个相机前全屏纹理层或 Stage6 final shader overlay：

```text
纹理平铺；
UV 沿降雨方向滚动；
风向控制水平偏移；
windV/envRainSnowSpeedScale 控制速度；
precipitationDensity 控制纹理阈值和透明度；
envMaxHeightRain/envTransHeightRain 控制是否处于有效降雨层。
```

红外表现：

```text
雨纹理不默认画成纯白；
MWIR/LWIR 中雨滴/雨幕灰度根据天气 profile 和背景灰度确定；
主要物理作用仍是目标对比度下降、大气透过率下降和路径项变化；
纹理条纹只作为可视化补充，透明度保持克制。
```

### 10.2 雪

使用：

```text
snow.rgba
```

建立 1～2 个固定纹理层：

```text
不同纹理缩放；
不同 UV 初始偏移；
不同下降速度；
少量水平漂移；
不生成单个雪花节点。
```

红外表现：

```text
雪花纹理灰度由 band/weather profile 控制；
MWIR/LWIR 不固定显示为高亮白点；
雪地地表背景变化由 groundGrayScale/温度模型处理；
空中雪纹理和地面雪辐射分开。
```

### 10.3 雾

雾不需要纹理，使用距离相关 shader：

```text
fogFactor = 1 - exp(-density * distance)
L = (1 - fogFactor) * L_scene + fogFactor * L_path_or_fog
```

输入：

```text
envVisibility
envHumidity
envSky
band
fogDensity
fogGray/path radiance
```

不得只把整帧线性叠灰；必须随距离增加。

### 10.4 天气状态切换

```text
0 晴：关闭云/雨雪/雾纹理层，恢复晴天 profile。
1 云：开启云纹理层。
2 雨：云 + 雨纹理层 + 雾/能见度衰减。
3 雪：云 + 雪纹理层 + 对应能见度衰减。
4 雾：关闭降水纹理，启用距离雾。
5 阴：高覆盖云纹理层，无降水。
```

状态切换时只 show/hide 或调整 uniform，不重复创建销毁资源。

### 10.5 天气验收

```text
六类 envSky 可实时切换。
云/雨/雪使用仓库纹理，画面非空且资源加载成功。
日志无逐帧 texture load。
EnableParticleEffects=false。
无 Panda3D 粒子系统节点。
MWIR 图像中天气不等同于可见光彩色纹理。
云、雨、雪不会把目标整体温度改高。
Headless TCP 输出与 VisibleWindow 效果一致。
单通道 800x800 开启每种天气短跑，目标仍为 60 FPS。
```

---

## 11. L1：24 小时自然照明

### 11.1 目标

使用 Panda3D 照明节点表达太阳/月亮方向变化，同时将自然照明物理量接入红外反射辐射链路。

### 11.2 时间来源

优先顺序：

```text
实时数据 time -> 仿真小时
本地配置 FallbackHour
```

将 24 小时时间映射为：

```text
太阳高度角
太阳方位角
昼夜标志
月亮近似方向
太阳直射比例
天空漫射比例
```

首阶段不做高精度天文历算，可使用配置化日出/日落和连续插值，保留未来接入日期、经纬度天文模型的接口。

### 11.3 Panda3D 灯光

```text
太阳：DirectionalLight
月亮：DirectionalLight，强度显著低于太阳
环境：AmbientLight 或 shader sky diffuse 输入
```

更新频率建议 1～2 Hz，方向变化进行平滑。

Panda3D 灯光主要影响：

```text
VIS/NIR/SWIR 场景预览和反射表现
```

MWIR/LWIR 不直接依赖 Panda3D light color 作为最终灰度。

### 11.4 红外耦合

继续使用 Stage5：

```text
sun direction
solar irradiance/weight
material reflectance
weather sunDirectScale
skyDiffuseScale
```

计算反射辐射。

波段策略：

```text
VIS/NIR/SWIR：自然照明影响明显。
MWIR：白天可保留较弱太阳反射，同时热辐射仍存在。
LWIR：太阳直接反射权重很低，主要通过材料温度慢变化体现；首阶段不做瞬时全局升温。
夜间：太阳直射为 0，月光只在 VIS/NIR/SWIR 保留弱反射。
```

---

## 12. L2：平台主动照明器

### 12.1 输入映射

现有协议字段映射：

```text
illuminatorX/Y/Z        -> 灯具相对载机安装位置
illuminatorPitch/Yaw/Roll -> 灯具安装姿态
illuminatorAngle        -> Spotlight 视场角，mrad 转度
illuminatorSpotRad      -> 强度参数，先按可配置标度映射
illuminatorEn           -> 实时启停
```

波段：

```text
BandMode=FollowSensor：跟随 trackerSensorBand。
BandMode=Configured：使用本地 ConfiguredBand。
```

不修改数据协议。

### 12.2 Panda3D Spotlight

创建一个挂在载机平台/传感器安装节点下的 `Spotlight`：

```text
位置和姿态来自初始化参数；
FOV 来自 illuminatorAngle；
实时 illuminatorEn 控制启停；
强度变化只更新 light/shader 输入；
节点在初始化时创建并复用。
```

### 12.3 红外照明链路

主动照明不能通过提高目标温度实现。

按波段分为：

```text
VIS/NIR/SWIR：
  Panda3D Spotlight 提供几何照明；
  shader 根据光锥、距离、表面法线和材料反射率计算反射。

MWIR/LWIR：
  仅在配置的照明源确实工作于对应红外波段时，
  将照明辐照度加入 reflectedRadiance；
  不改变 body temperature；
  不直接叠加屏幕光斑。
```

简化模型：

```text
E_illum = enabled
        * inCone
        * intensity
        * rangeAttenuation
        * max(0, dot(normal, lightDir))
        * atmosphereTau

L_reflected_illum = materialReflectanceBand * E_illum / pi
```

支持光锥边缘平滑，避免硬边。

### 12.4 主动照明性能

```text
只创建一个 Spotlight 和一组 shader uniform。
不启用实时阴影贴图作为默认要求。
照明状态/姿态/波段变化时强制刷新。
静态时按 ActiveIlluminator.UpdateHz 低频更新 CPU 参数。
```

---

## 13. A1：综合验收

### 13.1 功能矩阵

```text
通道：precise / coarse / 双进程
模式：Sync / Async 60
宿主：VisibleWindow / HeadlessOffscreen
编码：JPEG / H.264
天气：晴 / 云 / 雨 / 雪 / 雾 / 阴
照明：白天 / 夜间 / 主动照明关 / 主动照明开
```

### 13.2 性能边界

每个主场景记录：

```text
udpFps
renderFps
outputFps
displayFps
sceneUpdateMs
pandaDoFrameMs
readbackMs
weatherUpdateMs
lightingUpdateMs
jpegMs/h264EncodeMs
jpegDecodeMs/h264DecodeMs
tcpSendMs
queueDepth
sourceSeqLag
latencyAvgMs
latencyP95Ms
encodedBytes
```

目标：

```text
单通道 800x800：稳定 60 FPS。
平均端到端延迟不超过 80 ms。
无持续输入/编码/发送队列增长。
双进程时分别统计，不互相接收错误传感器数据。
纹理天气开启后若低于 60 FPS，先减少纹理层数量和更新频率，不改红外物理语义兜底。
RK3588 H.264 必须优先使用硬件编码达到 60 FPS。
```

### 13.3 回归边界

```text
默认 JPEG 路径仍可用。
默认天气关闭或晴天时画面与原基线一致。
默认主动照明关闭。
Headless direct_final 在无屏幕天气 overlay 时继续生效。
启用雨雪纹理 overlay 时允许进入必要 final pass，但必须记录 renderPath 和开销。
Annotation JSON、实时数据和图像/H.264 Access Unit 保持帧级对应。
```

---

## 14. 风险与决策

### 14.1 协议副本风险

风险：三个工程复制 `CommonData.h`，后续容易再次漂移。

当前措施：

```text
三文件同步；
自动 hash 检查；
三工程 sizeof 日志和构建验证。
```

长期可把协议头提取为共享目录或独立静态库，但不作为 R1 必须重构项。

### 14.2 H.264 依赖风险

Windows FFmpeg、RK3588 MPP 和 Qt 显示工程工具链不同。

决策：

```text
先统一 IVideoEncoder/decoder 语义；
Windows 完成功能闭环；
RK3588 替换为 MPP 后端；
JPEG 始终保留安全回退。
```

### 14.3 天气真实性风险

纹理云雨雪不是高精度气象体积模拟。

本阶段目标是：

```text
可控、低成本、红外合理、能表达六类天气、达到 60 FPS。
```

后续再独立评估体积云、深度分层云、粒子雨雪，不与本阶段混做。

### 14.4 雨雪屏幕纹理深度不足

首阶段雨雪主要是相机空间效果，缺少真实三维深度。

缓解：

```text
使用 1～2 层不同缩放/速度纹理；
结合真实距离雾和大气透过率；
限制高透明度，避免像可见光贴纸；
保留以后改成少量空间切片的接口，但仍不必使用粒子。
```

### 14.5 双进程性能风险

RK3588 同时运行两个 800x800@60 通道可能受 GPU、读回、编码和内存带宽限制。

决策：

```text
先验证单通道所有功能达到 60 FPS；
再验证双进程；
分别记录 readback/encode/do_frame；
不通过静默降低视频帧率隐藏问题。
```

---

## 15. 预计修改文件

### R1 主体

```text
HwaSim_IR/HwaSim_IR/main.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/UdpCommThread.h/.cpp
HwaSim_IR/HwaSim_IR/UdpCommThread_Linux.h/.cpp
HwaSim_IR/HwaSim_IR/Common/CommonData.h
HwaSim_IR/Bin/Config/NetworkConfig_precise.ini
HwaSim_IR/Bin/Config/NetworkConfig_coarse.ini
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
DataDrivenTestQT/CommonData.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/CommonData.h
tools/check_commondata_sync.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

### H.264 阶段

```text
HwaSim_IR/HwaSim_IR/Video/*
HwaSim_IR/HwaSim_IR/TcpCommThread*.h/.cpp
HwaSim_IR/HwaSim_IR/CMakeLists.txt
HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj
HwaSim_IR_VideoDisplay/.../TcpServerWorker*.h/.cpp
HwaSim_IR_VideoDisplay/.../VideoDecoder/*
HwaSim_IR_VideoDisplay 工程文件
```

### 天气阶段

```text
HwaSim_IR/HwaSim_IR/IR/IRWeatherEffects.h/.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h/.cpp
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/Bin/Config/Weather/weather_profiles.json
HwaSim_IR/Bin/Config/Weather/weather_textures.json
现有 Weather/Textures 资源，不新增粒子资源
```

### 照明阶段

```text
HwaSim_IR/HwaSim_IR/IR/IRLightingModel.h/.cpp（建议新增）
HwaSim_IR/HwaSim_IR/HwaSimIR.h/.cpp
HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h/.cpp
红外 shader 初始化代码
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
```

---

## 16. 实施记录模板

每阶段完成后在本节追加：

```text
日期 / 阶段 / 提交前工作区状态
修改文件
完成内容
未完成内容
Windows 构建结果
Linux/aarch64 构建结果
运行配置
测试命令
功能结果
性能摘要
已知问题
下一阶段建议
```

### 2026-07-16 / v1.1 planning correction

- 明确不修改协议，不给 `ControlP2cX1ObjTrackingCmd` 增加 `sensorID`。
- 控制命令仅按 `platID` 过滤；初始化和实时数据按 `platID + sensorID` 过滤，并支持 `sensorID=255` 广播。
- 明确三个主体程序的活动 `CommonData.h` 必须同步，但本轮不改变结构体字段布局。
- 天气首阶段不使用 Panda3D 粒子系统和体积云，只使用现有 Weather 纹理生成云、雨、雪。
- 云使用固定少量纹理层；雨雪使用固定相机空间纹理层/Stage6 overlay；雾使用距离 shader。
- 主动照明波段先采用本地配置或跟随传感器波段，不增加协议字段。
- 保留双进程方案，改为同一可执行文件通过配置选择 coarse/precise。

### 2026-07-16 / R1 实施记录

#### 基线与工作区

- 基线为 `main` / `origin/main`：`2f17f9cbd599fb7d7473a7e00e5ceff620ec4b77`。
- 本记录对应未提交工作区；未执行 commit 或 push。
- 仅实施 R1。未实施 H.264、天气或照明，未改变 JPEG、Headless、红外物理、标注和 TCP 帧包语义。

#### 修改文件

```text
DataDrivenTestQT/CommonData.h
DataDrivenTestQT/NetworkConfig.ini
DataDrivenTestQT/main.cpp
DataDrivenTestQT/mainwindow.cpp
DataDrivenTestQT/mainwindow.h
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/Bin/Config/NetworkConfig_precise.ini
HwaSim_IR/Bin/Config/NetworkConfig_coarse.ini
HwaSim_IR/HwaSim_IR/main.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/UdpCommThread.h
HwaSim_IR/HwaSim_IR/UdpCommThread.cpp
HwaSim_IR/HwaSim_IR/UdpCommThread_Linux.h
HwaSim_IR/HwaSim_IR/UdpCommThread_Linux.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/main.cpp
tools/check_commondata_sync.ps1
tools/r1_protocol_sender.cpp
tools/r1_runtime_route_smoke.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

`HwaSim_IR/HwaSim_IR/Common/CommonData.h` 作为基准未修改；`HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/CommonData.h` 已与基准一致，因此也无内容差异。三份活动协议头经换行归一化后内容和 SHA-256 完全一致。

#### 完成内容

- 将 DataDrivenTestQT 中嵌套的协议类型作用域与基准统一，并修复对应编译引用；结构体字段、字段顺序和既有协议布局均未改变，`ControlP2cX1ObjTrackingCmd` 仍无 `sensorID`。
- 增加三文件一致性检查和三端 `[ProtocolLayout]` 日志。关键结构体大小为 `24 / 385 / 506 / 17` 字节，依次对应 Control、Init、Display、InitAck。
- 同一个 `HwaSim_IR.exe` 支持 `--channel precise|coarse` 和 `--network-config <path>`；移除源码注释切换配置，增加 CLI、环境变量和默认配置的来源优先级。
- precise/coarse 网络配置增加 `[Identity]`；启动 `[RuntimeInstance]` 输出实际配置路径、通道、本地 ID、广播/动态远端策略以及 UDP/TCP 端点和配置来源。
- Windows/Linux UDP 路径统一执行“长度和 flag 校验 → 结构体解析 → ID 过滤 → 通过后才更新动态远端和进入业务处理”。Control 仅匹配 `platID`；Init/Display 匹配 `platID` 和本地 `sensorID`，并按配置接受 `sensorID=255`。
- 拒绝包在业务处理前返回，不入队、不修改状态、不转发 TCP、不发送 ACK，也不更新 UDP 远端。`allowDynamicRemote=false` 时保留配置远端；广播 Init 的 ACK 使用本地真实 `platID/sensorID`。
- 增加 `[RenderControl]` 配置与 `ExternalPreferred|ConfigOnly` 仲裁。外部合法 `simMode=1/2` 可覆盖本地模式，非法值回退配置；模式应用前清输入/TCP 队列并重置 telemetry、序号和帧计数。同步模式一包一帧，异步模式按有效 `videoFps` 消费最新状态。
- DataDrivenTestQT 从现有配置读取并发送既有 `simMode/videoFps/platID/sensorID`，测试 CLI 只覆盖这些已有字段，未增加协议字段。
- 增加低频 `[RuntimeInstance]`、`[ProtocolLayout]`、`[PacketRoute]`、`[PacketRouteReject]` 和 `[RenderControl]` 日志。

#### 构建结果

- HwaSim_IR Windows x64 Release：通过，`HwaSim_IR/Bin/HwaSim_IR.exe`。
- DataDrivenTestQT Qt 5.12.12 MinGW 7.3 x64 Release：通过，`build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe`；仅有既有未使用变量/符号比较警告。
- HwaSim_IR_VideoDisplay Windows x64 Release：通过，`HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe`；构建时显式指定 Qt 5.12.12 msvc2015_64 安装路径。
- aarch64：未执行。当前 Windows 环境无 `cmake`、`aarch64-linux-gnu-g++`、`ninja`、`make`，也无 Panda3D/OpenCV aarch64 sysroot；未以宿主编译替代交叉编译结果。

#### 测试结果

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\check_commondata_sync.ps1
PASS：三协议头归一化 SHA-256 均为 30CD0B98A37C09C661F36D848DF434F53762D941E6BD219CD98604C0F5444FD6；Control.sensorID 不存在。

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\r1_runtime_route_smoke.ps1
PASS：同一二进制 precise/coarse 双进程；Control 正确/错误 platID；Init 精确/错误/255 sensorID；Display 精确/错误/255 sensorID 与错误 platID；广播 ACK 本地真实 ID；禁用动态远端；simMode=1；simMode=2/videoFps=60；非法 simMode 回退本地 2/60 配置。
测试使用脚本生成的 loopback precise/coarse 配置，跟踪的生产网络配置保持不变。
日志：logs/r1-runtime-20260716-234855
HwaSim_IR.exe SHA-256：419358C9AFAD4CEEC56027C1A9341408D3B71B155549E24BAEDDCC79DE8EBB4F

DataDrivenTestQT 实际发送 smoke
PASS：临时 loopback 配置下，DataDrivenTestQT 实际发送 platID=1001、sensorID=2、simMode=2、videoFps=60；HwaSim_IR 接受 Init/Display 并应用异步 60 FPS。
日志：logs/r1-datadriven-20260716-234359

三端 ProtocolLayout 启动 smoke
PASS：DataDrivenTestQT 和 HwaSim_IR_VideoDisplay 均输出 24/385/506/17；HwaSim_IR 同值由路由 smoke 验证。
日志：logs/r1-layout-20260716-234726

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\stage3_modtran_tau_loader_check.ps1 -Strict
PASS：Stage3 保持 MODTRAN tau-only，无 path/sky/solar 或 Stage5 扩展。

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\stage4_hotspot_check.ps1 -Strict
PASS：Stage4 热源/亮斑职责、完整目标键、可见性门控和无毁伤模型边界保持。
```

#### 性能摘要与未完成项

- R1 路由热路径只增加固定次数的长度/flag/ID 比较和采样计数；拒绝包在入队与业务处理前终止，未增加逐包动态分配。Display 路由日志仅记录启动阶段首批事件和之后每 120 包一次。
- 双进程 smoke 中两个通道使用同一个二进制并同时保持运行；异步模式确认应用 `videoFps=60` 和 `AsyncInputPolicy=Latest`，同步模式确认应用一包一帧调度。
- DataDrivenTestQT loopback smoke 的 60 FPS 发送采样为 `60.490 / 59.989 / 59.981 FPS`，363 包累计平均 `60.153 FPS`；该数据只说明输入发送时钟达到目标，不等同于渲染/TCP 端到端性能。
- 本次是控制闭环与路由 smoke，不宣称已完成持续 800x800 双通道 60 FPS 性能验收；H.264、天气、照明及其持续吞吐测试按范围留给后续阶段。
- 唯一环境性未完成项为 aarch64 交叉编译；需要提供对应编译器、CMake/构建工具以及 Panda3D/OpenCV aarch64 sysroot 后复测。

### 2026-07-20 / R2 双进程 60 FPS 基线收口实施记录

#### 基线、范围与工作区

- 基线为最新 `main` / `origin/main`：`9fb641891f8cc616924fa628b3ef77f86b08b228`。
- 本记录对应未提交工作区；未执行 commit 或 push。
- 仅实施 R2。未实现 H.264、天气或照明；未修改协议字段/布局、红外物理、图像质量、JPEG 默认质量、800x800 分辨率、标注语义或 TCP 帧包格式。
- 自动验收显式使用 JPEG、`h264En=0`，并关闭不在本轮验收范围内的 `saveMP4En`；DataDrivenTestQT 的默认 `saveMP4En=true` 未改变。

#### 修改文件

```text
DataDrivenTestQT/main.cpp
DataDrivenTestQT/mainwindow.cpp
DataDrivenTestQT/mainwindow.h
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/IR/IRPerfStats.cpp
HwaSim_IR/HwaSim_IR/IR/IRPerfStats.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/main.cpp
tools/r2_dual_60fps_acceptance.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

#### 完成内容与修复

- 新增 `tools/r2_dual_60fps_acceptance.ps1`。脚本为每组/每通道生成独立 loopback HwaSim_IR、DataDrivenTestQT、VideoDisplay 临时 INI，通过显式配置路径启动，不复制、覆盖或改写生产 precise/coarse 配置；运行前后校验两份生产配置 SHA-256。
- 五组均执行至少 5 秒预热和 30 秒正式统计。正式窗口通过日志字节偏移隔离，生成每组 `summary.json` 和顶层 `summary.json/csv/md`，按通道给出 PASS/FAIL 与失败原因。
- 双进程组同时运行同一个 `HwaSim_IR.exe` 的 precise/coarse 实例、两个 DataDrivenTestQT 激励实例和两个 VideoDisplay 实例；`[RuntimeInstance]`、`[Perf]`、`[RenderPerfProbe]`、`[VideoPerf]`、`[StimPerf]` 和同步帧日志带 `channel/platID/sensorID/pid`。
- DataDrivenTestQT 和 VideoDisplay 增加仅用于实例化/验收的 `--network-config`、`--channel` 和既有 ID 参数入口；DataDrivenTestQT 增加 `--save-mp4=0|1` 测试覆盖入口，协议本身未增加字段。
- HwaSim_IR 性能汇总补充实例身份、`outputQueueDepth` 统一命名、输入 Latest 覆盖数、TCP 输出覆盖数和 dropped/overwritten 累计值。VideoDisplay 性能汇总补充实例身份和实际配置路径。
- 自动向每个 HwaSim_IR UDP 端口注入对端 sensorID 的 Display 包；要求出现 `[PacketRouteReject] reason=sensor_mismatch`，并断言不存在对应 accepted 路由，双通道隔离全部通过。
- 初始 precise/VisibleWindow/Async60 基线只有 `outputFps=58.285`、`displayFps=58.296`；输入为 `59.981 FPS`，队列/lag 不增长，`pandaDoFrameMs=16.557`，而读回/JPEG/TCP 分别只有 `1.045/5.188/0.119 ms`。这将瓶颈定位到帧率锁定，不是红外计算、读回、JPEG 或 TCP。
- 修复为单一调度所有者：窗口交换不再独立执行 sync-video 等待；同步模式继续由 UDP 包驱动，一包一帧；异步模式将 Panda `M_limited` 的逐帧相对睡眠替换为主循环绝对 deadline 调度。绝对 deadline 消除 Windows 睡眠误差逐帧累计，并在长暂停后重置 deadline，避免补发陈旧突发帧。

#### Windows Release 构建与回归

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\stage0_build.ps1
PASS：HwaSim_IR Windows x64 Release。
PASS：DataDrivenTestQT Qt 5.12.12 / MinGW 7.3 x64 Release；仅保留既有未使用变量和有符号/无符号比较警告。

MSBuild HwaSim_IR_VideoDisplay.sln /t:Build /p:Configuration=Release /p:Platform=x64
PASS：HwaSim_IR_VideoDisplay Windows x64 Release，Qt 5.12.12 msvc2015_64。

powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\r1_runtime_route_smoke.ps1
PASS：R1 同一二进制双通道、Control platID、Init/Display 精确/错误/255 sensorID、错误 platID、ACK 身份、禁用动态远端、simMode=1、simMode=2/60 和非法模式回退全部通过。
日志：logs/r1-runtime-20260720-160037
HwaSim_IR.exe SHA-256：4702665E90FFBD84A449601EC20EA93A81D1B90CA34900098848D94D9DD0CC2A
```

#### R2 自动验收矩阵

命令：

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\r2_dual_60fps_acceptance.ps1 -WarmupSeconds 5 -MeasureSeconds 30
```

结果：`PASS`。完整日志和机器可读汇总位于 `logs/r2-60fps-20260720-154747`；生产 precise/coarse 配置哈希前后一致。

速率和阶段耗时（FPS 为正式窗口平均值，耗时单位 ms）：

| 场景 | 通道 | udpFps | renderFps | outputFps | displayFps | pandaDoFrame | readback | JPEG | TCP send |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| precise / Visible / Sync | precise | 60.002 | 60.002 | 60.003 | 59.993 | 5.142 | 1.106 | 4.275 | 0.104 |
| precise / Visible / Async60 | precise | 59.983 | 59.984 | 60.019 | 60.010 | 4.804 | 1.157 | 4.434 | 0.103 |
| coarse / Visible / Async60 | coarse | 60.019 | 59.986 | 60.021 | 60.003 | 5.233 | 1.289 | 6.273 | 0.128 |
| dual / Visible / Async60 | precise | 59.994 | 59.994 | 60.029 | 60.003 | 6.577 | 1.618 | 7.858 | 0.156 |
| dual / Visible / Async60 | coarse | 60.009 | 59.975 | 60.011 | 59.978 | 6.025 | 1.616 | 7.826 | 0.158 |
| dual / Headless / Async60 | precise | 59.990 | 59.990 | 60.025 | 59.983 | 4.509 | 1.484 | 5.355 | 0.110 |
| dual / Headless / Async60 | coarse | 59.974 | 60.008 | 60.043 | 60.021 | 4.523 | 1.609 | 5.346 | 0.116 |

队列、序号、延迟和丢弃/覆盖（深度与 lag 为正式窗口最大值；计数从模式应用重置后累计，包含预热）：

| 场景 | 通道 | inputQ | outputQ | sourceSeqLag | latencyAvg | latencyP95 | dropped | input overwritten | output overwritten | 对端 sensorID 拒绝 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| precise / Visible / Sync | precise | 0 | 1 | 0 | 10.301 | 12.812 | 0 | 0 | 0 | PASS |
| precise / Visible / Async60 | precise | 1 | 1 | 1 | 16.543 | 27.886 | 0 | 536 | 0 | PASS |
| coarse / Visible / Async60 | coarse | 1 | 1 | 1 | 21.347 | 34.131 | 0 | 530 | 1 | PASS |
| dual / Visible / Async60 | precise | 1 | 1 | 1 | 25.145 | 35.647 | 0 | 530 | 3 | PASS |
| dual / Visible / Async60 | coarse | 1 | 1 | 1 | 24.993 | 35.729 | 0 | 517 | 3 | PASS |
| dual / Headless / Async60 | precise | 1 | 1 | 1 | 23.458 | 35.847 | 0 | 516 | 1 | PASS |
| dual / Headless / Async60 | coarse | 1 | 1 | 1 | 23.330 | 35.176 | 0 | 551 | 1 | PASS |

- 所有通道 `outputFps>=59`、`displayFps>=59`，平均端到端延迟均低于 80 ms；最高平均延迟为双进程 Visible precise 的 `25.145 ms`。
- 同步组有效输入/输出覆盖均为 0，sourceSeqLag 为 0。异步组按 `Latest` 策略允许覆盖旧状态；input/output queue 最大值均为 1，sourceSeqLag 最大值为 1，首段/末段统计均未持续增长。
- 双进程 Visible 是当前最重组合，JPEG 平均约 `7.83~7.86 ms`，但 `pandaDoFrameMs` 仍仅 `6.03~6.58 ms`，TCP send 低于 `0.16 ms`，有充足的 16.67 ms 帧预算；当前瓶颈已不再限制 60 FPS。

#### 未完成项与下一阶段边界

- RK3588 / Debian 11 / aarch64 实机未测试。本机未找到 `aarch64-linux-gnu-g++`、`aarch64-linux-gnu-gcc` 或 `aarch64-linux-gnu-cmake`，也没有目标机 GPU/驱动环境，因此不以 Windows 结果代替 RK3588 结论。
- 后续在 RK3588 上必须复用相同 5 秒预热 + 30 秒正式窗口、双激励/双 VideoDisplay（或等价接收器）和逐通道身份化统计，重点复核 Headless 双进程读回、JPEG 和内存带宽。
- 本轮没有开始 H.264、天气或照明；这些仍按 R3/W1/L1 之后阶段实施，不应回填到 R2 结果中。

### 2026-07-21 / V1～V3 编码器解耦与 Windows H.264 端到端闭环实施记录

#### 基线、范围与依赖

- 基线为最新 `main` / `origin/main`：`a5ce85633f99e56592a40efdc4cc4dca53abc515`（`双进程 60 FPS 收口版本_20260720_1632`）。本记录对应未提交工作区；未执行 commit 或 push。
- 本轮完成通用编码接口、Windows FFmpeg H.264 内存编码、既有 TCP 帧包发送、VideoDisplay 持久解码、显示和现有 MP4 录像闭环。JPEG 仍是 `h264En=false` 时的默认路径。
- 未实现 RK3588 MPP、天气或照明；未修改 `CommonData.h`、协议结构体、TCP 帧包字节布局、红外物理、800x800 分辨率、JPEG 默认质量、翻转/灰度语义、标注目标语义或录像输入的 `QImage` 链路。
- Windows 验证依赖为 FFmpeg `n8.1.2-29-g703dcc25b9-20260720` GPL shared build（BtbN；GCC 15.2.0），`libavcodec 62.28.102`、`libavutil 60.26.102`、`libswscale 9.5.102`，构建包含 `libx264`。SDK 通过 `FFMPEG_ROOT` 传入，本地 `.deps/` 已忽略，不写死开发机路径。
- 继续使用既有 OpenCV 4.4.0、Qt 5.12.12、MSVC/MSBuild 14.0 和 MinGW 7.3 工具链。

#### 修改文件

```text
.gitignore
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/HwaSim_IR/CMakeLists.txt
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj
HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj.filters
HwaSim_IR/HwaSim_IR/TcpCommThread.h/.cpp
HwaSim_IR/HwaSim_IR/TcpCommThread_Linux.h/.cpp
HwaSim_IR/HwaSim_IR/Video/VideoEncoder.h
HwaSim_IR/HwaSim_IR/Video/JpegFrameEncoder.cpp
HwaSim_IR/HwaSim_IR/Video/H264FfmpegEncoder.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.vcxproj
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.vcxproj.filters
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.h/.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/Video/VideoDecoder.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/Video/VideoDecoder.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/Video/H264FfmpegDecoder.cpp
tools/stage0_build.ps1
tools/r2_dual_60fps_acceptance.ps1
tools/v3_h264_recovery_smoke.ps1
tools/r1_protocol_sender.cpp
tools/phase2a_sync60_save_smoke.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

#### 完成内容

- 新增 `RawVideoFrame`、`VideoEncoderConfig`、`EncodedVideoFrame`、`IVideoEncoder`、`JpegFrameEncoder` 和 `H264FfmpegEncoder`。Windows/Linux `TcpCommThread` 共用接口；编码器和输出容量跨帧保持，`TcpCommThread` 只负责队列、编码器选择、实际结果 JSON、原 TCP 包组装和发送，不再直接调用 `cv::imencode`。
- JPEG 编码移入 `JpegFrameEncoder`，保留质量 100、BGR 字节解释、`COLOR_RGB2GRAY` 历史转换、垂直翻转和 RGB/gray 输出；VideoDisplay 的 JPEG 解码仍调用 `QImage::loadFromData(..., "JPEG")`。R2 JPEG 性能回归通过。
- `H264FfmpegEncoder` 使用持久 `AVCodecContext/AVFrame/AVPacket/SwsContext`，输入转 YUV420P，`max_b_frames=0`、单编码线程、`ultrafast + zerolatency + baseline`，输出 Annex-B。每个 TCP payload 聚合一帧的完整 Access Unit；关键 AU 强制校验同时含 SPS、PPS 和 IDR NAL。
- 首帧、TCP 成功重连、发送失败、初始化转发、控制/新回合和帧计数复位均请求下一帧 IDR。编码器不逐帧创建；复位时才安全重建。
- annotation JSON 由实际编码结果填写 `payloadCodec/activeCodec/keyFrame/ptsMs/encodedBytes/encoderName/codecFallbackReason`，兼容字段 `codec/h264EncoderName` 保留。`sendFramePacket` 仅把原 `jpegData` 变量泛化为 encoded payload，长度头、结构体段、JSON 段和 payload 段顺序及 TCP 帧包语义不变。
- 新增持久 `IVideoDecoder/JpegFrameDecoder/H264FfmpegDecoder`。H.264 接收端按 `payloadCodec` 分流，未收到真实 IDR 前丢弃依赖帧；重连、初始化、控制/回合复位时 reset，SPS/PPS 变化时重建并用同一 IDR 恢复。解码到 RGB `QImage` 后继续走原显示和 `AsyncVideoRecorder` 链路。
- `FFMPEG_ROOT` 为 MSBuild/CMake 可选依赖：具备头文件和 import library 时定义 `HWASIM_HAS_FFMPEG`、链接 libavcodec/libavutil/libswscale 并复制运行 DLL；未配置时编译真实 unavailable stub，H.264 请求按配置回退 JPEG，不伪报 H.264 可用。
- 增加低频 `[VideoEncoder]`、`[VideoDecoder]`、`[CodecFallback]`、`[H264Perf]`；发送/接收性能日志均带 `channel/platID/sensorID/pid`，并输出实际 codec、编码/解码耗时、payload 字节、关键帧、队列和既有 FPS/延迟统计。

#### Windows Release 构建与回归

```text
HwaSim_IR x64 Release + FFMPEG_ROOT                         PASS
DataDrivenTestQT Qt 5.12.12 / MinGW 7.3 x64 Release       PASS
HwaSim_IR_VideoDisplay x64 Release + FFMPEG_ROOT           PASS
HwaSim_IR x64 Release（独立 OutDir、无 FFMPEG_ROOT）       PASS
HwaSim_IR_VideoDisplay x64 Release（独立 OutDir、无 SDK）  PASS
```

- 最终 HwaSim_IR.exe SHA-256：`4344A70F57FC79FD2380C1B3F2838F7BEA2FAC5BC63AC6728137A3F5384D7F92`。
- 无 SDK 构建输出位于 `logs/nosdk-build-20260721/hwa` 和 `logs/nosdk-build-20260721/video`；相关源文件仍参与编译，但 `HWASIM_HAS_FFMPEG` 未定义。
- R1 最终回归：`PASS`，日志 `logs/r1-runtime-20260721-114847`；协议尺寸仍为 `24/385/506/17`，双通道、ID 路由、广播 ACK、动态远端关闭及 simMode 1/2 全通过。
- R2 JPEG 最终回归：`PASS`，日志 `logs/r2-60fps-20260721-113146`；生产 precise/coarse 配置哈希前后一致。

#### Windows 端到端测试矩阵

所有正式性能组均预热 5 秒、统计 30 秒，使用脚本生成的 loopback 临时配置；`output/display FPS >= 59`、平均延迟 `< 80 ms`、输入/输出队列和 sourceSeqLag 不持续增长、错误 sensorID 被拒绝。

| 场景 | 通道 | codec | outputFps | displayFps | latency Avg/P95 ms | encode ms | decode ms | 平均 payload B | 结果 |
|---|---|---|---:|---:|---:|---:|---:|---:|---|
| JPEG Visible Async60 | precise | jpeg | 60.010 | 60.000 | 26.788 / 40.353 | 7.932 | 4.873 | 31,528 | PASS |
| JPEG dual Visible Async60 | precise | jpeg | 60.003 | 59.992 | 27.324 / 38.143 | 8.487 | - | 32,331 | PASS |
| JPEG dual Visible Async60 | coarse | jpeg | 59.995 | 59.990 | 27.639 / 37.980 | 8.491 | - | 32,169 | PASS |
| H.264 Visible Async60 | precise | h264_annexb | 60.009 | 60.022 | 16.898 / 30.315 | 3.934 | 1.641 | 7,421 | PASS |
| H.264 Headless Async60 | precise | h264_annexb | 60.003 | 60.017 | 18.246 / 29.322 | 2.934 | 1.241 | 1,675 | PASS |
| H.264 dual Visible Async60 | precise | h264_annexb | 60.002 | 60.010 | 18.213 / 30.397 | 4.208 | 1.578 | 1,402 | PASS |
| H.264 dual Visible Async60 | coarse | h264_annexb | 59.969 | 59.991 | 18.996 / 28.926 | 4.168 | 1.655 | 8,085 | PASS |

H.264 正式日志：单 Visible `logs/r2-60fps-20260721-114712`，Headless 与双进程 `logs/r2-60fps-20260721-113354`。所有 H.264 组均为发送端/接收端 `activeCodec=h264_annexb`，`jpegBytesMax=0`、`h264KeyFrameSeen=1`、`h264DecodeErrors=0`，队列深度和 sourceSeqLag 最大值均为 1，且 precise/coarse 对端 sensorID 路由拒绝通过。

恢复性验收命令：

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\v3_h264_recovery_smoke.ps1 -FFmpegRoot <path>
```

结果：`PASS`，日志 `logs/v3-h264-recovery-20260721-112902`。强制终止并重启 VideoDisplay 后，发送端 TCP 重连请求 IDR，接收端首个可解码 AU 为 IDR；复位→重新初始化→开始后再次恢复 IDR。将后端配置为未集成的 `mediafoundation` 时实际 `requestedCodec=h264 / activeCodec=jpeg`，`codecFallbackReason=requested_encoder_not_integrated:mediafoundation`。H.264 解码后的现有录像链路生成可由 ffprobe 完整读取的 MP4，共 775 帧。

#### JPEG/H.264 对比、瓶颈判断与未完成项

- 同一 precise/Visible/Async60 内容下，JPEG 平均 payload `31,528 B`（约 `15.13 Mbit/s@60`），H.264 平均 `7,421 B`（约 `3.56 Mbit/s@60`），带宽降低约 `76.5%`、为 JPEG 的约 `1/4.25`。
- 同场景 H.264 编码 `3.934 ms`，低于 JPEG 的 `7.932 ms`；H.264 解码 `1.641 ms`，低于本次 JPEG 解码采样 `4.873 ms`。TCP 发送约 `0.16 ms`，当前 Windows 双进程 60 FPS 下未观察到编码、解码、TCP 或队列瓶颈。
- payload 大小依赖画面内容，Headless/precise 的低字节数不应用作固定码率承诺；验收依据仍是实际 codec、FPS、延迟、关键帧和队列稳定性。
- RK3588 MPP 硬件编码未实现，Debian 11/aarch64 也未交叉编译或实机测试；当前 Windows 环境没有 aarch64 编译器、Panda3D/OpenCV sysroot 或目标板驱动。后续 MPP 必须继续实现同一 `IVideoEncoder`，不能改变 TCP payload/JSON/协议语义，也不能用 Windows libx264 结果替代板端结论。
- 天气与照明未实现；红外物理、目标三元组映射、可见性门控、标注和录像语义保持 R1/R2 基线。

### 2026-07-29 / V4 RK3588 MPP 与 TCP Packet v3 实施记录

#### 基线、范围与环境边界

- 基线为最新 `main` / `origin/main`：`c523d02c9a6d593ccffcfcf0033b5885740dbd2d`（`Windows端H264实现_20260729_1301`）。未执行 commit 或 push。
- 只实施 V4：Linux/aarch64 RKMPP 编码后端、编码后端选择、TCP Packet v3、VideoDisplay v3 兼容和验收/部署工具。未实现天气或照明。
- `CommonData.h`、UDP 协议/路由、红外物理、800x800 分辨率、R1/R2 调度和标注目标语义均未修改。工作区原有 `.idea` 删除/修改不属于 V4，实施过程未覆盖。
- Windows 可访问 FFmpeg SDK，但当前机器没有 `cmake`、`aarch64-linux-gnu-g++`、MPP sysroot、Debian 虚拟机连接信息或 RK3588 设备连接，因此没有执行或声称 aarch64 编译、MPP 运行和板端性能实测。

#### 修改文件

```text
HwaSim_IR/HwaSim_IR/Common/TcpVideoPacketV3.h
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/HwaSim_IR/CMakeLists.txt
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/HwaSim_IR/TcpCommThread.cpp
HwaSim_IR/HwaSim_IR/TcpCommThread.h
HwaSim_IR/HwaSim_IR/TcpCommThread_Linux.cpp
HwaSim_IR/HwaSim_IR/TcpCommThread_Linux.h
HwaSim_IR/HwaSim_IR/Video/VideoEncoder.h
HwaSim_IR/HwaSim_IR/Video/H264MppEncoder.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/AsyncVideoRecorder.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/AsyncVideoRecorder.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.h
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.cpp
HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.h
tools/runtime_config_check.ps1
tools/rk3588_mpp_compile_check.cpp
tools/rk3588_mpp_compile_check.sh
tools/rk3588_v4_deploy_acceptance.sh
tools/tcp_packet_v3_header_check.cpp
tools/tcp_packet_v3_header_check.ps1
tools/v4_packet_v3_acceptance.ps1
docs/HwaSimIR_V4_RK3588_MPP_CLion_Deployment.md
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

#### RK3588 MPP 编码器

- 新增 `H264MppEncoder : IVideoEncoder`，仅由 CMake 在 Linux+aarch64 且定义 `HWASIMIR_HAS_RKMPP` 时编译。Windows VS 工程不包含 MPP 源文件、头文件或库。
- 输入支持 BGR24、RGB24 和 Gray8，按既有垂直翻转标志转换到 NV12 / `MPP_FMT_YUV420SP`；宽高要求为偶数，stride 按 16 对齐。
- 持久复用 `MppCtx/MppApi/MppEncCfg`、DRM `MppBufferGroup`、输入 `MppBuffer` 和 `MppFrame`。编码配置为 `MPP_VIDEO_CodingAVC`、CBR、既有码率/FPS/GOP、Baseline、无 CABAC/8x8/B 帧；输出分片聚合到一个完整 Annex-B Access Unit。
- 首帧、编码器 reset、初始化/控制新回合和 TCP 重连沿用 `IVideoEncoder::requestKeyFrame()` 闭环，使用 `MPP_ENC_SET_IDR_FRAME`；配置 `MPP_ENC_HEADER_MODE_EACH_IDR`，并缓存 `MPP_ENC_GET_HDR_SYNC` 返回的 SPS/PPS。IDR AU 缺 SPS/PPS 时在内存中补入，随后校验 SPS/PPS/IDR NAL 均存在。
- `[VideoEncoder]`/`[CodecFallback]` 准确记录 requested/active backend；`[MppPerf]` 记录 `colorConvertMs/mppEncodeMs/payloadBytes/keyFrame/queueDepth`。
- 后端选择为：Windows auto = FFmpeg → JPEG；Linux/aarch64 auto = MPP → FFmpeg → JPEG；显式支持 `mpp|rk_mpp|ffmpeg|libavcodec|jpeg`。显式 MPP 失败不转 FFmpeg，只按 `H264FallbackToJpeg` 选择 JPEG 或丢帧，避免违反显式后端请求。
- CMake 增加 `HWASIMIR_ENABLE_RKMPP`、`RKMPP_ROOT`、`HWASIMIR_ENABLE_FFMPEG`。MPP 开启时严格检查：

```text
${RKMPP_ROOT}/usr/include/rockchip/rk_mpi.h
${RKMPP_ROOT}/usr/lib/aarch64-linux-gnu/librockchip_mpp.so
```

  缺失、非 Linux 或非 aarch64 均 `FATAL_ERROR`；成功时输出实际 header/library 路径。

#### TCP Packet v3 格式

TCP 外层仍以 4 字节网络序 `totalLength` 开始；初始化和控制转发仍保持原 `[totalLength][structLength][struct]` 格式。显示帧 v3 的包体固定为：

```text
[56-byte v3 header]
[optional realtime DisplayC2cObjTrackingData]
[optional annotation UTF-8 JSON]
[optional encoded video Access Unit/JPEG]
```

固定头所有多字节整数均为网络序：

| Offset | 大小 | 字段 |
|---:|---:|---|
| 0 | 4 | magic = `0x48575633` (`HWV3`) |
| 4 | 2 | version = `3` |
| 6 | 2 | headerBytes = `56` |
| 8 | 4 | section flags：bit0 realtime、bit1 annotation、bit2 video |
| 12 | 1 | codecId：0 none、1 JPEG、2 H.264 Annex-B |
| 13 | 1 | keyFrame |
| 14 | 2 | reserved |
| 16 | 8 | frameSeq |
| 24 | 8 | outputOrdinal |
| 32 | 8 | ptsMs |
| 40 | 4 | realtimeBytes |
| 44 | 4 | annotationBytes |
| 48 | 4 | videoBytes |
| 52 | 4 | reserved |

- `[TcpPayload] PacketVersion=3` 为当前默认；`SendVideo/SendAnnotation/SendRealtimeData/ForwardInitControl` 均默认 true，并支持同名环境变量覆盖。
- v3 codec 只由固定头决定，接收端不依赖 annotation JSON 分流。v2 继续从既有 annotation codec 字段识别 H.264，最旧的纯 JPEG 包也继续兼容。
- `SendVideo=false` 时发送线程不复制像素、不选择或执行 JPEG/H.264 编码；`SendAnnotation=false` 时不调用完整 annotation JSON 构造；`SendRealtimeData=false` 时不写入 tracking 结构体段。
- 三段全部关闭时不发送显示帧包，并且每次配置应用后只告警一次。`ForwardInitControl=false` 只关闭既有 init/control TCP 转发，不改变 UDP 处理。
- VideoDisplay 严格校验 magic/version/header size/flags/长度/codec 和 tracking 结构体大小；只有 HasVideo 才调用持久 JPEG/H.264 解码器。无视频包不报解码错误、不清空最后画面；是否存在 annotation/realtime 通过独立布尔值传递，录像侧不会用零结构体伪造缺失数据。
- 新增 `[TcpFramePacket]` / `[TcpFramePacketRx]` 低频字段：`packetVersion/flags/codec/keyFrame/realtimeBytes/annotationBytes/videoBytes/frameSeq/outputOrdinal/ptsMs`。关键帧到达时强制记录，不受 120 帧采样间隔影响。

#### Windows 构建与协议测试

```text
HwaSim_IR x64 Release + FFMPEG_ROOT                   PASS
DataDrivenTestQT Qt 5.12.12 / MinGW 7.3 x64 Release PASS
HwaSim_IR_VideoDisplay x64 Release + FFMPEG_ROOT     PASS
HwaSim_IR x64 Release（未设置 FFMPEG_ROOT）          PASS
Packet v3 固定头 round-trip/非法 flags-length       PASS
CommonData 三文件一致、Control.sensorID 不存在       PASS
```

最终 HwaSim_IR.exe SHA-256：
`6F2237C67024A71135AE37D645103D72323EF3DD71C60F987180FBC11EF93AC0`。

V4 Windows 分段矩阵使用 `tools/v4_packet_v3_acceptance.ps1` 和 loopback 配置。各 case 均生成独立 summary；首次整组运行暴露“IDR 到达时可能未命中 120 帧低频包日志”的诊断缺口，补为关键帧强制日志后，下列组合均通过：

| 场景 | version | flags | 结果/日志 |
|---|---:|---:|---|
| v2 JPEG（video+annotation+realtime） | 2 | 0x7 | PASS，`logs/v4-packet-v3-20260729-144421/v2_jpeg` |
| v2 H.264 | 2 | 0x7 | PASS，`logs/v4-packet-v3-20260729-145316` |
| v3 仅 JPEG 视频 | 3 | 0x4 | PASS，`logs/v4-packet-v3-20260729-144315` |
| v3 仅 H.264 视频 | 3 | 0x4 | PASS，`logs/v4-packet-v3-20260729-145222` |
| v3 H.264 + annotation | 3 | 0x6 | PASS，`logs/v4-packet-v3-20260729-145408` |
| v3 H.264 + realtime | 3 | 0x5 | PASS，`logs/v4-packet-v3-20260729-145504` |
| v3 H.264 + annotation + realtime | 3 | 0x7 | PASS，`logs/v4-packet-v3-20260729-145558` |
| v3 annotation + realtime，无视频 | 3 | 0x3 | PASS，`logs/v4-packet-v3-20260729-144421/v3_annotation_realtime` |
| v3 三段全部关闭 | 3 | 0x0 | PASS，`logs/v4-packet-v3-20260729-144421/v3_all_disabled` |

无视频两组确认发送端不配置编码器、JPEG/H.264 编码字节为 0，接收端不执行解码；全关闭组无显示帧包且告警恰好一次。v3 各组合的 header flags 和三段长度均由发送/接收日志交叉校验。

#### R1/R2/V1～V3 回归与性能摘要

- R1：`PASS`，`logs/r1-runtime-20260729-145921`。同一二进制 precise/coarse、Control/Init/Display ID 路由、255 广播、ACK 身份、禁用动态远端、simMode 1/2/非法回退全部通过。
- R2 JPEG 五组：全部 `PASS`，日志 `logs/r2-60fps-20260729-150008`、`150126`、`150243`、`150405`、`150521`；生产 precise/coarse 配置哈希前后一致。
- V1～V3 H.264 单 Visible、单 Headless、双 Visible：全部 `PASS`，日志 `logs/r2-60fps-20260729-150851`、`151005`、`151338`。实际 codec 均为 `h264_annexb`、发送端 JPEG 字节为 0、解码错误为 0、队列和 lag 不持续增长。
- TCP 重连、初始化/复位 IDR、不可用后端 JPEG 回退和 H.264→MP4：`PASS`，`logs/v3-h264-recovery-20260729-150710`；ffprobe 读取 760 帧。

| 场景 | 通道 | codec | output/display FPS | latency Avg/P95 ms | encode/decode ms | 平均 payload B |
|---|---|---|---:|---:|---:|---:|
| precise Visible Sync | precise | JPEG | 60.014 / 59.989 | 18.003 / 21.792 | 7.865 / 4.296 | 31,344 |
| precise Visible Async60 | precise | JPEG | 60.013 / 59.998 | 24.393 / 34.040 | 7.124 / 4.026 | 31,876 |
| coarse Visible Async60 | coarse | JPEG | 59.986 / 59.967 | 24.703 / 40.211 | 7.287 / 4.073 | 31,258 |
| dual Visible Async60 | precise | JPEG | 59.997 / 60.011 | 29.424 / 44.235 | 8.586 / 5.229 | 31,720 |
| dual Visible Async60 | coarse | JPEG | 60.005 / 60.001 | 29.134 / 42.015 | 8.576 / 5.231 | 32,261 |
| dual Headless Async60 | precise | JPEG | 60.054 / 59.994 | 26.172 / 35.662 | 5.685 / 4.977 | 29,987 |
| dual Headless Async60 | coarse | JPEG | 60.022 / 59.989 | 26.496 / 35.542 | 5.682 / 4.962 | 30,044 |
| precise Visible Async60 | precise | H.264 | 60.008 / 59.994 | 16.935 / 29.276 | 4.011 / 1.529 | 8,633 |
| precise Headless Async60 | precise | H.264 | 59.978 / 59.999 | 19.469 / 34.949 | 2.756 / 1.515 | 8,480 |
| dual Visible Async60 | precise | H.264 | 59.993 / 60.019 | 19.139 / 31.518 | 4.511 / 1.834 | 8,407 |
| dual Visible Async60 | coarse | H.264 | 60.009 / 60.004 | 18.594 / 31.843 | 4.079 / 1.755 | 992 |

所有正式性能组 input/output queue 最大深度均为 1；sourceSeqLag 最大为 1～2，未持续增长；跨通道 sensorID 拒绝通过。JPEG 性能没有明显回退。

#### CLion、部署脚本与未完成项

CLion 参数和手动流程见 `docs/HwaSimIR_V4_RK3588_MPP_CLion_Deployment.md`，核心参数为：

```text
-DHWASIMIR_ENABLE_RKMPP=ON
-DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp
-DHWASIMIR_ENABLE_FFMPEG=OFF
-DPANDA3D_ROOT=/opt/panda3d-aarch64
-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4
```

- `tools/rk3588_mpp_compile_check.sh` 编译最小 AVC MPP API 程序，并验证输出 ELF 为 aarch64。
- `tools/rk3588_v4_deploy_acceptance.sh build|deploy|run|verify` 提供 Debian 交叉编译、复制运行资产、板端显式 MPP 启动和日志断言；两份 shell 脚本已通过 `bash -n`。
- 尚未完成：Debian VM 上最小 MPP 编译、完整 CMake Release 交叉编译、RK3588 `/dev/mpp_service` 编码、三输入颜色验证、双进程 60 FPS、CPU/带宽/延迟实测、MPP 失败回退和板端重连/复位 IDR。必须由用户在已知 VM/板卡环境执行脚本并把日志回填，Windows FFmpeg 结果不能替代这些结论。
- 天气与照明仍未实施；V4 没有修改相关模型或配置。

### 2026-08-04 / V4 RK3588 MPP 1.3.8 兼容修正记录

基线为 `main@e3b18b4b9432f51501977fc030a9a30577d13961`。根因是原实现无条件使用新版
MPP 的 `mpp_buffer_sync_begin/end`，而目标板端 MPP 1.3.8 的 `mpp_buffer.h` 不声明
这两个 API，导致生产 `H264MppEncoder.cpp` 交叉编译失败。

#### 修正内容与文件

- `HwaSim_IR/HwaSim_IR/CMakeLists.txt`
  - 用编译并链接探测确认 `MPP_BUFFER_FLAGS_CACHABLE` 与 buffer sync API 同时可用；
  - 仅探测成功时定义 `HWASIMIR_MPP_HAS_BUFFER_SYNC`；
  - 探测失败明确打印 MPP 1.3.8 非缓存 DRM 路径，不静默关闭 MPP。
- `HwaSim_IR/HwaSim_IR/Video/H264MppEncoder.cpp`
  - MPP 1.3.8 使用 `MPP_BUFFER_TYPE_DRM`，通过 `mpp_buffer_get_ptr()` 直接填写 NV12，
    不引用 sync API；新版路径才使用 cachable DRM 和 sync begin/end；
  - `MppCtx/MppApi/MppEncCfg/MppBufferGroup/输入 MppBuffer` 持久复用；每帧创建并设置
    `MppFrame`，`encode_put_frame` 后立即释放，保持既有 Annex-B 分片聚合、SPS/PPS、
    IDR、码率/FPS/GOP 逻辑；
  - 首个真实 AU 成功后输出一次 `[H264EncodeSuccess]`，init/reset/TCP reconnect
    请求关键帧后允许再次输出。
- `HwaSim_IR/HwaSim_IR/Video/VideoEncoder.h`
  - 增加 MPP 成功日志的一次性状态，不改变通用帧接口。
- `HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/Video/H264FfmpegDecoder.cpp`
  - 仅在 FFmpeg 真正输出非空 `QImage` 后打印 `[H264DecodeSuccess]`。
- `tools/rk3588_mpp_compile_check.cpp`、`tools/rk3588_mpp_compile_check.sh`
  - 从少量 API 探针改为直接编译、链接生产 `H264MppEncoder.cpp`，覆盖生产源实际
    使用的 MPP API，并打印与 CLion 一致的三个核心 CMake options。
- `tools/rk3588_v4_deploy_acceptance.sh`
  - 板端 verify 增加真实 MPP IDR/SPS/PPS/payload 成功日志断言。
- `docs/HwaSimIR_V4_RK3588_MPP_CLion_Deployment.md`
  - 补充 1.3.8/新版分支、预期 CMake 输出和端到端成功日志说明。

未修改 `CommonData.h`、Packet v3 固定头/三段格式、UDP、红外物理、分辨率、
Windows 传输矩阵、天气或照明，也未修改 `.idea`。

#### Windows Release 与回归

- `HwaSim_IR` Release x64（FFmpeg 8.1 SDK）：PASS；
- `DataDrivenTestQT` Qt 5.12.12 Release：PASS；
- `HwaSim_IR_VideoDisplay` Release x64（FFmpeg 8.1 SDK）：PASS；
- Packet v3 header 静态检查：PASS，header 56 bytes，magic `0x48575633`；
- v2 JPEG/H.264 与 v3 九组传输矩阵：9/9 PASS，汇总
  `logs/v4-packet-v3-20260804-171657/summary.json`；
- 五个 H.264 视频组合均记录真实 `[H264DecodeSuccess]`，800x800、keyFrame=true，
  首次解码耗时为 2.465～11.127 ms；
- TCP 重连 IDR、初始化/复位 IDR、不可用后端 JPEG fallback、解码后 MP4：PASS，
  `logs/v3-h264-recovery-20260804-172230`，MP4 读取 778 帧。

#### 未完成项

当前 Windows 环境没有 Debian VM/RK3588 主机入口，因此没有执行或宣称 aarch64
交叉编译、MPP 1.3.8 链接或板端硬件编码通过。用户仍需在
`/home/linaro/userdata/HwaSimIR` 执行 `tools/rk3588_mpp_compile_check.sh` 和
`tools/rk3588_v4_deploy_acceptance.sh build|deploy|run|verify`，并留存
`[H264EncodeSuccess]`、Windows `[H264DecodeSuccess]`、60 FPS、延迟与队列日志。

### 2026-08-17 / RK3588 H.264/Mali 基线固定与 60 FPS 性能收口记录

#### 固定基线与范围

- 工作区基线为 `main@fbac7275558b004498a7fa6c036c3007e5abd871`，全过程未 commit、未 push、未修改 `.idea`。
- 固定板端链路为最小 Xorg `:0` + `HeadlessOffscreen` + `GL_VENDOR=ARM` + `GL_RENDERER=Mali-LODX` + `hardwareGpu=1` + MPP H.264 + Packet v3 `flags=0x7`。
- 运行配置保持 `H264Encoder=mpp`、`H264FallbackToJpeg=false`、视频/标注/realtime 三段全开；800×800、红外物理、H.264、标注意义和 TCP/UDP 协议均未改变。
- 基线清单、SHA-256、源码 patch 和两轮 45 秒数据保存于 `logs/rk3588-60fps-closeout-20260817-173046`。

#### 修改文件与有效优化

```text
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
tools/rk3588_hwasimir_performance_mode.sh
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

- 增加真实 `[GpuBackend]` 和可按需开启的 render/IR/capture 分段计时，确认单 Stage6 `direct_final`、Panda core 约 13.2 ms、capture 约 2.5～2.9 ms；生产正式轮关闭深度 `RenderPerfProbe`，保留低频汇总。
- 目标可见性 `show/hide` 只在状态变化时写入，重复写入从每帧 13 次降为 0；每帧末仍隐藏本包未映射目标。
- 修复 Async 模式无条件绕过 `IRUpdateHz` 的问题；状态变化仍立即刷新，目标姿态、实时协议处理和视频输出仍保持 60 Hz 调度。
- 标注几何更新由 15 Hz 调整到 10 Hz，跳过帧复用最近有效几何；标注 JSON、realtime 和视频三段仍逐帧发送，Packet v3 格式与语义不变。
- 新增非持久 governor 辅助脚本，保存原 CPU/GPU governor 后启用 performance，并可恢复；不修改驱动、频率表、网络或开机服务。

#### 性能数据

| 组别 | output FPS | display FPS | renderMs | sceneMs | IR ms | annotation ms | readback ms | preprocess ms | MPP ms | CPU |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| baseline-01 | 55.004 | 55.048 | 17.402 | 0.710 | 0.831 | 0.570 | 2.082 | 11.478 | 2.123 | 152.4% |
| baseline-02 | 55.818 | 55.943 | 17.119 | 0.728 | 0.876 | 0.570 | 1.944 | 10.819 | 2.127 | 151.7% |
| final 60s-01 | 59.028 | 59.084 | 16.383 | 0.483 | 0.486 | 0.336 | 1.804 | 8.145 | 2.103 | 137.8% |
| final 60s-02 | 58.761 | 58.800 | 16.454 | 0.488 | 0.498 | 0.335 | 1.840 | 8.089 | 2.096 | 147.9% |

- 两轮最终测试均预热 5 秒后正式统计 60 秒；平均 output/display 为 `58.895/58.942 FPS`，较基线均值提高 `3.484/3.446 FPS`。
- 第一轮满足 `>=59`，第二轮未满足，因此结论为“接近 60 FPS，但尚未稳定达到验收线”，没有通过降低质量、分辨率或物理精度掩盖问题。
- 最终两轮 input queue 最大 1、output/TCP queue 最大 0、sourceSeqLag 最大 1、dropped=0；有效流平均延迟约 22 ms、最大约 32 ms。三端未离线校时，不声明跨机 P95。
- MPP encode 约 2.10 ms，NV12 preprocess 约 8.12 ms 且在独立线程，队列不增长；当前首要瓶颈仍是 Panda/Mali render 主路径，而非 MPP。

#### 构建、恢复回归与撤销项

- Windows 三个 Release 构建：PASS；VM 生产 MPP API compile check：PASS；最终 ELF 为 AArch64 且 `NEEDED librockchip_mpp.so.1`。
- 最终 AArch64 SHA-256：`b36108625a426e1d8584ebe28f0ea093ce9d469ac75bd3f7aecacb073f9c5577`。
- TCP 重连/再次初始化恢复：PASS。同一板端进程记录 7 次真实 MPP `[H264EncodeSuccess]`、2 次 TCP connected IDR 请求、2 个 init；Windows 重连前后均有 `[H264DecodeSuccess]`，CodecFallback=0。
- `TargetUpdateCullInvisible=true`、`IRUpdateHz=20` 和帧 vector 回收池均因没有稳定端到端收益而撤销；未叠加无效修改。
- 剩余最大三项：Panda cull/draw 约 13.2 ms；readback+800×800 RGB copy 约 2.5 ms；编码线程 NV12 preprocess 约 8.1 ms。后续如继续优化，应先处理前两项；不建议在队列稳定时优先引入 RGA/DMA-BUF 大改。

### 2026-08-18 / W1 世界空间纹理云层与红外云实施记录

#### 范围、参考与架构

- 基线为 `main@8f16293afb782833abc0e45ecb966f224d385963`，未 commit、未 push、未修改 `.idea`，未实施雨、雪、雾、体积云、ray marching、天气照明或 SilverLining。
- 只阅读本机合法安装内容：`D:\Presagis\Suite22\Ondulus_IR_22_0` 与 `D:\Presagis\Suite22\Vega_Prime_22_0` 的公开 docs/include/samples/appdata/config；未反编译、未复制私有实现。参考结论是 2D 云层应有世界高度、底/顶边界、覆盖率、风场和观察者无关纹理坐标；3D/SilverLining 只作为后续规划。
- 旧的 `m_cameraNode.attach_new_node(...)` 相机前固定 Cloud Card、FOV 撑屏、`depth_test=false`、`CloudBaseDistanceM/CloudLayerSpacingM` 正式路径已删除。Stage7 现有 3D Sky Dome 和 Lower Hemisphere Shell 保持不变，`UseReal3DBackground=1` 仍是默认。
- 新增统一 `CloudRenderMode`：`World2D` 为一层世界平面；`Layered2_5D` 为 2～3 层水平切片，默认 3 层；`Volumetric3D` 仅保留枚举和明确禁用日志，本轮没有生产 raymarch 代码。
- 云几何挂在 `m_renderRoot`，默认云底为 `m_stage7GroundReferenceZ + 2500 m`，厚度 800 m；30 km 世界网格按 5 km 固定网格围绕相机吸附，纹理 UV 来自绝对 world XY，因此相机运动不会带动纹理贴屏滑动。网格使用固定节点，Clear 只隐藏、不反复创建销毁。
- 修复 WGS84 到局部 ENU 建立参考点后的高度重基准：记录初始平台海拔并将 Stage7 地面参考转换为局部坐标，避免相机归零后云层仍错误保留在绝对 `+2500 m`。低频 `[WeatherCloudGrid]/[WeatherCloudSpatial]` 可验证世界位置、投影和穿云因子。

#### 云形状与红外模型

- 纹理来自 `weather_textures.json` 的 `cloud_few/cloud_scattered/cloud_overcast/cloud_storm/cloud_cumulus`，由路径级缓存和 Panda `TexturePool` 复用；同一纹理只在首次加载或天气切换时输出 `[WeatherTextureLoaded]`。
- 每层 shader 以绝对世界坐标采样同一缓存纹理的 base/detail 两级频率，叠加各层独立 scale/offset/weight，再以 coverage threshold 和 edge softness 得到密度。Overcast 使用非线性 coverage 阈值，避免 luminance mask 饱和成均匀灰膜，同时保留高覆盖。
- PNG alpha/luminance 只生成云密度 `M`，不直接作为 MWIR/LWIR 灰度。红外合成为：`opticalDepth=profileOpticalDepth*M`、`tau=exp(-opticalDepth)`、`Lout=tau*Lin+(1-tau)*Lcloud`、`alpha=1-tau`。
- `Lcloud` 复用 `IRRadianceModelV2` 的波段中心与 Planck 辐射计算；`cloudTemperatureK`、当前 sensor band、天空背景辐射和 profile 共同决定云灰度，不修改目标温度或 Stage3～Stage6 目标物理。VIS/NIR/SWIR 保留更明显纹理反射变化，MWIR/LWIR 使用云自身辐射与透射衰减。
- 10 Hz 天气更新只在状态变化时重写常量 shader 状态，世界 UV 时间仍逐帧更新；移除了同一更新周期内重复的 Stage6 最终参数写入。板端稳态 weather CPU 为 `0.015～0.178 ms`。

#### 修改文件

```text
DataDrivenTestQT/main.cpp
DataDrivenTestQT/mainwindow.cpp
DataDrivenTestQT/mainwindow.h
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/HwaSim_IR/IR/IRWeatherEffects.cpp
tools/stage7_weather_check.ps1
tools/stage7_weather_perf_check.ps1
tools/stage7_weather_perf_smoke.ps1
tools/w1_cloud_windows_smoke.ps1
tools/w1_world_cloud_imaging_test.ps1
tools/w1_cloud_rk3588_acceptance.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

DataDrivenTestQT 只增加测试 CLI 对既有字段和 `1.txt` 的控制，并修复自动测试首帧用默认零值而非文件首条实时数据的问题；没有增加协议字段。`CommonData.h`、Packet v3、UDP、800×800、标注语义、H.264/MPP 和 Stage3～Stage6 均未修改。

#### Windows 成像与构建

- `HwaSim_IR` x64 Release + FFmpeg：PASS；DataDrivenTestQT Qt 5.12.12 Release：PASS；HwaSim_IR_VideoDisplay x64 Release + FFmpeg：PASS。
- `World2D`：PASS，日志与图像 `logs/w1-world-cloud-imaging-20260818-175140`。
- `Layered2_5D` Clear/Cloudy/Overcast：PASS；最终 Quick 回归 `logs/w1-world-cloud-imaging-20260818-185034`，完整波段/温度矩阵 `logs/w1-world-cloud-imaging-20260818-174359`。
- 同一路径下 cloud-front 使用 500 m 云底、cloud-behind 使用 2500 m 云底：前方目标不被云覆盖，后方目标出现云密度和辐射衰减。移动输入的 MP4 中世界纹理相对画面连续运动，无相机贴屏锁定。
- MWIR `240 K/260 K` A/B 的最终帧 YAVG 为 `87.916/88.128`；NIR/LWIR 为 `92.513/86.127`，证明云灰度来自温度/波段模型而非固定 PNG RGB。最终 Overcast 结构图在 `logs/w1-world-cloud-imaging-20260818-183951`。
- Windows 所有正式图像包均为 H.264 Annex-B、Packet v3 `flags=0x7`，发送端无 CodecFallback，VideoDisplay 有真实 `[H264DecodeSuccess]`。

#### RK3588 实机结果

板端保持无 default route；使用最小 Xorg `:0`、HeadlessOffscreen、`GL_VENDOR=ARM`、`GL_RENDERER=Mali-LODX`、`hardwareGpu=1`、MPP H.264、Packet v3 三段全开。VM 的 MPP 生产 API 检查和完整 Release 交叉构建均通过，最终 ELF 为 AArch64 且 `NEEDED librockchip_mpp.so.1`，最终 SHA-256 为 `c441bf0819dadfabe6c952097802ffb5d792f6e87242441f30f0efb61ec94b55`。

三层正式矩阵每组预热 5 秒、统计 30 秒：

| 天气 | render FPS | output FPS | display FPS | render ms | weather ms | readback ms | preprocess ms | MPP ms | CPU |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Clear | 60.005 | 60.004 | 60.644 | 15.925 | 0.178 | 1.761 | 8.105 | 2.061 | 140.3% |
| Cloudy | 58.882 | 58.882 | 59.394 | 16.434 | 0.039 | 1.767 | 8.074 | 2.028 | 145.0% |
| Overcast（最终阈值） | 59.138 | 59.138 | 59.691 | 16.363 | 0.058 | 1.758 | 8.064 | 2.015 | 143.4% |

- 相对同轮 Clear，Cloudy/Overcast render 增量约 `+0.509/+0.438 ms`；weather CPU 达到 `<=0.2 ms`。Cloudy 同配置复测为 `58.108 output / 58.630 display FPS、16.649 ms`，说明 59 FPS 目标尚未稳定达到，而不是单次统计误差；未通过降低分辨率、关闭红外/标注或修改 H.264 掩盖。
- 两层透明 slice A/B 没有稳定收益，已撤销并恢复 3 层正式配置；没有叠加无效优化。
- 三组均有 `[H264EncodeSuccess] backend=mpp`、Windows `[H264DecodeSuccess] backend=ffmpeg`、SPS/PPS+IDR、Packet v3 `flags=0x7`；input queue 最大 1、output queue 最大 0、sourceSeqLag 最大 1、dropped=0、CodecFallback=0。
- TCP 重连回归 PASS：同一板端进程记录 4 次 `[H264EncodeSuccess]`，Windows 接收端重启后重新真实解码；日志 `logs/w1-world-cloud-rk3588-20260818-175756-final/tcp_reconnect`。
- 正式矩阵见 `logs/w1-world-cloud-rk3588-20260818-175756-final`，最终阈值补测见 `logs/w1-world-cloud-rk3588-20260818-175756-final-shader`。测试后已恢复板端原 runtime/profile 与 CPU/GPU governor，保留最小 Xorg 基线和最终 W1 ELF；网络始终只有 `192.168.1.0/24`。

#### 已知限制与后续入口

- 当前是大尺度世界平面/切片，不包含真实体积内部散射、云影、自遮挡和云顶/云底独立温度场；透明排序仍采用 Panda 常规 back-to-front，不引入 OIT。
- 下一阶段可定义为 W1.5/W3 `Volumetric3D Cloud`：低分辨率 3D noise、weather volume、adaptive raymarch、temporal reprojection、cloud shadow 与分层温度。W1 未实现任何生产 raymarch 代码。
- W2 的雨、雪、雾仍未实施。

### 2026-08-24 / W1.5 Streamed World-Space Volumetric Clouds 实施记录

#### 范围与世界网格架构

- 实施基线为 `main@d7b1b8750b9c023710c7b040a717b375eb30486a`；未 commit、未 push、未修改 `.idea`，未实施雨、雪、雾、天气照明，也未改变 CommonData、UDP、Packet v3、Stage3～Stage6 红外目标物理、800×800 或标注语义。
- 新增 `StreamedWorld3D` 模式；`Volumetric3D` 继续作为兼容 renderer 名称，但 placement/lifecycle 同样使用 `StreamedWorld`。W1 的 `World2D/Layered2_5D` 仍承担远场云，3D 云只负责目标附近的近中距离体积感。
- 跟踪目标仅作为 streaming center。按完整协议键 `targetType + targetPlatID + targetID` 解析目标位置，目标节点不作为云父节点；所有云体代理均挂在 `m_renderRoot/Stage7VolumeRoot`。目标移动只改变附近哪些固定 world cell 被加载，不改变 descriptor 的世界位置。
- 新增独立的 `IRWorldCloudStreaming`：`floor(worldXY/CellSizeM)` 得到 cell，随后对 `cellX/cellY/WeatherSeed/weatherProfile` 做稳定 64 位 hash。该 seed 决定 hasCloud、世界位置、高度、非均匀半径、密度、共享模板、温度偏移、旋转和 noise offset；运行中不调用逐帧 `rand()`。
- `StreamingRadiusM=6000`、`DeactivationRadiusM=7500` 形成 1500 m 滞回区；固定 8 节点对象池负责 activate/fade/deactivate/reuse，不按 cell 创建销毁 NodePath。边界 fade 调制 density/optical depth，非白色 alpha 硬切。
- 内置 `--w15-cloud-model-check` 和 `tools/w15_world_cloud_model_check.ps1` 验证同一 cell 在卸载/重载后 descriptor 完全一致；实测 result=PASS、candidateCount=17、hysteresis=1500 m。

#### 3D 密度、raymarch 与红外积分

- 初始化时一次生成并缓存 4 个共享 `32×32×32 R8` density template。低频 shape、detail noise、cavity 与 ellipsoid edge falloff 共同形成内部空洞和柔边；cloudId 只选择模板并改变 scale/rotation/noise offset，不逐云、逐帧生成 3D texture。
- 每个 active 云体使用非均匀 ellipsoid proxy，fragment 先做 ray-volume intersection，只在 proxy 内 raymarch；Near/Medium/Far 默认 `10/7/4` steps，按距离与投影尺寸降级，`T<0.03` 时提前终止，没有 fullscreen volumetric pass。
- opaque raw scene 先写 GPU depth，体积云合成再采样该 depth，把积分终点限制在场景表面之前；没有 GPU→CPU depth readback。RK3588 g6p0 的 `eglGraphicsPipe` 只有在已有 host/GSG 时才能创建 `BF_can_bind_every` raw FBO，因此 Headless+3D 路径先创建普通 final color host，再创建共享 GSG 的 raw color+depth 输出。短探针确认 `rawBufferReady=1/finalBufferReady=1` 且无 `GL_INVALID_*`。
- 每段积分使用 `T *= exp(-sigma*density*stepLength)`；最终 alpha 为 `1-T`，云辐射灰度复用 W1 的 `IRWeatherEffects::cloudEmissionGray` 与现有 Planck/band 映射。`cloudTemperatureK + descriptor.temperatureOffsetK` 决定 MWIR/LWIR 云自身辐射，不改变目标温度。
- 3D 可见时远场 Layered2_5D contribution 调到 0.72，避免同一区域过度叠加；3D 不可见时恢复 1.0。

#### 修改文件

```text
HwaSim_IR/HwaSim_IR/IR/IRWorldCloudStreaming.h
HwaSim_IR/HwaSim_IR/IR/IRWorldCloudStreaming.cpp
HwaSim_IR/HwaSim_IR/HwaSimIR.h
HwaSim_IR/HwaSim_IR/HwaSimIR.cpp
HwaSim_IR/HwaSim_IR/IR/IRWeatherEffects.h
HwaSim_IR/HwaSim_IR/IR/IRWeatherEffects.cpp
HwaSim_IR/HwaSim_IR/main.cpp
HwaSim_IR/HwaSim_IR/CMakeLists.txt
HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj
HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj.filters
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/Bin/Config/Weather/weather_profiles.json
tools/w15_world_cloud_model_check.ps1
tools/w15_streamed_cloud_windows_smoke.ps1
tools/w15_streamed_cloud_rk3588_acceptance.ps1
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
```

`weather_profiles.json` 只增加各天气的 `volumeCloudProbability/volumeCloudDensityScale`；Clear 为 0，Cloudy/Overcast 分别为 0.35/0.65。最终业务概率与密度不硬编码在 renderer 中。

#### Windows 构建、运行与成像

- 三个 Windows Release 构建 PASS。SHA-256：HwaSim_IR `FF2F54A98C0D26B23FE09ED91699822689F7E553DF16E2F3AFFB883C67CBBE6B`；DataDrivenTestQT `4455559C26D33663B2BB9B7C8198A6BE2E6D15FB6D588D3AD44E178E533C0F43`；VideoDisplay `4D6A0B3873BCF20DA72A9B736C4E27FA6D48E40AC215356DB4B5CBBDF08A4B46`。
- 45 秒 MWIR 往返多 cell 闭环 PASS：`logs/w15-streamed-cloud-windows-20260824-141348`。共 36 次 activate，12 个 cloudId 在 deactivate 后按原 ID/seed/position 重新 activate；renderer、4 个共享 density template、目标 streaming center、GPU depth、可见云、FFmpeg H.264 编解码、Packet v3 和无 fallback 全部通过。
- 离线空间检查在同一日志中得到目标到活动 ellipsoid 的最小归一化距离 0.831（小于 1），证明轨迹实际进入云体；depth texture 把目标表面之后的积分截断。往返录像与远离/接近/穿越/离开抽帧保存在 `logs/w15-streamed-cloud-windows-20260824-141348/frames`。
- MWIR 240 K / 260 K 使用相同 seed、轨迹、H.264 和 Packet v3，仅修改测试 profile 副本：两组分别见 `logs/w15-streamed-cloud-windows-20260824-141810` 与 `141923`。日志的 `cloudGray` 从 `0.271327` 增到 `0.339889`；对照图为 `logs/w15-mwir-temperature-ab-20260824/mwir_240K_vs_260K.png`，证明 3D 云亮度响应云温而非 PNG RGB。
- W1 原有 Layered2_5D 板端基线本轮继续 PASS。最新 Windows World2D smoke 的 Clear/Overcast、世界空间、H.264/Packet v3 均通过；Cloudy 自动切换因工作区中用户未提交的 DataDrivenTestQT 修改把 init `envSky` 固定为 5 而无法形成有效 Cloudy case，该失败不归因于 W1/W1.5 renderer，且没有覆盖这项用户修改。

#### RK3588 实机与性能

- VM 生产 MPP API 检查和完整 Release 交叉构建 PASS；最终 ELF 为 AArch64，`NEEDED librockchip_mpp.so.1`，部署二进制 SHA-256 为 `6c0a3e24883dfeea1ded0c0313298bf8f6ff87fc0b64f85000ffc9e20d8cb55c`。
- 板端全过程无 default route，只保留 `192.168.1.0/24`；使用最小 Xorg `:0`、HeadlessOffscreen、`GL_VENDOR=ARM`、`GL_RENDERER=Mali-LODX`、`hardwareGpu=1`、MPP H.264、Packet v3 `flags=0x7`。
- 100 µrad/pixel 正式四组矩阵（5 秒预热、30 秒统计）见 `logs/w15-streamed-cloud-rk3588-20260824-130715`：

| 组别 | render FPS | output FPS | display FPS | render ms | weather ms | readback ms | preprocess ms | MPP ms | CPU |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Layered2_5D only | 57.185 | 57.186 | 57.764 | 16.944 | 0.046 | 1.734 | 8.059 | 2.005 | 141.8% |
| StreamedWorld3D MaxVisible=1 | 60.003 | 60.002 | 60.555 | 16.050 | 0.264 | 1.769 | 8.070 | 2.048 | 139.9% |
| StreamedWorld3D MaxVisible=2 | 59.972 | 59.970 | 60.505 | 16.122 | 0.267 | 1.784 | 8.084 | 2.052 | 143.6% |
| StreamedWorld3D MaxVisible=4 | 60.022 | 60.021 | 60.556 | 16.034 | 0.281 | 1.764 | 8.051 | 2.072 | 139.9% |

- 修正后的四次往返轨迹 MaxVisible=1 复测见 `logs/w15-streamed-cloud-rk3588-20260824-142356`：`output/display=59.907/60.435 FPS`、render 16.013 ms、28 次 activate/20 次 deactivate、8 个 cloudId 重载 identity PASS、queue 最大 1、sourceSeqLag 最大 1、dropped=0。
- 1000 µrad/pixel 宽视场补测见 `logs/w15-streamed-cloud-rk3588-20260824-142846`：MaxVisible 1/2/4 的 output 为 `59.861/60.005/60.005 FPS`，display 为 `60.424/60.572/60.537 FPS`，三组均有 8 个 deterministic cloud reactivation。实际轨迹和随机分布同屏仍最多命中 1 个云体，因此 MaxVisible=2/4 的配置与生命周期已测试，但“同时 2/4 个 proxy 产生 fragment”的最坏 GPU 上限没有被本场景饱和，不能据此虚构 4 云体满屏成本。
- 所有 3D 正式组均有 `[H264EncodeSuccess] backend=mpp`、Windows `[H264DecodeSuccess] backend=ffmpeg`、Packet v3、CodecFallback=0、output queue=0、dropped=0。个别 Layered/MaxVisible=1 轮出现一次 `sourceSeqLagMax=2`，未持续增长；最终 MaxVisible=2/4 与往返 MaxVisible=1 为 1。

#### 结论与已知限制

- W1.5 的 world-grid streaming、deterministic seed、滞回、对象池、共享 3D density、局部 raymarch、GPU scene-depth 截断、W1 红外辐射复用及距离/屏幕 LOD 均已闭环。推荐 RK3588 默认 `MaxVisibleVolumes=1～2`。
- 当前透明合成是单次吸收/云自身辐射近似，没有多次散射、云影、temporal reprojection 或天气 volume；这些属于后续增强，不在本轮扩展。
- streaming 轨迹快速跨 cell 时低频 weather/streaming CPU 平均约 0.26～0.37 ms；W1 Layered2_5D 本身仍约 0.046 ms。若后续要求 3D streaming CPU 也严格低于 0.2 ms，应先减少 10 Hz 查询中的 descriptor 重算/日志，而不是降低 800×800、红外质量或 H.264 功能。
