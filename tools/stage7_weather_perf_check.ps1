param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$runtimeIni = Join-Path $rootPath "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
$displayCheck = Join-Path $rootPath "tools\stage6_sensor_display_check.ps1"

function Read-Text {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $encoding = [System.Text.Encoding]::Default
    return $encoding.GetString([System.IO.File]::ReadAllBytes($Path))
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$runtimeIniText = Read-Text $runtimeIni
$displayCheckText = Read-Text $displayCheck

$checks = New-Object System.Collections.Generic.List[object]

$defaultOk = ($runtimeIniText -match "\[Stage7Weather\]") -and
    ($runtimeIniText -match "EnableWeatherEffects=1") -and
    ($runtimeIniText -match "EnableCloudLayer=1") -and
    ($runtimeIniText -match "CloudRenderMode=Layered2_5D") -and
    ($runtimeIniText -match "CloudLayerCount=3") -and
    ($runtimeIniText -match "CloudUpdateHz=10") -and
    ($runtimeIniText -match "EnableFog=1") -and
    ($runtimeIniText -match "EnablePrecipitation=0") -and
    ($runtimeIniText -match "Stage7PrecipitationMode=ScreenOverlay") -and
    ($runtimeIniText -match "PrecipitationMaxParticles=0")
$checks.Add((Add-Check "Stage7Weather W1 defaults use three cached world slices" $defaultOk $runtimeIni)) | Out-Null

$textureCacheOk = ($appHeaderText -match "m_stage7CloudTexture") -and
    ($appHeaderText -match "m_stage7RainTexture") -and
    ($appHeaderText -match "m_stage7SnowTexture") -and
    ($appSourceText -match "RefreshStage7WeatherTextureCache") -and
    ($appSourceText -match "m_stage7WeatherTextureCacheKey")
$checks.Add((Add-Check "weather textures are cached by state" $textureCacheOk "$appHeader; $appSource")) | Out-Null

$updateMatch = [regex]::Match($appSourceText, "void\s+HwaSimIR::UpdateStage7WeatherNodes[\s\S]*?\r?\n}\r?\n\r?\nvoid\s+HwaSimIR::LogStage7Perf")
$updateBody = $updateMatch.Value
$updateLoopOk = $updateMatch.Success -and
    ($updateBody -notmatch "TexturePool::load_texture") -and
    ($updateBody -notmatch "FileExists\(") -and
    ($updateBody -notmatch "\.set_texture\(")
$checks.Add((Add-Check "UpdateStage7WeatherNodes does not load or bind textures per frame" $updateLoopOk $appSource)) | Out-Null

$perfLogOk = ($appSourceText -match "\[Stage7 Perf\]") -and
    ($appSourceText -match "weatherNodeCount=") -and
    ($appSourceText -match "cloudNodeCount=") -and
    ($appSourceText -match "precipitationNodeCount=") -and
    ($appSourceText -match "textureLoadCountThisFrame=") -and
    ($appSourceText -match "updateWeatherNodesMs=") -and
    ($appSourceText -match "totalWeatherMs=") -and
    ($appSourceText -match "STAGE7_TEXTURE_LOAD_IN_FRAME")
$checks.Add((Add-Check "Stage7 perf logs and texture-load warning exist" $perfLogOk $appSource)) | Out-Null

$nodeGuardOk = ($appSourceText -match "if \(!m_stage7WeatherEnabled \|\| m_renderRoot\.is_empty\(\)\)") -and
    ($appSourceText -match "std::max\(2, std::min\(3, m_stage7CloudLayerCount\)\)") -and
    ($appSourceText -match "m_renderRoot\.attach_new_node") -and
    ($appSourceText -match "cameraAttached=0") -and
    ($appSourceText -match "m_stage7PrecipitationEnabled && m_stage7PrecipitationMode == 2") -and
    ($appSourceText -match "m_stage7PrecipitationMaxParticles")
$checks.Add((Add-Check "W1 cloud nodes are fixed world slices and precipitation remains guarded" $nodeGuardOk $appSource)) | Out-Null

$overlayOk = ($appSourceText -match "Stage7PrecipitationMode") -and
    ($appSourceText -match "ScreenOverlay") -and
    ($appSourceText -match "u_stage7_final_precipitation_mode") -and
    ($appSourceText -match "u_stage7_final_sensor_fov_deg") -and
    ($appSourceText -match "Stage7RainOverlay") -and
    ($appSourceText -match "Stage7SnowOverlay") -and
    ($appSourceText -match "ApplyStage7FinalPrecipitationOverlay") -and
    ($appSourceText -match "\[Stage7 PrecipitationOverlay\]")
$checks.Add((Add-Check "screen-space final sensor precipitation overlay exists" $overlayOk $appSource)) | Out-Null

$skyCloudOk = ($appSourceText -match "Stage7_CloudWorldSlice_") -and
    ($appSourceText -match "u_cloud_world_uv_reciprocal") -and
    ($appSourceText -match "base_mask") -and
    ($appSourceText -match "detail_mask") -and
    ($appSourceText -match "tau_cloud = exp\(-extinction\)") -and
    ($appSourceText -match "Lout=tau\*Lbackground\+\(1-tau\)\*Lcloud")
$checks.Add((Add-Check "W1 uses world-space cloud slices and exponential IR transmittance" $skyCloudOk $appSource)) | Out-Null

$sameOutputOk = ($displayCheckText -match "windowSource=final_sensor") -and
    ($displayCheckText -match "tcpSource=final_sensor") -and
    ($displayCheckText -match "sameOutput=1") -and
    ($appSourceText -match "finalSensorTex=Stage6FinalSensorTex")
$checks.Add((Add-Check "Stage6 final sensor same-output route remains intact" $sameOutputOk "$displayCheck; $appSource")) | Out-Null

$forbiddenOk = ($appSourceText -notmatch "Stage7C\.1[^\r\n]*(path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance)") -and
    ($appSourceText -notmatch "Stage7C\.1[^\r\n]*(AGC|MTF|blur|H264|UDP video)") -and
    ($appSourceText -notmatch "Stage7_CloudLayer_Card") -and
    ($appSourceText -notmatch "m_cameraNode\.attach_new_node\(cloudMaker\.generate\(\)\)")
$checks.Add((Add-Check "Stage7C.1 keeps forbidden boundaries" $forbiddenOk $appSource)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage7 weather perf check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage7 weather perf check passed." -ForegroundColor Green
}
