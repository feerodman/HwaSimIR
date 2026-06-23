param(
    [int]$Seconds = 10,
    [string]$OutputPath = "",
    [string]$FrameDir = "",
    [string]$ClipDir = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage6\detector_noise_ab_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage6\phase6c_frames"
}
if ([string]::IsNullOrWhiteSpace($ClipDir)) {
    $ClipDir = Join-Path $root.Path "logs\stage6\phase6c_clips"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null
New-Item -ItemType Directory -Force -Path $ClipDir | Out-Null

$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Assert-Link60 {
    param($Summary, [string]$CaseName, [bool]$Strict)
    $issues = New-Object System.Collections.Generic.List[string]
    foreach ($check in @(
        @{ Name = "sentFps"; Value = [double]$Summary.sentFps },
        @{ Name = "udpFps"; Value = [double]$Summary.udpFps },
        @{ Name = "renderFps"; Value = [double]$Summary.renderFps },
        @{ Name = "outputFps"; Value = [double]$Summary.outputFps },
        @{ Name = "displayFps"; Value = [double]$Summary.videoDisplayFps }
    )) {
        if ($check.Value -lt 59.5) {
            $issues.Add("$($check.Name)=$($check.Value) < 59.5") | Out-Null
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        $issues.Add("latencyAvgMs=$($Summary.latencyAvgMs) > 80") | Out-Null
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        $issues.Add("recordingDroppedFrames=$($Summary.recorderDroppedFrames)") | Out-Null
    }
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        $issues.Add("sourceSeqContinuous=$($Summary.sourceSeqContinuous)") | Out-Null
    }
    if ([double]$Summary.stage5ModtranLookupMs -gt 0.001) {
        $issues.Add("stage5ModtranLookupMs=$($Summary.stage5ModtranLookupMs) expected 0") | Out-Null
    }
    if ($CaseName -eq "baseline" -and [double]$Summary.stage6DetectorNoiseMs -gt 0.001) {
        $issues.Add("baseline stage6DetectorNoiseMs=$($Summary.stage6DetectorNoiseMs) expected 0") | Out-Null
    }
    if ($issues.Count -gt 0) {
        $message = "Phase6C $CaseName validation: " + ($issues -join "; ")
        if ($Strict) {
            throw $message
        }
        Write-Warning $message
    }
}

function Format-Token {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.####}", $Value)).Replace(".", "p")
}

function Export-RepresentativeFrame {
    param($Summary, [string]$CaseName)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) {
        return ""
    }
    $seekSec = if ($Seconds -ge 6) { [math]::Min([double]$Seconds - 1.0, 5.5) } else { [math]::Max(1.0, [double]$Seconds / 2.0) }
    $frameName = "phase6c_{0}_noise{1}_temporal{2}_fpn{3}_column{4}_agc{5}_sourceSeq{6}.png" -f `
        $CaseName,
        $Summary.detectorNoiseEnabled,
        (Format-Token ([double]$Summary.temporalNoiseSigmaGray)),
        (Format-Token ([double]$Summary.fpnSigmaGray)),
        (Format-Token ([double]$Summary.columnNoiseSigmaGray)),
        $Summary.agcEnabled,
        $Summary.writtenFrames
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

function Export-ShortClip {
    param($Summary, [string]$CaseName)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) {
        return ""
    }
    $clipPath = Join-Path $ClipDir ("phase6c_{0}_clip.mp4" -f $CaseName)
    & $ffmpeg.Source -y -loglevel error -ss 2 -t ([math]::Min(3, [math]::Max(1, $Seconds - 2))) `
        -i $Summary.outputMp4 -c copy $clipPath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $clipPath)) {
        return ""
    }
    return $clipPath
}

$cases = @(
    @{
        Name = "baseline"
        EnableDetectorNoise = "false"
        EnableTemporalNoise = "true"
        TemporalNoiseSigmaGray = 0.005
        EnableFPN = "false"
        FPNSigmaGray = 0.003
        EnableColumnNoise = "false"
        ColumnNoiseSigmaGray = 0.002
        EnableAGC = "false"
        EnableMTFBlur = "false"
        Strict = $true
        Visual = "reference"
    },
    @{
        Name = "temporal_weak"
        EnableDetectorNoise = "true"
        EnableTemporalNoise = "true"
        TemporalNoiseSigmaGray = 0.003
        EnableFPN = "false"
        FPNSigmaGray = 0.003
        EnableColumnNoise = "false"
        ColumnNoiseSigmaGray = 0.002
        EnableAGC = "false"
        EnableMTFBlur = "false"
        Strict = $true
        Visual = "subtle_grain_expected"
    },
    @{
        Name = "temporal_normal"
        EnableDetectorNoise = "true"
        EnableTemporalNoise = "true"
        TemporalNoiseSigmaGray = 0.006
        EnableFPN = "false"
        FPNSigmaGray = 0.003
        EnableColumnNoise = "false"
        ColumnNoiseSigmaGray = 0.002
        EnableAGC = "false"
        EnableMTFBlur = "false"
        Strict = $true
        Visual = "visible_noise_expected"
    },
    @{
        Name = "fpn_normal"
        EnableDetectorNoise = "true"
        EnableTemporalNoise = "false"
        TemporalNoiseSigmaGray = 0.005
        EnableFPN = "true"
        FPNSigmaGray = 0.004
        EnableColumnNoise = "true"
        ColumnNoiseSigmaGray = 0.0015
        EnableAGC = "false"
        EnableMTFBlur = "false"
        Strict = $true
        Visual = "fixed_pattern_expected"
    },
    @{
        Name = "combined_candidate"
        EnableDetectorNoise = "true"
        EnableTemporalNoise = "true"
        TemporalNoiseSigmaGray = 0.004
        EnableFPN = "true"
        FPNSigmaGray = 0.0025
        EnableColumnNoise = "true"
        ColumnNoiseSigmaGray = 0.001
        EnableAGC = "false"
        EnableMTFBlur = "false"
        Strict = $true
        Visual = "candidate_review"
    },
    @{
        Name = "agc_noise_observe"
        EnableDetectorNoise = "true"
        EnableTemporalNoise = "true"
        TemporalNoiseSigmaGray = 0.004
        EnableFPN = "true"
        FPNSigmaGray = 0.0025
        EnableColumnNoise = "false"
        ColumnNoiseSigmaGray = 0.002
        EnableAGC = "true"
        EnableMTFBlur = "false"
        Strict = $false
        Visual = "agc_noise_observe_only"
    }
)

$rows = New-Object System.Collections.Generic.List[object]
foreach ($case in $cases) {
    Write-Host "Running Phase6C $($case.Name) seconds=$Seconds detectorNoise=$($case.EnableDetectorNoise) temporal=$($case.TemporalNoiseSigmaGray) fpn=$($case.EnableFPN) agc=$($case.EnableAGC) ..."
    $runOutput = & $runner -Seconds $Seconds `
        -EnableDetectorNoise $case.EnableDetectorNoise `
        -NoiseApplyTo "final_display" `
        -NoisePosition "BeforeAGC" `
        -EnableTemporalNoise $case.EnableTemporalNoise `
        -TemporalNoiseSigmaGray ([double]$case.TemporalNoiseSigmaGray) `
        -EnableFPN $case.EnableFPN `
        -FPNSigmaGray ([double]$case.FPNSigmaGray) `
        -FPNSeed 12345 `
        -EnableColumnNoise $case.EnableColumnNoise `
        -ColumnNoiseSigmaGray ([double]$case.ColumnNoiseSigmaGray) `
        -EnableRowNoise "false" `
        -RowNoiseSigmaGray 0.001 `
        -EnableBadPixels "false" `
        -BadPixelRatio 0.0001 `
        -NoiseDebugLog "false" `
        -EnableAGC $case.EnableAGC `
        -AGCMode "Percentile" `
        -AGCApplyTo "final_display" `
        -AGCStatsSource "previous_readback" `
        -AGCUpdateHz 30 `
        -AGCStride 8 `
        -EnableMTFBlur $case.EnableMTFBlur `
        -UseSensorInputForDisplay "false"
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase6C $($case.Name) did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $case.Name -Strict ([bool]$case.Strict)
    $framePath = Export-RepresentativeFrame -Summary $summary -CaseName $case.Name
    $clipPath = Export-ShortClip -Summary $summary -CaseName $case.Name
    $rows.Add([pscustomobject]@{
        caseName = $case.Name
        enableDetectorNoise = $case.EnableDetectorNoise
        enableTemporalNoise = $case.EnableTemporalNoise
        temporalSigma = $case.TemporalNoiseSigmaGray
        enableFPN = $case.EnableFPN
        fpnSigma = $case.FPNSigmaGray
        enableColumnNoise = $case.EnableColumnNoise
        columnSigma = $case.ColumnNoiseSigmaGray
        enableAGC = $case.EnableAGC
        enableMTFBlur = $case.EnableMTFBlur
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        stage6DetectorNoiseMs = $summary.stage6DetectorNoiseMs
        stage6AgcStatsMs = $summary.stage6AgcStatsMs
        stage6MtfBlurMs = $summary.stage6MtfBlurMs
        jpegMs = $summary.jpegMsAvg
        readbackMs = $summary.readbackMsAvg
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        framePath = $framePath
        clipPath = $clipPath
        visualStatus = $case.Visual
        flickerStatus = $(if ([string]::IsNullOrWhiteSpace($clipPath)) { "not_observed_from_still_frame" } else { "clip_saved_manual_review_needed" })
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath
Write-Output "summaryCsv=$OutputPath"
Write-Output "frameDir=$FrameDir"
Write-Output "clipDir=$ClipDir"
$rows | Format-Table caseName,enableDetectorNoise,enableTemporalNoise,temporalSigma,enableFPN,fpnSigma,enableColumnNoise,enableAGC,sentFps,udpFps,renderFps,outputFps,displayFps,latencyAvgMs,stage6DetectorNoiseMs,recordingDroppedFrames,visualStatus -AutoSize
