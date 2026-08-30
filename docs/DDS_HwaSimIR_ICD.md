# HwaSimIR DDS 接口控制文档（F2.1）

## 1. 总体架构

DDS 与 Legacy 并列存在，不互相替换：

- Legacy 控制面继续使用 UDP 0x41/0x36/0x38/0x37，视频继续支持 TCP Packet v3。
- DDS 控制面使用共享 Topic 和 `platID/sensorID` 应用层路由。
- DDS 视频面默认使用每个传感器独立的 identity Topic。
- DDS 视频 Sample 仍只包含视频本体，不包含 frameSeq、PTS、geometry、annotation 或自定义头。

```text
                 One Stim process
                        |
        +---------------+---------------+
        |               |               |
   ControlWriter    InitWriter    RealtimeWriter
        |               |               |
        +---------------+---------------+
                        |
               Shared DDS Topics
                        |
          +-------------+-------------+
          |             |             |
       1001/1        1001/2        1001/3
       HwaSimIR      HwaSimIR      HwaSimIR
          |             |             |
 Video.1001.1.* Video.1001.2.* Video.1001.3.*
```

默认 Domain 为 150。QoS 为 `tcpv4 + RELIABLE`；Control/Init/Realtime/Ack/视频使用 KEEP_ALL，VideoStatus 使用可靠状态 QoS。配置文件为 `Config/DDS/ZRDDS_PROTOCOL_QOS.xml`。

## 2. 共享控制 Topic

| Topic | Type | Key | Legacy |
|---|---|---|---|
| `HwaSimIR.Control` | `ControlCommandV1` | `platID` | 0x41 |
| `HwaSimIR.Init` | `InitCommandV1` | `platID,sensorID` | 0x36 |
| `HwaSimIR.Realtime` | `RealtimeDataV1` | `platID,sensorID` | 0x38 |
| `HwaSimIR.InitAck` | `InitAckV1` | `platID,sensorID` | 0x37 |
| `HwaSimIR.VideoStatus` | `VideoStatusV1` | `platID,sensorID` | DDS 状态 |

正式 IDL 位于 `DDS/IDL/HwaSimIRProtocolV1.idl`，generated 文件位于 `DDS/Generated/HwaSimIRProtocolV1`。Legacy packed size 必须保持 24/385/506/17 bytes。

## 3. ID 路由

所有 UDP/DDS ingress 共用 `DDS/Protocol/ProtocolRoute.h`：

- Control：`packet.platID == localPlatID` 时接受；Control 不增加 sensorID。
- Init/Realtime exact：platID 与 sensorID 均匹配时接受。
- Init/Realtime broadcast：platID 匹配、`sensorID=255` 且本地 `acceptSensorBroadcast=true` 时接受。
- 其余返回 `plat_mismatch` 或 `sensor_mismatch`，业务 handler 不执行。
- 广播 Init 的 Ack 使用每个进程自己的本地身份，例如 1001/1、1001/2，而不是 1001/255。

一个 Stim 正常只创建一组 Writer。增加传感器不会增加 Writer 数量；同一 InitWriter 可依次写 1001/1、1001/2、1001/3。

## 4. 视频 Topic

生产推荐：

```ini
[DdsVideo]
TopicMode=identity
TopicPattern=HwaSimIR.Video.{platID}.{sensorID}.{codec}
```

Codec token 固定为 `H264`、`RawGray8`、`RawBGR24`。示例：

```text
HwaSimIR.Video.1001.1.H264
HwaSimIR.Video.1001.2.RawGray8
HwaSimIR.Video.1001.3.RawBGR24
```

`VideoStatusV1.videoTopic` 是接收端正式发现 Topic 的唯一事实源。VideoDisplay、Gateway 和 CustomerReceiver 先按 platID/sensorID 过滤共享 VideoStatus，再订阅其中的 videoTopic。`--video-topic` 仅是调试 override。

兼容模式 `TopicMode=legacy_channel` 保留 precise/coarse 六个历史 Topic，不删除老客户接口。

## 5. 视频 Sample 契约

- Type 始终为内置 `DDS::Bytes`。
- H264：一个 Sample 等于一个完整 Annex-B Access Unit。
- RawGray8：一个 Sample 等于一帧，严格为 `width*height` bytes。
- RawBGR24：一个 Sample 等于一帧，严格为 `width*height*3` bytes。
- 禁止 BEST_EFFORT、UDP 大包零拷贝、应用层覆盖旧帧和自定义视频头。

## 6. VideoStatus、Meta 与 Annotation

共享状态 Topic `HwaSimIR.VideoStatus` 携带 identity、running、codec、pixelFormat、videoTopic、width、height、fps、bitrate、GOP 和 round。

identity 模式的逐流辅助 Topic：

```text
HwaSimIR.VideoMeta.{platID}.{sensorID}
HwaSimIR.Annotation.{platID}.{sensorID}
```

历史 `HwaSimIR.VideoMeta.precise/coarse` 与 `HwaSimIR.Annotation.precise/coarse` 继续由 legacy_channel 模式兼容。

每个 START 的首帧 frameSeq=1。同一逻辑帧的 video、VideoFrameMeta、Annotation、TCP Packet v3 和 Local MP4 输入共享帧身份。跨 Topic 到达顺序不保证，接收端按 `currentRound + frameSeq` 使用 pending map 对齐。

## 7. START/STOP 生命周期

1. RESET/INIT 完成业务状态更新；InitAck 返回实际本地 identity。
2. START 开启本回合，frameSeq 从 1 开始。
3. 不同 DDS Topic 不提供跨 Topic 顺序保证；Stim 在 START 后使用 bounded settle，再开始固定 60 Hz Realtime。
4. STOP 停止新帧 ingress，依次 drain video/meta/annotation，flush/close MP4，最后发布 `VideoStatus.running=false`。
5. `wait_for_acknowledgments()` 不能作为尾帧到达的唯一证据，必须结合 app queue drain、bounded drain 和双端计数。

## 8. Decode Gateway

```text
HwaSimIR.Video.<plat>.<sensor>.H264
  -> RKMPP decode
  -> NV12 Y / Gray8
  -> HwaSimIR.Decoded.<plat>.<sensor>.RawGray8
```

Gateway callback 只复制 AU 并进入 bounded no-drop queue；worker 执行 MPP decode 和 WholeFrame Raw DDS publish。Gateway 先发布 decoded `running=false` VideoStatus，供客户预建 Reader；STOP 时 drain decoder、Raw writer，再发布 running=false。F2.1 不实现 VideoChunkV1。

## 9. Legacy 与配置模式

```ini
[CommandTransport]
Input=udp|dds|both
Ack=match_input|udp|dds|both
```

UDP 与 DDS 都进入同一套业务 handler。both 模式按既有 semantic key 去重。Legacy precise/coarse 继续使用独立 UDP/TCP 端口；channel 仅用于可读名称、Legacy 配置和 legacy_channel Topic，DDS 身份以 platID+sensorID 为准。

## 10. Vendor Known Issues

1. SDK 安装路径标称 2.4.5，实际 runtime banner 为 2.4.4-r6873577。
2. `wait_for_acknowledgments()` 可能早于接收应用处理完末尾 Sample 返回。
3. Trial runtime 会改写 licence；共享 licence 的本次 fan-out A/B 也可运行，但生产并发进程仍建议使用独立可写副本，避免并发改写风险。
4. RK3588 显式绑定 `192.168.1.116` 时 discovery 异常；现场验收使用官方支持的 `tcpv4://default//0`。
5. 800x800 WholeFrame RawGray8 在同板 Gateway RELIABLE loopback 实测约 20.46 FPS；可靠计数为零丢帧，但未达到 55 FPS。不得通过丢帧、BEST_EFFORT 或 VideoChunkV1 伪造性能。
