# HwaSimIR M1 NIR/MWIR 物理大气运行链实施与验收记录

日期：2026-09-07  
基线：`main`，起始提交 `c68b445`  
范围：NIR 0.70–1.10 μm、MWIR 3.00–5.00 μm  
结论：双波段 SI 数据链、插值 loader、太阳位置、NIR/MWIR 辐射组合、compare-only gate 与临时 runtime gate 已实现。生产配置仍为 legacy，未打开 MODTRAN runtime。

## 1. 完成边界

本阶段正式实现：

- NIR：`tau_up`、目标处太阳直射辐照度、向下天空漫射辐照度、LOS 太阳/散射路径辐亮度。
- MWIR：`tau_up`、热路径辐亮度。
- MODTRAN native 波数谱到 SI 波长谱的逐点雅可比转换。
- `responseMode=RectangularBand` 的归一化矩形 band response；当前两个 SensorWave 文件均没有可审计 SRF 曲线，因此没有宣称真实 SRF。
- 分类轴 `band / atmosphere / aerosol / humidity_profile`，连续轴 `observerAlt / targetAlt / range / visibility`，NIR 再加 `solarZenith`。
- 小型独立 SolarPosition 模块，以及 ENU 到 Panda3D 世界坐标的核对。
- 30 Hz 低频公共太阳/大气状态更新；CSV 只在启动时读取，shader 不查 CSV。
- compare-only 默认不改变最终图像；临时测试 gate 可分别启用 NIR/MWIR。

明确未实现：太阳热加载/热惯量、heat capacity/conduction/convection、主动 illuminator、VIS/SWIR/LWIR、完整 QE/electron detector model、通信协议变更。

## 2. 数据与单位链

### 2.1 单位

运行时 loader 只输出：

- `tau_up`：dimensionless
- radiance：`W/(m^2 sr um)`
- irradiance：`W/(m^2 um)`

MODOUT2 native radiance 为 `W/(cm^2 sr cm^-1)`。逐谱点转换为：

```text
L_lambda[W/(m^2 sr um)] = L_wavenumber[W/(cm^2 sr cm^-1)] * 1e8 / lambda_um^2
```

flux/irradiance 使用同一谱变量雅可比，但没有 `sr`。转换后才在波长域积分；矩形响应采用归一化加权平均，即 `∫S(λ)R(λ)dλ / ∫R(λ)dλ`，所以结果仍是每 μm 的谱密度。边界采样只允许在一个原始谱网格步长内做线性端点外推，并记录到 `conversion_method`。

`tools/test_modtran_units.py` 对多个 NIR/MWIR 波长点做 native→SI→native 回算。QC 最大相对回算误差为 `0.000e+00`。

### 2.2 MODTRAN source 字段

| 物理量 | MODTRAN source | 说明 |
| --- | --- | --- |
| NIR/MWIR `tau_up` | `COMBIN TRANS` | 无量纲；tau 插值转到 optical depth 后进行 |
| MWIR `path_thermal` | `PTH_THRML` | 不使用 `TOTAL_RAD` |
| NIR direct solar at target | `SOL TR` | 使用目标高度太阳直射 case；不使用 TOA `SOLAR` |
| NIR sky diffuse down | `.flx DOWNWARD` | 与 LOS path radiance 保持为独立物理量 |
| NIR LOS path scattering | `SOL_SCAT` | 不使用 `TOTAL_RAD` |

形式 LUT 明确拒绝 `TOTAL_RAD` 和 TOA `SOLAR` 的替代写法。旧 candidate 中 `solar=967.2334292` 是 TOA 量，已被正式目标处 `SOL TR=844.956419072` 替换；旧 `sky_radiance` 重复 `TOTAL_RAD`，未进入新 LUT。

## 3. 原始 case 与正式 LUT

MWIR 标准 case 原始目录：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906\mwir_std_trans
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906\mwir_std_thermal
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906\mwir_std_solar_direct
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906\mwir_std_solar_scatter
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906\mwir_std_flux
```

NIR M1 新生成原始目录根：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\m1_nir_mwir_20260906
```

该目录含 820 个 case、3433 个文件、326,560,512 bytes：670 个 SZA 20/70 LOS scattering case、75 个目标太阳直射 case、75 个 downward flux case。经审计的 SZA 45 trans/scattering candidate 作为中间平面保留，SZA 20/70 使用本轮真实输出补齐。

NIR 标准 case 的新增原始输入/输出路径示例：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\m1_nir_mwir_20260906\NIR_solar_tar5_vis23_aerRural_humdefault_sza45
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\m1_nir_mwir_20260906\NIR_flux_tar5_vis23_aerRural_humdefault_sza45
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\m1_nir_mwir_20260906\NIR_scattering_obs10_tar5_rng10_vis23_aerRural_humdefault_sza20
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\m1_nir_mwir_20260906\NIR_scattering_obs10_tar5_rng10_vis23_aerRural_humdefault_sza70
```

正式 SI LUT：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv
```

共 1340 行：MWIR 335 行，NIR 1005 行。每条记录包含：

```text
schema_version, case_id, band, atmosphere_model, aerosol_model,
humidity_profile, visibility_km, observer_alt_km, target_alt_km,
range_km, solar_zenith_deg, tau_up,
path_thermal_W_m2_sr_um,
direct_solar_irradiance_at_target_W_m2_um,
downward_sky_diffuse_irradiance_W_m2_um,
los_path_scattering_radiance_W_m2_sr_um,
radiance_unit, irradiance_unit, tau_unit, response_mode,
conversion_method, modtran_source_fields, source_case_ids, source_files
```

分类值为 `Mid-Latitude Summer / Rural / default`。`humidityPercent` 不再作为伪连续查询维度；当前正式数据只支持 `humidity_profile=default`，其它 profile 会有显式告警并 fallback 到 default。

## 4. 数值基准

### 4.1 MWIR 黄金 case

条件：3–5 μm、Mid-Latitude Summer、Rural、visibility 23 km、observer 10 km、target 5 km、range 10 km。

| 项 | 目标 | 正式 LUT | 绝对误差 |
| --- | ---: | ---: | ---: |
| `tau_up` | 0.666728313 | 0.6667283123 | 7.0e-10 |
| `pathThermal` | 0.0360989067 | 0.03609890662 | 8.0e-11 |

C++ 启动自检实际打印 `status=PASS rows=1340`。运行时实际几何中心距离为 10.008785 km 时，插值得到 `tau=0.666653`、`pathThermal=0.036107`，变化方向与距离差一致。

### 4.2 NIR 标准 case

条件：0.70–1.10 μm、observer 10 km、target 5 km、range 10 km、visibility 23 km、SZA 45°。

| 项 | 正式 SI 值 |
| --- | ---: |
| `tau_up` | 0.9329166851 |
| target `SOL TR` | 844.956419072 W/(m² μm) |
| `.flx DOWNWARD` | 134.112843667 W/(m² μm) |
| LOS `SOL_SCAT` | 0.8262748858 W/(m² sr μm) |

### 4.3 Planck 数量级

3–5 μm 矩形 band mean：250 K = 0.108517488、300 K = 0.932978127、500 K = 83.7638889、1000 K = 3253.36698 `W/(m² sr μm)`。这些只用于数量级 QC，没有作为调灰度 scale。

完整 range/visibility/SZA 表在：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\m1_nir_mwir_qc.csv
```

覆盖 MWIR range 1/10/30/50 km、visibility 5/23/50 km，以及 NIR 同组 range/visibility 和 SZA 20/45/70°。30 km 位于 20/35 km 正式 cell 间，tau 走 OD 插值，其它量线性插值。

## 5. Loader 与插值

`IRModtranRadianceLut` 只接受正式 SI schema 和单位。`IRModtranTauLut` 改为兼容 facade，内部委托正式 SI loader；不存在 MODOUT2 native 直接进入 `IRRadianceModelV2` 的路径。

分阶段规则网格插值顺序：

```text
targetAlt -> observerAlt -> range -> visibility -> solarZenith(NIR only)
```

- `tau`：每个采样点先计算 `OD=-ln(tau)`，线性插值 OD，最后 `tau=exp(-OD)`。
- MWIR `path_thermal`：线性插值。
- NIR direct solar / sky diffuse / path scattering：线性插值。
- 正常路径没有 nearest-neighbor，也没有 silent clamp。
- axis OOR 或 cell missing 返回 invalid，并带 `fallbackReason / fallbackAxis / fallbackQuery / fallbackMin / fallbackMax`；调用侧打印 band 和查询值，最终图像 fallback 到 legacy。

## 6. SolarPosition 与坐标

新增 `IR/IRSolarPosition.h/.cpp`。输入为纬度、经度、海拔、UTC 日期和日内 UTC 时间；输出太阳方位、高度、天顶角及世界方向。

协议实时平台位置优先，初始化平台位置次之，天气配置位置最后。当前协议只有日内毫秒，没有完整日期，因此 `HwaSimIRRuntime.ini` 提供 `FallbackUtcDate`。运行日志明确打印 lat/lon/alt、UTC、solarAz、solarEl、solarZenith、sunDirection 和来源。

`GeoTransform` 与实际运行核对结果：本工程 Panda3D 世界坐标为 `X=East, Y=North, Z=Up`，所以输出方向直接写成 `(east,north,up)`，没有猜测或硬编码轴交换。赤道、2026-03-20 09:00 UTC 的实际日志为：

```text
solarAz=90.127219 solarEl=43.125083 solarZenith=46.874917
sunDirection=(0.729861,-0.001621,0.683593)
frame=ENU/Panda(X=East,Y=North,Z=Up)
```

太阳位置与公共 atmosphere state 每个 IR update 只计算一次，再复用于同帧所有目标。

## 7. 辐射主链

MWIR：

```text
L_sensor = tau_up * L_surface + L_path_thermal
```

NIR：

```text
L_solar_reflected = rho_NIR/pi * E_sun_direct * max(0,NdotL) * sunVisibility
L_sky_reflected   = rho_NIR/pi * E_sky_down * skyVisibility
L_surface_NIR     = L_solar_reflected + L_sky_reflected
L_sensor_NIR      = tau_up * L_surface_NIR + L_path_scattering
```

NIR shader 使用 world normal 与 `sunDirection` 计算逐像素 `NdotL`。Panda RGB light color 不参与正式 NIR radiance。CPU compare 日志使用 `NdotL=1` 作为迎光参考分量，便于审计；最终 NIR runtime shader 才应用每像素法线。

MaterialDatabase 当前没有正式 `nirReflectance` 字段。本轮使用受控 fallback：

```text
rho_NIR = clamp(1 - emissivity - transmissivity, 0, 1)
reflectanceSource=fallback
```

不再使用可见纹理 `textureLuma` 作为正式 NIR reflectance。几何 shadow visibility 尚未接入，当前明确记录 `shadowVisibility=not_implemented`、`sunVisibility=1`。

## 8. Compare-only 与临时 runtime A/B

生产 INI 默认：

```text
CompareOnly=false
EnableRuntime=false
EnableNIRRuntime=false
EnableMWIRRuntime=false
```

compare-only 实测：

- NIR 10.008785 km：`tau=0.932874`、direct=841.366690、sky=131.993877、pathScatter=0.912121、`m1Sensor=156.989758`、legacy=0.044402，日志 `finalOutput=legacy compareOnly=1`。
- MWIR 10.008785 km：`tau=0.666653`、pathThermal=0.036107、`m1Sensor=0.383053`、legacy=0.040187，日志 `finalOutput=legacy compareOnly=1`。

临时环境变量 gate 实测后未写回生产 INI：

- NIR：`M1EnableRuntime=1, M1EnableNIRRuntime=1`，日志 `finalOutput=M1`。
- MWIR：`M1EnableRuntime=1, M1EnableMWIRRuntime=1`，日志 `finalOutput=M1`。

两次测试均使用原有 display scale/offset/gamma；未改 gray gain、tone-map、body floor 或经验 scale。日志证据：

```text
D:\HwaSimIR\logs\m1-compare-nir.out.log
D:\HwaSimIR\logs\m1-compare-mwir.out.log
D:\HwaSimIR\logs\m1-runtime-nir.out.log
D:\HwaSimIR\logs\m1-runtime-mwir.out.log
```

帧截图：

```text
D:\HwaSimIR\logs\m1-runtime-nir.png
D:\HwaSimIR\logs\m1-runtime-mwir.png
```

当前 10 km AIM120D 在传感器视场中接近点目标，截图适合证明 runtime、look-at、可见性和输出链路，不适合评价材料细节或主观画质。没有为截图放大目标或修改显示参数。

## 9. 资源加载 P0

启动日志打印以下绝对 resolved path 和加载结果：

- `D:\HwaSimIR\materials\MaterialDatabase.csv`
- `D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv`
- `D:\HwaSimIR\HwaSim_IR\Bin\Config\SensorWave\default_NVG.json`
- `D:\HwaSimIR\HwaSim_IR\Bin\Config\SensorWave\default_MWIR.json`
- 每个目标的 model、base texture、`*_mat.tif` 和 material XML。

本轮实际 AIM120D/F35/AIM9X 初始化中，MaterialDatabase、LUT、SensorWave、material-id TIFF 和 XML 均 `loaded/exists=1`。映射失败不会静默，日志含平台类型和缺失绝对路径。

`default_NVG.json` 的正式低端由 0.40 μm 收紧为 0.70 μm，高端保持 1.10 μm；MWIR 保持 3.00–5.00 μm。

## 10. 性能

测试环境：Windows x64、HeadlessOffscreen、640×512 protocol sensor、300 个 60 Hz UDP stimulus frame、NIR M1 runtime、无 TCP video consumer。

稳定 stimulus 区间：

- UDP input：57.85–58.66 fps；render：58.66–60.37 fps。
- `irUpdateMs`：0.410–0.733 ms。
- `stage5ModtranLookupMs`：0.069–0.084 ms。
- `stage5RadianceComponentMs`：0.409–0.785 ms。
- LUT 在启动时读取一次；运行时查询在约 30 Hz IR update 上执行，shader 无 CSV I/O。

本测试没有 TCP consumer，因此 `outputFps=0`、端到端 video latency 计数为 0，不能当作链路延迟结论。性能原始日志：

```text
D:\HwaSimIR\logs\m1-runtime-nir-perf.out.log
```

## 11. 构建与自动检查

- `tools/m1_nir_mwir_physics_check.ps1`：4 个单位测试 + raw/LUT/QC 全链审计，PASS。
- Windows VS2015 Release x64 主工程：PASS；只有既有 `math_algorithm.h` 未使用局部变量 warning。
- aarch64：在用户指定 `linaro@192.168.203.128` 上同步本轮 C++/CMake 增量，仅编译、不部署。2026-09-07 07:00:11 +0800 在虚拟机重新开机后 SSH 现场复核，四个关键源文件 SHA-256 与本机逐项一致；随后执行 `cmake --build cmake-build-codex-rk3588 --clean-first --parallel 4`，清理 38 个旧构建文件并从头完成 Ninja 38/38 编译和链接，退出码 0。新产物时间为 2026-09-07 07:08:52 +0800，`file` 确认为 `ELF 64-bit LSB pie executable, ARM aarch64`，BuildID `a222f1cabe4a92e4961f1116036f0f6becfc7123`。

QC 报告：

```text
D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\m1_nir_mwir_qc.md
```

## 12. 修改文件

数据/工具：

- `tools/modtran_generate_cases.py`
- `tools/modtran_run_m1.ps1`
- `tools/modtran_convert_to_si.py`
- `tools/modtran_build_lut.py`
- `tools/modtran_qc.py`
- `tools/test_modtran_units.py`
- `tools/m1_nir_mwir_physics_check.ps1`
- `tools/m1_runtime_stimulus.py`
- `tools/fixtures/NetworkConfig.loopback.ini`
- `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/m1_nir_mwir_20260906/`
- `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`
- `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/m1_nir_mwir_qc.csv`
- `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/m1_nir_mwir_qc.md`

C++/配置：

- `IR/IRModtranRadianceLut.h/.cpp`
- `IR/IRModtranTauLut.h/.cpp`
- `IR/IRSolarPosition.h/.cpp`
- `IR/IRRadianceModelV2.h/.cpp`
- `IR/IRSceneMaterialMapper.cpp`
- `IRSimulation.cpp`
- `HwaSimIR.h/.cpp`
- `HwaSimIRRuntime.ini`
- `SensorWave/default_NVG.json`
- `CMakeLists.txt`
- `HwaSim_IR.vcxproj/.filters`

## 13. 已知未完成项与优先级

P1：MaterialDatabase 增加实测/可追溯 `nirReflectance` 或 band reflectance，替换当前明确记录的 fallback。  
P1：接入可靠几何 `sunVisibility`，分别验证太阳反射阴影；当前不伪造 shadow。  
P1：接入带真实测量来源的 NIR/MWIR SRF 后，将 `RectangularBand` 替换为正式 SRF 权重并重建 LUT。  
P2：增加带 TCP consumer 的端到端 60 Hz latency/FPS 回归；本轮只测到渲染和 LUT 成本。  
P2：扩展 humidity profile 分类 case；在数据存在前不恢复 `humidityPercent` 插值参数。

这些事项不影响本轮“证明 NIR/MWIR 数据、单位、loader、插值和基本成像闭环正确”的结束条件。生产 MODTRAN gate 保持关闭；本阶段在此停止。
