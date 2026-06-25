# HwaSimIR HeadlessOffscreen 实施方案

> 版本：v6，H0/H1/H2/H3 实施记录 + H4 RK3588 direct_final 性能收口版
> 更新时间：2026-06-25  
> 目标平台：Windows 调试默认 VisibleWindow；Linux/RK3588 可通过配置切换 HeadlessOffscreen  
> 涉及工程：本阶段只修改 `HwaSim_IR`；不修改 `DataDrivenTestQT`、`HwaSim_IR_VideoDisplay`、TCP 协议或编码逻辑  
> 默认行为：生产默认仍为 VisibleWindow + JPEG/fallback，不新增真实 H.264 编码

---

## 0. 背景与边界

Phase7A 已完成 H.264 相关配置项与明确 fallback 机制，但生产默认仍走 JPEG。HeadlessOffscreen 与 H.264/JPEG 的职责边界如下：

```text
HeadlessOffscreen：Panda3D 渲染宿主、finalSensorTex、GPU 到 CPU 的读回源。
H.264/JPEG：TcpCommThread 收到 CPU 像素帧后的 payload 编码方式。
```

只要 HeadlessOffscreen 继续向 `TcpCommThread` 提供同样的 `frame.pixels / width / height / telemetry / annotationRecord`，就不应影响 Phase7A。当前阶段暂停真实 H.264 编码，保留 JPEG fallback，优先解决 RK3588 无显示器、开机自启、仅网口通信运行的问题。

明确非目标：

1. 不实现真实 H.264、FFmpeg/libx264、MediaFoundation 或 RK MPP 编码。
2. 不实现真正多通道渲染。
3. 不修改 TCP 协议和编码逻辑。
4. 不修改 `DataDrivenTestQT`。
5. 不修改 `HwaSim_IR_VideoDisplay`。
6. 不改变 Stage5/Stage6 物理效果参数。
7. 不改变默认 VisibleWindow + JPEG/fallback 行为。

---

## 1. 当前耦合点

旧启动路径在 `open_framework()` 后立即 `open_window()`，随后直接访问 `m_pMainWindow->get_graphics_window()`。网络初始化、模型路径初始化、IR shader、Stage6/Stage7 和红外仿真初始化都包在 `if (m_pMainWindow)` 内，`run()` 也要求 `m_pMainWindow` 存在。

这会导致 RK3588 无显示器场景下，窗口创建失败后无法启动 UDP/TCP 线程，也无法进入主循环；更糟糕时会空指针崩溃。

---

## 2. 目标架构

### 2.1 渲染宿主模式

```text
VisibleWindow
  Windows / RK3588 接显示器调试路径。
  创建 Panda3D WindowFramework / GraphicsWindow。
  Stage6 final sensor 输出显示到窗口，并作为 TCP 读回源。

HeadlessOffscreen
  RK3588 无显示器部署目标路径。
  H1 阶段解耦启动、配置、网络线程、CaptureTask 和空指针防护。
  H2 阶段不创建可见窗口，创建真实 offscreen GraphicsBuffer/finalSensorTex。
  TCP 仍从 CPU RGB frame 输入读取，不感知窗口或离屏来源差异。
```

### 2.2 配置策略

新增 `[RenderBackend]`：

```ini
[RenderBackend]
PresentationMode=VisibleWindow
WindowPreview=true
EnableFrameRateMeter=true
HeadlessWidth=800
HeadlessHeight=800
```

环境变量优先级仍遵循现有 `IRRuntimeConfig`：

```text
RenderPresentationMode
RenderWindowPreview
RenderEnableFrameRateMeter
RenderHeadlessWidth
RenderHeadlessHeight
```

Windows 默认 `VisibleWindow`。Linux/RK3588 使用同一代码库，通过配置或 systemd 环境变量切到 `HeadlessOffscreen`。

---

## 3. 阶段计划

### H0：文档记录与安全边界

目标：记录 Phase7A 后 H.264 与 HeadlessOffscreen 的边界，明确本阶段暂停真实 H.264，记录双通道需求但不实现。

验收：

```text
文档包含版本、目标、阶段、验收、修改记录。
默认行为仍为 VisibleWindow + JPEG/fallback。
```

### H1：渲染后端配置与启动解耦

目标：新增 RenderBackend 配置和启动分支，不实现真实 headless final buffer，但构造函数、run、网络线程、配置读取不再强绑定 `m_pMainWindow`。

实现要点：

1. 新增 `RenderPresentationMode`：`VisibleWindow` / `HeadlessOffscreen`。
2. 新增 `LoadRenderBackendConfig()`、`IsVisibleWindowMode()`、`IsHeadlessOffscreenMode()`、`IsRenderBackendReady()`、`LogRenderBackendConfig()`。
3. `VisibleWindow` 模式保持旧窗口行为；窗口创建失败打印 `[Fatal][RenderBackend]` 并安全返回。
4. `HeadlessOffscreen` 模式 H1 不创建窗口，只建立基础状态并打印 `headless_final_pipeline_not_ready`。
5. `InitVisibleWindowUi()` 只负责键盘、背景、按键和 FrameRateMeter。
6. `InitCommonCaptureTask()` 负责 `Stage6FinalSensorTex` 和 `CaptureTask`；headless 下不访问 `m_pGraphicsWindow`。
7. `run()` 改为依赖 `m_pFramework && IsRenderBackendReady()`。
8. `SetupStage6FinalPipeline()` 在 headless 下只打印 TODO 并返回。
9. `LogStage6FinalPipeline()` 增加 `renderMode`、`windowPreview`、`tcpSource`、`finalSensorTex` 字段。

验收：

```text
Windows Release x64 编译通过。
HwaSim_IR Release aarch64 交叉编译通过。
默认配置仍为 PresentationMode=VisibleWindow；TcpOutput/JPEG/H.264 fallback 配置不变。
默认 VisibleWindow 日志包含 [RenderBackend] mode=VisibleWindow windowCreated=1 renderBackendReady=1。
open_window 失败时打印 [Fatal][RenderBackend]，不得空指针崩溃。
RenderPresentationMode=HeadlessOffscreen 时不得访问空窗口指针；允许打印 headless_final_pipeline_not_ready。
```

### H2：真正 HeadlessOffscreen 离屏输出

目标：不创建可见窗口，使用 Panda3D offscreen output 作为渲染宿主，建立 raw scene -> final sensor postprocess -> `Stage6FinalSensorTex` 的 RAM copy 链路。

实现要点：

1. Headless 下创建 `HeadlessSensorCamera`，复用 `m_cameraNode / m_camera / m_cameraLens`，FOV 和 near/far 保持 VisibleWindow 默认值。
2. 平台资源路径映射仍初始化，模型加载改为 Panda3D `Loader::load_sync()`，不再依赖 `m_pMainWindow->load_model()`。
3. Headless `SetupStage6FinalPipeline()` 创建 `Stage6RawSceneBuffer` 与 `Stage6FinalSensorBuffer`，并将 `Stage6FinalSensorTex` 以 `RTM_copy_ram` 绑定到 final sensor buffer。
4. `m_stage6PresentationOutput` 抽象 final presentation 输出：VisibleWindow 指向窗口，HeadlessOffscreen 指向 final sensor buffer。
5. Annotation overlay display region 挂到 `m_stage6PresentationOutput`，Headless 下进入 `Stage6FinalSensorTex`。
6. Panda3D offscreen texture 可能存在 power-of-two padding；CaptureTask 在 Headless 下按 `m_stage6FinalWidth/Height` 裁剪有效 CPU RGB frame 后再交给 `TcpCommThread`。
7. 本阶段仍不修改 TCP 协议、不修改编码逻辑、不实现真实 H.264、不实现双通道。

验收：

```text
Windows Release x64 编译通过。
默认 VisibleWindow 日志包含 [RenderBackend] mode=VisibleWindow windowCreated=1 renderBackendReady=1。
默认 VisibleWindow 日志包含 [Stage6 FinalPipeline] renderMode=VisibleWindow tcpSource=final_sensor。
RenderPresentationMode=HeadlessOffscreen 时不创建窗口，日志包含 windowCreated=0 renderBackendReady=1。
HeadlessOffscreen 日志包含 headless_final_pipeline_ready=1、rawBufferReady=1、finalBufferReady=1、finalSensorTex=Stage6FinalSensorTex。
HeadlessOffscreen 运行日志不再出现 headless_final_pipeline_not_ready。
带本地 UDP 激励短跑时，Stage6 Capture 输出 tcpWidth=800 tcpHeight=800 source=final_sensor channels=RGB8。
TcpOutputConfig / Codec 仍保持 Phase7A JPEG/fallback 路径；不新增真实 H.264。
```


### H3：HeadlessOffscreen 性能收口与诊断

目标：在不修改 TCP 协议、不继续推进真实 H.264、不改变默认 VisibleWindow 行为的前提下，解决 RK3588 HeadlessOffscreen 下 `renderMs≈18–21ms`、`outputFps≈39–44`、队列长期满的问题；同时解释并修正 Windows HeadlessOffscreen 黑图或仅有 annotation overlay 的问题。

#### H3.1 按实际启用模块动态组装渲染链路

当前 H2 HeadlessOffscreen 固定走：

```text
3D scene -> Stage6RawSceneBuffer/Stage6RawSceneTex
         -> Stage6FinalSensorBuffer/Stage6FinalSensorTex
         -> annotation overlay
         -> RTM_copy_ram
         -> TCP/JPEG
```

即使 `EnableMTFBlur=0`、`EnableDetectorNoise=0`、`EnableAGC=0`、天气屏幕叠加不生效，仍然存在 raw pass + final pass + final card 采样。H3 需要新增 Render Pass Planner：

```text
无图像域后处理：
  3D scene -> Stage6FinalSensorTex
  跳过 Stage6RawSceneBuffer 和 Stage6_FinalSensor_Card

有图像域后处理：
  3D scene -> Stage6RawSceneTex
  rawSceneTex -> final shader -> Stage6FinalSensorTex
```

判断为 no-op 的条件：

1. `EnableMTFBlur=0` 或 MTF 不生效。
2. `EnableDetectorNoise=0` 或 DetectorNoise 不生效。
3. `EnableAGC=0` 或 AGC 不生效。
4. 天气屏幕叠加不生效，例如 `Stage7 PrecipitationOverlay active=0`。
5. `WhiteHot=true`，`DisplayGain=1`，`DisplayOffset=0`，`noiseEnable=0`。
6. 没有其他 final_display 后处理需要保留。

新增配置建议：

```ini
[RenderBackend]
HeadlessFastDirectFinal=true
HeadlessImageProbe=false
```

对应环境变量：

```text
RenderHeadlessFastDirectFinal
RenderHeadlessImageProbe
```

验收日志：

```text
[Stage6 FinalPipeline] renderMode=HeadlessOffscreen headlessFastDirectFinal=1 finalPostprocessBypass=1 rawBufferReady=0 finalBufferReady=1 tcpSource=final_sensor
```

如果后处理模块开启，则自动回退双 pass：

```text
[Stage6 FinalPipeline] renderMode=HeadlessOffscreen headlessFastDirectFinal=0 finalPostprocessBypass=0 reason=mtf_or_agc_or_noise_or_weather_active
```

#### H3.2 JPEG 优化

当前 RK3588 日志中 `JpegEncodeMode=rgb`、`JpegQuality=100`，JPEG 往往耗费数毫秒到十几毫秒。红外图像本质是灰度图，实时模式建议新增或完善 JPEG realtime profile：

```ini
[TcpOutput]
JpegEncodeMode=gray
JpegQuality=90
```

或：

```ini
[TcpOutput]
JpegEncodeMode=gray
JpegQuality=95
```

实现要求：

1. 保持默认生产配置不强制改变；新增推荐的 RK3588 realtime 模板或文档说明。
2. 保留 `rgb + quality=100` 作为质量/调试档。
3. 避免每帧重复分配 `cv::Mat` 和编码缓冲，尽量复用 CPU frame buffer。
4. 记录 `jpegMs`、`flipMs`、`resizeMs`、编码输入通道数和输出字节数。

验收指标：

```text
jpegMs 相比 rgb+quality100 明显下降
activeCodec=jpeg
h264En=0 或 fallback 逻辑保持不变
TCP 协议不变
```

#### H3.3 读回优化：减少 GPU 到 CPU 同步等待

当前 `Stage6FinalSensorTex` 通过 `RTM_copy_ram` 提供 CPU 读回。GPU 到 CPU readback 可能引入同步等待，部分等待时间可能计入 `do_frame()` 的 `renderMs`，部分计入 capture 的 `readbackMs`。

优化方向：

1. 不在运行中反复创建/销毁 `Stage6RawSceneBuffer`、`Stage6FinalSensorBuffer`、`Stage6FinalSensorTex`。
2. 分辨率不变时复用 buffer、texture、display region、shader card。
3. 确保只读有效区域，例如 800x800，避免 offscreen texture power-of-two padding 导致 1024x1024 读回或采样。
4. 研究双缓冲或三缓冲读回：
   - GPU 渲染第 N 帧；
   - CPU 编码第 N-1 帧；
   - 可接受增加 1 帧延迟，但不得让 sourceSeqLag 持续增长。
5. 记录 `readbackMs`、`frameCopyMs`、`copyRamWaitMs` 或等价诊断字段。

验收指标：

```text
readbackMs 稳定下降或至少不恶化
renderMs 中疑似 GPU wait 减少
sourceSeqLag 不再长期保持 17
inputQueueDepth 不再长期满 16
```

#### H3.4 标注开销进一步降低

当前 RK3588 日志中 annotation 仍有数毫秒级开销。H3 要把“标注计算”和“标注画到红外图像里”解耦：

新增配置建议：

```ini
[Annotation]
OverlayInSensorImage=false
JsonPerFrame=true
```

策略：

1. JSON 标注继续每帧随 TCP 包输出，保证图像帧、annotation、实时数据一一对应。
2. 图像内 overlay 默认可关闭，尤其 RK3588 realtime/headless 模式下建议关闭。
3. 若需要在显示端看到框和关键点，由 `HwaSim_IR_VideoDisplay` 依据 annotation JSON 叠加；本阶段可先不修改 VideoDisplay，只保留配置与日志。
4. bbox/keypoint 计算按 `AnnotationUpdateHz` 低频更新，中间帧复用上次结果或做轻量插值。
5. 遮挡检测/mesh collision 数据只构建一次，目标姿态变化时仅更新矩阵；避免每帧重建 collision solids。
6. 若必须保存“带框图像”，可在后续保存模块中做离线合成，不阻塞实时 TCP 主链路。

验收指标：

```text
annotationMs 明显下降
Annotation JSON 每帧仍存在
sourceSeqContinuous=1
图像内 overlay 可配置开关
```

#### H3.5 拆分 renderMs 计时，查清谁最贵

当前 `renderMs` 是 `m_pFramework->do_frame(current_thread)` 的外层墙钟时间，包含 Panda3D task、多个 GraphicsOutput、display region、offscreen pass、overlay region、驱动等待、RTM_copy_ram 相关同步等。H3 需要加入更细粒度的诊断，而不是只看一个总 renderMs。

新增诊断字段建议：

```text
stage6RawPassEnabled
stage6FinalPassEnabled
annotationOverlayEnabled
graphicsOutputCount
activeDisplayRegionCount
stage6RawOutputReady
stage6FinalOutputReady
renderPath=direct_final/dual_pass
copyRamMode=RTM_copy_ram
pandaDoFrameMs
captureReadbackMs
frameCopyMs
jpegMs
annotationComputeMs
annotationOverlayMs
```

建议新增 A/B 测试脚本或运行模式：

```text
A：Headless dual pass + overlay on + jpeg rgb quality100
B：Headless dual pass + overlay off
C：Headless direct final + overlay off
D：Headless direct final + overlay off + jpeg gray quality95
E：VisibleWindow baseline
```

验收输出：

```text
每个 case 输出 30 秒 summary:
outputFps/renderFps/renderMs/readbackMs/jpegMs/annotationMs/inputQueueDepth/sourceSeqLag/latencyAvgMs
```

#### H3.6 Windows HeadlessOffscreen 测试

可以让 Codex 在 Windows 里做 HeadlessOffscreen 运行测试，但要明确测试目标：

1. Windows 默认仍使用 `VisibleWindow`，HeadlessOffscreen 是开发诊断模式。
2. Windows HeadlessOffscreen 应该能输出非黑图；如果仍黑，需要通过 `HeadlessImageProbe` 打印 final sensor 的 min/max/mean/nonBlackRatio。
3. 若 Windows 原生 no-window offscreen 对 Panda3D/显卡驱动不稳定，可记录为平台差异；Windows 调试继续推荐 VisibleWindow，RK3588 才是 HeadlessOffscreen 正式部署目标。
4. Windows 测试不能替代 RK3588 板端无 DISPLAY 实测，只能验证代码路径和图像正确性。

推荐 Windows 测试配置：

```ini
[RenderBackend]
PresentationMode=HeadlessOffscreen
WindowPreview=false
HeadlessFastDirectFinal=true
HeadlessImageProbe=true
```

验收：

```text
Stage6 Capture source=final_sensor channels=RGB8
ImageProbe final mean/max/nonBlackRatio 正常
VideoDisplay 图像不是纯黑或仅 annotation overlay
```

### H4：RK3588 systemd 开机自启

目标：板卡上电后由 systemd 启动，配置 `RenderPresentationMode=HeadlessOffscreen`，不设置 `DISPLAY`。

本阶段不实施。

---

## 4. 双通道记录

近期若必须双通道，优先两个 `HwaSim_IR` 进程，各自独立 UDP/TCP 端口和配置，改动小、隔离强。

长期可做单进程多通道：多 camera、多 `Stage6FinalSensorTex`、多 TCP output queue、多传感器配置。该方向会触及调度、annotation、性能统计和资源生命周期，本阶段不实现。

---

## 5. H0/H1/H2/H3 修改记录

### 2026-06-25 / v5

- 追加 H3 HeadlessOffscreen 性能收口规划：Render Pass Planner、HeadlessFastDirectFinal、JPEG realtime profile、读回双缓冲/有效区域读回、annotation overlay 可选、renderMs 拆分计时。
- 明确当前 `renderMs` 是 Panda3D `do_frame()` 外层墙钟时间，不等同于单一 shader 或单一 draw call 时间。
- 明确无后处理时应允许 `3D scene -> Stage6FinalSensorTex` 直接输出，避免固定 raw pass + final card pass。
- 明确 Windows HeadlessOffscreen 可以作为 Codex 本地测试项，但不能替代 RK3588 无 DISPLAY 实机测试；Windows 默认仍推荐 VisibleWindow。
- 继续保持 H.264、TCP 协议、双通道为非目标。


### 2026-06-25 / v5 H3 implementation update

- Added RenderBackend flags `HeadlessFastDirectFinal`, `HeadlessImageProbe`, `RenderPerfProbe`; defaults keep VisibleWindow + JPEG rgb/q100 behavior unchanged.
- Added HeadlessOffscreen render pass planner: no-op Stage6 final postprocess uses `renderPath=direct_final` and skips `Stage6RawSceneBuffer/Stage6RawSceneTex/Stage6_FinalSensor_Card`; active MTF/DetectorNoise/AGC/Stage7 screen overlay/display transform falls back to `renderPath=dual_pass`.
- Added annotation switches `OverlayInSensorImage` and `JsonPerFrame`; `OverlayInSensorImage=false` disables image overlay display region while preserving annotation manager JSON snapshots for TCP frames.
- Added diagnostics: `RenderPerfProbe`, `HeadlessImageProbe`, Stage6 pipeline fields `renderPath/finalPostprocessBypass/finalPostprocessNoop/rawBufferReady/finalBufferReady`, capture fields `renderTextureWidth/renderTextureHeight/tcpWidth/tcpHeight/textureCropApplied/copyRamMode`.
- Added `tools/phase_h3_headless_perf_ab.ps1` with A/B cases: dual pass overlay on rgb q100, dual pass overlay off rgb q100, direct final overlay off rgb q100, direct final overlay off gray q95.
- Fixed Windows HeadlessOffscreen dual-pass black image: headless `Stage6RawSceneBuffer` now attaches `Stage6RawSceneTex` with `RTM_copy_texture` instead of `RTM_bind_or_copy`; `rawSceneCopyMode` is logged. Verification `B_dual_pass_overlay_off_jpeg_rgb_q100` shows `final_sensor_black=0`, `nonBlackRatio=1.0`, so the scene is present without relying on annotation overlay.
- H3 unchanged boundaries: no TCP protocol change, no real H.264 implementation, no dual-channel implementation, default production behavior remains VisibleWindow + JPEG/fallback.
- H3 verification update: Windows Release x64 build passed; Windows default VisibleWindow smoke passed before the dual-pass raw texture fix; Windows HeadlessOffscreen A/B smoke passed after the fix. RK3588/Linux no-DISPLAY runtime validation remains pending.

### 2026-06-25 / v6 H4 implementation update

- H4 goal: RK3588 direct_final performance closure after H3 proved `renderPath=direct_final` and `OverlayInSensorImage=false`; focus moved from render pass count to `sceneUpdateMs`, annotation compute, invisible-target update culling, and diagnostics.
- Fixed `OverlayInSensorImage=false` semantics: annotation JSON can still update, but image overlay draw is no longer called. Windows validation should show no `[AnnotationDraw]` lines when `OverlayInSensorImage=false`.
- Added annotation realtime fast path config: `FastJsonMode`, `BBoxUpdateHz`, `OcclusionUpdateHz`, `ReuseLastWhenSkipped` plus environment overrides `AnnotationFastJsonMode`, `AnnotationBBoxUpdateHz`, `AnnotationOcclusionUpdateHz`, `AnnotationReuseLastWhenSkipped`. Default `FastJsonMode=false` keeps existing behavior.
- FastJson behavior: bbox/keypoint geometry updates at `BBoxUpdateHz`, mesh occlusion updates at `OcclusionUpdateHz`, skipped frames reuse the latest annotation geometry while updating `frameIndex/simTime/sensorID/size`, so TCP JSON can remain per-frame.
- Added collision cache diagnostics: `[AnnotationCollisionCache] platform=... built=... reused=... triangles=... solids=...` to distinguish first-build cost from cache reuse.
- Added optional invisible-target update culling via `TargetUpdateCullInvisible=false` default. When enabled, beyond-far or non-renderable targets skip Stage4/Stage5 shader/radiance/plume updates while preserving target mapping state. Logs use `[TargetUpdateCull] total=... visible=... skippedBeyondFar=... skippedShaderApply=...`.
- Expanded diagnostics with `[ScenePerfProbe]` and extra `[RenderPerfProbe]` fields: `processRealSceneMs`, `targetMappingMs`, `cameraControlMs`, `stage4Stage5UpdateMs`, `shaderInputApplyMs`, `annotationBBoxMs`, `annotationOcclusionMs`, `annotationJsonMs`, `pandaDoFrameMs`, `readbackMs`, `frameCopyMs`, `jpegMs`.
- Added `tools/phase_h4_rk3588_perf_triage.ps1` A/B cases: current direct_final gray q95 overlay off, fast JSON on, occlusion off, JSON off diagnosis, target update cull on.
- H4 unchanged boundaries: no TCP protocol change, no real H.264 implementation, no dual-channel implementation, default production behavior remains VisibleWindow + JPEG/fallback.
- H4 pending verification: Windows Release x64 build, Windows default VisibleWindow smoke, Windows HeadlessOffscreen direct_final smoke, and RK3588/Linux no-DISPLAY runtime validation with the recommended realtime flags.

### 2026-06-25 / v4

- H2 实现真实 HeadlessOffscreen 离屏渲染宿主：`Stage6RawSceneBuffer` -> final postprocess -> `Stage6FinalSensorBuffer` -> `Stage6FinalSensorTex`。
- 新增 `m_stage6FinalSensorBuffer`、`m_stage6PresentationOutput`、`m_headlessCameraNode`，并新增 `InitHeadlessSceneCamera()`。
- `SetupAnnotationOverlayRegion()` 改为使用 `m_stage6PresentationOutput`，Headless 下 overlay 能进入 final sensor buffer。
- `LoadPlatformAssetNode()` 改为通用 Loader 加载，不再依赖 `WindowFramework`，Headless 下模型可挂到 `HeadlessOffscreenRenderRoot`。
- CaptureTask 对 Headless offscreen texture padding 做有效尺寸裁剪，继续向 `TcpCommThread` 提供 CPU RGB frame、宽高、telemetry、annotationRecord。
- 配置默认仍为 `PresentationMode=VisibleWindow`，Linux/RK3588 推荐以环境变量 `RenderPresentationMode=HeadlessOffscreen` 运行，无需 `DISPLAY`。
- 验收结果：Windows Release x64 编译通过；默认 VisibleWindow 短跑通过；HeadlessOffscreen 短跑通过并打印 `headless_final_pipeline_ready=1 rawBufferReady=1 finalBufferReady=1`。
- 带本地 UDP 激励短跑结果：`[Codec] h264En=0 requestedCodec=jpeg activeCodec=jpeg`，`[Stage6 Capture] frameWidth=800 frameHeight=800 tcpWidth=800 tcpHeight=800 source=final_sensor channels=RGB8`。
- 未完成项：Linux/RK3588 aarch64 交叉编译与板端无 DISPLAY 实机验证仍需在对应工具链/板卡环境执行；H.264、TCP 协议、双通道仍保持非目标。

### 2026-06-25 / v3

- 根据 Phase7A H.264 fallback 后状态，明确 HeadlessOffscreen 不修改 TCP 协议和编码链路。
- 新增 `[RenderBackend]` 默认配置，保持 `PresentationMode=VisibleWindow`。
- 新增 VisibleWindow/HeadlessOffscreen 启动分支与 `[RenderBackend]` 日志。
- 拆分 `InitVisibleWindowUi()` 与 `InitCommonCaptureTask()`。
- HeadlessOffscreen H1 只做安全启动和 TODO 日志，不创建真实 offscreen buffer。
- `run()`、Stage6 final pipeline、模型加载和天空初始化增加空窗口防护。
- 记录双通道需求：近期建议两进程，长期可评估单进程多通道。
