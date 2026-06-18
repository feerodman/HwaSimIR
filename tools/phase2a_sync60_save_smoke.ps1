param(
    [int]$Seconds = 30,
    [string]$UseModtranPathRuntime = "false",
    [string]$ModtranPathRuntimeMode = "Off",
    [double]$ModtranPathScale = 1.0,
    [double]$ModtranPathOffset = 0.0,
    [double]$ModtranPathClampMin = 0.0,
    [double]$ModtranPathClampMax = 10.0,
    [double]$ModtranPathBlend = 1.0,
    [string]$EnableModtranRadianceDebug = "false",
    [string]$CompareLegacy = "false",
    [string]$EnableAeroThermalModel = "true",
    [string]$ApplyAeroToRadiance = "false",
    [string]$AeroDebugLog = "false",
    [string]$Stage5LogComponents = "false",
    [int]$Stage5ComponentLogEveryFrames = 120
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logRoot = Join-Path $root "logs\phase2a-final-$stamp"
New-Item -ItemType Directory -Force -Path $logRoot | Out-Null

$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWork = Join-Path $root "HwaSim_IR\Bin"
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWork = Split-Path -Parent $videoExe
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWork = Join-Path $root "DataDrivenTestQT"
$runtimeIni = Join-Path $hwaWork "Config\HwaSimIRRuntime.ini"
$hwaNetwork = Join-Path $hwaWork "Config\NetworkConfig.ini"
$videoNetwork = Join-Path $videoWork "NetworkConfig.ini"
$stimNetwork = Join-Path (Split-Path -Parent $stimExe) "NetworkConfig.ini"
$mp4Root = Join-Path $videoWork "MP4"

foreach ($path in @($hwaExe, $videoExe, $stimExe, $runtimeIni)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required path missing: $path"
    }
}

function Set-IniValue {
    param([string]$Text, [string]$Key, [string]$Value)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=.*$"
    if ($Text -match $pattern) {
        return [regex]::Replace($Text, $pattern, "$Key=$Value")
    }
    return $Text + "`r`n$Key=$Value`r`n"
}

function Get-NumericValues {
    param([string]$Text, [string]$Tag, [string]$Field)
    $pattern = "(?m)^\[" + [regex]::Escape($Tag) + "\].*?\b" +
        [regex]::Escape($Field) + "=([-+]?[0-9]+(?:\.[0-9]+)?)"
    return @([regex]::Matches($Text, $pattern) | ForEach-Object {
        [double]$_.Groups[1].Value
    })
}

function Get-Average {
    param([double[]]$Values)
    $usable = @($Values | Where-Object { $_ -gt 0.0 })
    if ($usable.Count -eq 0) { return 0.0 }
    return ($usable | Measure-Object -Average).Average
}

function Get-Maximum {
    param([double[]]$Values)
    if ($Values.Count -eq 0) { return 0.0 }
    return ($Values | Measure-Object -Maximum).Maximum
}

function Stop-TestProcess {
    param($Process)
    if ($null -eq $Process) { return }
    $Process.Refresh()
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

$backupPaths = @($runtimeIni, $hwaNetwork, $videoNetwork, $stimNetwork)
$backups = @{}
foreach ($path in $backupPaths) {
    if (Test-Path -LiteralPath $path) {
        $backups[$path] = [IO.File]::ReadAllBytes($path)
    } else {
        $backups[$path] = $null
    }
}

$utf8 = New-Object Text.UTF8Encoding($false)
$oldQtForceStderr = $env:QT_FORCE_STDERR_LOGGING
$oldPathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
$oldPathUpperValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
$caseStart = Get-Date
$video = $null
$hwa = $null
$stim = $null

try {
    $pathValue = $oldPathValue
    if ([string]::IsNullOrEmpty($pathValue)) {
        $pathValue = $oldPathUpperValue
    }
    if (-not [string]::IsNullOrEmpty($pathValue)) {
        [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
        [Environment]::SetEnvironmentVariable("Path", $pathValue, "Process")
    }

    [IO.File]::WriteAllText(
        $hwaNetwork,
        "[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=8888`r`nremoteIp=127.0.0.1`r`nremotePort=9999`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=5555`r`n",
        $utf8)
    [IO.File]::WriteAllText(
        $videoNetwork,
        "[Network]`r`nip=127.0.0.1`r`nport=5555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n",
        $utf8)
    [IO.File]::WriteAllText(
        $stimNetwork,
        "[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=9999`r`nremoteIp=127.0.0.1`r`nremotePort=8888`r`n",
        $utf8)

    $runtimeText = [IO.File]::ReadAllText($runtimeIni)
    $runtimeText = Set-IniValue $runtimeText "Codec" "auto"
    $runtimeText = Set-IniValue $runtimeText "JpegQuality" "100"
    $runtimeText = Set-IniValue $runtimeText "JpegEncodeMode" "rgb"
    $runtimeText = Set-IniValue $runtimeText "EnableH264Experimental" "false"
    $runtimeText = Set-IniValue $runtimeText "JpegPerfABTest" "false"
    $runtimeText = Set-IniValue $runtimeText "LegacyEngineBodyHeating" "false"
    $runtimeText = Set-IniValue $runtimeText "EnableIRVerboseLog" "0"
    $runtimeText = Set-IniValue $runtimeText "DebugView" "Off"
    $runtimeText = Set-IniValue $runtimeText "LogComponents" $Stage5LogComponents
    $runtimeText = Set-IniValue $runtimeText "ComponentLogEveryFrames" ([string]$Stage5ComponentLogEveryFrames)
    $runtimeText = Set-IniValue $runtimeText "EnableModtranRadianceDebug" $EnableModtranRadianceDebug
    $runtimeText = Set-IniValue $runtimeText "UseModtranPathRuntime" $UseModtranPathRuntime
    $runtimeText = Set-IniValue $runtimeText "UseModtranSkyRuntime" "false"
    $runtimeText = Set-IniValue $runtimeText "UseModtranSolarRuntime" "false"
    $runtimeText = Set-IniValue $runtimeText "ModtranPathRuntimeBand" "MWIR"
    $runtimeText = Set-IniValue $runtimeText "ModtranPathRuntimeMode" $ModtranPathRuntimeMode
    $runtimeText = Set-IniValue $runtimeText "ModtranPathUnitMode" "Native"
    $runtimeText = Set-IniValue $runtimeText "ModtranPathScale" ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $ModtranPathScale))
    $runtimeText = Set-IniValue $runtimeText "ModtranPathOffset" ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $ModtranPathOffset))
    $runtimeText = Set-IniValue $runtimeText "ModtranPathClampMin" ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $ModtranPathClampMin))
    $runtimeText = Set-IniValue $runtimeText "ModtranPathClampMax" ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $ModtranPathClampMax))
    $runtimeText = Set-IniValue $runtimeText "ModtranPathBlend" ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $ModtranPathBlend))
    $runtimeText = Set-IniValue $runtimeText "ModtranPathABLog" "true"
    $runtimeText = Set-IniValue $runtimeText "CompareLegacy" $CompareLegacy
    $runtimeText = Set-IniValue $runtimeText "EnableAeroThermalModel" $EnableAeroThermalModel
    $runtimeText = Set-IniValue $runtimeText "ApplyAeroToRadiance" $ApplyAeroToRadiance
    $runtimeText = Set-IniValue $runtimeText "AeroDebugLog" $AeroDebugLog
    [IO.File]::WriteAllText($runtimeIni, $runtimeText, $utf8)

    $env:QT_FORCE_STDERR_LOGGING = "1"
    $video = Start-Process -FilePath $videoExe -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logRoot "video.out.log") `
        -RedirectStandardError (Join-Path $logRoot "video.err.log")
    Start-Sleep -Seconds 2
    $hwa = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logRoot "hwa.out.log") `
        -RedirectStandardError (Join-Path $logRoot "hwa.err.log")
    Start-Sleep -Seconds 5
    $stimArgs = @(
        "--phase1b-auto-seconds=$Seconds",
        "--phase1d-h264=0"
    )
    $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logRoot "stim.out.log") `
        -RedirectStandardError (Join-Path $logRoot "stim.err.log")
    if (-not $stim.WaitForExit(($Seconds + 30) * 1000)) {
        throw "Stimulus timeout"
    }
    Start-Sleep -Seconds ([math]::Max(10, [math]::Ceiling($Seconds / 3.0)))
}
finally {
    Stop-TestProcess $stim
    Stop-TestProcess $hwa
    Stop-TestProcess $video
    foreach ($path in $backups.Keys) {
        if ($null -eq $backups[$path]) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        } else {
            [IO.File]::WriteAllBytes($path, $backups[$path])
        }
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQtForceStderr
    if (-not [string]::IsNullOrEmpty($oldPathUpperValue)) {
        [Environment]::SetEnvironmentVariable("PATH", $oldPathUpperValue, "Process")
    }
    if (-not [string]::IsNullOrEmpty($oldPathValue)) {
        [Environment]::SetEnvironmentVariable("Path", $oldPathValue, "Process")
    }
}

$stimText = Get-Content -LiteralPath (Join-Path $logRoot "stim.err.log") -Raw
$hwaText = Get-Content -LiteralPath (Join-Path $logRoot "hwa.out.log") -Raw
$videoText = Get-Content -LiteralPath (Join-Path $logRoot "video.err.log") -Raw
$round = Get-ChildItem -LiteralPath $mp4Root -Directory |
    Where-Object { $_.LastWriteTime -ge $caseStart.AddSeconds(-2) } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

$annotations = 0
$targetAnnotations = 0
$mp4Path = ""
$mp4Frames = 0
if ($round) {
    $mp4Path = Join-Path $round.FullName "output.mp4"
    $annotationsPath = Join-Path $round.FullName "annotations.txt"
    $targetAnnotationsPath = Join-Path $round.FullName "target_annotations.txt"
    if (Test-Path -LiteralPath $annotationsPath) {
        $annotations = (Get-Content -LiteralPath $annotationsPath).Count
    }
    if (Test-Path -LiteralPath $targetAnnotationsPath) {
        $targetAnnotations = (Get-Content -LiteralPath $targetAnnotationsPath).Count
    }
    $ffprobe = Get-Command ffprobe -ErrorAction SilentlyContinue
    if ($ffprobe -and (Test-Path -LiteralPath $mp4Path)) {
        $ffText = & $ffprobe.Source -v error -select_streams v:0 -count_frames `
            -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 $mp4Path
        [void][int]::TryParse(($ffText | Select-Object -First 1), [ref]$mp4Frames)
    }
}

$summary = [pscustomobject]@{
    logDir = $logRoot
    roundDir = $(if ($round) { $round.FullName } else { "" })
    outputMp4 = $mp4Path
    sentFps = [math]::Round((Get-Average (Get-NumericValues $stimText "StimPerf" "sentFpsInstant")), 3)
    udpFps = [math]::Round((Get-Average (@(Get-NumericValues $hwaText "Perf" "udpFps" | Where-Object { $_ -ge 30 }))), 3)
    renderFps = [math]::Round((Get-Average (@(Get-NumericValues $hwaText "Perf" "renderFps" | Where-Object { $_ -ge 30 }))), 3)
    outputFps = [math]::Round((Get-Average (@(Get-NumericValues $hwaText "Perf" "outputFps" | Where-Object { $_ -ge 30 }))), 3)
    videoReceiveFps = [math]::Round((Get-Average (@(Get-NumericValues $videoText "VideoPerf" "receiveFps" | Where-Object { $_ -ge 30 }))), 3)
    videoDisplayFps = [math]::Round((Get-Average (@(Get-NumericValues $videoText "VideoPerf" "displayFps" | Where-Object { $_ -ge 30 }))), 3)
    latencyAvgMs = [math]::Round((Get-Average (Get-NumericValues $videoText "VideoPerf" "latencyAvgMs")), 3)
    jpegMsAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "TcpPerf" "jpegMs")), 3)
    readbackMsAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Stage6 Capture" "readbackMs")), 3)
    irUpdateMsAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "irUpdateMs")), 3)
    stage7SkyGroundMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage7SkyGroundMs")), 3)
    stage4HotspotMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage4HotspotMs")), 3)
    shaderInputApplyMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "shaderInputApplyMs")), 3)
    shaderInputSetCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "shaderInputSetCount")), 3)
    shaderInputSkipCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "shaderInputSkipCount")), 3)
    shaderInputCacheHitRateAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "shaderInputCacheHitRate")), 3)
    stage5RadianceComponentMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage5RadianceComponentMs")), 3)
    stage5AeroThermalMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage5AeroThermalMs")), 6)
    stage5ModtranLookupMs = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage5ModtranLookupMs")), 6)
    stage5ModtranCacheHitCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage5ModtranCacheHitCount")), 3)
    stage5ModtranCacheMissCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage5ModtranCacheMissCount")), 3)
    modtranPathRuntimeMode = $ModtranPathRuntimeMode
    useModtranPathRuntime = $UseModtranPathRuntime
    modtranPathScale = $ModtranPathScale
    modtranPathBlend = $ModtranPathBlend
    enableAeroThermalModel = $EnableAeroThermalModel
    applyAeroToRadiance = $ApplyAeroToRadiance
    aeroDebugLog = $AeroDebugLog
    stage5LogComponents = $Stage5LogComponents
    stage7FullUpdateCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage7FullUpdateCount")), 3)
    stage7PositionOnlyCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage7PositionOnlyCount")), 3)
    stage7SkipCountMax = [math]::Round((Get-Maximum (Get-NumericValues $hwaText "Perf" "stage7SkipCount")), 3)
    stage4UpdateCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage4UpdateCount")), 3)
    stage4SkipCountAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "Perf" "stage4SkipCount")), 3)
    sourceSeqLagMax = [int](Get-Maximum (Get-NumericValues $hwaText "Perf" "sourceSeqLag"))
    inputQueueDepthMax = [int](Get-Maximum (Get-NumericValues $hwaText "Perf" "inputQueueDepthMax"))
    recorderWriteMs = [math]::Round((Get-Average (Get-NumericValues $videoText "RecorderPerf" "writeMsAvg")), 3)
    recorderDroppedFrames = [int](Get-Maximum (Get-NumericValues $videoText "RecorderPerf" "droppedFrames"))
    sourceSeqContinuous = $(if ($videoText -match "sourceSeqContinuous=0") { 0 } else { 1 })
    sourceSeqContinuousWritten = $(if ($videoText -match "sourceSeqContinuousWritten=0") { 0 } else { 1 })
    inputQueueOverflow = [int](Get-Maximum (Get-NumericValues $hwaText "Perf" "inputQueueOverflowCount"))
    tcpOverwritten = $(if ($hwaText -match "overwritten=1") { 1 } else { 0 })
    writtenFrames = [int](Get-Maximum (Get-NumericValues $videoText "RecorderPerf" "sourceSeqWritten"))
    mp4Frames = $mp4Frames
    annotations = $annotations
    targetAnnotations = $targetAnnotations
}

$summaryPath = Join-Path $logRoot "phase2a_sync60_save_summary.json"
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
$summary | Format-List
Write-Output "summary=$summaryPath"
