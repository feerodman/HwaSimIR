# SensorWave 配置使用边界

本文说明 `ConsoleApplication1_LLA/Bin/Config/SensorWave/default_*.json` 在 HwaSimIR 中的使用范围。SensorWave 文件来源接近 Presagis Ondulus IR profile，字段很多，HwaSimIR 只读取白名单字段，不一次性接入所有系统。

## 优先级

传感器几何和固有参数优先级：

1. UDP 初始化包：仿真输入优先，例如 `trackerSensorWidth`、`trackerSensorHeight`、`trackerSensorViewMin`、`trackerSensorViewMax`、`trackerSensorPixelAngle`。
2. SensorWave/default_*.json：传感器固有参数和 fallback profile。
3. HwaSimIRRuntime.ini：运行 fallback 和显示/调试开关。
4. 代码默认值：最后兜底。

实时仿真输入仍只来自 UDP realtime 包，例如 `targetState`、`weaponState`、`viewValid`、`engineState`、`strikeFlag`、`strikePart`、`lookatEn`，不允许 ini/env 覆盖。

## 当前使用字段

当前 HwaSimIR 白名单读取以下字段：

- `SensorConfigurationSystem.Width`：传感器宽度 fallback。UDP init 提供有效宽度时不覆盖。
- `SensorConfigurationSystem.Height`：传感器高度 fallback。UDP init 提供有效高度时不覆盖。
- `SensorConfigurationSystem.ADCBitNumber`：ADC 位数，用于传感器 profile 日志和后续显示链路参考。
- `SensorConfigurationSystem.DisplayBits`：显示位数，用于传感器 profile 日志和后续显示链路参考。
- `SensorConfigurationSystem.SpectralResponseRangeLow`：波段光谱下限，单位 um。
- `SensorConfigurationSystem.SpectralResponseRangeHigh`：波段光谱上限，单位 um。
- `SensorConfigurationSystem.NoiseEquivalentTemperatureDifference`：NETD，当前只记录到 profile，不做完整 NETD 噪声物理标定。
- `SensorConfigurationSystem.DetectorPitch`：探测器像元尺寸，当前用于 profile 记录和后续 Stage6/Stage6C 参考。
- `SensorConfigurationSystem.FocalLength`：焦距，当前用于 profile 记录和后续几何参考。
- `SensorConfigurationSystem.LensFnumber`：镜头 F 数，当前用于 profile 记录和后续辐射/噪声参考。
- `SensorConfigurationSystem.BlackHot`：黑热偏好，当前只记录；正式显示极性由 `HwaSimIRRuntime.ini` 的 `[Stage6Display] WhiteHot` 控制。

## 只作为 fallback 或日志参考

- `Width` / `Height`：只有 UDP init 宽高无效或缺失时才使用 SensorWave。
- `FOVH` / `FOVV`：当前真实 FOV 优先由 `trackerSensorPixelAngle` 按 `umrad/pixel` 与宽高计算。只有 `trackerSensorPixelAngle` 无效时，才允许用 SensorWave `FOVH` 反推出 fallback pixel angle。

## 当前明确不使用

以下 Presagis/Ondulus IR 复杂系统当前不接入：

- `ImageFusionSystem`
- `DistributedApertureSystem`
- `IntensifierConfigurationSystem`
- `TaskSSR`
- 未白名单的高级渲染、融合、增强、任务系统字段

## 后续可能接入

以下字段或系统后续可能逐步接入，但不在本轮实现：

- `GainControlSystem`
- `MTF`
- `Vignetting`
- `FXAA`
- `TemporalAA`
- 更完整的 NETD / 固定图样噪声 / 探测器响应模型

## 保留每波段 default_*.json

后续仍保留不同波段的 `default_*.json`。每个波段有独立光谱范围、NETD、探测器参数和可能的镜头参数。HwaSimIR 只读取白名单字段，Presagis 原始复杂系统不一次性照搬。
