# HwaSimIR 阶段 4：发动机/尾喷热源与特殊亮斑控制

## 范围边界

本轮阶段 4 只实现温度场中的目标动态热源入口，重点是发动机/尾喷热源，以及由 `WeaponState.strikeFlag` / `WeaponState.strikePart` 控制的头部/中部特殊辐射亮斑。

本轮明确不做以下内容：

- 不做 damage、damaged 或毁伤状态模型。
- 不创建 damage hotspot。
- 不让 `strikeFlag` 控制发动机/尾喷热源。
- 不让 `engineState` 控制头部/中部 BrightSpot。
- 不接入 `path_radiance`、`sky_radiance`、`solar_irradiance`。
- 不进入阶段 5 完整辐射链路。
- 不重跑 MODTRAN production，也不改变 `UseModtranTauForAtmosphere` 默认关闭状态。

## 术语

### ThermalHotspot

`ThermalHotspot` 是物理热源，表示真实温度场中的局部高温结构。本轮主要用于发动机后部和尾喷方向。

核心属性：

- `temperatureK` / `targetTempK` / `currentTempK`：Kelvin 温度。
- `heatTauSec`：发动机开启时升温时间常数。
- `coolTauSec`：发动机关闭时向环境温度冷却的时间常数。
- `localPos`、`localDir`：目标模型局部坐标和方向。
- `shape`、`size`：热源形状和尺度。
- `intensity`：legacy shader 映射到现有 `u_hotspot_rear_temp` 的强度缩放。

更新公式：

```text
alpha = 1 - exp(-dt / tau)
currentTempK += (targetTempK - currentTempK) * alpha
```

`engineState=true` 时使用 `heatTauSec` 向 `targetTempK` 升温；`engineState=false` 时使用 `coolTauSec` 向 `ambientTempK` 冷却。关闭时 shader 的 `u_hotspot_rear_en` 仍保持关闭，当前温度只用于日志和后续扩展。

### BrightSpot

`BrightSpot` 是特殊辐射亮斑，用于模拟目标头部或中部的可控特殊辐射效果。它不是物理热源，不等同 Kelvin 温度，不参与发动机冷却，也不参与毁伤状态。

控制规则：

- `strikeFlag=false`：关闭 BrightSpot。
- `strikeFlag=true && strikePart==1`：启用头部 BrightSpot。
- `strikeFlag=true && strikePart==2`：启用中部 BrightSpot。
- 其他 `strikePart`：关闭 BrightSpot 并打印 warning。

核心属性：

- `part`：`Head` 或 `MidBody`。
- `localPos`：模型局部坐标。
- `radius`：亮斑半径。
- `intensity`：特殊辐射叠加强度。

现有 legacy shader uniform `u_brightspot_temp` 在本阶段暂时继续传 `LVecBase2f(intensity, 0)`。这里的字段名带 `temp` 只是旧 shader 命名，阶段 4 注释和变量命名均按 `intensity` 理解，不代表 Kelvin 温度。

## 实现策略

- 新增 `IRTemperatureModel`，集中维护 `IRHotspotState` 和 `IRBrightSpotState`。
- 新增配置 `Config/IRHotspots/target_hotspots.json`，按平台配置发动机后部热源和头部/中部 BrightSpot。
- 配置缺失时使用安全默认值并输出 warning，不中断仿真。
- HwaSimIR 每帧只设置现有 shader uniform：
  - `u_hotspot_rear_en`
  - `u_hotspot_rear_pos`
  - `u_hotspot_rear_radius`
  - `u_hotspot_rear_temp`
  - `u_brightspot_en`
  - `u_brightspot_pos`
  - `u_brightspot_radius`
  - `u_brightspot_temp`
- 生产默认路径不新增复杂 shader 逻辑；仅保留默认关闭的可视化诊断 mask，不改变阶段 3 MODTRAN tau-only 行为。

## 可视化接线诊断补充

针对“`engineState` / `strikeFlag` 日志变化，但目标模型仍然黑”的问题，阶段 4 增加默认关闭的接线诊断能力，专门验证状态是否真正进入 shader uniform 和最终像素输出：

- `[Stage4 Input]` 在真实 Display 包进入后立即打印 `targetID`、平台、`engineState`、`strikeFlag`、`strikePart` 和 `sourcePacketTimeMs`，用于确认 DataDrivenTestQT 的值是否进入 HwaSimIR。
- `[Stage4 Uniform]` 在目标节点设置 shader uniform 后低频打印 `nodeName`、`hasShader`、基础纹理/材质 ID 纹理状态、`hotspotRearEn`、`brightspotEn`、亮斑位置、半径和强度。
- `[Stage4 VisualDebug]` 只在显式打开 `EnableStage4HotspotVisualDebug=1` 时打印，用于确认诊断 mask 正在工作。
- `EnableStage4HotspotVisualDebug=0` 为默认值；打开后 shader 会把 Stage4 hotspot/brightspot mask 区域抬亮，便于判断 uniform 是否进入可见像素链路。
- `ForceStage4BrightSpotVisible=0` 与 `ForceStage4RearHotspotVisible=0` 为默认值；打开时仅用于排查，将对应目标调试为大半径、强强度，避免局部坐标或半径过小造成不可见。

这些开关都是诊断入口，不改变生产默认渲染；不会接入 `path_radiance`、`sky_radiance`、`solar_irradiance`，也不会进入阶段 5 完整辐射链路。

## 目标映射与可见性

初始化流程保持为：`InitP2cObjectTrackingCmd` 根据 `platParam[2]` 生成载机平台并给第一个载机绑定相机，根据 `trackingInit` 缓存传感器参数，再根据 `MissileMaxCount120`、`MissileMaxCount9`、`MissileMaxCountMMD` 预生成 AIM120、AIM9X、MMD 三类目标槽位。

实时 Display 包到达后，目标槽位不再只按 `targetID` 匹配，而是按 `targetType + targetPlatID + targetID` 三元组唯一映射。`targetNumValid` 只控制前 N 个 `TargetState` 是否显示：目标状态和位置仍可按三元组更新，但索引不在前 N 或 `TargetState.viewValid=false` 时节点隐藏。若该目标也是 `WeaponState` 指向目标，`WeaponState.viewValid=false` 也会隐藏它。

相机控制规则为：`WeaponState.lookatEn=true` 时看向 `WeaponState` 三元组对应目标；`lookatEn=false` 时使用 `xxOutAng[0]` 方位角和 `xxOutAng[1]` 俯仰角驱动相机，并同步本帧 `offsetAng[0]=pitch`、`offsetAng[1]=yaw`。

## 日志

每帧按两条独立日志输出，确保热源和亮斑不混淆：

```text
[Stage4 ThermalHotspot] engineState, currentTempK, targetTempK, enabled
[Stage4 BrightSpot] strikeFlag, strikePart, part, localPos, radius, intensity
```

## 验收

阶段 4 新增检查脚本：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage4_hotspot_check.ps1 -Strict
powershell -ExecutionPolicy Bypass -File tools\stage4_hotspot_smoke.ps1 -Bands @(1,2,3) -DelayMs 500
powershell -ExecutionPolicy Bypass -File tools\stage4_hotspot_visual_smoke.ps1 -Bands @(1,2,3) -DelayMs 500
```

阶段 4 还应继续通过阶段 0 构建和阶段 3 tau-only 检查：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1
powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_loader_check.ps1 -Strict
```
