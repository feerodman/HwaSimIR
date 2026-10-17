# P19 实施与验收记录

P19原功能交付后，用户已恢复测试并授权本轮独占板卡。本次新增并实际验证三条DDS链路独立配置域，修复旧版静默覆盖；云绘制延迟继续排查，尚未宣布通过。最新程序与配置摘要以 `logs/p19/deployment/current.json` 为准，旧08交接回执仅作历史证据。功能、图像、性能和真实客户互通分别判定。

独立域说明与直接可用配置见 [DDS域配置](HwaSimIR_P19_DDS_Domains.md)。板端当前控制150、原视频1、客户1；两路从实际INI读入测试通过。UI构建12分别导出三个域，17项校验通过。原IDL、模型、尾焰、HDR和深度修复保持。

## 已实测的功能

| 项目 | 结果与证据（相对仓库根目录） |
|---|---|
| 原始大 Sample | PASS：`logs/p19/tests/domain1_tcp_matrix01/matrix.json`、`board_domain1_tcp_matrix01/matrix.json`；9种长度 × 1/10/60Hz，包含102400、307200、640000字节 |
| 同 PID 恢复 | PASS：`logs/p19/runs/udp_fault_final08/result.json`；真实可靠 UDP Reader 停止取样触发 SDK write 返回10；移除故障 Reader，STOP/RESET/INIT/ACK/START 后第二回合900帧；DDS全局初始化计数1 |
| 四 Topic / 像素格式 | PASS：`swapped_topics_rgb30`、`swapped_gray30`、`normal_rgb30`、默认双路；raw实际长度、逐帧摘要与H264整流摘要核对 |
| 独立客户输出 | PASS：`customer_only_precise1`；不依赖VideoDisplay及原视频输出使能 |
| 双路与任意身份 | PASS：`ids_models_dual30`；sensor7/42、不同plat、角色对应Topic不随sensor号硬编码 |
| 独立模型/数据源 | PASS：`independent_sources_final`；两路不同已有模型、不同普通文件；默认同源调度一次推进 |
| 两回合 | PASS：`dual120_two_rounds_final`；同renderer PID，240帧/路，独立INIT/ACK |
| H264末帧 | PASS：1/30/120帧、1400整除AU、结束NAL跨Sample、暂停、两回合；`logs/p19/tests/end*` |
| 异常截断 | PASS（负向验证）：`tests/truncated_final`；拒绝把缺少标准结束的截断流宣称正常完成 |
| 晚加入/重连 | PASS：`tests/receiver_late_reconnect_v2`；真实SDK接收540帧，解码/显示更新/保存一致 |
| 原链退出重开 | 原链功能PASS：`dual60s_old_reopen_final`中1051+2370帧真实HwaFrameV2 SEI；但该用例客户精跟因旧90秒测试期限不足失败，保留FAIL，不能整体改写成PASS |
| 客户纯流 | PASS：`logs/p19/h264/private_sei_audit.json`；实际UUID解析，客户流0个私有SEI，旧流保留 |
| UI加载/验证/迁移 | PASS：`logs/p19/ui/validation_migration_fixed`；11类合法/非法输入；截图`scale150_final.png`。原生保存对话框自动化未完成，不声称该UI路径已通过 |
| 独立编译 | PASS：Windows Qt完整接收包独立构建；最小raw无Qt ARM CMake构建、板端实收222样本无缺口/重复/非法长度 |
| 版本整理 | PASS：P17板外归档与恢复摘要核验；P18两路各181帧回滚烟测；切换P18再P19；只保留两个完整部署，精确清理释放584973312字节 |

表内未以`logs/`开头的运行名位于`logs/p19/runs/`，测试名位于明确标注的`logs/p19/tests/`。历史失败和重试均保留。

## 当前性能结论

在同一RK3588上运行粗320 raw与精800 H264，原始数据前3600行、各60Hz、无VideoDisplay：晴空两路约60FPS；多云精跟后段降至约35FPS并形成秒级积压，功能可完整收到3600帧，但性能不通过。温度与CPU/GPU频率采样没有发现活动期间降频；编码、DDS写入及接收解码不能解释主要积压，绘制耗时随云覆盖增长。

`dual60s_cloud_diagnosis`、`dual60s_cloud_specialized09`、`dual60s_cloud_specialized10`均完整收发，STOP仍约9.4–9.7秒。常量特化和面剔除未实测获得改善，不能作为成功优化交付。后续候选也未消除积压；`dual60s_cloud_radiometric_diagnostic`仅诊断关闭白色外观，STOP约2.8秒，该外观改变不作为正式方案。该暂停已由用户明确恢复；旧实验仍为FAIL，新实验单独记录，不能算修复完成。详见[云性能暂缓记录](HwaSimIR_P19_Cloud_Deferred.md)。

报告按每路分别统计Sample、AU、帧与保存数。`performance.json`的Windows激励发送至解码QImage时间使用同一Windows单调时钟，并核对业务摘要与路由顺序；它不是屏幕呈现延迟。板卡/Windows未校准的墙钟不直接相减。60FPS / 80ms目标未降低，59.99FPS等小偏差不单独阻断；秒级积压保留为待解决问题，不将本次功能交付解释为性能验收通过。

## 用户关心的七项结论

1. **12秒**来自接收工具idle退出后才flush。改为正常STOP发送标准H264结束NAL，接收侧跨分片识别并排空parser/decoder；最终双回合末次parser输出0.0885ms、decoder输出0.8948ms、MP4完成514.0636ms，单回合短测封装约114–143ms（均从接收结束码计时，见`h264/final_timing_summary.json`）。完整业务STOP另计，不能用末帧快掩盖云绘制积压。
2. **raw故障**分两层：现场最初transport码7根因尚未确认；本轮真实复现API10历史空间超时，并修复客户回合失败后的明确同PID恢复。操作是移除故障Reader → STOP → RESET → 分别INIT/等ACK → START。无重复全局Init/Finalize。
3. **客户代码**从`DDS/CustomerRawReceiverMinimal/README.md`开始；默认Domain1、TX/LDecode、320×320 Gray8。完整四Topic接收/解码/显示/保存见`DDS/HwaSimIRCustomerTxVideoReceiverDemo/README.md`。可交付压缩包在`logs/p19/deliverables/HwaSimIR_P19_Customer.zip`，不含许可证。
4. **双路UI**每路独立角色、platID、sensorID、完整INIT和发送勾选；默认粗320raw/精800H264，共用1.txt及目标，可逐路选模型、覆盖数据源。普通文件不限制名称/摘要/4318行。平台级Control仍会联动同plat实例，UI提示真实限制；本地身份/域/QoS经导出配置和启动修改。
5. **H264**板端默认实流为Baseline L4.0、800×800@60、refs1、CAVLC、B0、GOP30、重复SPS/PPS。硬解候选为L3.2、AUD、4Mbps CBR；详见兼容性文档的INI键及实际SPS报告。未知客户硬件、完整HRD/CPB一致性未验证。
6. **版本**板端`/userdata/HwaSimIR/releases/P19`与`P18`，`start_current.sh coarse/precise`启动；退出renderer后`use_release.sh P18`回滚，`P19`恢复。P17位于本机`logs/p19/board/p17_offboard/user_p17_v2.tar.gz`，已校验可恢复，不在客户包内。
7. **证据边界**本轮亲测的是本机/板卡ZRDDS、MPP/FFmpeg、软解、UI逻辑与构建、版本回滚。客户软件H264为USER_REPORTED；客户现场raw最终互通和硬解为NOT_VERIFIED。认可的P18视觉为用户此前确认，冻结文件本轮核验一致；云优化图像与性能仍在恢复测试，尚未通过。原生UI保存对话框自动化和屏幕呈现时延未验证；配置保存/加载/导出函数路径已验证。

## 详细操作与源码

- [raw排障与恢复](HwaSimIR_P19_CustomerRawDDS_Troubleshooting.md)
- [H264参数、收尾与硬解边界](HwaSimIR_P19_H264_Compatibility.md)
- [双路UI指南](HwaSimIR_P19_DualSensor_UI_Guide.md)
- [版本启动、回滚与P17归档](HwaSimIR_P19_Board_Releases.md)
- [源码与关键函数索引](HwaSimIR_P19_Source_Index.md)
- [完整DDS链](HwaSimIR_DDS_EndToEnd_Guide.md)、[客户ICD](HwaSimIR_Customer_TX_DDS_ICD.md)、[双路运行](HwaSimIR_P18_DualChannel_Runbook.md)

工作区没有自动commit/push。最终构建身份以`logs/p19/deployment/current.json`为准，旧08状态已被恢复测试的新构建取代。入口为`logs/p19/index.html`，机器可读结论为`logs/p19/acceptance.json`。客户独立包为`logs/p19/deliverables/HwaSimIR_P19_Customer.zip`；完整功能交付包为同目录`HwaSimIR_P19_Functional.zip`。交付包含实际程序、源码、配置、收发媒体与结果；运行所需授权及冻结的大资源继续使用现有配套环境，不把用户许可证放进压缩包。

## 恢复测试交付补充

最新为aarch64_24_domains_clean / Windows09 / UI12。DDS分域实测通过，重云精跟最终39.22FPS、STOP5.385秒仍FAIL；无效候选撤出，原成像保留。测试进程已停止。当前交付包为HwaSimIR_P19_Resumed.zip及HwaSimIR_P19_Customer_Domains.zip，旧Functional.zip是08历史交接包。详细数据和边界见[恢复测试报告](HwaSimIR_P19_Resumed_Test_Report.md)。
