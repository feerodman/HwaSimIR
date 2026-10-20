# P20 DDS 三域配置

P20 沿用最新 P19 的独立分域实现，没有重建 DDS 架构。当前隔离候选两路实际INI为 control=150、oldVideo=1、customerVideo=1；测试逐项检查启动日志与接收计数。最终结果见 `logs/p20/acceptance.json`，历史候选的PASS不代替当前版本。原视频域与客户域相同不代表二者必须相同。

| INI段 | 用途 | 仓库默认 | 本次板端实际 |
|---|---|---:|---:|
| DdsProtocol | Control/INIT/Realtime输入，InitAck，控制域Status镜像 | 150 | 150 |
| DdsVideo | 原视频及SEI、Meta、Annotation、Status、诊断/时钟 | 150 | 1 |
| CustomerDdsVideo | TX/HD264、TX/LD264、TX/HDecode、TX/LDecode | -1继承DdsVideo | 1 |

仓库客户 Enable=false。只有客户域可使用-1；有效域范围0..232。相同域复用Participant，不同域仍共用一次DDSIF::Init/Finalize。三域不隔离GPU算力，客户固定Topic没有platID/sensorID字段，同域同Topic不能放两个同角色生产者。

## 修改和核验

1. 两路都退出。先核对启动日志 `runtimeConfigPath` 和环境 `HWASIMIR_RUNTIME_CONFIG`，修改实际加载的完整INI。
2. 检查 `HwaSimIRDdsProtocolDomainId`、`HwaSimIRDdsVideoDomainId`、`HwaSimIRCustomerDomainId`；它们优先于INI。更改后需要重启，STOP/RESET不迁移Participant。
3. DataDrivenTestQT对齐控制域；VideoDisplay的NetworkConfig或`--dds-domain`对齐原视频域；客户Demo `--domain`对齐客户域。
4. 查看 `[DdsDomains]` 的值/来源、`[DdsRuntime] participantCreated` 和实际接收计数。只看write返回OK不代表对端收到。

当前板端实际完整INI在 `/userdata/HwaSimIR/releases/P19/coarse/Config/HwaSimIRRuntime.ini` 和 `precise/Config/HwaSimIRRuntime.ini`；P20候选使用各自隔离目录内的副本。current仍是P19。本说明不要求改变用户当前150/1/1配置。

## QoS与Status

进程级Factory只加载一套XML。控制启用时使用DdsProtocol.QosFile；DdsVideo请求另一份文件会记录`sharedFactoryQos`、`videoQosRequest`、`action=use_shared_factory_profiles`，并使用共享文件中各自profile。独立域不等于可独立加载任意三份全局XML。

VideoStatus在原视频域发布；与控制分域时镜像到控制域，同域只发一次。Meta/Annotation及BoardTelemetry跟随原视频域。客户四Topic不新增Status/Meta。

可合并片段见 `logs/p20/configs/`：全150、全1、150/1/1、150/150/1，以及保留的150/1/2诊断组合。它们是示例，不声称全部已在本次运行；逐项实测以acceptance.json为准。

## 源码

- `DDS/Runtime/DdsRuntimeManager.cpp`：start、participantForDomain、shutdown。
- `DDS/Protocol/HwaSimIRProtocolEndpoint.cpp`：videoParticipant、controlStatusWriter、publishVideoStatus。
- `DDS/Runtime/BoardTelemetryService.h`：participantForDomain。
- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`：LoadD2VideoOutputConfig、InitDdsProtocol。
- 原视频/客户Publisher调用同一runtime的participantForDomain；DataDrivenTestQT/DualSensorPanel保留既有三域导出。

## P20 控制激励最后一条数据

同一候选21 ELF、同一完整4318行输入的受控比较中，原配置多次出现粗4318/精4317；增加有界Writer ACK等待仍未单独解决。SDK头文件明确 `DataWriterProtocolQosPolicy.push_on_write` 对应TCP_NODELAY，默认false。仅发送端协议Writer改为true后，MWIR/SWIR两路均收齐4318，不能把这个现象归为客户raw内部码7。

本轮只在共享XML的 `hwasimir_protocol_writer` profile 加入 `<protocol><push_on_write>true</push_on_write></protocol>`，不修改客户四类型/Topic。激励启动日志 `[DdsStimEffectiveQos]` 用SDK `get_qos`打印实际值；应为 `getReturn=0 pushOnWrite=1`。DataDrivenTestQT和命令行激励都应加载更新后的同一QoS，单改renderer侧XML不能保证远端发送端启用。STOP前最多等待250ms传输ACK；SDK返回码原样保留，该ACK不是应用排空确认，最终仍核对接受、执行、渲染、发送、解码和保存数。
