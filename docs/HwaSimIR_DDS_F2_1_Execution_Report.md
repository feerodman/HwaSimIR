# HwaSimIR DDS F2.1 Final Architecture Closeout

## Baseline and workspace

- 开始 HEAD：`63c4a551ada300ed3ced1b35c86e963e56979d0e`，提交说明 `DDS集成F2阶段_20260830_1421`。
- 开始时保留用户修改：`HwaSim_IR/Bin/Config/NetworkConfig_coarse.ini` 的板端 local IP 修正为 `192.168.1.116`。
- 本阶段未执行 reset、restore、clean、commit 或 push。

## Final architecture

DDS 控制面固定为共享 `HwaSimIR.Control/Init/Realtime/InitAck/VideoStatus` Topic；HwaSimIR 使用统一 `EvaluateProtocolRoute()` 按 platID/sensorID 处理 exact、mismatch 和 sensorID=255 broadcast。DDS 视频面默认使用：

```text
HwaSimIR.Video.{platID}.{sensorID}.{codec}
HwaSimIR.VideoMeta.{platID}.{sensorID}
HwaSimIR.Annotation.{platID}.{sensorID}
```

`legacy_channel` precise/coarse Topic 与原 UDP/TCP 端口继续保留。

## Routing and fan-out evidence

- 纯逻辑测试：7 cases，0 failure。
- 一个 Writer、三个 Reader：每个 Reader `ddsCallbackSamples=12 routeAccepted=5 routeRejected=7`，exact/broadcast geometry 最终正确，错误平台样本在 callback 后被业务 route 拒绝。
- exact Init：1001/1=640x512/11.1，1001/2=1280x1024/22.2，1001/3=800x800/33.3。
- broadcast Init：1001/255 被三个允许广播的 Reader 接受，返回 Ack 1001/1、1001/2、1001/3；禁用第三路广播后只收到 2 个 Ack。
- Windows 和 RK3588 AArch64 fan-out 均 PASS。
- 共享 licence A/B 也能 fan-out；旧 FAIL 的根因是 DDS ingress 缺少统一应用 ID route，而不是 shared Topic 或 Participant 不支持 fan-out。考虑 Trial runtime 改写 licence，生产并发进程仍使用独立可写副本。

## Identity video and geometry

- 1001/1：`HwaSimIR.Video.1001.1.H264`，Status/实际输出 640x512。
- 1001/2：`HwaSimIR.Video.1001.2.H264`，Status/实际输出 1280x1024。
- VideoDisplay、Gateway、CustomerReceiver 按共享 VideoStatus 的 identity 自动选 Topic。
- 修复 HwaSimIR 在 Network identity 载入前创建 `.0.0` Meta/Annotation Writer 的问题；现在 identity 写入端在本地身份就绪后重新解析。
- 修复 VideoDisplay 运行中 `UnSubTopic` 卡住：新 Reader 先创建，旧 Reader 延迟到 shutdown 回收并过滤旧回调；INIT 阶段 Status 预建视频 Reader，避免首帧 discovery 丢失。

## Topology A and B

- A DataDriven DDS -> HwaSimIR -> VideoDisplay DDS：RESET/INIT/Ack/START/约 60 Hz Realtime/STOP 全通；VideoDisplay 自动 H264 Topic，433 Sample，DDS error=0，显示约 55.51 FPS，geometry 800x800。
- B StimDdsDemo -> HwaSimIR -> VideoDisplay DDS：最终短轮 Stim 180 Realtime，Async 输出 video/meta/annotation=195/195/195，pending=0、mismatch=0、DDS error=0。另一个 5 秒窗口完整接收 300 H264 Sample。START 后增加可配置 bounded settle（默认 250 ms），避免 Control 与 Realtime 跨 Topic 处理竞态。

## Gateway

- callback 只执行 owned AU copy + bounded no-drop enqueue；worker 执行 RKMPP decode 和 WholeFrame Raw DDS publish。
- Gateway 先发布 decoded `running=false` Status，使 CustomerReceiver 在 START 前完成 Raw Reader discovery。
- 完整 STOP 闭环：`sourceH264AUs=314 decodedFrames=314 rawPublished=314 customerReceived=314`，decode/writer/DDS/drop error 均为 0，queue max=3。
- MPP 纯 decode 累计耗时折算 142.43 FPS，7.021 ms avg / 39.653 ms max。
- WholeFrame RELIABLE Raw 同板端到端 20.457 FPS；F2 原实现为 19.843 FPS。可靠性 PASS，但 `>=55 FPS` 性能目标 FAIL，瓶颈仍是当前 ZRDDS WholeFrame Raw loopback/backpressure 路径，不允许通过丢帧、BEST_EFFORT 或 VideoChunkV1 绕过。

## Dual-instance 30-second maximum load

配置：1001/1 coarse 640x512、1001/2 precise 1280x1024；开启 DDS Control/Init/Realtime、H264、VideoStatus、VideoMeta、Annotation 和 Local MP4。

| Stream | Video/Meta/Annotation | FPS | DDS write avg/max | queue max | MP4 |
|---|---:|---:|---:|---:|---:|
| 1001/1 | 1815/1815/1815 | 60.072 | 0.208/2.151 ms | 1 | 1815/1815, drop 0 |
| 1001/2 | 1370/1370/1370 | 45.437 | 0.160/1.054 ms | 1 | 1370/1370, drop 0 |

中窗资源：1001/1 CPU 96.9%、RSS 180900 KiB；1001/2 CPU 114%、RSS 218140 KiB。两路 writerErrors=0、droppedSamples=0、pending/mismatch=0，queue 无持续增长。

MP4 均为 `shared_h264_remux`。Windows ffprobe/decode：

- coarse：H264 640x512，60/1 FPS，30.25 s，decode PASS。
- precise：H264 1280x1024，60/1 FPS，22.833 s，decode PASS。

## Legacy regression

- R1 使用 Windows loopback 临时配置重新执行 PASS，协议尺寸 24/385/506/17，覆盖双 channel、exact/mismatch/broadcast、Ack identity、simMode 1/2。
- TCP Packet v3 header 当前执行 PASS：56 bytes，magic `0x48575633`。
- F2 基线已有 TCP Packet v3、reconnect、RESET/INIT IDR recovery PASS；F2.1 未改变 TCP wire format。

## Final gates

| Gate | Result | Evidence |
|---|---|---|
| 1. DDS ID routing | PASS | Control/Init/Realtime exact+mismatch，7/7 pure tests |
| 2. sensorID=255 | PASS | broadcast Init 3 Ack；broadcast Realtime fan-out |
| 3. one Writer / multi sensor | PASS | 3 个不同 geometry exact Init |
| 4. fan-out | PASS | Windows/board 1 Writer -> 3 Readers |
| 5. identity Video Topic | PASS | 1001/1、1001/2 + Status auto discovery |
| 6. different geometry | PASS | 640x512 与 1280x1024 独立输出 |
| 7. Gateway >=55 FPS | FAIL | 314==314==314==314、0 drop；端到端 20.457 FPS |
| 8. Topology A/B | PASS | DataDriven 与 Stim 两条 VideoDisplay DDS 闭环 |
| 9. dual-instance 30 s | PASS | 两路 count 对齐、MP4 对齐、queue bounded |
| 10. Legacy R1/TCP | PASS | R1 + Packet v3；reconnect/IDR 沿用 F2 回归 |
| 11. workspace | PASS | 最终 diff check；无 commit/push |
