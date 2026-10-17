# P19 DDS 域配置与分域实测

本轮继续处理域配置和云延迟问题；以新的构建回执和实测为准，旧功能交接报告是历史快照。

## 旧程序的问题

原功能版08的 `LoadF1ProtocolConfig()` 在控制和旧视频均启用时，直接将 DdsVideo.domainId 改成 DdsProtocol.domainId；客户分支借用同一个Participant。因此填写控制150、旧视频1时，实际三个链路都可能在150。启动前的 `D2VideoOutputConfig ddsDomain=1` 与后面的实际初始化日志domain150互相矛盾，容易误判配置生效。

`logs/p19/runs/domain_old_silent_override/` 已真实复现：请求旧视频1，实际旧视频和客户视频日志150，在客户150接收30帧。该次还检测到另一个P18实例在运行，故仅作为配置覆盖/字节收发证据，不用作独占性能基线。用户随后授权停止P18，后续测试独占板卡。

这能确认配置覆盖缺陷；没有当时具体错误日志，不能认定用户遇到的每条DDS错误都是它导致，更不能将SDK内部码解释为errno。

## 新配置对应关系

| HwaSimIRRuntime.ini | 控制的链路 | 缺省 |
|---|---|---|
| `[DdsProtocol] DomainId` | INIT、Control、Realtime输入，INIT ACK；控制UI的VideoStatus反馈 | 150 |
| `[DdsVideo] DomainId` | 给HwaSim_IR_VideoDisplay的旧视频、原SEI；配套VideoMeta、Annotation、VideoStatus和诊断时钟/性能信息 | 150 |
| `[CustomerDdsVideo] DomainId` | 四个客户TX Topic的视频 | -1，继承DdsVideo的域 |

三个域可以独立指定0..232。只有客户项允许-1表示继承；不要给控制/旧视频填-1。域相同复用Participant，域不同在同一进程级工厂下创建Participant；不重复全局Init/Finalize。客户分支仍只输出四个既有类型/Topic，不新增客户元数据。

分域时VideoStatus在旧视频域发布，并在控制域发布一份供激励UI观察；同域只发一份。Meta/Annotation和诊断跟随旧视频域。旧协议字段与客户IDL没有变化。

推荐控制150、两种视频1：

```ini
[DdsProtocol]
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml
[DdsVideo]
DomainId=1
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml
[CustomerDdsVideo]
DomainId=1
```

若要原VideoDisplay留在150、只给客户1，把DdsVideo改为150，CustomerDdsVideo仍显式设1即可。激励程序 `SenderNetworkConfig.ini [DdsProtocol] DomainId` 对齐控制域；VideoDisplay的NetworkConfig.ini/`--dds-domain`对齐旧视频域；客户Demo `--domain` 对齐客户域。不要把renderer本地network.ini中的身份配置误当成HwaSimIRRuntime.ini里的DDS域配置。

## 配置生效与限制

- 域配置在启动时读取，STOP/RESET不会切换已经创建的Participant。改INI后正常退出并重启对应renderer和接收器。
- `HWASIMIR_RUNTIME_CONFIG`指定实际INI路径；未指定时按程序配置搜索路径加载。先核对日志中的实际文件路径。
- `HwaSimIRDdsProtocolDomainId`、`HwaSimIRDdsVideoDomainId`、`HwaSimIRCustomerDomainId`环境变量优先于INI。新日志`[DdsDomains]`分别给出有效值和env/ini/default/继承来源。
- 一个工厂只加载一套QoS XML；当前不是三个域各自独立加载不同XML。控制启用时以DdsProtocol.QosFile为共同文件，冲突的视频QoS路径会明确记录请求值和共同文件。不同reader/writer仍用各自既有profile。
- 同一域、partition和固定TX Topic不要同时启动两个相同角色生产者。P18/P19并行或粗精角色配置重复，会产生混流，不能靠platID替客户固定Topic分流。

## 实测记录（持续更新）

- `domains150_1_1_dual_two`：控制150、旧视频1、客户1，粗精各两回合240帧，真实INIT/ACK、同PID、逐帧/码流一致；VideoDisplay240帧，meta/annotation关联无错配。构建17时性能诊断尚停留控制域，后续修正另行验收。
- 三独立域、错误域零接收、同PID故障恢复及最终诊断域回归以 `logs/p19/resumed_domains/acceptance.json` 为准，未完成的用例不提前标PASS。

- `domains150_1_2_isolation`：三域150/1/2分别工作；错在150域订阅客户raw实际收0，正确客户2域收120帧，两路及旧VideoDisplay均正常。
- `domains19_telemetry_two`：构建19，三域150/1/2两回合共240帧/路，VideoDisplay诊断available=true、clockValid=true，原帧身份/Meta/Annotation正确。
- `domains19_raw_sdk_recover`：三域150/1/2，UDP可靠故障Reader真实挂起后出现SDK写失败；移除故障Reader后同PID第二回合900帧逐字节一致，initCount=1。
- `domains19_same150`：全150回归，60帧/路通过。
- `domains19_customer_only`：控制150、客户2、原DDS视频禁用、不启动VideoDisplay，60帧/路通过。
- `domains19_ini_actual`：板端两个真实INI明确设150/1/1，不使用域环境覆盖，启动日志三个source均为ini，120帧/路及旧VideoDisplay通过。修改前INI备份在 `logs/p19/resumed_domains/deployed_150_1_1_v2`，运行时摘要与部署完整性元数据同步更新。

双路UI构建12增加三域导出，17项合法/非法配置与迁移测试通过。`ConfigExamples/P19/control150_video1`为推荐分域样例；仍保留全150和全1样例。旧P18行为未改，回滚后按P18自身实际域1使用。

最终稳定版24补测见 [恢复测试报告](HwaSimIR_P19_Resumed_Test_Report.md)：真实INI两回合、四Topic/RGB，以及不覆盖原QoS路径均通过。
