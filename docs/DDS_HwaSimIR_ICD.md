# HwaSimIR DDS 接口控制文档（F1）

本文件冻结 F1 的 DDS 控制面、状态面和视频接口。Legacy UDP `0x41/0x36/0x38/0x37`
及 TCP Packet v3 保留；DDS 是可并列选择的 Transport，不改变原协议布局。

## 1. 中间件与 QoS

- SDK 路径标签：ZRDDS 2.4.5；Windows/板端实测 runtime banner：`2.4.4-r6873577`。
- 默认 Domain：`150`；Transport：`tcpv4`。
- Control、Init、Realtime、InitAck：`RELIABLE_RELIABILITY_QOS` + `KEEP_ALL_HISTORY_QOS`。
- VideoStatus：RELIABLE + KEEP_LAST depth=1。
- Video：RELIABLE + KEEP_ALL。
- 客户通用配置：`Config/DDS/ZRDDS_PROTOCOL_QOS.xml`，网卡为 `tcpv4://default//0`。
- 现场多网卡选错时可使用测试专用绑定文件；不得将现场 IP 写死进通用代码。
- 禁止 BEST_EFFORT 和 ZRDDS UDP 大包零拷贝。

## 2. 类型与 Topic

正式 IDL 位于 `DDS/IDL/HwaSimIRProtocolV1.idl`，命名空间为 `HwaSimIRDds`。

| Topic | Type | Key | Legacy 对应 |
|---|---|---|---|
| `HwaSimIR.Control` | `ControlCommandV1` | `platID` | UDP 0x41 |
| `HwaSimIR.Init` | `InitCommandV1` | `platID,sensorID` | UDP 0x36 |
| `HwaSimIR.Realtime` | `RealtimeDataV1` | `platID,sensorID` | UDP 0x38 |
| `HwaSimIR.InitAck` | `InitAckV1` | `platID,sensorID` | UDP 0x37 |
| `HwaSimIR.VideoStatus` | `VideoStatusV1` | `platID,sensorID` | F1 新增状态 |

`sensorID=255` 广播仍由 HwaSimIR 应用层判断。DDS typed sample 不依赖 `flag`
反序列化，但保留 0x41/0x36/0x38/0x37，便于 UDP/DDS A/B 和语义审计。

## 3. 字段映射

转换代码位于 `DDS/Protocol/CommonDataDdsAdapter.cpp`，逐字段赋值，禁止 `memcpy`
DDS object。

| Legacy struct | DDS type | 字段组 |
|---|---|---|
| `ControlP2cX1ObjTrackingCmd` | `ControlCommandV1` | flag, JB, platID, simCommand, roundCut, currentRound |
| `InitP2cObjectTrackingCmd` | `InitCommandV1` | flag/JB/platID/sensorID、platParamInit、trackingInit、7 个 MissileMaxCount |
| `DisplayC2cObjTrackingData` | `RealtimeDataV1` | flag/platID/sensorID/time、platLoc、weaponState、targetNumValid、targetState[5] |
| `InitAckC2pObjectTrackingCmd` | `InitAckV1` | flag, JB, platID, sensorID, trackingReady |

嵌套数组保持 `trackerSensor[1]`、`targetState[5]`、`xxOutAng[2]`、`offsetAng[2]`。
Legacy packed size 必须保持 24/385/506/17 bytes。

## 4. VideoStatusV1

字段：`platID`、`sensorID`、`channel`、`running`、`codec`、`pixelFormat`、
`videoTopic`、`width`、`height`、`fps`、`bitrateKbps`、`gopFrames`、
`compressed`、`currentRound`。

HwaSimIR 在 DDS ready、INIT、START、STOP、codec/topic/geometry 改变及 1 Hz 刷新时发布。
START 为 `running=true`，STOP 为 `running=false`。Receiver 应等待一个 running Status，
再按其中 Topic 和 geometry 创建视频 Reader；CLI override 只用于调试。

## 5. 视频 Topic

| Channel | 格式 | Topic |
|---|---|---|
| precise | H264 | `HwaSimIR.Video.precise.H264` |
| precise | Gray8 | `HwaSimIR.Video.precise.RawGray8` |
| precise | BGR24 | `HwaSimIR.Video.precise.RawBGR24` |
| coarse | H264 | `HwaSimIR.Video.coarse.H264` |
| coarse | Gray8 | `HwaSimIR.Video.coarse.RawGray8` |
| coarse | BGR24 | `HwaSimIR.Video.coarse.RawBGR24` |

视频 Type 始终仅为内置 `DDS::Bytes`。VideoStatus 是独立 typed Topic，不嵌入视频 Sample。

### H264

一个 DDS Sample 精确等于一个完整 Annex-B Access Unit。无长度前缀、自定义 header、
frameSeq、PTS、geometry、codec 或任何 TCP Packet v3 字段。接收端按 Sample 顺序原样
append 即可重组 `.h264`。

### Raw

一个 DDS Sample 精确等于一整帧。RawGray8 必须为 `width*height` bytes；RawBGR24 必须为
`width*height*3` bytes，紧密 BGR 排列、无行 padding。geometry 来自 VideoStatus，尺寸不符
必须报错，不得猜测。

## 6. Transport 与业务状态机

HwaSimIR `[CommandTransport] Input=udp|dds|both`；Ack 可为
`match_input|udp|dds|both`。两种入口最终进入同一套 `handleControlCmd`、`handleInitCmd`、
`handleDisplayData`。DDS callback 只复制、adapter、route/queue，不直接操作 Panda3D。

both 模式去重键：Control=`platID/simCommand/currentRound/roundCut`；Init=语义 hash；
Realtime=`platID/sensorID/time`。默认窗口 1000 ms。被判为 duplicate 的消息不再次执行
业务，也不产生第二份业务 Ack。

## 7. 可靠停止

STOP 停止新视频/标注输出并 flush Local MP4；DDS 视频应用 queue 先 drain，Writer/Participant
可跨回合复用。`wait_for_acknowledgments()` 不能作为尾帧到达的唯一证据，必须配合 bounded
drain 和 sender/receiver Sample 计数。

## 8. D3/F1 已验证结果

D3 生产测试已验证 precise DDS H264 689/689、TCP+DDS 716/716、TCP+DDS+record
684/684、RawGray8 622/622、双通道 377/377 和 372/372、20 round 尾帧一致。

F1 控制闭环已验证 VS2015 Stim DDS RESET/INIT/Ack/START/360 realtime/STOP；MinGW Stim
60 realtime；DataDriven both 中每对 UDP+DDS 仅接受一次。板端新 Customer Receiver 实测：
H264 30 Samples/2,887 bytes/0 error，RawGray8 10 Samples/6,400,000 bytes/0 error，均由
VideoStatus 自动选择 Topic/geometry。

## 9. 厂商已知事项

1. 安装目录标称 2.4.5，但 runtime banner 为 2.4.4-r6873577。
2. `wait_for_acknowledgments()` 可能早于接收应用完成最后 Sample drain 返回。
3. CAEP Trial runtime 会改写 licence；每个并发进程必须使用独立、可写的 licence 副本。
4. RK3588 显式绑定 `192.168.1.116` 时 discovery 异常；当前现场验收使用 `tcpv4://default//0`。
5. 2.4.4 runtime 在同板双 HwaSimIR 共享协议 Topic 的测试中出现一路收不到广播及 `Send message failed:18`；在厂商确认前，双通道不得仅凭单路计数验收。

F1 没有 Annotation、VideoFrameMeta、Gateway 或 DDS 视频自定义 IDL；这些只能在 F2 单独设计。

## 10. F2 帧同步接口

F2 保持视频数据为内置 `DDS::Bytes`，不向 H264 AU 或 Raw 帧中加入头。帧身份由两个独立 typed Topic 给出：

| Channel | VideoMeta Topic | Annotation Topic |
|---|---|---|
| precise | `HwaSimIR.VideoMeta.precise` | `HwaSimIR.Annotation.precise` |
| coarse | `HwaSimIR.VideoMeta.coarse` | `HwaSimIR.Annotation.coarse` |

`VideoFrameMetaV1` 包含 `platID/sensorID/channel/frameSeq/currentRound/ptsMs/keyFrame/codec/width/height`。`AnnotationFrameV1` 包含相同帧身份和不超过 32768 字节的 JSON。两者 key 均为 `platID,sensorID`。

每次 START 将输出序号归零，首个实际输出帧为 1。同一逻辑帧的 DDS video、VideoMeta、Annotation、TCP Packet v3 和 Local MP4 输入共用序号。跨 Topic 到达顺序不构成协议顺序；接收端必须按 `currentRound + frameSeq` 建立小型 pending map。`realtimeAnnotation=false` 时可以不发布 Annotation，但 VideoMeta 仍逐帧发布。

STOP 依次停止新业务帧、完成已进入输出队列的帧、排空 video/meta/annotation、flush/close MP4 和本地文件，最后发布 `VideoStatus.running=false`。下一 START 建立新 round，`frameSeq` 再从 1 开始。

## 11. F2 Decode Gateway

生产直传仍优先使用 `HwaSimIR.Video.<channel>.H264`，一 AU 一 Sample。需要 Raw 的客户可在 RK3588 部署：

`H264 DDS -> RKMPP decode -> NV12 Y plane -> HwaSimIR.Decoded.<channel>.RawGray8`

Gateway 发布的每个 Raw Sample 是一整帧 `width*height` 字节，并在 `HwaSimIR.VideoStatus` 发布 `codec=raw_gray8`、`pixelFormat=gray8`、decoded Topic 和 geometry。Gateway 不使用 ShapeType、1 KB 分片或 VideoChunkV1。

F2 precise 实测计数为 source AU 300、RKMPP decoded 300、Raw published 300、Customer received 300，所有 decode/writer/DDS/drop error 为 0。MPP 输入按一 Sample 一完整 AU 处理，parser split mode 必须关闭。
