param(
    [int]$Seconds = 30,
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

function Set-Utf8File {
    param(
        [string]$Path,
        [string]$Text
    )
    $enc = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Text, $enc)
}

function Get-Metric {
    param(
        [string]$Line,
        [string]$Name
    )
    if ([string]::IsNullOrWhiteSpace($Line)) { return "" }
    $m = [regex]::Match($Line, "(^|\s)" + [regex]::Escape($Name) + "=([^\s]+)")
    if ($m.Success) { return $m.Groups[2].Value }
    return ""
}

function Set-IniValueText {
    param(
        [string]$Text,
        [string]$Key,
        [string]$Value
    )
    $pattern = "(?m)^" + [regex]::Escape($Key) + "=.*$"
    if ([regex]::IsMatch($Text, $pattern)) {
        return [regex]::Replace($Text, $pattern, "$Key=$Value")
    }
    return $Text + "`r`n$Key=$Value`r`n"
}

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $Root "logs\phase_h4_rk3588_perf_triage-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hwaExe = Join-Path $Root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaCwd = Join-Path $Root "HwaSim_IR\Bin"
$videoExe = Join-Path $Root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoCwd = Join-Path $Root "HwaSim_IR_VideoDisplay\x64\Release"
$stimExe = Join-Path $Root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimCwd = Join-Path $Root "DataDrivenTestQT"

foreach ($p in @($hwaExe, $videoExe, $stimExe)) {
    if (-not (Test-Path -LiteralPath $p)) {
        throw "Missing executable: $p"
    }
}

$hwaCfg = Join-Path $Root "HwaSim_IR\Bin\Config\NetworkConfig.ini"
$runtimeCfg = Join-Path $Root "HwaSim_IR\Bin\Config\HwaSimIRRuntime.ini"
$videoCfg = Join-Path $Root "HwaSim_IR_VideoDisplay\x64\Release\NetworkConfig.ini"
$stimCfg = Join-Path $Root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\NetworkConfig.ini"
$backup = @{}
foreach ($p in @($hwaCfg, $runtimeCfg, $videoCfg, $stimCfg)) {
    if (Test-Path -LiteralPath $p) {
        $backup[$p] = [System.IO.File]::ReadAllBytes($p)
    }
}

$oldQtLog = [Environment]::GetEnvironmentVariable("QT_FORCE_STDERR_LOGGING", "Process")

$cases = @(
    @{ Name = "A_current_direct_final_gray_q95_overlayOff"; FastJson = "false"; Occlusion = "true";  Json = "true";  TargetCull = "false" },
    @{ Name = "B_annotation_fast_json_on";                 FastJson = "true";  Occlusion = "true";  Json = "true";  TargetCull = "false" },
    @{ Name = "C_annotation_occlusion_off";                FastJson = "true";  Occlusion = "false"; Json = "true";  TargetCull = "false" },
    @{ Name = "D_annotation_json_off_for_diagnosis_only";  FastJson = "true";  Occlusion = "false"; Json = "false"; TargetCull = "false" },
    @{ Name = "E_target_update_cull_on";                   FastJson = "true";  Occlusion = "true";  Json = "true";  TargetCull = "true" }
)

try {
    Set-Utf8File $hwaCfg "[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=8888`r`nremoteIp=127.0.0.1`r`nremotePort=9999`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=5555`r`n"
    Set-Utf8File $videoCfg "[Network]`r`nip=127.0.0.1`r`nport=5555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n"
    Set-Utf8File $stimCfg "[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=9999`r`nremoteIp=127.0.0.1`r`nremotePort=8888`r`n"
    $env:QT_FORCE_STDERR_LOGGING = "1"

    $summary = @()
    foreach ($case in $cases) {
        $caseDir = Join-Path $logDir $case.Name
        New-Item -ItemType Directory -Force -Path $caseDir | Out-Null

        $runtimeText = [System.IO.File]::ReadAllText($runtimeCfg)
        $runtimeText = Set-IniValueText $runtimeText "PresentationMode" "HeadlessOffscreen"
        $runtimeText = Set-IniValueText $runtimeText "WindowPreview" "false"
        $runtimeText = Set-IniValueText $runtimeText "HeadlessFastDirectFinal" "true"
        $runtimeText = Set-IniValueText $runtimeText "HeadlessImageProbe" "true"
        $runtimeText = Set-IniValueText $runtimeText "RenderPerfProbe" "true"
        $runtimeText = Set-IniValueText $runtimeText "OverlayInSensorImage" "false"
        $runtimeText = Set-IniValueText $runtimeText "JsonPerFrame" $case.Json
        $runtimeText = Set-IniValueText $runtimeText "FastJsonMode" $case.FastJson
        $runtimeText = Set-IniValueText $runtimeText "BBoxUpdateHz" "10"
        $runtimeText = Set-IniValueText $runtimeText "OcclusionUpdateHz" "5"
        $runtimeText = Set-IniValueText $runtimeText "ReuseLastWhenSkipped" "true"
        $runtimeText = Set-IniValueText $runtimeText "OcclusionEnable" $case.Occlusion
        $runtimeText = Set-IniValueText $runtimeText "TargetUpdateCullInvisible" $case.TargetCull
        $runtimeText = Set-IniValueText $runtimeText "JpegEncodeMode" "gray"
        $runtimeText = Set-IniValueText $runtimeText "JpegQuality" "95"
        Set-Utf8File $runtimeCfg $runtimeText

        $video = $null
        $hwa = $null
        $stim = $null
        try {
            $video = Start-Process -FilePath $videoExe -WorkingDirectory $videoCwd -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "video.out.log") `
                -RedirectStandardError (Join-Path $caseDir "video.err.log")
            Start-Sleep -Seconds 2
            $hwa = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaCwd -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "hwa.out.log") `
                -RedirectStandardError (Join-Path $caseDir "hwa.err.log")
            Start-Sleep -Seconds 5
            $stim = Start-Process -FilePath $stimExe -ArgumentList "--phase1b-auto-seconds=$Seconds" -WorkingDirectory $stimCwd -WindowStyle Hidden -PassThru `
                -RedirectStandardOutput (Join-Path $caseDir "stim.out.log") `
                -RedirectStandardError (Join-Path $caseDir "stim.err.log")
            if (-not $stim.WaitForExit(($Seconds + 25) * 1000)) {
                throw "Stim timeout for case $($case.Name)"
            }
            Start-Sleep -Seconds 5
        }
        finally {
            foreach ($p in @($stim, $hwa, $video)) {
                if ($p) {
                    $p.Refresh()
                    if (-not $p.HasExited) {
                        Stop-Process -Id $p.Id -Force
                        $p.WaitForExit()
                    }
                }
            }
        }

        $hwaLog = Join-Path $caseDir "hwa.out.log"
        $perfLine = ""
        $sceneProbeLine = ""
        $renderProbeLine = ""
        if (Test-Path -LiteralPath $hwaLog) {
            $perfLines = Get-Content -LiteralPath $hwaLog | Where-Object { $_.StartsWith("[Perf] ") }
            $perfLine = $perfLines | Where-Object {
                $fps = Get-Metric $_ "outputFps"
                -not [string]::IsNullOrWhiteSpace($fps) -and ([double]$fps) -gt 0.0
            } | Select-Object -Last 1
            if ([string]::IsNullOrWhiteSpace($perfLine)) {
                $perfLine = $perfLines | Select-Object -Last 1
            }
            $sceneProbeLine = Get-Content -LiteralPath $hwaLog | Where-Object { $_.StartsWith("[ScenePerfProbe]") } | Select-Object -Last 1
            $renderProbeLine = Get-Content -LiteralPath $hwaLog | Where-Object { $_.StartsWith("[RenderPerfProbe]") } | Select-Object -Last 1
        }

        $summary += [pscustomobject]@{
            Case = $case.Name
            OutputFps = Get-Metric $perfLine "outputFps"
            RenderFps = Get-Metric $perfLine "renderFps"
            SceneUpdateMs = Get-Metric $perfLine "sceneUpdateMs"
            AnnotationMs = Get-Metric $perfLine "annotationMs"
            RenderMs = Get-Metric $perfLine "renderMs"
            PandaDoFrameMs = Get-Metric $renderProbeLine "pandaDoFrameMs"
            TargetMappingMs = Get-Metric $sceneProbeLine "targetMappingMs"
            Stage4Stage5UpdateMs = Get-Metric $sceneProbeLine "stage4Stage5UpdateMs"
            AnnotationBBoxMs = Get-Metric $sceneProbeLine "annotationBBoxMs"
            AnnotationOcclusionMs = Get-Metric $sceneProbeLine "annotationOcclusionMs"
            ReadbackMs = Get-Metric $perfLine "readbackMs"
            FrameCopyMs = Get-Metric $perfLine "frameCopyMs"
            JpegMs = Get-Metric $perfLine "jpegMs"
            InputQueueDepth = Get-Metric $perfLine "inputQueueDepth"
            SourceSeqLag = Get-Metric $perfLine "sourceSeqLag"
            LatencyAvgMs = Get-Metric $perfLine "latencyAvgMs"
            LogDir = $caseDir
        }
    }

    $summary | Format-Table -AutoSize
    $summary | ConvertTo-Csv -NoTypeInformation | Set-Content -LiteralPath (Join-Path $logDir "summary.csv") -Encoding UTF8
    Write-Output "LOGDIR=$logDir"
}
finally {
    foreach ($p in $backup.Keys) {
        [System.IO.File]::WriteAllBytes($p, $backup[$p])
    }
    [Environment]::SetEnvironmentVariable("QT_FORCE_STDERR_LOGGING", $oldQtLog, "Process")
}
