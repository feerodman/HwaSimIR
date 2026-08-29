# Direct DDS 与 `DDS/dds_pub_sub` 分片方案审计

## 结论

生产 H264 继续使用 `DDS::Bytes` 且一 AU 一 Sample；生产 Raw 继续使用 `DDS::Bytes` 且一帧
一 Sample。`DDS/dds_pub_sub` 保留为 reference/test，不将 `ShapeType` 或 1 KB 分片引入主协议。
目前功能 benchmark 证明所有候选均可重组且零错误，但尚未完成同 NIC、同机型 CPU/RSS 的
公平吞吐测量，因此不能声称分片比 Direct 更快。

## 同事方案代码审计

- 类型：自定义 `ShapeType`，把视频字节放在 generated type 的 payload 字段。
- `H264_PACKET_MAX`：把一个编码帧/AU 切成固定上限的小块；1 KB 默认会把 100 KB AU
  扩成约 98 个 DDS Sample。
- QoS：原 sample 的默认 QoS可作为参考，但只有改为 tcpv4 + RELIABLE + KEEP_ALL 后，
  才能与生产 DirectBytes 公平比较。
- 帧边界：chunk 模式必须携带/推断帧号、chunk index/count 并在 Reader 真实重组；任一
  分片缺失都会使整帧不可用。不能用 received sample 数冒充 received frame 数。
- MPP decode：应在完整 AU 重组后送入 MPP；逐 1 KB chunk 解码会破坏 AU 边界。
- Gray publish：一帧 640,000 bytes；1 KB 分片约 625 Samples/frame，调度和 TypeSupport
  调用量显著增加。
- 丢包路径：BEST_EFFORT、回调阻塞、无界/覆盖队列、重组超时均可能产生不可见的整帧丢失。
  F1 比较强制 RELIABLE、KEEP_ALL、drop/error 数字验收。

## F1 本机功能 benchmark

固定内存 payload、相同 tcpv4/RELIABLE/KEEP_ALL、本机 loopback，所有模式 errors=0、drops=0：

| Payload | 模式 | sourceFrames | DDS samples | receiverFrames |
|---|---:|---:|---:|---:|
| H264-like 100 KB | DirectBytes-AU | 30 | 30 | 30 |
| H264-like 100 KB | ShapeType-1KB | 30 | 2940 | 30 |
| H264-like 100 KB | ShapeType-32KB | 30 | 120 | 30 |
| H264-like 100 KB | Chunk64K | 30 | 60 | 30 |
| H264-like 100 KB | Chunk32K | 30 | 120 | 30 |
| RawGray8 640 KB | WholeFrameBytes | 10 | 10 | 10 |
| RawGray8 640 KB | Chunk64K | 10 | 100 | 10 |
| RawGray8 640 KB | Chunk32K | 10 | 200 | 10 |

该表只证明 wire/reassembly 功能，不是跨 NIC 性能结论。正式公平比较仍需同一 payload、
时长、NIC、发送/接收主机，采集 appCopy/enqueue/queueWait/ddsWrite、CPU、RSS、MiB/s。

## 推荐

- H264：DirectBytes-AU。天然保持 codec AU 边界，Sample 数最少，且 D1-D3 已生产验证。
- Raw：WholeFrameBytes。只有跨 NIC 数字证明 ZRDDS whole-frame 本身是瓶颈时，才在 F2
  评估带明确 frame/chunk identity 的新协议；不得复用 ShapeType 几何语义冒充视频协议。

## RK3588 -> Windows 公平实测（2026-08-29）

测试链路为 RK3588 `192.168.1.116` Publisher 到 Windows `192.168.1.188`
Subscriber。所有模式都使用 tcpv4、RELIABLE、KEEP_ALL、相同 resource limits、
相同 payload、600 帧、60 FPS。Windows 显式绑定直连网卡；板端使用
`tcpv4://default//0`，因为本机 vendor runtime 对板端显式地址配置不能 discovery。
这只是地址选择差异，reliability/history/resource QoS 完全相同。

H264-like 固定 payload 为 102,400 bytes：

| 模式 | DDS Samples | Frames 收/发 | 接收 MiB/s | appCopy ms/帧 | DDS write ms/帧 | 板端 CPU | RSS peak KiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| DirectBytes-AU | 600 | 600/600 | 5.870 | 0.061 | 0.329 | 4.324% | 7,720 |
| ShapeType-1KB | 60,000 | 600/600 | 5.869 | 0.306 | 3.999 | 20.868% | 9,136 |
| ShapeType-32KB | 2,400 | 600/600 | 5.869 | 0.081 | 0.571 | 5.377% | 9,144 |
| Chunk64K | 1,200 | 600/600 | 5.870 | 0.102 | 0.333 | 4.070% | 7,388 |

RawGray8 固定 payload 为 640,000 bytes：

| 模式 | DDS Samples | Frames 收/发 | 接收 MiB/s | appCopy ms/帧 | DDS write ms/帧 | 板端 CPU | RSS peak KiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| WholeFrameBytes | 600 | 600/600 | 36.694 | 0.299 | 1.376 | 9.268% | 9,440 |
| Chunk64K | 6,000 | 600/600 | 36.669 | 0.517 | 1.817 | 12.204% | 7,816 |
| Chunk32K | 12,000 | 600/600 | 36.660 | 0.601 | 2.296 | 14.833% | 7,920 |

七组均为 `errors=0`、`drops=0`、接收完整重组 600 帧。分片没有提升 payload
吞吐；1KB ShapeType 的 Sample 数放大 100 倍，板端 CPU 约为 Direct 的 4.8 倍，
DDS write 总时间约为 Direct 的 12.2 倍。Raw whole-frame 同样比 64K/32K chunk
占用更低 CPU 和更少 write/copy 时间。因此 F1 最终推荐保持 DirectBytes-AU 与
WholeFrameBytes，不把 `ShapeType` 引入生产协议。
