怎么看当前程序可以修改的所有$env？我之前测好像好多测试参数都没改就运行了，输出所有$env并标出用途，我都手动测一下

可以。这里分两件事：

1. **看当前 PowerShell 里已经设置了哪些 `$env`**
2. **看 HwaSimIR 源码支持读取哪些 `$env`**

注意：`$env` 必须在 **启动 HwaSimIR 之前** 设置。程序启动后，你再在 PowerShell 里改 `$env`，已经运行的 HwaSimIR 进程不会自动收到新值。

------

## 1. 查看当前已经设置的 HwaSimIR 相关 `$env`

在启动 HwaSimIR 的 PowerShell 窗口里执行：

```powershell
Get-ChildItem Env: |
  Where-Object { $_.Name -match '^(Stage|Enable|Force|UseModtran|HwaSimIR)' } |
  Sort-Object Name |
  Format-Table -AutoSize
```

如果想只看值：

```powershell
$names = @(
"EnableModtranTauDebug",
"UseModtranTauForAtmosphere",
"EnableStage4HotspotVisualDebug",
"ForceStage4BrightSpotVisible",
"ForceStage4RearHotspotVisible",
"EnableStage5RadianceDebug",
"Stage5DebugViewMode",
"Stage5DebugToneMap",
"Stage5OutputFrameDump",
"Stage5OutputFrameDumpPath",
"Stage5OutputFrameDumpEvery",
"Stage6WhiteHot",
"Stage6DisplayGain",
"Stage6DisplayOffset",
"Stage6DisplayApplyToWindow",
"Stage6BackgroundDisplayEnable",
"Stage6NoiseEnable",
"Stage6NoiseSigmaGray",
"EnableStage7SkyHorizon",
"Stage7DebugMode",
"Stage7GroundZOffset",
"HwaSimIRExitOnStop"
)

$names | ForEach-Object {
  [PSCustomObject]@{
    Name = $_
    Value = (Get-Item "Env:\$_" -ErrorAction SilentlyContinue).Value
  }
} | Format-Table -AutoSize
```

------

## 2. 从源码自动扫描所有支持的 `$env`

在仓库根目录执行：

```powershell
$src = Get-Content .\ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp -Raw

[regex]::Matches($src, 'ReadProcessEnv(?:Flag|String|Double|Int)\("([^"]+)"') |
  ForEach-Object { $_.Groups[1].Value } |
  Sort-Object -Unique
```

HwaSimIR 里的环境变量就是通过 `ReadProcessEnvFlag/String/Double/Int` 读取的；布尔值只有 `"1"`、`"true"`、`"TRUE"`、`"True"` 会被当成 true，其他情况走 false 或默认值。

------

## 3. 当前 HwaSimIR 支持的 `$env` 和用途

### Stage3 / MODTRAN

| 环境变量                     | 默认 | 用途                                            | 建议                   |
| ---------------------------- | ---- | ----------------------------------------------- | ---------------------- |
| `EnableModtranTauDebug`      | `0`  | 打印 MODTRAN tau 与 legacy tau 对比，不改变画面 | 一般 `0`               |
| `UseModtranTauForAtmosphere` | `0`  | 让 NIR/MWIR 使用 MODTRAN tau 参与大气透过率     | 当前先 `0`，稳定后再测 |

这两个是在大气模型初始化时读取的。

------

### Stage4 / 热源亮斑调试

| 环境变量                         | 默认 | 用途                       | 建议     |
| -------------------------------- | ---- | -------------------------- | -------- |
| `EnableStage4HotspotVisualDebug` | `0`  | Stage4 热源/亮斑可视化诊断 | 一般 `0` |
| `ForceStage4BrightSpotVisible`   | `0`  | 强制 BrightSpot 可见       | 只调试用 |
| `ForceStage4RearHotspotVisible`  | `0`  | 强制尾喷热源可见           | 只调试用 |

这几个用于验证 `engineState -> rear ThermalHotspot`、`strikeFlag/strikePart -> BrightSpot` 是否接通。

------

### Stage5 / 辐射调试

| 环境变量                            | 默认                     | 用途                                                         | 建议              |
| ----------------------------------- | ------------------------ | ------------------------------------------------------------ | ----------------- |
| `EnableStage5RadianceDebug`         | `0`                      | 开启 Stage5 辐射调试路径                                     | 正常运行 `0`      |
| `Stage5DebugViewMode`               | `Composite`              | 显示模式：`Composite` / `BodyOnly` / `HotspotOnly` / `BrightSpotOnly` | 调试主体/热源时用 |
| `Stage5DebugToneMap`                | 配置文件值，常用 `asinh` | Stage5 tone map：`linear` / `log` / `asinh`                  | 一般不改          |
| `Stage5BodyRadianceScale`           | 配置文件值               | 主体辐射显示缩放                                             | 少改              |
| `Stage5HotspotRadianceScale`        | 配置文件值               | Hotspot 辐射显示缩放                                         | 少改              |
| `Stage5BrightspotRadianceScale`     | 配置文件值               | BrightSpot 辐射显示缩放                                      | 少改              |
| `Stage5DebugMinBodyGray`            | 配置文件值               | Stage5 debug 主体最低灰度                                    | 调主体可见性      |
| `Stage5SolarReflectanceWeight`      | 配置文件值               | 全波段太阳反射权重覆盖                                       | 少改              |
| `Stage5SolarReflectanceWeight_VIS`  | 配置文件值               | VIS 太阳反射权重                                             | 少改              |
| `Stage5SolarReflectanceWeight_NIR`  | 配置文件值               | NIR 太阳反射权重                                             | 少改              |
| `Stage5SolarReflectanceWeight_SWIR` | 配置文件值               | SWIR 太阳反射权重                                            | 少改              |
| `Stage5SolarReflectanceWeight_MWIR` | 配置文件值               | MWIR 太阳反射权重                                            | 少改              |
| `Stage5SolarReflectanceWeight_LWIR` | 配置文件值               | LWIR 太阳反射权重                                            | 一般保持 0        |
| `Stage5BodyDisplayGain`             | 配置文件值               | Stage5C 主体显示增益                                         | 少改              |
| `Stage5ReflectedDisplayGain`        | 配置文件值               | 反射项显示增益                                               | 少改              |
| `Stage5HotspotDisplayGain`          | 配置文件值               | 热源显示增益                                                 | 少改              |
| `Stage5BrightspotDisplayGain`       | 配置文件值               | 亮斑显示增益                                                 | 少改              |
| `Stage5CompositeMinGray`            | 配置文件值               | Composite 最小灰度                                           | 少改              |
| `Stage5CompositeMaxGray`            | 配置文件值               | Composite 最大灰度                                           | 少改              |
| `Stage5UseBaseTextureModulation`    | 配置文件值               | 是否用基础纹理调制 Stage5 debug 输出                         | 调纹理时用        |
| `Stage5OutputFrameDump`             | `0`                      | 保存 Stage5 输出帧                                           | smoke/debug 用    |
| `Stage5OutputFrameDumpPath`         | 空                       | dump 输出目录                                                | 配合上一项        |
| `Stage5OutputFrameDumpEvery`        | `5`                      | 每几帧 dump 一张                                             | 配合上一项        |

Stage5 的很多默认值来自 `stage5_debug_display.json`，然后环境变量可以覆盖。

------

### Stage6 / 最终显示与输出

| 环境变量                        | 默认                                            | 用途                                    | 建议                               |
| ------------------------------- | ----------------------------------------------- | --------------------------------------- | ---------------------------------- |
| `Stage6WhiteHot`                | `1`                                             | `1=白热`，`0=黑热`                      | 手动必测                           |
| `Stage6DisplayGain`             | `1.0`                                           | 最终显示增益，整体调亮/暗               | 手动必测                           |
| `Stage6DisplayOffset`           | `0.0`                                           | 最终显示偏移，单位灰度值                | 手动必测                           |
| `Stage6DisplayApplyToWindow`    | 当前代码默认 `1`                                | 是否把 Stage6 显示应用到窗口/final pass | 当前 final sensor 管线一般保持 `1` |
| `Stage6BackgroundDisplayEnable` | `1`                                             | 背景是否参与 Stage6 显示变换            | 排查背景可设 `0`                   |
| `Stage6NoiseEnable`             | `0`，但协议 `noiseEn/trackerSensorNoise` 可触发 | 开启轻量噪声                            | 调试时先 `0`                       |
| `Stage6NoiseSigmaGray`          | `0.0`，但协议可覆盖                             | 噪声强度，灰度单位                      | 肉眼测试可设 `10~25`               |

Stage6 的显示配置会在初始化命令后读取；如果没有设置 `Stage6NoiseEnable/Stage6NoiseSigmaGray`，代码会继续看协议里的 `noiseEn/trackerSensorNoise`，所以你之前可能“没设 env 但噪声还开了”，就是被 DataDrivenTestQT 的协议参数带起来了。

------

### Stage7 / 天空、地面、海面背景

| 环境变量                 | 默认  | 用途                                                | 建议               |
| ------------------------ | ----- | --------------------------------------------------- | ------------------ |
| `EnableStage7SkyHorizon` | `1`   | 开启 Stage7 天空/地面背景                           | 保持 `1`           |
| `Stage7DebugMode`        | `Off` | `Off` / `SkyOnly` / `GroundOnly` / `SkyGroundColor` | 当前排查重点       |
| `Stage7GroundZOffset`    | `0.0` | 手动平移 ground/sea 平面高度                        | 当前可测试 `11000` |

你上传的版本里 Stage7 的开关、debug mode、ground offset 都是通过环境变量读取的。

------

### 测试退出

| 环境变量             | 默认 | 用途                         | 建议                       |
| -------------------- | ---- | ---------------------------- | -------------------------- |
| `HwaSimIRExitOnStop` | `0`  | 收到停止控制包后自动退出程序 | smoke 脚本用，手动一般 `0` |

这个在控制指令 `停止` 分支里读取。

------

## 4. 建议你先这样做一组“干净测试”

先清掉相关变量，避免之前的值残留：

```powershell
$names = @(
"EnableModtranTauDebug",
"UseModtranTauForAtmosphere",
"EnableStage4HotspotVisualDebug",
"ForceStage4BrightSpotVisible",
"ForceStage4RearHotspotVisible",
"EnableStage5RadianceDebug",
"Stage5DebugViewMode",
"Stage5DebugToneMap",
"Stage5BodyRadianceScale",
"Stage5HotspotRadianceScale",
"Stage5BrightspotRadianceScale",
"Stage5DebugMinBodyGray",
"Stage5SolarReflectanceWeight",
"Stage5SolarReflectanceWeight_VIS",
"Stage5SolarReflectanceWeight_NIR",
"Stage5SolarReflectanceWeight_SWIR",
"Stage5SolarReflectanceWeight_MWIR",
"Stage5SolarReflectanceWeight_LWIR",
"Stage5BodyDisplayGain",
"Stage5ReflectedDisplayGain",
"Stage5HotspotDisplayGain",
"Stage5BrightspotDisplayGain",
"Stage5CompositeMinGray",
"Stage5CompositeMaxGray",
"Stage5UseBaseTextureModulation",
"Stage5OutputFrameDump",
"Stage5OutputFrameDumpPath",
"Stage5OutputFrameDumpEvery",
"Stage6WhiteHot",
"Stage6DisplayGain",
"Stage6DisplayOffset",
"Stage6DisplayApplyToWindow",
"Stage6BackgroundDisplayEnable",
"Stage6NoiseEnable",
"Stage6NoiseSigmaGray",
"EnableStage7SkyHorizon",
"Stage7DebugMode",
"Stage7GroundZOffset",
"HwaSimIRExitOnStop"
)

$names | ForEach-Object {
  Remove-Item "Env:\$_" -ErrorAction SilentlyContinue
}
```

然后只设置这几个：

```powershell
$env:Stage6WhiteHot="1"
$env:Stage6DisplayGain="1"
$env:Stage6DisplayOffset="0"
$env:Stage6NoiseEnable="0"
$env:Stage6NoiseSigmaGray="0"
$env:Stage6BackgroundDisplayEnable="1"

$env:EnableStage7SkyHorizon="1"
$env:Stage7DebugMode="SkyGroundColor"
$env:Stage7GroundZOffset="11000"
```

启动：

```powershell
cd D:\HwaSimIR\ConsoleApplication1_LLA\Bin
.\ConsoleApplication1.exe
```

------

## 5. 三组重点手动测试

### 测天空球是否显示

```powershell
$env:Stage7DebugMode="SkyOnly"
$env:Stage7GroundZOffset="11000"
```

期望：窗口大面积是亮灰天空。如果还是黑，说明 sky dome 没渲染出来。

### 测地面是否位置合理

```powershell
$env:Stage7DebugMode="GroundOnly"
$env:Stage7GroundZOffset="11000"
```

期望：只看地面/海面平面，位置随相机姿态合理变化。

### 测天空+地面强制色

```powershell
$env:Stage7DebugMode="SkyGroundColor"
$env:Stage7GroundZOffset="11000"
```

期望：天空约亮灰，地面约中灰。如果这组正常，而 `Off` 异常，就是默认 skyGray/groundGray 标定问题；如果这组也异常，就是几何或 shader 问题。

------

## 6. DataDrivenTestQT 里改的不是 `$env`

这些不是 `$env`，要在 DataDrivenTestQT 初始化包里改：

```text
trackerSensorWidth
trackerSensorHeight
trackerSensorViewMin
trackerSensorViewMax
trackerSensorPixelAngle
noiseEn
trackerSensorNoise
envTerrain
envSky
envRadScaleTerrain
envRadScaleSky
```

其中 `trackerSensorViewMax` 会变成 HwaSimIR 的 `farClipM`，目标 50 km 时建议设 `60000~80000`。Stage6 会打印 `[Stage6 SensorGeometry]`，里面能看到 width、height、viewMax、FOV、near/far 的最终值。