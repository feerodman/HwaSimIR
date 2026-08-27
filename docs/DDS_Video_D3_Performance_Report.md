# HwaSimIR DDS 视频 D3 性能与可靠性报告

## 1. 结论

D3 在基线 `b46458b781e9943667305a4e7341c7f3acd8789d` 上完成。H264 生产主链、Raw 完整帧、双通道、多 Reader、20 回合 STOP 尾帧、共享 H264 MP4 和 Windows/VM/RK3588 构建均有实测证据。所有正常消费者用例满足 `sentSamples == receivedSamples`、应用 dropped=0、writer/reader error=0。

D3 不能宣称全绿：故意在 DDS callback 中阻塞 1 秒、持续 50 个 Sample 的强慢消费者用例得到 749 sent / 328 received。Writer 应用队列未上升且 ACK 立即返回，表明当前 ZRDDS runtime 在该压力模型下把积压吸收到不可观测的中间件层，5 秒 bounded drain 后仍未完全交付。因此“slow-reader backpressure no-drop”门禁为 **FAIL**，原始日志保留。没有通过丢帧、补发尾帧或放宽相等条件掩盖结果。

板端生产场景渲染基线本身显著低于 59 FPS，并且同一轮测试中 `renderMs` 从约 17 ms 到 110 ms 波动。D3 记录实际值，不把它归因于 DDS，也不用 Clear 场景冒充生产场景。DDS H264 write 时间约 0.09–0.16 ms，队列最大深度 1；现有证据显示 transport 不是主要耗时，但由于 A/B 的 renderer 状态不稳定，无法给出可信的“DDS 绝对 FPS delta <= 1”结论。

## 2. 构建与 ABI

| 项目 | 结果 |
|---|---|
| Windows HwaSim_IR VS2015 v140 Release x64，ZRDDS+FFmpeg+AVFormat | PASS |
| Windows VideoDisplay VS2015 v140 Release x64，ZRDDS+FFmpeg decoder | PASS |
| Customer Receiver/Smoke VS2015 v140 Release x64 | PASS |
| VM `Release-aarch64-rk3588-ssh` CMake Reload | PASS |
| VM 同 Profile Build | PASS |
| ELF | ELF64 AArch64 |
| 动态依赖 | `librockchip_mpp.so.1`、`libZRDDSCpp.so`、`libavformat.so.58`、`libavcodec.so.58`、`libavutil.so.56` |

VM 使用固定路径 `/home/linaro/userdata/HwaSimIR`、ZRDDS sysroot `/home/linaro/sysroots/zrdds-aarch64`、MPP sysroot `/home/linaro/sysroots/rk3588-mpp`。板端使用 `/usr/ZRDDS/ZRDDS-2.4.5`，未联网安装依赖。

## 3. H264 生产 A/B

每组使用 precise、800x800、H264、相同 UDP 刺激和生产场景，墙钟运行约 60 秒。板卡渲染状态在各组之间存在明显波动，因此 FPS 仅按实际记录呈现。

| 组 | 输出 | 发送/接收 | drop/error | 接收或显示 FPS | DDS write avg/max | queue max | 结论 |
|---|---|---:|---:|---:|---:|---:|---|
| A | TCP H264 only | TCP Packet v3 连续解码 | H264 decode error 0 | VideoDisplay 活跃区间约 21.9–26.5；板端局部 25.6–43.0 | N/A | TCP output 1 | TCP baseline PASS；legacy async overwrite 存在 |
| B | DDS H264 only | 689/689 | 0/0 | 13.663 Samples/s | 0.138/0.572 ms | 1 | zero-drop PASS |
| C | TCP + DDS H264 | 716/716；TCP 连续 | 0/0 | DDS 14.132 Samples/s | 0.115/0.454 ms | 1 | simultaneous PASS |
| D | TCP + DDS + Local MP4 | DDS 684/684；MP4 684/684 | 0/0 | DDS 13.065 Samples/s | 0.113/0.372 ms | 1 | maximum single-channel PASS |

C/D 的 `[VideoOutputProducts]` 显示 `tcpH264=1 ddsH264=1`，D 另有 `localRecordH264=1`，同一帧 `h264EncodeCount=1`。MP4 使用同一 AU remux，没有第二次 H264 编码。

### FPS delta 判定

A baseline 本身低于 59 FPS，而且 A 与 B/C/D 的 `renderMs` 不处于同一稳定区间：A 常见约 17–32 ms，后续生产组常见约 105–110 ms。因此用端点 FPS 直接相减会把 renderer 波动误算成 DDS 开销。D3 的可归因 transport 指标是：H264 DDS write avg 0.09–0.16 ms、max 0.57 ms、queue max 1、无持续增长。严格的同温度/同 renderer steady-state `<1 FPS` 增量门禁仍是 D3 后阻塞项。

## 4. Sync / Async

- Sync `simMode=1`：623 sent / 623 received，writer/reader/drop error 全 0；START 有效窗口精确相等。UDP/渲染总计 642 包，边界前的 19 包不属于 START 后 DDS 窗口。
- Async + DDS：所有正常 D3 用例 `outputOverwritten=0`，no-drop/backpressure 路径保留。
- Async + DDS=false + recording=false：A 组 `outputOverwritten` 持续增长，验证 legacy latest-overwrite 未被全局改变。

`inputOverwritten` 是原异步 UDP 状态输入覆盖统计，不等同于 DDS 视频输出丢帧；D3 的 DDS no-drop 门禁检查 `outputOverwritten`、DDS dropped 和端点计数。

## 5. precise/coarse 双进程

### DDS H264 only

第一次使用 Receiver idle 10 秒时，precise 455/455、coarse 443/407，原始失败保留。把 Receiver idle 窗口改为 25 秒后：

| 通道 | sent/received | receive FPS | write avg/max | queue max | drop/error |
|---|---:|---:|---:|---:|---:|
| precise | 377/377 | 8.755 | 0.103/0.410 ms | 1 | 0 |
| coarse | 372/372 | 8.765 | 0.102/0.489 ms | 1 | 0 |

第二次为最终 PASS 证据，也证明当前 runtime 的尾部交付不能仅依赖 ACK 返回。

### TCP + DDS + LocalRecording 最大组合

| 通道 | DDS sent/received | MP4 input/written | DDS write avg/max | MP4 write avg/max | 结果 |
|---|---:|---:|---:|---:|---|
| precise | 261/261 | 261/261 | 0.089/0.263 ms | 0.208/40.583 ms | PASS |
| coarse | 222/222 | 222/222 | 0.097/0.283 ms | 0.065/1.252 ms | PASS |

两路 TCP Packet v3 均解码，annotation/realtime 保留；两路 MP4 独立、无覆盖、ffprobe/decode PASS。满载时两进程实际约 4–5 FPS，主要受两套生产渲染负载影响，队列未无界增长。

## 6. 多 Reader

一个 precise H264 Writer 同时连接 VideoDisplay DDS Reader 和 Customer Receiver Demo，运行约 30 秒：

- Writer：492 Samples，writeErrors=0，droppedSamples=0。
- Customer Demo：492 Samples，ddsErrors=0。
- VideoDisplay：连续到至少 Sample 480，`packetVersion=0`、`hasAnnotation=0`、`hasRealtimeData=0`、H264 decode error=0。

结论：PASS。第二 Reader 加入未导致生产异常。

## 7. STOP / drain 20 回合

一个 Writer/Participant 进程复用 20 回合，每回合独立 Receiver。发送/接收数依次为：

```text
1, 14, 37, 3, 26, 41, 8, 22, 39, 2,
19, 33, 2, 19, 31, 2, 16, 31, 2, 12
```

20/20 回合发送增量与接收数精确相等，无最后 1–2 帧随机缺失。`DdsVideoDrain` 新增记录 `queueDrainMs`、`ackReturn`、`ackWaitMs`、`boundedDrainMs` 和 `totalDrainMs`；没有重复发送最后一帧。结论：PASS。

## 8. 慢消费者 / 背压

| 用例 | 延迟 | sent/received | DDS app queue max | drop/error | 判定 |
|---|---:|---:|---:|---:|---|
| 轻度慢 H264 | 每 Sample 100 ms | 436/436 | 1 | 0 | PASS，但中间件容量足以吸收，未触发应用层背压 |
| 强阻塞 H264 | 前 50 Sample 各 1000 ms | 749/328 | 1 | writer/drop/reader error 0 | FAIL，接收端退出窗口内未 drain |

强阻塞用例中 ACK 仍很快返回，5 秒 bounded drain 不能证明中间件尾部交付。当前测试将 sleep 放在 DDS callback 内，这是刻意的最差 Reader 处理模型。后续需要厂商确认 tcpv4 RELIABLE/KEEP_ALL 的内部资源和阻塞传播语义，或将客户耗时处理移到其自己的 no-drop worker 后重新验证。不得把 749/328 写成 PASS。

## 9. Raw 性能

### RawGray8 800x800

- Sample size：640,000 bytes，622/622。
- 总 payload：398,080,000 bytes。
- received FPS：12.878，接收 payload 约 8.24 MB/s。
- DDS write avg/max：1.190/10.303 ms。
- DDS queue max：1；累计 publish backpressure wait 1.802 ms。
- `rawPrepMs` 约 6.5 ms。
- writer/reader/drop error：0。
- 板端 production renderer 常见 105–127 ms（约 7.5–9.5 FPS），是当前 60 FPS 目标的首要限制。

可靠性 PASS，sustained FPS 已测量 PASS；`>=59 FPS` 性能目标未达到，未通过丢帧、降分辨率或降质量恢复 FPS。

### RawBGR24 800x800

| target FPS | sent/received | Sample bytes | received FPS | 约 payload | drop/error |
|---:|---:|---:|---:|---:|---:|
| 30 | 618/618 | 1,920,000 | 28.180 | 54.1 MB/s | 0 |
| 45 | 640/640 | 1,920,000 | 26.903 | 51.7 MB/s | 0 |

提高目标到 45 没有增加实际吞吐，说明当前生产渲染/内存路径先达到上限。由于 55/60 不可能越过该上限且 60 FPS payload 已接近 1 GbE 物理极限，D3 未伪造更高档位。`MaxReliableRawBgr24Fps=28.180`。

## 10. Local MP4

60 秒墙钟 D 组：

- backend：`shared_h264_remux`；
- input/written：684/684；dropped=0；queue max=1；
- write avg/max：0.076/1.433 ms；
- ffprobe：H264、800x800、60/1、684 frames、media duration 11.4 s、62,345 bytes；
- 完整 FFmpeg decode：PASS。

容器 PTS 按内部 frame index 和配置 60 FPS 构造，所以生产 renderer 只生成 684 帧时 media duration 为 684/60=11.4 秒，不使用 wall-time 伪造 PTS。

连续五回合生成 5 个独立文件，分别为 1、4、5、4、6 帧；所有文件 ffprobe/decode PASS，input==written、drop=0。资源诊断显示：进程初始 23 threads/5 fd；首次编码后 25 threads/20 fd，其中新增项是 7 个 MPP dmabuf、MPP/DRM 设备、4 个 Panda 模型文件和既有 sockets；第二回合仍为 25/20，未线性增长。第五次采样的 21 fd 中额外 1 个是当时仍打开的 MP4 文件，关闭后不属于泄漏。结论：没有按回合增长的 recorder fd/thread 泄漏。

## 11. 板端资源

采样来自 `/proc/<pid>/{status,stat,io,fd}` 和 `eth0` statistics，约每秒一次。CPU 为“单核 100%”口径，RK3588 多核总计可超过 100%。资源文件覆盖进程启动、运行和 drain，网络平均会被空闲阶段稀释。

| 组 | CPU avg | RSS peak | VmHWM peak | Threads peak | fd peak | NIC Tx avg |
|---|---:|---:|---:|---:|---:|---:|
| A TCP only | 549.9% | 367.1 MiB | 384.9 MiB | 24 | 21 | 0.190 MiB/s |
| B DDS H264 | 607.5% | 322.2 MiB | 378.3 MiB | 30 | 83 | 0.002 MiB/s |
| C TCP+DDS | 614.5% | 340.8 MiB | 397.4 MiB | 30 | 84 | 0.074 MiB/s |
| D TCP+DDS+record | 609.0% | 340.6 MiB | 396.0 MiB | 31 | 85 | 0.080 MiB/s |
| RawGray8 | 608.5% | 323.6 MiB | 378.2 MiB | 29 | 72 | 3.217 MiB/s |

NIC 平均包含长启动/停止空窗，不能替代 receiver payload 吞吐；RawGray8 的有效 payload 约 8.24 MB/s。所有正常用例 DDS queue max=1，无持续队列增长。

## 12. 回归

- TCP Packet v3 H264：PASS，video/annotation/realtime flags 保留。
- TCP reconnect：PASS。
- INIT/reset IDR recovery：PASS。
- R1 route/protocol smoke：PASS。
- DDS=false/record=false 的 legacy async overwrite：PASS。
- 没有增加 Control/Init/Realtime/InitAck DDS 类型或 IDL。

## 13. Known Vendor Issues

1. SDK 路径标签为 2.4.5，runtime banner 为 `2.4.4-r6873577`。
2. `wait_for_acknowledgments()` 不能作为接收端已收到最后 Sample 的唯一证据；需端点计数和 bounded drain。
3. CAEP Trial licence 会修改可写 licence 副本；多进程需独立运行副本。
4. callback 强阻塞时，当前 tcpv4 RELIABLE/KEEP_ALL runtime 未把中间件积压及时反压到应用 writer queue，且 ACK 不能反映该尾部积压；需厂商确认资源限制和回调线程模型。

## 14. 门禁

| 门禁 | 结果 | 说明 |
|---|---|---|
| baseline HEAD recorded | PASS | 精确匹配 D2 基线 |
| git diff --check | PASS | 最终执行 |
| Windows HwaSim_IR Release | PASS | v140 x64 |
| Windows VideoDisplay Release | PASS | v140 x64 |
| Customer Receiver Release | PASS | v140 x64 |
| VM AArch64 cross build | PASS | exact profile |
| CLion-profile Reload / Build | PASS | 命令行等价 Reload+Build，无鼠标操作 |
| AArch64 dependency check | PASS | MPP/ZRDDS/AVFormat/AVCodec/AVUtil |
| precise H264 DDS-only 60s zero-drop | PASS | 689/689 |
| precise H264 TCP+DDS 60s zero-drop | PASS | 716/716 |
| precise H264 TCP+DDS+record 60s zero-drop | PASS | 684/684 |
| Sync DDS H264 zero-drop | PASS | 623/623 |
| Async DDS H264 no-overwrite | PASS | outputOverwritten=0 |
| DDS disabled legacy Async overwrite | PASS | A 组持续 overwrite |
| two DDS subscribers H264 | PASS | Demo 492；Display 连续到 480+ |
| 20 round STOP/drain tail-sample | PASS | 20/20 exact |
| slow-reader backpressure no-drop | **FAIL** | 强阻塞 749/328；原始日志保留 |
| RawGray8 60s zero-drop | PASS | 622/622 |
| RawGray8 sustained FPS measured | PASS | 12.878 FPS，目标 59 未达到 |
| RawBGR24 max reliable FPS measured | PASS | 28.180 FPS |
| LocalRecording 60s zero-drop | PASS | 684/684 |
| LocalRecording 5-round flush | PASS | 5 files, exact counts |
| all MP4 ffprobe/decode | PASS | 60s 与 5-round 文件全部通过 |
| precise+coarse DDS H264 60s | PASS | 377/377，372/372 |
| precise+coarse maximum load | PASS | 261/261，222/222，双 MP4 |
| DDS sentSamples == receiver samples | **FAIL** | 强慢 Reader 为 749/328；其他正式用例相等 |
| DDS droppedSamples=0 / writerErrors=0 / receiver ddsErrors=0 | PASS | 包括强慢 Reader 在内均为 0，但不能替代端点相等 |
| CPU/RSS/network/queue collected | PASS | TSV + summary |
| no unbounded queue growth | **FAIL** | 正常消费者 queue max 1；强慢 Reader 的中间件尾部不可观测，无法证明有界 |
| no fd/thread leak | PASS | 首次初始化后不按回合增长 |
| TCP/R1/reconnect/IDR regressions | PASS | 相关回归通过 |
| Customer Guide / Checklist / Report / ICD | PASS | D3 更新 |
| no Control/Init/Realtime DDS added | PASS | video-only |
| no commit / no push | PASS | 工作树保留 |

## 15. D3 后阻塞项

1. 强慢 Reader 的中间件积压/ACK/反压语义需要 ZRDDS 厂商确认并重新验收。
2. 需要在 renderer steady-state、板卡温度和场景完全可比时重跑紧邻 A/B，才能给出可信的 DDS H264 FPS delta；当前不能证明 `<1 FPS`。
3. 生产场景 renderer baseline 远低于 59 FPS；这是渲染性能问题，不是通过丢 DDS 帧解决的问题。
4. RawGray8 在当前生产 renderer 下仅 12.878 FPS；若需要 60 FPS，应先解决 renderer baseline，再做无损内存/copy profile。
