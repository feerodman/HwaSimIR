# HwaSimIR 红外架构重构、同步 60 FPS 与低延时实施方案

> 版本：v2，同步实时输出约束补充版  
> 更新时间：2026-06-11  
> 目标平台：当前 Windows 功能测试，最终 RK3588 / Debian 11 / aarch64 运行  
> 涉及工程：`HwaSim_IR`、`HwaSim_IR_VideoDisplay`、`DataDrivenTestQT`  
> 主体目标：主要空中目标红外实时成像仿真；看向地面时只需物理一致的灰度背景；同步模式必须做到“一组 UDP 实时数据 → 一帧图像渲染 → 一帧图像输出”；`videoFps=60` 时同步链路必须稳定达到 60 FPS；从 HwaSim_IR 接收 UDP 实时包到 HwaSim_IR_VideoDisplay 显示图像的平均延时不大于 80 ms。

---

## 0. 本轮新增硬指标

### 0.1 帧率指标

`InitObjectTrackingParam::trackingInit.videoFps` 是仿真输出目标帧率，不只是录像参数。

同步模式：

```text
每收到 1 组 DisplayC2cObjTrackingData 实时 UDP 包
  → 主线程消费该包
  → 更新场景状态
  → 渲染 1 帧
  → 读回/编码/发送 1 帧
  → HwaSim_IR_VideoDisplay 显示对应图像
```

同步模式允许低帧率，例如 `videoFps=25` 时按 25 FPS 输出；但系统能力必须至少支持 `videoFps=60`。当 `videoFps=60` 且 DataDrivenTestQT 以约 16.67 ms 周期发送实时包时，HwaSim_IR 不能因为红外计算、日志、读回、JPEG 或 TCP 导致实际输出上不去。

异步模式：

```text
可以锁到低帧率，例如 25 / 30 FPS。
也可以锁到 60 FPS。
不限帧时，开始接收实时数据后也必须稳定不低于 60 FPS。
```

### 0.2 延时指标

端到端延时定义：

```text
Latency = HwaSim_IR_VideoDisplay 显示该帧的时间
        - HwaSim_IR 接收到对应 UDP 实时数据包的时间
```

硬指标：

```text
平均延时 <= 80 ms
建议 P95 延时 <= 120 ms
同步 60 FPS 时，队列深度必须受控，不能通过积压旧帧换取“不丢帧”。
```

注意：如果 HwaSim_IR 和 HwaSim_IR_VideoDisplay 不在同一设备或系统时钟不同步，端到端绝对时间需要 NTP/PTP 或统一时钟校准。否则第一阶段先分别统计：

```text
HwaSim_IR 内部：UDP receive → TCP send
HwaSim_IR_VideoDisplay 内部：TCP receive → QImage decode → QLabel display
```

两段相加作为工程近似。

### 0.3 发送降频约束

同步模式不能用 JPEG 发送降频规避性能问题。

```text
同步模式：TCP/JPEG 输出帧率必须等于输入 UDP 实时数据帧率，1 包对应 1 帧。
异步模式：TCP 输出帧率可以配置为低于渲染帧率，但必须明确标注为异步输出，不用于“一包一帧”验收。
```

---

## 1. 对关键问题的直接结论

### 1.1 `SetRenderMode(false, 0)` 为什么仍然掉帧

`SetRenderMode(false, 0)` 只是取消 Panda3D 的时钟限帧，让渲染循环尽量快跑。它不会减少每帧任务量。

仿真开始后目标出现，以下任务会集中启动：

```text
UDP 实时数据处理
目标位置/姿态更新
相机 look-at / 手动角更新
红外 radiance / 热源 / plume / 背景 / 天气更新
大量 set_shader_input
标注投影与 overlay
render texture GPU → CPU 读回
OpenCV flip / resize / JPEG 编码
TCP 发送
HwaSim_IR_VideoDisplay 解码显示 / 保存
std::cout 高频日志
```

所以异步不限帧仍掉到十几二十 FPS，本质是每帧工作量过大，不是时钟模式问题。

### 1.2 `get_ram_image_as()` 有没有更好的办法

当前 TCP/JPEG 输出必须得到 CPU 侧图像数据，因此完全避免 GPU→CPU 读回不现实。可优化方向是减少读回阻塞、减少格式转换、减少编码开销。

优先级如下：

1. **不要每帧做多余格式转换**  
   render texture 的格式、尺寸、方向应直接匹配 TCP 输出。避免每帧 `get_ram_image_as("RGB")` 后再 `cv::resize()`、`cv::flip()`。

2. **尽量使用已有 RAM copy，减少同步阻塞**  
   用 Panda3D render-to-texture 时研究 `RTM_copy_ram` / RAM image 更新方式。目标是让渲染结束时纹理已经有 CPU copy，发送线程只取已经完成的上一帧，而不是在主线程临时强制 readback。

3. **同步模式使用双缓冲或环形帧槽**  
   一帧渲染完成后，把该帧 CPU 图像指针/拷贝、实时数据快照、annotation 快照组成 `FramePacket` 放入受限队列。队列长度建议 1~2。队列满说明输出跟不上，应记录 overrun，而不是无限堆积。

4. **Windows 后续可考虑 PBO 异步读回**  
   通过 OpenGL Pixel Buffer Object 做异步 readback，取上一帧或上两帧数据，降低 GPU pipeline stall。Panda3D 高层接口未必直接暴露完整 PBO 流程，可能需要平台相关实现，因此放到性能第二阶段以后。

5. **RK3588 后续优先考虑硬件编码**  
   OpenCV JPEG 在 CPU 上 60 FPS 编码可能吃紧。RK3588 可评估 Rockchip MPP 硬件 H.264/H.265/MJPEG 编码；Windows 可评估 Media Foundation 或平台硬件编码。第一阶段仍保持 JPEG 协议不大改，只做耗时统计和拷贝/resize/flip 减少。

结论：第一阶段不建议推翻 TCP/JPEG 协议；先把读回、flip、resize、JPEG 分别计时，消除明显冗余。同步 60 FPS 如果卡在 JPEG，需要第二阶段引入硬件编码或更轻的帧格式。

### 1.3 现在是否使用 GPU 硬件渲染

HwaSim_IR 是 Panda3D 渲染程序，场景绘制、shader、透明 plume、后处理 pass 理论上由 GPU 完成。不是“纯 CPU 渲染”。

但当前仍有大量 CPU 工作：

```text
UDP/TCP 线程
场景状态更新
NodePath 矩阵与 shader input 更新
IRRadianceModel CPU 计算
标注投影
GPU→CPU 图像读回
OpenCV resize/flip/JPEG 编码
日志输出
```

需要在启动时打印真实图形后端，确认 Windows 和 RK3588 没有退化到软件渲染：

```text
GraphicsPipe 类型
GSG driver vendor
GSG driver renderer
GSG driver version
OpenGL / OpenGL ES 版本
是否 llvmpipe / softpipe / software rasterizer
```

如果 RK3588 上显示 `llvmpipe`、`softpipe` 或类似软件光栅器，则实际上是在 CPU 软件渲染，必须先修 GPU 驱动、EGL/OpenGL ES、Panda3D 构建和运行环境。

---

## 2. 当前总体结论

当前主分支代码已经不是简单灰度滤镜，而是已经形成 Stage2~Stage7 红外链路雏形：材质映射、MODTRAN tau LUT、发动机热源/亮斑、Stage5 radiance debug、尾焰 plume、Stage6 显示输出、Stage7 天空/地面/天气背景、TCP/JPEG/标注转发都已存在。

但是当前架构仍未收敛成正式物理红外链路，主要问题是：

1. **物理辐射与显示灰度混在一起**。`IRRadianceModel` 仍输出 `baseRadiance` 这类归一化灰度量，不是稳定的 `L_surface / L_aperture / L_band` 物理量。
2. **Stage5 仍是 debug 链路**。Stage5 radiance 默认关闭，打开后才有 body/hotspot/brightspot 分量；path/sky/solar radiance 尚未作为正式链路进入运行时。
3. **engineState 仍会给整个目标加温**。应改为只影响局部喷口/尾焰/发动机热源。
4. **AGC 与 MTF/blur 未完成**。Stage6 当前主要是 gain/offset/noise/white-hot，不是完整 Lens / Detector / Electronics / Display 链路。
5. **地面/天空背景偏经验灰度**。可以保留简化背景，但应由背景温度、发射率、大气 path radiance 和 AGC 映射得到灰度。
6. **实时性能必须前置**。同步 `videoFps=60` 是硬验收条件，不能等物理链路重构完成后再优化。

---

## 3. 三个工程职责边界

### 3.1 HwaSim_IR

职责：主仿真程序。

当前功能：

```text
Panda3D 场景加载、模型显示、相机控制
UDP 接收复位/初始化/开始/停止/实时成像数据
TargetState 驱动目标位置、姿态、可见性、发动机状态
红外 shader、材质映射、天气背景、尾焰 plume、后处理输出
TCP 转发初始化/控制命令和最终 JPEG 图像、实时数据、标注 JSON
Linux/aarch64 CMake 入口
```

后续方向：

```text
把同步实时帧流水线显式化：UDP Frame → Scene Update → Render → Readback → Encode → TCP Send
把红外核心从 HwaSimIR.cpp 中拆出，避免继续堆 Stage 开关和大段 shader 字符串
新增 IRPerfStats / IRFrameContext / IRFramePacket / IRShaderInputCache
```

### 3.2 DataDrivenTestQT

职责：激励数据发送端。

必须新增或修正：

```text
videoFps 作为实时发送周期来源
当 videoFps=60 时，发送周期约 16.67 ms
当 videoFps=25 时，发送周期 40 ms
增加发送实际 FPS、包序号、发送时间戳日志
增加 16 / 25 / 33 / 40 ms 快捷配置
增加高度 3/5/10/15/20 km、Mach 0.5/1/2/3 测试组合
```

同步模式下，如果 DataDrivenTestQT 发送不到 60 包/秒，HwaSim_IR 不可能做到严格“一包一帧 60 FPS”。因此第一阶段验收时要同时看：

```text
DataDrivenTestQT sentFps
HwaSim_IR udpFps
HwaSim_IR renderFps
HwaSim_IR outputFps
VideoDisplay receiveFps
VideoDisplay displayFps
```

### 3.3 HwaSim_IR_VideoDisplay

职责：TCP 图像接收、显示、保存 MP4、保存实时数据和标注文件。

后续建议：

```text
不参与红外物理计算
增加接收 FPS、显示 FPS、解码耗时、显示耗时、包序号连续性、延时统计
保存每帧 annotation JSON 中的 frameSeq / udpReceiveTime / tcpSendTime 等诊断字段
```

---

## 4. 同步/异步渲染模式定义

### 4.1 Strict Sync 模式

用于正式“一包一帧”验收。

```text
输入：DisplayC2cObjTrackingData UDP 包
输出：对应图像帧 + 同源 trackingData + 同源 annotationJson
约束：不丢实时包，不主动降输出帧率，不跳过 JPEG/TCP 输出
```

主循环逻辑建议：

```text
while running:
  wait until latest unconsumed UDP frame exists, with timeout for UI responsiveness
  copy exactly one UDP frame into currentFrameContext
  scene_update(currentFrameContext)
  ir_update(currentFrameContext)
  do_frame()
  readback final frame
  encode + send frame packet
```

如果处理时间超过 `1 / videoFps`：

```text
记录 syncOverrunCount
记录 overrunMs
记录 queueDepth
记录 latency
不得静默丢包
不得假装满足 60 FPS
```

### 4.2 Async Locked 模式

用于普通显示或低帧率输出。

```text
渲染帧率由 videoFps 或配置 TargetRenderFps 控制
UDP 使用 latest-only，可以丢弃中间旧包
TCP 输出可配置 25/30/60 FPS
```

### 4.3 Async Unlimited 模式

用于性能上限测试。

```text
不限帧跑满
接实时数据后仍要求 renderFps >= 60
不作为一包一帧同步验收
```

---

## 5. 端到端延时预算

目标平均延时 <= 80 ms。按 `videoFps=60`，单帧周期约 16.67 ms，建议预算：

```text
UDP receive + copy                 <= 1 ms
main-thread scene update            <= 3 ms
IR update / shader input update      <= 3 ms
Panda3D render                       <= 8~12 ms
GPU readback                         <= 4~8 ms
JPEG encode                          <= 5~10 ms
TCP send on LAN                      <= 1~5 ms
VideoDisplay receive/decode/display  <= 5~15 ms
queue waiting                        <= 0~16.7 ms，最多 1 帧
```

第一阶段必须新增诊断字段：

```text
frameSeq
udpReceiveTimeNs
sceneBeginNs / sceneEndNs
renderBeginNs / renderEndNs
readbackBeginNs / readbackEndNs
encodeBeginNs / encodeEndNs
tcpSendBeginNs / tcpSendEndNs
videoDisplayReceiveNs
videoDisplayImageDecodedNs
videoDisplayShownNs
```

若不同设备时钟不同步，HwaSim_IR 和 VideoDisplay 分别记录本机时钟，先用分段延时验收。

---

## 6. 帧率优化优先级

### P0：性能剖析，不先盲改

每 120 帧或每 2 秒打印一次：

```text
mode
videoFps
udpFps
renderFps
outputFps
videoDisplayFps
udp_copy_ms
scene_update_ms
camera_update_ms
ir_environment_ms
ir_radiance_ms
stage4_hotspot_ms
stage5_plume_ms
stage7_weather_ms
annotation_ms
render_ms
readback_ms
postprocess_ms
jpeg_ms
tcp_send_ms
tcp_queue_depth
latency_avg_ms
latency_p95_ms
sync_overrun_count
```

### P1：限制日志

所有 per-frame 日志改为：

```text
前 3 帧 + 每 120 帧 + 状态变化时打印
Release 默认关闭 verbose IR log
提供 EnablePerfLog / EnableIRVerboseLog
```

### P2：同步模式队列策略

同步模式不能 latest-only 丢包。建议：

```text
UDP 线程把实时包写入有界队列，队列长度建议 2~4
主线程每次取 1 包并渲染 1 帧
如果队列满，记录 overflow；正式验收时 overflow 必须为 0
```

异步模式可以 latest-only。

### P3：缓存运行环境和 shader inputs

```text
每帧构造一次 IRFrameContext
只有 band/env/weather/sensor 改变时刷新全局 shader input
每目标只更新 position、visible、engine/plume/brightspot 等动态 input
set_shader_input 做差分缓存，值没变不重复设置
```

### P4：隐藏目标跳过 IR 更新

```text
renderVisible=false 或 beyondFarClip=true：跳过 radiance、hotspot、plume shader 更新
隐藏目标 plume 直接 hide，不算 gray/radiance
```

### P5：热模型分频但同步输出不降帧

同步模式每包仍输出一帧，但低频物理量可以缓存：

```text
body/aero temperature：10~20 Hz 更新，帧间复用
engine nozzle/plume：30~60 Hz 更新，取决于 videoFps
shader 中用 currentTime 做轻量 flicker
```

### P6：Stage7 天气/背景轻量化

空中目标为主，默认：

```text
启用 sky/lower shell
关闭 cloud cards、precipitation particles
雾只作为 shader 参数，不创建大量节点
```

### P7：读回/编码优化

同步模式必须每输入帧输出一帧，但仍要优化：

```text
renderTex 尺寸等于 trackerSensorWidth/Height
输出方向在 shader/camera 阶段修正，避免每帧 cv::flip
禁止 capture_task 每帧 cv::resize
尽量 get_ram_image() 而不是 get_ram_image_as() 做格式转换
OpenCV JPEG quality 可配置
记录 readback_ms 与 jpeg_ms
```

### P8：后处理 GPU 化

MTF/blur、AGC、noise 优先做 shader pass，不做 CPU per-pixel。

---

## 7. 红外链路未完成项

### 7.1 材质分类 + 红外物性库

当前已有 Stage2 材质 ID 灰度纹理 + material map + MaterialDatabase 查询雏形。后续不是重做，而是完善：

```text
补齐目标专用材质：蒙皮、涂层、导引头罩、喷口、复材、玻璃
增加分波段 emissivity / reflectance
材质缺失必须 warning，不能静默兜底
```

### 7.2 MODTRAN path/sky/solar runtime 接入

最小先做：

```text
MWIR: tau_up + path_radiance
NIR/SWIR: tau_up + solar_irradiance
LWIR: tau_up + path_radiance
```

### 7.3 HotSpot / HeatSource 统一模型

核心改动：

```text
engineState 只控制 EngineNozzle / EnginePlume
strikeFlag / strikePart 只控制 StrikeBrightSpot
取消 engineState 给整个目标 body 加温
```

### 7.4 Lens / Detector / Electronics / Display

最终链路：

```text
IRSceneRadianceBuffer
  → Lens MTF / blur
  → Detector integration / NETD noise / FPN
  → Electronics gain / offset / ADC
  → Display AGC / white-hot / black-hot
  → 8-bit output
```

---

## 8. 高度 3~20 km、速度 0.5~3 Mach 对红外辐射的影响

可用协议：

```text
altitudeM = targetState.targetLoc.alt
speedKmh  = targetState.targetLoc.speed
speedMps  = speedKmh / 3.6
```

若 speed 不可靠，用相邻帧位置差和时间差估计。

最小模型：

```text
Mach = speedMps / speedOfSoundAtAlt
T_recovery = T_air * (1 + r * (gamma - 1) / 2 * Mach^2)
DeltaT_aero = partCoeff * heatBlend * max(0, T_recovery - T_air)
```

建议分部位：

```text
body_shell      机体基础温度
nose            机头/导引头，气动加热较强
wing_edge       翼面/前缘，气动加热较强
engine_nozzle   发动机喷口，engineState 控制
plume_core      尾焰核心，engineState 控制
plume_halo      尾焰外层，engineState 控制
brightspot      strikeFlag/strikePart 控制
```

---

## 9. 可以简化与不能省

可以简化：

```text
真实地形 DEM / DOM / CDB
建筑、道路、植被
复杂地表热平衡
复杂 BRDF
喷流 CFD
在线 MODTRAN
完整云雨雪体积散射
```

不能省：

```text
目标海拔高度
目标速度或 Mach
目标距离
目标材质/部位红外物性
目标发动机状态
喷口/尾焰局部坐标
大气 tau
至少 MWIR path radiance
传感器 FOV/IFOV/分辨率
MTF/blur
AGC
同步帧序号与延时统计
```

---

## 10. 分阶段实施方案

### 阶段 1：同步 60 FPS 性能与延时基线

目标：不改红外画面，只建立可验证的实时链路指标，并清除最明显性能杀手。

任务：

1. 新增 `IRPerfStats`，统计 UDP FPS、render FPS、output FPS、readback/JPEG/TCP/latency。
2. `videoFps` 驱动渲染模式：初始化后 `videoFps=60` 时同步目标 60，`videoFps=25` 时同步目标 25。
3. 同步模式增加 frameSeq 与一包一帧检查。
4. 所有 per-frame `std::cout` 限频。
5. capture/readback/JPEG 分段计时。
6. HwaSim_IR_VideoDisplay 增加接收 FPS、显示 FPS、解码耗时、延时字段显示/日志。
7. 禁止同步模式发送降频；异步模式发送降频只作为配置，不参与同步验收。
8. 启动时打印 GPU/GSG/renderer 信息，确认不是软件渲染。

验收：

```text
Windows Release x64，800x800，1~3 个目标：
videoFps=25，同步模式稳定 25 FPS。
videoFps=60，同步模式能测出实际瓶颈；优化后目标稳定 60 FPS。
异步锁 60 或不限帧时，接实时数据后 renderFps >= 60。
同步模式 outputFrames == udpFrames，frameSeq 连续，无静默丢帧。
平均 HwaSim_IR UDP receive → VideoDisplay display 延时 <= 80 ms，或先输出分段延时并定位超标段。
```

### 阶段 2：同步 60 FPS 急救优化

目标：把阶段 1 定位出的瓶颈降下来。

任务：

```text
去掉每帧 resize/flip/format conversion
renderTex 尺寸与 sensor 尺寸一致
只在必要时 set_shader_input
隐藏/超 far clip 目标跳过 IR 更新
Stage7 云卡/雨雪粒子默认关闭
热模型低频缓存
JPEG quality 可配置
TCP 队列有界，记录 queueDepth/overrun
```

验收：

```text
videoFps=60，同步模式一包一帧稳定输出，平均延时 <=80 ms。
如果 JPEG/readback 无法满足，则形成明确报告：readback_ms、jpeg_ms、tcp_ms，并进入硬件编码/PBO 第二轮优化。
```

### 阶段 3：取消 engineState 整体加温，统一 HeatSource 语义

目标：修正关键红外物理错误。

任务：

```text
删除 IRRadianceModel::evaluate() 中 engineOn 给 body 加 180/320K 的逻辑
engineState 只控制 nozzle/plume
strikeFlag/strikePart 只控制 brightspot
整理 legacy uniform 命名
```

验收：

```text
发动机开关只影响喷口/尾焰，不让整个目标突变。
```

### 阶段 4：Stage5 正式物理链路

目标：Stage5 debug 不再决定是否使用物理辐射。

任务：

```text
EnableIRPhysicalPipeline=1 默认开启
IRRadianceModel 输出分量：body/reflected/hotspot/plume/path/sensorInput
debug view 只查看分量
```

验收：

```text
不开 Stage5 debug 也走正式 radiance 合成。
```

### 阶段 5：目标高度/速度/Mach 热模型

目标：高度 3~20 km、Mach 0.5~3 对目标红外有可解释影响。

任务：

```text
新增 altitude atmosphere model
新增 Mach/aero heating model
输出 body/nose/edge/rear 温度修正
```

验收：

```text
高度、速度变化时温度、tau/path、目标对比度趋势合理。
```

### 阶段 6：材质库、MODTRAN、MTF/AGC 与背景物理化

按原方案继续执行：

```text
材质分类 + 分波段红外物性库
MODTRAN path/sky/solar runtime
MTF/blur pass
AGC pass
简化地面/天空背景物理化
```

### 阶段 7：RK3588 / Debian11 / aarch64 收口

目标：板卡可运行并满足阶段帧率目标。

任务：

```text
确认 RK3588 使用 Mali GPU 而非 llvmpipe/softpipe
清理 CMake/toolchain
GLSL ES 兼容
减少 shader exp/pow
评估 OpenCV JPEG，必要时接 Rockchip MPP 硬件编码
默认关闭昂贵天气/日志/frame dump
```

验收：

```text
Debian11/aarch64 Release 编译通过。
同步 videoFps=25/60 按阶段目标测试。
无 UDP/TCP 卡死。
输出阶段延时报告。
```

---

## 11. 阶段成果记录模板

后续每次 Codex 执行完成后，把结果追加在这里，新开聊天可直接读取本文件继续。

### 阶段记录格式

```text
阶段：
执行日期：
执行者：Codex / 人工
目标：
修改文件：
编译结果：
运行场景：
关键日志：
帧率结果：
延时结果：
发现的问题：
是否通过验收：
下一阶段建议：
```

### 阶段 1 记录

```text
阶段：1
执行日期：2026-06-11
执行者：Codex
目标：建立同步一包一帧、60 FPS、端到端延时和分段性能基线，并完成低风险性能修正。
修改文件：
- HwaSim_IR：HwaSimIR、Windows/Linux UDP/TCP、IRPerfStats、Runtime INI、VS/CMake 工程。
- HwaSim_IR_VideoDisplay：TCP 帧读取、接收/显示 FPS、解码和端到端延时统计。
- DataDrivenTestQT：videoFps 默认 60、精确定时发送、StimPerf 日志。
- tools：phase1_sync_perf_smoke.ps1、stage0_check.ps1。
编译结果：
- HwaSim_IR Windows Release x64 通过。
- DataDrivenTestQT Release 通过。
- HwaSim_IR_VideoDisplay Windows Release x64 通过。
运行场景：Windows Release x64，NVIDIA RTX 3070 Ti Laptop GPU，800x800，5 个目标有状态、targetNumValid=1，分别连续发送 25/60 Hz 各 6 秒。
关键日志：[GPU]、[StimPerf]、[Perf]、[SyncFrame]、[TcpPerf]、[VideoPerf]。
性能报告：logs/phase1/phase1-sync-perf-20260611-163726.json
帧率结果：
- videoFps=25：sent=24.946，udp=24.489，render=24.655，output=24.484 FPS，150/150/150 帧，序号连续。
- videoFps=60：sent=59.943，udp=60.058，render=60.060，output=59.891 FPS，360/360/360 帧，序号连续。
延时结果：
- 25 Hz 平均端到端延时 11.907 ms；readback 1.404 ms，JPEG 8.012 ms，TCP send 0.153 ms。
- 60 Hz 平均端到端延时 11.955 ms；readback 1.465 ms，JPEG 7.724 ms，TCP send 0.159 ms。
- 60 Hz 记录 1 次瞬时 overrun，但输入队列 overflow=0，TCP 无覆盖，VideoDisplay discontinuity=0。
发现的问题：
- VideoDisplay 的 readExactBytes 未优先消费已有缓冲，导致历史帧积压；已修复。
- 隐藏目标仍执行 radiance/hotspot/plume 节点更新，压缩了 60 Hz 余量；已改为隐藏目标只推进必要热惯性并跳过渲染状态写入。
- Windows 初始化/控制命令跨线程直接改渲染资源会偶发卡住；已统一进入主渲染线程命令队列。
- 高频日志、同步 latest-only/覆盖语义和发送端定时精度已修正并增加诊断。
回归结果：stage0_check、Stage3 MODTRAN tau-only、Stage4 hotspot/brightspot 静态检查与三波段运行烟测全部通过。
是否通过验收：通过。800x800 下 25/60 Hz 同步模式均一包一帧、零溢出、平均延时小于 80 ms。
下一阶段建议：继续降低 JPEG 约 7.7 ms 和 IR update 峰值，评估直接方向输出、PBO/异步 readback 与硬件编码；保持 Stage3 tau-only 和 Stage4 控制边界不变。
```

### 阶段 1B 真实 60 Hz 闭环记录

```text
阶段：1B
执行日期：2026-06-11
执行者：Codex
目标：修复人工真实运行中 videoFps=60 只有约 30 FPS，并建立 sourceSeq/outputOrdinal 分离的闭环诊断。
修改范围：仅 DataDrivenTestQT、HwaSim_IR、HwaSim_IR_VideoDisplay 的发送节流、同步遥测、日志限频、低频状态复用和显示节流；未改 MODTRAN、AGC、MTF、材质库及 JPEG 协议。
编译结果：三个工程 Windows Release x64 全部通过。
运行场景：800x800，videoFps=60，连续 30 秒，日志目录 logs/phase1b-final3-20260611-182443。
帧率结果：稳定区间 sent=60.006、udp=59.998、render=60.139、output=60.137、VideoDisplay receive/display=60.195 FPS。
序号结果：udpFrames=1799、renderFrames=1799、outputFrames=1799；sourceSeqContinuous=1，inputQueueOverflow=0，TCP overwritten=0。
延时结果：HwaSim_IR 平均 15.280 ms，稳定区间最大采样 150.242 ms（启动排队尖峰）；VideoDisplay 平均约 20.019 ms，均低于 80 ms 验收线。
分段结果：readback=1.351 ms，JPEG=6.764 ms，TCP send=0.140 ms，annotation=1.183 ms，IR update=4.551 ms，plume=0.041 ms。
性能修正：发送端 UI 5 Hz、普通诊断限频、annotation 15 Hz、IR 状态 30 Hz、plume 30 Hz并缓存；图像仍保持每个 UDP 包渲染并输出一帧。
显示端：60 Hz 下保存 MP4 同步写入自动暂停，避免阻塞显示；接收和显示队列无积压。
回归结果：Stage3 MODTRAN tau-only、Stage4 hotspot/brightspot 静态检查通过；Stage4 三波段烟测在测试期显式开启 verbose，运行后恢复默认配置。
是否通过验收：通过。真实 30 秒同步链路达到 60 Hz，瓶颈已从 HwaSim_IR 高频 scene/annotation/IR/plume 更新消除；当前主要余量消耗为 JPEG 和 IR update。
下一阶段建议：保持协议不变时优先评估 FlipInShader/FlipInVideoDisplay、PBO/异步 readback；如需同步录像，单独实现异步 MP4 写入。
```

### 阶段 1C 60 Hz 异步录像记录

```text
阶段：1C
执行日期：2026-06-12
执行者：Codex
目标：恢复 60 Hz 下 MP4、实时数据和目标标注保存，同时保持阶段 1B 的同步主链路帧率、序号连续性和延时指标。
修改范围：
- VideoDisplay 新增 AsyncVideoRecorder 常驻工作线程，主线程只执行非阻塞入队。
- 默认录像队列上限 180 帧，队列满时仅丢弃录像旁路帧并统计，不反压接收和显示。
- writer、QImage->BGR 转换、MP4、annotations.txt、target_annotations.txt 均移入录像线程。
- stop/reset/析构执行有超时 flush；每 2 秒输出 RecorderPerf。
- Stage6Capture 新增 FlipInShader=false、FlipInTcpThread=true；默认方向保持阶段 1B 不变，shader 翻转未启用。
- 未修改 MODTRAN、AGC、MTF、材质库、红外物理链路和 TCP/JPEG 协议。
编译结果：HwaSim_IR、DataDrivenTestQT、HwaSim_IR_VideoDisplay 三工程 Windows Release x64 全部通过。
运行场景：800x800，5 目标，videoFps=60，saveMP4En=true，连续 30 秒。
日志目录：logs/phase1c-final-20260612-115254。
录像目录：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260612_115308。
主链路帧率：sent=60.005、udp=60.012、render=60.190、output=60.121、VideoDisplay receive/display=60.219 FPS。
主链路序号：udpFrames=1801、renderFrames=1801、outputFrames=1801；sourceSeqContinuous=1，inputQueueOverflow=0，TCP overwritten=0。
延时与分段：平均端到端延时 16.463 ms；readback=1.242 ms，JPEG=4.710 ms，TCP send=0.106 ms，annotation=1.984 ms，IR update=5.553 ms，plume=0.042 ms。
录像结果：output.mp4 为 H.264、800x800、60 FPS、30.0167 秒，ffprobe 验证 1801 帧。
录像性能：writtenFps 平均 59.976，writeMsAvg=5.000 ms，writeMsMax=7.267 ms，maxQueueDepth=51，recordingDroppedFrames=0，sourceSeqContinuousWritten=1。
标注结果：annotations.txt=1801 行，target_annotations.txt=1801 行；两文件 JSON 全部有效，recordingFrameIndex 和 sourceSeq 均为 1..1801 连续。
显示主线程：recording enqueue 平均 0.009 ms、最大 0.078 ms，无 enqueue>1 ms 警告；保存视频未使 displayFps 降到 30。
回归结果：Stage3 MODTRAN tau-only strict、Stage4 hotspot/brightspot strict、Stage4 三波段运行烟测全部通过。
是否通过验收：通过。异步录像恢复后同步主链路与录像旁路均达到约 60 Hz，平均延时小于 80 ms，录像零丢帧。
下一阶段建议：暂不默认开启 FlipInShader。当前 TCP flip 平均约 0.808 ms，收益有限，而 shader 翻转还需窗口、TCP、VideoDisplay、bbox/keypoint 四路方向一致性专项验证。
```

### 阶段 1D JPEG A/B、h264En 诊断链路与 IR update 拆分

```text
阶段：1D
执行日期：2026-06-12
执行者：Codex
实施范围：
- TcpOutput 增加 Codec、JpegQuality、JpegEncodeMode、EnableH264Experimental、H264FallbackToJpeg 等运行配置。
- JPEG 支持 rgb/gray 输入，质量范围 40..100，默认以 rgb quality=100 作为基准。
- h264En 接入 requestedCodec -> activeCodec -> codecFallbackReason 诊断链路；本阶段未实现不可靠的实时 H.264 传输。
- VideoDisplay 支持单通道 JPEG 显示，异步录像写入前将灰度图转换为 BGR。
- [Perf] 增加 IR update 内部低频采样拆分；未修改 MODTRAN、AGC、MTF、材质库、PBO 和红外物理结果。
- 新增 tools/phase1d_codec_ab_smoke.ps1。

构建与回归：
- HwaSim_IR、DataDrivenTestQT、HwaSim_IR_VideoDisplay Windows Release x64 构建通过。
- Stage3 MODTRAN tau-only strict、Stage4 hotspot/brightspot strict、Stage4 三波段 smoke 通过。

10 秒 JPEG A/B 日志：logs/phase1d-codec-ab-20260612-175152
mode  quality  encodeMs  bytes/frame  decodeMs  latencyMs  displayFps
rgb   100      8.186     23824        5.135     313.475    52.932
rgb   90       14.488    16274        5.099     372.843    46.597
rgb   80       8.069     14610        5.104     306.928    52.474
rgb   70       9.469     13751        4.698     388.652    44.905
gray  100      3.677     15909        1.398     303.750    54.511
gray  90       3.746     12246        1.326     291.419    55.371
gray  80       3.745     10693        1.342     292.306    54.864
gray  70       3.434     10197        1.289     222.774    57.052

对比结果：
- 相对 gray quality=100，gray quality=90 编码耗时 +0.069 ms，单帧 -3663 bytes，延时 -12.331 ms。
- gray 模式的显示、录像、标注和 source sequence 检查正常，但尚未做主观画质验收。
- 整组测试期间多个非编码分段同时波动，rgb quality=90 为明显异常点；本表仅用于相对比较，不作为 60 Hz 通过依据。

h264En 验证：
- h264En=false：requestedCodec=jpeg，activeCodec=jpeg。
- h264En=true 且 EnableH264Experimental=false：打印一次 [Codec][WARN]，
  requestedCodec=h264，activeCodec=jpeg，codecFallbackReason=experimental_disabled。
- annotation JSON、[TcpPerf]、[VideoPerf] 均记录 codec 状态。

30 秒候选配置：Codec=auto，JpegEncodeMode=gray，JpegQuality=90。
日志目录：logs/phase1d-codec-ab-20260612-180547。
帧率：sent=60.024，udp=56.646，render=56.783，output=56.247，
VideoDisplay receive/display=57.538 FPS。
延时：平均 233.476 ms。
编码/显示：JPEG=2.726 ms，14459 bytes/frame；decode=1.403 ms。
录像：write=1.073 ms，dropped=0，maxQueueDepth=72，
sourceSeqContinuousWritten=1。
序号/队列：sourceSeqContinuous=1，input overflow=0，TCP overwrite=0。
帧数：UDP/render=1801/1801，TCP/录像=1785/1785；停止时仍有 16 帧输入积压。
录像文件：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260612_180602/output.mp4。
录像校验：800x800，60 FPS，1785 帧；annotations.txt 和
target_annotations.txt 均为 1785 行。

IR update 实际更新帧低频采样均值：
irEnvBuildMs=0.002，stage7SkyGroundMs=3.931，
platformRadianceMs=0.016，targetRadianceMs=0.078，
stage4HotspotMs=2.112，stage5PlumeMs=0.055，
shaderInputApplyMs=10.603。
shaderInputApply 与 Stage7/Stage4 子项存在包含关系，不能直接相加。

是否通过验收：未通过。功能链路、录像一致性和诊断完成，但本次 30 秒运行
只有约 56..58 FPS，平均延时超过 80 ms，且停止时存在尾部积压，不能宣称
保持了阶段 1C 的同步 60 Hz。

建议：
1. 保持生产默认 rgb quality=100；gray quality=90 作为恢复 60 Hz 后的优先复测候选。
2. H.264 单独立项，设计会话级封包、解码器生命周期和跨平台编码器接口。
3. 下一阶段先恢复 HwaSim_IR 主循环稳定 60 Hz 并消除尾部积压，不立即进入新的
   engineState 物理改动。engineState 继续只控制喷口、尾焰和后部热点，不引入整机整体加温。
```

### 阶段 2A engineState / EngineRear / EnginePlume / BrightSpot 语义修正

```text
阶段：2A
执行日期：2026-06-16
执行者：Codex
目标：修正 engineState 给整个目标机体加温的问题，并统一
EngineRear、EnginePlume、BrightSpot/StrikeSpot 语义。

修改范围：
- HwaSimIRRuntime.ini 的 [Stage4] 增加 LegacyEngineBodyHeating=false。
- EvaluateNodeRadiance 默认不再把 engineState 传入 body radiance 的
  legacy whole-body heating 分支；仅 LegacyEngineBodyHeating=true 时恢复旧效果。
- LegacyEngineBodyHeating=true 会打印 [Stage4][WARN]，标记为 legacy/debug 模式。
- Stage4 新增限频 [Stage4 HeatSourceDiag]，字段包括 sourceSeq、targetID、
  engineState、bodyTempK、bodyRadiance、rearHotspotEnabled、
  rearHotspotTempK/rearHotspotIntensity、plumeEnabled、strikeFlag、
  strikePart、brightspotPart、brightspotEnabled。
- u_brightspot_temp 保持 legacy uniform 名，但 C++ 注释和日志明确其传入的是
  brightspotIntensity，不是 Kelvin 温度。
- 扩展 tools/stage4_hotspot_check.ps1 和 tools/stage4_hotspot_smoke.ps1。
- 新增 tools/phase2a_sync60_save_smoke.ps1，用于 30 秒生产默认链路验收。

未修改：
TCP/JPEG/H.264 协议、gray JPEG 默认值、H.264 实时编码、PBO、MODTRAN
path/sky/solar、AGC/MTF、材质库结构、高度/Mach 热模型。

构建结果：
- HwaSim_IR Windows Release x64 通过。
- DataDrivenTestQT Release 通过。
- HwaSim_IR_VideoDisplay Windows Release x64 通过。
- 仅保留既有 C4101/C4251/C4267/PDB 类警告；VideoDisplay 0 warning / 0 error。

回归：
- Stage3 MODTRAN tau-only strict 通过。
- RuntimeConfig strict 通过。
- Stage4 hotspot/brightspot strict 通过。
- Stage4 三波段语义 smoke 通过，日志：
  logs/stage4/HwaSimIR-stage4-hotspot-smoke-20260616-113628.out.log。

Stage4 语义 smoke 摘要：
- engineState=false、strikeFlag=false：
  rear=0，plume=0，brightspot=0。
- engineState=true、strikeFlag=false：
  bodyTempK 仍为 298.15，bodyRadiance 仍为 0.0296085；
  rear=1，plume=1，brightspot=0。
- strikeFlag=true、strikePart=1：
  rear=0，plume=0，brightspotPart=Head，brightspot=1。
- strikeFlag=true、strikePart=2：
  rear=0，plume=0，brightspotPart=MidBody，brightspot=1。
- 结论：engineState 不再给 body 整体加温；strikeFlag/strikePart 不再影响
  rear hotspot 或 plume。

30 秒生产默认链路验收：
配置：Codec=auto，JpegEncodeMode=rgb，JpegQuality=100，
EnableH264Experimental=false，LegacyEngineBodyHeating=false，saveMP4En=true。
日志目录：logs/phase2a-final-20260616-114332。
录像目录：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260616_114347。

帧率：
- sent=60.020 FPS
- udp=56.134 FPS
- render=56.202 FPS
- output=55.700 FPS
- VideoDisplay receive/display=56.688 FPS

延时与队列：
- latencyAvgMs=293.448
- sourceSeqContinuous=1
- inputQueueOverflow=0
- TCP overwritten=0
- inputQueueDepth 长期达到 16，sourceSeqLag 约 16，存在输入积压。

录像：
- output.mp4 成功生成，ffprobe 读取 1785 帧。
- annotations.txt=1785 行，target_annotations.txt=1785 行。
- recordingDroppedFrames=0，sourceSeqContinuousWritten=1。

性能热点：
- jpegMs=5.809，readbackMs=1.404，recorderWriteMs=6.707。
- irUpdateMs=6.982，其中 stage7SkyGroundMs=3.856，
  stage4HotspotMs=2.062，shaderInputApplyMs=10.504。
- 与阶段 1D 的波动形态一致，主要瓶颈仍是 HwaSim_IR 主循环积压，
  不是本次 engineState 语义改动新增的 per-frame 日志或录像丢帧。

是否通过验收：未通过 60 Hz 性能验收。语义修正与录像一致性通过，但当前
自动 30 秒链路只有约 56 FPS、平均延时超过 80 ms，不能宣称保持阶段 1C
同步 60 Hz。下一步应先处理 HwaSim_IR 主循环 inputQueueDepth=16、
shaderInputApply/Stage7/Stage4 热点和停止时尾部积压，再继续物理重构。
```

---

## 12. 给 Codex 的第一阶段实施 Prompt

见单独文件：`Codex_Phase1_Sync60_Perf_Prompt.md`。
