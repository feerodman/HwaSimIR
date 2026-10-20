# HwaSimIR：P19 最新分域实现速查（P20 使用）

本文件是开始P20前的P19检索快照。P20已接板的实际配置和运行证据请以 [P20三域指南](HwaSimIR_P20_DDS_Domain_Guide.md) 及所选构建对应的部署回执为准；早期候选不能代替最终构建验收。

## 核对范围

本说明依据 `main@18a6f427e0aef731804d0951802979b044998106` 中的最新P19恢复测试文档、Runtime.ini及DdsRuntimeManager源码。部署域值来自P19报告，不代表本次重新连接板卡读取。使用时仍以实际INI路径及 `[DdsDomains]` 日志为准。

## 三条链路

| Runtime.ini段/键 | 实际用途 | 仓库默认 | P19最新报告记录的板端两路配置 |
|---|---|---:|---:|
| `[DdsProtocol] DomainId` | 接收Control/Init/Realtime；返回InitAck；控制端观察VideoStatus | 150 | 150 |
| `[DdsVideo] DomainId` | 原HwaSim_IR_VideoDisplay视频/SEI及Meta、Annotation、VideoStatus、诊断/时钟信息 | 150 | 1 |
| `[CustomerDdsVideo] DomainId` | TX/HD264、TX/LD264、TX/HDecode、TX/LDecode | -1，继承DdsVideo | 1 |

仓库客户输出的 `Enable=false`，所以仓库默认“-1→150”只是启用后采用的域，不表示默认正在发送客户视频。

最新P19支持独立配置：同域复用Participant，异域在同一个进程级Factory下创建Participant。全局Init/Finalize仍各一次。早期08版曾把原视频域静默覆盖成控制域；不能把那个历史限制当成当前实现。

项目当前允许0..232；只有客户域允许-1继承。-1不是DDS通用“默认域”常量，不可填到DdsProtocol或DdsVideo。

## 示例A：按P19最新部署记录，控制150、两种视频1

```ini
[DdsProtocol]
Enable=true
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[DdsVideo]
Enable=true
DomainId=1
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[CustomerDdsVideo]
Enable=true
DomainId=1
```

这是相关键的片段，合并到现有完整配置，不要用片段覆盖整份文件。Mode、PixelFormat、身份、分辨率等继续沿用各路有效配置。

对端：DataDrivenTestQT控制域150；VideoDisplay原视频域1；客户Demo域1。粗精两路的Domain可以相同，仍由各自身份/Topic区分。

## 示例B：内部控制与VideoDisplay留在150，只给客户域1

```ini
[DdsProtocol]
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[DdsVideo]
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[CustomerDdsVideo]
DomainId=1
```

对端：DataDrivenTestQT150；VideoDisplay150；客户Demo1。这里客户不能用-1，否则将继承150。

其它常用组合：全150为150/150/150（客户也可-1）；全1为1/1/1（客户也可-1）。三域隔离测试曾使用150/1/2。

## 对端配置与域修改步骤

1. 确认实际程序、当前release和 `HWASIMIR_RUNTIME_CONFIG` 指向的INI；两路分别核对，不能只改源码目录模板。
2. 检查环境变量覆盖：`HwaSimIRDdsProtocolDomainId`、`HwaSimIRDdsVideoDomainId`、`HwaSimIRCustomerDomainId`。它们优先于INI。
3. 退出相关renderer后修改域，并同步对端：DataDrivenTestQT的SenderNetworkConfig/三域导出；VideoDisplay的NetworkConfig或 `--dds-domain`；客户Demo `--domain`。
4. 正常重启相应进程。STOP/RESET不改变已经创建的Participant。
5. 从 `[DdsDomains]`、`[DdsRuntime]`、接收ready与实收计数核对有效域及来源，不以INI文本存在或write返回OK冒充对端收齐。

## 域独立与QoS独立不是一回事

目前同一Factory加载一套QoS XML。控制启用时，以DdsProtocol.QosFile作为共同文件；不同Writer/Reader使用各自profile。不要以为给DdsVideo填写另一份XML，就能在同进程独立加载第二套全局DDS配置。

VideoStatus在原视频域发布，分域时镜像一份到控制域供UI观察；同域只发布一份。Meta/Annotation和诊断跟随原视频域。客户四Topic不因此新增Status或业务字段。

固定客户Topic不含platID/sensorID，同Domain/Partition/Topic不要同时放入多个相同角色生产者。Domain不同也不会隔离CPU/GPU算力。

## 源码/报告入口

- `docs/HwaSimIR_P19_DDS_Domains.md`
- `docs/HwaSimIR_P19_Resumed_Test_Report.md`
- `DDS/Runtime/DdsRuntimeManager.{h,cpp}`，尤其 `participantForDomain()`。
- `DDS/Protocol/HwaSimIRProtocolEndpoint.{h,cpp}`。
- `DDS/Runtime/BoardTelemetryService.h`。
- `HwaSim_IR/HwaSim_IR/Video/DdsVideoPublisher.cpp`。
- `HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.{h,cpp}`。
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`。
- `DataDrivenTestQT/DualSensorPanel.*`。
