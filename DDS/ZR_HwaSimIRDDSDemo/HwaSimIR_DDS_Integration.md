# HwaSimIR DDS 集成说明

## 1. 修改原因

原 `ServToProxy_publiser/subscriber` 使用原生 DDS C++ API，参考工程的
`HwaSimIRSimpleDdsClient` 使用 DDSIF、原生 API、SimpleDataReaderListener 和阻塞 Ack 等待混合封装。
本工程将 HwaSimIR 业务重新组织为 `hwasimir_publiser` 和 `hwasimir_subscriber`，显式创建
Participant、注册类型、创建 Topic、Publisher/Subscriber、DataWriter/DataReader 并转换为类型化接口。

`HwaSimIRSimpleDdsClient` 不作为本工程接口，不加入编译；参考工程没有修改。
唯一保留的简易接口是公共 `DdsRuntime` 中的 `DDSIF::Init` 和最后一次 `DDSIF::Finalize`。
业务层不调用 `DDSIF::CreateDP/SubTopic`，不使用 SimpleDataReaderListener、mutex、condition_variable
或 `waitForInitAck()`。

## 2. 最终架构与所有权

```text
DdsRuntime：一个公共 Factory，预先加载 QoS XML
├── ZR Participant，Domain 6，默认 Participant QoS
│   └── ServToProxy / TOPIC_ABREQ_CMD
└── HwaSimIR Participant，Domain 150，hwasimir_tcp
    ├── HwaSimIR.Control  → ControlCommandV1 Writer
    ├── HwaSimIR.Init     → InitCommandV1 Writer
    ├── HwaSimIR.Realtime → RealtimeDataV1 Writer
    └── HwaSimIR.InitAck  → InitAckV1 Reader → 业务 callback
```

两套模块只共享 Factory，不共享业务 Participant、Domain、Topic 或 QoS。
HwaSimIR 发布对象拥有自己的 Participant；其订阅对象借用**同一个 HwaSimIR Participant**，
保持参考客户端的三个 Writer 和一个 Reader 属于同一 Participant。
因此订阅构造函数接收 `factory, participant, callback, topicName`，不是另外创建一个 Participant。
`demo2` 先声明 writer、再声明 reader，确保 reader 先析构。

原 ZR 仍使用 Domain 6、`TOPIC_ABREQ_CMD`、`ServToProxy`、默认 Participant/Publisher/Subscriber QoS，
Writer/Reader 仍仅设置原有 `history.depth = 5`。原 `demo` 的 `hello` 发送和 5 秒发现等待保留，
其订阅示例仍保持原来的注释状态。原协议字段和 HwaSimIR generated 字段均未修改。

## 3. 为什么仍然需要 QoS XML

代码风格一致不代表传输配置相同。`Config/ZRDDS_PROTOCOL_QOS.xml` 从参考工程逐字节复制：

| 配置名 | 用途 |
|---|---|
| `hwasimir_factory` | 全进程一次性 Factory 初始化和 XML 加载 |
| `hwasimir_tcp` | HwaSimIR Participant 使用 TCP user traffic |
| `hwasimir_protocol_writer` | HwaSimIR Writer 的 RELIABLE、KEEP_ALL 和资源限制 |
| `hwasimir_protocol_reader` | InitAck Reader 的 RELIABLE、KEEP_ALL 和资源限制 |

HwaSimIR Writer 从 profile 读取 QoS 后，保留参考客户端的显式设置：
RELIABLE、KEEP_ALL、`max_samples=4096`、`max_samples_per_instance=4096`、
`max_instances=64`、`max_blocking_time=60s`。InitAck Reader 使用原 profile，
不套用原 ZR 的 `history.depth=5`。

工程不自动复制或部署 XML。运行前，请自行将工程内 `Config/ZRDDS_PROTOCOL_QOS.xml`
放到可执行文件旁的 `Config` 目录；`main.cpp` 从该位置加载。也可以自行修改加载路径。
`.pro` 只列出源码、头文件和 `.pri`，不包含复制命令、输出目录计算或构建后步骤。

已核对工程内 ZRDDS 头文件及 `ZRDDS/doc/cppdoc/html/class_d_d_s_1_1_domain_participant_factory.html`。
当前 API 的真实签名为：

```cpp
create_participant_with_qos_profile(domainId, libraryName, profileName,
                                    qosName, listener, mask);
get_participant_qos_from_profile(participantQos, libraryName, profileName, qosName);
get_datawriter_qos_from_profile(writerQos, libraryName, profileName, qosName);
get_datareader_qos_from_profile(readerQos, libraryName, profileName, qosName);
```

HwaSimIR 使用 `default_lib / default_profile / hwasimir_tcp` 创建 Participant。
原 ZR 则继续 `get_default_participant_qos` → `create_participant`，不修改工厂默认 Participant QoS。

## 4. 初始化顺序

```text
DdsRuntime::init：只准备公共 Factory，不创建业务 Participant
↓
demo 构造：创建原 ZR Participant（第一个业务 Participant）
↓
demo2::initDds：创建 HwaSimIR Participant（第二个业务 Participant）和全部端点
↓
QCoreApplication::exec()
```

`HWA_SIMIR_DOMAIN_ID` 在 `demo2.h` 中定义为 150，与当前 `HwaSimIRRuntime.ini` 和
`DdsStimClient` 默认配置一致；**按实际对端 DomainID 修改**。不新增命令行参数或配置系统。

## 5. 释放顺序

```text
exec() 返回
↓
InitAck Reader 解除 listener，删除自身 Subscriber/Topic，再删除 listener
↓
HwaSimIR 发布对象删除其 Participant 内实体，再 delete_participant
↓
原 demo / ServToProxy 删除自己的 Participant
↓
DdsRuntime::shutdown：唯一一次 DDSIF::Finalize
```

业务类禁止 Finalize 公共 Factory。`main.cpp` 用普通作用域保证上述顺序。
`DdsRuntime` 只有一个 static 工厂指针和 `init/factory/shutdown`，没有引用计数、线程或状态机。

## 6. 客户分散调用方法

### DDS 初始化

整个进程先执行 `DdsRuntime::init(qosFile)`，再初始化原 ZR，最后：

```cpp
demo2 dds;
dds.initDds();
```

`initDds()` 中的实际创建形式为：

```cpp
hwaSimIR_writer.reset(new HwaSimIR_publiser(
    DdsRuntime::factory(), HWA_SIMIR_DOMAIN_ID));
HwaSimIR_RX::ProcessDataCallBack callback = std::bind(&demo2::recvInitAck,
    this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
hwaSimIR_reader.reset(new HwaSimIR_subscriber(DdsRuntime::factory(),
    hwaSimIR_writer->participant(), callback, "HwaSimIR.InitAck"));
```

`demo2` 的函数展示各个业务位置如何发送，不实现完整 RunOneRound，也不自动循环或发出 HwaSimIR 控制命令。

### Reset：需要复位的业务位置

调用 `dds.sendReset()`，函数中使用真实字段 `flag=0x41`、`JB=1`、`platID`、
`simCommand=1`、`roundCut/currentRound`，然后：

```cpp
hwaSimIR_writer->pubControl(command);
```

### Init：初始化参数准备完成的位置

在 `demo2::sendInit()` 填入真实的 `platParamInit`、`trackingInit` 等业务参数，再调用 `dds.sendInit()`。
现有函数中的平台 1、传感器 1、640×512、波段 2 等值仅用于展示填字段方式。
报文使用 `flag=0x36`、`platID`、`sensorID`，然后：

```cpp
hwaSimIR_writer->pubInit(command);
```

### InitAck：收到应答的业务位置

`MyListenerInitAck : DDS::DataReaderListener` 实现：
`on_data_available` → 类型化 Reader → `take` → 检查 `valid_data` → callback → `return_loan`。

`recvInitAck()` 展示原协议典型判断：

```cpp
ack->platID == expectedPlatID &&
ack->sensorID == expectedSensorID &&
ack->trackingReady == true
```

DDS 层不等待、不缓存 Ack，不自动发送 Start。客户业务层决定何时开始及如何处理超时和多回合时序；
同一 key 的旧 Ack 不能仅因字段匹配就用于下一回合。
callback 在 DDS 收包线程执行，样本指针只在回调内有效；需要保存或投递 GUI 线程时先复制样本。
当前 `InitAckV1` 只有标量字段，可按值复制。

### Start：业务确认应答后的仿真开始位置

调用 `dds.sendStart()`，使用原协议 `simCommand=2`，通过 `pubControl(command)` 发送。

### Realtime：每帧或每周期数据更新位置

```cpp
HwaSimIRDds::RealtimeDataV1 sample = {};
HwaSimIRDds::RealtimeDataV1Initialize(&sample);
sample.flag = 0x38;
sample.platID = 1;
sample.sensorID = 1;
// 在此填写本周期 time、platLoc、weaponState、targetNumValid、targetState。
dds.sendRealtime(sample); // 内部直接 hwaSimIR_writer->pubRealtime(sample)
HwaSimIRDds::RealtimeDataV1Finalize(&sample);
```

### Stop：停止业务位置

调用 `dds.sendStop()`，使用原协议 `simCommand=3`，通过 `pubControl(command)` 发送。
这些命令值与现有发送 Demo 和接收端 `simCommand` 处理一致，没有新增 IDL 枚举或字段。

### 样本所有权

HwaSimIR 的三个发布接口统一使用 `const &`，只借用样本并返回写入是否成功；发布类不 Finalize 调用方数据。
`sendReset/sendInit/sendStart/sendStop` 在函数内 Initialize/发送/Finalize；实时数据由调用方完成这一生命周期。
原 `ServToProxy::pubData` 保留按值传参和内部 Finalize。它目前只有标量及定长数组，没有拥有堆内存的指针；
不能将“按值传参后可自由 Finalize”推广到含字符串/序列指针的其他 generated 类型。

### 程序退出

业务对象须先离开作用域，然后调用：

```cpp
DdsRuntime::shutdown();
```

## 7. MinGW64 编译与实际运行验证

工具链：Qt 5.12.12、MinGW g++ 7.3.0、`x86_64-w64-mingw32`。
沿用原 `ZRDDS/zrdds.prf`，只链接本工程 `ZRDDS/lib/ZRDDSCppz.lib`（Release 静态库），
未引入其他安装目录、VS2015 库或 CMake。SDK 文档为 2.4.5；本地随包库启动横幅实际显示
`2.4.4-re6b418c`，编译时间为 2025-12-18，未替换该库。

在 PowerShell 执行以下命令可重现干净 Release 编译（PATH 仅在当前会话临时设置）：

```powershell
$savedPath = $env:PATH
try {
    $env:PATH = 'D:\Qt\Qt5.12.12\Tools\mingw730_64\bin;D:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin;C:\Windows\System32;C:\Windows'
    New-Item -ItemType Directory -Force D:\HwaSimIR\build-ZR_HwaSimIRDDSDemo-native-Release | Out-Null
    Push-Location D:\HwaSimIR\build-ZR_HwaSimIRDDSDemo-native-Release
    try {
        qmake D:\HwaSimIR\DDS\ZR_HwaSimIRDDSDemo\ZRDDSDemo.pro -spec win32-g++ 'CONFIG+=release' 'CONFIG-=debug'
        mingw32-make clean
        mingw32-make -j4
    } finally { Pop-Location }
} finally { $env:PATH = $savedPath }
```

上述 PATH 是最小构建环境示例。当前 `.pro` 已恢复为简单的文件列表和 `.pri` 引用，
所有自动复制步骤及配套条件配置均已删除，不再依赖 `cp` 或 `qinstall`。
修改工程后先在 Qt Creator 执行“运行 qmake”，再构建，清除旧 Makefile 中残留的复制命令。
正常程序继续使用 Qt 事件循环，不增加运行参数。直接运行时需具备 Qt/MinGW 运行库及有效 ZRDDS 许可证。

已在用户实际 Debug 目录 `DDS/build-ZRDDSDemo-Desktop_Qt_5_12_12_MinGW_64_bit-Debug`
保留 `C:\Scoop\shims\sh.exe` 的 PATH 环境完成 qmake、MinGW 强制重新编译链接，退出码为 0。
当前验证日志为 `qmake-simple.log`、`build-simple.log`。`demo::getinf()` 的两个预留参数用
`Q_UNUSED` 标记，消除用户报告的未使用参数警告。

实际检查使用同一 `.pro`、相同 SDK 及业务源码，仅在被 Git 忽略的
`build-ZR_HwaSimIRDDSDemo-native-check` 内替换一次性检查入口；没有加入测试框架或更改产品启动参数。
检查顺序与生产 main 一致，创建端点后让 Qt 事件循环运行 1 秒再正常释放；不发送 HwaSimIR 控制命令。

已通过的日志关键结果：

```text
[DDS] Factory init: <同一个非空指针>
[ZR] Participant created, domain= 6 factory= <同一指针>
[HwaSimIR] Participant created, domain= 150 factory= <同一指针>
[HwaSimIR] writer types/topics/publisher ready; typed Writer count= 3
[HwaSimIR] InitAck type/topic/subscriber ready; typed Reader= <非空指针>
[CHECK] ZR + HwaSimIR participants and all typed endpoints ready
[HwaSimIR] InitAck subscriber cleanup= 0 0
[HwaSimIR] Participant cleanup= 0 0
[ZR] Participant deleted
[DDS] Factory finalize result= 0
[CHECK] exit= 0
```

检查还确认业务对象析构后 Domain 6 和 150 均无残留 Participant。
输出程序：`build-ZR_HwaSimIRDDSDemo-native-Release/release/ZRDDSDemo.exe`。
编译日志：同构建目录下的 `build.log`；运行日志：
`build-ZR_HwaSimIRDDSDemo-native-check/release/runtime.log`。
未进行两个真实外部对端之间的完整业务联调；本次已验证进程内初始化共存和有序释放。
