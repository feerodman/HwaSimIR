# HwaSimIR DDS F2.3 Final Closeout

## 1. 范围与基线

- 基线：`061a09a2c79d699dfbc76f461359f7ca65ed7427`（DDS集成F2.2阶段_20260831_0637）。
- IDL、Control/Init/Realtime、视频 `DDS::Bytes` 契约和可靠 QoS 均未修改。
- 目的：隔离 Direct 与 DecodeGateway 的 VideoStatus，并完成无手工 video Topic 的客户发现链。

## 2. 最终状态发现

| Role | Status Topic | 视频 Topic 来源 |
|---|---|---|
| direct | `HwaSimIR.VideoStatus` | 匹配 identity 且 `running=true` 的 `VideoStatusV1.videoTopic` |
| decoded | `HwaSimIR.DecodedVideoStatus` | 匹配 identity 且 `running=true` 的 `VideoStatusV1.videoTopic` |

Gateway 默认 `SourceStatusTopic=HwaSimIR.VideoStatus`、
`DecodedStatusTopic=HwaSimIR.DecodedVideoStatus`。Customer Receiver 使用
`--stream-role direct|decoded`；VideoDisplay 默认 direct，可用 `--stream-role decoded`
调试 Gateway 输出。`--video-topic` 仅为故障覆盖。

自动发现还必须等待 `running=true`。原因是可靠状态 QoS 会立即交付上一回合或启动期的
`running=false` 状态，而 HwaSim_IR 在 INIT 时可能从默认 Raw 切换到 H264；提前创建 Reader
会锁定旧 Topic。MPP 固定帧验收在 STOP 后退出等待，再由主线程做 EOS flush，避免最后一帧
留在 MPP 而形成等待自锁。

## 3. 运行证据

板端日志位于 `/userdata/HwaSimIR/f21/logs/`：

- Direct 300：`f23-direct300-final2`。发送 300，接收 300，MPP 解码 300，
  `wrongTopicSelections=0`，错误/丢帧为 0；接收 60.866 FPS。
- Gateway 300：`f23-decoded300-final`。source/decode/raw publish/customer receive
  均为 300；Gateway source callback 60.007 FPS、decode 61.081 FPS、Raw publish
  61.263 FPS、客户接收 61.369 FPS；错误/丢帧为 0。
- 启动顺序：`f23-startup-order-final/summary.txt`。10/10 轮所有 Stim、Direct、
  Decoded、Gateway 退出码为 0；每轮三端计数一致，Direct/Decoded
  `wrongTopicSelections=0`。
- 多传感器：`f23-multisensor-status`。Direct Status 同时包含
  `1001/1 -> HwaSimIR.Video.1001.1.H264 (640x512)` 与
  `1001/2 -> HwaSimIR.Video.1001.2.H264 (800x800)`；Gateway 仅发布
  `1001/2 -> HwaSimIR.Decoded.1001.2.RawGray8` 到 Decoded Status。

Decoded `running=false` 日志位于 Gateway 的 MPP flush、Raw writer drain 之后。

## 4. 构建

- Windows VS2015 Release：HwaSim_IR、VideoDisplay、Customer Receiver、Stim PASS。
- Windows MinGW 7.3：Stim PASS，使用 MinGW ZRDDS SDK。
- Debian VM AArch64：HwaSim_IR、Customer Receiver、DecodeGateway、Stim PASS；均为
  ELF64 AArch64。

## 5. 最终门禁

1. Direct Status isolation：PASS。
2. Decoded Status isolation：PASS。
3. 10-round startup-order，wrongTopicSelections=0：PASS。
4. Direct automatic discovery，无 `--video-topic`：PASS。
5. Gateway automatic discovery，无 `--video-topic`：PASS。
6. Performance/build regression（Gateway >=55，precise >=59，build）：PASS。
7. Workspace/docs（diff check、docs、无 commit/push）：PASS。

## 6. Vendor Known Issues

1. SDK 目录标称 2.4.5，runtime banner 为 2.4.4-r6873577。
2. `wait_for_acknowledgments()` 不能作为尾 Sample 到达接收应用的唯一证据。
3. Trial licence 会被 runtime 修改；并发进程继续使用独立可写副本。
4. RK3588 显式绑定 `192.168.1.116` 的 discovery 问题继续记录；本轮使用已验证的
   `tcpv4://default//0`。
