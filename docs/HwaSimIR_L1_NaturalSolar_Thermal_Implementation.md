# HwaSimIR L1 Natural Solar and Material Thermal State

Date: 2026-09-07  
Baseline: `main` at `78fc4862d7c6c863a8e6a4108821d7812f243af3`  
Scope: NIR 0.70-1.10 um and MWIR 3.00-5.00 um only

## 1. Outcome and production gate

L1 implements a gated NIR/MWIR natural-solar and directional material-temperature path without changing the wire protocol. M1 NIR/MWIR MODTRAN runtime is a production candidate on Windows x64: both bands passed the real
`DataDrivenTestQT -> HwaSim_IR -> HwaSim_IR_VideoDisplay` TCP consumer path at 60 Hz. Production defaults remain disabled until deployment approval:

```ini
[M1NirMwirPhysics]
CompareOnly=false
EnableRuntime=false
EnableNIRRuntime=false
EnableMWIRRuntime=false

[NaturalSolar]
Enable=false
EnableOpticalShadow=false
EnableSolarThermal=false
```

The tests temporarily enabled the gates and restored the INI afterwards. No gray gain, tone-map, body-floor, empirical MODTRAN scale, or visible texture-luma reflectance was changed.

| Gate | Evidence | Result |
|---|---|---|
| NIR compare-only | `D:\HwaSimIR\logs\phase2a-final-20260907-124350` | PASS; MODTRAN calculation valid, `finalOutput=legacy`, `compareOnly=1` |
| MWIR compare-only | `D:\HwaSimIR\logs\phase2a-final-20260907-124440` | PASS; MODTRAN calculation valid, `finalOutput=legacy`, `compareOnly=1` |
| NIR 60 Hz runtime TCP | `D:\HwaSimIR\logs\phase2a-final-20260907-124548` | PASS; 720/720 frames, source sequence continuous, no dropped/overwritten frames, `finalOutput=M1` |
| MWIR 60 Hz runtime TCP | `D:\HwaSimIR\logs\phase2a-final-20260907-124643` | PASS; 720/720 frames, source sequence continuous, no dropped/overwritten frames, `finalOutput=M1` |
| L1 solar thermal runtime | `D:\HwaSimIR\logs\phase2a-final-20260907-121822` | PASS; solar LUT and four material regions active, smooth directional deltas |
| Windows Release x64 | `tools/stage0_build.ps1 -Configuration Release -Platform x64` | PASS |
| aarch64 clean Release | VM `192.168.203.128`, build tree `/home/linaro/userdata/HwaSimIR/cmake-build-codex-rk3588` | PASS; clean 41-object build, ELF64 ARM aarch64 |
| RK3588/Mali execution | real board unavailable in this run | NOT RUN; VM host is x86_64 and cannot execute the ARM ELF |

An earlier 2026-09-07 run with protocol band 3 was rejected because protocol band 3 is LWIR; the accepted MWIR run uses unchanged protocol band 2.

## 2. Modified files

Core runtime:

- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`, `HwaSimIR.h`
- `HwaSim_IR/HwaSim_IR/IR/IRMaterialBandOptics.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/IR/IRSolarHeatingLut.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/IR/IRMaterialThermalState.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.cpp`
- `HwaSim_IR/HwaSim_IR/CMakeLists.txt`, `HwaSim_IR.vcxproj`
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`
- `materials/MaterialBandOptics.csv`

Generation and verification:

- `tools/modtran_generate_cases.py`
- `tools/modtran_run_m1.ps1`
- `tools/modtran_build_solar_heating_lut.py`
- `tools/modtran_solar_heating_qc.py`
- `tools/l1_natural_solar_qc.py`
- `tools/l1_video_qc.py`
- `tools/phase2a_sync60_save_smoke.ps1`
- `DataDrivenTestQT/main.cpp`, `mainwindow.cpp`, `mainwindow.h` (test-only UTC-hour selection; packet layout unchanged)

## 3. M1 atmosphere chain retained

Formal LUT: `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`.

- rows: 1340 (`NIR=1005`, `MWIR=335`)
- generated local cases: 820, missing outputs: 0
- tau: dimensionless
- radiance: `W/(m^2 sr um)`
- irradiance: `W/(m^2 um)`
- response: `responseMode=RectangularBand`; no real SRF is claimed
- native conversion: pointwise `SI = native * 1e8 / lambda_um^2`, followed by wavelength-domain trapezoidal integration/mean
- `TOTAL_RAD` is not used as path or sky diffuse; TOA `SOLAR` is not used as target irradiance

Continuous axes use staged regular-grid interpolation. Tau is converted to optical depth `OD=-ln(tau)`, interpolated, and converted back with `exp(-OD)`. Radiance and irradiance are linearly interpolated. Query altitudes within 1 m of a formal altitude plane are snapped to that plane to suppress ECEF centimetre noise; this is not a general clamp. Any real out-of-range axis or missing cell remains an explicit logged fallback.

MWIR standard case (`Mid-Latitude Summer`, Rural, observer 10 km, target 5 km, range 10 km, visibility 23 km):

- tau: `0.6667283123`, absolute error against golden `7.0e-10`
- path thermal: `0.03609890662 W/(m^2 sr um)`, absolute error `8.0e-11`
- runtime: `Lsensor = tau_up * Lsurface + Lpath_thermal`

NIR standard case at SZA 45 deg:

- tau: `0.9329166851`
- target direct `SOL TR`: `844.956419072 W/(m^2 um)`
- downward diffuse `.flx DOWNWARD`: `134.112843667 W/(m^2 um)`
- LOS `SOL_SCAT`: `0.8262748858 W/(m^2 sr um)`

Detailed QC is in `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/m1_nir_mwir_qc.md`.

## 4. Band optical properties

`materials/MaterialBandOptics.csv` is an independent overlay and does not alter `MaterialDatabase.csv`. No traceable local band measurement was found for the currently used materials, so the band columns are intentionally blank rather than populated with invented values.

Resolution order and exact log values are:

- NIR: explicit `NIRReflectance`, else `1 - SolarAbsorptivity - Transmissivity`, logged `reflectanceSource=band_database` or `solar_absorptivity_fallback`
- MWIR: explicit `MWIRReflectance`, else `1 - ThermalEmissivity - Transmissivity`, logged `reflectanceSource=band_database` or `thermal_emissivity_fallback`

| Material | rho NIR | rho MWIR | Current source | Thickness |
|---|---:|---:|---|---:|
| BM_METAL-ALUMINIUM | 0.70 | 0.55 | controlled fallback | 0.01 m from target XML |
| BM_PAINT | 0.50 | 0.25 | controlled fallback | 0.05 m from primary substrate XML |
| BM_GLASS | 0.76 | 0.07 | controlled fallback | 0.02 m from target XML |
| BM_METAL-IRON | 0.53 | 0.40 | controlled fallback | 0.20 m from target XML |

The resolved absolute paths of `MaterialDatabase.csv`, band optics, sensor JSONs, material-ID TIFF, and material XML are logged at startup. Missing material-ID/XML or mapping is a warning/error, not a silent success.

## 5. Solar geometry and optical shadow

`IRSolarPosition` remains the single shared solar solution. Inputs are platform latitude, longitude, altitude, UTC date and UTC time. A configured UTC fallback date is used only because the existing protocol carries time-of-day rather than a complete date. Runtime logs include lat/lon/alt, UTC, azimuth, elevation, zenith and direction.

The coordinate mapping was checked against the project transform path: Panda world `X=East`, `Y=North`, `Z=Up`. Object-local sun direction is calculated through the actual target `NodePath` transform; no X/Y/Z convention is hard-coded for target attitude.

Optical and thermal visibility are separate values:

```text
sunVisibilityOptical
sunVisibilityThermal
```

L1 v1 uses one shared, low-frequency (default 10 Hz) geometric result for both interfaces:

- target volumes come from each active node's actual tight bounds;
- a sunward ray/sphere test supplies target-to-target cast occlusion;
- per-fragment local normal supplies self-shadow through `max(0,NdotL)`;
- a full-key `platform type + targetPlatID + targetID` prevents cross-target state leakage;
- when the sun is below the horizon, direct solar and both visibility values are forced to zero.

The runtime key mismatch found during the first thermal smoke was fixed and retested: accepted logs contain `sunVisibilityOptical=1` and `sunVisibilityThermal=1` for the unoccluded target. This v1 shared pass does not yet rasterize terrain/building depth, so terrain/building cast shadows are a known limitation; no fixed `SunVisibility=1` is used when the L1 optical-shadow gate is active.

Formal NIR shader equation:

```text
Lsolar = rho_NIR/pi * EDirect * max(0,NdotL) * sunVisibilityOptical
Lsky   = rho_NIR/pi * ESkyDown * skyVisibility
Lsensor_NIR = tau_up * (Lsolar + Lsky) + Lpath_scattering
```

Panda RGB light colour is not part of this radiance equation.

## 6. Broadband MODTRAN solar-heating LUT

Raw root: `HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/l1_solar_heating_20260907`.

- 90 executed MODTRAN cases: 45 direct plus 45 flux
- grid: target altitude `3/5/10/15/20 km`, visibility `5/23/50 km`, SZA `20/45/70 deg`
- categories: `Mid-Latitude Summer`, Rural, `humidity_profile=default`
- wavelength range: 0.30-2.50 um
- formal table: `processed/solar_heating_lut_si.csv`, 45 rows

Direct heating uses target-level MODOUT2 `SOL TR`, converted pointwise from `W/(cm^2 cm^-1)` and wavelength-integrated. Diffuse heating uses `.flx DOWNWARD`, converted from `W/(cm^2 nm)` with `*1e7` and wavelength-integrated. `.flx DIRECT` is retained only as a horizontal diagnostic and is never added to diffuse, preventing direct double counting.

Standard raw paths:

- `raw/l1_solar_heating_20260907/SWHEAT_direct_tar5_vis23_aerRural_humdefault_sza45/MODOUT2.txt`
- `raw/l1_solar_heating_20260907/SWHEAT_flux_tar5_vis23_aerRural_humdefault_sza45/spectral_flux.flx`

Standard result at target 5 km, visibility 23 km, SZA 45 deg:

- direct: `1057.22377655 W/m^2`
- diffuse down: `210.901635455 W/m^2`
- `.flx DIRECT` diagnostic: `654.492486525 W/m^2`

All 45 points and source hashes are recorded in the formal CSV. `processed/l1_solar_heating_qc.md` is PASS.

## 7. Directional material thermal state

`IRMaterialThermalState` is independent of `IRAeroThermalModel`. State is maintained per target, per material/region, and per six object-space normal bins (`+X,-X,+Y,-Y,+Z,-Z`). The fragment shader blends six temperatures from the local normal, so the sun-facing surface warms without instantly heating the entire target and the hotspot direction migrates with attitude/sun motion.

For each bin:

```text
qSolar = alphaSolar * (EDirect * max(0,NdotL) * sunVisibilityThermal
                       + EDiffuse * skyFactor)
qConv  = h(speed) * (T - Tair)
qRad   = epsilon * sigma * (T^4 - Tenv^4)
qCond  = min(conductivity/effectiveThickness, CoreRelaxationWm2K)
         * (T - (baseTemp + aeroDeltaK))
dT/dt  = (qSolar - qConv - qRad - qCond) / arealHeatCapacity
arealHeatCapacity = density * specificHeat * effectiveThickness
```

Units are W/m2 for heat flux, J/(m2 K) for areal heat capacity, K for temperature, seconds for time, kg/m3 for density and J/(kg K) for specific heat. The database `w-sec/gm/K` value is numerically kJ/(kg K) and is converted by `*1000`. XML thickness is preferred; `DefaultEffectiveThicknessM` is used only with `thicknessSource=fallback` logging.

Final MWIR semantics are:

```text
Tsurface = baseTemp + aeroDeltaK + directionalSolarDeltaK
Lsensor_MWIR = tau_up * Lsurface(Tsurface) + Lpath_thermal
```

Engine exhaust/plume stays a local source and is not folded into body solar state. Aero heating is calculated only by `IRAeroThermalModel`; L1 does not double count it.

## 8. A/B and performance results

Deterministic A/B: `processed/l1_natural_solar_ab.md` (PASS, dt=0.1 s).

NIR BM_PAINT sensor radiance:

| SZA | front | back / direct shadow | night | Unit |
|---:|---:|---:|---:|---|
| 20 | 150.785508 | 23.4592253 | 0 | W/(m2 sr um) |
| 45 | 146.196816 | 20.7391222 | 0 | W/(m2 sr um) |
| 70 | 136.028124 | 17.6778972 | 0 | W/(m2 sr um) |

MWIR material sequence (sun 120 s, direct shadow 120 s with diffuse retained, night 120 s):

| Material | delta after sun | delta after shadow | delta after night | Unit |
|---|---:|---:|---:|---|
| aluminum | 1.633358 | 1.609690 | 1.440383 | K |
| paint | 0.807119 | 0.862966 | 0.844410 | K |
| glass | 0.740826 | 0.758715 | 0.708225 | K |

Paint/glass can continue warming briefly in the shadow interval because diffuse flux and stored heat remain; the night interval cools all materials. There are no instantaneous temperature steps.

Using the standard MWIR tau/path and the runtime 4 um spectral Planck function, the corresponding material-ROI radiance/contrast values are:

| Material | unheated | sun 120 s | shadow 120 s | night 120 s | contrast after sun/shadow/night | Unit |
|---|---:|---:|---:|---:|---|---|
| aluminum | 0.168396 | 0.178040 | 0.177896 | 0.176870 | 0.009644 / 0.009500 / 0.008474 | W/(m2 sr um) |
| paint | 0.256594 | 0.264418 | 0.264968 | 0.264785 | 0.007824 / 0.008374 / 0.008191 | W/(m2 sr um) |
| glass | 0.309513 | 0.318407 | 0.318624 | 0.318010 | 0.008894 / 0.009112 / 0.008497 | W/(m2 sr um) |

These are physical radiance-domain ROI equivalents, not display-gray measurements. The full values and method are emitted by `tools/l1_natural_solar_qc.py` into `processed/l1_natural_solar_ab.md`.

Full-chain Windows metrics:

| Band | input/render/output FPS | display FPS | average / P95 latency | input queue max | source lag | drops/overwrites |
|---|---|---:|---|---:|---:|---|
| NIR | 60.10 / 60.30 / 60.21 | 60.22 | 17.39 / 21.42 ms | 3 transient, steady <=1 | max 1, no growth | 0 / 0 |
| MWIR | 60.10 / 60.22 / 60.13 | 60.02 | 17.79 / 25.13 ms | 5 transient, steady <=1 | 0 | 0 / 0 |

Video QC sampled every tenth recorded frame without display changes:

| Band | frames | mean/std gray | black <=1 | white >=254 | center ROI mean/std |
|---|---:|---|---:|---:|---|
| NIR | 720 | 79.133 / 2.903 | 0 | 0 | 79.751 / 8.469 |
| MWIR | 720 | 80.111 / 2.836 | 0 | 0 | 80.718 / 8.390 |

Representative frames:

- `D:\HwaSimIR\logs\phase2a-final-20260907-124548\nir_video_qc.png`
- `D:\HwaSimIR\logs\phase2a-final-20260907-124643\mwir_video_qc.png`

With NaturalSolar and material thermal enabled, the 10-second MWIR smoke sustained 60.06 display FPS, 15.09 ms average latency, continuous source sequence and zero drops/overwrites. Logged solar deltas grew continuously from approximately 0 K to 0.109 K (aluminum), 0.055 K (paint), 0.042 K (glass), and 0.006 K (iron); this difference comes from absorptivity, heat capacity, thickness, losses and orientation rather than an image scale.

## 9. Configuration

`[NaturalSolar]` contains:

- `Enable`, `EnableOpticalShadow`, `EnableSolarThermal`
- `ShadowUpdateHz` and `ThermalUpdateHz` in Hz
- `BroadbandSolarLut` and `MaterialBandOptics`
- `DefaultEffectiveThicknessM` in m
- `ConvectionBaseWm2K` and `ConvectionSpeedCoeffWm2KPerSqrtMps`
- `CoreRelaxationWm2K`, `SkyFactor`, `MaxSolarDeltaK`
- `DebugLog`, `DebugView`

Solar position and common atmosphere state are computed once per low-frequency IR update and reused. Shadow defaults to 10 Hz and thermal state to 10 Hz; the existing atmosphere/radiance update remains approximately 30 Hz while video, attitude and communication remain 60 Hz. LUTs are loaded once, never read per frame, and CSV lookup is never performed in a shader.

## 10. Known incomplete items and stop boundary

- Full terrain/building cast-shadow depth is not in L1 v1; shared target-volume cast occlusion plus per-pixel self-shadow is implemented and logged.
- Real RK3588/Mali OpenGL ES headless smoke was not possible on the x86_64 VM. The aarch64 Release ELF did compile and link cleanly against the configured RK/Panda/MPP sysroots.
- Band reflectance measurements remain unavailable locally. The explicit database schema is ready, while current rows intentionally use logged physics fallbacks.
- SRF remains explicitly `RectangularBand`.
- No solar thermal loading is preserved across process restart.
- VIS/SWIR/LWIR, active illuminator, detector electron/QE model, and multilayer FEM/CFD are not implemented.

Work stops at the NIR/MWIR natural-solar and material thermal-state loop. L2 active illumination and further solar thermal complexity are intentionally not started.
