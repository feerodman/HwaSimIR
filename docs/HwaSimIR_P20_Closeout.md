# P20 工程交付与未关闭项

**工程状态 ENGINEERING_CHECKED_WITH_LATENCY_DEVIATIONS_PENDING_VISUAL，用户视觉PENDING；未切换正式current。** 已完成新云实现、Windows/AArch64构建、板端测试和真实媒体；功能、性能、局部光学和客户互通分别列出。未完成项不能被其它PASS覆盖。

入口：[离线媒体/性能/验收索引](../logs/p20/index.html)。[机器验收](../logs/p20/acceptance.json)、[性能报告](HwaSimIR_P20_Performance_Report.md)、[云设计](HwaSimIR_P20_Cloud_Rendering_Design.md)、[调度](HwaSimIR_P20_RK3588_Runtime_Tuning.md)、[三域说明](HwaSimIR_P20_DDS_Domain_Guide.md)、[源码索引](HwaSimIR_P20_Source_Index.md)。

## 实际版本

aarch64_40_prefix_prewarm，ELF `da428e18b9781ed7a61876facc483665f0213da491b60c4a396f5d9d749d22e6`，buildId `686a9f6706e8c0f4e3883fda3532810e4c01a996`。保留P19稳定24及P18，没有整体回退P17，也没有采用已撤回的P19云25/26。Windows构建日志 `logs/p20/build_windows40.log`；AArch64收据和源码快照位于 `logs/p20/build/aarch64_40_prefix_prewarm/`。未commit/push。

## 逐项范围

| 要求 | 结论与范围 |
|---|---|
| 云算法替换 | 已实现24个世界空间紧支撑椭球的解析部分光程、CPU光照预计算、共享SI/WhiteCloud光学附件；保留LegacyVolume对照 |
| 原生分辨率 | 粗320/精800，未降低最终分辨率，未低分辨率云附件上采样 |
| 不透明小目标/入云 | 实际Mali800 RGBA16F夹具30项PASS，含1/3/7像素、云前中后、边缘、FOV与双云；不同源介质排序仍为有界样本近似 |
| 透明尾焰与云 | 使用原气体每步实际深度云前缀；独立GPU云前中后/跨边缘/关闭云对照及实际Mali检查，来源见机器验收；仍属所测集合 |
| 尾焰/雨雪/深度/HDR | 原资源/尾焰形状/对数深度文件保留，SWIR/MWIR低空代表性真实视频；云交叠失败另列，用户视觉未代签 |
| 性能与调度 | 原/新算法×原/performance四组合；完整双波段和310秒运动压力按实际表格，缓存增长根因已修，短测尾延迟抖动披露 |
| 三域 | 沿用P19架构，板端150/1/1；全150、150/1/2及错误域零接收实测；共享一次Factory/QoS文件 |
| 末帧/多回合 | 标准结束NAL；parser/decoder/MP4分段计时；真实SDK精确1400边界、13秒暂停及双回合，未缩短idle伪装 |
| 同PID恢复 | 应用分支故障注入后STOP/RESET/INIT/ACK/START恢复，initCount=1；真实故障Reader原码7未复现，不能等同SDK故障恢复已证明 |
| 原链与客户链 | 共存、VideoDisplay退出重开、无VideoDisplay、禁用旧输出、客户晚加入重连均有独立证据；私有HwaFrameV2不进入客户分支 |
| 四Topic/像素/身份 | 四Topic短测、Gray8/RGB24、800Gray raw、640000/640001容量边界；任意合法身份/角色与独立模型/源；P19双路UI未改 |
| 同步RGB读回 | 原生RGB8同步读取、现有NEON转Panda底行BGR RAM、modify_ram_image推进身份；双读/全像素/同PID尺寸切换另列补充验收，未引入异步额外帧 |
| 实际客户/硬解/标定 | 全部NOT_VERIFIED；不因本轮内部测试自动关闭 |

## 当前候选真实功能记录

| 用例 | 功能结果 | 系统策略恢复 |
|---|---|---|
| [puff40_application_recovery](../logs/p20/runs/puff40_application_recovery/result.json) | PASS | True |
| [puff40_cloudy_600](../logs/p20/runs/puff40_cloudy_600/result.json) | PASS | True |
| [puff40_cloudy_original_r1](../logs/p20/runs/puff40_cloudy_original_r1/result.json) | PASS | True |
| [puff40_cloudy_original_r2](../logs/p20/runs/puff40_cloudy_original_r2/result.json) | PASS | True |
| [puff40_cloudy_original_r3](../logs/p20/runs/puff40_cloudy_original_r3/result.json) | PASS | True |
| [puff40_cloudy_performance_r1](../logs/p20/runs/puff40_cloudy_performance_r1/result.json) | PASS | True |
| [puff40_full_mwir_coexist](../logs/p20/runs/puff40_full_mwir_coexist/result.json) | PASS | True |
| [puff40_full_swir_no_vd](../logs/p20/runs/puff40_full_swir_no_vd/result.json) | PASS | True |
| [puff40_gray_inverse_topics_all150](../logs/p20/runs/puff40_gray_inverse_topics_all150/result.json) | PASS | True |
| [puff40_identity_customer_only](../logs/p20/runs/puff40_identity_customer_only/result.json) | PASS | True |
| [puff40_lowalt_band0_sky0](../logs/p20/runs/puff40_lowalt_band0_sky0/result.json) | PASS | True |
| [puff40_lowalt_band0_sky2](../logs/p20/runs/puff40_lowalt_band0_sky2/result.json) | PASS | True |
| [puff40_lowalt_band0_sky3](../logs/p20/runs/puff40_lowalt_band0_sky3/result.json) | PASS | True |
| [puff40_lowalt_band2_sky0](../logs/p20/runs/puff40_lowalt_band2_sky0/result.json) | PASS | True |
| [puff40_lowalt_band2_sky2](../logs/p20/runs/puff40_lowalt_band2_sky2/result.json) | PASS | True |
| [puff40_lowalt_band2_sky3](../logs/p20/runs/puff40_lowalt_band2_sky3/result.json) | PASS | True |
| [puff40_native_raw_600](../logs/p20/runs/puff40_native_raw_600/result.json) | PASS | True |
| [puff40_overcast_sky5](../logs/p20/runs/puff40_overcast_sky5/result.json) | PASS | True |
| [puff40_overcast_swir](../logs/p20/runs/puff40_overcast_swir/result.json) | PASS | True |
| [puff40_rgb_inverse_topics](../logs/p20/runs/puff40_rgb_inverse_topics/result.json) | PASS | True |
| [puff40_stress_310s](../logs/p20/runs/puff40_stress_310s/result.json) | PASS | True |
| [puff40_stress_310s_repeat](../logs/p20/runs/puff40_stress_310s_repeat/result.json) | PASS | True |
| [puff40_three_domains_150_1_2](../logs/p20/runs/puff40_three_domains_150_1_2/result.json) | PASS | True |

本地协议补充测试见 `logs/p20/local_tests/`；原始四Topic/容量检查日志、H264 SPS/PPS和标准SEI检查在 `logs/p20/`，并在机器验收中保留来源及版本。功能计数PASS不等于视觉或性能PASS。

原VideoDisplay退出/重开分别保存27和4198帧录像（重连从下一可解码关键帧恢复），客户两路仍完整4318帧。实际MP4已逐帧解码、独立解析HwaFrameV2、核对原SEI标注哈希、侧文件哈希和完整JSON字段。Qt会重排侧文件JSON键；两种哈希分别对应各自字节，不能要求它们相等。旧P7严格字节比较的失败日志保留，P20核验结果在两段录像目录的`p20_validation.json`。

## 启动和回退

候选路径 `/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm`。先停止两路renderer及冲突的同Topic生产者；运行：

```sh
python3 /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm
```

该脚本成对启动、持有系统策略快照，Ctrl+C仅停止其拥有的两路并恢复策略。先在UI STOP等待排空，再退出脚本。它不切换current。普通P19入口仍为 `/userdata/HwaSimIR/start_current.sh coarse` 和另一终端 `precise`；两路退出后 `use_release.sh P19` 可明确选择P19，P18回退用 `use_release.sh P18`（P18域1）。不要对候选目录宽泛删除。

DataDrivenTestQT控制域150，两路各自INIT/ACK，coarse plat1001/sensor1/320raw、precise plat1001/sensor2/800H264，同一公共源按tick推进一次；VideoDisplay域1，客户Demo域1分别TX/LDecode和TX/HD264。实际完整INI/network/QoS导出在 `logs/p20/configs/aarch64_40_prefix_prewarm/`。环境变量覆盖优先，修改域必须重启；详见三域说明。候选使用PuffHybrid及缓存维护60帧，仓库默认仍LegacyVolume；禁止把候选未通过的光学边界解释成P19正式效果已改变。

## 交接与保留证据

成对启动烟测已执行；两路退出，系统策略恢复=True；剩余renderer=''。实际current=/userdata/HwaSimIR/releases/P19，previous=/userdata/HwaSimIR/releases/P18。

用户明确授权后，逐文件核对本地归档大小和SHA256才删除完成测试的板端大日志，共释放156439141字节；26个不一致文件保留。没有删除程序、配置、P19/P18、未匹配文件或整个目录。证据在 `logs/p20/board_log_archive/`。后续测试日志改放/tmp并完整拷回本机。

## 透明路径修复与近似边界

历史23将已经混入base的透明尾焰按整条背景光程衰减。均匀slab反例（背景2、云源4、尾焰50、alpha0.5、总tau2）中，云前参考26.8647而旧式为6.9774，云内参考12.3259而旧式为6.9774。结果见 [历史解析反例](../logs/p20/optics/transparent_contract.json)。它明确否决23的透明路径，不能因23性能良好而发布。

当前按原24/40个气体步计算`Cprefix(z)+Tprefix(z)*L`，雨雪使用真实片元深度；后方云留在背景剩余贡献内。R32F参数上传规避实际Mali的RGBA浮点转换断言。30起复用已有RGBA16F云附件，R保存已传输SI、G保存白云源、B保存云SI、A保存合成覆盖率，不重复消光、不逆除T。31进一步共享云相机，保留原尾流/核心/雨雪相对顺序。具体构建身份以本报告首段为准。

35采用精确R32F gather，38启用同步原生RGB读回，40接入INIT透明实例准备。`DirectRgbReadbackAudit`与`HwaSimIRDirectRgbTiming`只用于诊断，正式运行关闭。短测P95偏差、冷启动最大值与长期结果分别披露；“无持续积压”不等于每帧都低于80ms。原模型/尾焰资源不改，透明局部云前缀带来的可见性变化仍需用户看实际媒体确认。

不同源函数的相交云仍有对象级排序近似，光学夹具只证明预设集合内误差。新表示允许形态差异，原生产视频未经增强；用户视觉和真实标定仍分别待确认/未验证。
