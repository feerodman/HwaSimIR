# P19 Codex 执行 Prompt
## 客户 DDS 故障恢复、H.264 末帧收尾、双传感器测试 UI、硬解配置与板端版本收口

直接在当前 HwaSimIR 工作区完成实现、构建、当前可用环境中的真实验证和交付。不要只写方案。使用中文报告。以实际源码、运行日志和用户最新要求为准，不机械继承 P18 的全部验收门槛或历史结论。

## 0. 本轮范围与优先级

用户最新决定：

1. P18 尾焰已经认可，暂时不要动。天气、对数深度、固定 HDR、模型、现有标注外观同样冻结；本轮不是图像效果阶段。
2. 首要解决客户 `TX/LDecode` 未压缩视频触发发送失败，以及客户退出后仍需重启 renderer 才能恢复的问题。
3. 解决压缩最后一帧依赖约 12 秒 idle flush 的接收收尾问题。
4. 把 DataDrivenTestQT 做成方便手工测试的双传感器 UI：两路独立初始化/身份/精粗角色，默认共用同一目标和 `1.txt`，允许替换数据源和模型，勾选发一路或两路。
5. 整理、简化客户接收 Demo，增加可读中文注释，提供可交给客户的最小 raw 接收例子。
6. 客户现场 DDS Domain 是 1；我方原工程默认 150。明确配置和实际生效域，不靠散落的代码常量修改。
7. 分析附件 H.264 参考，说明现有编码参数，补齐必要的硬解兼容配置，不能照搬相互覆盖的参考代码。
8. 板卡清理仅保留最新与上一版。用户的 P17 已保存为 `Config_new` 和 `HwaSim_IR_new`，必须识别和保护，不能误判丢失或直接覆盖。

性能目标仍是两路正常约 60 FPS、约 80 ms 的既有使用目标，但本轮不是冲刺严格性能门槛。P18 的 59.9897 FPS / 稳态最大 83.046 ms 属已知实测；不要为了这些小偏差开展大规模渲染优化、改画质或阻塞其它功能交付。保留真实测量，明显持续退化、秒级阻塞、无界积压仍必须处理；不把近似达标写成严格 `>=60 / <=80` 全部通过。

不得改四个客户 IDL 的字段/顺序/拼写/类型名，不新增客户私有包头、SEI 业务数据或控制/状态 Topic，不把 raw 改成应用层 1400 分包。原 HwaSim_IR → VideoDisplay 的视频、SEI、实时数据、标注和录像兼容能力必须保留。

## 1. 先读取并核实的材料

### 1.1 仓库

本次外部审阅时 main HEAD 为 `fded9de2f0ff652aab3afdfee3d350910def5baf`，提交标题为 P18 DDS 视频数据直出与尾焰修复。它仅是审阅定位信息，执行时以实际工作区为准，不 checkout/reset 到旧版本，不丢弃未提交文件。

先读根目录/子目录适用的 AGENTS.md、CLAUDE.md 等项目规则，再读：

- `docs/HwaSimIR_P18_Closeout.md`
- `docs/HwaSimIR_DDS_EndToEnd_Guide.md`
- `docs/HwaSimIR_Customer_TX_DDS_ICD.md`
- `docs/HwaSimIR_P18_DualChannel_Runbook.md`
- `DDS/CustomerTX/IDL/*`、`DDS/CustomerTX/Generated/*`
- `DDS/CustomerTX/CustomerVideoContract.h`
- `DDS/CustomerTX/CustomerTxWriter.h`
- `HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.*`
- `DDS/HwaSimIRCustomerTxVideoReceiverDemo/main.cpp`
- `DataDrivenTestQT/mainwindow.*`、`main.cpp` 及现有 UI/配置文件
- `DDS/Protocol/DdsStimClient.*`、`ProtocolRoute.h`、`HwaSimIRProtocolEndpoint.*`
- `DDS/Runtime/DdsRuntimeManager.*`
- 两平台 `TcpCommThread.*` / `TcpCommThread_Linux.*`
- `Video/VideoEncoder.h`、`H264MppEncoder.cpp`、`H264FfmpegEncoder.cpp`
- runtime/network INI、ZRDDS QoS XML、现有 P18 构建/双路/部署脚本。

### 1.2 用户附件与现象

读取 `粗跟DDS错误.md`、`20261014-DDS视频帧信息.txt`、`H264参考文件.rar` 及本资料包提取的原始文件。附件中的旧 VLC 安装程序不需要安装或运行；不要执行未知二进制，也不要将旧播放器随客户 Demo 发布。

用户现场现象：

- 客户接收精跟 H.264 时可以正常软件解码（用户确认；不是本轮亲测硬解结果）。
- 客户不订阅粗跟 raw 时，我方 Demo 接收 raw 正常。
- 客户订阅粗跟 raw 后，renderer 出现发送失败。
- 客户随后退出 raw 订阅，但原 renderer 进程不重启，我方 Demo 仍收不到；重启 renderer 后又正常。

本轮测试不要把“客户 H.264 软件接收已由用户验证”继续一概记作完全未知；应与“本轮自动证据”“客户 raw 未通”“客户硬解未验证”分开标注。

### 1.3 当前已观察的代码事实，必须复核

- 错误日志先出现 `(TX/LDecode) Send message failed:7, locator deleted.`，后出现 `Waiting sample (sn:1) space failed: 10`，再出现 `[CustomerTX][ERROR] ... action=abort_customer_round old_branch_continues=1`。
- 同一日志旧 `HwaSimIR.Video.1001.1.RawGray8` 继续有发送计数，不能把旧链成功误认为 TX/LDecode 成功。
- `CustomerDdsVideoPublisher::Impl::failLocked` 会设置 failed/cancel、不再接受客户帧并清空待发队列；submit 随后持续拒绝；beginRound 才重新清状态并 open writer。这能解释“撤掉客户后同一回合不自动恢复”，但不是最初 transport failure 的根因证明。
- `CustomerTX::Writer::send` 目前把不同失败折成 bool，丢失真实 write 返回码及发生阶段。
- raw Writer/Reader QoS 是 RELIABLE、KEEP_ALL、max_samples=16，write 等待 100 ms；压缩相应上限 4096。`max_samples` 是样本数，不是单样本字节容量。
- Demo 在定时器 idle/timeout 后退出，然后 receiver.stop → flush parser/decoder；P18 测试 idle=12000。v4 已撤销“停顿两秒就重置解码器”的错误做法，不得恢复。
- P18 双路控制使用固定身份/尺寸/ackMask，以及 Realtime sensor255 广播；不适用于任意身份、只选一路或两路不同模型的新 UI。
- P18 文档写了 channel 强制匹配 sensor1/2；本阶段需取消这种语义耦合，1/2只保留为默认值，不修改旧控制 IDL。

## 2. P19A：客户 raw 故障定位与证据

先区分两个问题，不要直接归责客户：

A. 客户 Reader 加入后，为什么大 raw Sample 发送失败？
B. 出错后我方客户分支为什么持续停发，如何在 renderer 同一进程内恢复？

### 2.1 日志与诊断最小补齐

保留错误码原值及实际调用 API，记录 domain、partition、Topic、type name、当前角色和本地身份、frameId、stream.length、实际 payload 字节、write 耗时、应用队列深度、writer 接受/失败计数和发生阶段。使用实际 SDK 支持的接口记录匹配 Reader 数量、匹配变化、incompatible QoS、sample rejected/lost、liveliness，以及可读出的远端 GUID/locator、transport 和资源限制。

`Send message failed:7` 是内部日志码，不能直接当 errno=7 或某个 DDS ReturnCode；`space failed:10` 同样要结合实际 SDK 定义/调用上下文确认。能查到则引用实际头文件或厂商文档；查不到保留 `UNKNOWN_VENDOR_CODE`，不得猜出一个确定解释。

不要打印 licence 正文/密钥或客户敏感数据。默认低频汇总，出错完整快照；不要每像素/每包大量打印影响正常使用。

### 2.2 优先检查维度

1. DDS transport 和大消息：双方到底是 tcpv4 还是 udpv4；实际发布的 locator 是否可达、是否指向错误网卡；SDK 是否允许该尺寸的序列、单 Sample 大小、重组及发送/接收缓存。raw320一帧102400字节，还要加 CDR/RTPS/transport 开销；压缩单片仅1400有效字节，两者不等价。
2. QoS：真正 get_qos 的 reliability/history/resource_limits、max_blocking_time、durability/partition，而不只查看 XML 文本。检查 profile 名是否加载正确、发送和接收用的是不是各自预期的类型配置。保持可靠且有界，不通过 BEST_EFFORT 或无限缓存掩盖问题。
3. 客户类型：`LDDecoder` 全局类型，IDL long 是32位、frameId在前、bounded sequence<char,640000>在后、无key；不能把 sequence 当固定 char数组、native struct裸 memcpy 或字符串。不用 sizeof(sample) 作为序列内容长度。
4. 客户接收：是否持续取样、是否把解码/显示/磁盘写入放进 DDS callback；是否正确归还 loan/遵循 SimpleDataReaderListener 的所有权规则；是否持有已经失效的 sequence 指针。
5. 资源与性能：raw320 Gray8 @60 的有效载荷约6.144 MB/s（49.152 Mbit/s）；单个reader跟不上或链路重组失败可能产生可靠历史积压。这是诊断假设，不是仅凭日志可宣布的结论。

当前16个raw历史样本约对应60Hz下267ms生产量。它与错误发生时间相近也不能作为根因证据，必须看真实匹配、ACK、队列及资源日志。

### 2.3 最小测试矩阵

先写只依赖 ZRDDS 和原始 generated 类型的 raw 测试发送/接收路径，排除 renderer、Qt图像显示和磁盘录像影响。依次测试合法帧对应的不同 sequence长度：小于1400、约4KiB、16KiB、约64KiB两侧、102400、307200、640000；记录实际长度。测试非图像长度时明确为 DDS payload probe，不能送到严格图像接收逻辑伪装有效帧。

频率按单帧/1Hz → 10Hz → 60Hz，分别测试：仅我方Demo、仅客户Reader（现场可用才做）、二者同时、故意慢Reader/停止消费、Reader离开后正常Reader重新加入。同一renderer PID内完成故障/恢复验证。

分别提供 Domain150实验配置、Domain1现场配置；不向生产网络广泛扫描或故意制造风暴。未能连接客户程序时，完成本地复现/负向测试并提供现场采集命令，不伪造客户根因。

若需调大某项资源，依据实际每帧字节数、缓存预算和SDK上限设置，读回验证。不要仅把100ms改成几十秒，也不要raw应用分包绕开原结构。

## 3. P19B：客户分支故障隔离及同进程恢复

最小闭环必须让用户不必杀掉整个 HwaSim_IR：故障Reader退出后，同一PID能通过明确的恢复操作或既有 STOP → RESET/INIT → ACK → START 恢复客户发送。

先验证原有 beginRound 能否重建 writer；若已经有效，就保留其机制并补齐UI状态与流程，不为“恢复”过度重构。若残留DDS实体/locator/历史导致不能恢复，定位并清理客户分支实体，不能盲目重新全局 DDSIF::Init/Finalize。

建议状态清楚分为 Disabled、Ready、Running、Failed、Recovering。最少完成：

- 出错后立即记下真实原因、失败帧、已写片数、abort/reject数量；原链与控制继续，STOP有界返回。
- 明确区分当前回合“已失败”与可以执行下一回合。新的开始不能因上次 error 字符串或cancel残留永久拒绝。
- 没有 in-flight write/callback 时，安全释放并重建客户 Writer/Publisher/必要的客户Participant实体；操作前后保证不会重复创建Topic writer或使用悬空指针。
- `RELIABLE` SDK write 超时可能只影响局部发送，也可能已经被部分Reader收到。重试策略不得假定失败Sample一定无人收到，不盲目整帧重复发送；raw用frameId统计重复/缺口；H264半个AU失败后不要重播尾半片，恢复时从新SPS/PPS+IDR建立解码边界，并明确中断期间不保证连续帧。
- 自动重连如实现，必须限次/退避、资源有界、故障和恢复可见；不能无限创建线程/participant或在UI/渲染线程等待。
- 用户可在同PID重新开一轮正常raw发送；用接收端计数/像素校验证明，不以writer返回OK代替接收成功。

实际SDK写超时、transport故障与人工返回码注入是三种证据，分别记录。P18只测过应用队列溢出，本轮不能复用它冒充真实SDK超时隔离。SDK调用若不遵守声明的等待时间，应记录真实阻塞，并使用厂商支持的取消/释放方式；不能用detach悬挂线程或强制销毁正在调用的对象伪造有界恢复。

## 4. P19C：消除约12秒末帧等待

### 4.1 先测清楚等待在哪里

分别记录：最后有效输入、最后编码AU完成、最后客户分片write、最后分片到达、parser产出最后AU、decoder产出最后图像、录像封装完成。需要跨机测量时声明时钟边界；不要把离线MP4保存耗时或idle退出时间算成每帧网络时延。

复现1帧、少量帧和120帧后正常STOP，核实最后一帧是否已收到完整字节但停在parser。检验编码器自身是否还缓存帧，与无后继起始码导致的parser等待分开处理。

### 4.2 首选方案：标准码流收尾，保持四个客户结构不变

可在发送端真实结束回合、所有已接受帧编码并发送完成后，在客户H.264连续码流中发送标准H.264 `end_of_sequence / end_of_stream` NAL（具体选择、语法与重启语义依据实际标准及解码器验证）。仍通过原 HD264Stream/LD264Stream 和1..1400有效DataLength发送，不新增Topic、字段、私有header或HwaSim私有SEI。这是编解码层标准结束标记，不是新的客户业务协议。

不要假设“加一个标准NAL”当前 FFmpeg parser 就必然自动输出尾帧。检查部署的 parser 实现，必要时在接收增量扫描器可靠识别完整标准结束标记后：先排出parser内的最后AU，再向decoder进入draining，收尽所有帧；最后复位本地解析/解码状态，以便同一个Reader继续接收下一回合的新SPS/PPS+IDR。不能把结束标记交给普通picture计数，不能增加一张虚构末帧。

只在真实正常结束时发送终止NAL；不能每帧发终止NAL使参考链失效。AUD可以辅助AU边界，但“每帧开头加AUD”本身不自动解决没有下一帧时的最后AU问题，必须实测尾部行为。

如果实际目标硬解器不支持该标准结束方式，则给出保持IDL不变的兼容路径并明确限制，例如接收端收到外部真实的结束通知后显式调用 finishStream。不能引入旧HwaSim私有状态订阅作为客户Demo的隐性依赖。没有结束信号、下一帧或可确定边界时，仅从沉默时长无法严格区分断网/长暂停/真正结束，必须写清这个边界。

### 4.3 严禁的“修复”

- 不能只把 idle 从12000改到20/30/80ms就宣布正确。
- 不能把 DataLength<1400 当一帧结束；整除1400的AU、跨NAL/跨Sample都必须支持。
- 不发 DataLength=0 哨兵，不借DataLength高位编码标志，不添加自定义帧号/帧长。
- 不补发重复最后一帧、空白帧或伪造一帧推进parser。
- 不中途用 avcodec_send_packet(NULL) 周期清空仍在播放的解码器。
- 不恢复P18曾经的“2秒暂停即重置”逻辑。

### 4.4 验收

正常STOP后无需12秒idle才得到最后图像；实测并分别列出终止码到解码尾帧时间及MP4收尾时间。连续Demo默认不因普通停顿自动退出，idle退出只保留为显式离线测试/兼容选项。

覆盖1/30/120帧、最后AU长度为1400整数倍与非整数倍、结束NAL跨Sample、NAL中途3秒及更长暂停、晚加入、同Reader两回合、故障断流后恢复；完整帧/字节计数守恒。对于异常中断的未完成尾NAL，报告truncated/incomplete，不用强制flush生成破图当成功。

## 5. P19D：DataDrivenTestQT 双传感器 UI

### 5.1 UI和数据模型

不用第二个控制程序替代。改现有默认UI，普通零特殊参数启动就能看到两路设置，可使用两张表单/两个标签页，但重要状态与发送勾选必须常显。窗口100%/125%/150%缩放可操作，不能靠手工改INI才能改第二路。

建立两份独立配置与运行状态，例如 SensorChannelConfig / SensorChannelState，逐路包含：

- 启用/发送勾选；角色 coarse/precise；可编辑 platID、sensorID。
- 每路完整 INIT 数据：现有全部传感器字段、平台初始化状态、分辨率、波段、像元角、H264开关、帧率/仿真模式、噪声/标注/录像等已有字段；已有环境初始化字段也不能隐式被另一路覆盖。高级字段可折叠或复用表单，不删除原能力。
- 默认粗路1001/1、320×320、raw；精路1001/2、800×800、H264。
- 默认目标/模型选择跟随公共设置；允许关闭跟随后逐路选择已有不同模型类型。不得为了换模型改原1.txt，也不添加新的目标运动/跟踪算法。
- 公共数据源默认现有1.txt，可浏览选择其他符合格式的数据文件；每路可跟随公共数据源，或显式选择自己的文件。只支持已实现/本轮实际实现的源，不能列出不可用来源充当完成。
- 逐路 INIT/ACK/运行/停止/错误状态、发送计数、当前数据源/行位置、有效身份；域/配置文件/实际Topic可查看。
- 保存/读取两路配置，兼容旧单路配置迁移；提供“复制本路到另一路”以减少重复录入，但复制动作不能不提示地制造相同路由身份或客户Topic冲突。

不要只在原函数中继续硬编码两个传感器数组下标和m_dualAckMask。共享表单构造与校验即可，不引入复杂插件系统。

### 5.2 身份和角色必须解耦

platID/sensorID 是路由身份；coarse/precise 是本地角色和客户固定Topic选择，分辨率是INIT参数。coarse=1、precise=2仅默认，不再是强制语义。

搜索并修复P18的角色/ID强制校验、ackMask、固定Topic选择和固定sensor255广播。不得改旧Control/Init/Realtime IDL来增加channel。

客户PixelFormat、Domain、role等若不是旧INIT结构中的字段，就属于本地/部署配置：UI可编辑、保存和导出，但不得声称已经通过不存在的INIT字段下发。

改UI里的ID或role不会魔法修改已经启动的renderer本地配置。提供每路 NetworkConfig/RuntimeConfig 导出或匹配的启动命令，明确 renderer 必须按这些身份/role启动，现有实例不匹配时提示并等待真实ACK，而不是写死返回成功。普通默认UI仍可对现有1001/1和1001/2两实例测试。

同一Domain/Partition中的同一固定客户Topic只能有一个可识别生产者；两个启用路的身份重复、或都选择相同role+codec导致客户Topic冲突时，在INIT前明确提示。不能靠不同platID就宣称固定TX Topic会自动分流。

### 5.3 INIT、控制与发送选择语义

- 每个选中路独立快照和发送INIT，等待本路真实platID/sensorID ACK；不要阻塞GUI事件循环。
- ACK按身份和本地初始化状态匹配，超时逐路显示；任一路失败不能被另一路ACK掩盖。旧协议没有事务号的限制如实说明，不发明线上事务字段。
- 两路均选中时，两路准备好后共同START；只选一路时不向另一路发INIT/Realtime。
- Control仅platID无sensorID：同一平台的START/STOP/RESET会影响该平台其它已初始化实例。对选中平台去重后各发一次，UI明确“发送选择针对INIT/Realtime；平台级控制会联动同平台实例”。需要真正独立控制时使用不同platID；不假造单sensor STOP。
- 新手工模式默认 exact发送，避免sensor255广播把未选中路或其它实例也驱动。两路同源时同一60Hz tick读一次公共输入，复制为两份消息，仅按本路配置转换路由和明确的模型选择。
- 共用源的一行/时间/基础平台目标运动状态必须一致，不能两个定时器各自读一遍导致错一行或总共推进两次。
- 独立源使用明确本地时钟/游标规则，源结束或失败逐路显示；不静默重复末行维持假60FPS。共享平台的EOF/STOP联动限制必须说明。
- INIT后冻结对应有效配置；RESET后解锁。是否选中发送可在非运行状态修改；运行时改变选择涉及平台控制，要有明确信息，不能无提示破坏另一通道。
- 无VideoDisplay/原VideoStatus关闭时照样可用，不把旧视频状态等待当客户ready或运行必须条件。

### 5.4 取消输入文件哈希准入

用户明确可以换数据源且“不对比哈希值”：普通数据源加载、INIT、START和新UI测试不得要求1.txt固定SHA、固定文件名、固定4318行、固定路径或与旧记录哈希相等；也不要要求新测试必须做输入文件哈希对比才能通过。

使用格式、列数、数值有限性、实际有效行数、业务身份、时间字段和现有必要物理支持域校验。源文件不由程序自动改写。不能把取消输入哈希限制扩展成删除所有LUT/物理资源安全校验；如另一合法轨迹超出支持域，报告实际域问题，不报“不是原始1.txt”。软件发布/回滚文件的完整性校验可以保留，它不是数据源准入限制。

## 6. P19E：接收 Demo 可交付、易阅读

梳理现有main.cpp，分开配置、DDS订阅、raw处理、H264增量处理、显示/保存和诊断；避免一行很多语句及模板层层隐藏实际API。适度拆文件即可，不把简化做成删错误处理或协议校验。

保留原Demo可运行参数兼容；生产默认关故障注入、逐包审计和高成本诊断，需要时再开。增加中文注释，说明对象/内存/线程所有权和关闭次序。

另提供同一交付包中的 `CustomerRawReceiverMinimal` 小目标或清楚的独立示例，默认 Domain1、TX/LDecode、320×320、Gray8。只依赖ZRDDS和原generated类型，先做到计数和取到像素；不要求客户先集成Qt、FFmpeg、HwaSimIR、CommonData、FrameProductV2、旧Status/Meta。给出最短编译和运行方法。

最小例子应直观展示实际SDK调用：Init一次 → CreateDP → 类型支持/注册及SubTopic（若封装内部完成注册就说明）→ 接收LDDecoder → 校验frameId/stream.length → 回调内复制 → 工作线程消费 → 正确释放。

明确：

- 320×320 Gray8实际length=102400，800×800 Gray8=640000，320×320 RGB24=307200。不能把sequence最大容量当每帧实际长度。
- sequence不是C字符串，0x00/0x80/0xFF都是普通数据，不能strlen；IDL long不是Linux原生64位long；只能用generated序列化。
- 使用SimpleDataReaderListener时先检查SDK内部take/return_loan和SampleInfo.valid_data语义，不在回调里重复take同一批样本；如果展示底层DataReader模式则完整配对take/return_loan。
- 不持有回调返回后无效的数据指针，不在DDS回调中解码/显示/写大文件。
- QImage/OpenCV包装外部内存时讲清深拷贝或共享缓冲生命周期；RGB24不是旧BGR24。
- raw不需要H264解码器；H264每个DDS Sample不是完整帧，连续交给parser再decode。

增加“只接收计数”模式及一页客户排查步骤：先counter正常，再显示，再存盘。客户硬件SDK不可用时只提供明确的完整AU/参数集输入边界，不造一个虚假的硬解适配器。

## 7. P19F：Domain1现场配置

不全仓库替换150为1。保留实验室默认150，交付明确的`lab_domain150`与`customer_domain1`配置/启动样例。UI显示使用哪个配置、哪个实际Domain和QoS路径。

当前客户Writer借用进程级同一participant，客户域实际受共享runtime影响。先完成最小可靠方式：控制端、两renderer、所需旧VD、客户Demo的相关参与者统一Domain1；同时核实DdsProtocol/DdsVideo与环境覆盖等有效值，不允许只改XML中的无关值或只改一处INI造成“看似1实际150”。

本轮不强制同时跨域桥接。若为了保持内部150、客户1而实现独立客户域，应显式增加CustomerDdsVideo.DomainId（默认继承），通过同一个已初始化Factory创建/借用相应DomainParticipant；同域复用、异域分participant，生命周期有单一管理者，不重复全局Init/Finalize。该扩展必须有真实1/150并行测试，不能只加INI键却仍向原participant发。做不到就说明目前需要统一域，不能假装实现。

匹配条件还包括Topic、注册type、partition和兼容QoS；同Domain不等于必然互通。Domain1已有客户流时避免角色Topic生产者冲突。

## 8. P19G：H.264参数说明与必要配置化

### 8.1 独立解析参考，不能盲目照搬

本包附带分析结果，只作可复核起点：

- DDS.264：800×800，Constrained Baseline，level3.1，YUV420P 8bit，1参考帧，CAVLC，8767解码帧（I36/P8731/B0）。
- UDP.264：640×512，High，level3.2，YUV420P 8bit，4参考帧，CABAC，9341解码帧（I59/P2341/B6941）。
- 两个文件不是同一编码配置，也没有附件说明它们分别代表硬解成功/失败；不能按文件名推断。
- 附带txt设置gop=250、max_b_frames=1，又设置bframes=3、high/cabac/refs/rc_lookahead，随后覆盖preset/crf并启用zerolatency；未检查av_opt_set返回值。它不是可直接采纳的硬件能力清单。

使用实际ffprobe/trace_headers复核profile、level、bit depth、chroma、SPS/PPS、参考帧、B帧/重排、VUI/帧率、颜色范围、NAL/AU结构。原始.264不含实际网络采集时间戳，不用ffprobe估计帧率冒充发送FPS。不要以NAL大于1400断言错误。

### 8.2 当前源码与INI，必须向用户解释

已有 `[TcpOutput]`：H264Encoder=auto，H264BitrateKbps=4000，H264GopFrames=30，H264LowLatency=true，H264FallbackToJpeg=false，H264ForceKeyFrameOnStart=true。

当前MPP请求NV12、CBR、码率上下限目标的15/16与17/16、profile66、level40、cabac0、trans8x8=0、每IDR带SPS/PPS；部分参数在源码固定，不是用户现在往INI随便添加同名键就会生效。

当前FFmpeg请求YUV420P/baseline/B0/ultrafast、可选zerolatency、Annex-B和重复headers；level未像MPP固定为40。分别记录“请求配置”“API设置/读回结果”“实际SPS/PPS/帧类型”，以码流为最终验证，不能把源码意图当实测。

### 8.3 配置化要求

保留旧默认行为，集中在HwaSimIRRuntime.ini已有编码配置或清楚的独立段中提供：profile（baseline/main/high，后端支持才接受）、level（auto/显式）、GOP、目标码率/必要的码控模式、低延迟、参考帧（实际支持才开）、重复SPS/PPS、可选AUD及本轮标准收尾开关。

本阶段实时默认B帧维持0，不为模仿附件引入B帧/长lookahead。如果配置接口不支持某项，启动/INIT明确报错或标记不支持，不能默默忽略。检查每个FFmpeg/MPP选项返回值；不抄新MPP文档中板端旧库不存在的键。

同样的有效配置必须传入原编码器和客户独立Gray8编码器；RGB共享clean AU只在有效配置一致时共享。不要出现“INI改了，实际客户流还是固定旧profile/level”的问题；必要的客户专用override应有明确继承和日志。

做一个`customer_hw_compat`示例：保守Baseline/CAVLC、8-bit 4:2:0、无B帧、短GOP30、每IDR参数集、Annex-B；level使用经过完整校验的auto或显式值。它是候选兼容配置，不是保证任意未知硬件可解码。

level必须检查实际宏块尺寸、帧率、MaxFS/MaxMBPS/DPB/码率等：800×800@60=150000宏块/秒，不能level3.1（108000）；其它约束满足时可候选3.2（216000）。1024×1024@60=245760宏块/秒，不能继续用3.2，需要核验至少4.0和其它限制。不得仅改SPS字节伪装低level，也不得为匹配未知硬件偷偷降尺寸/帧率。

输出一页客户确认项：硬解器芯片/SDK、支持profile/level/分辨率/帧率/位深/4:2:0、最大参考/重排、Annex-B还是length-prefixed/avcC、喂流需要完整AU还是连续字节、输入缓冲和对齐、是否需要独立参数集、错误码。没有这些信息就保留`NOT_VERIFIED_CUSTOMER_HARDWARE`，但不要因此停止软件侧可做的配置/码流检查。

交付真实编码的短clean .h264、解析报告、对应INI、最简解码命令；用户已通过的软件解码回归必须保留。禁止把MP4容器字节当客户Annex-B载荷，禁止夹带HwaSim私有SEI。

## 9. P19H：板端版本整理与清理

用户明确P17在 `/userdata/HwaSimIR/Config_new` 与 `/userdata/HwaSimIR/HwaSim_IR_new`，先核实可执行/配置配对、部署身份和用途。不要因名字new就当P19，也不能宣称P17缺失。P18报告中的candidate4目录只是线索，不能替代当前板端实际状态。

用户已授权整理旧版本，但必须可恢复地执行：

1. 只读盘点路径、大小、可执行/配置配对、软链接真实路径、当前进程打开文件、启动脚本/自启引用、关键共享资源；生成before清单与拟保留/拟清理清单。
2. 在本机/交付机板外归档用户的P17及其它确需保留的历史证据，验证归档可列出并可恢复。不得把唯一的Config_new/HwaSim_IR_new直接覆盖。板外归档失败时，只暂停破坏性清理，其余开发继续。
3. P19构建部署并烟测通过后，板端仅保留两个完整可运行的版本：最新P19与上一版P18。上一版必须包括可运行ELF、匹配配置/资源及两路启动信息，而不只是一个裸EXE。P17转为板外归档，不在板端再保留第三套完整历史部署。
4. 目录命名可用releases/<version>及current/previous，或保留现有布局的最小变更。兼容 `/userdata/HwaSimIR/HwaSim_IR` 和Config等既有入口、工作目录、两路隔离配置、licence路径及Xorg/Mali条件；不要要求用户每次猜candidate目录。
5. 用本轮实际盘点得到的明确路径白名单清理陈旧Config.before_*、旧candidate/rollback及临时产物；不要使用宽泛rm -rf Config*或不核实symlink的递归删除，不清空整个logs，不删除DDS库、licence、模型、LUT、用户输入/视频或未知客户文件。
6. 保留必要证据；大历史日志/媒体可验证迁出板外，文档更新路径，不制造失效链接。
7. 测试切换上一版再恢复最新，输出after清单、释放空间、两版定位、启动和回滚命令。此前某版本名与实际配对不清楚的文件先隔离/归档，不凭日期猜测后直接删除。

本阶段结束不应留下又一批Config.before_p19_*多份完整备份。测试产生的临时版本在最终保留策略中收口。

## 10. 测试清单与交付判定

以功能故障修复和易用性为门禁，不复刻P17/P18全部视觉/物理测试。使用现有真实板端Mali/MPP和Windows Qt环境，不能用模拟write成功代替实际收发。

最低必须覆盖：

| 类别 | 必做验证 |
|---|---|
| 图像冻结 | 尾焰/天气/深度/HDR生产文件未被本轮改动；只做必要普通场景烟测 |
| raw故障 | Domain1大Sample测试、只我方/慢reader等可用组合、真实返回码；客户不可用则保留现场复测清单 |
| 同PID恢复 | 故障后正常Reader加入，同renderer PID经明确操作恢复；旧链不中断或不被客户全局Finalize破坏，STOP有界 |
| H264收尾 | 少量帧、整除/非整除分片、暂停、真实STOP、两回合；尾帧不靠12秒idle、不增加假帧 |
| UI一路/两路 | 分别只粗、只精、两路；真实INIT/ACK；实际尺寸320/800、raw/H264正确 |
| 任意ID/角色 | 同平台不同sensor，以及不同平台；至少使用非1/2的合法sensor ID，角色/Topic仍正确；控制联动语义明确 |
| 数据源 | 默认同一1.txt；另一个合法文件不同名字/内容/行数能加载运行，不比较输入SHA；空/坏数据清楚拒绝 |
| 不同模型 | 两路可独立选已有模型，真实运行与UI一致，不自动改输入文件 |
| Demo | 最小raw计数示例和完整显示接收都能独立编译运行；仅四个客户类型/Topic |
| 编码兼容 | 现默认及候选配置真实SPS/PPS/帧结构校验、软件解码；硬解仅在实际设备存在时判通过 |
| 生命周期 | 无VideoDisplay、旧新并行、停止重启/RESET、关闭UI、失败清理无死锁 |
| 性能记录 | 同板粗320raw+精800H264短稳态/约60秒测量，逐路计数和时延边界；小偏差不升级为本轮主要任务 |
| 版本收口 | 最新+上一版可运行、用户P17板外归档、回滚并恢复、清理清单可追踪 |

发送两路时默认一个调度器、一份共享源；DDS消息总次数可为两路各60Hz，不把120次write/秒当每路120FPS。按流分别统计Samples、frames、bytes、abort、decode、GUI更新和保存；H264分片数不能与帧数直接等同。

各项区分PASS、FAIL、BLOCKED_ENVIRONMENT、NOT_VERIFIED、USER_REPORTED。客户根因未能复现时，定位工具/恢复修复可通过，但现场根因与客户raw最终互通保持开放；不要写“必定客户问题”。硬件软解不是客户硬解。原始历史失败保留摘要，不重写成成功。

## 11. 最终交付

创建/更新以下文档（名称可少量适配，但内容必须齐全）：

- `docs/HwaSimIR_P19_Closeout.md`：逐项实现/测试/未验证与已知限制，不再因59.99单项自动否定全部新功能。
- `docs/HwaSimIR_P19_CustomerRawDDS_Troubleshooting.md`：日志因果链、码值来源、QoS有效值、size/rate矩阵、同PID恢复、客户现场操作。
- `docs/HwaSimIR_P19_H264_Compatibility.md`：参考附件实测、当前/新增INI参数、MPP/FFmpeg映射、实际码流和尾帧收尾、客户硬解确认清单。
- `docs/HwaSimIR_P19_DualSensor_UI_Guide.md`：普通界面截图、两路独立INIT、ID/role、数据源、只发一路/两路、平台级Control限制、配置导出启动。
- `docs/HwaSimIR_P19_Board_Releases.md`：P17归档位置、当前/上一版、两路启动、回滚和清理结果。
- 同步更新原EndToEnd Guide、Customer ICD、DualChannel Runbook及源码索引。删除/更正“2秒停顿重置”和“v4保留状态”的冲突描述；更正“role强制sensor1/2”和新任意ID实现的冲突。

交付源码、实际Windows构建产物、板端Release部署回执、可给客户的精简接收源码/IDL/generated/QoS/README、真实clean .h264短样本、配置样例和机器可读测试结果。不要打包无关旧VLC/未知EXE或泄露许可证内容，不混发不适配的VS/MinGW SDK二进制。

证据集中于 `logs/p19/`，提供可打开的相对路径索引。引用开发机绝对路径时同时给出离线包对应项，不再只有D:/...链接。

最终回复必须直接回答用户：

1. 12秒是什么、改动在哪、修后正常STOP尾帧实测多久？
2. raw最初失败已确认/未确认的原因是什么，怎样在同renderer进程恢复？
3. 客户最少看哪些接收代码、怎么编译运行、Domain1怎样配？
4. 双传感器UI怎样设置两路、任意ID/role、共用或更换数据源？
5. H264当前实际参数、哪些INI键可改、哪组配置给硬解测试、哪些仍需客户设备确认？
6. 板端最新/上一版/P17归档在哪里，怎样启动和回滚？
7. 哪些由本轮亲测，哪些仅用户确认，哪些未验证？

不自动commit/push，不使用git clean/reset/restore清掉用户工作。具体环境不可用时只将相关验证标BLOCKED，继续完成可做的代码/文档/构建；不得把缺少客户硬件当成全部停止理由，也不得伪造硬件结果。
