param(
    [int]$SecondsPerCase = 10,
    [string[]]$CaseFilter = @(),
    [switch]$SkipH264Fallback
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logRoot = Join-Path $root "logs\phase1d-codec-ab-$stamp"
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

$cases = @(
    [pscustomobject]@{ Label = "rgb_q100"; Mode = "rgb"; Quality = 100; H264 = 0 },
    [pscustomobject]@{ Label = "rgb_q90"; Mode = "rgb"; Quality = 90; H264 = 0 },
    [pscustomobject]@{ Label = "rgb_q80"; Mode = "rgb"; Quality = 80; H264 = 0 },
    [pscustomobject]@{ Label = "rgb_q70"; Mode = "rgb"; Quality = 70; H264 = 0 },
    [pscustomobject]@{ Label = "gray_q100"; Mode = "gray"; Quality = 100; H264 = 0 },
    [pscustomobject]@{ Label = "gray_q90"; Mode = "gray"; Quality = 90; H264 = 0 },
    [pscustomobject]@{ Label = "gray_q80"; Mode = "gray"; Quality = 80; H264 = 0 },
    [pscustomobject]@{ Label = "gray_q70"; Mode = "gray"; Quality = 70; H264 = 0 }
)
if (-not $SkipH264Fallback) {
    $cases += [pscustomobject]@{ Label = "h264_fallback"; Mode = "rgb"; Quality = 100; H264 = 1 }
}
if ($CaseFilter.Count -gt 0) {
    $cases = @($cases | Where-Object { $CaseFilter -contains $_.Label })
}
if ($cases.Count -eq 0) {
    throw "No codec A/B cases selected"
}

function Set-IniValue {
    param([string]$Text, [string]$Key, [string]$Value)
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=.*$"
    if ($Text -notmatch $pattern) {
        throw "Runtime key not found: $Key"
    }
    return [regex]::Replace($Text, $pattern, "$Key=$Value")
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
$results = New-Object System.Collections.Generic.List[object]

try {
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
    $env:QT_FORCE_STDERR_LOGGING = "1"

    foreach ($case in $cases) {
        $caseDir = Join-Path $logRoot $case.Label
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
        $runtimeText = [IO.File]::ReadAllText($runtimeIni)
        $runtimeText = Set-IniValue $runtimeText "Codec" "auto"
        $runtimeText = Set-IniValue $runtimeText "JpegQuality" ([string]$case.Quality)
        $runtimeText = Set-IniValue $runtimeText "JpegEncodeMode" $case.Mode
        $runtimeText = Set-IniValue $runtimeText "EnableH264Experimental" "false"
        $runtimeText = Set-IniValue $runtimeText "H264FallbackToJpeg" "true"
        $runtimeText = Set-IniValue $runtimeText "JpegPerfABTest" "true"
        [IO.File]::WriteAllText($runtimeIni, $runtimeText, $utf8)

        $caseStart = Get-Date
        $video = $null
        $hwa = $null
        $stim = $null
        try {
            $video = Start-Process -FilePath $videoExe -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "video.out.log") `
                -RedirectStandardError (Join-Path $caseDir "video.err.log")
            Start-Sleep -Seconds 2
            $hwa = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "hwa.out.log") `
                -RedirectStandardError (Join-Path $caseDir "hwa.err.log")
            Start-Sleep -Seconds 5
            $stimArgs = @(
                "--phase1b-auto-seconds=$SecondsPerCase",
                "--phase1d-h264=$($case.H264)"
            )
            $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork `
                -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "stim.out.log") `
                -RedirectStandardError (Join-Path $caseDir "stim.err.log")
            if (-not $stim.WaitForExit(($SecondsPerCase + 25) * 1000)) {
                throw "Stimulus timeout: $($case.Label)"
            }
            # Allow UDP backlog to drain so the stop command reaches HwaSim_IR,
            # VideoDisplay can flush the recorder, and MP4 writes its moov atom.
            $postStopWaitSeconds = [math]::Max(5, [math]::Ceiling($SecondsPerCase / 2.0))
            Start-Sleep -Seconds $postStopWaitSeconds
        }
        finally {
            Stop-TestProcess $stim
            Stop-TestProcess $hwa
            Stop-TestProcess $video
        }

        $stimText = Get-Content -LiteralPath (Join-Path $caseDir "stim.err.log") -Raw
        $hwaText = Get-Content -LiteralPath (Join-Path $caseDir "hwa.out.log") -Raw
        $videoText = Get-Content -LiteralPath (Join-Path $caseDir "video.err.log") -Raw
        $round = Get-ChildItem -LiteralPath $mp4Root -Directory |
            Where-Object { $_.LastWriteTime -ge $caseStart.AddSeconds(-2) } |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1

        $perfUdp = @(Get-NumericValues $hwaText "Perf" "udpFps" | Where-Object { $_ -ge 30 })
        $perfRender = @(Get-NumericValues $hwaText "Perf" "renderFps" | Where-Object { $_ -ge 30 })
        $perfOutput = @(Get-NumericValues $hwaText "Perf" "outputFps" | Where-Object { $_ -ge 30 })
        $videoReceive = @(Get-NumericValues $videoText "VideoPerf" "receiveFps" | Where-Object { $_ -ge 30 })
        $videoDisplay = @(Get-NumericValues $videoText "VideoPerf" "displayFps" | Where-Object { $_ -ge 30 })
        $recorderDropped = Get-Maximum (Get-NumericValues $videoText "RecorderPerf" "droppedFrames")
        $recorderWritten = Get-Maximum (Get-NumericValues $videoText "RecorderPerf" "sourceSeqWritten")
        $annotations = 0
        $targetAnnotations = 0
        $mp4Path = ""
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
        }

        $requestedCodec = if ($case.H264 -eq 1) { "h264" } else { "jpeg" }
        $activeCodec = if ($hwaText -match "activeCodec=([a-z0-9_]+)") { $Matches[1] } else { "unknown" }
        $fallbackReason = if ($hwaText -match "codecFallbackReason=([a-z0-9_]+)") { $Matches[1] } else { "none" }
        $decodedChannels = Get-Maximum (Get-NumericValues $videoText "VideoPerf" "decodedChannels")
        $visualStatus = if (
            (Test-Path -LiteralPath $mp4Path) -and
            $annotations -gt 0 -and
            $annotations -eq $targetAnnotations -and
            $annotations -eq [int]$recorderWritten -and
            $recorderDropped -eq 0 -and
            $videoText -notmatch "sourceSeqContinuous=0" -and
            $videoText -notmatch "sourceSeqContinuousWritten=0"
        ) { "pass" } else { "check" }

        $results.Add([pscustomobject]@{
            label = $case.Label
            codec = "jpeg"
            mode = $case.Mode
            quality = $case.Quality
            h264En = $case.H264
            requestedCodec = $requestedCodec
            activeCodec = $activeCodec
            codecFallbackReason = $fallbackReason
            sentFps = [math]::Round((Get-Average (Get-NumericValues $stimText "StimPerf" "sentFpsInstant")), 3)
            udpFps = [math]::Round((Get-Average $perfUdp), 3)
            renderFps = [math]::Round((Get-Average $perfRender), 3)
            outputFps = [math]::Round((Get-Average $perfOutput), 3)
            videoReceiveFps = [math]::Round((Get-Average $videoReceive), 3)
            videoDisplayFps = [math]::Round((Get-Average $videoDisplay), 3)
            latencyAvgMs = [math]::Round((Get-Average (Get-NumericValues $videoText "VideoPerf" "latencyAvgMs")), 3)
            encodeMsAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "TcpPerf" "jpegMs")), 3)
            jpegBytesAvg = [math]::Round((Get-Average (Get-NumericValues $hwaText "TcpPerf" "jpegBytes")), 0)
            decodeMsAvg = [math]::Round((Get-Average (Get-NumericValues $videoText "VideoPerf" "decodeMsAvg")), 3)
            recorderWriteMs = [math]::Round((Get-Average (Get-NumericValues $videoText "RecorderPerf" "writeMsAvg")), 3)
            recorderDroppedFrames = [int]$recorderDropped
            maxRecordingQueueDepth = [int](Get-Maximum (Get-NumericValues $videoText "RecorderPerf" "maxQueueDepth"))
            decodedChannels = [int]$decodedChannels
            sourceSeqContinuous = if ($videoText -match "sourceSeqContinuous=0") { 0 } else { 1 }
            sourceSeqContinuousWritten = if ($videoText -match "sourceSeqContinuousWritten=0") { 0 } else { 1 }
            inputQueueOverflow = [int](Get-Maximum (Get-NumericValues $hwaText "Perf" "inputQueueOverflowCount"))
            tcpOverwritten = if ($hwaText -match "overwritten=1") { 1 } else { 0 }
            writtenFrames = [int]$recorderWritten
            annotations = $annotations
            targetAnnotations = $targetAnnotations
            outputMp4 = $mp4Path
            visualStatus = $visualStatus
        }) | Out-Null
    }
}
finally {
    foreach ($path in $backups.Keys) {
        if ($null -eq $backups[$path]) {
            if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Force }
        } else {
            [IO.File]::WriteAllBytes($path, $backups[$path])
        }
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQtForceStderr
}

foreach ($mode in @("rgb", "gray")) {
    $baseline = $results | Where-Object { $_.mode -eq $mode -and $_.quality -eq 100 -and $_.h264En -eq 0 } |
        Select-Object -First 1
    if ($baseline) {
        foreach ($result in $results | Where-Object { $_.mode -eq $mode -and $_.h264En -eq 0 }) {
            $result | Add-Member -NotePropertyName deltaEncodeVsJpegQ100 -NotePropertyValue (
                [math]::Round($result.encodeMsAvg - $baseline.encodeMsAvg, 3))
            $result | Add-Member -NotePropertyName deltaBytesVsJpegQ100 -NotePropertyValue (
                [math]::Round($result.jpegBytesAvg - $baseline.jpegBytesAvg, 0))
            $result | Add-Member -NotePropertyName latencyDeltaVsQ100 -NotePropertyValue (
                [math]::Round($result.latencyAvgMs - $baseline.latencyAvgMs, 3))
        }
    }
}

$csvPath = Join-Path $logRoot "phase1d_codec_ab_summary.csv"
$jsonPath = Join-Path $logRoot "phase1d_codec_ab_summary.json"
$results | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $jsonPath -Encoding UTF8
$results | Format-Table label,mode,quality,h264En,activeCodec,encodeMsAvg,jpegBytesAvg,decodeMsAvg,latencyAvgMs,videoDisplayFps,recorderDroppedFrames,visualStatus -AutoSize
Write-Output "LOG_ROOT=$logRoot"
Write-Output "CSV=$csvPath"
Write-Output "JSON=$jsonPath"
