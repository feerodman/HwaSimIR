# P18 客户 TX 视频接口实施说明

本文件描述当前实现，不代表客户已确认所有工程约定。权威原文为 [20261014-DDS视频帧信息.txt](HwaSimIR_P18_Codex_Package/inputs/20261014-DDS视频帧信息.txt)；原件及参考图哈希见 [baseline/identity.json](../logs/p18/baseline/identity.json)。真实客户程序尚未参加测试，互通结论为 **NOT_VERIFIED**。自写独立 Demo 的真实收发证据另列。

## 固定 wire contract

```idl
struct HD264Stream {
    unsigned long DataLength;
    octet VedioStream[1400];
};
struct LD264Stream {
    unsigned long DataLength;
    octet VedioStream[1400];
};
struct HDDecoder {
    long frameId;
    sequence<char,640000> stream;
};
struct LDDecoder {
    long frameId;
    sequence<char,640000> stream;
};
```

| 本地身份 | 模式 | 固定 Topic | 注册类型名 |
|---|---|---|---|
| precise / sensorID=2 | H.264 | TX/HD264 | HD264Stream |
| coarse / sensorID=1 | H.264 | TX/LD264 | LD264Stream |
| precise / sensorID=2 | raw | TX/HDecode | HDDecoder |
| coarse / sensorID=1 | raw | TX/LDecode | LDDecoder |

无 module、namespace、key、扩展字段或版本包头。`VedioStream` 保留原拼写。SDK 生成的 DDS_Long / DDS_ULong 均验证为 32 位，不能以 AArch64 原生 `long` 的内存布局替代 CDR 类型。sequence 的 char 作为二进制字节处理，实际 typed 往返覆盖 00、80、FF。

IDL 位于 `DDS/CustomerTX/IDL`，实际 ZRDDS 生成代码位于 `DDS/CustomerTX/Generated`。Windows VS2015 与 MinGW SDK 的 zrddsgen 输出已逐文件比较；AArch64 编译使用同一 generated 源码和板端 ZRDDS SDK。

## 压缩流工程约定

载荷为 H.264 Annex-B 连续字节流。发送端在**编码 AU 完成、HwaFrameV2::insert 之前**复制干净 AU，独立 worker 按顺序切片；同一 Writer 不交错两个 AU。每个 Sample 的 DataLength 为 1..1400 有效字节，未用数组尾部清零。整除时不增加哨兵；不发零长度 EOS；短片不能作为可靠帧边界。1400 不包括 DDS/CDR/网络传输开销。

不增加客户头、编号、源时间、JSON、HwaSim UUID SEI、状态/标注/EOS Topic。编码器产生的标准 SPS/PPS/VCL/SEI 保留。原分支继续添加 HwaFrameV2，再由原 writer 更新时间戳；客户使用独立 vector，不能被异步 stamp 污染。

RGB24 与原彩色 H.264 内容相同且配置相同时复用编码。Gray8 先明确从显示 BGR 得到灰度，构造 R=G=B 的中性编码输入，再经既有 limited-range YUV 路径编码；不能把原有色码流改个名称充当灰度。色卡的解码 U/V=128 验证另附。未改 SWIR/MWIR 显示曲线。

接收端把有效字节连续交给 av_parser_parse2，允许起始码、NAL、参数集跨 Sample；DDS callback 只校验和复制。首次/迟加入须等待可解码 SPS/PPS+IDR，GOP 默认 30 帧。协议没有重连帧号或丢片恢复消息；无法承诺补回离线期间视频。Demo 本地接收间隔超过 2 秒时 flush 并重建解析器，进程重开同样从新解码上下文开始。有限录像使用明确 idle/timeout 退出并 flush，不能以短包作 EOF。

## raw 与像素工程约定

每帧一个 typed Sample；sequence.length 等于紧密排列的 width×height×通道数。行从上到下，RGB24 字节顺序 R、G、B；Gray8 每像素一字节。原分支 BGR24 不变。灰度由整数 `(77*R+150*G+29*B+128)>>8` 得到。无行 padding，无垂直翻转歧义。Demo 必须显式指定尺寸、像素格式、fps；wire 不提供这些字段，也不自动猜测。

| 几何/格式 | 字节数 | 行为 |
|---|---:|---|
| 320×320 Gray8 | 102400 | 一个 raw Sample |
| 800×800 Gray8 | 640000 | 一个 raw Sample，刚好上限 |
| 320×320 RGB24 | 307200 | 一个 raw Sample |
| 800×800 RGB24 | 1920000 | INIT 拒绝 |
| 1024×1024 Gray8 | 1048576 | INIT 拒绝 |

容量使用安全乘法校验。超限不截断、不缩放、不拆 raw、不改编码；沿用现有非法 INIT 的失败机制：记录 `raw_capacity_exceeded required=... allowed=640000`，不发成功 ACK，不启动该客户回合。未新增失败 ACK 布局。客户关闭时不会限制原合法 1024 输出。

frameId 对齐本回合 logicalFrameSeq，START 从 1 开始，范围 1..INT32_MAX。新回合/进程可重复 frameId；它不能单独消除跨回合歧义。变更尺寸/格式时先 STOP、同步接收配置、RESET/INIT/ACK/START。该约定待客户确认。

## 运行时、QoS 与有界失败

默认 Domain 150、tcpv4、RELIABLE、KEEP_ALL，属于工程约定。共用进程级 DdsRuntimeManager/participant；客户模块不调用全局 Init/Finalize。两个 renderer 进程各 initCount=1。旧 QoS profile 不变。

新增 `customer_tx_h264_writer/reader` 与 `customer_tx_raw_writer/reader`，压缩历史上限 4096 Samples，raw 16 Samples，writer max_blocking_time=100ms，实际 get_qos 再验证可靠性/历史/有限等待。客户帧队列默认 32 帧，允许 1..256；worker 内分片。独立于旧帧队列。默认排空 5000ms（100..10000），发现等待 1000ms（0..5000）。排队溢出、转换/编码/typed write/ACK drain 异常显式 abort 客户回合并记录 accepted/sentFrames/samples/bytes/aborted/rejected/failed；不会无限积压或静默覆盖。

write 返回 OK 只表示 API 接受，最终收齐仍需接收日志/字节摘要。接收进程慢但 DDS callback 快时，压力可能只出现在 Demo 工作队列；这种失败不能冒充已测得发送端 write timeout。没有无限慢消费者同时满足零损失、固定帧率、有限内存的承诺。

## 本地配置

`[CustomerDdsVideo]` 默认关闭。环境变量优先于指定 runtime INI，再取默认。`HWASIMIR_RUNTIME_CONFIG` 指定唯一文件，不可读时报错，避免两个实例悄然共用另一配置。

| INI 键 | 环境变量 | 默认/含义 |
|---|---|---|
| Enable | HwaSimIRCustomerVideoEnable | false |
| Mode | HwaSimIRCustomerVideoMode | auto 跟本次 INIT h264En；可 raw/h264 |
| PixelFormat | HwaSimIRCustomerPixelFormat | gray8；可 rgb24 |
| QueueMaxFrames | HwaSimIRCustomerQueueMaxFrames | 32 |
| DrainMs | HwaSimIRCustomerDrainMs | 5000 |
| DiscoverySettleMs | HwaSimIRCustomerDiscoverySettleMs | 1000 |
| AuditPrefix | HwaSimIRCustomerAuditPrefix | 空；本地审计文件前缀 |
| DiagnosticColorCard | HwaSimIRCustomerDiagnosticColorCard | false；仅诊断客户像素，不是红外场景 |

Mode 覆盖打印 requestedH264/actualH264。channel 继承真实本地 Identity，不再独立配置客户 channel。一个 Domain/Partition/固定 Topic 只部署一个指定生产者；客户结构无 platID/sensorID，不能拆分同 Topic 混入的多个平台。

## 独立接收 Demo

工程 `DDS/HwaSimIRCustomerTxVideoReceiverDemo`，构建 `tools/p18_build_customer_receiver.ps1`，Windows Qt 5.12.12 / MinGW 7.3 / ZRDDS / FFmpeg。不链接 renderer，不包含 CommonData、FrameProductV2，不订阅旧控制/Status/Meta/Annotation。

两个 Demo 实例分别显示 coarse/precise。参数 `--topic --codec --pixel-format --width --height --fps --domain --qos --output --display --idle-ms --timeout-ms --ffmpeg` 必须与本地部署匹配。诊断 `--slow-ms` 可模拟工作队列延迟；`--queue-bytes` 默认 32MiB，上限 256MiB。回调校验失败/溢出显式失败，输出 summary.json、receive.jsonl、原始载荷、首末 PNG、MP4。MP4 帧数由独立 ffprobe 核验，GUI 更新次数与解码次数分列，不冒充实际屏幕呈现时延。

`--callback-block-ms` 默认0，仅用于受控故障注入，范围0..5000；非零时故意破坏正常快速回调约束，不可用于生产配置。已执行的1000ms故障测试中客户进程未在测试截止内结束，外部终止并记录失败；不能据此声称客户完整收齐或已触发发送端SDK写超时。正常回调不sleep、不解码、不写盘。最新版v4将IDR恢复之后真正的解码错误标为failed；恢复之前丢弃的不可解码packet单独计数。可靠流中的收包停顿不代表丢包或回合边界，v4保留NAL解析/参考帧状态；接收进程重新启动时才创建新的SPS/PPS+IDR恢复上下文。实际三秒中途停顿测试见结项记录。哈希使用已有FFmpeg SHA256，保留完整流、Sample和像素审计。

无EOS的压缩流最后一帧可能留在解析器直到明确idle/timeout flush；正式双路测试idle=12000ms，末帧约12秒后完成。这是已观察的接收收尾边界，不符合末帧80ms屏幕时延保证，不通过添加私有结束Topic或零长度Sample绕过协议。

未约定/未验证：真实客户 SDK 与 QoS 兼容、客户 Domain/Partition、客户帧号跨回合解释、真实传感器标定、经时钟校准的跨主机端到端和屏幕呈现延迟。四个业务 Topic 不承担控制功能；renderer 仍需要原有 INIT/START/Realtime。
