# H264 参考附件：独立解析结果与配置边界

本文件由本次回复的分析过程生成，不是客户硬件能力声明，也不是 P19 已执行报告。

## 1. 实际读取的附件

原附件：`H264参考文件.rar`。实际读取：`264/DDS.264`、`264/UDP.264`、`264/analyze.py`、`新建文本文档.txt`。
归档还包含 `264/vlc-2.0.3-win32(1).exe`；未解压、安装或执行该程序。本次没有运行附件中的 analyze.py；码流解析使用运行环境内的 ffprobe / ffmpeg，并独立统计 NAL 和逐帧类型。

## 2. 文件级实测，不等于实时网络性能

| 项目 | DDS.264 | UDP.264 |
|---|---:|---:|
| 文件字节数 | 26047574 | 971899 |
| 可解码尺寸 | 800×800 | 640×512 |
| ffprobe profile | Constrained Baseline | High |
| SPS profile_idc | 66 | 100 |
| SPS level_idc | 31（3.1） | 32（3.2） |
| 像素格式 | yuv420p，8-bit | yuv420p，8-bit |
| SPS max_num_ref_frames | 1 | 4 |
| PPS entropy_coding_mode_flag | 0（CAVLC） | 1（CABAC） |
| 实际 I / P / B 帧数 | 36 / 8731 / 0 | 59 / 2341 / 6941 |
| 总解码帧数 | 8767 | 9341 |
| SPS max_num_reorder_frames | 0 | 2 |
| SPS / PPS / IDR NAL 数 | 36 / 36 / 36 | 59 / 59 / 59 |
| AUD / end_sequence / end_stream NAL 数 | 均为 0 | 均为 0 |
| 最大 IDR NAL 有效长度 | 77112 | 2807 |
| 全量帧类型解析 stderr | 空 | 空 |

附件原始文件并不带每帧采集时间戳。本次 ffprobe 给出的 r_frame_rate/avg_frame_rate 不应当作实际网络输出帧率。DDS.264 的 VUI 为 num_units_in_tick=1、time_scale=50、fixed_frame_rate_flag=1；UDP.264 对应 1/200/0。不能因文件名 DDS/UDP 推断它们分别是客户成功或失败的硬解案例；原附件未提供这种明确说明。

两个样本的尺寸、profile、B帧和参考帧不同，不能直接用文件大小比较 DDS/UDP 传输效率，不能用 NAL 超过 1400 字节断言传输错误：1400 是应用 Sample 的有效载荷分片上限，接收端需要跨 Sample 重建连续码流。

## 3. 附带参考代码的实际内容

`新建文本文档.txt` 是 FFmpeg 编码/MP4 初始化片段，不是硬件解码器规格表。

它设置 YUV420P、gop_size=250、max_b_frames=1，并设置 profile=high、cabac=1、refs=3、bframes=3、rc_lookahead=40 等选项；后面又把 preset 从 medium 改为 ultrafast、crf 从 23 改为 20，并设置 tune=zerolatency。它没有检查每个 av_opt_set 的返回值，也没有给出最终打开的具体编码器及最终实际码流参数。

因此应逐项核对有效选项和实际码流，不应原样照搬，也不能把注释“低延迟”当成没有 B 帧/前视缓冲的证明。

## 4. 当前仓库源码的对照观察

检查基线：`feerodman/HwaSimIR` main 在本次读取时的 HEAD 为 `fded9de2f0ff652aab3afdfee3d350910def5baf`。

- `HwaSim_IR/HwaSim_IR/Video/H264MppEncoder.cpp` 的 configure 请求 NV12（MPP_FMT_YUV420SP）、CBR、目标码率来自 config、上下限 15/16 与 17/16、fps 来自 config、GOP 来自 config、profile=66、level=40、cabac_en=0、trans8x8=0，并设置每个 IDR 带 SPS/PPS。
- `HwaSim_IR/HwaSim_IR/Video/H264FfmpegEncoder.cpp` 请求 YUV420P、baseline、max_b_frames=0、ultrafast、按 lowLatency 启用 zerolatency、Annex-B、repeat-headers 和 scenecut=0；没有像 MPP 一样固定 level=40。
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini` 的 `[TcpOutput]` 已有 H264Encoder=auto、H264BitrateKbps=4000、H264GopFrames=30、H264LowLatency=true、H264FallbackToJpeg=false、H264ForceKeyFrameOnStart=true。
- 客户 Gray8 分支可能使用独立编码器，RGB24 配置相同时可以共享注入私有 SEI 前的 clean AU。新增编码配置必须传到两个路径，不能只改旧输出。

上述是源码请求值，不代表已经读取客户现场实际运行 ELF 输出的 SPS/PPS。现场配置文件还可能通过 HWASIMIR_RUNTIME_CONFIG 和环境覆盖切换。

## 5. 硬解兼容建议与约束

暂不照搬 High + B帧 + 长 GOP 配置。保留当前低延迟默认，新增可选硬解配置和有效值日志，交付实际 clean H.264 短流供客户硬解。

对 800×800@60，按逐行帧宏块计算：ceil(800/16)×ceil(800/16)×60=150000 宏块/秒。Level 3.1 的 MaxMBPS=108000，不满足；Level 3.2 的 MaxMBPS=216000，在其它限制（MaxFS、DPB、码率/CPB等）也满足时可作为候选。当前源码的 Level 4.0 请求不因这个吞吐量而错误，但尚不知道客户硬件支持到哪个 level。

对 1024×1024@60 为 245760 宏块/秒；不能继续固定 3.2，应完整检查至少 Level 4.0 及其它限制。不得仅修改 SPS 的 level_idc 来掩盖真正的编码负载约束。

NV12 与 YUV420P 是不同的内存平面布局，不能把它们的名称当作码流 profile；应检查实际 bit_depth/chroma_format、颜色范围、帧结构及硬件输入接口。

## 6. 可复现命令

```bash
ffprobe -v error -count_frames -select_streams v:0 \
  -show_entries stream=codec_name,profile,level,width,height,pix_fmt,r_frame_rate,avg_frame_rate,has_b_frames,refs,color_range,color_space,color_transfer,color_primaries,field_order,nb_read_frames \
  -of json DDS.264

ffmpeg -hide_banner -v verbose -i DDS.264 -map 0:v:0 -c:v copy \
  -bsf:v trace_headers -frames:v 3 -f null -

ffprobe -v error -select_streams v:0 -show_entries frame=pict_type -of json DDS.264
```

原始输出在本包 analysis 目录，`refs` 的论断优先使用 trace_headers 的 max_num_ref_frames，而不是单独依赖 ffprobe 的摘要属性。

## 7. 外部依据（公开原始资料）

- FFmpeg H.264 level 表： https://www.ffmpeg.org/doxygen/6.0/h264__levels_8c.html
- FFmpeg 发送/接收和 draining 语义： https://ffmpeg.org/doxygen/3.3/group__lavc__encdec.html
- FFmpeg H.264 parser： https://www.ffmpeg.org/doxygen/6.1/h264__parser_8c_source.html
- Rockchip MPP 配置说明： https://github.com/rockchip-linux/mpp/blob/develop/doc/Rockchip_Developer_Guide_MPP_EN.md

公开资料用于解释通用约束；P19 必须以工程实际安装的 FFmpeg/ZRDDS/MPP 版本和实测输出为准，不能因公开版本有某选项就假设板端旧 SDK 也支持。
