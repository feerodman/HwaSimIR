# HwaSimIR DDS F2 执行报告

## 基线与工作区

- F2 开始时记录的基线：`de9cfd25d98d6089b0cde12a06cdbcf2d1527a59`，工作区 clean。
- 收口时当前 `main/origin/main` 已前进到 `dc9b035c85fda4957f0882425128a4a7189a04de`（`DDS集成F2阶段未完成_20260829_1530`）。本轮未执行 reset、restore、clean、commit 或 push。
- 生产视频契约未改变：H264 一 AU 一 `DDS::Bytes` Sample；Raw 一整帧一 Sample；tcpv4 + RELIABLE + KEEP_ALL；未实现 VideoChunkV1。

## F2 实现

- V1 IDL 追加 `VideoFrameMetaV1`、`AnnotationFrameV1`，canonical TypeSupport 已由官方 zrddsgen 生成。
- 每个 START 将 `frameSeq` 清零，首个最终输出帧为 1；DDS video/meta/annotation、TCP Packet v3 和 Local MP4 使用同一逻辑帧身份。
- STOP 关闭新 round ingress，等待已接收输出帧完成，依次 drain video 与 typed frame products、flush/close MP4，最后发布 `VideoStatus.running=false`。
- Customer Receiver 支持 Meta/Annotation pending 对齐及真实 RKMPP H264 -> NV12/Y 解码。
- Decode Gateway 使用 RKMPP：完整 H264 AU -> Gray8 整帧 DDS，并发布 decoded VideoStatus。
- MPP parser 使用 `MPP_DEC_SET_PARSER_SPLIT_MODE=0`，因为上游契约已经是一 Sample 一完整 AU；split 模式会产生 N-1 帧尾帧问题。
- 新 Writer 创建后默认等待 `DiscoverySettleMs=1000`，避免 INIT 后首 AU 早于 discovery。

## Topic

| Channel | Meta | Annotation | Gateway Raw |
|---|---|---|---|
| precise | `HwaSimIR.VideoMeta.precise` | `HwaSimIR.Annotation.precise` | `HwaSimIR.Decoded.precise.RawGray8` |
| coarse | `HwaSimIR.VideoMeta.coarse` | `HwaSimIR.Annotation.coarse` | `HwaSimIR.Decoded.coarse.RawGray8` |

## 实测结果

### 帧同步与 Customer Receiver

- H264 direct：发送/接收/MPP 解码均为 300；`decodeErrors=0`；800x800；receive 60.207 FPS；decode 111.191 FPS；decode 8.994 ms avg / 38.413 ms max。
- Meta + Annotation：video/meta/annotation 均为 300；pending=0；mismatch=0；DDS error=0。
- 5 round：每回合 video/meta/annotation 均为 60，frameSeq 从 1 重启；5 次 RESET/INIT/Ack/START/STOP；Realtime 总数 300；errors=0。修正了 Stim 误复用旧 InitAck 的问题，并在 round 间保留 6000 ms bounded drain。
- RawGray8 direct：360/360，230400000 bytes，60.142 FPS，800x800，meta=360，drop/error/mismatch=0。

### Decode Gateway

- source H264 AU=300，MPP decoded=300，Raw published=300，Customer received=300。
- Raw bytes=192000000；decode 9.220 ms avg / 42.289 ms max；decode/writer/DDS/drop error 均为 0。
- 同板 RELIABLE Raw loopback 接收为 19.843 FPS，表现为背压而非丢帧。

### 最大功能组合（单 precise）

30 秒，H264 DDS + VideoMeta + Annotation + Local MP4：

- video/meta/annotation：1800/1800/1800；约 60 FPS；pending/mismatch/error/drop=0。
- DDS video write：0.468 ms avg / 2.003 ms max；queue max=1；累计 backpressure=1.143 ms。
- Meta write：0.386 ms avg / 1.258 ms max；Annotation write：0.086 ms avg / 1.728 ms max。
- MP4：input/written=1800/1800，drop=0，queue max=1，write 0.096 ms avg / 28.372 ms max；`shared_h264_remux`。
- ffprobe/decode：H264，800x800，60 FPS，30.000 s，1800 frames；完整 decode exit=0。

### 双通道与拓扑

- coarse 单路在广播测试中完成 600/600；但同板 precise+coarse 两个 HwaSimIR 实例同时订阅同一协议 Topic 时，广播命令只到 coarse，precise 未进入新 round，且出现一次 `Send message failed:18, reconnecting`。双通道与双通道最大负载判定 FAIL。
- A（DataDriven DDS -> HwaSimIR -> VideoDisplay DDS）：本轮未完成运行闭环，FAIL。
- B（Stim -> HwaSimIR -> VideoDisplay DDS）：Stim/Hwa 链已验证，VideoDisplay 只完成构建，完整运行闭环未执行，FAIL。
- C（Stim -> HwaSimIR -> Gateway -> Raw Customer Receiver）：PASS，300==300==300==300。
- D（H264 direct + MPP；RawGray8 direct）：PASS，分别 300/300 和 360/360。

### Legacy 回归

- TCP Packet v3 header：PASS，56 bytes，magic `0x48575633`。
- Packet v3 H264 + annotation + realtime：PASS，日志 `logs/v4-packet-v3-20260830-140034/`。
- TCP reconnect、RESET/INIT IDR recovery：PASS，日志 `logs/v3-h264-recovery-20260830-140220/`。
- R1 route/protocol 脚本：FAIL；Windows 子用例读取板端 `192.168.1.116:8888` 配置，bind 返回 10049，日志 `logs/r1-runtime-20260830-140033/`。未把该失败改写为 PASS。

## Known Issues

1. SDK 路径标称 2.4.5，runtime banner 为 `2.4.4-r6873577`。
2. `wait_for_acknowledgments()` 不能单独证明末尾 Sample 已由接收应用处理；必须保留 app queue drain、bounded drain 和双端计数。
3. Trial licence 会被 runtime 改写；并发 DDS 进程必须使用独立可写副本。Gateway 曾因失效副本 Init 失败，换用已验证的独立可写副本后恢复。
4. RK3588 显式绑定 `192.168.1.116` 的 discovery 行为异常；当前验收使用 `tcpv4://default//0`。
5. 当前 vendor runtime 在同板双 HwaSimIR、共享协议 Topic 场景出现单 Reader 收不到广播及 reconnect 错误，阻塞 precise+coarse 最终门禁。

## 主体门禁

| 门禁 | 结果 | 证据/说明 |
|---|---|---|
| Build：VS2015 / MinGW / AArch64 | PASS | HwaSimIR、VideoDisplay、Stim、Customer Receiver；AArch64 Hwa/Stim/Receiver/Gateway，ELF AArch64，RKMPP/ZRDDS 依赖正确 |
| Protocol：F1 regression，Meta/Annotation generated/runtime | PASS | 300/300/300 typed runtime 对齐 |
| Frame synchronization / 5 round | PASS | 每回合 60/60/60，frameSeq 从 1 重启，pending=0 |
| Customer Receiver H264 MPP / Raw | PASS | H264 300==decoded 300；Raw 360/360 |
| Gateway source==decoded==published==received | PASS | 300==300==300==300 |
| Topology A/B/C/D | FAIL | A/B 未完成 VideoDisplay 运行闭环；C/D PASS |
| precise+coarse | FAIL | 同板 DDS 协议广播只到 coarse |
| Maximum load / MP4 / bounded queues | FAIL | 单 precise 30 s PASS；要求的双通道最大负载被上述问题阻塞 |
| Legacy UDP/TCP v3/reconnect/IDR | FAIL | Packet v3 与 reconnect/IDR PASS；R1 route 脚本因 Windows bind 板端 IP 失败 |
| Docs | PASS | ICD、客户指南、现场检查单、执行报告已更新 |
| Workspace | PASS | `git diff --check` 最终执行；本轮无 commit/push |
