# P19 恢复测试记录：DDS分域与延迟

本次用户恢复板卡测试并授权停止原P18进程、独占板卡。DDS分域功能已修改、构建、部署并通过真实收发；云重负载的秒级积压仍未解决，不能宣布整个P19通过。没有把不可接受的图像变化或降低成像规格作为成功优化。

## 可直接使用的域配置

板端两路现为控制150、原VideoDisplay视频1、客户TX视频1；两路INI修改前均有备份。新的三个键、环境覆盖优先级、重启要求和共享QoS限制见 [DDS域说明](HwaSimIR_P19_DDS_Domains.md)。UI构建12分别导出这三项，推荐启动 `build-p19-stim/release/start_control150_video1.cmd`。全150、全1配置继续保留。

旧功能版08真实复现了静默覆盖：配置原视频1，实际初始化和客户发送仍用控制域150。新的同工厂多Participant实现修复此缺陷；不能据此断言用户当天所有SDK报错都已查明，缺少当时报错原文，原SDK内部码7根因仍未确认。

## 功能回归

构建19完成：150/1/1双路两回合；150/1/2三独立域；错域客户raw订阅0样本；旧VideoDisplay的SEI、Meta、Annotation关联及板端诊断/时钟反馈；真实UDP可靠Reader故障后同PID第二回合900帧；全150回归；禁用原视频且不启动VideoDisplay时客户独立输出；直接读取真实板端INI；2400帧/路晴空场景下VideoDisplay退出重开。每个进程initCount=1。

UI新增域严格数值检查，17项有效/无效输入及旧配置迁移通过，实际导出读回150/1/1通过。配置保存/导出函数路径已验证，原生保存对话框自动化仍未重新验收。详见 [机器可读分域结果](../logs/p19/resumed_domains/acceptance.json)。

## 性能实验：没有通过

以下是同一原始轨迹的offset3000、600帧重云片段，发送按60Hz调度，正式800×800精跟；不是把历史3600帧长测与600帧短测直接比较。功能完整收发与性能分别判断。

| 实验 | 精跟实际渲染 FPS | 精跟 STOP排空 ms | 证据 |
|---|---:|---:|---|
| 原路径，双路raw | 40.12 | 5012.9 | [原始测量](../logs/p19/runs/cloud_resumed_reference_raw/performance.json) |
| 裁剪代理/深度剔除 | 39.31 | 5344.6 | [原始测量](../logs/p19/runs/cloud_clip18_raw/performance.json) |
| 主线程固定大核 | 39.72 | 5157.1 | [原始测量](../logs/p19/runs/cloud_bigcore18_tail/performance.json) |
| 显示积分半精度 | 41.87 | 4379.5 | [原始测量](../logs/p19/runs/cloud_half21_raw/performance.json) |
| 仅精跟 | 40.29 | 4941.6 | [原始测量](../logs/p19/runs/cloud21_precise_alone/performance.json) |
| RGBA最终缓冲 | 21.41 | 18070.8 | [原始测量](../logs/p19/runs/cloud_rgba22_raw/performance.json) |
| FBO切换提前提交 | 40.03 | 5033.7 | [原始测量](../logs/p19/runs/cloud22_flush_raw/performance.json) |
| 仅RGBA读回 | 23.54 | 15539.3 | [原始测量](../logs/p19/runs/cloud22_read_rgba_raw/performance.json) |
| PBO同帧读回 | 21.45 | 18054.9 | [原始测量](../logs/p19/runs/cloud22_pbo_raw/performance.json) |

单独运行精跟仍约40FPS，说明主要不是另一路竞争导致。插桩测得读回前GPU等待约10.7–11.3ms、完成GPU工作后RGB读回约3.4ms；这些插桩会改变调度，仅用于分解耗时。驱动实际推荐的读取格式为RGB/UNSIGNED_BYTE。RGBA与同帧PBO试验更慢，提前flush无改善；都已撤出正式源码。半精度小幅提升不足以消除积压，也未作为正式视觉优化接收。

数字图像对照分别保存在各raw用例的visual_raw_comparison.json，只是数值证据，不自动代表视觉验收。未降低输出分辨率、名义60FPS/80ms目标，没有丢帧、复用旧帧或以接收完整冒充性能通过。原图像路径恢复，尾焰、云雨雪、深度、HDR保持原实现。

晴空场景的分域双路2400帧测试没有持续积压，解码端延迟记录见 `domains19_old_reopen/performance.json`。这里的端点是同Windows时钟下的输入到解码图像，不是屏幕呈现时间。云重负载仍需继续解决渲染/读回关键路径，当前状态为UNRESOLVED，绝不标成环境BLOCKED或PASS。

## 交付与边界

最终构建和摘要以 `logs/p19/deployment/current.json` 为准；正式源码不包含被否决的云/读回候选。新包为 `HwaSimIR_P19_Resumed.zip` 与 `HwaSimIR_P19_Customer_Domains.zip`，保留旧包，不含许可证。原输入、模型、四个客户IDL和Topic未改，未自动commit/push。已识别的通信默认键变更与冻结成像资源核验见 `baseline/frozen_check_current.json`。

真实客户现场raw最终互通、未知客户硬件解码、屏幕呈现时延均NOT_VERIFIED。新分域功能通过不意味着这些外部验收项目通过。

## 最终稳定版复核

最终ARM构建 `aarch64_24_domains_clean`，ELF SHA256：`b7e50a1b4d7d919e1e79b99770a5444e4367457ddd758d84f68c7dc14ab103e3`；Windows renderer构建09、UI构建12均通过。两路板端实际ELF与INI摘要已读回，current=P19、previous=P18、无HwaSim_IR测试进程；6个临时GL探针库在板外备份校验后移除。板端时钟与控制机日期不同，回执分别标明来源；性能使用steady_clock，不以板端墙钟计算跨机时延。

- `domains24_ini_rounds`：真实INI150/1/1，两回合各240帧；旧VideoDisplay两段各120帧经ffprobe独立验证，原Meta/Annotation关联无错配，客户末帧从结束码收到到解码0.8604ms、MP4完成379.8065ms。
- `domains24_rgb_swapped`：RGB24，TX/LD264和TX/HDecode各30帧，码流/raw逐字节核对并保存成功；结合主用例覆盖四Topic。
- `domains24_deployed_qos`：不覆盖域、也不覆盖renderer QoS，实际INI和Config/DDS/ZRDDS_PROTOCOL_QOS.xml，60帧/路通过，三个域来源均ini，initCount=1。
- `cloud24_final_tail`：600帧/路功能完整，但精跟39.22FPS、最大输入积压216帧、STOP5385.206ms、输入到解码图像p95=5179.2409ms；粗路60.15FPS、STOP34.348ms。性能FAIL。约10秒输入需要15.3秒渲染，不能用快速H264末帧排空掩盖前面的5秒积压。

冻结资源核查：4444个文件无缺失，唯一变化是Runtime.ini新增CustomerDdsVideo.DomainId=-1及注释；剔除此通信默认键后与原文件完全相同。成像资源不变，正式HwaSimIR.cpp中的云试验已撤出。
