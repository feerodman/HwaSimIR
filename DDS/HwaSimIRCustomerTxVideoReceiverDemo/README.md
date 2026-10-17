# 客户视频接收 Demo（P19）

只订阅所选的一个客户 Topic；启动两个进程即可同时接两路。不订阅 HwaSimIR 的控制、Status、Meta、Annotation，不解析私有帧身份。

| Topic | 原始类型 | 内容 |
|---|---|---|
| TX/LDecode | LDDecoder | 粗路 raw，一帧一个 sequence Sample |
| TX/HDecode | HDDecoder | 精路 raw，一帧一个 sequence Sample |
| TX/LD264 | LD264Stream | 粗路连续 Annex-B，每 Sample 1..1400 有效字节 |
| TX/HD264 | HD264Stream | 精路连续 Annex-B，每 Sample 1..1400 有效字节 |

`ReceiverOptions.h` 负责配置校验；`CustomerDdsSubscription.h` 在 DDS 回调中复制有效数据；`CustomerReceiver.h` 的工作线程处理 raw 或增量 H264；`main.cpp` 管理 DDS、Qt 显示和退出次序。raw 用 `QImage` 包装副本后立即深拷贝，显示线程不会引用已经归还的 DDS 内存。

## 构建

设置 `ZRDDS_MINGW730_ROOT` 指向匹配 MinGW7.3 的 SDK，`FFMPEG_ROOT` 指向提供 libavcodec/libavutil/libswscale 头文件、导入库和 DLL 的 FFmpeg。用 Qt5 的 qmake 构建本 `.pro`，运行 `mingw32-make`、`windeployqt`。复制匹配的 ZRDDS/FFmpeg DLL、QoS，提供自己的许可。不要混用 VS 与 MinGW SDK，也不需要安装附件里的旧 VLC。

## Domain1 双路命令

```text
HwaSimIRCustomerTxVideoReceiverDemo --domain 1 --topic TX/LDecode --codec raw --pixel-format gray8 --width 320 --height 320 --fps 60 --qos ZRDDS_PROTOCOL_QOS.xml --output coarse --display 1
HwaSimIRCustomerTxVideoReceiverDemo --domain 1 --topic TX/HD264 --codec h264 --pixel-format gray8 --width 800 --height 800 --fps 60 --qos ZRDDS_PROTOCOL_QOS.xml --output precise --display 1 --ffmpeg ffmpeg
```

`--count-only 1 --display 0` 关闭存图/录像；H264 仍解析解码以计数真实图像。逐包 SHA 审计默认关闭，诊断才使用 `--audit 1`。队列上限默认 32 MiB 和 8192 Samples，超限明确失败。不同运行使用新的 output 目录。

Gray8 一字节/像素；RGB24 是 **RGB**，不是旧链的 BGR，紧密排列、左上原点、从上到下。800×800 Gray8=640000；320×320 RGB24=307200；800×800 RGB24 raw=1920000，必须拒绝。H264 灰度是在发送端将 Gray8 转为中性 RGB，再走已有 YUV420/NV12 范围转换，接收像素允许有损量化误差。

## 暂停、收尾与恢复

H264 Sample 不是帧；DataLength<1400 不是边界。连续字节进入 parser，完整 AU 再送 decoder。`AnnexBEndScanner` 跨任意 Sample 扫描标准 NAL 10/11；真实正常结束时显式排空 parser、decoder，完成本回合 MP4，再重置以接下一回合新 SPS/PPS+IDR。结束 NAL 不增加图像计数。

默认 `--idle-ms 0 --timeout-ms 0`，长暂停不退出、不清参考帧。有限测试可用 `--exit-after-ends 1`。没有标准结束码、下一帧或外部真实结束通知时，仅靠沉默不能判断断网还是结束；强制关闭未结束流记录 `unterminatedOrIncomplete=true`，不强行 flush 截断尾 NAL 并把补偿图像算成功。

正常结束保存 `round_N.h264`、`round_N.mp4`；完整接收字节保留 `received.h264`。raw 在正常关闭时封装 MP4。MP4 保存耗时与尾帧解码耗时分列；它不是每帧网络时延。晚加入从新参数集和 IDR 恢复，之前图像不补造。断流后如已发生字节缺口，应重新打开接收器再启动新回合；可靠传输中的普通停顿不重置。

硬件接入点是 `packet()` 得到的完整 Annex-B AU（包含必要 SPS/PPS），具体硬件的缓冲、参数集和结束语义须由客户 SDK 确认。本包没有虚构的硬解适配器。
