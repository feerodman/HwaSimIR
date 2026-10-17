# P18 执行任务：尾焰外形恢复、客户 ZRDDS 纯视频双通道输出与 DDS 链路交付

你在 feerodman/HwaSimIR 当前工作区直接实施本阶段。完成代码修改、构建、可用环境内的部署、真实收发测试、媒体检查和交付文档，不要只提交设计方案，也不要用模拟器自测代替真实生产链。

## 0. 基线与范围

先读取仓库中的 AGENTS.md/相关开发约束及下列材料：
- docs/HwaSimIR_P17_Closeout.md
- docs/HwaSimIR_P17_Followup.md
- docs/HwaSimIR_P17_SWIR_MWIR_Retest.md
- docs/DDS_HwaSimIR_ICD.md
- docs/HwaSimIR_A1_DDS_Production_Integration_Acceptance.md
- 当前运行配置、DDS IDL、ZRDDS 运行时管理代码和实际构建/部署脚本。

客户协议以任务附件“20261014-DDS视频帧信息.txt”为准；两张上传图片是尾焰外形参考。随本 Prompt 的资料包 inputs 中也提供原协议和 plume_reference_1.png、plume_reference_2.png。记录输入文件哈希，不能把附件缺失写成已经完成精确对照。

记录当前 HEAD、git status、实际 EXE/ELF、工作目录、资源/配置绝对路径及哈希。保留已有未提交修改；不要整体回退 P17，不要清空历史证据；没有另外授权时不自动 commit/push。

本阶段边界：
1. 用户已认可当前云、雨、雪效果，将其作为保留基线，不再开展天气重构或重新调亮。
2. 尾焰只做外形恢复及必要的局部修复，保留 P17 对数深度、MWIR 固定 HDR、云纹理上传/绑定等修复。
3. 保留 HwaSim_IR → HwaSim_IR_VideoDisplay 现有 DDS 视频、实时数据、帧身份和标注能力，新增客户纯视频分支，不替换原分支。
4. 不改旧 CommonData/IDL 布局，不修改原始 1.txt、原模型文件、首有效位置规则、ViewValid 和稳定编号(x,y)语义。
5. 不做新的标定、大气数据扩展或全面纹理重构。历史未关闭项单列；真实标定继续 NOT_VERIFIED_CALIBRATION。

## 1. P18A：先审计真实链路，再动手

重点入口：
- HwaSim_IR/HwaSim_IR/main.cpp、HwaSimIR.cpp
- HwaSim_IR/HwaSim_IR/TcpCommThread.cpp、TcpCommThread_Linux.cpp
- HwaSim_IR/HwaSim_IR/Video/DdsVideoPublisher.cpp
- HwaSim_IR/HwaSim_IR/Video/ 下的编码器与本地录像实现
- DDS/Runtime/DdsRuntimeManager.h/.cpp
- DDS/Protocol/ProtocolRoute.h、FrameProductV2.h，以及协议 endpoint/topic resolver
- DDS/IDL/HwaSimIRProtocolV1.idl 及实际 generated 文件
- HwaSim_IR/Bin/Config/NetworkConfig_precise.ini、NetworkConfig_coarse.ini、HwaSimIRRuntime.ini
- HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/ 下的 DDS 接收、解码、帧对齐和保存代码
- DataDrivenTestQT、已有 DDS 激励/接收 Demo。

审计必须画出两张图：现有生产链；新增客户链后的完整链。逐项追踪：
启动参数/环境变量 → 实际配置文件 → 本地身份 → RESET/INIT/ACK/START/Realtime/STOP → 渲染/读回 → 像素整理/编码 → 每个 DDS Writer → 接收/解码/显示/保存。

当前已核对的线索必须再次按工作区验证：
- coarse 默认 platID=1001、sensorID=1；precise 默认 platID=1001、sensorID=2。
- Control 按 platID 路由，没有 sensorID；Init/Realtime 按 platID+sensorID 或允许的 sensorID=255 广播路由。
- INIT 里有 trackerSensorWidth、trackerSensorHeight、h264En，不能仅凭 precise/coarse 名称写死分辨率或压缩模式。
- DdsRuntimeManager 负责进程级 Init/CreateDP/Finalize；协议与视频借用同一运行时。
- 原视频 Writer 使用 DDS::Bytes；现有 H.264 AU 在发送前还会注入 HwaFrameV2 私有 SEI，DdsVideoPublisher 内还会更新时间戳。旧文档中“只含视频、无自定义数据”的描述不能直接代表当前代码。
- sendFrameThreadFunc 中 independentOutput、needH264、roundActive 等输出条件不能遗漏客户新分支。

同时解释 platID、sensorID、platParamInit.id、targetPlatID、targetID 的不同作用，避免把被观测目标身份当作本地传感器身份。梳理 CLI、环境变量、INI、本地回退、INIT 的真实优先级。若文档与源码冲突，保留历史说明并修订当前操作文档，不能按过期文档反向改坏代码。

审计产出一个简明现状记录后继续实施，不要停在审计报告。

## 2. P18B：恢复参考图中的尾焰外形

目标是参考图所示的细长、连续、柔边羽流：喷口附近有窄亮核心，外层柔和，向后逐渐消散，有轻微非规则变化；不是粗白棒、分段卡片、梳齿、明显圆盘/球团或硬边锥体。参考图里的方块对象不是必须照抄的机体形状。

沿 Git 历史追踪尾焰几何、profile、纹理和 shader，定位此前接近参考效果的实现；先保存当前 P17 尾焰 before，再做局部恢复。恢复的是外观目标，不是原封不动恢复旧 32 张周期 billboard 的离散缺陷。

优先调整尾焰专用的长度/半径包络、core/halo 比例、密度/透明覆盖、轴向衰减和非周期噪声。保留正确喷口位置、模型坐标/米制尺度、遮挡、深度一致性和辐射源语义。不要通过整机加热、提高全局曝光、逐图 AGC、改雨雪温度、屏幕后贴图或扩大饱和白块达成效果。

检查侧视、斜视、端视、近远距离、运动和 engineState 开关；同一状态隔离 core/halo/mask 排查，但诊断图不能替代普通生产输出。SWIR/MWIR 都检查，800×800 为主要视觉证据，并在 320×320 确认不重新出现明显硬块/断裂。

before/after 使用同模型、同输入状态、同实际相机/FOV、同固定显示曲线。保存普通输出 PNG、完整视频与必要局部裁剪；至少覆盖轨迹后段，不能只选开头的好看帧。视觉目标属于工程外观恢复，不声称取得真实辐射标定。

## 3. P18C：客户协议固定，不增加字段或应用 Topic

必须使用 ZRDDS typed DDS，严格使用以下四个 Topic 和四个类型。VedioStream 的原拼写必须保留：

| 通道 | 模式 | Topic | Type |
|---|---|---|---|
| precise | H.264 压缩 | TX/HD264 | HD264Stream |
| coarse | H.264 压缩 | TX/LD264 | LD264Stream |
| precise | 未压缩 | TX/HDecode | HDDecoder |
| coarse | 未压缩 | TX/LDecode | LDDecoder |

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

建立独立客户 IDL/generated 目录，使用当前 ZRDDS IDL 工具生成类型支持，接入实际 Windows/AArch64 工程。不得把自定义 struct 的内存、序列化后的 struct 或旧 DDS::Bytes 当作上述 typed DDS 类型替代品。

不得改变字段名、顺序、IDL 类型、数组/sequence 上限、大小写；不得擅自加 module/namespace 改变对外注册类型名、添加 @key 或扩展成新版本结构。C++ 可以做不改变 wire type 的内部封装。核对 ZRDDS 的实际注册类型名与生成器输出。

IDL long/unsigned long 按 IDL 类型处理，不按 Linux 原生 C++ long 大小 memcpy；验证生成类型宽度及 sequence 二进制字节无损，包括 0x00、0x80、0xFF。

客户接收端仅需这四类业务类型/对应 Topic，不依赖 HwaSimIR.VideoStatus、VideoMeta、Annotation、FrameProductV2、CommonData 或我方控制 Topic。不要新增客户版 Status/Meta/ACK/EOS Topic 补协议字段。DDS 自身发现、可靠传输和状态回调不属于新增应用协议。

附件未规定 Domain、传输/QoS、像素顺序、DataLength 详细语义和 frameId 回合语义。以下实施约定要明确标注为本阶段工程约定，而不是声称客户已确认；真实客户端未参加测试时，客户互通状态只能是 NOT_VERIFIED，不能用自写 Demo 代签。

## 4. P18D：载荷语义、容量与纯视频边界

### 4.1 压缩视频

沿用项目已有 H.264 Annex-B 编码能力；不是 MP4 文件片段，不是 JPEG，不是 TCP Packet v3。

在一个完整、尚未加入 HwaSim 私有数据的编码 AU 上依次切片，每片最多 1400 字节。DataLength 本阶段约定为本次 Sample 的实际有效视频字节数，范围 1..1400；完整片为 1400，最后不足 1400 的片填写实际长度。数组无效尾部清零，接收端只消费前 DataLength 字节。

不加包序号、帧号、总长、包头、时间戳或私有 SEI；不用 DataLength 高位编码其它信息；不发零长度结束包；不把不足 1400 当作可靠帧结束标记，长度刚好整除时也不补哨兵包。

同一 Writer 串行发送，保持 AU 内和 AU 间字节顺序，不能让两个帧的分片交错。接收端按连续字节流增量解析 H.264，允许 start code、NAL、SPS/PPS、SEI 和画面数据跨 DDS Sample 边界；一包不等于一帧。

1400 是客户字段的有效载荷上限，不等于网络包总长，不宣称包含 DDS/CDR/TCP/IP 头以后仍是 1400。

### 4.2 关键实现：在私有 SEI 注入前分流

编码器输出 clean AU
  ├─ 客户分支：保存独立/不可变 clean AU → 1400 切片 → TX/HD264 或 TX/LD264
  └─ 原分支：保留原 HwaFrameV2::insert/stampWriter → 原 DDS/VideoDisplay/录像

不能直接分包转发现有 m_ddsPublisher 收到的已注入载荷。新增客户 Writer 也不能调用会自动插入/更新时间戳 SEI 的旧写入逻辑。

客户流不含 HwaSim 的身份、标注、实时数据、JSON、私有包头或 HwaSim UUID 的 user_data_unregistered SEI。不要为此粗暴删除所有标准 SEI 或破坏 SPS/PPS/VCL；优先从编码器干净输出处分流。

原链路依赖的私有 SEI、辅助 Topic 和存储语义完整保留。两个分支不得因共享可变 vector、异步重用/扩容或 writer stamp 操作互相污染。同像素内容且编码配置相同时复用一次编码，不为多一个发送对象无故编码两次；原彩色与客户灰度不同时，不得假装两者可以直接共用同一有色码流。

“纯视频”指客户 wire 中不夹带私有业务数据；已有烧录到像素里的编号/标注沿用原开关，本阶段不擅自改变现有默认画面。

### 4.3 未压缩视频

每帧只发送一个 HDDecoder/LDDecoder Sample。sequence.length 必须等于实际像素字节数；640000 是容量上限，不是所有帧都填满。不对 raw 做应用层 1400 分包，DDS 底层分段由中间件处理。

至少支持 Gray8 和真正 RGB24；旧链路使用 BGR24 时进行明确通道转换，不能将 BGR 数据标称为 RGB。行紧密排列，无 padding；统一上下方向。原分支的 BGR24 契约不能一起改掉。

发送前用安全乘法核对 width*height*bytesPerPixel：
- 320×320 Gray8：102400 字节，允许。
- 800×800 Gray8：640000 字节，允许。
- 320×320 RGB24：307200 字节，允许。
- 800×800 RGB24：1920000 字节，拒绝。
- 1024×1024 Gray8：1048576 字节，拒绝。

不能截断、只取前 640000 字节、偷偷缩放、拆成多个 raw Sample、改成 JPEG/H.264 或扩大 IDL 上限。不能因此把整个渲染器最大分辨率改成 800；1024×1024 的其它合法输出继续保留。

客户 raw 分支启用且 INIT 要求超限时，用现有初始化失败机制明确拒绝该配置，记录需要/允许字节数；不返回“客户链路成功”。客户分支关闭时不得限制原链路的合法分辨率。

raw 没有宽高/格式字段：接收端通过本地命令/配置明确输入 width、height、Gray8/RGB24、fps、像素方向；这些约定不塞进 stream。运行时改变它们需要接收端同步重新配置。

frameId 本阶段建议与该回合既有 logicalFrameSeq 对齐，每次 START 从 1 开始；实施前检查客户附件/仓库是否另有明确规定。有其它权威规定则按规定执行并记录差异，否则将该建议写为待客户确认的工程约定。明确进程重启/回合复位边界，不声称仅凭 frameId 可无歧义识别跨进程、跨回合帧。

### 4.4 灰度压缩

H.264 灰度模式必须保证编码输入无彩色分量，正确处理亮度和中性色度；不能仅改格式字符串。用灰阶/黑白测试卡检查范围、单调性、上下方向和偏色。不得为了灰度传输调整 MWIR/SWIR 显示曲线或重新量化已生成的传感器画面。颜色模式使用明确 RGB/BGR→编码器格式转换。

## 5. P18E：最小集成、运行时与背压

采用独立小模块，例如 CustomerDdsVideoPublisher；Windows 与 Linux 共用协议/打包逻辑，分别补齐现有输出 worker 的集成点，不复制两套易漂移的打包代码。

客户新分支可独立开关，原 [DdsVideo] 保持原义。可增加独立 [CustomerDdsVideo] 配置段：启用、压缩模式(auto/h264/raw)、像素格式(gray8/rgb24)、队列/写入/排空限制及所需 QoS 选择。最终字段名以最小实现为准，文档必须与解析器逐项一致。

channel 继承该进程的真实本地身份，不另造一套与 sensorID 冲突的通道状态。Topic 由 channel+mode 映射到附件固定字符串，不自动追加 platID/sensorID，不把 precise 永久绑 H.264、coarse 永久绑 raw。

压缩模式 auto 跟随本次有效 INIT h264En；显式覆盖必须有请求值/实际值日志。像素格式是本地明确配置，不暗改旧 INIT 布局。

默认复用已有进程级 DdsRuntimeManager、factory 和 participant。不得再调用一套全局 DDSIF::Init/Finalize；客户分支退出不能析构原分支所用运行时。两进程各拥有自身运行时，runtimeInitCount 每进程为 1。

客户未提供其它 Domain/QoS 时，先沿用经验证的 Domain 150、tcpv4、可靠有序传输作为工程默认，保持旧 QoS 行为不变；核对实际 profile 和资源限制。不要因为“1400 分包”擅自切到 BEST_EFFORT/UDP。若需要不同 participant/domain，只允许经同一运行时管理器明确管理，不复制全局初始化；不为尚无需求的跨 Domain 功能做大重构。

客户队列按完整帧/AU 有界管理，worker 内分片；不要让 1400 分片直接占用旧“帧队列”语义。不在渲染线程中循环阻塞 write；复用读回和必要像素转换，确保缓冲生命周期。

正常消费者下要求无丢包/丢帧。慢消费者下使用有界排队及有限写入/排空等待；长期积压必须显式报告客户链路失败/中止并记账，不能无限增长、死锁、覆盖旧帧或继续假报零丢。新分支故障不得无限期拖住原链路、渲染或 STOP。不能承诺无限慢消费者、零丢失、有限内存、固定帧率四者同时成立。

压缩中断后不能从任意残缺分片继续声称完整；用已有关键帧机制、周期 SPS/PPS+IDR 和接收端解码复位恢复。除健康运行计数外，明确记录中止/未交付数据及恢复边界。

## 6. P18F：独立运行与双通道配置

不能依赖 VideoDisplay 的进程存在、TCP 连接、DDS Reader 匹配、视频窗口创建或回传数据才出图。控制/激励由 DataDrivenTestQT 或既有 DDS Sender 提供；“不依赖 VideoDisplay”不等于“不需要 INIT/START/Realtime”。

全面检查两平台的“是否有输出”“是否读回”“是否编码”“是否进入有效回合”“STOP 是否排空”等条件，纳入客户分支。尤其验证旧 DdsVideo=false、录像=false、TCP payload 全关，而客户分支开启时仍然可以输出。

沿用合法 INIT 宽高调整渲染缓冲、最终读回、编码器和原 VideoStatus，实际输出必须是请求尺寸，不是把统一 800 图缩成 320 后冒称原生 320 渲染。查清 HeadlessWidth/Height 与 INIT 的优先级，不允许离屏纹理始终 800 而状态报告 320。

生成互不污染的两套启动/运行配置和脚本，不能由两个进程轮流覆盖同一个 INI；确认现有每进程 runtime 配置覆盖机制，确需补充时只增加必要能力。两套日志、录像目录、DDS licence 运行副本等按实例隔离；不打印 licence 内容，不改系统永久网络/启动设置。

正式双路配置：
A. coarse：platID=1001、sensorID=1；INIT 320×320、h264En=false；客户 Gray8 → TX/LDecode；每帧 102400 字节。
B. precise：platID=1001、sensorID=2；INIT 800×800、h264En=true；客户灰度 H.264 → TX/HD264；每 Sample 有效载荷不超过 1400。

两路运行同一可执行程序的两个实例，优先在同一目标 RK3588 上同时运行；不能用先后运行或两段离线重放代替。原 DDS 分支保持开启，另有原分支关闭/客户独开的隔离测试。

激励端：
- 一份未经修改的原始 1.txt，完整 4318 行，不改变源时间、目标/平台状态及 ViewValid。
- 分别向 1001/1 与 1001/2 发送不同尺寸/压缩设置的 INIT，等两份真实身份 ACK，之后按现有状态机 START。
- 不发送一份广播 INIT 覆盖两种分辨率。
- Realtime 优先利用已验证的 sensorID=255 广播，让两个实例获得同一个业务样本；也可由一个调度器向两个精确 sensorID 写入仅路由字段不同、业务内容相同的样本。选择一种方式，不能同一行又广播又单播造成重复。
- 两路各自接受的业务样本、sourceSeq/源时间和目标状态离线核对一致；原前 659 行隐藏目标语义保留，不能跳过背景帧。
- Control 是平台级，两路共用 1001 时 START/STOP/RESET 会作用于两路；不要宣称现协议可单独 STOP 同平台某 sensor。
- 明确分辨率、像元角与总 FOV 的关系；同激励不等于两路像素完全相同，也不等于两路自动保持相同总 FOV。

附件结构没有 platID/sensorID。一个 Domain/Partition/Topic 中只允许一个指定客户视频生产者；同一个 TX/HD264 上混入两路不同平台的视频无法靠此结构区分。把此部署边界写进说明，不偷偷扩展字段。

## 7. P18G：独立客户接收 Demo

提供能编译运行的最小接收工程及完整源码，推荐独立名称 HwaSimIRCustomerTxVideoReceiverDemo，避免覆盖已有不同协议的 CustomerReceiverDemo。

仅依赖 ZRDDS、四个客户生成类型及实际需要的解码/显示库。它不链接 HwaSim_IR，不包含 FrameProductV2/CommonData，不订阅旧 Status/Meta/Annotation，不靠旧旁路数据才能还原图像。

能同时展示 coarse/precise 两路，也允许启动两个 Demo 实例。命令行/配置明确提供 Topic、Domain、QoS、codec、width/height、像素格式、fps和保存目录；不要凭字节数自动猜分辨率和 RGB 顺序。

压缩回调检查 DataLength，复制有效字节后快速入队，由 worker 连续增量解析/解码。不能逐包当完整帧送解码器，不能在 DDS callback 阻塞做显示/磁盘写入。处理跨包 start code、晚加入、SPS/PPS/IDR、连接重建和结束时 decoder flush。

raw 按单 Sample 单帧校验 sequence.length 与预配置尺寸，显示并记录 frameId。保存原始 .gray8/.rgb24 与可直接查看的媒体；压缩保存重建 .h264，并在允许条件下封装 MP4。没有自定义 EOS，有限录像由测试控制、操作或明确 idle timeout 结束并 flush，不能把“短包”等同 EOF。

提供 Windows 可用构建入口；Qt 方案沿用项目 Qt/MinGW 工具链并写清 .pro/.pri 和依赖，主渲染器继续保持 VS2015 v140 和 AArch64 构建兼容。实际没有完成某平台构建，标 NOT_VERIFIED/BLOCKED，不能把另一平台编译结果代替。

## 8. P18H：测试与验收

### A. 协议与分包单测

验证四个 typed Topic/类型注册及实际收发，不仅验证 memcpy。分包边界至少覆盖 1、1399、1400、1401、2800、2801 字节；检查整除无额外结束包、尾部长度、无效 DataLength、字节顺序及缓冲生命周期。另用真实 H.264 测试 start code/NAL 跨包解析。

核对发送 clean AU 拼接流与接收重建 .h264 的 SHA256/字节数一致；分片总数等于各 AU ceil(bytes/1400) 之和。压缩有损，不能要求解码像素与编码前像素逐位相等。

检查客户流无 HwaSim 私有 SEI/包头；同时验证原 HwaSimIR 流仍包含可解析帧身份、实时数据和既有标注语义。原 writer stamp 与客户异步发送并行运行时也不能互相污染。

### B. 必须完成的真实双路主验收

同一 RK3588 同时启动两个 HwaSim_IR：粗跟 320×320 raw Gray8，精跟 800×800 H.264 灰度；同一份激励并行输出。Windows 客户 Demo 同时实际收到并显示两路。

原 1.txt 完整回放，各路逐行审计，预期各自 4318 个逻辑产品；以实际接受/输出/收到/解码/保存计数及 STOP 排空核验，不用 MP4 标称帧率、重复尾帧或 unique pixel hash 代替新帧计数。不得删除首尾失败帧后宣称完整。

保存两路完整接收媒体、原始载荷/摘要、发送/接收日志和一个双路同屏证据。压缩包数与视频帧数分开统计，raw 核对每帧 102400 字节。相同激励以发送日志和两路已接受业务状态为证据，不以截图相似为证据。

### C. 灰度/RGB与四 Topic 覆盖

四个客户 Topic 均要有真实生产发送端到 Demo 的正向案例；补齐非主验收组合，不能只实现 HD264 与 LDecode。Gray8、RGB24 两类路径均验证；RGB 用诊断色卡检查 R/B 顺序，并明确诊断素材不是红外生产画面。

raw 800×800 Gray8、raw 320×320 RGB24 正向通过；raw 800×800 RGB24、raw 1024×1024 Gray8 明确拒绝且不写入超限 Sample。验证新分支关闭不限制旧链路合法大分辨率；压缩保持当前合法最大尺寸支持。

### D. 独立性和生命周期

分别测试：
1. 全程不启动 VideoDisplay，两路客户接收正常；不存在隐藏的 VideoDisplay 后台进程。
2. 原 DDS 与客户链同时启用，VideoDisplay 和客户 Demo 各自正常；运行中退出/重开 VideoDisplay 不影响客户。
3. 旧视频 DDS、TCP payload、本地录像关闭，仅客户分支开启，仍有真实新帧。
4. 客户不开、晚加入、退出重开、短时慢接收及受控超时；原链路不被无限阻塞，不发生内存无界增长或静默丢包。
5. RESET→分路 INIT→两份 ACK→START→Realtime→STOP，至少两个回合；再做允许的重新初始化/尺寸切换与接收端同步重配置。
6. 不串 sensor/plat，不串回合；不把广播身份 255 当成本地传感器身份；没有两个 Writer 向同一个固定客户 Topic 混流。

### E. 尾焰/天气和旧链路回归

普通生产路径验证 SWIR/MWIR 尾焰参考外形；固定显示设置，保存 before/after。云、雨、雪做最小保留性回归，不重新调参。确认对数深度、原始模型/输入哈希、数字标注、原 DDS 视频+数据+录像行为未回退。

VideoDisplay 同时作为独立的原链路回归对象，不把“客户 Demo 能解码”当成原链路无回归。

### F. 性能

对比本阶段新分支关闭/开启的同条件双实例开销；提供预热后至少 60 秒的两路 render/capture/output/receive/decode 帧率、排队长度、写入耗时、复制/转换/编码耗时、内存、错误/丢失及 STOP 排空时间。

继承项目 60 FPS / 80 ms 门槛，不把 58/59 或 59.99 自动改判 PASS，不降分辨率、跳行、降低帧率或放宽门槛。功能、视觉、性能、真实客户互通分别判定，不能因性能不达标就隐去已完成的功能，也不能因有画面就宣称整体通过。

客户 wire 没有自定义时间戳，跨主机时延通过双端本地审计/字节或帧对应与时钟校准测量；审计文件可以落盘，但不能加入客户载荷。无法可靠对齐时明确 NOT_VERIFIED；发送调用耗时不冒充端到端延迟，解码提交不冒充屏幕呈现。

缺少板卡、有效 SDK/licence、工具链或不能真正操作 GUI时，如实记录 BLOCKED/NOT_VERIFIED，并完成其余可执行项目；不捏造日志、截图、运行记录或全量 PASS。

## 9. P18I：最终交付

至少交付：
- docs/HwaSimIR_P18_Closeout.md：逐项结果、问题根因、修改范围、剩余边界。
- docs/HwaSimIR_DDS_EndToEnd_Guide.md：完整链路图、各 Topic/Type/方向/作用、帧/包语义、启动和停止顺序、发现/身份/过滤、ID 与粗精跟映射、源码“文件+函数”索引。
- docs/HwaSimIR_Customer_TX_DDS_ICD.md：四结构原文、IDL/类型注册、1400 分包、raw 容量、像素格式、DataLength/frameId 工程约定、Domain/QoS、单生产者限制、迟加入/重启和未知项。
- docs/HwaSimIR_P18_DualChannel_Runbook.md：可复制的双进程/激励/接收命令、全部实际配置、正常操作顺序、无需 VideoDisplay 的操作步骤与预期日志。
- 客户 Receiver Demo 的源码、IDL/generated、构建入口、依赖说明及实际构建产物；需要的 .pri/.pro/CMake/VS 工程纳入仓库。
- logs/p18/ 下的可打开证据索引、完整媒体、配置、manifest、机器可读计数、测试结果和回滚说明。大文件按现有仓库/交付规则处理，文档中的链接必须实际存在。

DDS 总说明必须区分：旧 typed 控制/状态链；旧 DDS::Bytes 视频链及 HwaFrameV2 SEI；旧 Meta/Annotation 辅助 Topic；可选 decoded gateway；新增四个 TX 客户视频 Topic；显式 legacy UDP/TCP。不能只列名称、不说明哪些当前启用及谁负责生产/消费。

最终回复按用户的 10 个需求逐条给结果，并单列：
- 实际改动文件与关键函数；
- 两路真实启动命令、身份、分辨率、编码/像素格式、Topic；
- 发送/收到/解码/保存帧数与压缩分片数；
- 尾焰 before/after 和双路同时出图证据；
- 原链路兼容、独立运行、功能/性能/视觉各自结论；
- 自写 Demo 互通结果与真实客户端尚未确认项。

不要把“编译通过”“协议单测通过”“视频可播放”直接写成整个 P18 PASS。完成当前环境能够执行的真实修改和验证后再交付。
