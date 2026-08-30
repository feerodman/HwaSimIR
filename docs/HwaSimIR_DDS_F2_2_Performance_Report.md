# HwaSimIR DDS F2.2 Performance Final Closeout

## 1. 验收边界

- 协议允许的最大可配置成像分辨率：`1024×1024`。
- 生产精跟踪性能验收条件：RK3588 Mali、`800×800`、H264、60 FPS。
- `1024×1024` 仅做初始化、成像、DDS 和 MP4 功能 smoke，不作为 60 FPS 门禁。
- 本阶段未测试 `1280×1024`，未修改 DDS Topic、IDL、QoS 或既有多传感器架构。
- 视频仍为 `DDS::Bytes`：H264 一 AU 一 Sample，Raw 一整帧一 Sample；QoS 仍为 tcpv4、RELIABLE、KEEP_ALL。

## 2. Gateway 根因与收口

F2.1 约 20.457 FPS 并非 MPP 解码或同板 WholeFrame Raw DDS 的吞吐上限。隔离测试结果如下：

| Case | source FPS | decode FPS | Raw publish FPS | receive FPS | 计数 | 结果 |
|---|---:|---:|---:|---:|---:|---|
| G1 H264 DDS -> MPP，不发布 Raw | 60.129 | 60.634 | - | - | 614/614 | PASS |
| G2 Raw 800×800 -> Windows | 59.999 | - | 59.999 | 约 60.1 | 600/600 | PASS |
| G3 Raw 800×800 同板 | 59.999 | - | 59.999 | 约 60.1 | 600/600 | PASS |
| G4 H264 -> MPP -> Raw DDS -> 同板客户 Receiver | 60.135 | 60.533 | 60.539 | 60.565 | 614/614/614/614 | PASS |

G4 的 MPP 解码平均/最大耗时为 9.100/40.978 ms，Raw `BytesWrite` 平均/最大耗时为
0.778/9.785 ms，累计 writer blocked 477.469 ms，AU queue 最大深度 4，writer error、DDS error、drop 均为 0。

原 20 FPS 结果的根因是共享 `HwaSimIR.VideoStatus` 上源 HwaSimIR Raw Status 与 Gateway decoded Raw Status
使用同一 identity 时，客户 Receiver 启动时可能按到达时序选择了非 decoded Topic。F2.2 使用既有调试参数
`--video-topic HwaSimIR.Decoded.1001.2.RawGray8` 明确选择 Gateway 输出后，完整链恢复约 60.5 FPS。
由于普通 tcpv4 已满足目标，本阶段没有启用 ZRDDS shared-memory、zero-copy 或其他 same-host transport，也没有拆成双 worker。

## 3. 独立 Raw DDS 吞吐

- 同板：600 × 640000 bytes，59.999 Sample/s，36.621 MiB/s；Receiver 36.693 MiB/s；0 error、0 drop。
- RK3588 -> Windows：600 × 640000 bytes，59.999 Sample/s，36.621 MiB/s；Receiver 36.700 MiB/s；0 error、0 drop。
- 同板结果证明当前 tcpv4 RELIABLE KEEP_ALL 足以承担 800×800 Gray8 @60 的业务 payload。

## 4. 800×800 S1-S4

| Case | 配置 | precise 发送/接收 | precise FPS | coarse 发送/接收 | DDS drop/error | MP4 |
|---|---|---:|---:|---:|---:|---|
| S1 | 单 precise，DDS H264 | 1215/1215 | 60.116 | - | 0/0 | off |
| S2 | 单 precise，DDS + Meta + Annotation + MP4 | 1215/1215 | 60.096 | - | 0/0 | 1215/1215，drop 0 |
| S3 | 双实例，DDS H264 | 1215 sent，queue=1 | 约 60.1 | 1216 sent，queue=1 | 0/0 | off |
| S4 | 双实例，DDS + Meta + Annotation + MP4 | 1815/1815 | 60.018 | 1816/1816 | 0/0 | precise 1815/1815；coarse 1816/1816 |

S4 中 precise 的 video/meta/annotation 为 1815/1815/1815，coarse 为 1816/1816/1816；
两路 DDS queue max=1，录像 queue max=1，无持续增长。Windows ffprobe 与完整 decode 均 PASS：

- coarse：H264 640×512，60/1 FPS，30.266667 s，1816 帧。
- precise：H264 800×800，60/1 FPS，30.250000 s，1815 帧。

S4 中窗口资源采样：coarse CPU 97.4%、RSS 180976 KiB、16 threads；precise CPU 103%、RSS
172304 KiB、14 threads；GPU 300 MHz；温度约 43.5-45.3°C。没有热降频迹象。

## 5. 800×800 分段计时

低频有效 H264 帧采样显示：

| Case | BGR->NV12 avg/max ms | MPP encode avg/max ms | DDS enqueue call avg ms | frame total avg/max ms |
|---|---:|---:|---:|---:|
| S1 | 14.848/34.469 | 2.260/3.743 | 0.020 | 18.016/43.862 |
| S2 | 13.936/31.888 | 2.286/3.291 | 0.024 | 17.297/43.120 |
| S3 precise | 13.598/33.251 | 2.323/5.311 | 0.015 | 16.741/45.115 |
| S4 precise | 9.796/12.062 | 1.935/2.878 | 0.014 | 12.421/18.533 |

渲染主循环样本约为 render 14.5 ms、readback 1.95 ms。生产链中最大的持续 CPU stage 是
BGR->NV12（约 9.8-14.8 ms），DDS 应用入队调用仅约 0.014-0.024 ms；因此没有继续优化 DDS Sender。

## 6. VideoDisplay steady-state

拓扑为 DataDriven/Stim DDS -> HwaSimIR precise 800×800 -> VideoDisplay DDS。首 3 秒不计入性能。

| Stage | steady-state FPS/结果 |
|---|---:|
| source output | 约 60.0 |
| DDS sent | 915/915，约 60.0，writer error/drop 0 |
| Receiver Samples | 915/915；2 秒窗口约 59.3-60.7 |
| FFmpeg decode | 915/915，decode error 0，1.1-2.0 ms/帧 |
| GUI present | 2 秒窗口均值约 59.88，窗口范围约 59.3-60.7 |

最终 `DdsFrameSync` 为 video/meta=915/915，pending=0，mismatch=0，DDS error=0。
历史“VideoDisplay 约 50 FPS”本轮未复现。修正了 `[VideoPerf]` 的统计口径：区间帧数现在除以相邻日志间隔，
不再错误地除以从 reset 起的累计时长。

## 7. 1024×1024 最大配置 smoke

- Init、Stage6 FinalPipeline、VideoStatus 和实际输出均为 1024×1024。
- H264 DDS：297/297，0 error、0 drop；实测 57.340 FPS（仅记录，不作门禁）。
- MP4：shared_h264_remux，297/297，drop 0；ffprobe 为 H264 1024×1024、60/1、4.95 s、297 帧；完整 decode PASS。
- 无 crash、无越界、queue max=1。

## 8. 构建、回归与门禁

- Windows VideoDisplay VS2015 Release x64：PASS。
- VM AArch64 HwaSimIR 与 DecodeGateway：PASS，ELF64 AArch64。
- R1 Windows loopback affected smoke：PASS；覆盖双 channel、Control/Init/Realtime exact/mismatch/broadcast、Ack identity、simMode 1/2，legacy size 24/385/506/17 未变。

| 最终门禁 | 结果 | 证据 |
|---|---|---|
| 1. Gateway bottleneck identified | PASS | 状态驱动 Topic 选择时序；MPP 与 Raw DDS 隔离均约 60 FPS |
| 2. Gateway reliability | PASS | 614==614==614==614，0 drop/error |
| 3. Gateway performance >=55 FPS | PASS | 60.565 FPS end-to-end |
| 4. precise 800×800 single/dual >=59 FPS | PASS | S1/S2 60.1；S4 precise 60.018 |
| 5. VideoDisplay steady-state split | PASS | sender/receiver/decode/gui 均已拆分，GUI 约 59.88 FPS |
| 6. 1024×1024 + regression/workspace | PASS | 最大配置 smoke、R1、MP4 decode、diff check；无 commit/push |

## 9. Known Issues

1. 同一 identity 下若源与 DecodeGateway 都发布 VideoStatus，启动时的 retained/refresh 到达次序可能令接收端选择错误 Raw Topic；正式 Gateway 联调应使用 decoded Topic override，后续需由产品配置保证明确的 status source/stream role。
2. ZRDDS 安装目录标称 2.4.5，但 runtime banner 为 2.4.4-r6873577。
3. `wait_for_acknowledgments()` 不能作为尾 Sample 已抵达的唯一证据；继续保留应用队列 drain、bounded drain 和发送/接收计数验收。
4. Trial licence 会被 runtime 改写；并发 Windows DDS 进程需要各自独立、可写的 licence 副本。
5. HwaSimIR 旧 `[Perf] outputFps` 是 TCP 发送计数，DDS-only 时为 0；DDS 性能以 `[DdsVideoPerf]`、Receiver 统计和新增 `[VideoOutputPerf]` 为准。

