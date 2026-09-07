# HwaSimIR L2 Active Illuminator Implementation

Date: 2026-09-07  
Baseline: `main` at `0e2bd6e3ddea6fd6a33217b1ec2a9ce34a5dbf5e`  
Scope: NIR 0.70-1.10 um and MWIR 3.00-5.00 um only

## 1. Outcome and controlled production gate

L2 now has an independent, SI-unit active-illuminator component. It consumes only the existing protocol fields, performs explicit source/sensor spectral compatibility, cone and material incidence tests, applies co-located reciprocal MODTRAN transmission in both directions, and adds the result to the M1 NIR/MWIR sensor-radiance chain without changing material temperature.

Windows Release x64, the deterministic module QC, the real three-process TCP chain, NIR/MWIR protocol off/on/off, and the aarch64 clean Release link all passed. The real RK3588 at `192.168.1.116` was not reachable, so Mali execution is `NOT RUN` rather than being inferred from the x86_64 cross-build VM.

Production defaults remain controlled off:

```ini
[M1NirMwirPhysics]
CompareOnly=false
EnableRuntime=false
EnableNIRRuntime=false
EnableMWIRRuntime=false

[NaturalSolar]
Enable=false

[ActiveIlluminator]
Enable=false
```

All acceptance runs temporarily changed the gates and restored `HwaSimIRRuntime.ini` byte-for-byte in `finally`. No tone map, gain, body floor, empirical atmospheric scale, or material temperature was changed.

| Gate | Result | Evidence |
| --- | --- | --- |
| Material deployment/hash | PASS | `tools/l2_material_deployment_check.py`; Config and root SHA-256 values match |
| Active module QC | PASS, 24 rows | `D:\HwaSimIR\logs\l2-active-qc-20260907-221308\l2_active_illuminator_qc.csv` |
| M1 unit/QC regression | PASS | `tools/m1_nir_mwir_physics_check.ps1` |
| Windows Release x64 | PASS | MSBuild Release x64; SHA-256 `DBA919D18515011F8A544A3807B9D8C809452D149DF1F0D9ED731FBF9E26FA28` |
| DataDrivenTestQT Release | PASS | MinGW 7.3 Release rebuild; only test controls changed |
| NIR night TCP off/on/off | PASS | `logs/phase2a-final-20260907-190224` |
| NIR daylight TCP off/on/off | PASS | `logs/phase2a-final-20260907-190323` |
| MWIR TCP off/on/off | PASS | `logs/phase2a-final-20260907-190419` |
| MWIR sensor + NIR source mismatch | PASS, exact zero | `logs/phase2a-final-20260907-181838` |
| aarch64 clean Release | PASS | VM `192.168.203.128`, 42/42 compile/link, ARM ELF |
| real RK3588/Mali | NOT RUN | `192.168.1.116:22` unreachable on 2026-09-07 |

The physical/routing gate is a production candidate. `ActiveIlluminator.Enable` remains false until a real RK3588 smoke and operator acceptance of the existing final display mapping are completed.

## 2. Modified files

Runtime and physics:

- `HwaSim_IR/HwaSim_IR/IR/IRActiveIlluminator.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp`, `.h`
- `HwaSim_IR/HwaSim_IR/CMakeLists.txt`
- `HwaSim_IR/HwaSim_IR/HwaSim_IR.vcxproj`
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`

Deployment resources:

- `HwaSim_IR/Bin/Config/Materials/MaterialDatabase.csv`
- `HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv`

Test sender and verification:

- `DataDrivenTestQT/main.cpp`, `mainwindow.cpp`, `mainwindow.h`
- `tools/l2_material_deployment_check.py`
- `tools/l2_active_illuminator_unit.cpp`
- `tools/l2_active_illuminator_check.ps1`
- `tools/l2_video_ab_qc.py`
- `tools/phase2a_sync60_save_smoke.ps1`

`Common/CommonData.h`, its packet ordering, and all DDS/legacy protocol definitions were not changed. DataDrivenTestQT still reports packed sizes `24/385/506/17` bytes.

## 3. Material deployment closure

Production lookup order is now:

1. `Config/Materials/MaterialDatabase.csv` and `Config/Materials/MaterialBandOptics.csv` relative to `HwaSim_IR/Bin`;
2. compatible `HwaSim_IR/Bin/Config/Materials/...` candidates for development launch directories;
3. repo-root `materials/...` only as a development fallback.

The accepted TCP logs resolve both production resources inside `Bin`:

```text
MaterialDatabase=D:\HwaSimIR\HwaSim_IR\Bin\Config\Materials\MaterialDatabase.csv loaded=1
MaterialBandOptics=D:\HwaSimIR\HwaSim_IR\Bin\Config\Materials\MaterialBandOptics.csv loaded=1
```

Copy consistency:

| Resource | SHA-256 |
| --- | --- |
| `MaterialDatabase.csv` | `70930cfcf81c94cea46fd06f9a43832c2fedd992a7d786b206c59700456342a1` |
| `MaterialBandOptics.csv` | `5be04078b980a9d6f5a755b04ed846a1285262d6a9f0afb3d765158665883464` |

The startup loader independently computes FNV-1a-64 when both Config and root files exist and prints `[MaterialDeployment][WARN] ... reason=config_and_repo_root_copies_drifted` on mismatch. The check script uses SHA-256 and fails its gate on mismatch. The root copies were intentionally retained.

`MaterialBandOptics.csv` does not invent measurements. Its NIR/MWIR fields remain empty where no traceable band measurement exists, so runtime records the L1 fallbacks:

- NIR: `1 - SolarAbsorptivity - Transmissivity`, source `solar_absorptivity_fallback`;
- MWIR: `1 - ThermalEmissivity - Transmissivity`, source `thermal_emissivity_fallback`.

The aluminum/paint/glass QC values `0.70/0.50/0.27` are the NIR fallbacks calculated from the checked-in `MaterialDatabase.csv`, not new band measurements.

## 4. Protocol input and lifecycle

Only existing fields are used:

- initialization: `trackerSensorParam.illuminatorAngle`, interpreted as mrad;
- initialization: `trackerSensorParam.illuminatorSpotRad`, unit currently unspecified by the protocol;
- real time: `WeaponState.illuminatorEn`.

The source position is the current sensor/camera optical center. Source direction is the current tracker camera boresight, transformed from camera local `+Y` into `m_renderRoot`; it is not a fixed world XYZ assumption.

`illuminatorX/Y/Z` and `illuminatorPitch/Yaw/Roll` remain present and unused. Reinitialization and reset clear the active visibility and log state. Real-time off/on changes are consumed on the next low-frequency IR update; they do not require a new initialization packet.

DataDrivenTestQT adds only command-line test controls. `--freeze-geometry` holds the first existing trajectory sample for fixed-camera/target image A/B and does not alter a packet. The accepted sender log includes:

```text
[ProtocolLayout] ... 24/385/506/17
[StimIlluminatorConfig] angleMrad=5 spotRad=1000 ... protocolLayoutUnchanged=1
[StimGeometry] freezeGeometry=1 protocolLayoutUnchanged=1
```

## 5. Angle, spectrum, and intensity semantics

### 5.1 Full-cone angle

```text
halfAngleRad = illuminatorAngle_mrad * 1e-3 * 0.5
spotRadiusM  = rangeM * tan(halfAngleRad)
fullAngleDeg = illuminatorAngle_mrad * 1e-3 * 180/pi   # debug Spotlight only
```

For a 1 km range the unit gate produced radii `0.500000`, `1.000000`, and `2.500005 m` for `1/2/5 mrad`, monotonically increasing. No mrad value is passed to Panda as degrees.

### 5.2 Band compatibility

`Band=NIR`, `MWIR`, and `FollowSensor` are supported. The semantic band and explicit `[center-bandwidth/2, center+bandwidth/2]` interval must both overlap the SensorWave `SpectralResponseRange`. A deliberately inconsistent `Band=NIR` with a 4 um interval is rejected. `illuminatorEn=false`, global `Enable=false`, unsupported sensor band, or mismatch each produce exact zero.

Logs expose `sourceBand`, `sensorBand`, `spectralOverlap`, `overlapWidthUm`, and `activeContributionEnabled`. Runtime mismatch evidence has `sourceBand=NIR sensorBand=MWIR spectralOverlap=0 activeSensorWm2SrUm=0 fallbackReason=spectral_band_mismatch`.

Current SensorWave data has no valid SRF curve, so active band-integrated radiance is divided by the actual overlap width and enters M1 as band-mean spectral radiance. The log states `responseMode=RectangularBand`; no real SRF is claimed.

### 5.3 Intensity modes

`LegacyNormalized` uses:

```text
normalized = max(0, illuminatorSpotRad)
Eref = normalized * LegacyMaxReferenceIrradianceWm2
intensitySource=legacy_normalized_config_fallback
```

This is an explicit config calibration, not a claim that the protocol value is W/m2. It is intentionally not renamed into a physical protocol field.

`BandIrradianceAtReference` ignores the raw `illuminatorSpotRad` magnitude and uses configured `ReferenceIrradianceWm2 [W/m2]` at `ReferenceRangeM [m]`, logging `intensitySource=physical_reference_irradiance`. The unit test sets raw value 999 and confirms that configured `2 W/m2` remains exactly `2 W/m2`.

## 6. Beam, incidence, visibility, and atmosphere

The CPU module computes a target-center reference for audit logs. The GLES-compatible IR shader computes per fragment using world position and world normal:

```text
Egeom     = Eref * (Rref/R)^2 * beamFactor
Etarget   = Egeom * tauOutbound
Eincident = Etarget * max(0, N dot Lsource) * activeVisibility
Lsurface  = rho_band/pi * Eincident
Lsensor   = tauInbound * Lsurface
```

Gaussian is one on boresight and `1/16` at the declared cone edge, then zero outside. TopHat is one inside and zero outside. The module gate covers center, edge, outside, and occluded cases.

`activeVisibility` is independent from both L1 sun-visibility variables. A shared, low-frequency target-volume cast checks other targets between sensor and each target at `ShadowUpdateHz` (default 30 Hz). Per-fragment `N dot Lsource` provides self-facing semantics. This is intentionally not advertised as terrain/building shadow support.

The platform and sensor are co-located, so the outbound optical depth uses reciprocity:

```text
tauOutbound = tauInbound
outboundTauSource=co_located_reciprocal_modtran
LactiveSensor proportional to tauOutbound * tauInbound
```

It therefore contains two transmissions, not one. Invalid/OOR MODTRAN results produce zero contribution with `fallbackAxis/query/min/max` and `invalid_modtran_tau:<reason>`; no silent clamp is used. NIR line transmission is solar-angle independent. When a night query fails only on `solarZenithDeg`, active tau is taken from the audited SZA 45-degree slice with the explicit mode suffix `active_tau_solar_independent_slice_sza45`; the invalid NIR solar/sky/path fields are not reused.

Range QC, using checked NIR transmission values and `rho=0.5`, `Eref=1 W/m2`, `Rref=1 km`, 0.05 um overlap:

| R (km) | Egeom W/m2 | tau out/in | Etarget W/m2 | Lsurface W/(m2 sr um) | Lsensor W/(m2 sr um) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 1.000000 | 0.996138 | 0.996138 | 3.17080 | 3.15856 |
| 2 | 0.250000 | 0.993290 | 0.248323 | 0.790435 | 0.785132 |
| 5 | 0.040000 | 0.986040 | 0.0394416 | 0.125547 | 0.123794 |
| 10 | 0.010000 | 0.975534 | 0.00975534 | 0.0310522 | 0.0302925 |

## 7. Radiance-chain integration

`IRRadianceModelV2` keeps separate `activeSurfaceRadiance` and `activeSensorRadiance`. Only the sensor-side value is added to the final sensor equation, avoiding a second inbound multiplication.

NIR:

```text
LnaturalSurface = Lsolar + Lsky
LactiveSurface  = rho_NIR/pi * Eincident
Lsensor_NIR     = tauInbound * LnaturalSurface
                + LpathScattering
                + tauInbound * LactiveSurface
```

MWIR:

```text
Lsensor_MWIR = tauInbound * LthermalSurface(Tbase + aeroDelta + solarDelta)
             + LpathThermal
             + tauInbound * LactiveSurface
```

The active component does not write `IRMaterialThermalState`; it is reflection only. It does not overwrite solar, sky, body, path, plume, hotspot, or brightspot components. Visible texture luminance is not used for NIR/MWIR active reflectance.

## 8. Automated and image A/B results

### 8.1 Deterministic module gate

`tools/l2_active_illuminator_check.ps1` compiles the independent module and runs 24 checks:

- enable `0 -> 1 -> 0`;
- angle `1/2/5 mrad`;
- range `1/2/5/10 km` with explicit two-way tau fields;
- NIR, MWIR, FollowSensor and both mismatch directions;
- Gaussian center/edge/outside and target-cast occlusion;
- invalid tau explicit zero fallback;
- aluminum, paint, glass fallback-reflectance ordering;
- both intensity modes.

### 8.2 Fixed-geometry image gate

The final image runs use unchanged display parameters and temporarily set only `Annotation.OverlayInSensorImage=false`, because the green annotation occupied most of the 30x14-pixel target ROI. JSON target annotations remained enabled and supplied the exact ROI.

| Case | Physical active off/on/off W/(m2 sr um) | ROI contrast off / on / off (gray) | Frames | Result |
| --- | --- | --- | ---: | --- |
| NIR night, solar elevation -42.78 deg | `0 / 8.228928 / 0` | `-0.833 / -2.716 / -0.871` | 420 | reversible active image response; no saturation |
| NIR day, solar elevation 56.18 deg | `0 / 8.228928 / 0` | `7.428 / 7.483 / 7.483` | 421 | active increment is small against natural daylight |
| MWIR | `0 / 0.353384 / 0` | `7.355 / 7.405 / 7.405` | 420 | active component present; no thermal-state change |

All three videos have zero `gray<=1` and zero `gray>=254` ratios. The nighttime target response is reversible but appears darker in the current final display mapping even though the logged physical radiance is strictly positive. This report does not change polarity/tone mapping to make the screenshot look brighter; operator acceptance of that existing display response remains a deployment-gate item.

Representative outputs:

- `logs/phase2a-final-20260907-190224/nir_night_fixed_no_overlay_active_ab_{off_before,on,off_after}.png`
- `logs/phase2a-final-20260907-190323/nir_day_fixed_no_overlay_active_ab_{off_before,on,off_after}.png`
- `logs/phase2a-final-20260907-190419/mwir_fixed_no_overlay_active_ab_{off_before,on,off_after}.png`

The corresponding JSON files contain ROI mean, surrounding mean, contrast, black/white ratio, frame index, and physical active radiance.

## 9. TCP 60 Hz and latency

Final fixed-geometry `DataDrivenTestQT -> HwaSim_IR -> HwaSim_IR_VideoDisplay` results:

| Metric | NIR night | NIR day | MWIR |
| --- | ---: | ---: | ---: |
| sender FPS | 60.151 | 60.153 | 60.151 |
| render FPS | 60.316 | 60.667 | 60.723 |
| output FPS | 59.977 | 60.322 | 60.381 |
| display FPS | 59.977 | 60.408 | 60.485 |
| average latency ms | 20.254 | 19.607 | 18.898 |
| P95 latency ms | 32.944 | 24.326 | 25.357 |
| max input queue | 5 | 3 | 3 |
| max sourceSeqLag | 0 | 0 | 1 |
| dropped / overwritten | 0 / 0 | 0 / 0 | 0 / 0 |
| recorded / probed frames | 420 / 420 | 421 / 421 | 420 / 420 |

Active state is reused at the existing low-frequency IR update. The shader uses uniforms; it does not read CSV or create Panda objects per frame. `DebugVisualCone=false` creates no debug Spotlight.

## 10. Windows, aarch64, and RK3588

Windows Release x64 compiled and linked after the new module was added to both VS and CMake build descriptions. The accepted runtime executable was copied to `HwaSim_IR/Bin/HwaSim_IR.exe` for the tests.

The user-provided VM `linaro@192.168.203.128` was online. It identifies as `x86_64` and is used only for cross compilation. After syncing the L2 source and Config materials, the first regeneration correctly failed because the new IR files had been copied to the project root. They were moved to the required remote `IR/` directory and the command was rerun with `--clean-first`; 41 old objects were removed and Ninja rebuilt/link 42/42 successfully.

Final artifact:

```text
/home/linaro/userdata/HwaSimIR/cmake-build-codex-rk3588/HwaSim_IR
ELF 64-bit LSB pie executable, ARM aarch64
BuildID d665db67bb94aaafffb8efe77021447720acd264
SHA-256 e1458921393eeb9f1f312f63537e0c719eb0b65d764f61217679313b509359d8
size 2437280 bytes
timestamp 2026-09-07 18:29:25 +0800
```

The real board `192.168.1.116:22` did not accept a TCP connection. Consequently, resource copy to the board, HeadlessOffscreen startup, Mali renderer confirmation, and NIR/MWIR board smoke are all `NOT RUN`. The VM result is not reported as an RK3588 runtime pass.

## 11. Known limitations and stop boundary

- `illuminatorSpotRad` still has no trustworthy protocol unit; default runtime remains `LegacyNormalized` with an explicit config calibration source.
- Active occlusion covers target-to-target cast volumes plus per-fragment facing. Terrain/building full-depth shadow is not implemented.
- The optional Panda Spotlight/debug cone is not instantiated; formal Visible/Headless computation is shader/math based and independent of RGB light color.
- NIR/MWIR still use `RectangularBand`, not a measured SRF or detector QE/electron model.
- Nighttime NIR physical response is correct and reversible in logs/video, but the existing final display mapping renders its target contrast darker; this stage deliberately does not tune gray presentation.
- Real RK3588/Mali runtime acceptance remains pending connectivity.
- No VIS/SWIR/LWIR, active thermal loading, new protocol fields, detector QE/electron model, or solar-thermal extension was added.

L2 stops here. It does not continue into VIS/SWIR/LWIR or a higher-fidelity detector stage.
