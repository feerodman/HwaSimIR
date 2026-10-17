# 最小 raw 接收示例（P19）

先运行这个计数程序，确认收到像素，再接入显示、录像或算法。运行时只依赖匹配编译器/架构的 ZRDDS SDK 和原始 generated 类型；没有 Qt、FFmpeg、CommonData、Status、Meta、标注或私有 SEI 依赖。

默认 Domain **1**、`TX/LDecode`、全局类型 `LDDecoder`、320×320 Gray8。一个 DDS Sample 就是一帧：`frameId` 是 generated 的 32 位 `DDS_Long`，`stream.length()` 应为 **102400**；640000 是容量上限，不是固定实际长度。

## 编译和运行

从本目录构建（CMake 3.18+；Windows 选择与 SDK 相同的 MinGW/MSVC 工具链）：

```text
cmake -S . -B build -DZRDDS_ROOT=<匹配SDK根目录>
cmake --build build --config Release
CustomerRawReceiverMinimal --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml
```

Windows 将匹配 SDK 的 `ZRDDSCpp.dll` 放在程序旁或 PATH 中，提供自己的有效 SDK 许可。Linux 设置 `LD_LIBRARY_PATH=<SDK>/lib`。示例包不分发许可证。已有 Qt MinGW 构建环境也可以 `qmake CustomerRawReceiverMinimal.pro`、`mingw32-make`，此程序不链接 Qt。

有限时长测试：`--seconds 15`。`--probe-bytes 640000` 用于 payload probe；它不会把非默认长度伪装成 320×320 图像。`--slow-ms` 仅供明确的慢消费诊断，默认 0。

## 顺着 main.cpp 阅读

1. `DDSIF::Init` 一次，`CreateDP` 创建指定域 Participant。
2. `SubTopic` 使用 `LDDecoderTypeSupport` 注册/订阅原始类型，类型名无命名空间前缀、无 key；禁止裸 struct memcpy 替代 generated 序列化。
3. SDK `SimpleDataReaderListener` 已经 take、过滤 `valid_data`，然后调用 `on_process_sample`，最后 return_loan。回调里不要再次 take 或归还同一 loan。
4. 回调验证长度和帧号，把 sequence 内容复制到自己拥有的 vector；不保存 SDK 指针。工作队列最多 16 帧，满队列明确计数并返回失败结果，不无限积压。
5. 工作线程消费副本。`0x00/0x80/0xFF` 都是普通像素，不能 strlen。Gray8 按从上到下、从左到右排列，无 stride padding。RGB24 是 R,G,B 三字节，320×320 实际 307200 字节；本最小程序仅按显式长度计数。
6. 退出时先 `UnSubTopic` 停回调，再排空队列/join，最后 `Finalize` 一次。

## 现场排查顺序

核对实际 Domain、Topic、类型、partition、有效 QoS，再核对首帧 `length`、计数/缺口/重复、真实 SDK 日志。XML 声明不等于实际值：当前 SDK 的 TCP Reader 实测 `get_qos reliability=1`，UDP 为 2；头文件中 1=BEST_EFFORT、2=RELIABLE。这是本版本 SDK 的实测差异，不是客户问题定论。不要把内部 transport 码 7 当 errno 或 DDS ReturnCode。

计数成功后再增加显示；显示成功后再增加磁盘保存。DDS 回调中不要解码、显示、写大文件。完整 Demo 及恢复步骤见同包说明和 P19 排查文档。
