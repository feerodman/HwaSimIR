# HwaSimIR DDS F2 执行报告

执行基线：`de9cfd25d98d6089b0cde12a06cdbcf2d1527a59`，初始工作区 clean。F2 未 commit、未 push。

## 实现范围

- 在 V1 IDL 追加 `VideoFrameMetaV1`、`AnnotationFrameV1`，官方 zrddsgen 重新生成 canonical TypeSupport。
- HwaSimIR 输出 worker 在每个 START/round 生成从 1 开始的唯一 `frameSeq`，同一逻辑帧复用到 DDS video/meta/annotation、TCP Packet v3 和 Local MP4 输入。
- STOP 先关闭 round ingress，等待已接收输出帧完成，再 drain video 与 typed frame products，flush MP4，最后发布 `VideoStatus.running=false`。
- Customer Receiver 支持 Meta/Annotation pending 对齐和真实 RKMPP H264->NV12/Y 解码。
- 新增 AArch64 Decode Gateway：H264 AU -> RKMPP -> 整帧 RawGray8 DDS，并发布 decoded VideoStatus。
- VideoDisplay DDS worker 增加 Meta/Annotation 订阅和 `[DdsFrameSync]` 统计，不改变现有显示主链。

## Topic

| Channel | Meta | Annotation | Gateway Raw |
|---|---|---|---|
| precise | `HwaSimIR.VideoMeta.precise` | `HwaSimIR.Annotation.precise` | `HwaSimIR.Decoded.precise.RawGray8` |
| coarse | `HwaSimIR.VideoMeta.coarse` | `HwaSimIR.Annotation.coarse` | `HwaSimIR.Decoded.coarse.RawGray8` |

## 构建与运行证据

最终实测数字和 PASS/FAIL 以本轮日志及下方门禁为准；未完成的现场用例保持 FAIL，不以编译或静态检查替代。

## Known Issues

1. SDK 目录标称 2.4.5，runtime banner 为 `2.4.4-r6873577`。
2. `wait_for_acknowledgments()` 不能单独证明最后 Sample 已被接收应用处理，必须保留 queue drain、bounded drain 和双端计数。
3. Trial licence 会被 runtime 改写，并发 DDS 进程必须使用独立可写副本。
4. RK3588 显式绑定 `192.168.1.116` 的 discovery 行为异常；F2 验收优先使用已通过的 `tcpv4://default//0`。

## 主体门禁

| 门禁 | 结果 | 证据/说明 |
|---|---|---|
| Build：VS2015 / MinGW / AArch64 | 待最终更新 | |
| Protocol：F1 regression，Meta/Annotation generated/runtime | 待最终更新 | |
| Frame synchronization / 5 round | 待最终更新 | |
| Customer Receiver H264 MPP / Raw | 待最终更新 | |
| Gateway source==decoded==received | 待最终更新 | |
| Topology A/B/C/D | 待最终更新 | |
| precise+coarse | 待最终更新 | |
| Maximum load / MP4 / bounded queues | 待最终更新 | |
| Legacy UDP/TCP v3/reconnect/IDR | 待最终更新 | |
| Docs | PASS | ICD、客户指南、检查单、执行报告已更新 |
| Workspace | 待最终更新 | |
