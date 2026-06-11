# Stage 5A/5B: Minimum Target Radiance Chain

## Goal

Stage 5A adds a default-off debug path that makes the target body visible through a minimal radiance chain:

```text
materialTemperatureK + emissivity -> bodyRadiance
rear ThermalHotspot -> local hotspotRadiance
BrightSpot -> local brightspotRadiance
tauUp -> finalRadianceDebug -> finalGrayDebug
```

This is not final sensor calibration. `finalGrayDebug` is a normalized visibility diagnostic used to prevent a completely black target while the full Stage 5/6 sensor chain is still absent. The current debug floor defaults to 0.12 and is used only when `EnableStage5RadianceDebug=1`.

## Boundary

- `EnableStage5RadianceDebug=0` preserves the legacy render path.
- `EnableStage5RadianceDebug=1` enables the minimal debug path in the target shader.
- Stage 3 remains tau-only by default: `UseModtranTauForAtmosphere=0`.
- Stage 5A does not read `path_radiance`, `sky_radiance`, or `solar_irradiance` fields.
- Stage 5A does not rerun MODTRAN and does not add AGC, MTF, noise, or TCP/JPEG protocol changes.
- Stage 5A does not add damage logic.
- `EnableStage5RadianceDebug=1` must not make `engineState` raise whole-body radiance; the engine contribution stays local to rear `ThermalHotspot`.
- `strikeFlag` and `strikePart` affect only `BrightSpot`, never rear `ThermalHotspot`.

## Hotspot Versus Body Temperature

The target body is visible because of `materialTemperatureK`, `materialEmissivity`, `bodyRadiance`, and the Stage5 debug floor. A whole-target Hotspot is not used.

`ThermalHotspot` remains a local rear engine/nozzle heat source controlled only by `TargetState.engineState`.

`BrightSpot` remains a special local radiance mark controlled only by `WeaponState.strikeFlag` and `strikePart`. The legacy shader uniform `u_brightspot_temp` is still treated as intensity, not Kelvin temperature.

The Stage5A shader branch keeps local Stage4 masks alive: rear hotspot and brightspot masks still overlay the Stage5 body debug output. This avoids the debug branch making the target body visible while accidentally hiding local Hotspot/BrightSpot effects.

## Stage 5A.1 Debug Tone Mapping

The black target body seen with visible hotspot/brightspot is a debug display dynamic-range issue, not evidence that the target needs a whole-target Hotspot. The observed body radiance can be around 0.422 while rear hotspot radiance can be around 918, so a single linear display scale makes the hotspot visible while compressing the body almost to black.

Stage 5A.1 separates debug display mapping for each component:

- `Stage5DebugViewMode=Composite|BodyOnly|HotspotOnly|BrightSpotOnly` (default `Composite`)
- `Stage5DebugToneMap=linear|log|asinh` (default `asinh`)
- `Stage5BodyRadianceScale`
- `Stage5HotspotRadianceScale`
- `Stage5BrightspotRadianceScale`
- `Stage5DebugMinBodyGray=0.12`
- `Stage5UseBaseTextureModulation=0`

`BodyOnly` displays only `bodyGrayAfterFloor`. `HotspotOnly` displays only the rear hotspot mask with `hotspotGray`. `BrightSpotOnly` displays only the brightspot mask with `brightspotGray`. `Composite` adds body, rear hotspot, and brightspot debug components after independent tone mapping, so a bright rear plume no longer forces the body to black.

`Stage5UseBaseTextureModulation` defaults to 0 so debug body visibility is not multiplied down by a dark base texture, duplicated emissivity term, mask, or alpha path. The material/texture path is retained and can be re-enabled for inspection, but Stage 5A.1 prioritizes proving the body gray path reaches pixels.

## Stage 5A.2 Output Visibility

Stage 5A.2 freezes the debug display parameters into:

```text
HwaSim_IR/Bin/Config/IRRadiance/stage5_debug_display.json
```

The file supports per-band settings for:

- `Stage5DebugToneMap`
- `Stage5BodyRadianceScale`
- `Stage5HotspotRadianceScale`
- `Stage5BrightspotRadianceScale`
- `Stage5DebugMinBodyGray`
- `Stage5UseBaseTextureModulation`

The safe defaults remain `toneMap=asinh`, `minBodyGray=0.12`, and `Stage5UseBaseTextureModulation=0`. The config file cannot enable `EnableStage5RadianceDebug`; that switch remains default-off and must be explicitly set at runtime.

`tools/stage5_output_visibility_smoke.ps1` starts a local TCP JPEG receiver on the existing Stage0 video port, runs the Stage5 debug scenarios, and captures the latest output frame when available. If a JPEG frame is captured, the smoke reports `mean_luma`, `max_luma`, and `bright_pixel_count`. Stage 5A.2 still allowed log-gated fallback when the frame stream was unavailable:

- `bodyGrayAfterFloor >= Stage5DebugMinBodyGray`
- `finalGrayDebug > 0`
- engine-on Composite produces `hotspotGray > 0`
- strike scenarios produce `brightspotGray > 0`

This froze the debug display parameters and proved the log-level Stage5 output path. It is still not final AGC/noise/MTF.

## Stage 5A.3 Real Frame Capture

Stage 5A.3 is closed by manual confirmation that rear Hotspot and head/mid BrightSpot are visible, together with the Stage5 logs proving body/hotspot/brightspot radiance values. The automatic output-frame smoke remains available as an optional diagnostic, but it is no longer a required gate for Stage5B.

TCP length-prefixed JPEG capture is still attempted first in the optional smoke and the protocol format is unchanged. The smoke records concrete TCP diagnostics such as connection state, header count, invalid length headers, invalid JPEG payloads, timeout count, byte count, and the first invalid header bytes.

If TCP/JPEG does not produce a decodable image, the smoke enables a Stage5-only render texture dump through runtime environment variables:

- `Stage5OutputFrameDump=1`
- `Stage5OutputFrameDumpPath=<logs/stage5_output_visibility/frames/...png>`
- `Stage5OutputFrameDumpEvery=5`

This dump writes the final render texture to PNG from `capture_task()` only when `EnableStage5RadianceDebug=1`; it does not change the TCP/JPEG protocol and remains default-off outside the smoke. The PNG is written through a temporary file and atomically replaced so a later failed write cannot truncate an already valid captured frame. If both TCP and render-texture fallback fail, the summary and CSV include a specific `capture_failure_reason` instead of a bare unavailable state.

The optional smoke currently places the primary visible target at approximately 3 km. This avoids the earlier close-range 1.1 km target while keeping local Hotspot/BrightSpot above the default legacy tau floor. It is retained only as a historical/diagnostic tool and is not part of the Stage5B strict gate.

The 5A.3 scenarios remain:

- MWIR `BodyOnly`
- MWIR `Composite` engine off
- MWIR `Composite` engine on
- LWIR `BodyOnly`
- NIR `BrightSpotOnly`
- SWIR `BrightSpotOnly`

The minimum frame-level criteria are debug visibility, not physical calibration. Stage 5A is now closed for log-level evidence plus user-confirmed visual Hotspot/BrightSpot visibility; automatic frame capture can be improved later without blocking Stage5B.

## Stage 5B Direct Solar Reflection

Stage 5B adds a minimal empirical direct solar reflection term:

```text
reflectedRadiance = reflectance_band * solarStrength * max(0, dot(normal, sunDir)) * textureLuma * bandSolarWeight
```

This is not MODTRAN `solar_irradiance` table integration. `solarStrength` comes from the existing runtime environment, which already combines weather code and sun elevation. `sunDir` is derived on the CPU from sun azimuth/elevation and passed to the shader. The shader uses the mesh normal when available and falls back to a constant +Z direction if the normal is unavailable.

`textureLuma` comes from `baseColorTex` in the shader. If the target has no base texture, the CPU log emits `STAGE5_BASE_TEXTURE_FALLBACK` and the model-level diagnostic uses `textureLuma=1`.

The per-band empirical weights are configured in `stage5_debug_display.json`:

- VIS: `1.0`
- NIR: `0.8`
- SWIR: `0.7`
- MWIR: `0.15`
- LWIR: `0.0`

Composite debug output is now:

```text
bodyGray + reflectedGray + hotspotGray * rearHotspotMask + brightspotGray * brightspotMask
```

VIS/NIR/SWIR should show stronger texture and sun-direction differences. MWIR keeps body thermal and rear hotspot dominant while allowing weak reflected contribution. LWIR defaults to zero reflected contribution and remains thermal/body/hotspot focused.

## 阶段 5C：Stage5 输出视觉标定与 legacy 对齐

Manual comparison showed that `EnableStage5RadianceDebug=1` made the target body visible but made rear Hotspot and BrightSpot weaker than the legacy `EnableStage5RadianceDebug=0` path. Stage 5C treats this as a display-composite calibration issue, not a missing physics term.

`stage5_debug_display.json` now adds per-band display calibration:

- `Stage5BodyDisplayGain`
- `Stage5ReflectedDisplayGain`
- `Stage5HotspotDisplayGain`
- `Stage5BrightspotDisplayGain`
- `Stage5CompositeMinGray`
- `Stage5CompositeMaxGray`

These values only affect the Stage5 debug display path. `EnableStage5RadianceDebug=0` still preserves the current legacy output, and the config file still cannot turn Stage5 debug on by default.

The Stage5C shader composite consumes calibrated display gray values:

```text
bodyDisplayGray + reflectedDisplayGray + rearHotspotDisplayGray * rearHotspotMask + brightspotDisplayGray * brightspotMask
```

Raw `bodyRadiance`, `reflectedRadiance`, `hotspotRadiance`, and `brightspotRadiance` stay unchanged in `[Stage5 Radiance]`. If Stage5 raw hotspot/brightspot gray is visually weaker than the legacy local intensity, the debug display layer uses:

```text
rear hotspot display = max(stage5HotspotGray * gain, legacyRearHotspotIntensity * gain)
brightspot display   = max(stage5BrightspotGray * gain, legacyBrightspotIntensity * gain)
```

This fallback is display-only and is logged as `stage5DisplayFallbackApplied=1`. It does not change physical radiance values, does not add a whole-target Hotspot, and does not change the `engineState -> rear ThermalHotspot` or `strikeFlag/strikePart -> BrightSpot` separation.

Stage5C is not Stage 6 AGC. It only aligns the Stage5 debug composite with the already-visible legacy local features. Full AGC, noise, MTF, 8-bit mapping, and final imaging-system behavior remain deferred to Stage 6. `tools/stage5_output_visibility_smoke.ps1` remains optional.

## 阶段 5D：波段语义统一与 Stage5 收口

Manual true/false comparison has confirmed that `EnableStage5RadianceDebug=1` and the legacy `EnableStage5RadianceDebug=0` path both keep the target body, rear Hotspot, and BrightSpot visible. Stage5C visual calibration is therefore accepted for the current debug-display goal.

Stage 5D fixes the remaining band-numbering ambiguity before Stage 6. The protocol mapping stays unchanged:

- protocol `0 = SWIR`
- protocol `1 = NIR`
- protocol `2 = MWIR`
- protocol `3 = LWIR`
- protocol `4 = VIS`

The shader now receives explicit internal band semantics:

- `u_ir_band_index`: `0=VIS`, `1=NIR`, `2=SWIR`, `3=MWIR`, `4=LWIR`
- `u_ir_band_class`: `0=reflective` for VIS/NIR/SWIR, `1=mixed` for MWIR, `2=thermal` for LWIR

`u_wave_band` is retained only as a deprecated compatibility uniform and should not be used for new shader logic. Legacy body/detail weighting and sky tint now use `u_ir_band_class`, so reflective bands keep texture/solar detail, MWIR remains mixed thermal plus weak reflection, and LWIR stays thermal dominant.

This is only a semantic cleanup. It does not add physics, does not tune Hotspot/BrightSpot brightness, and does not change Stage5C display gains. With this closed, the next recommended step is Stage6A for the imaging-system postprocess path.

## Debug Outputs

Runtime logs use this prefix:

```text
[Stage5 Radiance]
```

Each target log includes:

- `targetKey`
- `band`
- `debugViewMode`
- `toneMap`
- `materialTempK`
- `emissivity`
- `tauUp`
- `sunElevation`
- `sunAzimuth`
- `ndotl`
- `textureLuma`
- `reflectanceBand`
- `solarWeight`
- `bodyRadiance`
- `reflectedRadiance`
- `hotspotRadiance`
- `brightspotRadiance`
- `bodyGrayBeforeFloor`
- `bodyGrayAfterFloor`
- `reflectedGray`
- `hotspotGray`
- `brightspotGray`
- `finalGrayDebug`
- `debugFloorApplied`
- `stage5DisplayFallbackApplied`

Stage5C display calibration logs use this prefix:

```text
[Stage5C VisualCalib]
```

Each log includes `band`, `bodyGray`, `reflectedGray`, `hotspotGrayRaw`, `hotspotGrayDisplay`, `brightspotGrayRaw`, `brightspotGrayDisplay`, `finalGrayDebug`, and `fallbackApplied`.

If the debug gray remains zero for consecutive frames, HwaSimIR emits:

```text
STAGE5_TARGET_BODY_RADIANCE_ZERO
```

If positive body radiance maps to zero gray, HwaSimIR emits:

```text
STAGE5_BODY_GRAY_ZERO_AFTER_MAPPING
```

When `bodyGrayAfterFloor > 0.1`, the log also emits the diagnostic hint:

```text
CHECK_SHADER_BODY_GRAY_PATH_OR_FRAME_OUTPUT
```

For Stage5B reflection diagnostics:

```text
STAGE5_REFLECTED_GRAY_ZERO
STAGE5_BASE_TEXTURE_FALLBACK
STAGE5_NORMAL_FALLBACK
```

## Verification

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1
powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_tau_loader_check.ps1 -Strict
powershell -ExecutionPolicy Bypass -File tools\stage4_hotspot_check.ps1 -Strict
powershell -ExecutionPolicy Bypass -File tools\stage5_min_radiance_check.ps1 -Strict
```

`tools/stage5_output_visibility_smoke.ps1` remains optional and is not part of the strict Stage5B gate.
