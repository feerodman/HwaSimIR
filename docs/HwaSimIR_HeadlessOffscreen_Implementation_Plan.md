# HwaSimIR HeadlessOffscreen 实施方案

> 版本：v3，H0/H1 实施记录版  
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
  H1 阶段暂不创建真实 offscreen buffer。
  先解耦启动、配置、网络线程、CaptureTask 和空指针防护。
  H2 阶段再实现真正 offscreen GraphicsBuffer/finalSensorTex。
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

本阶段不实施。

### H3：RK3588 systemd 开机自启

目标：板卡上电后由 systemd 启动，配置 `RenderPresentationMode=HeadlessOffscreen`，不设置 `DISPLAY`。

本阶段不实施。

---

## 4. 双通道记录

近期若必须双通道，优先两个 `HwaSim_IR` 进程，各自独立 UDP/TCP 端口和配置，改动小、隔离强。

长期可做单进程多通道：多 camera、多 `Stage6FinalSensorTex`、多 TCP output queue、多传感器配置。该方向会触及调度、annotation、性能统计和资源生命周期，本阶段不实现。

---

## 5. H0/H1 修改记录

### 2026-06-25 / v3

- 根据 Phase7A H.264 fallback 后状态，明确 HeadlessOffscreen 不修改 TCP 协议和编码链路。
- 新增 `[RenderBackend]` 默认配置，保持 `PresentationMode=VisibleWindow`。
- 新增 VisibleWindow/HeadlessOffscreen 启动分支与 `[RenderBackend]` 日志。
- 拆分 `InitVisibleWindowUi()` 与 `InitCommonCaptureTask()`。
- HeadlessOffscreen H1 只做安全启动和 TODO 日志，不创建真实 offscreen buffer。
- `run()`、Stage6 final pipeline、模型加载和天空初始化增加空窗口防护。
- 记录双通道需求：近期建议两进程，长期可评估单进程多通道。
