# Codex Phase 1 Prompt：同步 60 FPS 性能与延时基线

你在仓库 `D:\HwaSimIR` 中工作。目标不是重构红外物理，不改变画面效果；只做同步 60 FPS、延时、读回/JPEG/TCP 的性能基线与明显低风险优化。最终要能编译，并给出运行日志关键词。

## 背景与判断依据

当前 HwaSim_IR 在开始接收实时成像 UDP 数据后帧率从百帧级骤降到十几/二十帧。`SetRenderMode(false,0)` 只能取消限帧，不能减少每帧工作量。真正嫌疑是：每帧 IR 更新、场景更新、频繁 `std::cout`、大量 `set_shader_input`、GPU→CPU readback、OpenCV flip/resize/JPEG、TCP 发送、VideoDisplay 解码显示。同步模式要求：一组 UDP 实时数据对应一帧渲染和一帧输出；不能用 TCP/JPEG 降频规避。`trackingInit.videoFps` 是目标帧率，`videoFps=25` 可锁 25，`videoFps=60` 必须有能力达到 60。

## 硬目标

1. 不改变红外画面物理逻辑。
2. 增加性能统计和延时统计。
3. 同步模式必须保留“一包 UDP -> 一帧渲染 -> 一帧 TCP 输出”的检查。
4. 所有高频日志限频，Release 默认不刷屏。
5. 启动时打印 Panda3D/GPU 渲染后端信息，确认不是软件渲染。
6. HwaSim_IR_VideoDisplay 增加接收/显示 FPS 和延时日志。
7. 编译通过。

## 具体实施

### 1. 新增 IRPerfStats

新增轻量模块，例如 `IR/IRPerfStats.h/.cpp`，只用 C++14 标准库。统计：

- udpFps
- renderFps
- outputFps
- videoFpsTarget
- sceneUpdateMs
- irUpdateMs
- readbackMs
- jpegMs
- tcpSendMs
- tcpQueueDepth
- latencyMsAvg / latencyMsMax
- syncOverrunCount

每 120 帧或每 2 秒打印一次 `[Perf]` 日志。

### 2. videoFps 驱动渲染目标

在处理初始化命令时读取 `cmd.trackingInit.videoFps`：

- 若 `videoFps > 0`，保存为 `m_targetVideoFps`。
- 同步模式下，该值作为期望 UDP 输入频率和输出帧率。
- 异步锁帧模式下，默认锁到该值；如果有 runtime override，可保留 override。

不要把 `videoFps` 只当录像 FPS。

### 3. 同步模式一包一帧诊断

增加本地 `uint64_t frameSeq/udpSeq/outputSeq`，不改变 CommonData 结构也可以，把序号写入 annotationJson 或日志。

同步模式要求：

- 每消费一个 DisplayC2cObjTrackingData，计数 `udpConsumed++`。
- 每完成一帧 readback/发送，计数 `outputFrames++`。
- 打印 `[SyncFrame] udpSeq=... outputSeq=... videoFpsTarget=... frameMs=... latencyMs=... overrun=0/1`。
- 不要在同步模式做 latest-only 丢包；如果来不及，记录 queue/overrun。

### 4. 高频日志限频

把每帧 `std::cout` 改成：前 3 次 + 每 120 帧 + 状态变化时打印。重点搜索并处理：

- `接收UDP数据`
- `Stage4 ThermalHotspot`
- `Stage4 BrightSpot`
- `Stage4 Uniform`
- `Stage5 Radiance`
- `Stage5C VisualCalib`
- `Stage7 Weather`
- `CameraControl`
- `TargetMapping`
- `Stage6 Capture`

保留错误日志和状态变化日志。

### 5. 读回/JPEG/TCP 分段计时

在 `capture_task` 统计：

- `has_ram_image/get_ram_image/get_ram_image_as` 耗时
- resize 耗时
- updateFrame 拷贝耗时

在 `TcpCommThread::sendFrameThreadFunc` 统计：

- flip 耗时
- resize 耗时
- JPEG encode 耗时
- TCP send 耗时
- 当前队列/是否覆盖旧帧

第一阶段不要求改协议，但如果发现每帧强制 resize 到 800x800，先改成只在尺寸不一致时做，并在 Perf 里报告。不要同步模式降帧。

### 6. 启动时打印 GPU/GSG 信息

在窗口/GSG 创建后打印：

- Panda3D graphics pipe 类型
- driver vendor
- driver renderer
- driver version
- OpenGL/OpenGL ES version
- 如果 renderer 包含 `llvmpipe`、`softpipe`、`software`，打印 `[GPU][WARN] software renderer detected`。

### 7. HwaSim_IR_VideoDisplay 诊断

在 VideoDisplay 侧统计：

- receiveFps
- displayFps
- JPEG decode / QImage load 耗时（按当前实现能取到多少算多少）
- annotationJson 中若有 `udpReceiveTime` / `tcpSendTime` / `frameSeq`，打印端到端或分段延时

每 120 帧打印 `[VideoPerf]`。

## 验收输出

完成后给出：

1. 修改文件列表。
2. Windows Release 编译命令和结果。
3. 运行方式。
4. 必须能看到这些日志关键词：
   - `[GPU]`
   - `[Perf]`
   - `[SyncFrame]`
   - `[TcpPerf]`
   - `[VideoPerf]`
5. 用 `videoFps=25` 和 `videoFps=60` 分别测试同步模式，报告：
   - udpFps
   - renderFps
   - outputFps
   - avg latency
   - readbackMs
   - jpegMs
   - tcpSendMs
   - syncOverrunCount

不要做 Stage5 正式化、MODTRAN path、MTF、AGC、HeatSource 物理重构；这些留到下一阶段。
