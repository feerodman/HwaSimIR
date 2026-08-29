# HwaSimIR DDS 客户全链现场调试指南

## 1. 共同参数

- Domain：默认 `150`。
- QoS：`tcpv4 + RELIABLE + KEEP_ALL`；配置文件 `Config/DDS/ZRDDS_PROTOCOL_QOS.xml`。
- 板端常用网卡：RK3588 `192.168.1.116`，Windows `192.168.1.188`。
- SDK 目录标称 2.4.5，实测 runtime banner 为 `2.4.4-r6873577`。
- 每个 DDS 进程使用独立、可写的 Trial licence 副本，不记录 licence 正文。
- H264 一 AU 一 `DDS::Bytes` Sample；Raw 一整帧一 Sample，视频本体中没有自定义头。

控制顺序是 RESET -> INIT -> InitAck -> START -> 60 Hz Realtime -> STOP。`HwaSimIR.VideoStatus` 告知通道、视频 Topic、codec、geometry 和 running。F2 的 Meta/Annotation 是独立 Topic。

## 2. 三条最常用命令

客户 Stim 驱动 HwaSimIR：

```bash
HwaSimIRStimDdsDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --plat-id 1 --sensor-id 1 --sim-mode 2 --video-fps 60 --width 800 --height 800 --h264 1 --save-mp4 0 --duration 10 --realtime-hz 60
```

客户直接接收 H264 并用 RKMPP 解码：

```bash
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --channel precise --expect-codec h264 --output received.h264 --decode mpp --gray-output decoded.gray --receive-meta 1 --receive-annotation 1 --frames 300
```

Gateway 转 Raw 后由客户接收：

```bash
./HwaSimIRDecodeGatewayDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --channel precise --decoded-topic HwaSimIR.Decoded.precise.RawGray8 --frames 300
./HwaSimIRCustomerReceiverDemo --domain 150 --qos Config/DDS/ZRDDS_PROTOCOL_QOS.xml --channel precise --expect-codec raw_gray8 --output decoded.raw --frames 300
```

启动顺序：Receiver -> Gateway（若使用）-> HwaSimIR -> Stim。停止时由 Stim 发 STOP，让发送端先 drain，再结束 Receiver。

## 3. 成功判据

- Stim：InitAck=1，RESET/INIT/START/STOP 各一次，Realtime 约 60 Hz。
- HwaSimIR：`[DdsVideoPerf] droppedSamples=0 writeErrors=0`。
- 帧同步：`video == meta`；启用标注时 `video == annotation`；`frameSeq` 从 1 连续增长。
- MPP：`videoSamples == decodedFrames`，`decodeErrors=0`，Status geometry 等于 decoded geometry。
- Gateway：`sourceH264AUs == decodedFrames == rawPublished`，客户 `videoSamples` 相同。
- STOP：出现 `[OutputRoundDrain]`、`[DdsFrameProductsFinal]`，Status 最后为 `running=false`。

## 4. 常见故障

| 症状 | 检查 |
|---|---|
| DDSIF::Init 失败 | `ZRDDS_HOME`、独立可写 licence、工作目录中的配置文件 |
| receivedSamples=0 | Domain、Topic、channel、codec filter、QoS、NIC、防火墙 |
| H264 长时间无图 | START 是否请求 IDR；首 AU 是否含 SPS/PPS/IDR；是否在 Status 后订阅 |
| MPP decodeErrors | 确认 AArch64 RKMPP 构建；输入是完整 Annex-B AU；比较 geometry |
| Raw size mismatch | 必须等于 `width*height`（Gray8）或 `width*height*3`（BGR24） |
| Meta/Annotation pending | 等待 STOP drain；按 `round+frameSeq` 匹配，不假设跨 Topic 同时到达 |
| 最后几帧缺失 | 检查应用 queue、bounded drain 和双端计数，不能只看 ack API |
| 绑定 192.168.1.116 无 discovery | 使用已验证的 `tcpv4://default//0` 并作为 vendor issue 留证 |

## 5. 交付物

- IDL：`DDS/IDL/HwaSimIRProtocolV1.idl`
- Customer Receiver：`DDS/HwaSimIRCustomerReceiverDemo`
- Decode Gateway：`DDS/HwaSimIRDecodeGatewayDemo`
- ICD：`docs/DDS_HwaSimIR_ICD.md`
- 检查单：`docs/DDS_Field_Debug_Checklist.md`
