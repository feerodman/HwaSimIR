param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$weatherHeader = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRWeatherEffects.h"
$weatherSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\IR\IRWeatherEffects.cpp"
$appHeader = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.h"
$appSource = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\HwaSimIR.cpp"
$runtimeIni = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\HwaSimIRRuntime.ini"
$weatherProfiles = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Weather\weather_profiles.json"
$weatherTextures = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Weather\weather_textures.json"
$texturesDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Weather\Textures"
$cmakePath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj"
$filtersPath = Join-Path $rootPath "ConsoleApplication1_LLA\ConsoleApplication1\ConsoleApplication1.vcxproj.filters"
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

$weatherHeaderText = Read-Text $weatherHeader
$weatherSourceText = Read-Text $weatherSource
$weatherText = "$weatherHeaderText`n$weatherSourceText"
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$runtimeIniText = Read-Text $runtimeIni
$weatherProfilesText = Read-Text $weatherProfiles
$weatherTexturesText = Read-Text $weatherTextures
$cmakeText = Read-Text $cmakePath
$vcxprojText = Read-Text $vcxprojPath
$filtersText = Read-Text $filtersPath
$displayCheckText = Read-Text $displayCheck

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IRWeatherEffects header exists" (Test-Path -LiteralPath $weatherHeader -PathType Leaf) $weatherHeader)) | Out-Null
$checks.Add((Add-Check "IRWeatherEffects source exists" (Test-Path -LiteralPath $weatherSource -PathType Leaf) $weatherSource)) | Out-Null
$checks.Add((Add-Check "weather_profiles.json exists" (Test-Path -LiteralPath $weatherProfiles -PathType Leaf) $weatherProfiles)) | Out-Null
$checks.Add((Add-Check "weather_textures.json exists" (Test-Path -LiteralPath $weatherTextures -PathType Leaf) $weatherTextures)) | Out-Null
$checks.Add((Add-Check "weather textures directory exists" (Test-Path -LiteralPath $texturesDir -PathType Container) $texturesDir)) | Out-Null
$checks.Add((Add-Check "IRWeatherEffects in CMake" ($cmakeText -match "IR/IRWeatherEffects\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IRWeatherEffects in VS project" (($vcxprojText -match "IR\\IRWeatherEffects\.cpp") -and ($vcxprojText -match "IR\\IRWeatherEffects\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IRWeatherEffects in VS filters" (($filtersText -match "IR\\IRWeatherEffects\.cpp") -and ($filtersText -match "IR\\IRWeatherEffects\.h")) $filtersPath)) | Out-Null

$runtimeOk = ($runtimeIniText -match "\[Stage7Weather\]") -and
    ($runtimeIniText -match "EnableWeatherEffects=1") -and
    ($runtimeIniText -match "WeatherProfilePath=Config/Weather/weather_profiles\.json") -and
    ($runtimeIniText -match "WeatherTextureConfig=Config/Weather/weather_textures\.json") -and
    ($runtimeIniText -match "EnableCloudLayer=0") -and
    ($runtimeIniText -match "EnableFog=1") -and
    ($runtimeIniText -match "EnablePrecipitation=0") -and
    ($runtimeIniText -match "Stage7PrecipitationMode=ScreenOverlay") -and
    ($runtimeIniText -match "CloudLayerMaxCards=0") -and
    ($runtimeIniText -match "PrecipitationMaxParticles=0") -and
    ($runtimeIniText -match "UseWeatherUdpInput=1")
$checks.Add((Add-Check "RuntimeConfig has Stage7Weather section" $runtimeOk $runtimeIni)) | Out-Null

$mappingOk = $true
foreach ($pair in @('"0": "Clear"', '"1": "Cloudy"', '"2": "Rain"', '"3": "Snow"', '"4": "Fog"', '"5": "Overcast"')) {
    if ($weatherProfilesText -notmatch [Regex]::Escape($pair)) {
        $mappingOk = $false
    }
}
$checks.Add((Add-Check "envSky 0..5 mapping exists" $mappingOk $weatherProfiles)) | Out-Null

$profileFieldsOk = ($weatherProfilesText -match "skyGrayScale_VIS") -and
    ($weatherProfilesText -match "groundGrayScale_LWIR") -and
    ($weatherProfilesText -match "cloudEnable") -and
    ($weatherProfilesText -match "cloudCoverage") -and
    ($weatherProfilesText -match "cloudOpacity") -and
    ($weatherProfilesText -match "cloudTemperatureK") -and
    ($weatherProfilesText -match "fogEnable") -and
    ($weatherProfilesText -match "fogDensity") -and
    ($weatherProfilesText -match "precipitationType") -and
    ($weatherProfilesText -match "sunDirectScale") -and
    ($weatherProfilesText -match "skyDiffuseScale") -and
    ($weatherProfilesText -match "targetContrastScale")
$checks.Add((Add-Check "weather profile fields exist" $profileFieldsOk $weatherProfiles)) | Out-Null

$textureIndexOk = ($weatherTexturesText -match "cloud_cumulus\.png") -and
    ($weatherTexturesText -match "cloud_scattered\.png") -and
    ($weatherTexturesText -match "cloud_overcast\.png") -and
    ($weatherTexturesText -match "cloud_storm\.png") -and
    ($weatherTexturesText -match "rain_shaft\.png") -and
    ($weatherTexturesText -match "snow\.rgba") -and
    ($weatherTexturesText -match "moon\.png") -and
    ($weatherTexturesText -match "sun\.png")
$checks.Add((Add-Check "weather texture index supports required resources" $textureIndexOk $weatherTextures)) | Out-Null

$resourceFilesOk = @(
    "cloud_cumulus.png",
    "cloud_scattered.png",
    "cloud_overcast.png",
    "cloud_storm.png",
    "rain_shaft.png",
    "snow.rgba",
    "moon.png",
    "sun.png"
) | ForEach-Object { Test-Path -LiteralPath (Join-Path $texturesDir $_) -PathType Leaf }
$checks.Add((Add-Check "required weather texture resources copied" (-not ($resourceFilesOk -contains $false)) $texturesDir)) | Out-Null

$moduleOutputsOk = ($weatherHeaderText -match "IRStage7WeatherState") -and
    ($weatherHeaderText -match "weatherName") -and
    ($weatherHeaderText -match "cloudCoverage") -and
    ($weatherHeaderText -match "fogDensity") -and
    ($weatherHeaderText -match "precipitationType") -and
    ($weatherHeaderText -match "sunDirectScale") -and
    ($weatherHeaderText -match "skyDiffuseScale") -and
    ($weatherHeaderText -match "targetContrastScale") -and
    ($weatherSourceText -match "UDP\+weather_profiles\+RuntimeConfig")
$checks.Add((Add-Check "IRWeatherEffects outputs Stage7 weather state" $moduleOutputsOk "$weatherHeader; $weatherSource")) | Out-Null

$profileParserOk = ($weatherSourceText -match 'FindJsonObject\(text,\s*"profiles",\s*profilesText\)') -and
    ($weatherSourceText -match 'FindJsonObject\(profilesText,\s*names\[i\],\s*objectText\)')
$checks.Add((Add-Check "weather profile parser reads named profiles inside profiles object" $profileParserOk $weatherSource)) | Out-Null

$logsOk = ($appSourceText -match "\[Stage7 Weather\]") -and
    ($appSourceText -match "\[Stage7 CloudLayer\]") -and
    ($appSourceText -match "\[Stage7 Fog\]") -and
    ($appSourceText -match "\[Stage7 Precipitation\]") -and
    ($appSourceText -match "weatherName=") -and
    ($appSourceText -match "source=")
$checks.Add((Add-Check "Stage7 weather logs exist" $logsOk $appSource)) | Out-Null

$rawSceneOk = ($appSourceText -match "Stage7PrecipitationMode") -and
    ($appSourceText -match "Stage7_Precipitation_Card") -and
    ($appSourceText -match "m_stage7PrecipitationMode == 2") -and
    ($appSourceText -match "u_stage7_final_precipitation_mode") -and
    ($appSourceText -match "Stage7RainOverlay") -and
    ($appSourceText -match "Stage7SnowOverlay") -and
    ($displayCheckText -match "windowSource=final_sensor") -and
    ($displayCheckText -match "tcpSource=final_sensor")
$checks.Add((Add-Check "precipitation uses screen overlay by default with optional Cards diagnostic" $rawSceneOk $appSource)) | Out-Null

$shaderOk = ($appSourceText -match "u_stage7_weather_type") -and
    ($appSourceText -match "u_stage7_cloud_coverage") -and
    ($appSourceText -match "u_stage7_fog_density") -and
    ($appSourceText -match "u_stage7_precipitation_type") -and
    ($appSourceText -match "u_stage7_final_precipitation_density") -and
    ($appSourceText -match "ApplyStage7WeatherDisplay") -and
    ($appSourceText -match "ApplyStage7FinalPrecipitationOverlay")
$checks.Add((Add-Check "Stage7 weather shader uniforms and branches exist" $shaderOk $appSource)) | Out-Null

$boundariesOk = ($weatherText -notmatch "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance") -and
    ($appSourceText -notmatch "Stage7C[^\r\n]*(AGC|MTF|blur|H264|UDP video)") -and
    ($weatherText -notmatch "AGC|MTF|H264|UDP video") -and
    ($appSourceText -notmatch "Stage5.*IRWeatherEffects|IRWeatherEffects.*Stage5")
$checks.Add((Add-Check "Stage7C keeps forbidden boundaries" $boundariesOk "$weatherHeader; $weatherSource; $appSource")) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage7 weather check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage7 weather check passed." -ForegroundColor Green
}
