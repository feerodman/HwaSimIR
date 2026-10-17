# P19 H264 收尾、参数与兼容性

正常结束由发送端追加标准 Annex-B `00 00 00 01 0A`，接收端跨 Sample 识别后显式排空 parser、decoder，再保存 MP4。原先约12秒来自离线接收脚本的 idle退出后flush，不是编码帧必须等12秒。默认连续接收已设为 idle=0，普通长暂停不清参考状态。

## 收尾实测与生命周期修复

首轮板端120帧结束码到末帧1.474ms，但完整STOP还包含旧分支固定5秒等待。已取消保留旧Writer时不必要的sleep，并把客户排空放在旧链/本地录像排空之前。随后双路120帧STOP约33ms、结束码到末帧0.8122ms、MP4完成127.5551ms。

候选码流测试又暴露：客户Writer立即UnPub时，最后5字节可能没有到接收器，尽管write和ACK返回成功。最终正常STOP保留一个健康Writer，下次beginRound安全重建；失败时仅释放客户实体，不动共享participant。SDK batch开启时才调用支持的flush；未开启batch调用flush实测返回4，不能把这个不适用调用当正常排空。`runs/mpp_candidate_retained_writer`及`dual120_two_rounds_final`保留最终测试。

`tests/end1_exact1400_crossend`：一个真实AU加合法Annex-B trailing_zero_8bits达到12600字节，最后标准结束NAL强制跨5个Sample，计数仍只有1帧。`tests/end30_two_rounds_crossend`：同Reader两个30帧回合、NAL中途5秒暂停、字节一致、60帧。另有1/30/120帧及3秒暂停测试。没有短包判帧、零长度哨兵、重复末帧或新增客户Topic。

最终独立汇总见 `logs/p19/h264/final_timing_summary.json`：`dual120_two_rounds_final` 共240帧、2次结束，最后一次结束码到parser输出0.0885ms、decoder输出0.8948ms、MP4完成514.0636ms；30帧候选单回合分别0.0937/1.16/142.7243ms。双回合MP4完成并非固定130ms，文件封装耗时与回合/文件情况有关。此处均为接收端同钟测量，不是从UI STOP开始或屏幕呈现的端到端时延。

各端记录自身单调时钟：输入/编码/最后write、最后Sample收到/parser末AU/decoder末图/MP4完成分列。板卡和Windows墙钟当前不同，未校准的跨机时间戳不相减；接收端结束码到解码/封装可直接同钟比较。没有结束码的异常中断报告incomplete，不把强制flush补偿出来的图像计为正常末帧。

## 参考附件独立复核

| 文件 | 实际参数与帧数 |
|---|---|
| DDS.264 | 800×800，Constrained Baseline，L3.1，8-bit YUV420，refs1，CAVLC；8767帧=I36/P8731/B0；最大NAL77112字节 |
| UDP.264 | 640×512，High，L3.2，8-bit YUV420，refs4，CABAC；9341帧=I59/P2341/B6941；最大NAL2807字节 |

报告位于 `logs/p19/h264/reference_DDS`、`reference_UDP`。文件不包含网络采集时间，demux/VUI速率不等于实际发送FPS；文件名不代表硬解成功/失败。原RAR未随本ZIP提供，但提取的原始码流和文本已分析，未运行旧VLC或未知安装器。参考编码文本互相覆盖profile/preset/CRF/B帧/zerolatency且未检查返回值，没有照搬。

## 生效配置

集中于 `[TcpOutput]`，由HwaSimIR加载并传到两平台TcpCommThread及客户独立Gray8编码器。RGB仅在同配置下共享**私有SEI注入前**的clean AU。

| INI键 | 默认/支持边界 |
|---|---|
| H264Encoder | auto；板端MPP，Windows FFmpeg；不悄悄回JPEG |
| H264BitrateKbps / H264GopFrames | 4000 / 30 |
| H264LowLatency / H264ForceKeyFrameOnStart | true / true；实时B帧维持0 |
| H264Profile | baseline；可请求main/high，实际SPS为最终证据 |
| H264Level | auto；MPP保留至少4.0默认，FFmpeg按容量选；支持表内显式3.0..5.2 |
| H264RateControl | backend_default或cbr；MPP默认CBR，FFmpeg显式cbr设置max/min/VBV |
| H264ReferenceFrames / H264RepeatHeaders | 仅1 / true可用，其它明确拒绝 |
| H264Aud | false；true在AU前添加标准AUD |
| H264StandardEnd | true；关闭后客户必须获得外部真实结束通知再finishStream，不能靠沉默判定 |

MPP每项set/control返回值检查并读回宽高、profile、level、CABAC、8×8、fps、GOP、码率。FFmpeg检查选项设置、avcodec_open2和未消费选项。没有通过改SPS字节伪装level。

生产默认板端实测：800×800、Constrained Baseline L4.0、refs1、CAVLC、B0、I4/P116、重复SPS/PPS；候选板端实测L3.2、30个AUD、I1/P29；生产FFmpeg编码类独立探针实际输出120帧，Baseline L3.2、refs1、B0、每帧AUD。VUI num_units_in_tick=1,time_scale=120对应progressive60；不能把ffprobe裸流显示的120/1当120FPS网络实测。

800×800@60=2500宏块/帧、150000宏块/秒，L3.1的108000不足；L3.2的216000可容纳。1024×1024@60=245760，不得继续3.2。代码检查MaxFS、MaxMBPS、DPB、宏块边长和profile相关码率（MPP含17/16峰值）；实际短流也独立解析SPS/PPS。**完整HRD/CPB模型一致性没有由当前MPP接口读回证明**：实际SPS没有NAL/VCL HRD，不能据此宣称任意长流或未知硬件的完整level/缓冲兼容性已通过。

## 交给客户的候选

[customer_hw_compat/HwaSimIRRuntime.ini](../ConfigExamples/P19/customer_hw_compat/HwaSimIRRuntime.ini) 为800×800@60 Baseline/CAVLC、L3.2、CBR4Mbps、refs1、B0、GOP30、重复参数集、AUD、标准结束NAL候选。clean样本及对应解析报告在 `logs/p19/h264/`，最简软解：

```text
ffmpeg -v error -f h264 -i clean.h264 -f null -
```

需要客户确认芯片/SDK、profile/level/尺寸/FPS、8-bit420、参考/重排上限、Annex-B或avcC、连续字节或完整AU、输入缓冲/对齐、参数集单独喂入、结束NAL与错误码。客户H264软件接收为USER_REPORTED；本轮MPP/FFmpeg软件解码亲测；客户硬解为 **NOT_VERIFIED_CUSTOMER_HARDWARE**。

标准依据：[H.264 2024](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-H.264-202408-I%21%21PDF-E&lang=s&type=items)的7.3.2.5/6空RBSP结束NAL语法；[FFmpeg H264 parser](https://ffmpeg.org/doxygen/6.1/h264__parser_8c_source.html)。实际部署仍以本轮源码构建和码流实测为证据。
