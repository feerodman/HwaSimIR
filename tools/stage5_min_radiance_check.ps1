param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$radianceHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.h"
$radianceSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp"
$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$irTypesSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRTypes.cpp"
$stage3AtmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$stage4Check = Join-Path $rootPath "tools\stage4_hotspot_check.ps1"
$stage5Smoke = Join-Path $rootPath "tools\stage5_min_radiance_smoke.ps1"
$stage5BodySmoke = Join-Path $rootPath "tools\stage5_body_visibility_smoke.ps1"
$stage5OutputSmoke = Join-Path $rootPath "tools\stage5_output_visibility_smoke.ps1"
$stage5DebugDisplayConfig = Join-Path $rootPath "HwaSim_IR\Bin\Config\IRRadiance\stage5_debug_display.json"

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail
    )

    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Read-Text {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8
}

function Test-TextContains {
    param([string]$Path, [string]$Pattern)
    $text = Read-Text $Path
    return ($text -match $Pattern)
}

$radianceHeaderText = Read-Text $radianceHeader
$radianceSourceText = Read-Text $radianceSource
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$irTypesSourceText = Read-Text $irTypesSource
$stage3AtmosphereText = Read-Text $stage3AtmosphereSource
$stage5DebugDisplayConfigText = Read-Text $stage5DebugDisplayConfig
$stage5OutputSmokeText = Read-Text $stage5OutputSmoke
$radianceText = "$radianceHeaderText`n$radianceSourceText"

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IRRadianceModelV2 header exists" (Test-Path -LiteralPath $radianceHeader -PathType Leaf) $radianceHeader)) | Out-Null
$checks.Add((Add-Check "IRRadianceModelV2 source exists" (Test-Path -LiteralPath $radianceSource -PathType Leaf) $radianceSource)) | Out-Null
$checks.Add((Add-Check "Stage5 V2 exposes required inputs" (($radianceHeaderText -match "materialTemperatureK") -and ($radianceHeaderText -match "materialEmissivity") -and ($radianceHeaderText -match "materialReflectance") -and ($radianceHeaderText -match "tauUp") -and ($radianceHeaderText -match "solarStrength") -and ($radianceHeaderText -match "solarReflectanceWeight") -and ($radianceHeaderText -match "hotspotTemperatureK") -and ($radianceHeaderText -match "hotspotIntensity") -and ($radianceHeaderText -match "brightspotIntensity")) $radianceHeader)) | Out-Null
$checks.Add((Add-Check "Stage5 V2 exposes required outputs" (($radianceHeaderText -match "bodyRadiance") -and ($radianceHeaderText -match "reflectedRadiance") -and ($radianceHeaderText -match "hotspotRadiance") -and ($radianceHeaderText -match "brightspotRadiance") -and ($radianceHeaderText -match "finalRadianceDebug") -and ($radianceHeaderText -match "bodyGrayBeforeFloor") -and ($radianceHeaderText -match "bodyGrayAfterFloor") -and ($radianceHeaderText -match "reflectedGray") -and ($radianceHeaderText -match "hotspotGray") -and ($radianceHeaderText -match "brightspotGray") -and ($radianceHeaderText -match "finalGrayDebug") -and ($radianceHeaderText -match "debugFloorApplied")) $radianceHeader)) | Out-Null
$checks.Add((Add-Check "Stage5 debug config exposes tone map and independent scales" (($radianceHeaderText -match "IRStage5ToneMap") -and ($radianceHeaderText -match "bodyRadianceScale") -and ($radianceHeaderText -match "hotspotRadianceScale") -and ($radianceHeaderText -match "brightspotRadianceScale") -and ($radianceHeaderText -match "minBodyGray") -and ($radianceHeaderText -match "bodyDisplayGain") -and ($radianceHeaderText -match "hotspotDisplayGain") -and ($radianceHeaderText -match "brightspotDisplayGain") -and ($radianceHeaderText -match "compositeMinGray") -and ($radianceHeaderText -match "compositeMaxGray")) $radianceHeader)) | Out-Null
$checks.Add((Add-Check "band center approximation implemented" (($radianceSourceText -match "0\.55") -and ($radianceSourceText -match "0\.90") -and ($radianceSourceText -match "1\.80") -and ($radianceSourceText -match "4\.00") -and ($radianceSourceText -match "10\.0")) $radianceSource)) | Out-Null
$checks.Add((Add-Check "Planck constants use wavelength um units" (($radianceSourceText -match "1\.191042e8") -and ($radianceSourceText -match "1\.4387752e4") -and ($radianceSourceText -match "W/\(m\^2 sr um\)")) $radianceSource)) | Out-Null

$checks.Add((Add-Check "IRRadianceModelV2 in CMake" (Test-TextContains $cmakePath "IR/IRRadianceModelV2\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IRRadianceModelV2 in VS project" ((Test-TextContains $vcxprojPath "IR\\IRRadianceModelV2\.cpp") -and (Test-TextContains $vcxprojPath "IR\\IRRadianceModelV2\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IRRadianceModelV2 in VS filters" ((Test-TextContains $filtersPath "IR\\IRRadianceModelV2\.cpp") -and (Test-TextContains $filtersPath "IR\\IRRadianceModelV2\.h")) $filtersPath)) | Out-Null
$protocolBandMappingOk = ($irTypesSourceText -match "case 0:\s*return IRBand::ShortWaveInfrared") -and
    ($irTypesSourceText -match "case 1:\s*return IRBand::NearInfrared") -and
    ($irTypesSourceText -match "case 2:\s*return IRBand::MidWaveInfrared") -and
    ($irTypesSourceText -match "case 3:\s*return IRBand::LongWaveInfrared") -and
    ($irTypesSourceText -match "case 4:\s*return IRBand::Visible")
$checks.Add((Add-Check "IRBandFromProtocol mapping unchanged" $protocolBandMappingOk $irTypesSource)) | Out-Null

$configParsed = $false
$configHasBands = $false
$configDefaultsOk = $false
$solarWeightsOk = $false
$displayGainsOk = $false
try {
    $stage5ConfigObj = $stage5DebugDisplayConfigText | ConvertFrom-Json
    $configParsed = $true
    $bandNames = @("VIS", "NIR", "SWIR", "MWIR", "LWIR")
    $displayGainFields = @(
        "Stage5BodyDisplayGain",
        "Stage5ReflectedDisplayGain",
        "Stage5HotspotDisplayGain",
        "Stage5BrightspotDisplayGain",
        "Stage5CompositeMinGray",
        "Stage5CompositeMaxGray"
    )
    $configHasBands = $true
    foreach ($bandName in $bandNames) {
        if (-not ($stage5ConfigObj.bands.PSObject.Properties.Name -contains $bandName)) {
            $configHasBands = $false
        }
    }
    $configDefaultsOk = ($stage5ConfigObj.defaults.Stage5DebugToneMap -eq "asinh") -and
        ([double]$stage5ConfigObj.defaults.Stage5DebugMinBodyGray -eq 0.12) -and
        ([bool]$stage5ConfigObj.defaults.Stage5UseBaseTextureModulation -eq $false)
    $solarWeightsOk = ([double]$stage5ConfigObj.Stage5SolarReflectanceWeight_VIS -eq 1.0) -and
        ([double]$stage5ConfigObj.Stage5SolarReflectanceWeight_NIR -eq 0.8) -and
        ([double]$stage5ConfigObj.Stage5SolarReflectanceWeight_SWIR -eq 0.7) -and
        ([double]$stage5ConfigObj.Stage5SolarReflectanceWeight_MWIR -eq 0.15) -and
        ([double]$stage5ConfigObj.Stage5SolarReflectanceWeight_LWIR -eq 0.0)
    $displayGainsOk = $true
    foreach ($fieldName in $displayGainFields) {
        if (-not ($stage5ConfigObj.defaults.PSObject.Properties.Name -contains $fieldName)) {
            $displayGainsOk = $false
        }
    }
    foreach ($bandName in $bandNames) {
        foreach ($fieldName in $displayGainFields) {
            if (-not ($stage5ConfigObj.bands.$bandName.PSObject.Properties.Name -contains $fieldName)) {
                $displayGainsOk = $false
            }
        }
    }
}
catch {
    $configParsed = $false
}

$checks.Add((Add-Check "EnableStage5RadianceDebug defaults off" (($appSourceText -match 'getBool\("Stage5",\s*"EnableRadianceDebug",\s*"EnableStage5RadianceDebug",\s*false') -and ($appHeaderText -match "m_enableStage5RadianceDebug = false")) "$appHeader; $appSource")) | Out-Null
$checks.Add((Add-Check "stage5 debug display config exists" (Test-Path -LiteralPath $stage5DebugDisplayConfig -PathType Leaf) $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "stage5 debug display config parses" $configParsed $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "stage5 debug display config has all bands" $configHasBands $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "stage5 debug display config safe defaults" $configDefaultsOk $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "Stage5B solar reflectance weights configured" $solarWeightsOk $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "Stage5C display gain config exists per band" $displayGainsOk $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "stage5 debug display config cannot enable debug" ($stage5DebugDisplayConfigText -notmatch "EnableStage5RadianceDebug") $stage5DebugDisplayConfig)) | Out-Null
$checks.Add((Add-Check "Stage5 loads display config by band" (($appSourceText -match "stage5_debug_display\.json") -and ($appSourceText -match "LoadStage5DebugDisplayConfig") -and ($appSourceText -match "m_stage5DebugConfigs\[stage5BandIndex\]") -and ($appSourceText -match "m_stage5UseBaseTextureModulationByBand\[stage5BandIndex\]")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 debug env vars supported" (($appSourceText -match "Stage5DebugViewMode") -and ($appSourceText -match "Stage5DebugToneMap") -and ($appSourceText -match "Stage5BodyRadianceScale") -and ($appSourceText -match "Stage5HotspotRadianceScale") -and ($appSourceText -match "Stage5BrightspotRadianceScale") -and ($appSourceText -match "Stage5DebugMinBodyGray") -and ($appSourceText -match "Stage5UseBaseTextureModulation") -and ($appSourceText -match "Stage5SolarReflectanceWeight_LWIR") -and ($appSourceText -match "Stage5BodyDisplayGain") -and ($appSourceText -match "Stage5ReflectedDisplayGain") -and ($appSourceText -match "Stage5HotspotDisplayGain") -and ($appSourceText -match "Stage5BrightspotDisplayGain") -and ($appSourceText -match "Stage5CompositeMinGray") -and ($appSourceText -match "Stage5CompositeMaxGray")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 startup hint logs disabled state" (($appSourceText -match "Stage5 debug disabled, legacy output may remain dark") -and ($appSourceText -match "\[Stage5 Radiance\] EnableStage5RadianceDebug")) $appSource)) | Out-Null
$checks.Add((Add-Check "shader has unified band index/class uniforms" (($appSourceText -match "uniform int u_ir_band_index") -and ($appSourceText -match "uniform int u_ir_band_class") -and ($appSourceText -match "u_ir_band_class.*0 reflective") -and ($appSourceText -match "IRBandClassForShader") -and ($appSourceText -match 'set_shader_input\("u_ir_band_index"') -and ($appSourceText -match 'set_shader_input\("u_ir_band_class"')) $appSource)) | Out-Null
$checks.Add((Add-Check "u_wave_band retained only as deprecated compatibility" (($appSourceText -match "u_wave_band.*deprecated compatibility") -and ($appSourceText -notmatch "u_wave_band\s*(==|<=|>=|<|>)")) $appSource)) | Out-Null
$checks.Add((Add-Check "shader has Stage5 debug path" (($appSourceText -match "u_stage5_radiance_debug_en") -and ($appSourceText -match "u_stage5_radiance_debug_en == 1") -and ($appSourceText -match "u_stage5_debug_view_mode") -and ($appSourceText -match "u_stage5_use_base_texture_modulation") -and ($appSourceText -match "u_stage5_body_gray") -and ($appSourceText -match "u_stage5_hotspot_gray") -and ($appSourceText -match "u_stage5_brightspot_gray") -and ($appSourceText -match "u_tau_up")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 debug view modes implemented" (($appSourceText -match "BodyOnly") -and ($appSourceText -match "HotspotOnly") -and ($appSourceText -match "BrightSpotOnly") -and ($appSourceText -match "Composite") -and ($appSourceText -match "u_stage5_debug_view_mode == 1") -and ($appSourceText -match "u_stage5_debug_view_mode == 2") -and ($appSourceText -match "u_stage5_debug_view_mode == 3")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5C calibrated composite path present" (($appSourceText -match "u_stage5_body_display_gray") -and ($appSourceText -match "u_stage5_reflected_display_gray") -and ($appSourceText -match "u_stage5_hotspot_display_gray") -and ($appSourceText -match "u_stage5_brightspot_display_gray") -and ($appSourceText -match "u_stage5_composite_min_gray") -and ($appSourceText -match "u_stage5_composite_max_gray") -and ($appSourceText -match "\[Stage5C VisualCalib\]")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5C legacy intensity fallback is display-only" (($appSourceText -match "legacyRearHotspotDisplay") -and ($appSourceText -match "legacyBrightspotDisplay") -and ($appSourceText -match "std::max\(hotspotGrayRawDisplay, legacyRearHotspotDisplay\)") -and ($appSourceText -match "std::max\(brightspotGrayRawDisplay, legacyBrightspotDisplay\)") -and ($appSourceText -match "stage5DisplayFallbackApplied")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 debug preserves local Stage4 masks" (($appSourceText -match "rear_hotspot_debug") -and ($appSourceText -match "brightspot_debug") -and ($appSourceText -match "stage5_rear_mask") -and ($appSourceText -match "stage5_bright_mask") -and ($appSourceText -match "updateEngineRear\(platformName,\s*runtimeKey,\s*engineState") -and ($appSourceText -match "resolveBrightSpot")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 debug texture modulation defaults off" (($stage5DebugDisplayConfigText -match '"Stage5UseBaseTextureModulation"\s*:\s*false') -and ($appSourceText -match "u_stage5_use_base_texture_modulation == 1")) "$stage5DebugDisplayConfig; $appSource")) | Out-Null
$checks.Add((Add-Check "Stage5 render texture dump is smoke-only" (($appSourceText -match "Stage5OutputFrameDump") -and ($appSourceText -match "Stage5OutputFrameDumpPath") -and ($appSourceText -match "cv::imwrite") -and ($appSourceText -match "m_stage5OutputFrameDumpEnabled && m_enableStage5RadianceDebug")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 shader avoids MODTRAN path/sky/solar radiance tables" (($appSourceText -match "does not use MODTRAN solar irradiance tables") -and ($appSourceText -notmatch "u_stage5_.*path|u_stage5_.*sky|u_stage5_solar_irradiance|solar_irradiance")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 logs required metrics" (($appSourceText -match "\[Stage5 Radiance\]") -and ($appSourceText -match "targetKey=") -and ($appSourceText -match "debugViewMode=") -and ($appSourceText -match "toneMap=") -and ($appSourceText -match "materialTempK=") -and ($appSourceText -match "emissivity=") -and ($appSourceText -match "tauUp=") -and ($appSourceText -match "sunElevation=") -and ($appSourceText -match "sunAzimuth=") -and ($appSourceText -match "ndotl=") -and ($appSourceText -match "textureLuma=") -and ($appSourceText -match "reflectanceBand=") -and ($appSourceText -match "solarWeight=") -and ($appSourceText -match "bodyRadiance=") -and ($appSourceText -match "reflectedRadiance=") -and ($appSourceText -match "hotspotRadiance=") -and ($appSourceText -match "brightspotRadiance=") -and ($appSourceText -match "bodyGrayBeforeFloor=") -and ($appSourceText -match "bodyGrayAfterFloor=") -and ($appSourceText -match "reflectedGray=") -and ($appSourceText -match "hotspotGray=") -and ($appSourceText -match "brightspotGray=") -and ($appSourceText -match "finalGrayDebug=") -and ($appSourceText -match "debugFloorApplied=") -and ($appSourceText -match "stage5DisplayFallbackApplied=")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 body-gray warnings present" (($appSourceText -match "STAGE5_BODY_GRAY_ZERO_AFTER_MAPPING") -and ($appSourceText -match "CHECK_SHADER_BODY_GRAY_PATH_OR_FRAME_OUTPUT") -and ($appSourceText -match "STAGE5_TARGET_BODY_RADIANCE_ZERO")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5 tone mapping only enters debug path" (($appSourceText -match "if \(!m_enableStage5RadianceDebug\)") -and ($appSourceText -match "stage5Input\.debugConfig = m_stage5DebugConfigs\[stage5BandIndex\]") -and ($radianceSourceText -match "applyToneMap")) "$appSource; $radianceSource")) | Out-Null
$checks.Add((Add-Check "Stage5 body radiance uses engine-off base" ($appSourceText -match "EvaluateNodeRadiance\(MaterialNameForPlatform\(targetPlat\.type\), targetPlat\.nodePath, false, false, false, false, 0\.0") $appSource)) | Out-Null
$checks.Add((Add-Check "Stage5B reflected radiance path present" (($radianceText -match "reflectedRadiance") -and ($radianceText -match "reflectedGray") -and ($appSourceText -match "u_stage5_reflected_gray") -and ($appSourceText -match "u_stage5_sun_dir_local") -and ($appSourceText -match "Stage5SunDirectionLocal") -and ($appSourceText -match "STAGE5_REFLECTED_GRAY_ZERO")) "$radianceHeader; $radianceSource; $appSource")) | Out-Null

$forbiddenBandFields = "path_radiance_band|sky_radiance_band|path_scattering_radiance_band|solar_irradiance_band"
$forbiddenRuntimeFields = "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance"
$checks.Add((Add-Check "Stage5 V2 does not read path/sky/solar radiance" ($radianceText -notmatch $forbiddenRuntimeFields) "$radianceHeader; $radianceSource")) | Out-Null
$checks.Add((Add-Check "runtime still does not hook MODTRAN radiance band fields" (($appSourceText -notmatch $forbiddenBandFields) -and ($radianceText -notmatch $forbiddenBandFields)) "$appSource; $radianceHeader; $radianceSource")) | Out-Null
$checks.Add((Add-Check "UseModtranTauForAtmosphere default remains off" (($stage3AtmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and ($appSourceText -match 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false')) "$stage3AtmosphereSource; $appSource")) | Out-Null
$checks.Add((Add-Check "no whole-target hotspot added" (($appSourceText -notmatch "wholeTargetHotspot|whole-target hotspot|whole target hotspot") -and ($radianceText -notmatch "wholeTargetHotspot|whole-target hotspot|whole target hotspot")) "$appSource; $radianceHeader; $radianceSource")) | Out-Null
$checks.Add((Add-Check "u_brightspot_temp remains intensity not Kelvin" (($appSourceText -match "u_brightspot_temp.*intensity") -and ($appSourceText -match "不是Kelvin|Kelvin")) $appSource)) | Out-Null
$checks.Add((Add-Check "engineState and strikeFlag remain separated" (($appSourceText -match "updateEngineRear\(platformName,\s*runtimeKey,\s*engineState") -and ($appSourceText -match "Stage4WeaponAppliesToTarget") -and ($appSourceText -notmatch 'strikeFlag[^\r\n]*u_hotspot_rear_en|u_hotspot_rear_en[^\r\n]*strikeFlag') -and ($appSourceText -notmatch 'engineState[^\r\n]*u_brightspot_en|u_brightspot_en[^\r\n]*engineState')) $appSource)) | Out-Null
$stage5SensorPostTerms = "\bAGC\b|\bauto gain\b|\bnoise\b|\bMTF\b"
$checks.Add((Add-Check "no AGC/noise/MTF in Stage5A code" (($appSourceText -notmatch "Stage5[^\r\n]*($stage5SensorPostTerms)|($stage5SensorPostTerms)[^\r\n]*Stage5") -and ($radianceText -notmatch $stage5SensorPostTerms)) "$appSource; $radianceHeader; $radianceSource")) | Out-Null
$checks.Add((Add-Check "stage4 check updated for Stage5 separation" (Test-TextContains $stage4Check "Stage4 model remains separate from Stage5 radiance") $stage4Check)) | Out-Null
$checks.Add((Add-Check "stage5 smoke script exists" (Test-Path -LiteralPath $stage5Smoke -PathType Leaf) $stage5Smoke)) | Out-Null
$checks.Add((Add-Check "stage5 body visibility smoke script exists" (Test-Path -LiteralPath $stage5BodySmoke -PathType Leaf) $stage5BodySmoke)) | Out-Null
$checks.Add((Add-Check "stage5 output visibility smoke retained as optional" ((Test-Path -LiteralPath $stage5OutputSmoke -PathType Leaf) -and ($stage5OutputSmokeText -match "Stage5OutputFrameDump") -and ($stage5OutputSmokeText -match "render_texture_dump")) $stage5OutputSmoke)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 minimum radiance check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage5 minimum radiance check passed." -ForegroundColor Green
}
