# DDS 视频现场调试检查单

F2 已支持 DDS 全链 Control/Init/Realtime/Ack、VideoStatus、VideoMeta、Annotation；legacy 模式仍使用原 UDP+TCP，DDS full 模式由 `CommandTransport Input=dds|both` 选择。

## F2 帧同步与 Gateway

- [ ] Direct Receiver 使用 `--stream-role direct`，Status Topic 为 `HwaSimIR.VideoStatus`
- [ ] Decoded Receiver 使用 `--stream-role decoded`，Status Topic 为 `HwaSimIR.DecodedVideoStatus`
- [ ] 正常运行未传 `--video-topic`，输出 `wrongTopicSelections=0`
- [ ] 自动发现等待匹配 identity 的 `running=true` Status，未锁定启动期默认 Raw 历史状态
- [ ] precise/coarse 的 Video、Meta、Annotation Topic 未串通道
- [ ] 每个 START 首帧 `frameSeq=1`，同 round 连续且无重复
- [ ] STOP 后 video==meta；标注开启时 video==annotation；pending=0
- [ ] 5 个 round 独立，下一 round 不沿用上一 round 序号
- [ ] H264 Receiver `--decode mpp` 确认链接真实 `librockchip_mpp.so.1`
- [ ] MPP `receivedSamples==decodedFrames`、`decodeErrors=0`、geometry 一致
- [ ] Gateway `sourceH264AUs==decodedFrames==rawPublished==customer videoSamples`
- [ ] decoded Raw Topic 每 Sample 严格为 width*height bytes
- [ ] Gateway STOP 完成 decoder EOS、DDS drain，并发布 decoded `running=false`
- [ ] 视频 Bytes 中没有 frameSeq/PTS/Annotation/custom header

## 启动前

- [ ] Windows `ZRDDS_HOME=F:\Programs\ZRDDS\ZRDDS-2.4.5`
- [ ] RK3588 `ZRDDS_HOME=/usr/ZRDDS/ZRDDS-2.4.5`
- [ ] 双方 licence 存在、可写、License 初始化 PASS；未记录 licence 正文/Signature
- [ ] Windows 板卡网卡为 `192.168.1.188`
- [ ] RK3588 为 `192.168.1.116`
- [ ] 未绑定 `192.168.123.100`、VMware 或其他网卡
- [ ] ping、Windows 防火墙规则和 tcpv4 discovery 已确认
- [ ] 双方 Domain=`150`
- [ ] QoS 为 tcpv4 + RELIABLE + KEEP_ALL，无 BEST_EFFORT
- [ ] Topic 的 channel/codec/大小写完全一致
- [ ] Raw 的 width/height/pixel format 已由双方约定

## 启动顺序

- [ ] 先启动 Receiver，看到 `receiverReady=1`
- [ ] 再启动 HwaSim_IR，并从其正常资产工作目录运行
- [ ] 通过原 UDP 发送 INIT、START 和 realtime
- [ ] H264 首个可用 AU 含 SPS/PPS/IDR，或在一个 GOP 内恢复
- [ ] DDS VideoDisplay 显示 `packetVersion=0`；Init/Realtime 来自 typed DDS，Annotation 来自独立 Topic，不从视频 Bytes 解析

## 运行中

- [ ] `[DdsVideoPerf] writeErrors=0 droppedSamples=0`
- [ ] DDS application queue 无持续增长
- [ ] TCP+DDS+MP4 时 `[VideoOutputProducts] h264EncodeCount=1`
- [ ] DDS enabled 时 `outputOverwritten=0`
- [ ] DDS/record 均 disabled 时 legacy async latest-overwrite 仍存在
- [ ] RawGray8 每 Sample=`width*height`
- [ ] RawBGR24 每 Sample=`width*height*3`
- [ ] 记录 CPU、VmRSS/VmHWM、Threads、fd、NIC Tx/Rx、queue/backpressure

## STOP / 退出

- [ ] 先停止产生新 Sample，再 drain 应用队列
- [ ] 保留 `wait_for_acknowledgments` 加 bounded drain
- [ ] Receiver 使用足够长的 `--idle-exit-ms`，不要立即杀进程
- [ ] `sentSamples == receivedSamples`
- [ ] writerErrors=0、ddsErrors=0、droppedSamples=0
- [ ] 录像 `inputFrames == writtenFrames`、droppedFrames=0
- [ ] MP4 已 close，ffprobe 和完整 decode PASS
- [ ] 失败时保存原始命令、stdout/stderr，不用补发尾帧或跳过用例伪造 PASS

## 交付物

- [ ] HwaSim_IR、VideoDisplay、Customer Receiver 构建版本和 SHA256
- [ ] 使用的 ZRDDS 路径、runtime banner 和 licence 状态
- [ ] Domain/Topic/QoS/codec/Raw geometry
- [ ] 发送/接收/错误/丢帧统计
- [ ] CPU/RSS/network/queue 指标
- [ ] H264 或 Raw audit hash（仅验收开关）
- [ ] MP4 ffprobe/decode 输出
- [ ] `git diff --check`、`git diff --stat`、`git status`

## D3.1 手工黑屏专项门禁

- [ ] `ps -ef | grep '[H]waSim_IR'` 只有预期实例；启动前 UDP 8888 无旧 owner
- [ ] UDP bind 失败时出现 `[StartupFatal]` 且进程非零退出
- [ ] `/userdata/HwaSimIR/Config/DDS/ZRDDS_QOS_PROFILES.xml` 和现场 bound QoS 均存在
- [ ] Xorg `:0` 存在，`glVendor=ARM`、`glRenderer=Mali-LODX`、`hardwareGpu=1`
- [ ] 日志中无 llvmpipe、`hardwareGpu=0` 和 GL 0x502
- [ ] Windows 使用 `192.168.1.188` bound QoS；板端使用 `192.168.1.116` bound QoS
- [ ] `[DdsVideoReceiverSample]` 持续增长且 `[H264DecodeSuccess]` 存在
- [ ] `[DdsFrameDiag] max > min`、`stddev > 0`，首帧 PNG 存在
- [ ] GUI 在 START/Realtime 期间有图；STOP 后保留最后一帧属于正常行为
- [ ] 双 Reader 时每个 Windows 进程使用独立、可写的 Trial licence 副本
- [ ] DDS full 模式左侧 INIT/Realtime 来自 F1 typed Topic；F2 Meta/Annotation 仅用于帧同步，不写入视频 Bytes
