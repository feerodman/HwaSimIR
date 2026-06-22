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

### 阶段 2B：主循环恢复同步 60 Hz

```text
阶段：2B
日期：2026-06-16
执行者：Codex
目标：在阶段 2A 之后恢复 HwaSim_IR 稳定同步 60 Hz，降低
shaderInputApply、Stage7SkyGround、Stage4Hotspot 开销，并消除持续性的
inputQueueDepth/sourceSeqLag 积压。

本阶段变更：
- 增加 HwaSimIR 运行时 shader input 差分缓存，覆盖 float/int/bool-like
  vec2/vec3 uniform。
- [Perf] 增加 shaderInputSetCount、shaderInputSkipCount、
  shaderInputCacheHitRate、stage7FullUpdateCount、stage7PositionOnlyCount、
  stage7SkipCount、stage4UpdateCount、stage4SkipCount。
- 明确 [Perf] 计时口径：
  shaderInputApplyScope=exclusive、stage7SkyGroundScope=inclusive、
  stage4HotspotScope=inclusive。
- 增加 Stage7Background/Stage7UpdateHz=10。Stage7 full update 由 dirty-key
  或 Hz 门控触发；相机移动时只更新 sky dome / lower shell 位置。
- 增加 Stage4/Stage4UpdateHz=30，并将 Stage4 hotspot/brightspot shader
  写入接入差分缓存。EngineRear / EnginePlume / BrightSpot 语义不变。
- 更新 tools/stage4_hotspot_check.ps1，使其接受 SetShaderInputCached 作为
  Stage4 uniform 写入路径。
- 更新 tools/phase2a_sync60_save_smoke.ps1，规范进程环境中的重复 Path/PATH，
  并汇总阶段 2B 性能计数器。

未改变：
- 不改 TCP/JPEG/H.264 协议。
- 不改 VideoDisplay 录像旁路逻辑。
- 不做 MODTRAN path/sky/solar、AGC、MTF、材质库结构调整、PBO/硬件编码、
  高度/Mach 热模型或新的红外物理功能。

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余警告为既有 VS/Panda/PDB 警告。

回归测试：
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段 smoke：通过。
  日志：logs/stage4/HwaSimIR-stage4-hotspot-smoke-20260616-151221.out.log

30 秒生产默认同步测试：
- 配置：800x800、5 目标、videoFps=60、saveMP4En=true、
  Codec=auto、JpegEncodeMode=rgb、JpegQuality=100、
  EnableH264Experimental=false、LegacyEngineBodyHeating=false。
- 日志：logs/phase2a-final-20260616-151436
- MP4：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260616_151451/output.mp4

帧率：
- sentFps=60.034
- udpFps=59.979
- renderFps=60.376
- outputFps=60.300
- VideoDisplay receive/display=60.287

延时与队列：
- latencyAvgMs=34.823
- sourceSeqContinuous=1
- sourceSeqContinuousWritten=1
- inputQueueOverflow=0
- TCP overwritten=0
- recordingDroppedFrames=0
- 稳态 60 Hz 区间 sourceSeqLag 稳定在 0..1。
- 稳态 inputQueueDepth 当前值回到 0。启动/冷缓存区间仍出现
  inputQueueDepthMax=16；后续稳态区间大多 <=2，偶发 3..4 的单周期峰值。

优化前后关键热点：
- shaderInputApplyMs：10.504 -> 0.726 avg（稳态过滤均值 0.784）
- stage7SkyGroundMs：3.856 -> 0.212 avg（稳态过滤均值 0.262）
- stage4HotspotMs：2.062 -> 0.916 avg（稳态过滤均值 0.948）
- irUpdateMs：6.982 -> 0.922 avg（稳态过滤均值 0.909）
- latencyAvgMs：293.448 -> 34.823

阶段 2B 计数器：
- shaderInputSetCount 稳态 [Perf] 区间平均约 744.5。
- shaderInputSkipCount 稳态 [Perf] 区间平均约 25425.8。
- shaderInputCacheHitRate 平均约 97.16%。
- stage7FullUpdateCount 稳态 [Perf] 区间平均约 16.1。
- stage7PositionOnlyCount 稳态 [Perf] 区间平均约 43.9。
- Stage7 启用时 stage7SkipCount max=0。
- stage4UpdateCount 稳态 [Perf] 区间平均约 30.7。
- stage4SkipCount 稳态 [Perf] 区间平均约 282.1。

录像与输出：
- output.mp4 frames=1800。
- annotations.txt=1800 行。
- target_annotations.txt=1800 行。
- Recorder writeMsAvg=6.905 ms，droppedFrames=0。

验收结论：
- 通过：恢复 60 Hz 主链路、sourceSeq 连续、latency <=80 ms、
  input overflow=0、TCP overwrite=0、录像丢帧=0。
- 剩余说明：启动/冷缓存队列峰值仍需观察，但稳态 60 Hz 区间不再存在持续
  inputQueueDepth/sourceSeqLag 积压。

下一步：
- 暂时保持生产默认 rgb + JPEG quality=100。
- 在剩余启动队列峰值被多次运行证明无害，或通过 cache warm-up / startup
  sequencing 降低前，不进入新的红外物理功能。
```

### 阶段 3A：Stage5 radiance components 正式化

```text
阶段：3A
日期：2026-06-16
执行者：Codex
目标：将 Stage5 radiance debug 正式化为 IRRadianceComponents /
IRSceneRadianceOutput 骨架，同时保持阶段 2B 的同步 60 Hz。

本阶段变更：
- 在 IRRadianceModelV2 中新增 IRRadianceComponents 和 IRSceneRadianceOutput。
- 新增 evaluateComponents()，同时保留现有 evaluate() 兼容输出，保证当前
  shader/display 路径不被破坏。
- 正式分量包含 band、materialName、materialTempK、emissivity、reflectance、
  bodyRadiance、reflectedRadiance、rearHotspotRadiance、plumeRadiance、
  brightspotRadiance、tauUp、pathRadiance、pathRadianceSource、
  sensorInputRadiance、displayPreview、sourceFlags。
- 新增 [Stage5Radiance] 运行时配置：
  EnableIRPhysicalPipeline=true、DebugView=Off、LogComponents=false、
  ComponentLogEveryFrames=120。
- DebugView 只作为可视化/日志选择器，支持 Off、Body、Reflected、
  RearHotspot、Plume、BrightSpot、Atmosphere、SensorInput；
  Legacy Composite 作为 SensorInput 别名兼容。
- Stage5 component uniforms 全部通过 SetShaderInputCached 写入。
- 目标 Stage5 component uniforms 不再由通用 ApplyRadianceInputs 路径默认清零。
  这避免同一帧先写默认值、再写真实值，是 3A 主要的 cache-churn 风险修复。
- [Stage5 RadianceComponents] 由 LogComponents/DebugView/verbose 控制，并保持限频。
- Engine plume 在应用 Stage4/Stage5 target components 前更新，使 plumeRadiance
  使用当前帧 plume cache。
- 新增 tools/stage5_radiance_components_smoke.ps1，并更新 Stage5 静态检查以覆盖
  正式分量骨架。

未改变：
- 不接入 MODTRAN path/sky/solar runtime。pathRadianceSource 明确为
  legacy_empirical 或 disabled。
- 不做 AGC/MTF、高度/Mach 热模型、TCP/JPEG/H.264 协议改动、录像旁路改动、
  材质库结构调整、PBO 或硬件编码。
- engineState 仍只影响 EngineRear 和 EnginePlume；strikeFlag / strikePart
  仍只影响 BrightSpot。

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余警告为既有 VS/Panda/PDB 警告。

回归测试：
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段语义 smoke：通过。
  日志：logs/stage4/HwaSimIR-stage4-hotspot-smoke-20260616-170518.out.log
- Stage5 min radiance 静态检查：通过。
- Stage5 radiance components smoke：通过。
  摘要：logs/stage5/stage5_radiance_components_smoke_summary.csv

RadianceComponents smoke 摘要：
- SWIR baseline：body=0.00000659458、rear=0、plume=0、bright=0，
  pathRadianceSource=legacy_empirical、DebugView=Off。
- MWIR baseline：body=0.308302、rear=0、plume=0、bright=0。
- MWIR engine：body=0.308302、rear=57.3932、plume=1、bright=0。
- MWIR strike head：body=0.308302、bright=361.493。
- MWIR strike mid：body=0.308302、bright=271.12。
- LWIR baseline：body=4.43015、rear=0、plume=0、bright=0。
- engineState 不增加 bodyRadiance；rear/plume 随 engineState 增强。
- brightspotRadiance 只随 strikeFlag/strikePart 增强。
- pathRadianceSource 没有被误标为 MODTRAN runtime。

30 秒生产默认同步测试：
- 配置：800x800、5 目标、videoFps=60、saveMP4En=true、
  Codec=auto、JpegEncodeMode=rgb、JpegQuality=100、
  EnableH264Experimental=false、LegacyEngineBodyHeating=false、
  EnableIRPhysicalPipeline=true、DebugView=Off。
- 日志：logs/phase2a-final-20260616-171104
- MP4：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260616_171118/output.mp4
- sentFps=60.035、udpFps=60.051、renderFps=60.438、
  outputFps=60.251、VideoDisplay receive/display=60.316。
- latencyAvgMs=34.324。
- sourceSeqContinuous=1、sourceSeqContinuousWritten=1。
- inputQueueOverflow=0、TCP overwritten=0、recordingDroppedFrames=0。
- written/mp4/annotations/targetAnnotations=1800/1800/1800/1800。
- sourceSeqLagMax=2。稳态区间没有持续积压；当前 lag 回到接近 0，仅偶发短峰值。
- inputQueueDepthMax=14 来自启动/冷缓存；稳态当前深度回到接近 0，大多 <=2，
  偶发短峰值。

与阶段 2B 的性能对比：
- 阶段 2B 基线：irUpdateMs=0.922、shaderInputApplyMs=0.726、
  stage7SkyGroundMs=0.212、stage4HotspotMs=0.916、latencyAvgMs=34.823。
- 阶段 3A：irUpdateMs=0.985、shaderInputApplyMs=0.657、
  stage7SkyGroundMs=0.219、stage4HotspotMs=1.313、latencyAvgMs=34.324。
- shaderInputCacheHitRateAvg 仍保持 97.473%。
- Stage7/Stage4 仍受 dirty-key / 分频门控控制，没有回退到每帧全量更新。

实现说明：
- 3A 第一次尝试低于 60 Hz，是因为 target Stage5 component uniforms 被
  ApplyRadianceInputs 重置后，又被 Stage5 component pass 在同一帧覆盖。
  这让很大一部分 target uniform 流量翻倍，并把 cache hit rate 降到约 80%。
  将默认 component 写入限制到非目标对象后，恢复了 cache hit rate 和 60 Hz 预算。

验收结论：
- 通过：正式 Stage5 radiance component 骨架。
- 通过：Stage3/Stage4/Stage5 回归检查。
- 通过：保持 60 Hz 生产默认同步性能、latency <=80 ms、sourceSeq 连续、
  input overflow=0、TCP overwrite=0、录像丢帧=0。
- 剩余说明：启动/冷缓存区间仍偶发 sourceSeqLag/sourceQueue 峰值，但未观察到
  稳态持续积压。

下一步：
- 在 component 日志跨更多场景稳定前，不接入 MODTRAN path/sky/solar runtime。
- 下一步物理工作可以标定 body/reflected/path 分量，或设计真正的 MODTRAN path
  数据源；H.264/硬件编码仍作为单独传输项目处理。

```

### 阶段 3B：MODTRAN path/sky/solar radiance 运行时数据源与只记录对比链路

```text
阶段：3B
日期：2026-06-16
执行者：Codex
目标：为 MODTRAN path/sky/solar radiance 建立运行时数据源和只记录对比链路，
默认不改变生产画面。

本阶段变更：
- 新增 IRModtranRadianceLut，用于从 band_lut.csv 加载运行时 LUT 数据。
- 初始化或配置变化时一次性加载 path_radiance_band、sky_radiance_band、
  solar_irradiance_band；禁止每帧读取 CSV。
- 查询输入覆盖 band、目标距离、观测高度、目标高度、能见度、湿度和太阳天顶角。
- 当前采用 nearest_neighbor 查询，并显式输出 valid/fallback 状态。
- 增加按 target/band/bucket 组织的查询缓存，避免 60 Hz 下重复昂贵查询。
- IRRadianceComponents 增加 legacyPathRadiance、modtranPathRadiance、
  modtranSkyRadiance、modtranSolarIrradiance、modtranRadianceValid、
  modtranFallbackReason、modtranInterpolationMode。
- 新增 [Stage5ModtranRadiance] 配置：
  EnableModtranRadianceDebug=true、UseModtranPathRuntime=false、
  UseModtranSkyRuntime=false、UseModtranSolarRuntime=false、
  PreferredSource=band_lut、LogEveryFrames=120、CompareLegacy=true。
- 新增限频日志 [Stage5 ModtranRadianceCompare]。
- [Perf] 增加 stage5ModtranLookupMs、stage5ModtranCacheHitCount、
  stage5ModtranCacheMissCount。
- 新增 tools/stage5_modtran_radiance_compare_smoke.ps1，并扩展 Stage5
  静态检查以覆盖 MODTRAN radiance compare 链路。

未改变：
- 生产默认 pathRadianceSource 仍为 legacy_empirical。
- UseModtranPathRuntime 默认 false，MODTRAN path 只用于日志对比，不改变最终像素。
- UseModtranSkyRuntime / UseModtranSolarRuntime 本阶段仍为 log-only。
- 不做 AGC/MTF、高度/Mach 热模型、材质库结构改造、TCP/JPEG/H.264 协议改动、
  录像旁路改动、PBO 或硬件编码。
- 保留阶段 2B 的 shader input cache、Stage7 dirty-key、Stage4 skip 优化。

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余警告为既有 VS/Panda/PDB 警告。

回归测试：
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段语义 smoke：通过。
  日志：logs/stage4/HwaSimIR-stage4-hotspot-smoke-20260616-231137.out.log
- Stage5 min radiance 静态检查：通过。
- Stage5 radiance components smoke：通过。
  摘要：logs/stage5/stage5_radiance_components_smoke_summary.csv
- Stage5 MODTRAN radiance compare smoke：通过。
  CSV：logs/stage5/modtran_radiance_compare_summary.csv
  Markdown：logs/stage5/modtran_radiance_compare_summary.md

MODTRAN 对比 smoke 摘要：
- MWIR 在 visibility=5/15/30 km 下代表性样本 valid=1，
  interpolationMode=nearest_neighbor，pathRadianceSource 保持 legacy_empirical。
- MWIR 示例：
  rangeKm=5.00782、obsAltKm=10、tgtAltKm=10、visibilityKm=5，
  legacyPath=0.0401838、modtranPath=2.27886e-09、modtranSolar=1.50357e-06。
  rangeKm=21.2088、obsAltKm=10、tgtAltKm=3、visibilityKm=30，
  legacyPath=0.0401838、modtranPath=1.75622e-08、modtranSolar=1.50357e-06。
- NIR solar 可查到：
  rangeKm=20.0313、visibilityKm=15、modtranSolar=7.46197e-06、
  modtranSky=6.60839e-09、valid=1。
- 当前 LUT 中 SWIR 明确 fallback：
  valid=0、fallbackReason=missing_band。
- MWIR 越界查询明确 fallback：
  rangeKm=80.1245、valid=0、fallbackReason=out_of_lut_range。
- UseModtranPathRuntime=false 时，smoke 中没有任何行报告
  pathRadianceSource=modtran_runtime。
- 该 smoke 会发送要求的 MWIR range/altitude/visibility 组合，但当前运行时日志
  每次捕获的是代表性已处理样本，并非完整矩阵的每个包。若后续需要严格数值矩阵覆盖，
  建议补一个直接 LUT 单元 smoke 或每个 case 单独进程的 smoke。

30 秒生产默认同步测试：
- 配置：800x800、5 目标、videoFps=60、saveMP4En=true、
  Codec=auto、JpegEncodeMode=rgb、JpegQuality=100、
  EnableH264Experimental=false、LegacyEngineBodyHeating=false、
  EnableIRPhysicalPipeline=true、DebugView=Off、UseModtranPathRuntime=false。
- 日志：logs/phase2a-final-20260616-232612
- MP4：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260616_232626/output.mp4
- sentFps=60.033、udpFps=60.253、renderFps=60.513、
  outputFps=60.364、VideoDisplay receive/display=60.429。
- latencyAvgMs=33.576。
- sourceSeqContinuous=1、sourceSeqContinuousWritten=1。
- inputQueueOverflow=0、TCP overwritten=0、recordingDroppedFrames=0。
- written/mp4/annotations/targetAnnotations=1799/1799/1799/1799。
- sourceSeqLagMax=11、inputQueueDepthMax=16 来自启动/追帧瞬态；
  后段稳态 SyncFrame/Perf 样本显示 inputQueueDepth 多数为 1..2，
  sourceSeqLag 为 0..2，没有持续积压。

与阶段 3A 的性能对比：
- 阶段 3A：irUpdateMs=0.985、shaderInputApplyMs=0.657、
  stage7SkyGroundMs=0.219、stage4HotspotMs=1.313、latencyAvgMs=34.324。
- 阶段 3B：irUpdateMs=1.667、shaderInputApplyMs=0.691、
  stage7SkyGroundMs=0.171、stage4HotspotMs=2.852、latencyAvgMs=33.576。
- 后段 [Perf] 中 stage5ModtranLookupMs 约 0.009..0.030 ms，可见且不是瓶颈。
- stage5ModtranCacheHitCount/MissCount 已输出；后段样本为数百次命中、个位数 miss。
- shaderInputCacheHitRateAvg 仍保持 97.336%。
- Stage7/Stage4 未回退到每帧全量更新。

验收结论：
- 通过：MODTRAN radiance 运行时数据源和只记录对比链路。
- 通过：生产默认画面不变，pathRadianceSource 仍为 legacy_empirical。
- 通过：Stage3/Stage4/Stage5 回归测试。
- 通过：保持 60 Hz、latency <=80 ms、sourceSeq 连续、input overflow=0、
  TCP overwrite=0、录像丢帧=0。
- 剩余说明：自动验收脚本仍能看到启动/追帧期的 sourceSeqLag/inputQueueDepth 峰值；
  稳态会回到接近 0/低个位数，未观察到 3B MODTRAN lookup 导致的持续瓶颈。

下一步：
- UseModtranPathRuntime 继续保持 false，等单位和数值标定与 legacy empirical path
  对齐后再考虑接入图像。
- 如需严格覆盖 5/20/50 km、3/10/20 km、5/15/30 km 全矩阵，建议补直接 LUT
  单元 smoke 或每 case 独立进程 smoke。
- H.264 实时传输、AGC/MTF、高度/Mach 气动加热仍应作为独立后续阶段。

### 阶段 3B-Fix：生产配置与 MODTRAN radiance compare 彻底分离

执行日期：2026-06-17
执行者：Codex
目标：定位人工 5 目标运行约 40 FPS 的配置/路径差异，确认生产默认渲染不启用
MODTRAN path/sky/solar runtime，也不默认执行 MODTRAN radiance compare 查询。

根因判断：
- 阶段 3B 结束时，生产 ini 和代码默认仍保留
  EnableModtranRadianceDebug=true、CompareLegacy=true。
- UseModtranPathRuntime=false 确实保证了 MODTRAN path 不改变最终像素，
  但 compare/debug 打开时，Stage5 radiance components 路径仍会按目标执行
  MODTRAN radiance lookup 和 compare 日志统计。
- 该路径在自动验收中平均耗时很小，但手动运行若叠加 debug/log/旧配置或工作目录差异，
  容易被误判为生产渲染正在使用 MODTRAN runtime，也会增加排查噪声。

本阶段修正：
- 生产默认改为：
  EnableModtranRadianceDebug=false、CompareLegacy=false、
  UseModtranPathRuntime=false、UseModtranSkyRuntime=false、
  UseModtranSolarRuntime=false。
- 代码默认同步改为 compare/debug 关闭；只有显式 debug/smoke override 或 runtime
  开关打开时，Stage5 才查询 MODTRAN radiance LUT。
- 新增 Stage5ModtranCompareEffective 口径，统一判断 compare/runtime 是否实际生效。
- 新增 [EffectiveRuntimeConfig]，启动、init 后、第一帧 display 后打印有效运行配置：
  videoFps、syncMode、targetNumValid、IRUpdateHz、AnnotationUpdateHz、
  EnablePerfLog、EnableIRVerboseLog、EnableIRPhysicalPipeline、DebugView、
  LogComponents、EnableModtranRadianceDebug、UseModtranPathRuntime、
  UseModtranSkyRuntime、UseModtranSolarRuntime、CompareLegacy、
  Stage5ModtranCompareEffective、Stage7UpdateHz、Stage4UpdateHz、
  Stage5PlumeUpdateHz、Codec、JpegEncodeMode、JpegQuality、
  EnableH264Experimental、JpegPerfABTest、saveMP4En 和配置来源。
- [EffectiveRuntimeConfig][WARN] 会在生产风险开关打开时报警：
  DebugView != Off、LogComponents=true、EnableIRVerboseLog=true、
  任意 UseModtran*Runtime=true、JpegPerfABTest=true、
  Stage5ModtranCompareEffective=true。
- [Perf] 增加 stage5RadianceComponentMs，并保留 stage5ModtranLookupMs、
  stage5ModtranCacheHitCount、stage5ModtranCacheMissCount。
- [Perf][WARN] bottleneck summary 只在 60 Hz 目标下出现实际低帧/明显 backlog 时输出，
  避免启动第一帧或停机尾窗制造假瓶颈。
- 新增 tools/runtime_config_check.ps1，检查生产 ini 不允许打开
  DebugView/LogComponents/Verbose/UseModtran*Runtime/CompareLegacy/
  EnableModtranRadianceDebug/JpegPerfABTest/EnableH264Experimental/
  LegacyEngineBodyHeating。
- 修改 tools/stage5_modtran_radiance_compare_smoke.ps1：
  smoke 通过进程环境变量临时打开 MODTRAN compare/debug，
  运行前备份生产 ini，结束或失败都恢复。
- 修改 tools/phase2a_sync60_save_smoke.ps1：
  30 秒生产默认验收强制关闭 MODTRAN compare/debug/runtime，并在 summary 中输出
  stage5RadianceComponentMs、stage5ModtranLookupMs 和 cache hit/miss。
- 新增 tools/phase3b_fix_manual_equiv_smoke.ps1：
  自动执行生产配置检查 + 30 秒 60 Hz/saveMP4En=true 验收，
  并解析 [EffectiveRuntimeConfig]、stage5ModtranLookupMs、人工差异检查清单。

生产 EffectiveRuntimeConfig 摘要：
- reason=first_display
- runtimeConfigPath=Config/HwaSimIRRuntime.ini、runtimeConfigLoaded=1
- videoFps=60、syncMode=1、targetNumValid=5、saveMP4En=1
- EnableIRVerboseLog=0、EnableIRPhysicalPipeline=1、DebugView=Off、
  LogComponents=0
- EnableModtranRadianceDebug=0、UseModtranPathRuntime=0、
  UseModtranSkyRuntime=0、UseModtranSolarRuntime=0、CompareLegacy=0、
  Stage5ModtranCompareEffective=0
- Codec=auto、JpegEncodeMode=rgb、JpegQuality=100、
  EnableH264Experimental=0、JpegPerfABTest=0
- effectiveRuntimeConfigWarnCount=0

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余警告为既有 PDB/qtmain 调试符号警告。

回归测试：
- runtime_config_check.ps1：通过。
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段语义 smoke：通过。
- Stage5 min radiance 静态检查：通过。
- Stage5 radiance components smoke：通过。
- Stage5 MODTRAN radiance compare smoke：通过。
- compare smoke 结束后再次运行 runtime_config_check.ps1：通过，确认生产 ini 已恢复。

30 秒生产默认同步测试：
- 配置：800x800、5 目标、videoFps=60、saveMP4En=true、
  Codec=auto、JpegEncodeMode=rgb、JpegQuality=100、
  EnableH264Experimental=false、LegacyEngineBodyHeating=false、
  EnableIRPhysicalPipeline=true、DebugView=Off、UseModtranPathRuntime=false。
- 日志：logs/phase2a-final-20260617-110225
- MP4：HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260617_110239/output.mp4
- sentFps=60.033、udpFps=59.935、renderFps=60.311、
  outputFps=60.142、VideoDisplay receive/display=60.313。
- latencyAvgMs=35.105。
- stage5ModtranLookupMsAvg=0、stage5ModtranLookupMsMax=0。
- stage5RadianceComponentMsAvg=0.379296。
- sourceSeqContinuous=1、sourceSeqContinuousWritten=1。
- inputQueueOverflow=0、TCP overwritten=0、recordingDroppedFrames=0。
- written/mp4/annotations/targetAnnotations=1799/1799/1799/1799。
- sourceSeqLagMax=7、inputQueueDepthMax=13 为启动/追帧瞬态；
  后段稳态 [Perf] 显示 sourceSeqLag 多数 0..2、inputQueueDepth 0..1。
- 最新 [Perf][WARN] 只剩一条真实短窗口：
  outputFps=58.679、sourceSeqLag=4、stage5ModtranLookupMs=0；
  后续窗口恢复到 60 Hz，无持续积压。

结论：
- 已确认生产默认不使用 MODTRAN path/sky/solar runtime。
- 已确认生产默认不执行 MODTRAN radiance compare lookup：
  Stage5ModtranCompareEffective=0，stage5ModtranLookupMs=0。
- 自动 manual-equivalent 30 秒验收保持 60 Hz、latency <=80 ms、
  sourceSeq 连续、录像 0 丢帧。
- 若人工仍出现约 40 FPS，应优先按脚本输出的人工差异清单检查：
  exe 是否为 Release、工作目录是否为 HwaSim_IR/Bin、runtimeConfigPath 是否正确、
  是否有旧进程残留、DebugView/LogComponents/Verbose/JpegPerfABTest/h264 是否误开、
  VideoDisplay 是否阻塞或控制台是否有高频日志。

下一步：
- 不进入 3C；继续保持 UseModtranPathRuntime=false。
- 人工实测若仍掉到 40 FPS，请直接贴 [EffectiveRuntimeConfig]、[Perf]、
  [VideoPerf]、[RecorderPerf] 和 phase3b_fix_manual_equiv_summary.json，
  先比对运行目录/配置路径/旧进程，而不是继续加红外物理功能。
```

---

### 阶段 3C：MWIR MODTRAN path_radiance runtime A/B 试验

```text
阶段：3C
日期：2026-06-17
执行者：Codex
目标：仅针对 MWIR 建立 MODTRAN path_radiance runtime A/B 试验链路，重点解决单位/尺度和接入位置问题；
生产默认仍不启用 MODTRAN path runtime，不接 sky/solar runtime，不改变 TCP/JPEG/H.264/录像链路。

本阶段变更：
- 在 [Stage5ModtranRadiance] 下新增 MWIR path runtime A/B 配置：
  UseModtranPathRuntime=false
  ModtranPathRuntimeBand=MWIR
  ModtranPathRuntimeMode=Off
  ModtranPathUnitMode=Native
  ModtranPathScale=1.0
  ModtranPathOffset=0.0
  ModtranPathClampMin=0.0
  ModtranPathClampMax=10.0
  ModtranPathBlend=1.0
  ModtranPathABLog=true
- Runtime mode 支持 Off、CompareOnly、ReplaceLegacy、BlendLegacy。
- ReplaceLegacy / BlendLegacy 仅允许 MWIR；非 MWIR 自动 fallback legacy，并限频 WARN。
- IRRadianceComponents / IRRadianceModelV2 增加：
  modtranPathRaw、modtranPathScaled、modtranPathUnitMode、modtranPathScale、
  modtranPathOffset、modtranPathBlend、modtranPathRuntimeMode、
  effectivePathRadiance、sensorInputLegacy、sensorInputModtran。
- sensorInputRadiance 接入方式：
  legacy: tauUp * surfaceRadiance + legacyPath
  ReplaceLegacy: tauUp * surfaceRadiance + modtranPathScaled
  BlendLegacy: tauUp * surfaceRadiance + lerp(legacyPath, modtranPathScaled, blend)
- pathRadianceSource 明确区分：
  legacy_empirical、modtran_runtime_scaled、modtran_runtime_blend、fallback_legacy。
- 新增限频日志 [Stage5 ModtranPathAB]，输出 raw/scaled/effective path、
  legacy/modtran/effective sensorInput 和 fallback reason。
- 优化 MODTRAN compare / path A/B 日志函数：非前 3 帧、非采样帧、非 verbose 时直接返回，
  避免没有实际打印时仍每帧构造字符串状态，降低 A/B 路径 stage5RadianceComponentMs。
- 新增 tools/stage5_modtran_path_runtime_ab_smoke.ps1：
  1) 直接 LUT 单元标定；
  2) 运行 Off / CompareOnly / BlendLegacy / ReplaceLegacy 10 秒 A/B；
  3) 输出 runtime CSV，并在运行后确认生产 ini 恢复。
- runtime_config_check.ps1 增加 3C 生产默认检查。
- phase2a_sync60_save_smoke.ps1 增加 3C runtime override 和 summary 字段。
- phase3b_fix_manual_equiv_smoke.ps1 增加 3C EffectiveRuntimeConfig 检查和真实性能阈值；
  不再在 FPS/延时未达标时误报通过。

未改变：
- 生产默认仍为 UseModtranPathRuntime=false、ModtranPathRuntimeMode=Off。
- EnableModtranRadianceDebug=false、CompareLegacy=false，生产默认不执行 MODTRAN radiance compare lookup。
- 不接 sky/solar runtime，不做 AGC/MTF，不做高度/Mach，不改材质库结构。
- 不改 TCP/JPEG/H.264 协议，不改录像旁路，不做 PBO/硬件编码。

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay 本阶段未改代码，沿用阶段 3B-Fix 已通过 Release 产物。
- 剩余 warning 为既有 VS/PDB/未用变量 warning。

回归测试：
- runtime_config_check.ps1：通过。
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段语义 smoke：通过。
- Stage5 radiance components smoke：通过。
- Stage5 MODTRAN radiance compare smoke：通过，运行后 production ini 恢复。
- Stage5 MODTRAN path runtime A/B smoke：功能通过，但严格队列峰值 guard 仍有 warning。

MODTRAN path 单元标定：
- CSV：logs/stage5/modtran_path_runtime_ab_summary.csv
- MWIR suggestedScaleToLegacy 正值样本数：63
- suggestedScaleToLegacy 范围：
  min=4.22660789e5
  avg=4.452536046e6
  max=1.1648836548e7
- 结论：legacyPath 与 MODTRAN path_radiance 原始 LUT 值仍存在 5e5 到 1e7 量级差异。
  因此本阶段不建议直接把 ReplaceLegacy 作为默认，也不建议用未标定 scale 进入生产。

Runtime A/B 结果（10 秒，scale=1.0，blend=0.25，saveMP4En=true）：
- Off：
  sent=60.113, udp=59.970, render=59.359, output=58.514,
  VideoDisplay=59.028, latencyAvg=106.057 ms,
  stage5ModtranLookupMs=0, pathRadianceSource=not_logged。
  该短测未达 60 Hz/latency 阈值，主要是短测窗口和队列启动峰值影响。
- CompareOnly：
  sent=60.116, udp=60.778, render=61.176, output=60.959,
  VideoDisplay=61.230, latencyAvg=70.613 ms,
  stage5ModtranLookupMs=0.008, stage5RadianceComponentMs=1.332,
  pathRadianceSource=legacy_empirical。
- BlendLegacy：
  sent=60.114, udp=60.570, render=60.840, output=60.350,
  VideoDisplay=60.859, latencyAvg=58.233 ms,
  stage5ModtranLookupMs=0.007286, stage5RadianceComponentMs=1.224,
  pathRadianceSource=modtran_runtime_blend。
- ReplaceLegacy：
  sent=60.113, udp=59.842, render=61.291, output=59.737,
  VideoDisplay=59.773, latencyAvg=59.967 ms,
  stage5ModtranLookupMs=0.008, stage5RadianceComponentMs=1.284,
  pathRadianceSource=modtran_runtime_scaled。
- A/B 结论：
  CompareOnly、BlendLegacy、ReplaceLegacy 的 FPS 和延时可保持 60 Hz 级别；
  但三组均出现 sourceSeqLagMax/inputQueueDepthMax 峰值 warning，不作为默认推荐依据。
  ReplaceLegacy 使用 scale=1.0 时数值影响极小，不能代表已完成物理尺度标定。

30 秒生产默认实测：
- 配置：800x800，5 目标，videoFps=60，saveMP4En=true，
  Codec=auto，JpegEncodeMode=rgb，JpegQuality=100，
  EnableH264Experimental=false，LegacyEngineBodyHeating=false，
  EnableIRPhysicalPipeline=true，DebugView=Off，
  UseModtranPathRuntime=false，ModtranPathRuntimeMode=Off。
- 日志：logs/phase2a-final-20260617-155244
- MP4：
  HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260617_155258/output.mp4
- sentFps=60.032
- udpFps=60.011
- renderFps=60.347
- outputFps=60.177
- VideoDisplay receive/display=60.350
- latencyAvgMs=42.627
- stage5ModtranLookupMs=0
- stage5RadianceComponentMs=0.761
- sourceSeqContinuous=1
- sourceSeqContinuousWritten=1
- inputQueueOverflow=0
- TCP overwritten=0
- recordingDroppedFrames=0
- written/mp4/annotations/targetAnnotations=1799/1799/1799/1799
- 严格 guard：
  sourceSeqLagMax=4、inputQueueDepthMax=16，因此 phase3b_fix_manual_equiv_smoke.ps1
  当前会按严格队列峰值阈值报失败。
  但 [Perf] 稳态窗口显示 sourceSeqLag 多数为 1..2，inputQueueDepth 多数为 0..2，
  仅短窗口 spike 到 sourceSeqLag=4、inputQueueDepth=3；未观察到持续积压。

结论：
- 已确认生产默认不使用 MODTRAN path runtime：
  UseModtranPathRuntime=0、ModtranPathRuntimeMode=Off、
  Stage5ModtranCompareEffective=0、stage5ModtranLookupMs=0。
- 生产默认 30 秒主链路达到 60 Hz、latency <=80 ms、sourceSeq 连续、录像 0 丢帧。
- A/B runtime 能跑通，但单位/尺度尚未可信标定，且队列峰值 guard 仍有 warning。
- 不建议下一阶段把 MWIR path runtime 小比例设为默认候选；
  建议继续保持 Off，并先做单位归一化、legacy empirical path 对齐和视觉/数值评审。
- 如果要推进，应单独立项“MWIR MODTRAN path scale calibration”，不要与 sky/solar、AGC/MTF 或高度/Mach 同时混入。
```

---

### 阶段 4A：高度/Mach 气动加热模型骨架与 Stage5 log-only 接入

```text
阶段：4A
日期：2026-06-18
执行者：Codex
目标：建立目标高度 3~20 km、速度 0.5~3 Mach 对目标自身红外辐射影响的气动加热模型骨架。
默认生产画面不突变；先做 log-only / A-B 验证和 Stage5 components 接入。
不推进 MODTRAN path runtime 默认化，不接 sky/solar runtime，不做 AGC/MTF，不改 TCP/JPEG/H.264/录像链路，不改材质库结构。

本阶段变更：
- 新增 IRAeroThermalModel，输入 target altitude、target speed、dt、band、target/platform type。
- speedUnit 明确为 km/h，运行时换算 speedMps = speedRaw / 3.6。
- 实现 0~20 km 简化 ISA 空气温度与声速：
  0~11 km 使用 288.15 - 0.0065 * altitudeM，11~20 km 使用 216.65 K。
- 实现恢复温度模型：
  recoveryTempK = airTempK * (1 + recoveryFactor * (gamma - 1) / 2 * Mach^2)
  aeroDeltaK = clamp(max(0, recoveryTempK - airTempK), 0, ClampDeltaKMax)
- 默认参数：
  RecoveryFactor=0.85，Gamma=1.4，
  BodyCoeff=0.20，NoseCoeff=0.60，EdgeCoeff=0.45，RearCoeff=0.10，
  HeatTauSec=2.0，CoolTauSec=5.0，ClampMachMax=4.0，ClampDeltaKMax=250.0。
- 增加 per-target 平滑状态，速度/高度变化时按 heat/cool tau 平滑 body/nose/edge/rear delta。
- 对非法 altitude/speed 做 fallback=no_aero_heating；接入层把非有限或极小 denormal altitude 归一到 0，避免首帧日志出现未初始化味道的 1e-316。
- IRRadianceComponents 增加：
  altitudeM、speedMps、mach、airTempK、recoveryTempK、aeroDeltaK、
  bodyAeroDeltaK、noseAeroDeltaK、edgeAeroDeltaK、rearAeroDeltaK、
  aeroValid、aeroFallbackReason、aeroAppliedToRadiance。
- ApplyAeroToRadiance=false 时只写 components 和限频日志，不改变 bodyRadiance/最终图像。
- ApplyAeroToRadiance=true 时仅允许 bodyAeroDeltaK 进入 body material temperature offset；
  不影响 EngineRear、EnginePlume、BrightSpot，不做 nose/edge per-pixel mask。
- [Stage5 AeroThermal] 限频日志输出 altitude/speed/Mach/airTemp/recoveryTemp/aeroDelta/body/nose/edge/rear delta。
- [Perf] 增加 stage5AeroThermalMs。
- [EffectiveRuntimeConfig] 增加 EnableAeroThermalModel、ApplyAeroToRadiance、AeroDebugLog、RecoveryFactor、ClampDeltaKMax。
- runtime_config_check.ps1 增加生产默认保护：
  ApplyAeroToRadiance=false、AeroDebugLog=false、UseModtranPathRuntime=false、ModtranPathRuntimeMode=Off。
- 新增 tools/stage5_aero_thermal_smoke.ps1：
  输出 logs/stage5/aero_thermal_summary.csv，覆盖 3/10/20 km × Mach 0.5/1/2/3。
- 新增 tools/phase4a_aero_ab_smoke.ps1：
  运行 ApplyAeroToRadiance=false/true A-B，输出 logs/stage5/aero_runtime_ab_summary.csv。
  10 秒窗口若遇到编码/系统抖动导致误判，会自动用 RetrySeconds 重跑该 case。
- phase2a_sync60_save_smoke.ps1 增加 aero override 和 summary 字段，并把 summary 路径改为 Write-Output，方便上层脚本捕获。

生产默认保护：
- EnableAeroThermalModel=true
- ApplyAeroToRadiance=false
- AeroDebugLog=false
- UseModtranPathRuntime=false
- ModtranPathRuntimeMode=Off
- EnableModtranRadianceDebug=false
- CompareLegacy=false
- DebugView=Off
- LogComponents=false
- EnableIRVerboseLog=0
- JpegEncodeMode=rgb
- JpegQuality=100

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余 warning 为既有 math_algorithm.h 未引用变量、VS/PDB/qtmain 调试符号 warning。

回归测试：
- runtime_config_check.ps1：通过。
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段语义 smoke：通过。
- Stage5 min radiance strict：通过。
- Stage5 radiance components smoke：通过。
- Stage5 MODTRAN radiance compare smoke：通过，运行后 production ini 恢复。
- Stage5 aero thermal smoke：通过。
- Stage5 MODTRAN path runtime A/B：保持阶段 3C 结论，不默认启用。

Stage5 aero thermal CSV：
- 路径：logs/stage5/aero_thermal_summary.csv
- 趋势：同高度下 Mach 越高，aeroDeltaK 越大；同 Mach 下 3 km 空气温度高于 10/20 km；
  Mach=3 在当前 ClampDeltaKMax=250 K 下触顶，body/nose/edge/rear delta 分别为 50/150/112.5/25 K。

代表性标定结果：
- 3 km / Mach 0.5：airTempK=268.65，speedOfSoundMps=328.578，aeroDeltaK=11.418，body=2.284，nose=6.851，edge=5.138，rear=1.142。
- 3 km / Mach 1：aeroDeltaK=45.671，body=9.134，nose=27.402，edge=20.552，rear=4.567。
- 3 km / Mach 2：aeroDeltaK=182.682，body=36.536，nose=109.609，edge=82.207，rear=18.268。
- 3 km / Mach 3：aeroDeltaK=250.000 clamp，body=50.000，nose=150.000，edge=112.500，rear=25.000。
- 10 km / Mach 0.5：airTempK=223.15，speedOfSoundMps=299.463，aeroDeltaK=9.484，body=1.897。
- 10 km / Mach 1：aeroDeltaK=37.936，body=7.587。
- 10 km / Mach 2：aeroDeltaK=151.742，body=30.348。
- 10 km / Mach 3：aeroDeltaK=250.000 clamp，body=50.000。
- 20 km / Mach 0.5：airTempK=216.65，speedOfSoundMps=295.069，aeroDeltaK=9.208，body=1.842。
- 20 km / Mach 1：aeroDeltaK=36.831，body=7.366。
- 20 km / Mach 2：aeroDeltaK=147.322，body=29.464。
- 20 km / Mach 3：aeroDeltaK=250.000 clamp，body=50.000。

Runtime A/B：
- CSV：logs/stage5/aero_runtime_ab_summary.csv
- ApplyAeroToRadiance=false，10 秒：
  sent=60.086，udp=59.910，render=60.876，output=60.662，VideoDisplay=60.539，
  latencyAvg=25.290 ms，irUpdateMs=1.391，stage5RadianceComponentMs=1.361，
  stage5AeroThermalMs=0.0052，stage5ModtranLookupMs=0，recordingDroppedFrames=0。
- ApplyAeroToRadiance=true，10 秒：
  sent=60.074，udp=59.826，render=60.640，output=60.189，VideoDisplay=60.594，
  latencyAvg=22.901 ms，irUpdateMs=1.329，stage5RadianceComponentMs=1.327，
  stage5AeroThermalMs=0.004833，stage5ModtranLookupMs=0，recordingDroppedFrames=0。
- 注意：当前 DataDrivenTestQT 自动 5 目标场景 speedRaw=0 km/h，因此 Runtime A/B 只验证开关接入和性能；
  bodyAeroDeltaKAvg=0、machAvg=0，不代表已完成非零 Mach 画面主观/物理标定。
- 曾观察到一次 10 秒 ApplyAeroToRadiance=true 的编码/渲染侧抖动：
  stage5AeroThermalMs 仍为 0.006 ms，但 jpegMs/renderMs 瞬时升到约 22 ms，导致 outputFps 掉到约 47。
  单独 20 秒复测通过：sent=59.997，udp=60.010，render=60.385，output=60.327，VideoDisplay=60.385，
  latencyAvg=21.358 ms，recordingDroppedFrames=0。

30 秒生产默认实测：
- 日志：logs/phase2a-final-20260618-102847
- MP4：
  HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260618_102902/output.mp4
- 配置：800x800，5 目标，videoFps=60，saveMP4En=true，
  Codec=auto，JpegEncodeMode=rgb，JpegQuality=100，
  EnableH264Experimental=false，LegacyEngineBodyHeating=false，
  EnableIRPhysicalPipeline=true，DebugView=Off，
  UseModtranPathRuntime=false，ModtranPathRuntimeMode=Off，
  ApplyAeroToRadiance=false。
- sentFps=60.016
- udpFps=60.099
- renderFps=60.185
- outputFps=60.181
- VideoDisplay receive/display=60.248
- latencyAvgMs=19.541
- irUpdateMs=0.691
- stage5RadianceComponentMs=0.425
- stage5AeroThermalMs=0.003875
- stage5ModtranLookupMs=0
- shaderInputCacheHitRate=98.201
- sourceSeqContinuous=1
- sourceSeqContinuousWritten=1
- inputQueueOverflow=0
- TCP overwritten=0
- recordingDroppedFrames=0
- written/mp4/annotations/targetAnnotations=1801/1801/1801/1801
- sourceSeqLagMax=6、inputQueueDepthMax=10 为启动瞬态；后段 [Perf] 稳态 sourceSeqLag=0..1、inputQueueDepth=0..1。

结论：
- 已确认生产默认画面不改变：ApplyAeroToRadiance=false，AeroDebugLog=false，DebugView=Off，LogComponents=false。
- 已确认生产默认不使用 MODTRAN path/sky/solar runtime，也不执行 MODTRAN radiance compare lookup：
  UseModtranPathRuntime=false，UseModtranSkyRuntime=false，UseModtranSolarRuntime=false，
  Stage5ModtranCompareEffective=0，stage5ModtranLookupMs=0。
- 气动模型计算成本可忽略，生产默认 stage5AeroThermalMs 约 0.004 ms。
- 不建议下一阶段直接把 bodyAeroDeltaK 进入默认画面。
  建议先增加一个非零 speed/Mach 的运行态刺激或人工场景，做 bodyAeroDeltaK 小比例画面对比和物理审查；
  默认仍保持 ApplyAeroToRadiance=false。
```

---

### 阶段 4B：真实 UDP 速度字段链路核查与非零速度气动加热运行态 A/B

```text
阶段：4B
日期：2026-06-18
执行者：Codex
目标：确认 RedSpeedAir(km/h)、MissileSpeedAir(km/h) 等真实输入速度进入
DataDrivenTestQT UDP 发送、HwaSim_IR TargetState、IRAeroThermalModel 和 Stage5 components。
生产默认仍保持 ApplyAeroToRadiance=false；不启用 MODTRAN path 默认化，不接 sky/solar runtime，
不做 AGC/MTF，不改 TCP/JPEG/H.264/录像链路，不改材质库结构。

本阶段变更：
- DataDrivenTestQT 在 sendRealTimeData() 中把真实样本速度写入 UDP targetLoc.speed：
  - 红方/飞机目标使用 RedSpeedAir(km/h)。
  - 导弹/武器目标使用 MissileSpeedAir(km/h)。
  - 不改变 UDP 协议结构，只填充已有 speed 字段。
- 新增 [AeroSpeedSend] 限频日志，输出 sourceSeq、targetIndex、targetID、targetType、
  speedSourceColumn、speedRawKmh、altitudeM、lat/lon。
- HwaSim_IR 在 UDP 接收后新增 [AeroSpeedRecv] 限频日志，确认 receivedSpeedRaw、
  receivedAltitudeM、targetLoc.speed、speedUnit=km/h。
- HwaSim_IR 在目标状态映射后新增 [AeroSpeedState] 限频日志，输出 renderVisible、
  targetState.targetLoc.speed、selectedSpeedKmh、selectedSpeedSource。
- Stage5 AeroThermal 日志和 components 增加：
  selectedSpeedSource、speedRawKmh、speedMps、mach、bodyAeroDeltaKRaw、
  bodyAeroDeltaKEffective、bodyTempBaseK、bodyTempAeroAppliedK、
  bodyRadianceNoAero、bodyRadianceWithAero、sensorInputNoAero、
  sensorInputWithAero、aeroRadianceRatio。
- 新增小比例接入配置：
  AeroApplyScale=0.25，AeroApplyClampBodyDeltaK=40.0，AeroApplyOnlyBand=MWIR。
  ApplyAeroToRadiance=true 时仅 MWIR 把
  clamp(bodyAeroDeltaKRaw * AeroApplyScale, 0, AeroApplyClampBodyDeltaK)
  作为 body 温度偏置；不影响 EngineRear / EnginePlume / BrightSpot。
- 对 ApplyAeroToRadiance=true 的有效 body aero delta 做 0.05 K 量化，
  避免真实低速样本中 0.00x K 的逐帧微小变化破坏 shader input cache。
- 日志限频进一步收紧为前 3 帧 + 每 600 帧，状态变化使用速度/高度 bucket，
  避免真实 60 Hz 下控制台 IO 影响主链路。
- 新增 tools/phase4b_udp_speed_chain_smoke.ps1：
  运行真实 DataDrivenTestQT 发送路径，检查 Send/Recv/State/Stage5 速度链路，
  输出 logs/stage5/aero_speed_chain_summary.csv。
- 新增 tools/phase4b_aero_runtime_speed_ab.ps1：
  使用真实 UDP 速度数据运行 ApplyAeroToRadiance=false/true A/B，
  输出 logs/stage5/aero_runtime_speed_ab_summary.csv。
- phase2a_sync60_save_smoke.ps1 增加 AeroApplyScale、AeroApplyClampBodyDeltaK、
  AeroApplyOnlyBand override 和速度/A-B summary 字段。
- runtime_config_check.ps1 增加 AeroApplyScale、AeroApplyClampBodyDeltaK、
  AeroApplyOnlyBand 检查，并继续保护生产默认 ApplyAeroToRadiance=false。

构建：
- Windows Release x64 三工程构建通过。
- 剩余 warning 为既有 math_algorithm.h 未引用变量和 VS/PDB 调试符号 warning。

生产配置检查：
- runtime_config_check.ps1：通过。
- 关键生产默认：
  ApplyAeroToRadiance=false
  AeroDebugLog=false
  DebugView=Off
  LogComponents=false
  EnableIRVerboseLog=0
  UseModtranPathRuntime=false
  ModtranPathRuntimeMode=Off
  UseModtranSkyRuntime=false
  UseModtranSolarRuntime=false
  EnableModtranRadianceDebug=false
  CompareLegacy=false
  JpegEncodeMode=rgb
  JpegQuality=100

真实速度链路 smoke：
- 脚本：tools/phase4b_udp_speed_chain_smoke.ps1 -Seconds 10
- CSV：logs/stage5/aero_speed_chain_summary.csv
- 结果：通过。
- 精确匹配 OK 行：19 条。
- 未匹配/被并发控制台日志污染的诊断行：23 条，breakpoint=send_zero_or_missing，
  不参与通过判定。
- 代表性链路：
  sourceSeq=1，targetID=34，targetType=0x22，
  sendSpeedKmh=152.761，recvSpeedKmh=152.761，stateSpeedKmh=152.761，
  selectedSpeedSource=udp_targetLoc_speed，mach=0.124697，bodyAeroDeltaK=0.152338 K。
- 代表性链路：
  sourceSeq=2，targetID=3，targetType=0x22，
  sendSpeedKmh=152.802，recvSpeedKmh=152.802，stateSpeedKmh=152.802，
  altitudeM=10000.1，mach=0.141737，bodyAeroDeltaK=0.152347 K。
- 断点检查：未发现 send 非零后 recv/state/stage5 变 0 的真实链路断点。

非零速度运行态 A/B：
- 脚本：tools/phase4b_aero_runtime_speed_ab.ps1 -Seconds 20
- CSV：logs/stage5/aero_runtime_speed_ab_summary.csv
- ApplyAeroToRadiance=false：
  sent=60.023，udp=60.513，render=60.371，output=60.314，display=60.395，
  latencyAvg=20.502 ms，sourceSeqContinuous=1，recordingDroppedFrames=0，
  stage5AeroThermalMs=0.005273，stage5ModtranLookupMs=0，
  speedKmhAvg=172.813754，machAvg=0.153461，
  bodyAeroDeltaKRawAvg=0.187693，bodyAeroDeltaKEffectiveAvg=0，
  aeroRadianceRatio=1。
- ApplyAeroToRadiance=true：
  sent=60.053，udp=59.819，render=60.458，output=60.349，display=60.455，
  latencyAvg=22.435 ms，sourceSeqContinuous=1，recordingDroppedFrames=0，
  stage5AeroThermalMs=0.005545，stage5ModtranLookupMs=0，
  speedKmhAvg=172.468017，machAvg=0.152965，
  bodyAeroDeltaKRawAvg=0.187081，bodyAeroDeltaKEffectiveAvg=0.05，
  bodyRadianceNoAero=0.454807，bodyRadianceWithAero=0.455690，
  aeroRadianceRatio=1.001941。
- 说明：真实样例速度约 150~180 km/h，Mach 约 0.13~0.15；
  小比例接入后 body 辐射变化约 0.2%，画面变化弱是物理合理结果，不代表模型无效。

30 秒生产默认实测：
- 日志：logs/phase2a-final-20260618-165345
- MP4：
  HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260618_165400/output.mp4
- 配置：800x800，5 目标，videoFps=60，saveMP4En=true，
  Codec=auto，JpegEncodeMode=rgb，JpegQuality=100，
  EnableH264Experimental=false，LegacyEngineBodyHeating=false，
  EnableIRPhysicalPipeline=true，DebugView=Off，
  UseModtranPathRuntime=false，ModtranPathRuntimeMode=Off，
  ApplyAeroToRadiance=false。
- sentFps=60.025
- udpFps=60.079
- renderFps=60.242
- outputFps=60.082
- VideoDisplay receive/display=60.285
- latencyAvgMs=21.311
- irUpdateMs=0.827
- stage5RadianceComponentMs=0.653
- stage5AeroThermalMs=0.005588
- stage5ModtranLookupMs=0
- shaderInputCacheHitRate=96.45
- sourceSeqContinuous=1
- sourceSeqContinuousWritten=1
- inputQueueOverflow=0
- TCP overwritten=0
- recordingDroppedFrames=0
- written/mp4/annotations/targetAnnotations=1799/1799/1799/1799
- sourceSeqLagMax=7、inputQueueDepthMax=13 为启动/短窗瞬态；
  未观察到稳态积压。

结论：
- 已确认真实 UDP 速度链路打通：
  DataDrivenTestQT 发送非零 speedRawKmh，
  HwaSim_IR 接收非零 receivedSpeedRaw，
  TargetState 保留非零 selectedSpeedKmh，
  Stage5 AeroThermal 得到非零 Mach。
- 已确认生产默认画面仍不改变：
  ApplyAeroToRadiance=false，AeroDebugLog=false，DebugView=Off，LogComponents=false。
- 已确认生产默认不使用 MODTRAN path/sky/solar runtime：
  UseModtranPathRuntime=false，UseModtranSkyRuntime=false，UseModtranSolarRuntime=false，
  stage5ModtranLookupMs=0。
- ApplyAeroToRadiance=true A/B 在真实低速样本下保持 60 Hz，
  但画面差异很弱；不建议直接默认开启。
- 下一阶段建议优先做高 Mach 协议级测试，用 DataDrivenTestQT 按 altitude/Mach
  真实填充 speed 字段覆盖 Mach 0.5/1/2/3，验证气动加热可视范围；
  若用户更关注成像链路，再进入 MTF/blur。
```

---

### 阶段 4C：高 Mach 协议级测试与 MWIR 气动加热小比例候选评审

```text
阶段：4C
日期：2026-06-22
执行者：Codex
目标：通过 DataDrivenTestQT 按 altitude/Mach 真实填充 targetLoc.speed，
覆盖 3/10/20 km × Mach 0.5/1/2/3，验证 ApplyAeroToRadiance=true 时
bodyAeroDeltaK 对 bodyRadiance、sensorInputRadiance 和最终画面的影响。
生产默认仍保持 ApplyAeroToRadiance=false。

本阶段变更：
- DataDrivenTestQT 增加高 Mach 协议级测试模式：
  --phase4c-aero-mach
  --aero-alt-km=3/10/20
  --aero-mach=0.5/1/2/3
  --duration-sec=N
- DataDrivenTestQT 使用 0~20 km 简化 ISA 模型计算声速：
  speedMps = mach * speedOfSoundAtAltitude
  speedKmh = speedMps * 3.6
- 高 Mach 测试只写入已有 UDP 字段 targetLoc.speed 和 targetLoc.alt，
  不改变 UDP 协议结构，不在 HwaSim_IR 内部 fake speed。
- [AeroSpeedSend] 增加 phase4cAeroMach、altitudeKm、machCommand、
  speedKmh、speedMps、speedUnit=km/h。
- phase2a_sync60_save_smoke.ps1 支持 StimExtraArgs，供上层脚本透传
  DataDrivenTestQT 高 Mach 参数。
- phase2a FPS summary 改为只统计 >=58 FPS 的稳定窗口，
  避免短窗口测试被启动/停止后的 0/30/50 FPS Perf 尾段污染。
- 新增 tools/phase4c_aero_mach_protocol_ab.ps1：
  运行 3/10/20 km × Mach 0.5/1/2/3 × ApplyAeroToRadiance=false/true，
  每 case 8 秒，输出 logs/stage5/aero_mach_protocol_ab_summary.csv，
  并用 ffmpeg 从每个 case 的 MP4 中抽取代表帧到 logs/stage5/phase4c_frames。
- 脚本检查：
  sent/udp/render/output/display >=59.5，
  latencyAvgMs <=80，
  sourceSeqContinuous=1，
  recordingDroppedFrames=0，
  machMeasured 与 machCommand 偏差 <=5%，
  Apply=true 时 bodyRadianceWithAero >= bodyRadianceNoAero，
  bodyAeroDeltaKEffective 随 Mach 单调增加。

生产默认保护：
- ApplyAeroToRadiance=false
- AeroDebugLog=false
- DebugView=Off
- LogComponents=false
- EnableIRVerboseLog=0
- UseModtranPathRuntime=false
- ModtranPathRuntimeMode=Off
- UseModtranSkyRuntime=false
- UseModtranSolarRuntime=false
- JpegEncodeMode=rgb
- JpegQuality=100
- runtime_config_check.ps1：通过。

构建：
- HwaSim_IR Windows Release x64：通过。
- DataDrivenTestQT Release：通过。
- HwaSim_IR_VideoDisplay Windows Release x64：通过。
- 剩余 warning 为既有 PDB/qtmain 调试符号和历史未引用变量/符号 warning。

回归：
- runtime_config_check.ps1：通过。
- Stage3 MODTRAN tau-only strict：通过。
- Stage4 hotspot/brightspot strict：通过。
- Stage4 三波段 smoke：单独运行通过。
  注意：曾并行运行多个会启动 HwaSim_IR 的 smoke，Stage4 smoke 因端口/进程竞争误失败；
  单独重跑后 HeatSourceDiag/ThermalHotspot/BrightSpot 全部通过。
- Stage5 min radiance check：通过。
- Stage5 aero thermal smoke：通过。
- Phase4B UDP speed chain smoke：通过。

高 Mach 协议级 A/B：
- 脚本：tools/phase4c_aero_mach_protocol_ab.ps1 -Seconds 8
- CSV：logs/stage5/aero_mach_protocol_ab_summary.csv
- 代表帧目录：logs/stage5/phase4c_frames
- 代表帧数量：24 张。
- 全矩阵通过：3/10/20 km × Mach 0.5/1/2/3 × Apply false/true。
- Mach 测量偏差：约 0~0.003%，满足 <=5%。
- 性能汇总：
  Apply=false：minOutputFps=60.720，minDisplayFps=60.873，maxLatencyAvgMs=28.132，recordingDroppedFrames=0。
  Apply=true：minOutputFps=60.656，minDisplayFps=60.635，maxLatencyAvgMs=42.111，recordingDroppedFrames=0。
- stage5ModtranLookupMs=0，确认测试没有启用 MODTRAN path runtime。

Apply=true 关键结果：
- 3 km / Mach 0.5：effectiveBodyDelta=0.55 K，aeroRadianceRatio=1.02268。
- 3 km / Mach 1.0：effectiveBodyDelta=2.30 K，aeroRadianceRatio=1.09774。
- 3 km / Mach 2.0：effectiveBodyDelta=9.15 K，aeroRadianceRatio=1.43715。
- 3 km / Mach 3.0：effectiveBodyDelta=12.50 K，aeroRadianceRatio=1.63243。
- 10 km / Mach 0.5：effectiveBodyDelta=0.45 K，aeroRadianceRatio=1.01853。
- 10 km / Mach 1.0：effectiveBodyDelta=1.90 K，aeroRadianceRatio=1.08019。
- 10 km / Mach 2.0：effectiveBodyDelta=7.60 K，aeroRadianceRatio=1.35359。
- 10 km / Mach 3.0：effectiveBodyDelta=12.50 K，aeroRadianceRatio=1.63243。
- 20 km / Mach 0.5：effectiveBodyDelta=0.45 K，aeroRadianceRatio=1.01853。
- 20 km / Mach 1.0：effectiveBodyDelta=1.85 K，aeroRadianceRatio=1.07801。
- 20 km / Mach 2.0：effectiveBodyDelta=7.35 K，aeroRadianceRatio=1.34050。
- 20 km / Mach 3.0：effectiveBodyDelta=12.50 K，aeroRadianceRatio=1.63243。

sensorInputRadiance 评审：
- bodyRadianceWithAero 随 Mach 明显增加，趋势符合预期。
- 但当前 Stage5 components 中 sensorInputNoAero 与 sensorInputWithAero 基本相同，
  约 0.040231~0.040232。
- 原因：当前运行日志中 tauUp=0，sensorInput 框架退化为 legacy path 项，
  因此 bodyAeroDeltaK 尚未真正进入 sensorInputRadiance。
- 这说明 4C 已验证 body 分量候选，但还不能宣称 sensorInput 物理链路已经完整吃到气动加热。

视觉审查：
- 抽取代表帧目录：logs/stage5/phase4c_frames。
- 人工查看 3 km / Mach 3 / apply=false 与 apply=true 代表帧：
  画面肉眼几乎一致，没有过曝，没有淹没目标框/标注，也未观察到 rear/plume/brightspot 被吞没。
- 视觉变化弱与 sensorInput 未变化、当前默认显示链路仍以 legacy/display preview 为主相一致。

30 秒生产默认实测：
- 日志：logs/phase2a-final-20260622-111119
- MP4：
  HwaSim_IR_VideoDisplay/x64/Release/MP4/round_001_20260622_111133/output.mp4
- 配置：800x800，5 目标，videoFps=60，saveMP4En=true，
  Codec=auto，JpegEncodeMode=rgb，JpegQuality=100，
  EnableH264Experimental=false，LegacyEngineBodyHeating=false，
  EnableIRPhysicalPipeline=true，DebugView=Off，
  UseModtranPathRuntime=false，ModtranPathRuntimeMode=Off，
  ApplyAeroToRadiance=false。
- sentFps=60.026
- udpFps=60.182
- renderFps=60.334
- outputFps=60.098
- VideoDisplay receive/display=60.270
- latencyAvgMs=42.355
- irUpdateMs=0.903
- stage5RadianceComponentMs=0.649
- stage5AeroThermalMs=0.006412
- stage5ModtranLookupMs=0
- shaderInputCacheHitRate=97.173
- sourceSeqContinuous=1
- sourceSeqContinuousWritten=1
- inputQueueOverflow=0
- TCP overwritten=0
- recordingDroppedFrames=0
- written/mp4/annotations/targetAnnotations=1799/1799/1799/1799

结论：
- 高 Mach 协议级发送链路已验证：DataDrivenTestQT 按 altitude/Mach 写入真实 targetLoc.speed，
  HwaSim_IR 通过 UDP/TargetState/Stage5 得到匹配 Mach，不需要内部 fake speed。
- AeroApplyScale=0.25 在 bodyRadiance 层面表现为合理的“小比例候选配置”，
  Mach 0.5 较弱，Mach 2/3 明显；性能仍保持 60 Hz。
- 但由于当前 sensorInputRadiance 未随 bodyAeroDeltaK 改变，且代表帧肉眼差异弱，
  不建议把 ApplyAeroToRadiance 默认开启。
- 建议下一阶段把 AeroApplyScale=0.25 作为“可选候选配置”保留，
  同时优先修正/澄清 Stage5 sensorInput 的 tauUp/legacy display 接入位置；
  在 sensorInput 能反映 body 分量后，再决定是否进入默认候选。
- 如果用户更关注成像后端，可并行推进 MTF/blur；但不要把气动加热默认化和 MTF/blur 混在同一次验收里。
```

---

## 12. 给 Codex 的第一阶段实施 Prompt

见单独文件：`Codex_Phase1_Sync60_Perf_Prompt.md`。
