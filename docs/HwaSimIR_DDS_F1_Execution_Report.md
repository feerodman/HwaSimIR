# HwaSimIR DDS 全通信链 F1 执行报告

执行基线：`7383ef8653ab495b8a8cf071d8027b2a774f7a3f`。未 commit、未 push。
原始证据目录：`logs/dds-f1-20260828-190155/`。

## 实现摘要

- 冻结 `DDS/IDL/HwaSimIRProtocolV1.idl` 并以官方 `zrddsgen` 生成 canonical TypeSupport。
- 增加逐字段 `CommonDataDdsAdapter`、roundtrip/legacy-size test。
- 增加进程级 `DdsRuntimeManager`，HwaSimIR 的协议和视频共享一次 Init/CreateDP/Finalize。
- HwaSimIR 增加 UDP/DDS/both ingress、Ack route、both 去重和 VideoStatus。
- DataDrivenTestQT 增加 `--control-transport udp|dds|both`，维持 RESET→INIT→Ack→START→60Hz→STOP。
- VideoDisplay DDS worker 增加 typed control/init/realtime/status，按 VideoStatus 自动切视频 Topic。
- 增加纯 C++ Stim、板端 Receiver、视频 Transport Benchmark。
- `DdsVideoPublisher` 增加 owned/shared payload API 和 timing breakdown；ZRDDS 内部 copy 未作承诺。

## 三工具链

VS2015 x64、MinGW 7.3.0 x64 和 VM AArch64 generated/adapter/Stim 均实际编译通过；
VS/MinGW roundtrip 均运行通过。HwaSimIR VS Release、VideoDisplay VS Release、DataDriven
MinGW Release、VM HwaSimIR AArch64 Release 均通过。AArch64 HwaSimIR 是 ELF64 AArch64，
依赖包含 `librockchip_mpp.so.1`、`libZRDDSCpp.so`、`libavformat.so.58`、
`libavcodec.so.58`、`libavutil.so.56`。

## 控制闭环

- VS Stim：RESET=1、INIT=1、Ack=1、START=1、Realtime=360、STOP=1、errors=0。
- MinGW Stim：Realtime=60、Ack=1、errors=0。
- DataDriven both：同一业务的 DDS accepted=1；UDP counterpart duplicate=1；Ack 后 START/STOP。
- HwaSimIR 进程日志：`runtimeInitCount=1`，协议五 Topic 与视频 Writer 共享 runtime。

## VideoStatus/客户板端闭环

- H264：自动 Topic `HwaSimIR.Video.precise.H264`，statusReceived=31，videoSamples=30，
  videoBytes=2887，receiveFps=66.830，width/height=800/800，ddsErrors=0。
- RawGray8：自动 Topic `HwaSimIR.Video.precise.RawGray8`，statusReceived=29，videoSamples=10，
  videoBytes=6,400,000（每 Sample 640,000），receiveFps=66.846，ddsErrors=0。
- Receiver 基础构建没有链接 MPP，`decodeFps=0`；optional MPP decode 未实现。

## DDS 视频耗时

Windows FFmpeg H264 正式发送 360 Samples：appCopy 0.001 ms、enqueue 0.007 ms、
queueWait 0.001 ms、ddsWrite 0.030 ms、totalSender 0.038 ms（均为每 Sample 平均），
writeErrors=0、droppedSamples=0、maxQueueDepth=1。

RawGray8 累计到 662 Samples 时：appCopy 0.037 ms、enqueue 0.008 ms、queueWait
0.001 ms、ddsWrite 0.084 ms、totalSender 0.128 ms，writeErrors=0、droppedSamples=0、
maxQueueDepth=1。板端只为验证 Customer Receiver，在 10 Samples 达标后退出；Hwa sender
继续发送，故该短测不用于宣称全窗口 sent==received。

约 50 FPS 的显示瓶颈尚未以 source/decode/gui 三端同窗口完成定位；现有发送 timing 表明
DDS writer 本身不是毫秒级主瓶颈。F1 最终全 DDS 短闭环没有复现 50 FPS：sender=180、
VideoDisplay receiver=180，receive/display=60.796 FPS，decode=0.976 ms，GUI display=0.743 ms，
DDS write=0.144 ms。由此可排除该用例中的 Direct DDS、decoder 和 GUI present 为 50 FPS
瓶颈；若现场仍约 50 FPS，需要对该现场 renderer/source 场景用同一组分段日志复测。

## VideoDisplay 全 DDS

最终运行证据包含 RESET control、Init、START、Realtime count=1/2/3/120、STOP；VideoStatus
从 Raw 初始状态自动切换到 `HwaSimIR.Video.precise.H264`。首帧 IDR 经 FFmpeg 解码成功，
status geometry 800x800 等于 decoded geometry 800x800，label 800x800，
`aspectPreserved=1`。最终 `receivedSamples=180`、`receivedBytes=14350`、`ddsErrors=0`，
与 sender 180/14350 完全一致。

## Legacy 与 Local MP4 回归

- R1 route/protocol smoke PASS：Control/Init/Realtime/Ack、broadcast/wrong sensor、sync/async，
  legacy size 24/385/506/17。
- TCP Packet v3 H264 all flags PASS：video/realtime/annotation 与原 v3 wire path 保持。
- TCP reconnect + reset/init IDR recovery PASS。
- HwaSimIR Local MP4 `shared_h264_remux` PASS：156 input/156 written/0 dropped，800x800、
  H264、60/1 FPS、156 decoded frames、duration 2.6 s，STOP 正常 close。

## 未完成/失败项

- Customer Receiver optional MPP decode 未实现。
- D3.1 的完整长时生产矩阵未整套重跑；F1 已重跑相关 R1/TCP v3/reconnect/IDR/Local MP4。
- 第一次板端 H264 测试错误使用 Windows `H264Encoder=mpp`，0 Sample；原始失败日志保留，
  修正为 FFmpeg 后 30/30 PASS。

optional MPP decode 与 D3.1 全矩阵不是 F1 明列门禁；历史约 50 FPS 的精确复现仍记 FAIL。

## 2026-08-29 final closeout addendum

DataDrivenTestQT 的 F1 MinGW 产物又以纯 DDS 模式完成一次独立闭环。实际命令使用
`--control-transport=dds --duration-sec=3 --dds-discovery-wait-ms=2500`，结果为单次
`DdsRuntime` 初始化/释放、RESET=1、INIT=1、DDS InitAck=1、START=1、Realtime
约 60.484 Hz、STOP=1。HwaSimIR 侧依次记录 DDS control/init/realtime/control，
realtime ingress count 至少到 120，并在 DDS STOP 后按测试开关正常退出。证据为
`logs/dds-f1-20260828-190155/datadriven_pure_dds_pass.err.log` 与
`hwa_datadriven_pure_dds_pass.out.log`。

最终补跑 RK3588 到 Windows 同 NIC 公平性能矩阵后，F1 判定更新为 46 PASS / 1 FAIL。
DirectBytes、ShapeType-1KB/32KB、Chunk64K 与 Raw whole/64K/32K 均为 600/600、
errors=0、drops=0，并采集两端 CPU/RSS 与板端网卡字节。唯一 FAIL 是
历史“约 50 FPS”现场现象的精确复现。当前 180/180 全 DDS 闭环测到 60.796 FPS，
已经排除该测试场景中的 DDS writer、FFmpeg decoder 与 GUI present 为 50 FPS
瓶颈，但不能把“未复现”写成已精确定位。详细逐项判定见同一日志目录的
`summary.txt`。

跨机数字表与结论见 `docs/DDS_dds_pub_sub_Comparison.md`。核心结论是 H264
ShapeType-1KB 把 600 帧扩大为 60,000 Samples，板端 CPU 20.868%，而 Direct
仅 600 Samples、CPU 4.324%；Raw whole-frame 在相同 36.6 MiB/s 下也比 chunk
占用更低。F1 推荐 H264 DirectBytes-AU、Raw WholeFrameBytes。
