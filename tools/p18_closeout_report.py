"""Write the final closeout using completed evidence, with explicit scoped failures."""
import json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];B=ROOT/'logs/p18'
def read(name):return json.loads((B/name).read_text(encoding='utf-8'))
main=read('runs/dual_perf_on_v4/result.json');mwir=read('runs/dual_final_mwir_v4/result.json')
perf=read('performance_comparison.json');media=read('media_audit.json');environment=read('final_environment.json')
coarse=main['coarse'];precise=main['precise'];off=perf['comparison']['off'];on=perf['comparison']['on']
held=read('runs/held_dds_history_v4/result.json');heldlog=(B/'runs/held_dds_history_v4/board.out.log').read_text(errors='replace')
heldErrors=[s for s in heldlog.splitlines() if '[CustomerTX][ERROR]' in s]
heldDrains=[s for s in heldlog.splitlines() if '[CustomerTXDrain]' in s or '[ControlStopResult]' in s or '[SyncRoundConservation]' in s]
heldOld=read('runs/held_dds_history_v4/old_sei_check.json')
heldTimeout=any('typed_write_failed_or_timeout' in s or 'drain_timeout' in s for s in heldErrors)
bad=[m for m in media if not m['readable']];mismatch=[m for m in media if m.get('countMatchesReceiver') is False or m.get('countMatchesOldIndex') is False]
def f(value):return f'{value:.6f}'
def old_movies(case):
    return '；'.join(f"{m['path'].split('/'+case+'/')[-1]}：{m.get('decodedSavedFrames','未读出')}帧" for m in media if f'/{case}/old_recording' in m['path'] and m['readable'])
def rx_count(case):
    r=read(f'runs/{case}/result.json');return f"{r['receiver']['decodedFrames']}解码 / {r['savedFrames']}保存，decodeErrors={r['receiver']['decodeErrors']}"
rateRows=[]
for mode,values in [('关闭',off),('开启',on)]:
    for role in ['coarse','precise']:
        r=values[role];s=r['boardStages'];rateRows.append(f"| {mode} {role} | {f(s['render']['fps'])} | {f(s['captureTaskCompletion']['fps'])} | {f(s['output']['fps'])} | {f(r['receiveFrameCompletion']['fps']) if mode=='开启' else '不适用'} | {f(r['decodeSteadyBeforeIdleFlush']['fps']) if mode=='开启' else '不适用'} |")
loopRows=[]
for role in ['coarse','precise']:
    r=on[role]['windowsStimulusToDecodedImageMs'];s=r['steadyAfterWarmupExcludingIdleFlush'];loopRows.append(f"| {role} | {s['p50']:.3f} | {s['p95']:.3f} | {s['maximum']:.3f} | {r['lastFrameMs']:.3f} | {r['allFramesAtMost80Ms']} |")
scope=[
 (1,'尾焰/P17/天气','局部尾焰外形改善；天气保留回归完成，诊断深度修复；视觉仍是工程外观，非标定'),
 (2,'DDS完整链路','两张链路图、Topic/Type/方向/作用、配置优先级、身份路由和源码函数索引已交付'),
 (3,'原链保留/clean AU','客户在HwaFrameV2注入前复制；原SEI、实时数据、标注、writer stamp实际解析验证'),
 (4,'四结构/四Topic','SDK生成全局无key类型，四Topic实际生产收发；1400分片/raw单Sample，不加客户头或Topic'),
 (5,'Gray8/RGB/容量','top-down RGB24与Gray8色卡通过；raw超限INIT拒绝；旧1024与客户H2641024正向验证'),
 (6,'运行时/有界失败',('真实DDS Reader受控测试触发客户有限失败，原链继续，详见故障记录' if heldTimeout else '应用队列溢出隔离通过；不take的DDS Reader测试未触发发送端超时，不宣称该分项通过')),
 (7,'同板双实例','SWIR/MWIR均为两个原生320/800实例、两INIT两ACK、同一原始4318行；主计数'+str(main['functionalCountResult'])),
 (8,'独立Demo','最终Windows v4只用四客户类型，实际收发/显示/保存；停顿、晚加入、重连复测；真实客户互通NOT_VERIFIED'),
 (9,'独立运行/生命周期','无VD、客户独开、原新共存、VD重开、客户缺席/晚加入/重连/慢接收/有界失败、两回合及尺寸切换均有分项记录'),
 (10,'完整交付','四文档、源码/构建产物、启动命令、before/after、同期双路媒体、原生窗口、机器验收与manifest；性能不冒称通过')]
text=f'''# P18 执行与验收记录

本阶段已完成代码修改、Windows/AArch64构建、RK3588隔离部署和真实测试。**整体 P18 为 NOT_PASSED**：严格性能结论见第6节；真实客户互通、真实辐射标定、跨主机单向和屏幕呈现时延为 NOT_VERIFIED。此前板卡断连已由用户恢复，后续雪场景及最终矩阵完成；不再把已恢复的网络写成当前阻塞。

入口：[媒体与证据索引](../logs/p18/index.html)、[机器验收](../logs/p18/acceptance.json)、[性能](../logs/p18/performance_comparison.json)、[媒体逐帧审计](../logs/p18/media_audit.json)、[文件manifest](../logs/p18/delivery_manifest.json)、[源码函数索引](../logs/p18/source_index.json)。失败原件保留，不用编译成功、有画面或MP4标称FPS代替验收。

## 1. 基线与最终部署

工作区 `D:\\HwaSimIR`，HEAD `88730b3c373f011424a5fdc79ef0390b1ffaef2d`，未commit/push。初始4383个保护文件及输入附件摘要见 [baseline/identity.json](../logs/p18/baseline/identity.json)。原始 `1.txt` SHA256=`f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`，4318行、首659行隐藏目标、源时间、业务状态与ViewValid保留。

起初板端ELF/atlas落后于最新P17，`*_live`只作为陈旧部署观察，不是有效before。随后恢复并校验P17v12，ELF=`1b224d73a69f26c15c2eaaa8f563491a9c7ffcd3aa4f2f820f0ec255fc895919`，Config manifest=`75a77cb4aa822ec98215091a3d0ba36f42756786cdaf0d731f80bd341382e200`；有效before均以 `_p17v12` 结尾。

最终renderer部署在 `/userdata/HwaSimIR/p18/candidate4/{{coarse,precise}}`，共用ELF SHA256=`f7dddbdb4e19f4e3676f9e288255c59ee86cf41ae0817a3aa724bc09e044e40d`，BuildID=`830175b234d2959b987a1a1e486504632c5784ac`，Config manifest=`21f53d40be8edc775549e72a2e2ffd4602dd1eb4a3450664a09ab918a3d3c9c1`。两实例分别隔离cwd、network/runtime INI、日志与licence运行副本。[部署回执](../logs/p18/deployment/candidate4/receipt.json)。主目录 `/userdata/HwaSimIR` 保留P17v12。

Windows renderer构建 [build_windows_05.log](../logs/p18/build_windows_05.log)；AArch64 [build/aarch64_v3](../logs/p18/build/aarch64_v3/)；最终Demo v4 [build_receiver_08.log](../logs/p18/build_receiver_08.log)；最终激励 [build_stimulator_final.log](../logs/p18/build_stimulator_final.log)。Demo交付目录 `build-p18-customer-receiver-v4/release`。Linux版客户Demo未构建，NOT_VERIFIED；不能用AArch64 renderer构建冒充。

[保护复核](../logs/p18/protected_preservation.json)检查原模型、输入、旧协议、天气资源、固定HDR及对数深度资源。允许变化仅两个尾焰sprite、新客户配置/QoS和重建EXE。诊断P5生成喷口/遮挡板另外局部修正深度写入；没有修改生产模型、天气或物理profile。

## 2. 用户十项要求

| 项 | 要求 | 结果与边界 |
|---|---|---|
'''+ '\n'.join(f'| {n} | {title} | {result} |' for n,title,result in scope)+f'''

## 3. 同板双路主验收

主证据 `dual_perf_on_v4`，MWIR复验 `dual_final_mwir_v4`，均使用candidate4和最终Demo v4。原DDS开启，全程无后台VideoDisplay。唯一激励分别向1001/1与1001/2 INIT，等待两份真实身份ACK（ackMask=3），平台级START/STOP；Realtime每行只广播一次sensor255。本地身份仍为1/2，不把255当成本地sensor。

| 实例 | 原生输出 | Topic | 接受/执行/发送 | 收到Samples | 在线解码/独立保存 |
|---|---|---|---:|---:|---:|
| 1001/1 coarse | 320×320 raw Gray8 | TX/LDecode | 4318/4318/{coarse['senderFrames']} | {coarse['receivedSamples']} | {coarse['decodedFrames']}/{coarse['savedFramesVerified']} |
| 1001/2 precise | 800×800 H264灰度 | TX/HD264 | 4318/4318/{precise['senderFrames']} | {precise['receivedSamples']} | {precise['decodedFrames']}/{precise['savedFramesVerified']} |

粗路每帧102400字节，共{coarse['receivedBytes']}字节，SHA256=`{coarse['streamSha256']}`；逐帧发送/接收摘要匹配。精路共{precise['receivedBytes']}有效字节，clean AU拼接和接收重建SHA256=`{precise['streamSha256']}`；分片数等于各AU ceil(bytes/1400)之和。身份/源序列/业务摘要审计见 [主审计](../logs/p18/runs/dual_perf_on_v4/identity_timing_analysis.json)。MWIR功能计数结论 `{mwir['functionalCountResult']}`，详见 [结果](../logs/p18/runs/dual_final_mwir_v4/result.json)。

[粗路完整视频](../logs/p18/runs/dual_perf_on_v4/coarse/output.mp4)、[精路完整视频](../logs/p18/runs/dual_perf_on_v4/precise/output.mp4)、[同期两路并排完整回放](../logs/p18/visual/dual_simultaneous_recordings.mp4)。并排文件按实际同轮录像的逻辑帧顺序配对，粗路保持320并补黑边，非实时桌面截图，不用于时延。

[实际粗路窗口](../logs/p18/visual/dual_live_coarse.png)、[实际精路窗口](../logs/p18/visual/dual_live_precise.png)来自同一正式运行中原生窗口捕获，[采集时间与窗口身份](../logs/p18/visual/dual_live_windows.json)明确记录。它们是先后获取的两个窗口，GUI截图受桌面缩放影响；原生视频尺寸由媒体审计确认，不把截图大小冒充渲染尺寸。

两回合 `dual_two_rounds_v2` 两路各8636产品，逐回合logical/source序列1..4318、raw frameId复位、runtimeInitCount=1；[两回合审计](../logs/p18/runs/dual_two_rounds_v2/round_identity_analysis.json)。最终代码的后续renderer变动仅诊断几何深度；客户Demo后续变动为可靠停顿处理与哈希实现，不把不同版本的测试混称同一次运行。

## 4. 协议、像素和生命周期

四个原始IDL全局无key类型保持字段/顺序/大小写，`VedioStream`原拼写不改。两套实际SDK生成器输出逐文件比较见 [生成类型比较](../logs/p18/tests/generated_type_comparison.json)。[真实typed/QoS测试](../logs/p18/contract_test_qos_final.log)读回压缩4096、raw16的Reader/Writer max_samples，100ms writer等待；四类型实际往返通过，覆盖1/1399/1400/1401/2800/2801、整除无哨兵、尾部清零、非法长度及全字节范围。

| 实际案例 | 路径与结果 |
|---|---|
| precise_raw800_gray_v2 | HDecode 800 Gray8，4318帧，每帧640000字节；最终v4追加 raw800_receiver_v4：{rx_count('raw800_receiver_v4')} |
| coarse_h264_rgb_customer_only_v2 | LD264 RGB，旧DDS/TCP payload/本地录像全关，4318帧/4441片；独立读回/编码/回合门控有效 |
| coarse_raw_rgb_colorcard_v2 | LDecode RGB24，4318帧×307200字节，RGB顺序/上下方向色卡误差0 |
| coarse_h264_rgb_colorcard_v2 | LD264 RGB有损色卡最大误差2；最终 ld264_rgb_receiver_v4：{rx_count('ld264_rgb_receiver_v4')} |
| precise_h264_gray_colorcard_v2 | HD264灰度4318帧/4462片，最大误差1，U/V均128 |
| precise_h264_rgb_shared_v2 | 4318帧共享clean AU；原SEI保留、客户0私有UUID；不是复制旧DDS载荷 |
| reject_raw800_rgb_v2 / reject_raw1024_gray_v2 | 1920000/1048576字节明确拒绝INIT，不发成功ACK，不截断/缩放/换格式 |
| old1024_customer_disabled_v2 / customer_h264_1024_v2 | 旧1024保存4318帧；客户1024 H264保存1200帧，raw限制不污染合法压缩输出 |
| resize_raw320_to640_v2 | 同进程两回合同步接收重配，1201×102400及1200×409600，逐帧hash及复位通过 |
| customer_gray_encoder_override_v2 | 客户独开，INIT raw而本地显式h264，日志请求/实际不同，1201帧 |
| customer_absent_graceful_v3 | 无客户，原链4318帧并STOP；API接受不能当客户收到 |
| customer_slow_v2 | raw320工作线程延迟10ms，4318收齐，有界队列 |
| old_reopen_graceful_v4 | 原VD退出/重开期间客户 {rx_count('old_reopen_graceful_v4')}，0误重置；原VD两段为加入期间局部录像 |
| late_idr_recovery_v4 / reconnect_idr_recovery_v4 | 最终Demo实际晚加入/重连，加入后SPS/PPS+IDR恢复，0真实解码错误；离线帧不补发 |

最终两Topic真实跨Sample/跨NAL、三秒中途停顿测试：[HD264](../logs/p18/tests/incremental_HD264_v4_gap/verification.json)、[LD264](../logs/p18/tests/incremental_LD264_v4_gap/verification.json)，均120在线解码/120保存，177片、172508字节，SHA256=`53889405249e39d9deba56eba3e1962cc50157b89ba7ff1929d9b7262f2d01ac`。v3曾按两秒收包间隔重置解码，导致VD重开案例完整码流可保存4318但在线仅4291；v4去掉错误的时间推断，真实解码错误仍显式failed。FFmpeg SHA256减少审计开销，摘要一致性见 [测量](../logs/p18/tests/hash_benchmark.json)，没有削减审计或丢弃帧。

原SEI验证使用实际 `HwaFrameV2::extract`，不是仅搜索字符串。实时状态、标注、writer stamp及身份连续性见 [原链和客户逐流解析](../logs/p18/old_and_customer_sei_audit.json)。原灰度共存案例私有产品/实时数据/标注/writer stamp各4318；客户私有产品0。最终VD重开录像：{old_movies('old_reopen_graceful_v4')}。保留旧decoded gateway/UDP/TCP协议，optional gateway本轮未运行，NOT_VERIFIED。

## 5. 尾焰与天气

保留P17连续体积积分与对数深度，不恢复旧32周期billboard。局部修改 `GameVFX/sprite.vert/.frag`：core轴向范围1.2→1.5，核心半径向后收窄到0.35，halo包络1.25→0.35、密度0.22，柔化后段渐隐并保留连续轻微漂移。不改温度、发射率、整机热状态、全局曝光、逐图AGC、物理profile或天气参数。

P17v12 before包括两波段25/10µrad完整4318帧。最终普通after为 `plume_{{swir,mwir}}_old_graceful_v3`，相同10µrad、engine=1、同输入/固定曲线，各4318帧；客户与原VD均正常收尾。sourceSeq900/1200/2400/3600/4200全800×800图及明确标注ROI×2最近邻裁剪可查。后段可见原膨大尾部收为细长连续亮核心、柔边渐隐。[SWIR4200](../logs/p18/visual/swir_10_4200_comparison.png)、[MWIR4200](../logs/p18/visual/mwir_10_4200_comparison.png)、[来源索引](../logs/p18/visual/frame_index.json)。工程外观改善不等于用户已签字认可或真实辐射标定。

两波段侧/斜/端视、近/远、遮挡v3及320诊断、engine off、core/halo/mask均已执行。P5不透明诊断喷口/遮挡板原先固定函数深度与P17对数深度混用，造成假透出；局部绑定现有 `IRDepthPrecision::apply` 后，相同sourceSeq240遮挡板ROI由SWIR17..26→17..17、MWIR72..96→72..72。[遮挡审计](../logs/p18/occlusion_diagnostic.json)。v2遮挡证据因此判为无效/失败，不拿它证明遮挡正确。诊断喷口辐亮度固定且侧视裁掉尾部，只用于几何/分层检查，不能替代普通生产外观。engine off同时影响原机体热状态，on/off差图不冒称纯尾焰辐射分离。

两波段晴/雨/雪各1800帧及团云/层云保留回归完成；雪断连后使用 `retain_swir_weather3_retry`。SWIR雨雪可见，白色团云/层云保持；MWIR雪保留P17弱暗信号，sourceSeq1200背景差−3..+1，2246像素绝对差≥2，不能描述为高亮白雪或把全部差异归为粒子。[天气差异](../logs/p18/weather_retention.json)。P11既有62项尾焰物理检查与Stage7天气静态检查通过，[物理QC](../logs/p18/tests/plume_physics/p11_plume_physics_summary.json)。详细人工观察和局限见 [visual_review.json](../logs/p18/visual_review.json)。

## 6. 性能独立结论

客户关闭/开启均同板双实例、同原始4318行、同尺寸/固定配置。预热600行后，601..4318至少60秒；首尾产品不删除。`render` ledger在do_frame和capture_task返回后写入，因此render/capture完成FPS来自同一业务周期时间戳；doFrameMs、captureTaskMs及readbackMs分别记录耗时，不伪称拥有两个独立GPU时钟。精通道最后一帧无后续NAL/EOS，按本地idle=12000ms最终flush，稳态与含最终等待分别列出。

| 开关/实例 | render完成FPS | capture任务完成FPS | output FPS | receive帧完成FPS | decode稳态FPS |
|---|---:|---:|---:|---:|---:|
'''+ '\n'.join(rateRows)+f'''

严格60FPS结论 **{perf['strictFpsResult']}**；没有把59.99四舍五入改判。精通道含末帧idle的decode FPS={on['precise']['decodeIncludingFinalIdleFlush']['fps']:.6f}，最后接收→flush={on['precise']['finalFrameIdleFlushDelayMs']:.3f}ms。

本轮另用同一Windows `std::chrono::steady_clock`，按完整业务摘要/顺序与AU/raw字节对应，测量“Windows激励write开始→Windows客户解码QImage”的闭环时延，无需相减板/Windows时钟。它包括往返链路，不是跨主机单向时间，也不是屏幕呈现。

| 实例 | 预热后p50 ms | p95 ms | max ms（不含最终idle） | 最后帧ms | 全部帧≤80ms |
|---|---:|---:|---:|---:|---|
'''+ '\n'.join(loopRows)+f'''

全部帧80ms门槛未通过（包括压缩末帧idle）。跨主机单向时间无时钟校准、屏幕无present时间戳，两者 **NOT_VERIFIED**。配置fps不是实测，GUI更新次数不是显示器逐帧呈现。关闭/开启的RSS、队列上界、转换/编码/写入p50/p95/max、读回耗时和STOP排空原日志在性能JSON。客户复制发生在queuedNs与workerNs之间，因此queueWaitMs包含复制、入队和等待，可作该次复制开销上界；没有单独的copy-only计时，不能把整个queueWait都归为复制。不同进程启动的RSS分配差异不能解释成新增分支负开销。未降低60FPS/80ms、分辨率或原始行数。

## 7. 受控故障、失败原件与清理

`paused_connected_customer_v4` 在已证明收到63个Sample后暂停自己启动的客户进程8秒。客户最大队列33484800字节（小于32MiB上限），显式failed/rejected，收到/解码/保存447帧；原链仍完成1201产品和约5秒既有STOP排空。该异常的部分客户媒体不是收齐证据。

`held_dds_history_v4` 使用实际typed HDDecoder Reader，max_samples=16且应用不调用take，持续60秒，目的是尝试形成DDS背压。没有读出历史占用计数，因此不声称内部缓存确实填满。此例不创建假视频或伪造客户接收计数。发送端超时/排空失败是否观察到：**{'OBSERVED' if heldTimeout else 'NOT_TRIGGERED'}**，发送端SDK超时隔离仍NOT_VERIFIED。原私有产品={heldOld['privateProducts']}，senderExit={held.get('senderExit')}，boardExit={held.get('boardExit')}，holdingReaderExit={held.get('holdingReaderExit')}。原始错误和排空日志如下，实际结果边界由此限定：

```text
'''+ '\n'.join(heldErrors+heldDrains)+f'''
```

保留连接reset的 `dual_swir_original_02`、`customer_overload_v2`；保留无ACK的 `retain_swir_weather3_v2`；保留脚本finally错误的 `dual_perf_on_v2`、callback1000ms后客户超时终止的 `controlled_callback_timeout_v2`、暂停时点过早的 `paused_customer_transport_v3`、误重置的 `old_reopen_graceful_v3`。执行完成字段不代表功能通过。首次HD264 v4停顿脚本因该PowerShell环境缺少Get-FileHash而未完成汇总，真实数据已形成；保留原错误，改用.NET哈希并用独立Python/ffprobe完成已有数据验证，不捏造重跑日志。

独立ffprobe共审计{len(media)}份媒体，其中{len(bad)}份历史旧VD MP4缺moov，另有{len(mismatch)}项在线解码/保存或旧索引数量不一致。每项文件见媒体审计，损坏原件不覆盖。旧MP4根因是测试finally强制终止未写trailer，编排改用现有acceptance-exit正常退出并等待；新的缺席/重开/两波段尾焰旧录像均单独补录。旧重开v3的4291在线/4318保存差异也明确保留，v4已修复。

最终环境回执：[final_environment.json](../logs/p18/final_environment.json)。无剩余板端/本地测试进程={environment['noRemainingTestProcesses']}；板端governor恢复到事前观察的ondemand={environment['governorsRestoredToObservedBaseline']}。未改变系统永久网络/启动设置；板端时钟与Windows不一致，不用于未校准时延。商业licence/密码不进入manifest。已下载审计的板端释放操作逐文件先核对SHA，记录于audit_release.jsonl；不删除P17历史证据。

## 8. 操作、源码与回滚

[完整DDS说明](HwaSimIR_DDS_EndToEnd_Guide.md)、[客户ICD](HwaSimIR_Customer_TX_DDS_ICD.md)、[真实双路启动命令](HwaSimIR_P18_DualChannel_Runbook.md)、[独立Demo源码/依赖](../DDS/HwaSimIRCustomerTxVideoReceiverDemo/README.md)。粗1001/1原生320 raw Gray8→TX/LDecode，精1001/2原生800灰度H264→TX/HD264。操作手册给出两板端shell、两客户窗口和唯一激励命令；默认客户开关false，正式隔离配置true。

关键实现：`CustomerDdsVideoPublisher::validate/start/beginRound/submit/Impl::run/endRound`；`CustomerTX::Writer::open/send/drain`；两平台 `TcpCommThread::startOutputRound/stopOutputRound/sendFrameThreadFunc/sendFramePacket`；`HwaSimIR` runtime路径覆盖、INIT容量和读回门控；`MainWindow::sendInitCommand`与 `DdsStimClient` 双身份ACK/状态屏障；`Receiver::push/run/packet/flush/saveMedia`；两sprite外形包络及P5 `bindOpaqueFixtureDepth`。具体行号以source_index为准。

回滚先确认并停止cwd属于P18候选的实例，从保留的 `/userdata/HwaSimIR` 启动P17v12；本地客户可单独Enable=false。尾焰仅回退baseline两个sprite，不能整体回退P17。未commit/push。

交付网页的浏览器自动检查因URL策略拒绝 `file://` 而未执行，没有通过改地址/换浏览器绕过。页面按钮及浏览器内播放操作为NOT_VERIFIED；本地链接另行检查、媒体独立解码及真实客户GUI捕获均已完成。[浏览器检查边界](../logs/p18/browser_review.json)、[实际链接检查](../logs/p18/delivery_link_check.json)。

功能、视觉、性能、实际客户互通分别判断：已完成的功能证据保留；视觉属于限定场景的工程改善；性能未通过；真实客户、标定、屏幕时延仍未验证，不签发整个P18 PASS。
'''
(ROOT/'docs/HwaSimIR_P18_Closeout.md').write_text(text,encoding='utf-8')
(B/'requirements_results.json').write_text(json.dumps([dict(requirement=n,title=t,result=r) for n,t,r in scope],ensure_ascii=False,indent=2),encoding='utf-8')
print('Updated closeout from final evidence')
