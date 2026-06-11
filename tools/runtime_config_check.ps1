param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$runtimeHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRRuntimeConfig.h"
$runtimeSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRRuntimeConfig.cpp"
$runtimeIni = Join-Path $rootPath "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$irConfigSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRConfig.cpp"
$irTypesHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRTypes.h"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$sensorWaveDoc = Join-Path $rootPath "docs\sensorwave_config_usage.md"

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

$runtimeHeaderText = Read-Text $runtimeHeader
$runtimeSourceText = Read-Text $runtimeSource
$runtimeText = "$runtimeHeaderText`n$runtimeSourceText"
$runtimeIniText = Read-Text $runtimeIni
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$irConfigText = Read-Text $irConfigSource
$irTypesText = Read-Text $irTypesHeader
$cmakeText = Read-Text $cmakePath
$vcxprojText = Read-Text $vcxprojPath
$filtersText = Read-Text $filtersPath
$sensorWaveDocText = Read-Text $sensorWaveDoc

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IRRuntimeConfig header exists" (Test-Path -LiteralPath $runtimeHeader -PathType Leaf) $runtimeHeader)) | Out-Null
$checks.Add((Add-Check "IRRuntimeConfig source exists" (Test-Path -LiteralPath $runtimeSource -PathType Leaf) $runtimeSource)) | Out-Null
$checks.Add((Add-Check "HwaSimIRRuntime.ini exists" (Test-Path -LiteralPath $runtimeIni -PathType Leaf) $runtimeIni)) | Out-Null
$checks.Add((Add-Check "RuntimeConfig in CMake" ($cmakeText -match "IR/IRRuntimeConfig\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "RuntimeConfig in VS project" (($vcxprojText -match "IR\\IRRuntimeConfig\.cpp") -and ($vcxprojText -match "IR\\IRRuntimeConfig\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "RuntimeConfig in VS filters" (($filtersText -match "IR\\IRRuntimeConfig\.cpp") -and ($filtersText -match "IR\\IRRuntimeConfig\.h")) $filtersPath)) | Out-Null

$priorityOk = ($runtimeText -match "sourcePriority\(\).*env>ini>default") -and
    ($runtimeSourceText -match "readEnv") -and
    ($runtimeSourceText -match "m_seenEnvOverrides\.insert") -and
    ($runtimeSourceText -match "getIniString") -and
    ($runtimeSourceText -match "\*source = `"env`"") -and
    ($runtimeSourceText -match "\*source = `"ini`"") -and
    ($runtimeSourceText -match "\*source = `"default`"")
$checks.Add((Add-Check "RuntimeConfig implements env > ini > default priority" $priorityOk "$runtimeHeader; $runtimeSource")) | Out-Null

$typedAccessOk = ($runtimeHeaderText -match "getBool") -and
    ($runtimeHeaderText -match "getInt") -and
    ($runtimeHeaderText -match "getDouble") -and
    ($runtimeHeaderText -match "getString") -and
    ($runtimeHeaderText -match "section") -and
    ($runtimeHeaderText -match "key") -and
    ($runtimeSourceText -match "\[.*\]") -and
    ($runtimeSourceText -match "section\s*=\s*trim")
$checks.Add((Add-Check "RuntimeConfig supports bool/int/double/string and section.key" $typedAccessOk "$runtimeHeader; $runtimeSource")) | Out-Null

$runtimeLogOk = ($appSourceText -match "\[RuntimeConfig\]") -and
    ($appSourceText -match "loaded=") -and
    ($appSourceText -match "envOverrideCount=") -and
    ($appSourceText -match "iniValueCount=") -and
    ($appSourceText -match "sourcePriority=")
$checks.Add((Add-Check "HwaSimIR logs RuntimeConfig source summary" $runtimeLogOk $appSource)) | Out-Null

$iniSectionsOk = ($runtimeIniText -match "\[Stage3\]") -and
    ($runtimeIniText -match "\[Stage4\]") -and
    ($runtimeIniText -match "\[Stage5\]") -and
    ($runtimeIniText -match "\[Stage6Display\]") -and
    ($runtimeIniText -match "\[Stage7Background\]") -and
    ($runtimeIniText -match "\[Stage7Weather\]") -and
    ($runtimeIniText -match "\[Debug\]") -and
    ($runtimeIniText -match "NoiseOverrideEnable=1") -and
    ($runtimeIniText -match "UseReal3DBackground=1") -and
    ($runtimeIniText -match "EnableWeatherEffects=1") -and
    ($runtimeIniText -match "WeatherProfilePath=Config/Weather/weather_profiles\.json") -and
    ($runtimeIniText -match "WeatherTextureConfig=Config/Weather/weather_textures\.json") -and
    ($runtimeIniText -match "EnableCloudLayer=0") -and
    ($runtimeIniText -match "EnableFog=1") -and
    ($runtimeIniText -match "EnablePrecipitation=0") -and
    ($runtimeIniText -match "Stage7PrecipitationMode=ScreenOverlay")
$checks.Add((Add-Check "HwaSimIRRuntime.ini has required sections and defaults" $iniSectionsOk $runtimeIni)) | Out-Null

$migrationOk = ($appSourceText -match 'getBool\("Stage3",\s*"EnableModtranTauDebug",\s*"EnableModtranTauDebug",\s*false') -and
    ($appSourceText -match 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false') -and
    ($appSourceText -match 'getBool\("Stage4",\s*"EnableHotspotVisualDebug",\s*"EnableStage4HotspotVisualDebug",\s*false') -and
    ($appSourceText -match 'getBool\("Stage5",\s*"EnableRadianceDebug",\s*"EnableStage5RadianceDebug",\s*false') -and
    ($appSourceText -match 'getString\("Stage5",\s*"DebugViewMode",\s*"Stage5DebugViewMode"') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"WhiteHot",\s*"Stage6WhiteHot",\s*true') -and
    ($appSourceText -match 'getBool\("Stage7Background",\s*"EnableSkyHorizon",\s*"EnableStage7SkyHorizon",\s*true') -and
    ($appSourceText -match 'getBool\("Stage7Weather",\s*"EnableWeatherEffects",\s*"EnableStage7WeatherEffects",\s*true') -and
    ($appSourceText -match 'getString\("Stage7Weather",\s*"WeatherProfilePath",\s*"Stage7WeatherProfilePath"') -and
    ($appSourceText -match 'getString\("Stage7Weather",\s*"WeatherTextureConfig",\s*"Stage7WeatherTextureConfig"') -and
    ($appSourceText -match 'getString\("Stage7Weather",\s*"Stage7PrecipitationMode",\s*"Stage7PrecipitationMode"') -and
    ($appSourceText -match 'getBool\("Stage7Weather",\s*"UseWeatherUdpInput",\s*"Stage7UseWeatherUdpInput",\s*true') -and
    ($appSourceText -match 'getBool\("Debug",\s*"ExitOnStop",\s*"HwaSimIRExitOnStop",\s*false') -and
    ($appSourceText -notmatch "ReadProcessEnv")
$checks.Add((Add-Check "HwaSimIR runtime switches migrated from direct env helpers" $migrationOk $appSource)) | Out-Null

$stage7WeatherRuntimeOk = ($appSourceText -match "\[Stage7 WeatherConfig\]") -and
    ($appSourceText -match "m_stage7WeatherEffects\.loadProfilesFromCandidates") -and
    ($appSourceText -match "m_stage7WeatherEffects\.loadTextureConfigFromCandidates") -and
    ($appSourceText -match "BuildRuntimeConfigPathCandidates\(m_stage7WeatherProfilePath\)") -and
    ($appSourceText -match "BuildRuntimeConfigPathCandidates\(m_stage7WeatherTextureConfigPath\)") -and
    ($appSourceText -match "useWeatherUdpInput=")
$checks.Add((Add-Check "Stage7Weather runtime config is loaded through RuntimeConfig" $stage7WeatherRuntimeOk $appSource)) | Out-Null

$sensorWaveWhitelistOk = ($irConfigText -match "SpectralResponseRangeLow") -and
    ($irConfigText -match "SpectralResponseRangeHigh") -and
    ($irConfigText -match "Width") -and
    ($irConfigText -match "Height") -and
    ($irConfigText -match "ADCBitNumber") -and
    ($irConfigText -match "DisplayBits") -and
    ($irConfigText -match "NoiseEquivalentTemperatureDifference") -and
    ($irConfigText -match "DetectorPitch") -and
    ($irConfigText -match "FocalLength") -and
    ($irConfigText -match "LensFnumber") -and
    ($irConfigText -match "BlackHot") -and
    ($irTypesText -match "lensFNumber") -and
    ($irTypesText -match "blackHot") -and
    ($irConfigText -match "ignoredPresagisFields")
$checks.Add((Add-Check "SensorWave whitelist fields are parsed and tracked" $sensorWaveWhitelistOk "$irConfigSource; $irTypesHeader")) | Out-Null

$sensorWaveUsageLogOk = ($appSourceText -match "\[SensorWave Usage\]") -and
    ($appSourceText -match "usedFields=") -and
    ($appSourceText -match "fallbackFields=") -and
    ($appSourceText -match "ignoredPresagisFields=") -and
    ($appSourceText -match "priority=UDP_init>SensorWave>RuntimeConfig>default") -and
    ($appSourceText -match "pixelAngleFromFOVH")
$checks.Add((Add-Check "SensorWave usage and fallback priority are logged" $sensorWaveUsageLogOk $appSource)) | Out-Null

$noisePriorityOk = ($appSourceText -match "NoiseOverrideEnable") -and
    ($appSourceText -match "config\.noiseOverrideEnable") -and
    ($appSourceText -match "protocolNoisePresent") -and
    ($appSourceText -match "config\.noiseSource = `"protocol`"") -and
    ($appSourceText -match "noiseOverrideEnable=") -and
    ($appSourceText -match "noiseSource=")
$checks.Add((Add-Check "Stage6 noise override priority is explicit" $noisePriorityOk $appSource)) | Out-Null

$realtimeBoundaryOk = ($appSourceText -notmatch 'getBool\([^)]*"targetState"|getBool\([^)]*"weaponState"|getBool\([^)]*"viewValid"|getBool\([^)]*"engineState"|getBool\([^)]*"strikeFlag"|getBool\([^)]*"strikePart"|getBool\([^)]*"lookatEn"') -and
    ($appSourceText -notmatch "Stage6Fallback.*target|Stage6Fallback.*engine|Stage6Fallback.*strike")
$checks.Add((Add-Check "RuntimeConfig does not override realtime simulation fields" $realtimeBoundaryOk $appSource)) | Out-Null

$boundariesOk = ($runtimeText -notmatch "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance") -and
    ($appSourceText -notmatch "path_radiance_band|sky_radiance_band|solar_irradiance_band|pathRadianceBand|skyRadianceBand|solarIrradianceBand") -and
    ($appSourceText -notmatch "\bAGC\b|\bMTF\b|blur|H264|H\.264|UDP video") -and
    ($appSourceText -notmatch "wholeTargetHotspot|whole-target hotspot|whole target hotspot")
$checks.Add((Add-Check "Runtime config work does not add forbidden imaging or radiance features" $boundariesOk $appSource)) | Out-Null

$docOk = (Test-Path -LiteralPath $sensorWaveDoc -PathType Leaf) -and
    ($sensorWaveDocText -match "SensorWave") -and
    ($sensorWaveDocText -match "fallback") -and
    ($sensorWaveDocText -match "Presagis") -and
    ($sensorWaveDocText -match "default_\*\.json") -and
    ($sensorWaveDocText -match "Width") -and
    ($sensorWaveDocText -match "BlackHot")
$checks.Add((Add-Check "SensorWave usage Chinese doc exists" $docOk $sensorWaveDoc)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Runtime config check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Runtime config check passed." -ForegroundColor Green
}
