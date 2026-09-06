# HwaSimIR MODTRAN 物理与单位审计

日期：2026-09-06  
工程：`D:\HwaSimIR`  
结论状态：**MWIR 3–5 µm 标准 case 的单位、来源、原始输出和 SI 转换已证明；现有生产 LUT/loader 不可直接作为 SI radiance 链启用。**

## 0. 边界与结论

本轮没有修改 HwaSim_IR 的生产视觉参数、shader、C++ loader 或 runtime 开关；`UseModtranPathRuntime`、`UseModtranSkyRuntime`、`UseModtranSolarRuntime` 和 `UseModtranTauForAtmosphere` 均未开启。没有修改或规避 PcModWin/MODTRAN 许可证，也没有反编译 DLL。

标准 case 已由本机 `F:\Programs\PcModWin5\Bin\Mod5.2.1.0.exe` 实际运行。原始输入/输出位于：

`D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\audit_mwir_standard_20260906`

SI 光谱和汇总表位于：

`D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\audit_mwir_standard_20260906`

## 1. 六个必须回答的问题

| 问题 | 核查结论 |
| --- | --- |
| `band_lut.csv` radiance/irradiance 的真实原始单位 | `path_radiance_band`、`sky_radiance_band`、`path_scattering_radiance_band` 是 MODOUT2 的 **W/(cm²·sr·cm⁻¹)** 谱密度在波长域做矩形响应平均后的数值；`solar_irradiance_band` 是 **W/(cm²·cm⁻¹)**。这些不是 SI，也不是带内积分总量。更严重的是，旧 parser 将 `solar_irradiance_band` 取自 `SOLAR`（TOA 源），不是目标处 `SOL TR`；`sky_radiance_band` 取自 `TOTAL_RAD`。 |
| `band_lut_si_candidate.csv` 公式 | 对每个谱点先做 `value_SI = value_native × 10^8 / λ_µm²`，再在 3–5 µm 波长域做梯形积分并除以带宽，得到带内平均谱密度。公式本身正确，见 `tools/modtran/build_band_lut_si_candidate.py:119-123`；但输入字段语义继承了旧 parser 的 TOA solar / TOTAL_RAD 问题。 |
| raw path 与 legacy/Planck 差约 10⁶ 的原因 | 原始量是每 cm²、每 cm⁻¹；程序/Planck 是每 m²、每 µm。面积换算给 `10^4`，谱变量 Jacobian 给 `10^4/λ²`，合计 `10^8/λ²`。在 3–5 µm，倍率从 `1.11×10^7` 到 `4.0×10^6`，所以出现 10⁶–10⁷ 差异是单位必然结果，不应人工调 scale。 |
| 当前 C++ loader 能否读 SI candidate | **不能。** `IRModtranRadianceLut.cpp:131-145` 硬要求旧列名 `path_radiance_band/sky_radiance_band/solar_irradiance_band`，而 SI candidate 使用带 `_W_m2_...` 的正式列名；`HwaSimIR.cpp:8957-8960` 搜索的仍是 `band_lut.csv`。 |
| `humidityPercent` 是否参与查询 | **没有。** Query/Result 中存在并回显该值（`IRModtranRadianceLut.h:15,29`、`.cpp:251`），`HwaSimIR.cpp:12290-12337` 甚至生成 humidity bucket；但 LUT Entry、bounds 和 `normalizedDistance()` 都没有 humidity 维度。加载时只固定筛选字符串 `humidity_profile == default`（`.cpp:166-170`）。这是“参数存在但不参与”的假支持。Tau loader 连参数都没有。 |
| 当前 runtime 最终使用 legacy 还是 MODTRAN | **legacy。** `HwaSimIRRuntime.ini:70,160,162,164,168` 分别为 tau=0、path=false、sky=false、solar=false、mode=Off；C++ 只有 ReplaceLegacy/BlendLegacy 且开关为真时才可能替换。当前 MODTRAN radiance 只可作 debug/compare，最终像素仍走 legacy。 |

## 2. 仓库数据链审计

### 2.1 数据来源与旧链路

核心目录：

- raw 模板与历史输出：`D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw`
- processed LUT：`D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed`
- parser：`D:\HwaSimIR\tools\modtran\parse_modout2.py`
- band builder：`D:\HwaSimIR\tools\modtran\build_band_lut.py`
- SI candidate builder：`D:\HwaSimIR\tools\modtran\build_band_lut_si_candidate.py`

旧 parser 的两个 P0 证据：

- `parse_modout2.py:252` 使用 `tokens[3]`。MODOUT2 solar 表头为 `FREQ TRANS SOL TR SOLAR`，所以它取到 TOA `SOLAR`；目标处透射值应是 `tokens[2]`。
- `parse_modout2.py:269` 将 radiance 表的 `tokens[9]`（`TOTAL_RAD`）写入 `sky_radiance`。由于空列会改变 split 后的位置，这种 token 序号本身也不稳健。

旧 `band_lut.csv` 的“band”处理是波长域响应平均，不是物理带内积分。保留平均谱密度作为 runtime LUT 可以，但列名、单位和文档必须明确；如果需要带内总 radiance/irradiance，应另存不含 `/µm` 的积分列。

### 2.2 单位推导

令波数 `ν̃[cm⁻¹] = 10000 / λ_µm`，则：

```text
|dν̃/dλ_µm| = 10000 / λ_µm²
cm⁻² -> m⁻² = 10000

L_λ[W/(m² sr µm)] = L_ν̃[W/(cm² sr cm⁻¹)] × 10⁸ / λ_µm²
E_λ[W/(m² µm)]    = E_ν̃[W/(cm² cm⁻¹)]    × 10⁸ / λ_µm²
```

逆变换为 `native = SI × λ_µm² / 10⁸`。`spectral_flux.flx` 本机输出头明确写为 `W cm-2 / NM`，其精确换算是 `×10^7`（`10^4` 面积 × `10^3` nm→µm），这不是经验 scale。

本机 `F:\Programs\PcModWin5\PcModWin5Manual.pdf` 第 649–650 页确认：除 direct solar 外 radiance 以 W/cm²/sr per band 报告，可按 cm⁻¹ 或 µm；direct solar 同时报告 transmitted 与 source solar，并给出 per-µm/per-wavenumber。新标准 case 的 MODOUT1 双单位表也与上述 Jacobian 一致。

### 2.3 MODOUT2 字段的可用语义

| 字段 | 本轮采用方式 |
| --- | --- |
| `TOT_TRANS` | LOS 总透过率，无量纲。 |
| `PTH_THRML` / 手册 Path Thermal（有时文献写 THRML_EM） | 大气路径热辐射。手册说明：启用 multiple scattering 时该项已包含热散射/地面反射贡献。 |
| `THRML_SCT` | 热散射诊断子项；不能在已包含它的 `PTH_THRML` 上再次相加。 |
| `SOL_SCAT` | 太阳路径散射总项。 |
| `SING_SCAT` | `SOL_SCAT` 中的单次散射子项；若 multiple scattering 开启，可用 `SOL_SCAT-SING_SCAT` 作为多次散射诊断，不重复相加。 |
| `REF_SOL`、`SOL@OBS`、`TOA_SUN` | 太阳源/传播诊断量，不作为 path radiance。 |
| `TOTAL_RAD` | 源项总和，仅用于总辐亮度诊断；**严禁作为 path radiance 或 downward sky flux。** |
| `.flx` `UPWARD/DOWNWARD DIFFUSE/DIRECT SOLAR` | 分层垂直通量；本轮取 altitude=5 km 的 `DOWNWARD DIFFUSE` 和 `DIRECT SOLAR`。原始单位 W/(cm²·nm)。 |

## 3. 本机 PcModWin5 / MODTRAN5

### 3.1 安装与入口

- 注册表：`HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{2E0F3024-198E-4D33-AA4E-6E8E0EC86A84}`
- 产品：`Ontar PcModWin 5`，版本 `5.0 v1.3.2`
- 安装目录：`F:\Programs\PcModWin5`
- GUI：`F:\Programs\PcModWin5\Bin\PcModWin5.exe`
- 底层 MODTRAN：`F:\Programs\PcModWin5\Bin\Mod5.2.1.0.exe`
- 开始菜单：`C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Ontar Corporation\Ontar PcModWin5.lnk`
- GUI case 数据库/模板：`F:\Programs\PcModWin5\Usr\*.ltn`
- 本地手册：`F:\Programs\PcModWin5\PcModWin5Manual.pdf`、`F:\Programs\PcModWin5\MODTRAN_R_5.2.1.pdf`

`PcModWin.cfg` 确认 Bin/Help/Usr 均指向该安装，运行方式是 `RunModtranCfgType=inline`。未读取、复制或修改许可证 key。

### 3.2 CLI 与文件扩展名

CLI 可用且已验证。随附 `Modtran.bat` 的行为是把 `MODIN` 复制成 `tape5`，运行 `Mod5.2.1.0.exe`，再保存 `TAPE6/7/8`。本轮使用更安全的备份/恢复 runner：`D:\HwaSimIR\tools\run_modtran_audit_standard.ps1`。

本地 `MODTRAN_R_5.2.1.pdf` 第 6–9 页还说明正式多 case 批处理：在 `mod5root.in` 中逐行列 root name，输入为 `<root>.tp5`，输出包括 `<root>.tp6`、`.tp7`、`.tp8`、`.7sc`、`.flx`。PcModWin 的 GUI 保存格式是可读的 `.ltn`；`MODIN/tape5/.tp5` 是实际模型输入。

输出对应关系：

- `MODOUT1` / `tape6` / `.tp6`：输入回显、profile、geometry、双单位详细表和诊断。
- `MODOUT2` / `tape7` / `.tp7`：紧凑谱表。
- `MODOUT3` / `tape8` / `.tp8`：按配置生成的分层/差分附加数据。
- `tape7.scn` / `.7sc`：扫描后的谱输出。
- `specflux` / `.flx`：分层 upward/downward diffuse 与 direct solar flux。

可复用本地官方模板包括 `SolarIrrad.ltn`（IEMSCT=3）、`SingleSctSolar.ltn`（约 4.6 µm 单次太阳散射）和 `DisortScatter.ltn`（DISORT、多次散射、spectral flux）。

## 4. MWIR 标准 case

### 4.1 输入

- band：3–5 µm（2000–3333.33 cm⁻¹）
- atmosphere：Mid-Latitude Summer
- aerosol：Rural
- visibility：23 km
- LOS：observer 10 km、target 5 km、slant range 10 km
- solar zenith：45°

五个输入/输出子目录：

1. `...\mwir_std_trans`：LOS direct transmittance
2. `...\mwir_std_thermal`：path thermal
3. `...\mwir_std_solar_direct`：target altitude 5 km 的 transmitted direct solar
4. `...\mwir_std_solar_scatter`：LOS `SOL_SCAT/SING_SCAT`
5. `...\mwir_std_flux`：DISORT spectral flux，取 5 km 层

每个目录保留 `.tp5`、`MODOUT1.txt`、`MODOUT2.txt`；flux case 还保留 `spectral_flux.flx`。`case_manifest.csv/json` 保存 case id、profile、geometry、单位、转换方法、source path 和输入 SHA-256；`run_manifest.csv` 保存 engine path/version、运行时间、每个输出路径与 SHA-256。

### 4.2 SI 数值表

完整表：`D:\HwaSimIR\HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\audit_mwir_standard_20260906\mwir_standard_si_table.csv`

| 量 | 3–5 µm 响应平均 | 带内积分 |
| --- | ---: | ---: |
| LOS tau | 0.666728313 | — |
| solar-path tau | 0.668494358 | — |
| path thermal | 0.0360989067 W/(m²·sr·µm) | 0.0721869826 W/(m²·sr) |
| solar-scattered path | 0.000313442913 W/(m²·sr·µm) | 0.000626791784 W/(m²·sr) |
| direct solar at target | 7.22522191 W/(m²·µm) | 14.5518179 W/m² |
| downward sky diffuse flux at 5 km | 0.768583420 W/(m²·µm) | 1.53693627 W/m² |

旧 candidate 同一几何行的 tau 与 path 完全吻合到舍入精度，证明旧 candidate 的 Jacobian 实现正确；其 solar 为 `10.86727262 W/(m²·µm)`，高于新链 `7.22522191`，原因正是旧链取 TOA `SOLAR` 而非 transmitted `SOL TR`。

## 5. QC

自动输出：`...\processed\audit_mwir_standard_20260906\qc.md`、`trend_qc.csv`。单位测试：`D:\HwaSimIR\tools\test_modtran_units.py`。

### 5.1 Planck 数量级

| T (K) | 3–5 µm 平均 W/(m²·sr·µm) | 积分 W/(m²·sr) |
| ---: | ---: | ---: |
| 250 | 0.108517504 | 0.217035007 |
| 300 | 0.932978195 | 1.86595639 |
| 500 | 83.7638878 | 167.527776 |
| 1000 | 3253.36696 | 6506.73392 |

本标准 case 的路径热辐射 `0.0361` 低于 250 K 黑体带均值且远低于 300 K 黑体，量级合理；不存在再乘人工 10⁶ 的理由。

### 5.2 趋势

为使 1 km range 合法，range QC 使用 observer=target=5 km 的水平路径；visibility QC 使用 observer=target=1 km、range=10 km，以穿过 Rural 边界层；这两者不冒充标准 10→5 km LOS。

| 轴 | 采样 | 结果 |
| --- | --- | --- |
| range km | 1/10/30/50 | tau `0.7894/0.5738/0.4071/0.3196` 单调下降；path `0.0593/0.1170/0.1610/0.1844` 单调上升，PASS。 |
| visibility km | 2/5/10/23/50 | tau `0.0412/0.1345/0.2000/0.2657/0.2818` 单调上升；path `0.6132/0.5690/0.5389/0.5091/0.5019` 单调下降，PASS。 |
| solar zenith deg | 0/30/45/60/75 | direct target irradiance `7.5817/7.4388/7.2252/6.8176/5.9034` 单调下降，PASS。 |

raw↔SI 五点回算最大相对误差为 0；三个单元测试通过。高空 5 km visibility 初测几乎无响应，改在 1 km 后恢复显著响应，符合气溶胶垂直分布，而非转换故障。

## 6. C++ 最小修正设计（本轮未实施）

1. 新建正式 schema，例如 `band_lut_si.csv`；正式列名必须带单位。Loader 入口拒绝 `MODOUT2_native` 和缺失 unit metadata 的行，只输出 SI。
2. `IRModtranRadianceLut` 支持 `path_radiance_mean_W_m2_sr_um`、`solar_scatter_mean_W_m2_sr_um`、`direct_solar_irradiance_mean_W_m2_um`、`downward_sky_diffuse_flux_mean_W_m2_um`；禁止把 `TOTAL_RAD` 映射到 path/sky。
3. 当前无 humidity 维度时，删除 `humidityPercent` query/cache bucket，日志明确 `humidity_support=unsupported`；只有重建含 RH/profile 轴的 LUT 后再恢复该参数。
4. 分类维度（band/atmosphere/aerosol/profile）必须精确匹配。规则数值网格按 range→visibility→altitude→solar zenith 分阶段插值；tau 在 optical depth `-ln(tau)` 上插值，radiance/irradiance 线性插值。单元格不完整则失败而不是偷偷 nearest。
5. OOR 日志包含 axis、query、min/max、缺失 cell、source case id；fallback 到 legacy，并记录 `fallback_reason`。不做不可见 silent clamp。
6. atmosphere/LUT 查询以 30 Hz 或输入变化驱动，结果放入 frame-level atmosphere state/uniform；禁止每像素/每 fragment 查 LUT，也不每帧读 CSV。
7. 在独立 compare 模式先记录 SI MODTRAN 与 legacy 差异。通过单位、趋势、图像解释性 gate 后，才允许另一个变更开启 runtime。

## 7. 问题优先级

### P0

- 旧 direct-solar parser 取 `SOLAR` 而不是 `SOL TR`。
- 旧 `sky_radiance` 取 `TOTAL_RAD`，且 token split 会被空列破坏。
- C++ radiance loader 只认旧 native 列，无法加载正式 SI candidate；仍记录 `units=MODOUT2_native`。
- `humidityPercent` 是假支持，影响 cache key 却不影响 LUT 查询。
- 在上述问题修复前不得开启 `UseModtranPathRuntime/Sky/Solar`。

### P1

- nearest-neighbor 应替换为规则网格/分阶段插值，tau 在 optical depth 域插值。
- 平均谱密度与带内积分必须分列命名。
- path thermal、thermal-scatter 子项、solar-scatter、downward diffuse flux 必须分别存储，禁止重复相加。
- manifest 应扩展 engine version、input/output SHA-256 和运行时间。

### P2

- 增加多 atmosphere/aerosol/humidity profile 的正式 production grid。
- 增加真实传感器 spectral response，而不仅是矩形响应。
- 对 flux 的 layer/stream/DISORT 设置增加更多回归 case。

## 8. 下一阶段可直接实施的文件清单

- `HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h/.cpp`
- `HwaSim_IR/HwaSim_IR/IR/IRModtranTauLut.h/.cpp`
- `HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h/.cpp`
- `HwaSim_IR/HwaSim_IR/HwaSimIR.h/.cpp`
- `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`（新文件）
- `tools/modtran_generate_cases.py`
- `tools/modtran_convert_to_si.py`
- `tools/modtran_build_lut.py`
- `tools/modtran_qc.py`
- `tools/test_modtran_units.py`
- 新增 C++ loader/schema/interpolation tests；最后才单独评审 `HwaSimIRRuntime.ini`，本阶段保持不变。

## 9. 复现命令

```powershell
$py = 'C:\Users\kahn1\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $py tools\modtran_generate_cases.py --include-qc-grid
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\run_modtran_audit_standard.ps1
& $py -m unittest tools\test_modtran_units.py -v
```

runner 会在运行前备份 PcModWin 固定名工作文件，结束或异常时恢复。它只允许预期的 HwaSimIR case root 和 `F:\Programs\PcModWin5\Bin`，防止写错目录。
