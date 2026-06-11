param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path

$postHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRSensorPostProcess.h"
$postSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRSensorPostProcess.cpp"
$appHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.h"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$tcpSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\TcpCommThread.cpp"
$tcpLinuxSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\TcpCommThread_Linux.cpp"
$stage3AtmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$displaySmoke = Join-Path $rootPath "tools\stage6_sensor_display_smoke.ps1"

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

$postHeaderText = Read-Text $postHeader
$postSourceText = Read-Text $postSource
$postText = "$postHeaderText`n$postSourceText"
$appHeaderText = Read-Text $appHeader
$appSourceText = Read-Text $appSource
$appCodeText = Remove-DisabledIf0 $appSourceText
$appActiveText = Remove-CppComments $appCodeText
$stage3AtmosphereText = Read-Text $stage3AtmosphereSource
$tcpActiveText = Remove-DisabledIf0 (Read-Text $tcpSource)
$tcpLinuxActiveText = Remove-DisabledIf0 (Read-Text $tcpLinuxSource)
$cmakeText = Read-Text $cmakePath
$vcxprojText = Read-Text $vcxprojPath
$filtersText = Read-Text $filtersPath
$displaySmokeText = Read-Text $displaySmoke

$checks = New-Object System.Collections.Generic.List[object]

$sourceConsistencyOk = ($appSourceText -match "SetupStage6FinalPipeline") -and
    ($appSourceText -match "\[Stage6 FinalPipeline\]") -and
    ($appSourceText -match "\[Stage6 ViewportDiag\]") -and
    ($appHeaderText -match "m_stage6FinalRegion") -and
    ($appHeaderText -match "m_stage6RawSceneBuffer") -and
    ($appSourceText -match "u_stage6_final_uv_scale") -and
    ($appSourceText -match "CreateStage7SkyDomeNode") -and
    ($appSourceText -match "CreateStage7LowerHemisphereShellNode") -and
    ($appSourceText -match "\[Stage7 3DSkyGround\]")
if (-not $sourceConsistencyOk) {
    Write-Host "STAGE6_STAGE7_PATCH_NOT_PRESENT_IN_CURRENT_SOURCE" -ForegroundColor Red
}
$checks.Add((Add-Check "current source contains Stage6/Stage7 consistency patch" $sourceConsistencyOk "$appHeader; $appSource")) | Out-Null

$buildStampOk = $appSourceText -match "\[BuildStamp\] stage6b4_stage7a2_source_active=1"
$checks.Add((Add-Check "startup BuildStamp identifies active Stage6B.4/Stage7A.2 source" $buildStampOk $appSource)) | Out-Null

$checks.Add((Add-Check "IRSensorPostProcess header exists" (Test-Path -LiteralPath $postHeader -PathType Leaf) $postHeader)) | Out-Null
$checks.Add((Add-Check "IRSensorPostProcess source exists" (Test-Path -LiteralPath $postSource -PathType Leaf) $postSource)) | Out-Null
$checks.Add((Add-Check "IRSensorPostProcess in CMake" ($cmakeText -match "IR/IRSensorPostProcess\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "IRSensorPostProcess in VS project" (($vcxprojText -match "IR\\IRSensorPostProcess\.cpp") -and ($vcxprojText -match "IR\\IRSensorPostProcess\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "IRSensorPostProcess in VS filters" (($filtersText -match "IR\\IRSensorPostProcess\.cpp") -and ($filtersText -match "IR\\IRSensorPostProcess\.h")) $filtersPath)) | Out-Null

$rgbToGrayOk = ($postSourceText -match "0\.299\s*\*\s*r") -and
    ($postSourceText -match "0\.587\s*\*\s*g") -and
    ($postSourceText -match "0\.114\s*\*\s*b") -and
    ($postSourceText -match "output\[offset \+ 0u\]\s*=\s*value") -and
    ($postSourceText -match "output\[offset \+ 1u\]\s*=\s*value") -and
    ($postSourceText -match "output\[offset \+ 2u\]\s*=\s*value")
$checks.Add((Add-Check "postprocess converts RGB8 to grayscale RGB8" $rgbToGrayOk $postSource)) | Out-Null

$manualDisplayOk = ($postText -match "displayGain") -and
    ($postText -match "displayOffset") -and
    ($postSourceText -match "gray\s*=\s*gray\s*\*\s*config\.displayGain\s*\+\s*config\.displayOffset") -and
    ($postSourceText -match "ClampGray")
$checks.Add((Add-Check "postprocess supports manual display gain and offset" $manualDisplayOk $postSource)) | Out-Null

$polarityOk = ($postText -match "whiteHot = true") -and
    ($postSourceText -match "!config\.whiteHot") -and
    ($postSourceText -match "255\.0\s*-\s*gray")
$checks.Add((Add-Check "postprocess supports white-hot and black-hot" $polarityOk $postSource)) | Out-Null

$noiseOk = ($postText -match "noiseEnable = false") -and
    ($postText -match "noiseSigmaGray = 0\.0") -and
    ($postSourceText -match "nextUniformNoise") -and
    ($postSourceText -match "config\.noiseEnable") -and
    ($postSourceText -match "config\.noiseSigmaGray > 0\.0")
$checks.Add((Add-Check "postprocess supports lightweight noise with default off" $noiseOk $postSource)) | Out-Null

$initDisplayOk = ($appSourceText -match 'ApplyStage6DisplayConfig\(m_sensorParam,\s*"init-command"\)') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"WhiteHot",\s*"Stage6WhiteHot",\s*true') -and
    ($appSourceText -match 'getDouble\("Stage6Display",\s*"DisplayGain",\s*"Stage6DisplayGain",\s*1\.0') -and
    ($appSourceText -match 'getDouble\("Stage6Display",\s*"DisplayOffset",\s*"Stage6DisplayOffset",\s*0\.0') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"NoiseEnable",\s*"Stage6NoiseEnable",\s*false') -and
    ($appSourceText -match 'getDouble\("Stage6Display",\s*"NoiseSigmaGray",\s*"Stage6NoiseSigmaGray",\s*0\.0') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"ApplyToWindow",\s*"Stage6DisplayApplyToWindow",\s*true') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"BackgroundDisplayEnable",\s*"Stage6BackgroundDisplayEnable",\s*true') -and
    ($appSourceText -match 'getBool\("Stage6Display",\s*"NoiseOverrideEnable",\s*"Stage6NoiseOverrideEnable",\s*true') -and
    ($appSourceText -match "sensor\.noiseEn") -and
    ($appSourceText -match "sensor\.trackerSensorNoise")
$checks.Add((Add-Check "HwaSimIR initializes Stage6 display config from RuntimeConfig or protocol" $initDisplayOk $appSource)) | Out-Null

$noisePrecedenceOk = ($appSourceText -match "config\.noiseOverrideEnable") -and
    ($appSourceText -match "if \(config\.noiseOverrideEnable\)") -and
    ($appSourceText -match "else if \(protocolNoisePresent\)") -and
    ($appSourceText -match 'config\.noiseSource = "protocol"') -and
    ($appSourceText -match "noiseOverrideEnable=") -and
    ($appSourceText -match "noiseSource=")
$checks.Add((Add-Check "Stage6 display noise override controls protocol noise priority" $noisePrecedenceOk $appSource)) | Out-Null

$finalPassOk = ($appSourceText -match "InitStage6FinalPostShader") -and
    ($appSourceText -match "SetupStage6FinalPipeline") -and
    ($appSourceText -match "Stage6RawSceneTex") -and
    ($appSourceText -match "Stage6FinalSensorTex") -and
    ($appSourceText -match "Stage6_FinalSensor_Card") -and
    ($appSourceText -match "Stage6FinalSensorRoot") -and
    ($appSourceText -match "Stage6FinalSensorCamera") -and
    ($appSourceText -match "m_stage6FinalRegion") -and
    ($appSourceText -match "u_stage6_final_white_hot") -and
    ($appSourceText -match "u_stage6_final_display_gain") -and
    ($appSourceText -match "u_stage6_final_display_offset") -and
    ($appSourceText -match "u_stage6_final_noise_enable") -and
    ($appSourceText -match "u_stage6_final_noise_sigma_norm") -and
    ($appSourceText -match "u_stage6_final_uv_scale") -and
    ($appSourceText -match "texcoord \* u_stage6_final_uv_scale") -and
    ($appSourceText -match "dot\(rawColor\.rgb,\s*vec3\(0\.299,\s*0\.587,\s*0\.114\)\)") -and
    ($appSourceText -match "make_texture_buffer") -and
    ($appSourceText -match "make_display_region\(0\.0f,\s*1\.0f,\s*0\.0f,\s*1\.0f\)") -and
    ($appSourceText -match "finalCardMaker\.set_frame\(-1\.0f,\s*1\.0f,\s*-1\.0f,\s*1\.0f\)") -and
    ($appSourceText -match "OrthographicLens") -and
    ($appSourceText -match "finalLens->set_film_size\(2\.0f,\s*2\.0f\)") -and
    ($appSourceText -match "sourceRegion->set_active\(false\)") -and
    ($appSourceText -match "m_stage6RawSceneRegion->set_clear_color_active\(true\)")
$checks.Add((Add-Check "Stage6 final postprocess pass exists" $finalPassOk $appSource)) | Out-Null

$finalPipelineLogOk = ($appSourceText -match "\[Stage6 FinalPipeline\]") -and
    ($appSourceText -match "rawSceneTex=Stage6RawSceneTex") -and
    ($appSourceText -match "finalSensorTex=Stage6FinalSensorTex") -and
    ($appSourceText -match "windowSource=final_sensor") -and
    ($appSourceText -match "tcpSource=final_sensor") -and
    ($appSourceText -match "windowRegion=fullscreen") -and
    ($appSourceText -match "sameOutput=1")
$checks.Add((Add-Check "[Stage6 FinalPipeline] log confirms shared output" $finalPipelineLogOk $appSource)) | Out-Null

$viewportDiagOk = ($appSourceText -match "\[Stage6 ViewportDiag\]") -and
    ($appSourceText -match "windowSize=") -and
    ($appSourceText -match "rawSceneTexSize=") -and
    ($appSourceText -match "rawSceneRequested=") -and
    ($appSourceText -match "finalUvScale=") -and
    ($appSourceText -match "finalRegionDimensions=") -and
    ($appSourceText -match "finalCardBounds=-1,1,-1,1") -and
    ($appSourceText -match "renderTexSize=") -and
    ($appSourceText -match "STAGE6_FINAL_NOT_FULLSCREEN") -and
    ($appSourceText -match "m_stage6FinalRegion->set_dimensions\(0\.0f,\s*1\.0f,\s*0\.0f,\s*1\.0f\)")
$checks.Add((Add-Check "[Stage6 ViewportDiag] validates fullscreen final pass" $viewportDiagOk $appSource)) | Out-Null

$objectShaderRawOk = ($appSourceText -notmatch "gl_FragColor\s*=\s*Stage6(Display|Background)DisplayColor") -and
    ($appSourceText -match "vec4\(bg_intensity,\s*bg_intensity,\s*bg_intensity") -and
    ($appSourceText -match "vec4\(cloud_intensity,\s*cloud_intensity,\s*cloud_intensity") -and
    ($appSourceText -match "vec4\(stage5_intensity,\s*stage5_intensity,\s*stage5_intensity") -and
    ($appSourceText -match "vec4\(final_intensity,\s*final_intensity,\s*final_intensity") -and
    ($appSourceText -match "const bool shaderDisplayEnabled = false")
$checks.Add((Add-Check "object/background/cloud shader outputs raw scene values" $objectShaderRawOk $appSource)) | Out-Null

$tcpUpdateCallOk = ($appCodeText -match "updateFrame\(frameData,\s*frameWidth,\s*frameHeight\)")
$noCaptureCpuPostOk = ($appCodeText -notmatch "m_irSensorPostProcess\.processRgb8") -and
    ($appCodeText -match "source=final_sensor") -and
    $tcpUpdateCallOk
$checks.Add((Add-Check "capture_task reads final sensor output without second CPU postprocess" $noCaptureCpuPostOk $appSource)) | Out-Null

$sizePreservedOk = ($postHeaderText -match "int width") -and
    ($postHeaderText -match "int height") -and
    ($postSourceText -match "pixelCount = static_cast<std::size_t>\(width\) \* static_cast<std::size_t>\(height\)") -and
    ($postSourceText -match "output\.resize\(pixelCount \* 3u\)") -and
    ($appActiveText -match "frameWidth") -and
    ($appActiveText -match "frameHeight")
$checks.Add((Add-Check "postprocess preserves frame width and height" $sizePreservedOk $postSource)) | Out-Null

$displayLogOk = ($appSourceText -match "\[Stage6 Display\]") -and
    ($appSourceText -match "whiteHot=") -and
    ($appSourceText -match "displayGain=") -and
    ($appSourceText -match "displayOffset=") -and
    ($appSourceText -match "noiseEnable=") -and
    ($appSourceText -match "noiseSigmaGray=") -and
    ($appSourceText -match "applyToWindow=") -and
    ($appSourceText -match "applyToCapture=") -and
    ($appSourceText -match "backgroundDisplay=") -and
    ($appSourceText -match "configSource=") -and
    ($appSourceText -match "effectiveWhiteHot=") -and
    ($appSourceText -match "effectiveNoiseEnable=") -and
    ($appSourceText -match "effectiveNoiseSigmaGray=") -and
    ($appSourceText -match "source=")
$checks.Add((Add-Check "[Stage6 Display] log includes minimal display fields" $displayLogOk $appSource)) | Out-Null

$tcpProtocolOk = ($tcpActiveText -match 'imencode\("\.jpg"') -and
    ($tcpLinuxActiveText -match 'imencode\("\.jpg"') -and
    ($tcpActiveText -match "header\[0\]") -and
    ($tcpActiveText -match "header\[3\]") -and
    ($tcpLinuxActiveText -match "header\[0\]") -and
    ($tcpLinuxActiveText -match "header\[3\]") -and
    ($tcpActiveText -match "send\(m_tcpSocket,\s*header,\s*4") -and
    ($tcpLinuxActiveText -match "send\(m_tcpSocket,\s*header,\s*4") -and
    ($tcpActiveText -notmatch "Stage6WhiteHot|Stage6DisplayGain|Stage6DisplayOffset|Stage6NoiseEnable|Stage6NoiseSigmaGray|Stage6DisplayApplyToWindow") -and
    ($tcpLinuxActiveText -notmatch "Stage6WhiteHot|Stage6DisplayGain|Stage6DisplayOffset|Stage6NoiseEnable|Stage6NoiseSigmaGray|Stage6DisplayApplyToWindow")
$checks.Add((Add-Check "TCP/JPEG protocol remains length-prefixed JPEG" $tcpProtocolOk "$tcpSource; $tcpLinuxSource")) | Out-Null

$checks.Add((Add-Check "EnableStage5RadianceDebug defaults off" (($appSourceText -match 'getBool\("Stage5",\s*"EnableRadianceDebug",\s*"EnableStage5RadianceDebug",\s*false') -and ($appHeaderText -match "m_enableStage5RadianceDebug = false")) "$appHeader; $appSource")) | Out-Null
$checks.Add((Add-Check "UseModtranTauForAtmosphere default remains off" (($stage3AtmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and ($appSourceText -match 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false')) "$stage3AtmosphereSource; $appSource")) | Out-Null
$checks.Add((Add-Check "Stage6B does not connect path/sky/solar radiance" ($postText -notmatch "path_radiance|sky_radiance|solar_irradiance|pathRadiance|skyRadiance|solarIrradiance") "$postHeader; $postSource")) | Out-Null
$checks.Add((Add-Check "Stage6B does not add full AGC/MTF/blur/H264/UDP video" (($postText -notmatch "\bAGC\b|\bMTF\b|blur|H264|H\.264|UDP video") -and ($appSourceText -notmatch "Stage6B[^\r\n]*(\bAGC\b|\bMTF\b|blur|H264|H\.264|UDP video)")) "$postHeader; $postSource; $appSource")) | Out-Null
$displaySmokeGracefulExitOk = (Test-Path -LiteralPath $displaySmoke -PathType Leaf) -and
    ($displaySmokeText -match "HwaSimIRExitOnStop") -and
    ($displaySmokeText -match "New-ControlStopPacket") -and
    ($displaySmokeText -match "Stop-HwaProcessGracefully")
$checks.Add((Add-Check "stage6 sensor display smoke exits HwaSimIR gracefully" $displaySmokeGracefulExitOk $displaySmoke)) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage6 sensor display check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage6 sensor display check passed." -ForegroundColor Green
}
