# P6A 每波段字段消费清单

表中旧系统字段以 Archive/P5 的精确原文件为盘点全集；active 标明精简配置中是否保留。JSON 加载不代表实现遗留算法。

| 文件 / 完整 JSON 路径 | active / 解析 | 成员 | 消费者 | 状态 / 单位 |
|---|---|---|---|---|
| default_NVG.json / `DisplayColor.A` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `DisplayColor.B` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `DisplayColor.G` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `DisplayColor.R` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `DisplayColorEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `FXAAEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `HotSpotsEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ImageFusionEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `LensFocusAffectOTW` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ModulationTransferFunctionEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `NoiseEffectEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `PixelBleedingEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ReflectionsEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `SensorType` | 1 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ShowCurrentTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ShowMinMax` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ShowOverlays` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `ShowProcessingTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `SupersamplingLevel` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ColorImagerySystem.ColorEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ColorImagerySystem.VegetationColorEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.DigitalZoom` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.DigitalZoomOffsetX` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.DigitalZoomOffsetY` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.DisplayBits` | 1 / 1 | displayBits | LogActiveIRSensorProfile；实际视频固定 8 bit | 仅日志 / bit |
| default_NVG.json / `Systems.DisplaySystem.DisplayGamma` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.OutputLayout.Alpha` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.OutputLayout.Blue` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.OutputLayout.Green` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DisplaySystem.OutputLayout.Red` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DistributedApertureSystem.Cameras` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.DistributedApertureSystem.Enabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.Automatic` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.BlendingWidthForLocalAGC` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.Gain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.GridSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.Level` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.LocalAGC` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.GainControlSystem.Responsiveness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Algorithm` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Inverts[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Inverts[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Inverts[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Inverts[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Sources[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Sources[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Sources[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Sources[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.WeightingFactor` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Weights[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Weights[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Weights[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.ImageFusionSystem.Weights[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IncidentResponseSystem.IncidentResponseEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.FilenameMTF` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.FilenameQE` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.HaloEffectEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.HaloLightColorEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.HaloPersistentFactor` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.HaloSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.IITSyetemMTFMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.IITSystemEBI` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.IITSystemEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.IITSystemSNR` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.LimitResolution` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.LuminousSensitivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.LuminousTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.MaximumOutputBrightness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.PeakLuminousGain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.PeakQuantumEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.PeakWavelength` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.PhosphorRelativeEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.QuantumEfficiencyMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.RadiantSensitivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.RadiantWavelength` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.IntensifierConfigurationSystem.TubeDiameter` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.JitterSystem.Pattern` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.MaterialBlendingSystem.Mode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.MaterialBlendingSystem.Size` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.ADCBitNumber` | 1 / 1 | adcBits | LogActiveIRSensorProfile；不控制实际编码量化 | 仅日志 / bit |
| default_NVG.json / `Systems.SensorConfigurationSystem.AdjustableTemperatureRange` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.BackgroundTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.BlackHot` | 1 / 1 | blackHot | LogActiveIRSensorProfile；实际极性使用 HwaSimIR.Display | 仅日志 / bool |
| default_NVG.json / `Systems.SensorConfigurationSystem.BlurringType` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.DarkCurrentDensity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.DetectorConfigurationMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.DetectorPitch` | 1 / 1 | detectorPitchMm | LogActiveIRSensorProfile；未用于电子通量 | 仅日志 / mm |
| default_NVG.json / `Systems.SensorConfigurationSystem.DetectorTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.DetectorType` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.FOVH` | 1 / 1 | fovHDeg | 缺省 pixelAngle 反推；仍经过接口几何校验/上限 | 仅缺省 / degree |
| default_NVG.json / `Systems.SensorConfigurationSystem.FOVV` | 1 / 1 | fovVDeg | LogActiveIRSensorProfile | 仅日志 / degree |
| default_NVG.json / `Systems.SensorConfigurationSystem.FocalLength` | 1 / 1 | focalLengthMm | LogActiveIRSensorProfile；不参与有效接口 FOV 计算 | 仅日志 / mm |
| default_NVG.json / `Systems.SensorConfigurationSystem.FocusDistance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.Height` | 1 / 1 | height | ProcessRealSimSceneInitData -> IRSensorModel | 仅缺省 / pixel |
| default_NVG.json / `Systems.SensorConfigurationSystem.HorMaxSpatialFrequency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.HorizontalMTFFile` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.IntegrationTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.LensFnumber` | 1 / 1 | lensFNumber | LogActiveIRSensorProfile；未用于电子通量 | 仅日志 / ratio |
| default_NVG.json / `Systems.SensorConfigurationSystem.LensFocusMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.NoiseEquivalentPower` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.NoiseEquivalentTemperatureDifference` | 1 / 1 | netdK | LogActiveIRSensorProfile；未接入 NETD 噪声算法 | 仅日志 / legacy nominal K; uncalibrated |
| default_NVG.json / `Systems.SensorConfigurationSystem.ObjectEmissivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.PureBlackbodyEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.QuamtumEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.SpectralResponseRangeHigh` | 1 / 1 | spectralHighUm | 固定波段一致性检查；Stage5 debug input.sensorHighUm | 正常使用（验证/诊断） / um |
| default_NVG.json / `Systems.SensorConfigurationSystem.SpectralResponseRangeLow` | 1 / 1 | spectralLowUm | 固定波段一致性检查；Stage5 debug input.sensorLowUm | 正常使用（验证/诊断） / um |
| default_NVG.json / `Systems.SensorConfigurationSystem.TemperatureAdjustmentEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.TemperatureRangeLower` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.TemperatureRangeUpper` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.ThermalADCOFFSET` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.ThermalADCRange` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.ThermalGain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.ThermalGainEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.VerMaxSpatialFrequency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.VerticalMTFFile` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.WellCapacity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.SensorConfigurationSystem.Width` | 1 / 1 | width | ProcessRealSimSceneInitData -> IRSensorModel | 仅缺省 / pixel |
| default_NVG.json / `Systems.ShadowSystem.ShadowEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskADCSignal.DefectsTexture` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskAvgDistanceCompute.RegionSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskAvgDistanceCompute.Responsiveness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskFXAA.LumaThreshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskGainControl.Invert` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskPixelBleeding.Radius` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskPixelBleeding.Threshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskRadianceView.MaximumRadiance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskRadianceView.MinimumRadiance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskSSR.AutoscaleThickness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskSSR.LOD` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskSSR.ObjectThickness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskSSR.WaterOnly` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskTemporalAA.BlendFactor` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskTemporalAA.DistanceThreshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskThermal.MaximumTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskThermal.MinimumTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `Systems.TaskVignetting.VignettingMask` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `TemporalAAEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `VignettingEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_NVG.json / `HwaSimIR.SchemaVersion` | 1 / 1 | schemaVersion | 加载验证/来源日志 | 正常使用 / metadata |
| default_NVG.json / `HwaSimIR.Band` | 1 / 1 | band validation | 加载验证/来源日志 | 正常使用 / metadata |
| default_NVG.json / `HwaSimIR.Revision` | 1 / 1 | revision | 加载验证/来源日志 | 正常使用 / metadata |
| default_NVG.json / `HwaSimIR.LegacyArchive` | 1 / 0 | — | 加载验证/来源日志 | 追溯元数据 / metadata |
| default_NVG.json / `HwaSimIR.Display.DefaultPreset` | 1 / 1 | defaultDisplayPreset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / metadata |
| default_NVG.json / `HwaSimIR.Display.Presets.Legacy.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_NVG.json / `HwaSimIR.Display.Presets.Legacy.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_NVG.json / `HwaSimIR.Display.Presets.Legacy.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_NVG.json / `HwaSimIR.Display.Presets.Legacy.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_NVG.json / `HwaSimIR.Display.Presets.Legacy.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_NVG.json / `HwaSimIR.Display.Presets.Game.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_NVG.json / `HwaSimIR.Display.Presets.Game.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_NVG.json / `HwaSimIR.Display.Presets.Game.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_NVG.json / `HwaSimIR.Display.Presets.Game.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_NVG.json / `HwaSimIR.Display.Presets.Game.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_NVG.json / `HwaSimIR.Display.Presets.Auto.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_NVG.json / `HwaSimIR.Display.Presets.Auto.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_NVG.json / `HwaSimIR.Display.Presets.Auto.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_NVG.json / `HwaSimIR.Display.Presets.Auto.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_NVG.json / `HwaSimIR.Display.Presets.Auto.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_NVG.json / `HwaSimIR.Display.Presets.Black.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_NVG.json / `HwaSimIR.Display.Presets.Black.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_NVG.json / `HwaSimIR.Display.Presets.Black.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_NVG.json / `HwaSimIR.Display.Presets.Black.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_NVG.json / `HwaSimIR.Display.Presets.Black.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_MWIR.json / `DisplayColor.A` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `DisplayColor.B` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `DisplayColor.G` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `DisplayColor.R` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `DisplayColorEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `FXAAEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `HotSpotsEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ImageFusionEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `LensFocusAffectOTW` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ModulationTransferFunctionEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `NoiseEffectEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `PixelBleedingEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ReflectionsEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `SensorType` | 1 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ShowCurrentTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ShowMinMax` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ShowOverlays` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `ShowProcessingTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `SupersamplingLevel` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ColorImagerySystem.ColorEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ColorImagerySystem.VegetationColorEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.DigitalZoom` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.DigitalZoomOffsetX` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.DigitalZoomOffsetY` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.DisplayBits` | 1 / 1 | displayBits | LogActiveIRSensorProfile；实际视频固定 8 bit | 仅日志 / bit |
| default_MWIR.json / `Systems.DisplaySystem.DisplayGamma` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.OutputLayout.Alpha` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.OutputLayout.Blue` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.OutputLayout.Green` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DisplaySystem.OutputLayout.Red` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DistributedApertureSystem.Cameras` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.DistributedApertureSystem.Enabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.Automatic` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.BlendingWidthForLocalAGC` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.Gain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.GridSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.Level` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.LocalAGC` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.GainControlSystem.Responsiveness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Algorithm` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Inverts[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Inverts[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Inverts[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Inverts[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Sources[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Sources[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Sources[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Sources[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.WeightingFactor` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Weights[0]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Weights[1]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Weights[2]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.ImageFusionSystem.Weights[3]` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IncidentResponseSystem.IncidentResponseEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.FilenameMTF` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.FilenameQE` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.HaloEffectEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.HaloSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.IITSyetemMTFMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.IITSystemEBI` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.IITSystemEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.IITSystemSNR` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.LimitResolution` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.LuminousSensitivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.LuminousTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.MaximumOutputBrightness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.PeakLuminousGain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.PeakQuantumEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.PeakWavelength` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.PhosphorRelativeEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.QuantumEfficiencyMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.RadiantSensitivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.RadiantWavelength` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.IntensifierConfigurationSystem.TubeDiameter` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.JitterSystem.Pattern` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.MaterialBlendingSystem.Mode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.MaterialBlendingSystem.Size` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ADCBitNumber` | 1 / 1 | adcBits | LogActiveIRSensorProfile；不控制实际编码量化 | 仅日志 / bit |
| default_MWIR.json / `Systems.SensorConfigurationSystem.AdjustableTemperatureRange` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.BackgroundTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.BlackHot` | 1 / 1 | blackHot | LogActiveIRSensorProfile；实际极性使用 HwaSimIR.Display | 仅日志 / bool |
| default_MWIR.json / `Systems.SensorConfigurationSystem.BlurringType` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.DarkCurrentDensity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.DetectorConfigurationMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.DetectorPitch` | 1 / 1 | detectorPitchMm | LogActiveIRSensorProfile；未用于电子通量 | 仅日志 / mm |
| default_MWIR.json / `Systems.SensorConfigurationSystem.DetectorTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.DetectorType` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.FOVH` | 1 / 1 | fovHDeg | 缺省 pixelAngle 反推；仍经过接口几何校验/上限 | 仅缺省 / degree |
| default_MWIR.json / `Systems.SensorConfigurationSystem.FOVV` | 1 / 1 | fovVDeg | LogActiveIRSensorProfile | 仅日志 / degree |
| default_MWIR.json / `Systems.SensorConfigurationSystem.FocalLength` | 1 / 1 | focalLengthMm | LogActiveIRSensorProfile；不参与有效接口 FOV 计算 | 仅日志 / mm |
| default_MWIR.json / `Systems.SensorConfigurationSystem.FocusDistance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.Height` | 1 / 1 | height | ProcessRealSimSceneInitData -> IRSensorModel | 仅缺省 / pixel |
| default_MWIR.json / `Systems.SensorConfigurationSystem.HorMaxSpatialFrequency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.HorizontalMTFFile` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.IntegrationTime` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.LensFnumber` | 1 / 1 | lensFNumber | LogActiveIRSensorProfile；未用于电子通量 | 仅日志 / ratio |
| default_MWIR.json / `Systems.SensorConfigurationSystem.LensFocusMode` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.NoiseEquivalentPower` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.NoiseEquivalentTemperatureDifference` | 1 / 1 | netdK | LogActiveIRSensorProfile；未接入 NETD 噪声算法 | 仅日志 / legacy nominal K; uncalibrated |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ObjectEmissivity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.PureBlackbodyEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.QuamtumEfficiency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.SpectralResponseRangeHigh` | 1 / 1 | spectralHighUm | 固定波段一致性检查；Stage5 debug input.sensorHighUm | 正常使用（验证/诊断） / um |
| default_MWIR.json / `Systems.SensorConfigurationSystem.SpectralResponseRangeLow` | 1 / 1 | spectralLowUm | 固定波段一致性检查；Stage5 debug input.sensorLowUm | 正常使用（验证/诊断） / um |
| default_MWIR.json / `Systems.SensorConfigurationSystem.TemperatureAdjustmentEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.TemperatureRangeLower` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.TemperatureRangeUpper` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ThermalADCOFFSET` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ThermalADCRange` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ThermalGain` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.ThermalGainEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.VerMaxSpatialFrequency` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.VerticalMTFFile` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.WellCapacity` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.SensorConfigurationSystem.Width` | 1 / 1 | width | ProcessRealSimSceneInitData -> IRSensorModel | 仅缺省 / pixel |
| default_MWIR.json / `Systems.ShadowSystem.ShadowEffect` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskADCSignal.DefectsTexture` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskAvgDistanceCompute.RegionSize` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskAvgDistanceCompute.Responsiveness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskFXAA.LumaThreshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskGainControl.Invert` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskPixelBleeding.Radius` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskPixelBleeding.Threshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskRadianceView.MaximumRadiance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskRadianceView.MinimumRadiance` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskSSR.AutoscaleThickness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskSSR.LOD` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskSSR.ObjectThickness` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskSSR.WaterOnly` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskTemporalAA.BlendFactor` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskTemporalAA.DistanceThreshold` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskThermal.MaximumTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskThermal.MinimumTemperature` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `Systems.TaskVignetting.VignettingMask` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `TemporalAAEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `VignettingEnabled` | 0 / 0 | — | 无当前消费者 | 未实现/未解析 / 未确认 |
| default_MWIR.json / `HwaSimIR.SchemaVersion` | 1 / 1 | schemaVersion | 加载验证/来源日志 | 正常使用 / metadata |
| default_MWIR.json / `HwaSimIR.Band` | 1 / 1 | band validation | 加载验证/来源日志 | 正常使用 / metadata |
| default_MWIR.json / `HwaSimIR.Revision` | 1 / 1 | revision | 加载验证/来源日志 | 正常使用 / metadata |
| default_MWIR.json / `HwaSimIR.LegacyArchive` | 1 / 0 | — | 加载验证/来源日志 | 追溯元数据 / metadata |
| default_MWIR.json / `HwaSimIR.Display.DefaultPreset` | 1 / 1 | defaultDisplayPreset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / metadata |
| default_MWIR.json / `HwaSimIR.Display.Presets.Legacy.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_MWIR.json / `HwaSimIR.Display.Presets.Legacy.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_MWIR.json / `HwaSimIR.Display.Presets.Legacy.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_MWIR.json / `HwaSimIR.Display.Presets.Legacy.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_MWIR.json / `HwaSimIR.Display.Presets.Legacy.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_MWIR.json / `HwaSimIR.Display.Presets.Game.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_MWIR.json / `HwaSimIR.Display.Presets.Game.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_MWIR.json / `HwaSimIR.Display.Presets.Game.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_MWIR.json / `HwaSimIR.Display.Presets.Game.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_MWIR.json / `HwaSimIR.Display.Presets.Game.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_MWIR.json / `HwaSimIR.Display.Presets.Auto.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_MWIR.json / `HwaSimIR.Display.Presets.Auto.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_MWIR.json / `HwaSimIR.Display.Presets.Auto.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_MWIR.json / `HwaSimIR.Display.Presets.Auto.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_MWIR.json / `HwaSimIR.Display.Presets.Auto.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
| default_MWIR.json / `HwaSimIR.Display.Presets.Black.Gamma` | 1 / 1 | displayPresets[].gamma -> m_stage5SensorInputDisplayGamma | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / inverse exponent |
| default_MWIR.json / `HwaSimIR.Display.Presets.Black.Gain` | 1 / 1 | displayPresets[].gain -> m_stage6DisplayConfig.displayGain | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / ratio |
| default_MWIR.json / `HwaSimIR.Display.Presets.Black.OffsetGray` | 1 / 1 | displayPresets[].offsetGray -> m_stage6DisplayConfig.displayOffset | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / gray8 |
| default_MWIR.json / `HwaSimIR.Display.Presets.Black.WhiteHot` | 1 / 1 | displayPresets[].whiteHot -> m_stage6DisplayConfig.whiteHot | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / bool |
| default_MWIR.json / `HwaSimIR.Display.Presets.Black.Mode` | 1 / 1 | displayPresets[].automatic -> m_stage6AgcEnabled | ApplyStage6DisplayConfig / final shader / pre-display AGC | 正常使用 / Fixed/Auto |
