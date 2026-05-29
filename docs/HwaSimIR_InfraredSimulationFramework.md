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
### 2026-05-25 阶段 0 补充：固定 VS2015 与 Qt 5.12.12 自动构建

完成内容：
- 固定 VS2015 MSBuild 路径为 `C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe`。
- 固定 DataDrivenTestQT 使用 Qt 5.12.12 MinGW 7.3.0 64-bit：`D:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin\qmake.exe`。
- 增加阶段 0 自动构建脚本，默认构建 HwaSimIR `Release|x64` 与 DataDrivenTestQT `release`。
- 增加阶段 0 短时启动脚本，用于按正确工作目录启动 `ConsoleApplication1.exe` 和 `DataDrivenTestQT.exe`，并输出日志到 `logs/stage0`。
- 扩展阶段 0 检查脚本，加入 VS2015 MSBuild、Qt qmake、MinGW make 路径检查。

修改文件：
- `tools/stage0_build.ps1`
- `tools/stage0_smoke_run.ps1`
- `tools/stage0_check.ps1`
- `docs/runbook_windows.md`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_check.ps1 -Strict`
- 结果：全部 Stage 0 检查通过，包含 VS2015 MSBuild、Qt 5.12.12 qmake、MinGW make 路径检查。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：HwaSimIR 通过 MSBuild `Release|x64` 构建成功，输出 `D:\HwaSimIR\ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe`；DataDrivenTestQT 通过 Qt 5.12.12 MinGW release 构建成功，输出 `D:\HwaSimIR\build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe`。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_smoke_run.ps1 -Seconds 8`
- 结果：两个进程均成功启动并保持运行，随后由脚本停止；日志输出到 `logs/stage0`。HwaSimIR 日志确认 UDP/TCP 基线端点、材质库、MODTRAN 透过率、天空云初始化成功。

发现的问题：
- 首次在沙箱内运行 MSBuild 时无法读取 `C:\Users\kahn1\AppData\Local\Microsoft\MSBuild\v4.0\Microsoft.Cpp.x64.user.props`，使用允许的本机权限重新运行后构建成功。
- DataDrivenTestQT 短时启动未自动点击复位/初始化/开始按钮，因此本次只验证进程启动，不验证完整按钮流程。

下一步：
- 如需自动化完整阶段 0 流程，可后续增加 DataDrivenTestQT UI 自动点击或独立 UDP 激励回放脚本。

### 2026-05-26 阶段 1：配置与数据模型重构

完成内容：
- 新增 `IR/IRTypes.h/.cpp`，集中定义 `IRBand`、默认谱段、协议波段映射、默认 SensorWave 文件名和传感器 profile 数据结构。
- 新增 `IR/IRConfig.h/.cpp`，读取 `ConsoleApplication1_LLA/Bin/Config/SensorWave/default_*.json`，解析谱范围、宽高、FOV、焦距、探元间距、ADC bit、Display bit、NETD。
- 保留 `IRSimulation` 兼容 API，`IRRadianceModel::bandFromProtocol/rangeForBand/bandName` 转发到新 IR 类型模块，避免一次性改动辐亮度模型。
- HwaSimIR 启动时统一加载 SensorWave、MaterialDatabase、transmittance 三类配置输入。
- HwaSimIR 启动、初始化命令、运行时波段变化时打印当前传感器 profile，包括协议波段、波段名、SensorWave 谱范围、当前物理模型谱范围、尺寸、ADC bit、Display bit、NETD、FOV、来源文件。
- 增加 `tools/stage1_check.ps1`，检查阶段 1 模块、VS 工程项、协议映射和 SensorWave JSON 字段。
- 增加 `tools/stage1_band_switch_smoke.ps1`，启动 HwaSimIR 并依次发送 `trackerSensorBand=0,1,2,3,4` 初始化包，验证波段切换日志。

修改文件：
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRTypes.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRTypes.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRConfig.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRConfig.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj.filters`
- `ConsoleApplication1_LLA/ConsoleApplication1/CMakeLists.txt`
- `tools/stage1_check.ps1`
- `tools/stage1_band_switch_smoke.ps1`
- `docs/runbook_windows.md`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_check.ps1 -Strict`
- 结果：通过。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage1_check.ps1 -Strict`
- 结果：通过；输出 VIS/NIR/SWIR/MWIR/LWIR SensorWave profile 矩阵。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：HwaSimIR `Release|x64` 与 DataDrivenTestQT Qt 5.12.12 release 构建成功。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_smoke_run.ps1 -Seconds 8`
- 结果：两个 exe 成功启动并保持运行；HwaSimIR 日志确认 SensorWave、MaterialDatabase、transmittance 加载成功，并打印 MWIR 启动默认 profile。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage1_band_switch_smoke.ps1`
- 结果：HwaSimIR 接收 `trackerSensorBand=0,1,2,3,4` 初始化包，并分别打印 SWIR、NIR、MWIR、LWIR、VIS 的谱范围、ADC bit、Display bit、NETD。

发现的问题：
- SensorWave profile 多数为 `640x480`，而 DataDrivenTestQT 当前初始化协议发送 `640x512`；阶段 1 只记录两者，不改变输出尺寸，阶段 6 再统一传感器输出尺寸。
- `default_SWIR.json` 的低端谱范围为 `1.5um`，当前物理模型默认 SWIR 为 `1.1-2.5um`；阶段 1 保持旧模型范围以避免改变渲染结果，后续接入真实谱响应时需要决定是否以 SensorWave 为准。
- VIS/NIR 的 SensorWave 文件目前都覆盖 `0.4-1.1um`，物理模型仍区分 VIS `0.4-0.7um` 和 NIR `0.7-1.1um`。
- 波段切换冒烟加载 F35C/AIM120 OBJ 时出现 Assimp `.mtl` 缺失 fallback 日志；这不阻塞阶段 1，但阶段 2 材质映射需要处理模型材质文件或材质 ID 贴图。

下一步：
- 阶段 2 将目标模型材质从平台类型硬编码迁移到 OBJ/MTL/材质 ID 纹理映射，并对缺失 `.mtl` 的资产给出 fallback 或补齐资源。

### 2026-05-26 阶段 2：材质映射与目标资产绑定

用户补充：
- 目标模型已迁移到 `ConsoleApplication1_LLA/Bin/Config/TargetLib/models/` 的分目录资产：`f35`、`f22`、`aim120`、`aim9x`。
- 已删除 J20 资源，新增 F22 资源；当前没有 MMD 模型。
- 协议映射保持不变：`0x11 -> F35`、`0x22 -> AIM120`、`0x33 -> AIM9`、`0x44 -> MMD`，其中 `0x11` 仍“飞机类型暂时默认F35”。

完成内容：
- 扩展 `PlatformResPath`，从单纯 `modelPath/texturePath` 扩展为模型、基础纹理、材质 ID 纹理、材质映射 XML、资产目录、显示名、默认红外材质。
- 新增 `IR/IRSceneMaterialMapper.h/.cpp`，读取 `*_mat.tif.xml` 的 `Composite_Material index`，优先使用 `Surface_Substrate`，没有表层材质时回退到 `Primary_Substrate`，并映射到 `MaterialDatabase.csv` 中的 `BM_*` 物理材质。
- 将 F35、AIM120D、AIM9X 绑定到新的分目录资源：
  - F35：`models/f35/F35C.obj`、`f35c.jpg`、`f35c_mat.tif`、`f35c_mat.tif.xml`，默认材质 `BM_METAL-ALUMINIUM`。
  - AIM120D：`models/aim120/AIM120.obj`、`aim120.jpg`、`aim120_mat.tif`、`aim120_mat.tif.xml`，默认材质 `BM_METAL-STEEL`。
  - AIM9X：`models/aim9x/aim9x.obj`、`TX_AIM9X_Diffuse.png`、`TX_AIM9X_Diffuse_mat.tif`、`TX_AIM9X_Diffuse_mat.tif.xml`，默认材质 `IR_CERAMIC`。
- F22 资产已纳入阶段 2 检查，但暂不绑定协议枚举；后续若需要使用 F22，需要新增协议类型或明确是否把 `0x11` 从 F35 改为 F22。
- MMD 协议映射保留，但因暂无模型资源，运行时不会生成 MMD 资源。
- 红外 shader 新增材质 ID 通道和材质参数数组：基础纹理使用 `p3d_Texture0`，材质 ID 纹理通过第二个 `TextureStage` 进入 `p3d_Texture1`，材质参数数组提供发射率、反射率、太阳吸收率、粗糙度。
- 保留 CPU 辐亮度模型的平台默认材质计算；像素级材质差异先在 shader 中按材质 ID 调制局部发射率/反射率。`u_debug_material_id` 默认关闭，后续可打开为灰度材质 ID 调试视图。
- 修正活动模型 `.mtl` 中残留的旧机器绝对贴图路径，改成同目录相对路径，避免 Windows/RK3588 路径不一致和 Panda3D/Assimp 无效路径日志。
- 新增 `tools/stage2_check.ps1`，检查阶段 2 代码、VS/CMake 工程项、资源路径、协议映射、MTL 相对路径、材质 XML 解析结果。

修改文件：
- `ConsoleApplication1_LLA/ConsoleApplication1/Common/CommonDefine.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRSceneMaterialMapper.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRSceneMaterialMapper.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj.filters`
- `ConsoleApplication1_LLA/ConsoleApplication1/CMakeLists.txt`
- `ConsoleApplication1_LLA/Bin/Config/TargetLib/models/f35/F35C.mtl`
- `ConsoleApplication1_LLA/Bin/Config/TargetLib/models/aim120/AIM120.mtl`
- `ConsoleApplication1_LLA/Bin/Config/TargetLib/models/aim9x/aim9x.mtl`
- `ConsoleApplication1_LLA/Bin/Config/TargetLib/models/f22/f22.mtl`
- `tools/stage2_check.ps1`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage2_check.ps1 -Strict`
- 结果：通过；检查到 F35/AIM120D/AIM9X/F22 的材质 XML 行，包含 `BM_METAL-ALUMINIUM`、`BM_PAINT`、`BM_GLASS`、`BM_METAL-IRON` 等映射。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：HwaSimIR `Release|x64` 与 DataDrivenTestQT Qt 5.12.12 release 构建成功。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage1_band_switch_smoke.ps1 -Bands 2 -DelayMs 500`
- 结果：HwaSimIR 启动后接收 MWIR 初始化包，实际加载 F35、AIM120D、AIM9X；日志显示 `materialIdTex=OK materialMap=OK entries=4`；stderr 仅保留 Panda3D 启动信息，没有旧绝对 MTL 贴图路径、材质 ID shader input、贴图读取失败或模型加载失败错误。

发现的问题与后续需求：
- 当前 per-pixel 材质参数已进入 shader，但 CPU 辐亮度模型仍按平台默认材质计算；阶段 3 起需要把材质 ID 对应的温度/热惯量/发动机热源分区继续下沉到物理模型。
- F22 资源暂不参与运行时生成；如果要切换飞机模型，需要你确认协议类型或目标类型编码。
- MMD 暂无模型、基础纹理、材质 ID 纹理和材质 XML；如果后续要生成 MMD，需要补齐资产。
- 如果后续希望对喷口、机头、弹体、舵面等部位设定独立温度或热源，需要提供更细的材质 ID 语义说明或部件标注表。

### 2026-05-27 阶段 3：大气与环境模型

用户补充与设计结论：
- `Temperatures_Yemen_Summer.csv` 是 Yemen 地区样例 profile。功能测试可继续作为默认 profile，但真实仿真应替换为任务区域的经纬度、日期、逐小时温度、湿度、风速、能见度和太阳位置数据，否则太阳高度、方位角、地表/空气温度日变化会与场景地区不一致。
- `temperatures/1.新版_太阳位置计算表格.xlsx` 有用，适合作为离线太阳位置计算器和 CSV profile 校验工具；运行时代码不依赖 Excel，后续可把 Excel 结果导出成同结构 CSV。
- 阶段 3 暂不新增 Panda3D `DirectionalLight/AmbientLight` 模拟太阳/月亮。当前红外图像由辐亮度模型和 shader 参数合成，直接加 Panda3D 光源会影响可见纹理光照但不会自动进入红外物理链路，且可能与太阳反射项重复。后续如果做 VIS/NIR 预览或阴影/遮挡，可再把太阳/月亮光源作为可视化层，同时仍以物理环境参数驱动红外辐射。
- 用户已临时关闭常开尾喷口亮斑和发动机状态映射尾部亮斑；阶段 3 保持该状态，不重新打开热点。

完成内容：
- 扩展 `IRRuntimeEnvironment`，增加湿度、风速、风向、太阳方位角、仿真小时、天气码等环境量。
- 扩展 `IRAtmosphereModel::transmittanceForRange`，在 MODTRAN 平均谱透过率基础上按距离和能见度修正上行透过率，低能见度时目标对比度随距离更快下降。
- 新增 `IRWeatherProfile`，读取 `Temperatures_Yemen_Summer.csv` 的经纬度、日期、最大太阳辐照度、日温度范围和逐小时 `Time/TemperatureC/SunAzimuth/SunElevation`，并按仿真小时线性插值。
- 在 HwaSimIR 启动阶段加载 WeatherProfile，默认使用正午样例环境；初始化命令和运行时通过 `BuildRuntimeEnvironment()` 合成环境状态。
- 环境优先级实现为：UDP 初始化参数 `envTemp/envVisibility/envHumidity/envWindV/envWindDir/envSky` > 场景 profile > 内置默认值。
- 太阳强度按太阳高度和天气码近似衰减：晴天保留直射，云/雨/雪/雾/阴天按经验系数降低太阳项。
- `IRRadianceModel::evaluate` 接入环境状态：VIS/NIR/SWIR 更受太阳高度和太阳强度影响，MWIR/LWIR 更受空气温度、湿度、大气程辐射和天空热辐射影响；风速会带来简化冷却项。
- 天空和云的辐亮度开始受太阳高度、湿度、透过率、云密度影响，为阶段 7 的天空/云物理化保留入口。
- 新增 `tools/stage3_check.ps1`，检查天气 profile、太阳位置 workbook、环境字段、CSV loader、能见度透过率、UDP 覆盖、仿真时刻和阶段 3 环境日志。
- 将 `IRSimulation.h` 保存为 UTF-8 BOM，避免 VS2015 用 CP936 误读中文注释导致行注释吞掉下一行声明。

修改文件：
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.cpp`
- `tools/stage3_check.ps1`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_check.ps1 -Strict`
- 结果：通过；确认天气 CSV、太阳位置 workbook、环境字段、CSV loader、能见度透过率、UDP 覆盖、仿真时刻、阶段 3 环境日志均存在；样例输出读取到 Yemen profile 的 0-5 时温度和太阳角。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：HwaSimIR `Release|x64` 与 DataDrivenTestQT Qt 5.12.12 release 构建成功；`IRSimulation.h` 的 C4819 编码警告已消失，剩余为既有 `math_algorithm.h` 未使用变量和 Panda3D 头文件 DLL/类型转换警告。
- 命令：`powershell -ExecutionPolicy Bypass -Command "& .\tools\stage1_band_switch_smoke.ps1 -Bands @(0,1,2,3,4) -DelayMs 500"`
- 结果：HwaSimIR 依次接收 `trackerSensorBand=0,1,2,3,4` 初始化包，分别打印 SWIR、NIR、MWIR、LWIR、VIS profile；阶段 2 材质绑定仍为 `materialIdTex=OK materialMap=OK entries=4`；阶段 3 启动和初始化环境日志正常输出，stderr 仅保留 Panda3D 启动信息。
- 附加观察：误传 `-Bands 0,1,2,3,4` 给 `powershell -File` 时会被解析成异常 `1234`，程序按非法协议波段 fallback 到 MWIR 且不崩溃；后续脚本调用多值数组建议使用 `powershell -Command "& .\script.ps1 -Bands @(0,1,2,3,4)"`。

发现的问题与后续需求：
- 当前环境 profile 只包含温度、太阳方位角和太阳高度，湿度/风速/能见度仍主要来自 UDP 或默认值；真实地区仿真需要提供目标区域的逐小时气象 profile 或更完整的 MODTRAN 条件表。
- 当前 MODTRAN 只有单条件谱透过率，阶段 3 先按能见度做经验缩放；后续若要物理可信，需要提供不同距离、湿度、能见度、气溶胶、观测高度、目标高度的 `tau_up/tau_down/path radiance/sky radiance/solar irradiance` 表。
- 当前太阳/月亮没有作为 Panda3D 光源加入，红外链路只使用太阳高度/方位角/天气系数；若后续要阴影、遮挡或可见光真实光照，需要确认是否接入 Panda3D 光源和阴影贴图。
- 当前运行 smoke 只发送初始化包，没有发送实时激励帧，因此 `CurrentSimulationHour()` 的逐帧时刻变化路径由静态检查覆盖，后续可增加实时数据包 smoke 验证昼夜变化。

下一步：
- 阶段 4 应继续处理目标温度场和动态热源，把材质热惯量、太阳吸收、风冷、发动机/毁伤/喷口热源从平台默认值推进到部位或材质 ID 层级。

### 2026-05-28 MODTRAN LUT 离线数据工具链

完成内容：
- 按 NIR/MWIR 优先、VIS/SWIR/LWIR 稀疏保底的策略新增 `tools/modtran/modtran_grid_nir_mwir_priority.json`，默认 production sparse grid 为 2616 个 case，未超过 3000 上限。
- 新增 `tools/modtran/parse_modout2.py`，基于手工保存的 PcModWin5 `MODOUT2` 样例解析 `Transmittance`、`ThermalRadiance`、`DirectSolarIrradiance`、`RadianceWithScattering` 四类输出，并计算 `wavelength_um = 10000 / wavenumber_cm`。
- 新增 `tools/modtran/build_modtran_cases.py`，支持 `--dry-run` 生成 `case_manifest.csv` 和每个 case 的 `modin`，不调用 MODTRAN。
- 新增 `tools/modtran/run_modtran_cases.ps1`，为 pilot 运行预留流程；如果无法确认 PcModWin5/MODTRAN5 命令行可执行文件，会停止并说明原因，不伪造结果。
- 新增 `tools/stage3_modtran_lut_check.ps1` 和 `docs/modtran_lut_format.md`，检查目录、模板、配置、processed 表头、manifest 数量和已生成 modin 是否完整。
- 新增 `processed/path_lut_spectral.csv`、`solar_lut_spectral.csv`、`sky_lut_spectral.csv`、`band_lut.csv`、`manifest.csv` 和 `qc_report.md` 的表头/占位报告。
- 未修改 HwaSimIR C++ 主逻辑；新 MODTRAN LUT 仍作为后续可选增强数据源准备，不替换现有 `transmittance_0.3_15.txt`。

验证：
- 命令：`python tools\modtran\parse_modout2.py --input ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\templates\MWIR_transmittance_MODOUT2.txt --band MWIR --mode Transmittance --output ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples\MWIR_transmittance_parse_smoke.csv`
- 结果：通过，解析 1334 行。
- 命令：`python tools\modtran\parse_modout2.py --input ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\templates\MWIR_thermal_MODOUT2.txt --band MWIR --mode ThermalRadiance --output ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples\MWIR_thermal_parse_smoke.csv`
- 结果：通过，解析 1334 行。
- 命令：`python tools\modtran\parse_modout2.py --input ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\templates\MWIR_solar_MODOUT2.txt --band MWIR --mode DirectSolarIrradiance --output ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples\MWIR_solar_parse_smoke.csv`
- 结果：通过，解析 1341 行。
- 命令：`python tools\modtran\parse_modout2.py --input ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\templates\NIR_scattering_MODOUT2.txt --band NIR --mode RadianceWithScattering --output ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples\NIR_scattering_parse_smoke.csv`
- 结果：通过，解析 5196 行。
- 命令：`python tools\modtran\build_modtran_cases.py --config tools\modtran\modtran_grid_nir_mwir_priority.json --dry-run --clean`
- 结果：通过；生成 2616 个 case、2616 个 `generated/modin/*_modin.txt`，并同步写入 `processed/manifest.csv`。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；目录、模板、配置、processed 表头、manifest 数量和 generated modin 均检查成功。
- 命令：`powershell -NoProfile -Command '$errors=$null; [System.Management.Automation.PSParser]::Tokenize((Get-Content -Raw tools\modtran\run_modtran_cases.ps1), [ref]$errors) > $null; if ($errors) { $errors; exit 1 }'`
- 结果：通过；pilot runner PowerShell 语法检查无错误。

发现的问题与后续需求：
- 当前只完成 dry-run 和样例解析，没有实际调用 PcModWin5/MODTRAN5 批量运行；正式 pilot 前需要确认可用命令行入口，并用 `run_modtran_cases.ps1 -Pilot -Executable <真实可执行文件>` 小批量验证。
- VIS/SWIR 的 solar/scattering 手工模板尚未单独提供，dry-run 暂使用 NIR 同模式模板替换波数范围生成保底 case；如果后续需要更可信的 VIS/SWIR solar/scattering，应补齐对应手工模板。
- `unit_radiance` 和 `unit_irradiance` 先记录为 MODOUT2 native，需要在确认 PcModWin5 单位设置后固化为物理单位。

下一步：
- 先用 pilot grid 跑不超过 100 个真实 MODTRAN case，确认命令行调用、失败样本归档、processed CSV 追加和 QC 报告链路，再进入 production sparse grid。

### 2026-05-28 MODTRAN LUT 单 case 自动化验证

完成内容：
- 更新 `.gitignore`，忽略 `MODTRAN/generated/`、`raw/failed/`、`raw/archive/` 和临时 parser smoke CSV，避免 2616 个 dry-run `modin` 进入提交视野。
- 增强 `tools/stage3_modtran_lut_check.ps1`：Strict 模式下检查 NIR/MWIR 优先模板是否存在、非空、MODOUT2 是否有可识别表头；若 NIR trans/solar 手工模板缺失，会明确提示先从 PcModWin5 GUI 生成，不继续 pilot。
- 新增 `tools/modtran/find_modtran_entry.ps1`，只扫描 `F:\Programs\PcModWin5\bin` 下的 `*.exe/*.bat/*.cmd`，输出候选路径、大小、修改时间和提示，不盲目执行。
- 读取 `F:\Programs\PcModWin5\bin\Modtran.bat` 后确认其含 `PAUSE`，不适合自动化；真实命令行引擎为 `F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe`。runner 已补齐 `modin -> tape5`、`TAPE6/7/8 -> MODOUT1/2/3` 的同步流程。
- 更新 `tools/modtran/run_modtran_cases.ps1` 并新增根入口 `tools/run_modtran_cases.ps1`，支持 `-SingleCase`、`-CaseId`、`-CaseLimit`、`-ModtranExe`、`-PcModWinRoot`、`-NoDeleteRaw`；硬性限制最多 6 个真实 case，禁用 72-case pilot，且使用 lock file 保证单线程。
- QC 报告改为真实 case 级统计，记录 case_id、band、mode、wavelength range、row count、tau/radiance/irradiance min/max、parser column mapping 和 warnings/errors。

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；优先模板、配置、processed 表头、manifest、generated modin 引用、真实单 case 后的 tau 范围和 wavelength/wavenumber 一致性均检查成功。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\modtran\find_modtran_entry.ps1 -PcModWinRoot "F:\Programs\PcModWin5"`
- 结果：通过；候选包括 `Mod5.2.1.0.exe`、`PcModWin5.exe`、`Modtran.bat` 等，未执行任何候选。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -SingleCase -CaseLimit 1 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe" -NoDeleteRaw`
- 结果：通过；只运行 1 个 `MWIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault` case，MODTRAN5 v5.2.1.0 输出 2000-3330 cm^-1 进度，解析 `usr\MODOUT2` 得到 1334 行；样本复制到 `raw/samples/MWIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault/`。

发现的问题与后续需求：
- `Modtran.bat` 是有 `PAUSE` 的交互 wrapper，不应作为自动化入口；后续继续使用 `Mod5.2.1.0.exe` 并由 runner 负责 tape 文件同步。
- 当前只真实运行了 1 个 MWIR Transmittance case。样本不足以做距离/能见度趋势判断，QC 报告只做基础范围检查。
- 若继续扩展到 6 case，应按固定白名单运行 MWIR trans/thermal/solar 与 NIR trans/solar/scattering，仍不能启动 72-case pilot 或 2616-case production。

下一步：
- 在确认 1 case 结果可接受后，可运行最多 6 case 的白名单验证，再决定是否生成 band_lut 的矩形响应积分和更完整 QC。

### 2026-05-28 MODTRAN LUT 6 case 白名单验证

完成内容：
- 新增 `-ValidationSix` 入口，只选择固定白名单：MWIR trans/thermal/solar 与 NIR trans/solar/scattering，各 1 个 case；仍禁用 72-case pilot 和 2616-case production。
- 6 个 case 均使用 `F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe` 单线程真实运行，输出固定文件名由 runner 串行复制，避免互相覆盖。
- 每个 case 的 `modin`、`MODOUT1`、`MODOUT2` 已保存到 `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/samples/<case_id>/`。
- 解析结果分别追加到 `processed/path_lut_spectral.csv`、`processed/solar_lut_spectral.csv`、`processed/sky_lut_spectral.csv`，`processed/manifest.csv` 记录 6 行且状态为 `validation_succeeded`。
- 新增 `tools/modtran/build_validation_band_lut.py`，用波长域矩形响应积分生成最小 `processed/band_lut.csv`，当前为 MWIR 和 NIR 两行。
- `unit_radiance` 和 `unit_irradiance` 继续记录为 `MODOUT2_native`，未伪造 SI 单位；未删除旧 `transmittance_0.3_15.txt`；未修改 HwaSimIR C++ 主逻辑。

验证：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ValidationSix -CaseLimit 6 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe" -NoDeleteRaw`
- 结果：通过；只运行 6 个 validation case。解析行数分别为 MWIR Transmittance 1334、MWIR ThermalRadiance 1334、MWIR DirectSolarIrradiance 1341、NIR Transmittance 5196、NIR DirectSolarIrradiance 5206、NIR RadianceWithScattering 5196。
- 命令：`python tools\modtran\build_validation_band_lut.py --processed-dir ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed`
- 结果：通过；写入 2 行矩形响应 band LUT。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；检查模板、processed 表头、6 行 manifest、modin 引用、tau 范围、wavelength/wavenumber 一致性和 band_lut tau 范围。

发现的问题与后续需求：
- 6 case 仍只是白名单 validation，不足以给出 production 级距离/能见度趋势结论；QC 报告只做基础范围检查。
- `MODOUT2_native` 的辐亮度/辐照度单位仍需从 PcModWin5/MODTRAN5 导出设置中确认后再固化。
- 后续若要进入 72-case pilot，应由用户明确批准；当前状态不应自动继续运行更多 case。

### 2026-05-28 MODTRAN LUT 72 case Pilot 验证

完成内容：
- 新增 `tools/modtran/check_validation_outputs.py`，在 Pilot72 前审查 6-case validation 输出：processed 文件存在、manifest 恰好 6 行且 `validation_succeeded`、band_lut 含 NIR/MWIR 两行、tau 范围有效、NIR/MWIR 必要积分字段非空。
- 新增 `tools/modtran/build_band_lut.py`，按 `band/observer/target/range/visibility/solar_zenith` 合并 transmittance、thermal/solar/scattering 谱积分，生成 pilot `band_lut.csv`。
- 更新 runner 支持 `-Pilot72 -CaseLimit 72`，固定 grid 为 NIR/MWIR、obs10/tar10、range 1/5/20/50 km、visibility 5/23/50 km、Rural/default、solar_zenith 45；仍单线程运行，失败即停。
- 72 个 case 已真实运行并保存到 `raw/samples/<case_id>/`，`processed/manifest.csv` 记录 72 行，`status/stage` 为 `pilot72_succeeded`。
- `qc_report.md` 增加 `Pilot72 Summary`，包含 band/mode 覆盖、tau_up_band 矩阵、MWIR path_radiance 趋势、NIR solar/scattering 非空检查、失败列表和趋势 QC。
- 未启动 2616-case production sparse grid，未修改 HwaSimIR C++ 主逻辑。

验证：
- 命令：`python tools/modtran/check_validation_outputs.py --processed-dir ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed`
- 结果：通过；6-case validation 输出审查通过。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -Pilot72 -CaseLimit 72 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe" -NoDeleteRaw`
- 结果：通过；只运行 72 个 Pilot case。各 band/mode 均为 12 case。
- 命令：`python tools\modtran\build_band_lut.py --processed-dir ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed`
- 结果：通过；写入 24 行矩形响应 band LUT。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；processed 行数为 path 94368、solar 78564、sky 62352、manifest 72、band_lut 24，tau 范围与 wavenumber/wavelength 一致性检查通过。

发现的问题与后续需求：
- Pilot72 趋势 QC 当前为 `PASS`，失败 case 为 0；但 production 仍需明确授权后才可运行。
- Pilot72 输出显示同一 range 下不同 visibility 的 band 积分差异很小或相同，应在后续确认 PcModWin5 模板中 visibility 字段是否确实作用于对应模式。
- 辐亮度/辐照度单位仍为 `MODOUT2_native`，需要确认 PcModWin5/MODTRAN5 单位设置后再固化到 SI 或项目单位。

### 2026-05-28 MODTRAN LUT visibility effect audit 与 smoke

完成内容：
- 新增 `tools/modtran/check_visibility_effect.py`，从 `processed/band_lut.csv`、`processed/manifest.csv`、`raw/samples/*/modin` 和 `raw/samples/*/MODOUT1` 审计 Pilot72 的 visibility 设置是否进入模板和 MODTRAN 输出。
- 审计结果记录 requested visibility、`modin` 中疑似 visibility token、`MODOUT1` 中解析到的 aerosol/meteorological range 文本，以及 Pilot72 组合对应的 `tau_up_band`。
- runner 增加 `-VisibilitySmoke18 -CaseLimit 18`，固定只跑 NIR/MWIR transmittance，altitude pairs 为 `(3,3)`、`(10,10)`、`(20,3)`，range 为 50 km，visibility 为 5/23/50 km，aerosol/humidity 为 Rural/default。
- smoke 运行保持单线程、失败即停；每个 case 的 `modin`、`MODOUT1`、`MODOUT2` 保存到 `raw/samples/<case_id>/`。
- smoke 光谱结果追加到 `processed/path_lut_spectral.csv` 和 `processed/manifest.csv`，`status/stage = visibility_smoke_succeeded`。
- 新增独立的 `processed/band_lut_visibility_smoke.csv`，保存 18 行 smoke band 积分结果，不覆盖 Pilot72 的 `band_lut.csv`。
- `qc_report.md` 增加 `Visibility Effect Audit` 和 smoke summary；没有启动 production sparse grid，也没有修改 HwaSimIR C++ 主逻辑。

验证：
- 命令：`python tools\modtran\check_visibility_effect.py --processed-dir ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed --raw-dir ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples`
- 结果：通过。Pilot72 的 `modin` 和 `MODOUT1` 证据显示 requested visibility 5/23/50 km 已进入输入/输出；Pilot72 的 10 km/10 km 高空水平路径 band 积分 tau 仍可能在不同 visibility 下相同，因此记录为 warning，而不是模板失败。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -VisibilitySmoke18 -CaseLimit 18 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe" -NoDeleteRaw`
- 结果：通过；真实运行 18 个 MODTRAN case。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；当时 processed 规模为 path 153138 行、solar 78564 行、sky 62352 行、manifest 90 行，Pilot72 `band_lut.csv` 24 行，`band_lut_visibility_smoke.csv` 18 行。

visibility 结论：
- 低空水平路径 `(obs=3 km, target=3 km, range=50 km)` 对 visibility 有响应。NIR tau 从 5/23 km visibility 的 `0.295281` 增至 50 km 的 `0.478024`；MWIR 从 `0.180743` 小幅增至 `0.184090`。
- 高空水平路径 `(obs=10 km, target=10 km, range=50 km)` 在两个 band 中变化都很弱，记录为 `high_altitude_low_sensitivity`，不判定为模板失败。
- 斜穿路径 `(obs=20 km, target=3 km, range=50 km)` 对 visibility 有响应。NIR tau 从 `0.796390` 增至 `0.816102`；MWIR 从 `0.471967` 小幅增至 `0.472327`。
- 因为 `(3,3)` 和 `(20,3)` smoke 路径都出现非零敏感性，当前不存在 `FAIL_VISIBILITY_NOT_EFFECTIVE`，也没有立即回 PcModWin5 GUI 重建 Rural aerosol 模板的证据。

保留约束：
- 未经单独批准，不运行 2616/2520 级 production grid。
- 本阶段不扩展 VIS/SWIR/LWIR production。
- radiance/irradiance 单位在确认前继续保留为 `MODOUT2_native`，不伪造成 SI。

### 2026-05-28 MODTRAN LUT ProductionNirMwir 运行

完成内容：
- 新增 `tools/modtran/snapshot_processed.py`，将 Pilot72 加 VisibilitySmoke18 的 processed 成果归档到 `processed_snapshots/pilot72_visibility_smoke_<timestamp>/`。
- case 生成器增加 `--production-nir-mwir`。请求的 NIR/MWIR sparse grid 理论上为 2520 个 case，其中 510 个斜穿几何因 `range_km < abs(observer_alt_km - target_alt_km)` 对 MODTRAN 不成立，写入 `generated/production_invalid_geometry_manifest.csv`，不计入 runnable production。
- 单线程分批运行 2010 个 runnable NIR/MWIR case，批次为 `NIR_Transmittance`、`MWIR_Transmittance`、`MWIR_ThermalRadiance`、`Solar_NIR_MWIR`、`NIR_RadianceWithScattering`。
- `processed/manifest.csv` 记录 2010 行，全部为 `status/stage = production_nir_mwir_succeeded`。
- 使用矩形响应积分重建 `processed/band_lut.csv`，得到 670 行 NIR/MWIR compact LUT；radiance 和 irradiance 单位仍记录为 `MODOUT2_native`。
- `tools/stage3_modtran_lut_check.ps1 -Strict` 针对 production 规模 CSV 做了流式检查优化，避免大表检查时内存压力过高。

Production QC：
- 表结构和解析完整性通过：`path_lut_spectral.csv` 为 2,634,440 行，`solar_lut_spectral.csv` 为 2,193,245 行，`sky_lut_spectral.csv` 为 1,740,660 行，runnable case 失败数为 0。
- `qc_report.md` 的 `ProductionNirMwir Summary` 当时记录 `overall_status: FAIL`。失败原因是物理/模板 QA 发现，而不是 parser 失败：部分低空或斜穿组合 visibility sensitivity 缺失，且低空水平 MWIR path radiance 在 1 km 到 2 km 处下降，没有按预期增大或趋于饱和。
- 该 production LUT 在完成后续诊断和分级前，不应直接接入 HwaSimIR C++ 的渲染链路。

保留约束：
- 当轮没有修改 HwaSimIR C++ 主逻辑或 shader。
- 没有运行 VIS/SWIR/LWIR production。
- 没有删除旧 `transmittance_0.3_15.txt`。
- 没有把 MODOUT2 radiance/irradiance native 值重标为 SI 单位。

### 2026-05-29 阶段 3 补充：MODTRAN tau-only debug loader

完成内容：
- 新增 `IR/IRModtranTauLut.h/.cpp`，加载 `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv`，只读取 `band`、`atmosphere_model`、`aerosol_model`、`humidity_profile`、`visibility_km`、`observer_alt_km`、`target_alt_km`、`range_km`、`solar_zenith_deg`、`tau_up_band`、`tau_down_band`。
- 第一版固定筛选 `atmosphere_model=Mid-Latitude Summer`、`aerosol_model=Rural`、`humidity_profile=default`；查询 key 为 band、observer altitude、target altitude、range、visibility、solar zenith。
- tau 查询使用 optical depth 形式：`od = -ln(clamp(tau, 1e-6, 1.0))`，当前运行时先采用 nearest-neighbor OD 查询；找不到精确邻点时输出 `warning=nearest_neighbor`。
- 扩展 `IRAtmosphereModel`，启动时加载 MODTRAN compact LUT，并增加 `EnableModtranTauDebug` 环境变量开关；默认值保持 `0`。
- `transmittanceForRange()` 在 debug 打开时打印 `source=band_lut.csv`、band、obs/target/range/visibility、`tau_up`、`tau_down`、interpolation、fallback state、`old_tau/new_tau/diff`。
- 关键边界：`transmittanceForRange()` 仍然返回旧 `legacyTau`，MODTRAN tau 只做日志对比，不改变 shader uniform，不改变 `IRRadianceModel` 最终输出。
- 新增 `tools/stage3_modtran_tau_loader_check.ps1`，检查 `band_lut.csv`、tau 范围、旧 `transmittance_0.3_15.txt`、VS/CMake 工程项、debug 默认关闭、仍返回 `legacyTau`、未读取 band radiance 字段驱动 shader。
- 新增 `tools/stage3_modtran_tau_debug_smoke.ps1`，分别以 `EnableModtranTauDebug=1` 和 `EnableModtranTauDebug=0` 启动 HwaSimIR，发送 NIR/MWIR 初始化包、开始控制包和实时 display 包，验证 debug 打开时有 old/new tau 日志，关闭时无 tau debug 日志。

修改文件：
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRModtranTauLut.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRModtranTauLut.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/CMakeLists.txt`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj`
- `ConsoleApplication1_LLA/ConsoleApplication1/ConsoleApplication1.vcxproj.filters`
- `tools/stage1_band_switch_smoke.ps1`
- `tools/stage3_modtran_tau_loader_check.ps1`
- `tools/stage3_modtran_tau_debug_smoke.ps1`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证命令：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict`
- 结果：通过；production compact `band_lut.csv` 为 670 行，tau 范围为 `0.180743294` 到 `0.9991499122`，旧 MODTRAN processed 表结构检查仍通过。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_loader_check.ps1 -Strict`
- 结果：通过；确认 `EnableModtranTauDebug` 默认关闭、loader 已加入 VS/CMake、仍存在旧 `transmittance_0.3_15.txt`、debug 路径仍返回 `legacyTau`，且 tau loader 未读取 `path_radiance_band`、`sky_radiance_band`、`path_scattering_radiance_band`、`solar_irradiance_band`。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_debug_smoke.ps1 -Bands @(1,2) -DelayMs 500`
- 结果：通过；debug-on 日志出现 `MODTRAN Tau Debug`、`source=band_lut.csv`、NIR/MWIR、`tau_up/tau_down`、`old_tau/new_tau/diff` 和 `fallback_state`；debug-off 日志确认 `EnableModtranTauDebug=0` 且没有 tau debug 输出。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：通过；HwaSimIR `Release|x64` 和 DataDrivenTestQT release 构建成功。

运行时观察：
- NIR debug 样例：`range_km=5`、`visibility_km=23`、`tau_up=0.98604`、`tau_down=0.982608`、`old_tau=0.106184`、`new_tau=0.98604`、`diff=0.879857`。
- MWIR debug 样例：`range_km=5`、`visibility_km=23`、`tau_up=0.798684`、`tau_down=0.795256`、`old_tau=0.00271768`、`new_tau=0.798684`、`diff=0.795966`。
- display 包驱动的场景估算距离会出现 `range_km` 约 1.3 到 1.8 的查询；由于 production grid 是离散点，部分查询记录 `fallback_state=nearest_neighbor`，这是当前 debug-only 版本的预期行为。

发现问题：
- `powershell -File ... -Bands @(1,2)` 在 Windows PowerShell 命令行中可能只传入单个 `1`；`stage3_modtran_tau_debug_smoke.ps1` 已对该验收写法做兼容，恢复为 NIR/MWIR 双 band smoke。
- 只发送初始化包不会触发 `UpdatePlatformIRStatus()` 和 `transmittanceForRange()` 的运行时路径；debug smoke 已补发开始控制包和实时 display 包，用于触发阶段 3 运行时 tau 查询。
- 当前 C++ debug 查询仍固定使用 `observerAltKm=10`、`targetAltKm=10`、`solarZenithDeg=45`，range 来自场景估算，visibility 来自 UDP/default 环境；这足以验证 loader 和日志链路，但不是 tau-active 物理接入。

下一步：
- 若后续要从 debug-only 进入 tau-active，需要单独批准，并新增开关，不能直接替换当前 `legacyTau` 返回值。
- 若继续保持阶段 3，可把 observer/target altitude、solar zenith 和更精确 range 查询参数从场景/协议中显式传入 tau 查询，但仍只限 tau，不接 path/sky/solar radiance。
- path radiance、sky radiance、solar irradiance 仍只允许离线数值检查；进入 shader 或完整辐射链路属于阶段 5 范围，当前不做。
- 本轮仍属于阶段 3：大气与环境模型；不属于阶段 4 温度场，也不属于阶段 5 完整辐射链路。

### 2026-05-29 阶段 3 补充：MODTRAN tau-active 受控实验准备

完成内容：
- 对 `git status --short --untracked-files=all` 做分类盘点：源码、脚本、文档和 compact LUT 属于可提交候选；MODTRAN generated、raw samples、failed/archive、processed snapshots、Panda3D cache 和大型 spectral CSV 不应提交。
- 更新 `.gitignore`，继续忽略 `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/generated/`、`raw/samples/`、`raw/failed/`、`raw/archive/`、`processed_snapshots/`，并补充忽略 production 级大型 `*_lut_spectral.csv` 和本地 Panda3D cache；未删除任何数据。
- `IRAtmosphereModel` 增加 `UseModtranTauForAtmosphere` 受控开关，默认值为 `0`。默认状态下即使 MODTRAN 查询成功，`transmittanceForRange()` 仍返回 legacy tau。
- active 逻辑只允许 NIR/MWIR 且 MODTRAN query 成功时返回 `tau_up`；query 失败、LUT 缺失或 band 为 VIS/SWIR/LWIR 时回退 legacy tau。`tau_down` 只进入 debug 日志，不改变 shader。
- MODTRAN tau 查询输入从固定调试值推进到运行时状态：observer altitude 来自实时/初始化挂载平台高度，target altitude 来自目标状态高度或节点估算，range 来自传感器到节点的场景距离，visibility 来自阶段 3 环境合成，solar zenith 来自 `90 - sun_elevation_deg`。
- 如果 observer/target altitude 无法可靠取得，会使用旧默认值并在 debug 日志中标记 `fallback_input`；这仍是阶段 3 的大气查询输入改进，不是阶段 4 温度场。
- 新增 `tools/stage3_modtran_tau_active_smoke.ps1`，分别以 `EnableModtranTauDebug=1, UseModtranTauForAtmosphere=0` 和 `EnableModtranTauDebug=1, UseModtranTauForAtmosphere=1` 启动 HwaSimIR，发送 NIR/MWIR 初始化包、开始控制包和 display 包。
- 新增 `tools/stage3_modtran_tau_delta_report.py`，从 runtime debug 日志和 `band_lut.csv` 汇总 `old_tau/new_tau/diff/ratio/fallback_state`，输出 `logs/stage3_modtran_tau_delta_report.csv` 和 `logs/stage3_modtran_tau_delta_report.md`；差异大于阈值只标 WARNING，不标 FAIL。
- 增强 `tools/stage3_modtran_tau_loader_check.ps1`，检查 `UseModtranTauForAtmosphere` 默认关闭、active gate 只允许 NIR/MWIR、VIS/SWIR/LWIR 仍 fallback legacy，且未读取 band radiance 字段驱动 shader。

修改文件：
- `.gitignore`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRModtranTauLut.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IR/IRModtranTauLut.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/IRSimulation.cpp`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.h`
- `ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- `tools/stage3_modtran_tau_loader_check.ps1`
- `tools/stage3_modtran_tau_debug_smoke.ps1`
- `tools/stage3_modtran_tau_active_smoke.ps1`
- `tools/stage3_modtran_tau_delta_report.py`
- `docs/HwaSimIR_InfraredSimulationFramework.md`

验证命令：
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_loader_check.ps1 -Strict`
- 结果：通过；确认 `EnableModtranTauDebug=0`、`UseModtranTauForAtmosphere=0` 均为默认关闭，legacy fallback 文件仍存在，active gate 只允许 NIR/MWIR，且未接入 `path_radiance_band`、`sky_radiance_band`、`path_scattering_radiance_band`、`solar_irradiance_band`。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_debug_smoke.ps1 -Bands @(1,2) -DelayMs 500`
- 结果：通过；debug-on 输出 NIR/MWIR `MODTRAN Tau Debug`，debug-off 不输出 tau debug 日志。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_active_smoke.ps1 -Bands @(1,2) -DelayMs 500`
- 结果：通过；active=0 日志为 `return_source=legacy`，active=1 对 NIR/MWIR 输出 `return_source=modtran_tau` 或明确 fallback，日志包含 `old_tau/new_tau/diff`、`fallback_state`，且没有 VIS/SWIR/LWIR 使用 MODTRAN tau。
- 命令：`python tools\stage3_modtran_tau_delta_report.py`
- 结果：通过；写入 `logs/stage3_modtran_tau_delta_report.csv` 和 `logs/stage3_modtran_tau_delta_report.md`，当前仅作为 warning 级数值差异报告。
- 命令：`powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1`
- 结果：通过；HwaSimIR `Release|x64` 和 DataDrivenTestQT release 构建成功。

运行时观察：
- active=0 样例：`active=0 return_source=legacy fallback=legacy`，仍使用旧 `transmittance_0.3_15.txt` 的经验 tau 进入现有渲染链路。
- active=1 样例：NIR/MWIR 日志出现 `return_source=modtran_tau fallback=none`，但仅在显式设置 `UseModtranTauForAtmosphere=1` 时生效。
- 新查询输入中 observer altitude 在 smoke 中来自挂载平台高度约 `1 km`，solar zenith 来自天气 profile 的太阳高度，样例为约 `9.2473 deg`；target altitude 如果来自天空/背景或无法可靠映射，会记录 `fallback_input=target_alt`。
- delta report 当前发现 old/new tau 差异可较大，这是阶段 3 受控实验预期观察项，只标 WARNING，不作为 production QC 失败。

保留约束：
- 未重跑 MODTRAN production。
- 未运行 VIS/SWIR/LWIR production。
- 未修改 shader。
- 未接入 path radiance、sky radiance、solar irradiance。
- 未进入阶段 4 温度场。
- 未进入阶段 5 完整辐射链路。
- 默认不改变画面，`UseModtranTauForAtmosphere` 必须保持默认 `0`。
- 旧 `transmittance_0.3_15.txt` 保留为 fallback。

### 2026-05-29 阶段 3 补充：MODTRAN tau-active A/B smoke 与阶段 3 收口建议

完成内容：
- 新增 `docs/stage3_modtran_tooling_inventory.md`，将当前阶段 3 MODTRAN 工具按 `keep`、`optional`、`historical` 分层记录，便于后续收口时避免继续堆叠重复脚本。
- 确认 `.gitignore` 已忽略 MODTRAN generated、raw/samples、raw/failed、raw/archive、processed_snapshots、本地 logs 以及大型 spectral CSV；本轮不删除任何数据。
- 新增 `tools/stage3_modtran_tau_ab_smoke.ps1`，分别以 `UseModtranTauForAtmosphere=0` 和 `UseModtranTauForAtmosphere=1` 启动 HwaSimIR，发送同一组 NIR/MWIR 初始化包、开始控制包和 display 包，提取 `old_tau/new_tau/diff/return_source/fallback_state`。
- A/B smoke 输出 `logs/stage3_modtran_tau_ab/ab_metrics.csv` 和 `logs/stage3_modtran_tau_ab/ab_summary.md`。当前 smoke 链路只捕获运行日志，不消费输出帧或 JPEG，因此图像帧均值、最小值、最大值和标准差记录为 `frame metrics unavailable`。
- 增强 `tools/stage3_modtran_tau_loader_check.ps1`，把 A/B smoke 脚本纳入阶段 3 检查面。

阶段边界：
- `UseModtranTauForAtmosphere` 仍保持默认 `0`，不会默认改变当前画面。
- `UseModtranTauForAtmosphere=1` 只作为受控实验开关；NIR/MWIR 可以返回 MODTRAN `tau_up`，VIS/SWIR/LWIR 继续使用 legacy fallback。
- 旧 `transmittance_0.3_15.txt` 继续保留为 fallback。
- `path_radiance`、`sky_radiance`、`solar_irradiance` 仍未接入 shader 或 `IRRadianceModel` 最终输出；相关接入后置到阶段 5。
- 本轮仍属于阶段 3 的大气与环境模型收口，不属于阶段 4 温度场，也不属于阶段 5 完整辐射链路。

收口建议：
- 阶段 3 后续只有在用户明确确认后，才考虑把 MODTRAN tau 作为默认大气透过率来源。
- 如果不默认启用 MODTRAN tau，阶段 3 可以视为具备受控实验能力并转入阶段 4。
