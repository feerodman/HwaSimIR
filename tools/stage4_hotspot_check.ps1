param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$modelHeader = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRTemperatureModel.h"
$modelSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRTemperatureModel.cpp"
$hotspotConfig = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\IRHotspots\target_hotspots.json"
$appHeader = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.h"
$appSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp"
$commonData = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\Common\CommonData.h"
$cmakePath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj"
$filtersPath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj.filters"
$stage3AtmosphereSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IRSimulation.cpp"
$visualSmokeScript = Join-Path $rootPath "tools\stage4_hotspot_visual_smoke.ps1"

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

$modelHeaderText = Read-Text $modelHeader
$modelSourceText = Read-Text $modelSource
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$commonDataText = Read-Text $commonData
$stage3AtmosphereText = Read-Text $stage3AtmosphereSource
$configText = Read-Text $hotspotConfig
$stage4ModelText = "$modelHeaderText`n$modelSourceText"

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IRTemperatureModel header exists" (Test-Path -LiteralPath $modelHeader -PathType Leaf) $modelHeader)) | Out-Null
$checks.Add((Add-Check "IRTemperatureModel source exists" (Test-Path -LiteralPath $modelSource -PathType Leaf) $modelSource)) | Out-Null
$checks.Add((Add-Check "IRTemperatureModel exposes required enums" (($modelHeaderText -match "enum class IRHotspotKind") -and ($modelHeaderText -match "EngineRear") -and ($modelHeaderText -match "EnginePlume") -and ($modelHeaderText -match "CustomThermal") -and ($modelHeaderText -match "enum class IRHotspotShape") -and ($modelHeaderText -match "enum class IRBrightSpotPart")) $modelHeader)) | Out-Null
$checks.Add((Add-Check "IRHotspotState required fields" (($modelHeaderText -match "targetTempK") -and ($modelHeaderText -match "currentTempK") -and ($modelHeaderText -match "ambientTempK") -and ($modelHeaderText -match "heatTauSec") -and ($modelHeaderText -match "coolTauSec") -and ($modelHeaderText -match "localPos") -and ($modelHeaderText -match "localDir") -and ($modelHeaderText -match "intensity")) $modelHeader)) | Out-Null
$checks.Add((Add-Check "IRBrightSpotState required fields" (($modelHeaderText -match "IRBrightSpotState") -and ($modelHeaderText -match "part") -and ($modelHeaderText -match "radius") -and ($modelHeaderText -match "intensity")) $modelHeader)) | Out-Null
$checks.Add((Add-Check "temperature tau update formula" (($modelSourceText -match "1\.0f\s*-\s*std::exp\(-safeDt\s*/\s*tauSec\)") -and ($modelSourceText -match "currentTempK\s*\+=")) $modelSource)) | Out-Null

$checks.Add((Add-Check "IRTemperatureModel in CMake" (Test-TextContains $cmakePath "IR/IRTemperatureModel\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IRTemperatureModel in VS project" ((Test-TextContains $vcxprojPath "IR\\IRTemperatureModel\.cpp") -and (Test-TextContains $vcxprojPath "IR\\IRTemperatureModel\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IRTemperatureModel in VS filters" ((Test-TextContains $filtersPath "IR\\IRTemperatureModel\.cpp") -and (Test-TextContains $filtersPath "IR\\IRTemperatureModel\.h")) $filtersPath)) | Out-Null
$checks.Add((Add-Check "HwaSimIR owns IRTemperatureModel" (($appHeaderText -match "IRTemperatureModel m_irTemperatureModel") -and ($appSourceText -match "loadFromFileCandidates")) "$appHeader; $appSource")) | Out-Null

$checks.Add((Add-Check "target_hotspots.json exists" (Test-Path -LiteralPath $hotspotConfig -PathType Leaf) $hotspotConfig)) | Out-Null
$checks.Add((Add-Check "target_hotspots platforms configured" (($configText -match '"F35"') -and ($configText -match '"AIM120"') -and ($configText -match '"AIM9X"') -and ($configText -match '"engineRear"') -and ($configText -match '"brightspots"')) $hotspotConfig)) | Out-Null

$checks.Add((Add-Check "WeaponState strike fields exist" (($commonDataText -match "struct WeaponState") -and ($commonDataText -match "bool\s+strikeFlag") -and ($commonDataText -match "int\s+strikePart")) $commonData)) | Out-Null

$checks.Add((Add-Check "u_hotspot_rear_en default off" (($appSourceText -match 'set_shader_input\("u_hotspot_rear_en",\s*LVecBase2i\(0,\s*0\)\)') -and ($appSourceText -notmatch 'set_shader_input\("u_hotspot_rear_en",\s*LVecBase2i\(1,\s*0\)\)')) $appSource)) | Out-Null
$checks.Add((Add-Check "rear hotspot driven by engineState model" (($appSourceText -match "updateEngineRear\(platformName,\s*runtimeKey,\s*engineState") -and ($appSourceText -match 'set_shader_input\("u_hotspot_rear_en",\s*LVecBase2i\(rearEnabledForShader')) $appSource)) | Out-Null
$checks.Add((Add-Check "strikeFlag not directly controls rear hotspot uniform" ($appSourceText -notmatch 'strikeFlag[^\r\n]*u_hotspot_rear_en|u_hotspot_rear_en[^\r\n]*strikeFlag') $appSource)) | Out-Null
$checks.Add((Add-Check "engineState not directly controls brightspot uniform" ($appSourceText -notmatch 'engineState[^\r\n]*u_brightspot_en|u_brightspot_en[^\r\n]*engineState') $appSource)) | Out-Null
$checks.Add((Add-Check "brightspot controlled by WeaponState" (($appSourceText -match "Stage4WeaponAppliesToTarget") -and ($appSourceText -match "resolveBrightSpot") -and ($appSourceText -match "strikePart")) $appSource)) | Out-Null
$checks.Add((Add-Check "u_brightspot_temp documented as legacy intensity" (($appSourceText -match "u_brightspot_temp.*intensity") -and ($appSourceText -match "Kelvin")) $appSource)) | Out-Null

$checks.Add((Add-Check "Stage4 visual debug defaults off" (($appSourceText -match 'ReadProcessEnvFlag\("EnableStage4HotspotVisualDebug", false\)') -and ($appSourceText -match 'ReadProcessEnvFlag\("ForceStage4BrightSpotVisible", false\)') -and ($appSourceText -match 'ReadProcessEnvFlag\("ForceStage4RearHotspotVisible", false\)')) $appSource)) | Out-Null
$checks.Add((Add-Check "shader declares Stage4 visual debug uniform" (($appSourceText -match "uniform int u_stage4_visual_debug") -and ($appSourceText -match "stage4_debug_mask") -and ($appSourceText -match "u_stage4_visual_debug == 1")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage4 Input log present" ($appSourceText -match "\[Stage4 Input\]") $appSource)) | Out-Null
$checks.Add((Add-Check "Stage4 Uniform log present" (($appSourceText -match "\[Stage4 Uniform\]") -and ($appSourceText -match "STAGE4_SHADER_NOT_BOUND") -and ($appSourceText -match "STAGE4_TEXTURE_MISSING")) $appSource)) | Out-Null
$checks.Add((Add-Check "Stage4 VisualDebug log present" ($appSourceText -match "\[Stage4 VisualDebug\]") $appSource)) | Out-Null
$checks.Add((Add-Check "Target mapping uses targetPlatID key" (($appSourceText -match "TargetKeyMatches") -and ($appSourceText -match "targetState\.targetPlatID") -and ($appSourceText -match "WeaponTargetKeyMatches") -and ($appSourceText -match "weaponState\.targetPlatID")) $appSource)) | Out-Null
$checks.Add((Add-Check "targetNumValid gates render visibility" (($appSourceText -match "visibleTargetNum") -and ($appSourceText -match "visibleByTargetNum") -and ($appSourceText -match "renderVisible")) $appSource)) | Out-Null
$checks.Add((Add-Check "visual smoke script exists" (Test-Path -LiteralPath $visualSmokeScript -PathType Leaf) $visualSmokeScript)) | Out-Null

$checks.Add((Add-Check "Stage4 logs ThermalHotspot separately" ($appSourceText -match "\[Stage4 ThermalHotspot\]") $appSource)) | Out-Null
$checks.Add((Add-Check "Stage4 logs BrightSpot separately" ($appSourceText -match "\[Stage4 BrightSpot\]") $appSource)) | Out-Null

$checks.Add((Add-Check "no damage model in Stage4 model/config" ($stage4ModelText -notmatch "damage|damaged|毁伤|damageFlag") "$modelHeader; $modelSource")) | Out-Null
$checks.Add((Add-Check "Stage4 does not hook path/sky/solar radiance fields" ($stage4ModelText -notmatch "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance") "$modelHeader; $modelSource")) | Out-Null
$checks.Add((Add-Check "Stage4 model remains separate from Stage5 radiance" ($stage4ModelText -notmatch "Stage5|L_surface|L_aperture|solar_irradiance|bodyRadiance|finalGrayDebug") "$modelHeader; $modelSource")) | Out-Null
$checks.Add((Add-Check "UseModtranTauForAtmosphere default remains off" (($stage3AtmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and ($appSourceText -match 'ReadProcessEnvFlag\("UseModtranTauForAtmosphere", false\)')) "$stage3AtmosphereSource; $appSource")) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage4 hotspot/brightspot check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage4 hotspot/brightspot check passed." -ForegroundColor Green
}
