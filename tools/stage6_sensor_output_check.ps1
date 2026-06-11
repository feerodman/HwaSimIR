param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$sensorHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRSensorModel.h"
$sensorSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRSensorModel.cpp"
$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$tcpSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\TcpCommThread.cpp"
$tcpLinuxSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\TcpCommThread_Linux.cpp"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$stage3AtmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$stage6Smoke = Join-Path $rootPath "tools\stage6_sensor_geometry_smoke.ps1"

function Read-Text {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return ""
    }
    $encoding = [System.Text.Encoding]::Default
    return $encoding.GetString([System.IO.File]::ReadAllBytes($Path))
}

function Remove-DisabledIf0 {
    param([string]$Text)
    return [System.Text.RegularExpressions.Regex]::Replace($Text, "(?s)#if\s+0.*?#endif[^\r\n]*", "")
}

function Remove-CppComments {
    param([string]$Text)
    $withoutBlocks = [System.Text.RegularExpressions.Regex]::Replace($Text, "(?s)/\*.*?\*/", "")
    return [System.Text.RegularExpressions.Regex]::Replace($withoutBlocks, "(?m)//.*$", "")
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

$sensorHeaderText = Read-Text $sensorHeader
$sensorSourceText = Read-Text $sensorSource
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$appActiveText = Remove-CppComments (Remove-DisabledIf0 $appSourceText)
$tcpActiveText = Remove-DisabledIf0 (Read-Text $tcpSource)
$tcpLinuxActiveText = Remove-DisabledIf0 (Read-Text $tcpLinuxSource)
$cmakeText = Read-Text $cmakePath
$vcxprojText = Read-Text $vcxprojPath
$filtersText = Read-Text $filtersPath
$stage3AtmosphereText = Read-Text $stage3AtmosphereSource
$stage6Text = "$sensorHeaderText`n$sensorSourceText"

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "IRSensorModel header exists" (Test-Path -LiteralPath $sensorHeader -PathType Leaf) $sensorHeader)) | Out-Null
$checks.Add((Add-Check "IRSensorModel source exists" (Test-Path -LiteralPath $sensorSource -PathType Leaf) $sensorSource)) | Out-Null
$checks.Add((Add-Check "IRSensorModel in CMake" ($cmakeText -match "IR/IRSensorModel\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IRSensorModel in VS project" (($vcxprojText -match "IR\\IRSensorModel\.cpp") -and ($vcxprojText -match "IR\\IRSensorModel\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IRSensorModel in VS filters" (($filtersText -match "IR\\IRSensorModel\.cpp") -and ($filtersText -match "IR\\IRSensorModel\.h")) $filtersPath)) | Out-Null

$usesAllProtocolFields = ($sensorSourceText -match "trackerSensorWidth") -and
    ($sensorSourceText -match "trackerSensorHeight") -and
    ($sensorSourceText -match "trackerSensorViewMin") -and
    ($sensorSourceText -match "trackerSensorViewMax") -and
    ($sensorSourceText -match "trackerSensorPixelAngle")
$checks.Add((Add-Check "sensor model uses trackerSensor fields" $usesAllProtocolFields $sensorSource)) | Out-Null

$fovFormulaOk = ($sensorSourceText -match "1\.0e-6") -and
    ($sensorSourceText -match "2\.0\s*\*\s*std::atan\([^;\r\n]*config\.width[^;\r\n]*std::tan\(config\.pixelAngleRad\s*/\s*2\.0\)") -and
    ($sensorSourceText -match "2\.0\s*\*\s*std::atan\([^;\r\n]*config\.height[^;\r\n]*std::tan\(config\.pixelAngleRad\s*/\s*2\.0\)") -and
    ($sensorSourceText -match "pixelAngle_urad_per_pixel")
$checks.Add((Add-Check "pixel angle is treated as urad per pixel" $fovFormulaOk $sensorSource)) | Out-Null

$rangeValidationOk = ($sensorSourceText -match "trackerSensorPixelAngle <= 0\.0") -and
    ($sensorSourceText -match "kMaxNormalPixelAngleUrad = 200\.0") -and
    ($sensorSourceText -match "trackerSensorViewMin <= 0") -and
    ($sensorSourceText -match "trackerSensorViewMax <= config\.viewMinM") -and
    ($sensorSourceText -match "kMinSensorDimension = 256") -and
    ($sensorSourceText -match "kMaxSensorDimension = 4096")
$checks.Add((Add-Check "sensor model validates dimension, clip, and urad ranges" $rangeValidationOk $sensorSource)) | Out-Null

$appAppliesConfig = ($appSourceText -match "BuildSensorDisplayConfig\(") -and
    ($appSourceText -match "m_sensorParam\.trackerSensorWidth") -and
    ($appSourceText -match "m_sensorParam\.trackerSensorHeight") -and
    ($appSourceText -match "m_sensorParam\.trackerSensorViewMin") -and
    ($appSourceText -match "m_sensorParam\.trackerSensorViewMax") -and
    ($appSourceText -match "m_sensorParam\.trackerSensorPixelAngle") -and
    ($appSourceText -match "ApplySensorOutputConfig\(sensorDisplayConfig")
$checks.Add((Add-Check "HwaSimIR applies sensor output config after init" $appAppliesConfig $appSource)) | Out-Null

$resizeOk = ($appSourceText -match "void HwaSimIR::resize_window") -and
    (($appSourceText -match "set_properties\(new_props\)") -or ($appSourceText -match "request_properties\(new_props\)")) -and
    ($appSourceText -match "setup_2d_texture\(new_width,\s*new_height") -and
    ($appSourceText -match "\[Stage6 Resize\]")
$checks.Add((Add-Check "resize_window synchronizes window and render texture" $resizeOk $appSource)) | Out-Null

$lensOk = ($appSourceText -match "set_fov\(config\.horizontalFovDeg,\s*config\.verticalFovDeg\)") -and
    ($appSourceText -match "set_near_far\(config\.nearClipM,\s*config\.farClipM\)") -and
    ($appSourceText -match "\[Stage6 SensorGeometry\]")
$checks.Add((Add-Check "camera lens uses Stage6 FOV and near/far" $lensOk $appSource)) | Out-Null

$captureLogActive = ($appActiveText -match "\[Stage6 Capture\]") -and
    ($appActiveText -match "frameWidth=") -and
    ($appActiveText -match "frameHeight=") -and
    ($appActiveText -match "tcpWidth=") -and
    ($appActiveText -match "tcpHeight=") -and
    ($appActiveText -match "renderTextureWidth=") -and
    ($appActiveText -match "renderTextureHeight=") -and
    ($appActiveText -match "channels=RGB8") -and
    ($appActiveText -match "std::cout\s*<<\s*captureLog\.str\(\)")
$captureOk = ($appSourceText -match "capture_task") -and
    ($appSourceText -match "get_x_size\(\)") -and
    ($appSourceText -match "get_y_size\(\)") -and
    ($appSourceText -match "updateFrame\([\s\S]*frameData[\s\S]*frameWidth[\s\S]*frameHeight[\s\S]*\)") -and
    $captureLogActive
$checks.Add((Add-Check "capture_task forwards render texture width/height" $captureOk $appSource)) | Out-Null
$checks.Add((Add-Check "[Stage6 Capture] log is active code" $captureLogActive $appSource)) | Out-Null

$resetAllowsReinit = ($appActiveText -match "m_sensorDisplayConfigReady = false") -and
    ($appActiveText -match 'ApplySensorOutputConfig\(sensorDisplayConfig,\s*"init-command"\)')
$checks.Add((Add-Check "reset allows next init to reapply sensor geometry" $resetAllowsReinit $appSource)) | Out-Null

$tcpNo800Resize = ($tcpActiveText -notmatch "cv::Size\(800,\s*800\)") -and
    ($tcpActiveText -notmatch "width\s*!=\s*800\s*\|\|\s*height\s*!=\s*800") -and
    ($tcpLinuxActiveText -notmatch "cv::Size\(800,\s*800\)") -and
    ($tcpLinuxActiveText -notmatch "width\s*!=\s*800\s*\|\|\s*height\s*!=\s*800") -and
    ($tcpActiveText -match "Stage6A: encode the current sensor output size") -and
    ($tcpLinuxActiveText -match "Stage6A: encode the current sensor output size")
$checks.Add((Add-Check "TCP/JPEG does not force 800x800" $tcpNo800Resize "$tcpSource; $tcpLinuxSource")) | Out-Null

$farClipWarningOk = ($appSourceText -match "STAGE6_TARGET_BEYOND_FAR_CLIP") -and
    ($appSourceText -match "targetRangeM > static_cast<float>\(m_sensorDisplayConfig\.farClipM\)")
$checks.Add((Add-Check "far clip target warning exists" $farClipWarningOk $appSource)) | Out-Null

$checks.Add((Add-Check "EnableStage5RadianceDebug defaults off" (($appSourceText -match 'getBool\("Stage5",\s*"EnableRadianceDebug",\s*"EnableStage5RadianceDebug",\s*false') -and ($appHeaderText -match "m_enableStage5RadianceDebug = false")) "$appHeader; $appSource")) | Out-Null
$checks.Add((Add-Check "UseModtranTauForAtmosphere default remains off" (($stage3AtmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and ($appSourceText -match 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false')) "$stage3AtmosphereSource; $appSource")) | Out-Null
$checks.Add((Add-Check "Stage6A does not add path/sky/solar radiance" ($stage6Text -notmatch "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance") "$sensorHeader; $sensorSource")) | Out-Null
$checks.Add((Add-Check "Stage6A does not add AGC/MTF/H264/UDP video" (($stage6Text -notmatch "\bAGC\b|\bMTF\b|H264|H\.264|UDP video") -and ($appSourceText -notmatch "Stage6[^\r\n]*(\bAGC\b|\bMTF\b|H264|H\.264|UDP video)")) "$sensorHeader; $sensorSource; $appSource")) | Out-Null
$checks.Add((Add-Check "stage6 sensor geometry smoke exists" (Test-Path -LiteralPath $stage6Smoke -PathType Leaf) $stage6Smoke)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage6 sensor output check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage6 sensor output check passed." -ForegroundColor Green
}
