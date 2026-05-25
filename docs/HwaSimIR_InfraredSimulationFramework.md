# HwaSimIR 基于三维场景的红外成像仿真框架设计

生成日期：2026-05-25  
目标仓库：`D:\HwaSimIR` / `feerodman/HwaSimIR`  
当前要求：本文件只做框架设计与实施路线，不修改 HwaSimIR 源码。

## 1. 给后续 Codex 的快速上下文

本仓库包含三个主要工程：

- `ConsoleApplication1_LLA/ConsoleApplication1`：当前 HwaSimIR 主体，Panda3D C++，VS2015/v140，带 CMake/RK3588 迁移雏形。核心入口为 `HwaSimIR.cpp/.h`。
- `DataDrivenTestQT`：Qt 5.12.12 激励发送端，通过 UDP 发送复位、初始化、开始、停止和实时数据。
- `MaterialTest`：Panda3D 材质映射试验工程，已验证“基础纹理 + 材质 ID 纹理 + shader uniform 数组”的方向。

当前 HwaSimIR 已有一版低复杂度红外雏形：

- `IRSimulation.h/.cpp` 定义 `IRBand`、材质库、大气透过率表和经验辐亮度模型。
- `HwaSimIR.cpp` 中已接入 `InitInfraredSimulation()`、`InitInfraredShader()`、`InitSkyAndCloudScene()`、`UpdatePlatformIRStatus()`。
- 当前 shader 输出灰度图，但内部已经较早把辐射归一化到 0~1，偏经验混合，并非完整物理单位链路。
- 当前目标材质按平台类型硬编码，例如 F35/J20 用铝，AIM120 用钢，AIM9 用陶瓷，没有按 OBJ/MTL/UV/纹理区域绑定材质。
- 当前视频输出链路通过 Panda3D render texture 拷贝 RGB 到内存，再由 `TcpCommThread` 翻转、缩放到 800x800、JPEG 编码并 TCP 发送。

论文《基于三维场景的红外成像仿真系统及实现_李晨阳.pdf》的核心框架：

1. 三维场景构建：几何特征数据库、纹理数据库、材质数据库。
2. 大气要素计算：太阳辐照度、上下行大气透过率、天空下行辐射、大气程辐射和大气热辐射。
3. 场景可见性计算：太阳直射可见性、天空半球可见性、面元间可见性。
4. 辐射能量场计算：温度场、入射场、出射场。
5. 混合像元。
6. 红外成像系统模型：把入瞳前辐亮度转换为 8-bit 灰度图像。

本文档后续每一阶段完成后，都应在第 13 节“阶段成果记录”追加结果、测试命令、已知问题和下一阶段建议。新聊天只读本文件即可快速续作。

## 2. 总体目标与边界

目标是在 Panda3D C++ 框架内实现可运行、可测试、可移植到 RK3588 Debian 11 aarch64 的三维场景红外实时仿真链路。输出只要求 8-bit 视频流，但内部计算应保留物理量，最后一步再映射到 0~255。

必须保留的核心计算：

- 波段积分或采样，不能只用“温度越高越亮”的经验灰度。
- 材质发射率、反射率、透过率、粗糙度和热参数参与计算。
- 目标自身热辐射、太阳相关反射辐射、大气衰减、程辐射/大气热辐射进入链路。
- 不同波段使用不同权重和简化策略：VIS/NIR/SWIR 重太阳反射，LWIR 重热辐射，MWIR 同时保留太阳与热辐射。
- 传感器参数、视场角、分辨率、噪声/MTF/AGC/8-bit 量化作为单独成像系统模块。

当前不做：

- CUDA。RK3588 的 Mali-G610 MP4 不支持 CUDA。未来可考虑 OpenGL ES shader、Vulkan compute、OpenCL 或离线预计算，但本阶段以 CPU + Panda3D GLSL shader 为主。
- 完整多次反射全局光照。先做论文链路的可控近似，保留接口。
- 高精度 MODTRAN 在线调用。先读现有谱透过率表，后续再支持多条件 MODTRAN 数据库。

## 3. 波段定义与协议扩展建议

建议统一波段枚举：

| 名称 | 范围 | 主要成像机制 | 首阶段计算策略 |
|---|---:|---|---|
| VIS 可见光 | 0.40-0.70 um | 太阳反射 | 用纹理亮度、太阳方向、材质反射率、大气透过率 |
| NIR 近红外 | 0.70-1.10 um | 太阳反射为主 | 与 VIS 类似，可弱化热辐射 |
| SWIR 短波红外 | 1.10-2.50 um | 太阳反射为主，高温目标可见 | 保留太阳反射，发动机高温可叠加热项 |
| MWIR 中波红外 | 3.00-5.00 um | 太阳反射 + 热辐射 | 全链路重点波段 |
| LWIR 长波红外 | 8.00-14.00 um | 热辐射为主 | 保留热辐射、大气透过率、程辐射，可忽略太阳反射一阶近似 |

现有 `trackerSensorBand` 注释只明确 `0=短波红外, 2=中波红外`，`IRRadianceModel::bandFromProtocol()` 又映射了 1=NIR、3=LWIR、4=Visible。建议正式约定：

- `0=SWIR`
- `1=NIR`
- `2=MWIR`
- `3=LWIR`
- `4=VIS`

如果需要可见光-近红外联合传感器，可增加 `5=VIS_NIR` 或 `5=VIS_SWIR`，从 `default_VIS-SWIR.json` 读取 `SpectralResponseRangeLow/High`。

## 4. 论文数学模型到程序链路的落地

### 4.1 基础辐射量

内部使用光谱辐亮度 `L_lambda`，单位建议统一为 `W/(m^2 sr um)`。最终输出 8-bit 之前不要提前归一化。

黑体光谱辐亮度采用普朗克公式：

```text
L_bb(lambda, T) = c1 / (lambda^5 * (exp(c2 / (lambda * T)) - 1))
```

其中 `lambda` 用 um 时，应使用与 um 一致的常数。程序中必须明确常数单位，避免把 `m` 与 `um` 混用。

物体自身热辐射：

```text
L_obj(lambda) = epsilon(lambda, material, T) * L_bb(lambda, T)
```

对不透明表面，一阶近似：

```text
rho(lambda) = clamp(1 - epsilon(lambda) - transmissivity(lambda), 0, 1)
```

若没有光谱曲线，先用 `MaterialDatabase.csv` 的 `Thermal Emissivity`、`Solar Absorptivity`、`Transmissivity` 和 `Roughness` 构造各波段常数。

### 4.2 下行入射辐射

下行入射包含：

- A 类：太阳直射。
- B 类：天空散射太阳光。
- G 类：周围背景反射太阳光。
- E 类：天空热辐射。
- H 类：背景热辐射。

首阶段落地公式：

```text
E_direct(lambda) = C_sun * tau_down(lambda) * E_sun(lambda) * max(0, dot(n, sun_dir))
E_sky(lambda)    = V_sky * L_sky(lambda) * pi
E_bg(lambda)     = V_bg  * L_bg(lambda)  * pi
E_down(lambda)   = E_direct + E_sky + E_bg
```

简化说明：

- `C_sun` 为云层/遮挡衰减，晴天为 1，云/雾按天气和云密度降低。
- `V_sky` 为天空可见因子。首阶段可按法线与天空半球估计，后续做 8x18=144 方向采样。
- `V_bg` 为背景可见因子。首阶段可使用环境地表平均辐亮度，后续做面元间可见性或屏幕空间近似。

### 4.3 零视距表面辐射

论文中零视距辐射由自身热辐射和反射辐射组成。首阶段用 Lambert 漫反射近似：

```text
L_surface(lambda) = epsilon(lambda) * L_bb(lambda, T)
                  + rho(lambda) * E_down(lambda) / pi
                  + L_hotspot(lambda)
```

其中 `L_hotspot` 用于发动机、尾喷口、毁伤、照明器、爆炸等高温或主动辐射源。对于目标模型，热源不应只靠模型整体温度，应支持局部材质或局部 mask。

### 4.4 上行出射到入瞳

大气上行传输：

```text
L_aperture(lambda) = tau_up(lambda, range, band, visibility) * L_surface(lambda)
                   + L_path(lambda, range, band, visibility)
                   + L_atm_thermal(lambda, range, band, visibility)
```

现有 `transmittance_0.3_15.txt` 只有波长-透过率和 `Reference_Path_Length=500m`。可先使用：

```text
tau_up(lambda, R) = tau_ref(lambda)^(R / ReferencePath)
```

但正式框架应扩展为按能见度、湿度、气溶胶、路径长度、目标高度、观测角和太阳几何索引的 MODTRAN 数据库。

### 4.5 波段积分

每个波段输出应由光谱响应加权：

```text
L_band = integral(lambda_low, lambda_high, S(lambda) * L_aperture(lambda) d_lambda)
       / integral(lambda_low, lambda_high, S(lambda) d_lambda)
```

若暂无真实 `S(lambda)`，先用矩形响应。采样点数量建议：

- VIS/NIR/SWIR：每 20-50 nm 一个样本，或至少 16 个样本。
- MWIR/LWIR：至少 16-32 个样本，避免普朗克曲线采样过粗。
- RK3588 首阶段可预计算每个材质/温度/波段的查找表，shader 只查表或接收 CPU 结果。

### 4.6 8-bit 成像系统

内部 `L_band` 进入传感器模型：

```text
signal = K * L_band + B + noise
dn14   = clamp(signal, 0, 2^ADCBit - 1)
dn8    = AGC_or_window_level(dn14)
```

首阶段可以实现：

- 辐射定标：线性 `K/B`。
- 自动增益 AGC：全帧 min/max 或低通平滑窗口。
- 噪声：NETD 相关高斯噪声、固定图样噪声、读出噪声。
- MTF/光学模糊：Panda3D 后处理 pass 或 OpenCV resize/blur。
- 输出：白热/黑热 8-bit 灰度，再按现有 RGB/JPEG/TCP 或后续 H.264/UDP 输出。

注意：输出要求是 8-bit 视频流，不代表内部只能 8-bit。建议内部至少 float 或 16-bit，最后一步压到 8-bit。

## 5. 现有数据资源评估

### 5.1 `materials/MaterialDatabase.csv`

有用，适合作为首版材质物性库。已有字段包括：

- `Solar Absorptivity`
- `Thermal Emissivity`
- `Characteristic length`
- `Specific Heat`
- `Conductivity`
- `Density`
- `Transmissivity`
- `Roughness`

不足：

- 不是光谱发射率/反射率曲线，只是宽波段或经验常数。
- 缺少飞机蒙皮、涂层、玻璃/座舱、发动机喷口、尾焰、导弹壳体、雷达罩等目标专用材质。
- 缺少温度边界、热源功率、热惯量初始条件和材料厚度。

建议扩展为：

```text
MaterialId, Name, Category, BaseEmissivity, SolarAbsorptivity,
Reflectance_VIS, Reflectance_NIR, Reflectance_SWIR,
Emissivity_MWIR, Emissivity_LWIR,
Conductivity, Density, SpecificHeat, ThicknessM, Roughness,
DefaultTemperatureK, ThermalClass, Notes
```

### 5.2 `transmittance/transmittance_0.3_15.txt`

有用，适合作为首版大气谱透过率表，覆盖 0.3-15 um，正好覆盖 VIS/NIR/SWIR/MWIR/LWIR。

不足：

- 当前看起来只有单一参考路径长度 500m。
- 只有透过率，没有 path radiance、sky radiance、solar irradiance 等完整 MODTRAN 输出。
- 没有随能见度、湿度、气溶胶、目标高度、观测角变化的索引。

建议阶段 1 继续使用该表，阶段 3 扩展为多条件表：

```text
AtmosphereProfile:
  location/date/time
  visibility_km
  humidity_percent
  aerosol_model
  observer_alt_m
  target_alt_m
  path_length_m
  wavelength_um -> tau_up, tau_down, path_radiance, sky_radiance, solar_irradiance
```

### 5.3 `temperatures/Temperatures_Yemen_Summer.csv`

有用，适合作为热平衡模型验证和默认气象样例。该文件包含纬度、经度、日期、日出日落、最大太阳辐照度、24 小时温度、太阳方位角、太阳高度角。

是否必须提供仿真经纬度当地数据：

- 如果只做功能测试，协议中的 `envTemp/envVisibility/envHumidity/envWindV/envWindDir/envSky` 可先驱动环境。
- 如果要让地表、天空、云、目标外表面温度随昼夜和地区变化可信，需要提供仿真经纬度、日期、当地逐小时温度/湿度/风速/太阳高度，或提供可由程序计算太阳位置的日期和地点。
- Yemen CSV 可作为默认 `YemenSummer` profile，但不应硬编码为所有场景。

### 5.4 `MaterialTest`

非常有用。它已经演示了论文中“纹理图 + 材质编号纹理”的思路：

- 基础纹理 `baseColorTex` 用于 VIS/NIR/SWIR 反射细节。
- 材质 ID 纹理 `materialIDTex` 用 nearest 采样，像素值映射材质编号。
- shader uniform 数组可传材质参数。

建议把这个思路正式合入 HwaSimIR 的资产预处理和 shader 管线，而不是只读取 `.mtl` 中的 `newmtl` 名称。

## 6. 模型格式与材质绑定建议

### 6.1 OBJ/MTL/JPG 的可行性

OBJ + MTL + JPG 可以继续作为首版主格式，原因：

- 当前资产已有 OBJ/JPG。
- 论文也采用 OBJ 保存几何、法线、UV。
- Panda3D 能加载 OBJ，MaterialTest 已验证 UV/纹理流程。

但仅靠 `.mtl` 不够。当前 AIM9 的 MTL 只有 `Material__4` 和 `map_Kd aim9.jpg`，不能说明纹理上每个区域是金属、涂层、陶瓷还是喷口。

推荐绑定方式：

1. 每个模型保留基础纹理，如 `aim9.jpg`。
2. 增加一张同 UV 的材质 ID 纹理，如 `aim9_mat_id.png/tif`，每个像素的 R 通道是材质 ID。
3. 增加模型材质映射表，如 `aim9_material_map.csv`：

```text
MaterialId, MaterialName, SemanticPart, Notes
0, BM_METAL-ALUMINIUM, body_shell, missile shell
1, BM_PAINT, paint, outer coating
2, IR_CERAMIC, nose, seeker dome
3, ENGINE_NOZZLE, rear_nozzle, dynamic hotspot
```

4. Panda3D shader 同时采样基础纹理和材质 ID 纹理，根据材质 ID 查参数。

### 6.2 EGG / FLT / glTF 的取舍

- `.egg`：Panda3D 原生友好，适合保留层级、节点名、动画、多个 texture stage、材质名和自定义 tag。建议中长期把稳定目标转成 EGG，提高加载稳定性和节点语义控制。
- `.flt`：OpenFlight 对传统视景仿真友好，仓库已有 F35/F16 FLT/EGG 资产；但 C++ 管线里要确认 Panda3D 构建是否带 FLT 支持，并且材质 ID/热参数绑定仍需要额外表。
- `.obj`：最简单、最透明，适合当前阶段和材质测试。
- `glTF`：现代 PBR 资产友好，但 Panda3D 旧环境/VS2015 兼容性需要验证。若后续升级 Panda3D，可考虑。

首阶段建议：继续 OBJ，新增材质 ID 纹理与映射表；对需要层级/运动部件的模型，再转 EGG。

## 7. Panda3D 模块化框架设计

建议把新框架作为 `IR/` 子模块逐步接入，避免继续扩大 `HwaSimIR.cpp`。

建议文件结构：

```text
ConsoleApplication1_LLA/ConsoleApplication1/IR/
  IRTypes.h
  IRSpectralBand.h/.cpp
  IRMaterialDatabase.h/.cpp
  IRAtmosphereDatabase.h/.cpp
  IRTemperatureModel.h/.cpp
  IRSceneMaterialMapper.h/.cpp
  IRRadianceModel.h/.cpp
  IRSensorModel.h/.cpp
  IRShaderManager.h/.cpp
  IRFramePipeline.h/.cpp
  IRDebugDump.h/.cpp
```

### 7.1 `IRTypes`

统一物理量、配置和运行时状态：

- `IRBandSpec`：波段范围、采样点、太阳/热权重策略。
- `IRSensorSpec`：FOV、分辨率、焦距、F 数、ADC bit、NETD、MTF、AGC、输出位深。
- `IREnvironmentState`：经纬度、日期时间、温度、湿度、风、能见度、天气、太阳方向。
- `IRObjectState`：平台 ID、类型、NodePath、材质映射、热源状态、距离、姿态。
- `IRRadianceResult`：保留 `L_surface`、`tau_up`、`L_path`、`L_band`、`dn8` 等中间值，便于调试。

### 7.2 `IRMaterialDatabase`

职责：

- 读取 `MaterialDatabase.csv`。
- 支持材料名、材料 ID、语义部位查询。
- 提供按波段估计的 `epsilon/rho/transmissivity/roughness`。
- 后续支持光谱曲线 CSV。

首阶段目标：

- 替代当前 `IRMaterialDatabase` 的简单字段读取。
- 保留向后兼容，仍可读取现有 CSV。
- 增加默认材质和错误报告，避免材质名拼错时静默落到铝。

### 7.3 `IRAtmosphereDatabase`

职责：

- 读取 `transmittance_0.3_15.txt`。
- 按波段和距离计算 `tau_up/tau_down`。
- 提供 `L_path`、`L_sky_thermal`、`L_sky_solar` 的经验或表格值。

首阶段目标：

- 用现有透过率表做谱积分。
- path radiance 用 `(1 - tau) * L_bb(T_air) * scale` 一阶近似。
- sky radiance 用天气、云密度、太阳高度和空气温度估计。

后续目标：

- 支持 MODTRAN 批量表：多个能见度、湿度、气溶胶、路径长度和观测角。

### 7.4 `IRTemperatureModel`

职责：

- 计算地表、云、天空、目标材料的温度。
- 区分环境热平衡、目标动态热源和毁伤/爆炸状态。

首阶段简化：

- 对每种材质维护 `baseTemperatureK`。
- `Temperatures_Yemen_Summer.csv` 或协议环境温度驱动日变化。
- 金属/涂层/陶瓷根据太阳吸收率、热惯量和风速给出温度偏移。
- 发动机、尾喷口、爆炸、照明器作为局部 hotspot。

后续物理模型：

- 地表热平衡：`E_sun + E_sky - H - LE - M_g - G = 0`。
- 建筑/目标外壳：`E_sun + E_sky - E_conv - M_g - G = 0`。
- 一维瞬态导热有限差分：

```text
dT/dt = alpha * d2T/dz2
alpha = conductivity / (density * specificHeat)
```

### 7.5 `IRSceneMaterialMapper`

职责：

- 从 Panda3D `NodePath/Geom` 读取模型、纹理、UV、材质名。
- 绑定基础纹理、材质 ID 纹理和材质参数表到 shader。
- 为 CPU 调试计算提供 per-object/per-material/per-part 参数。

首阶段目标：

- 复用 MaterialTest 思路，给 F35/AIM9/AIM120/J20 支持 `_mat_id` 纹理。
- 没有材质 ID 纹理时，使用 MTL `newmtl` 或平台默认材质作为 fallback。

### 7.6 `IRRadianceModel`

职责：

- 实现第 4 节公式，从材质、温度、环境、大气和距离求 `L_band`。
- 支持每对象 CPU 常量计算和 shader per-pixel 计算。

首阶段分工：

- CPU：计算环境、大气、材质、温度、波段积分中的低频量。
- Shader：根据材质 ID、基础纹理、法线、太阳方向、热源 mask 计算最终像素灰度。

注意：

- 不再在模型最前端直接输出 `baseRadiance 0~1`。
- 保留 debug 开关，能输出 `temperature/radiance/tau/materialId` 等中间图。

### 7.7 `IRSensorModel`

职责：

- 从协议 `trackerSensorParam` 和 `SensorWave/*.json` 生成真实传感器参数。
- 处理 FOV、分辨率、MTF、噪声、ADC、AGC、白热/黑热、8-bit 量化。

首阶段目标：

- 用 `trackerSensorWidth/Height` 设置 render texture 和输出尺寸，不固定 800x800。
- 用 `coarseTrackResolution/preciseTrackResolution` 或 `FOVH/FOVV` 设置 Panda3D lens。
- 输出 8-bit 灰度 RGB，兼容现有 TCP/JPEG 流。

### 7.8 `IRShaderManager`

职责：

- 管理 GLSL shader 源码、uniform、texture stage、后处理 pass。
- 兼容 Windows Panda3D 和 RK3588 OpenGL ES。

建议 shader 管线：

1. Object IR shader：每个目标/场景面元输出物理近似灰度或 radiance。
2. Sky/cloud shader：天空背景和云粒子，受天气、太阳高度、云密度影响。
3. Sensor postprocess shader：AGC、噪声、MTF/blur、黑热/白热和 8-bit 映射。

### 7.9 `IRFramePipeline`

职责：

- 将 HwaSimIR 的 UDP 状态、Panda3D scene graph、IR 模块和视频输出串起来。
- 保持同步渲染模式：一组 UDP 实时数据驱动一帧。

集成位置：

- `handleInitCmd()`：读取传感器、环境、波段、模型数量，初始化 IR 配置。
- `ProcessRealSimSceneInitData()`：加载目标并绑定材质/IR shader。
- `handleDisplayData()`：更新目标姿态、热源状态、跟踪/视线状态。
- `UpdatePlatformIRStatus()`：应改为调用 `IRFramePipeline::updateFrameConstants()`。
- `capture_task()`：只取最终 8-bit frame。

## 8. 天空背景与 Panda3D 粒子云设计

天空和云不能只作为普通背景色，应参与辐射链路：

- 天空背景：按波段、太阳高度、天气、空气温度、能见度输出 sky radiance。
- 云粒子：作为半透明/半遮挡物，影响 `C_sun`、`V_sky` 和局部亮度。
- 云在 VIS/NIR/SWIR 通常更受太阳照射影响，在 MWIR/LWIR 体现云顶温度和天空热辐射差异。

首阶段实现：

- 保留当前 camera-attached sky card。
- 云使用 Panda3D 粒子或 billboard card，增加 `cloudDensity/cloudTemperatureK/cloudOpticalDepth`。
- shader 中使用云密度调制 alpha 与 radiance。
- 云层遮挡太阳时降低 `C_sun`，可先用全局天气开关，后续按太阳方向和云分布估计。

后续增强：

- 使用 Panda3D 粒子系统或自定义 billboard manager 管理云团。
- 云的噪声纹理作为 density map。
- 天空半球采样时把云密度纳入 `V_sky`。

## 9. 目标热特征设计

目标热特征至少分四层：

1. 材质层：蒙皮、涂层、玻璃、雷达罩、喷口等物性不同。
2. 部件层：发动机、尾喷口、弹体头部、导引头、翼面、受损区域。
3. 状态层：发动机开关、毁伤、爆炸、照明器开关、速度和飞行时间。
4. 成像层：距离、大气、视角、遮挡和传感器响应。

首阶段热源参数：

```text
engine_on:
  nozzle_temperatureK: 650-950 for MWIR/LWIR visible hotspot
  plume_temperatureK: optional, use billboard/cone
damaged:
  local_temperatureK: +100-300K
  hotspot_radius_model_units
illuminator:
  VIS/NIR/SWIR active spot, not LWIR thermal source unless specified
```

需要用户后续提供或确认：

- F35/J20/AIM9/AIM120 的材质分区图或材质 ID 纹理。
- 发动机/喷口/尾焰位置，最好用模型局部坐标或命名节点。
- 发动机开机、关机、毁伤、爆炸时的温度范围或可接受视觉样例。
- 目标是否需要尾焰/烟羽/爆炸云，还是只要目标表面热斑。

## 10. 对现有代码的实施阶段

### 阶段 0：基线冻结与测试入口

目标：

- 记录当前 HwaSimIR 能接收 DataDrivenTestQT 数据、初始化、渲染、输出 TCP/JPEG 的基线。
- 加入最小调试输出，不改变行为。

建议任务：

- 整理当前运行命令、VS 配置、依赖路径。
- 确认 `DataDrivenTestQT` 的 `trackerSensorBand`、FOV、目标数量和实时数据文件。
- 建立 `docs/runbook_windows.md`。

验收：

- Windows 可启动 HwaSimIR 和 DataDrivenTestQT。
- 复位、初始化、开始、实时数据、停止流程可跑通。

### 阶段 1：配置与数据模型重构

目标：

- 从 `HwaSimIR.cpp` 拆出 IR 数据模型，不改变渲染结果。
- 让 SensorWave JSON、MaterialDatabase CSV、transmittance txt 成为统一配置输入。

建议任务：

- 新建 `IR/` 模块和 `IRTypes`。
- 扩展 `IRBand` 与协议映射。
- 读取 `SensorWave/default_*.json` 中的波段、ADC、NETD、FOV、分辨率等。
- 保留现有 `IRSimulation` API 兼容层，逐步迁移。

验收：

- 初始化时打印当前波段、谱范围、传感器尺寸、ADC bit、NETD。
- 切换 `trackerSensorBand` 能切换波段范围。

### 阶段 2：材质映射与目标资产绑定

目标：

- 模型不再用平台类型硬编码材质，而是支持 OBJ/MTL/UV/材质 ID 纹理。

建议任务：

- 引入 `IRSceneMaterialMapper`。
- 为至少 AIM9 或 F35 制作一套 `*_mat_id` 示例纹理和映射 CSV。
- shader 接入 `baseColorTex + materialIDTex + materialParamTex/array`。
- 没有材质 ID 时 fallback 到现有平台默认材质。

验收：

- MaterialTest 的双纹理思路迁移到 HwaSimIR。
- debug 模式可显示材质 ID 灰度图。

### 阶段 3：大气与环境模型

目标：

- 把现有透过率表升级为光谱-波段积分接口。
- 接入温度/太阳高度/天气/能见度。

建议任务：

- `IRAtmosphereDatabase` 读取 `transmittance_0.3_15.txt`。
- `IRWeatherProfile` 读取 `Temperatures_Yemen_Summer.csv`。
- 环境优先级：UDP 初始化参数 > 场景 profile > 默认值。
- 实现 `tau_up(lambda, R)`、`tau_down(lambda)`、`pathRadiance(lambda)` 近似。

验收：

- 改变目标距离，目标对比度随透过率变化。
- 改变能见度，图像整体雾化/程辐射变化。
- 改变仿真小时，VIS/SWIR 亮度随太阳高度变化，LWIR 随温度变化。

### 阶段 4：温度场与目标动态热源

目标：

- 实现材质温度随环境和状态变化，发动机/毁伤局部热源真实参与辐射。

建议任务：

- 首版先按材质热惯量和太阳吸收率估计温度偏移。
- 发动机/尾喷口/毁伤用局部 mask 或局部坐标 hotspot。
- 后续加入一维热传导有限差分缓存。

验收：

- 发动机关机后 MWIR/LWIR 热斑逐渐衰减，而不是瞬间消失。
- 毁伤状态切换后出现局部高温。
- LWIR 常温目标与背景有合理对比。

### 阶段 5：完整辐射链路

目标：

- 实现下行入射、零视距表面辐射、上行入瞳和波段积分。

建议任务：

- `IRRadianceModel` 保留物理单位到最后。
- 按波段采样积分。
- Shader 接收必要参数，per-pixel 计算材质、纹理、法线、热源。
- 对 RK3588，预计算材质/温度/波段 LUT，减少 shader exp 计算。

验收：

- VIS/NIR/SWIR 纹理细节和太阳方向明显。
- MWIR 太阳与热辐射都可见。
- LWIR 弱纹理、强温度和发射率差异。
- 同一场景不同波段差异符合论文表 5.8 趋势：SWIR 太阳项占主导，MWIR 两者相近，LWIR 热项占主导。

### 阶段 6：混合像元、成像系统与 8-bit 输出

目标：

- 输出符合传感器参数的 8-bit 视频流。

建议任务：

- Render texture 尺寸来自 `trackerSensorWidth/Height`。
- 增加 sensor postprocess：MTF/blur、noise、AGC、white-hot/black-hot、8-bit quantization。
- `TcpCommThread` 不再强制 800x800，除非用户配置要求。
- 保持现有 TCP/JPEG 可用；如需要 UDP/H.264 再单独做。

验收：

- 输出帧尺寸与初始化参数一致。
- `noiseEn/trackerSensorNoise` 生效。
- 8-bit 灰度可用，RGB 三通道相同或按显示模式输出。

### 阶段 7：天空、云和环境效果

目标：

- 天空背景和云粒子进入辐射模型，而非纯装饰。

建议任务：

- 用 `envSky` 映射晴/云/雨/雪/雾/阴。
- 云 billboard/particle 使用 density texture 和温度。
- 云层调制太阳直射 `C_sun`、天空漫射和背景亮度。

验收：

- 晴天、阴天、云天的 VIS/SWIR/MWIR/LWIR 背景和目标对比不同。
- 云在 LWIR 中体现云顶温度，透明度和密度可调。

### 阶段 8：调试、校准与性能

目标：

- 让每一段物理量可看、可记录、可对比。

建议任务：

- 增加 debug view：materialId、temperatureK、tau、radiance、dn8。
- 记录每帧耗时：UDP 更新、scene update、IR constants、render、capture、encode。
- 建立小场景回归：单一材质平板、单目标、不同距离/波段/时间。

验收：

- 每个物理模块有独立 debug 输出。
- 在 Windows 上满足功能测试帧率。
- 给 RK3588 移植列出瓶颈。

### 阶段 9：Linux/RK3588 迁移准备

目标：

- 在不做硬件加速专项的前提下，让代码结构可移植。

建议任务：

- 抽象 WinSock/Linux socket 差异，统一 UDP/TCP 接口。
- 清理 CMake：当前 `CMakeLists.txt` 指向 `main.cpp`，但实际入口是 `HwaSimIR.cpp` 中的 `main`，需要核对。
- 确认 Panda3D aarch64 构建、OpenGL ES 能力、shader 版本。
- 避免 VS/Windows 专属路径进入核心代码。

验收：

- Linux aarch64 能编译基础工程。
- 无 CUDA 依赖。
- shader 有 GLES 兼容版本。

## 11. 需要用户提供或确认的内容

优先级最高：

- 真实传感器参数：每个波段的谱响应 `S(lambda)`、分辨率、FOV、焦距、F 数、像元尺寸、ADC bit、NETD、积分时间、AGC/白热黑热规则。
- 目标模型材质分区：每个 OBJ 的材质 ID 纹理，或至少部位-材质表。
- 目标热源参数：发动机、喷口、尾焰、毁伤、爆炸、照明器的温度/位置/半径/持续时间。
- 输出协议：当前 TCP/JPEG 是否就是最终 8-bit 视频流，还是最终需要 UDP/H.264/裸灰度帧。

中优先级：

- 仿真经纬度、日期、时间、天气、湿度、风速、能见度，或对应地区逐小时气象 CSV。
- MODTRAN 输出：不同距离、能见度、湿度、气溶胶、观测高度、目标高度的 `tau_up/tau_down/path radiance/sky radiance/solar irradiance`。
- 天空/云的设计要求：晴、云、雾、雨、雪是否都要视觉上区分。

可后置：

- 高精度 BRDF 数据。
- 复杂地形和地物模型。
- Mali-G610 上的 shader/compute 加速方案。

## 12. 实施注意事项

- 不要把物理辐射量过早 clamp 到 0~1。只有显示/8-bit 输出阶段才做 tone mapping。
- 首阶段可简化背景互反射，但要保留 `E_bg/L_bg/V_bg` 接口。
- 每个模块都应能独立 debug，否则后续视觉不对时很难定位。
- 模型坐标和协议坐标已由 `GeoTransform`、`AttitudeTransform` 处理，IR 模块不应重复改姿态逻辑。
- 现有同步渲染模式“一组 UDP 数据一帧”适合功能测试，红外计算不要阻塞 UDP 线程。
- RK3588 目标上优先减少 CPU 每帧逐三角计算，尽量把 per-pixel 材质/辐射合成放 shader，把低频项表格化。

## 13. 阶段成果记录

后续每完成一个阶段，在本节追加一条记录，格式如下：

```text
### YYYY-MM-DD 阶段 N：标题

完成内容：
- ...

修改文件：
- ...

验证：
- 命令/操作：
- 结果：

发现的问题：
- ...

下一步：
- ...
```

### 2026-05-25 阶段设计：初始框架文档

完成内容：

- 阅读本地仓库结构，确认 HwaSimIR 主体、DataDrivenTestQT、MaterialTest 和数据资源。
- 阅读论文文本，提炼全链路模型：三维场景、大气要素、可见性、温度/入射/出射辐射场、混合像元、成像系统。
- 评估 `MaterialDatabase.csv`、`transmittance_0.3_15.txt`、`Temperatures_Yemen_Summer.csv` 和 `MaterialTest` 的用途。
- 设计基于 Panda3D C++ 的模块化 IR 框架和分阶段实施路线。

修改文件：

- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：

- 本阶段未修改 C++ 源码，未进行编译。
- 已确认 GitHub 仓库 `feerodman/HwaSimIR` 可访问，本地分支为 `main...origin/main`。

发现的问题：

- 当前已有 IR 代码是经验化简版，尚未保持完整物理单位链路。
- 当前传感器输出固定缩放到 800x800，与初始化分辨率未完全一致。
- 当前目标材质硬编码，缺少按 UV/材质 ID 的真实分区。
- 当前大气表只有单一透过率表，缺少 path radiance、sky radiance、多气象条件索引。

下一步：

- 从阶段 0 和阶段 1 开始，让后续 Codex 先建立基线运行记录和 IR 配置/数据模型，再逐步替换现有经验模型。

### 2026-05-25 阶段 0：基线冻结与测试入口

完成内容：
- 建立 Windows 基线运行手册，记录 HwaSimIR、DataDrivenTestQT 的启动顺序、工作目录、依赖、UDP/TCP 端口和手动冒烟流程。
- 建立阶段 0 自动检查入口，用于确认关键工程文件、资源文件、端口、传感器默认值、实时数据默认值、VS 工具集和 Qt 模块仍符合当前基线。
- 在 HwaSimIR 中加入低频 Stage0 控制台诊断输出，只记录启动端点、初始化参数、控制指令摘要、实时数据第 1 帧和每 100 帧摘要，不改变仿真状态机、网络协议、渲染逻辑或视频编码逻辑。

修改文件：
- `docs/runbook_windows.md`
- `tools/stage0_check.ps1`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_check.ps1`
- 结果：全部 Stage 0 检查通过。

发现的问题：
- DataDrivenTestQT 中存在注释保留的历史 IP，自动检查脚本已忽略 `//` 注释行，避免误判。
- 本阶段未进行 Visual Studio/Qt 编译，也未启动 Panda3D 窗口；这需要本机依赖路径确认后手动验证。

下一步：
- 在完成一次 Windows 手动冒烟后，把实际启动日志、初始化应答、TCP 视频接收情况补充到 `docs/runbook_windows.md` 或本节成果记录。
- 阶段 1 开始拆分 IR 配置与数据模型，优先统一 SensorWave、MaterialDatabase、transmittance 的加载入口。
