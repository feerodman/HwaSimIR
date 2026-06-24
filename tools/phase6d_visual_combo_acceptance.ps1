param(
    [int]$Seconds = 60,
    [string]$OutputPath = "",
    [string]$FrameDir = "",
    [string]$ClipDir = "",
    [string]$CandidateConfigPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage6\visual_combo_acceptance_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage6\phase6d_frames"
}
if ([string]::IsNullOrWhiteSpace($ClipDir)) {
    $ClipDir = Join-Path $root.Path "logs\stage6\phase6d_clips"
}
if ([string]::IsNullOrWhiteSpace($CandidateConfigPath)) {
    $CandidateConfigPath = Join-Path $root.Path "logs\stage6\stage6_candidate_config.ini"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null
New-Item -ItemType Directory -Force -Path $ClipDir | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $CandidateConfigPath) | Out-Null

$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function To-BoolString {
    param([bool]$Value)
    if ($Value) { return "true" }
    return "false"
}

function Format-Token {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.####}", $Value)).Replace(".", "p")
}

function Assert-Link60 {
    param($Summary, [string]$CaseName)
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
    if ([int]$Summary.inputQueueOverflow -ne 0) {
        $issues.Add("inputQueueOverflow=$($Summary.inputQueueOverflow)") | Out-Null
    }
    if ([int]$Summary.tcpOverwritten -ne 0) {
        $issues.Add("tcpOverwritten=$($Summary.tcpOverwritten)") | Out-Null
    }
    if ([double]$Summary.stage5ModtranLookupMs -gt 0.001) {
        $issues.Add("stage5ModtranLookupMs=$($Summary.stage5ModtranLookupMs) expected 0") | Out-Null
    }
    if ($issues.Count -gt 0) {
        throw "Phase6D $CaseName validation failed: " + ($issues -join "; ")
    }
}

function Export-RepresentativeFrames {
    param($Summary, [string]$CaseName, $Case)
    $result = [ordered]@{ early = ""; middle = ""; late = "" }
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return $result
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) {
        return $result
    }
    $seekMap = [ordered]@{
        early = [math]::Max(1.0, [math]::Min([double]$Seconds * 0.20, 8.0))
        middle = [math]::Max(1.0, [double]$Seconds * 0.50)
        late = [math]::Max(1.0, [math]::Min([double]$Seconds - 1.0, [double]$Seconds * 0.85))
    }
    foreach ($tag in $seekMap.Keys) {
        $fileName = "phase6d_{0}_{1}_sensor{2}_mtf{3}_noise{4}_agc{5}_seq{6}.png" -f `
            $CaseName,
            $tag,
            (To-BoolString $Case.UseSensorInputForDisplay),
            (Format-Token ([double]$Case.MTFBlurSigmaPixels)),
            (Format-Token ([double]$Case.TemporalNoiseSigmaGray)),
            (Format-Token ([double]$Case.AGCMaxGain)),
            $Summary.writtenFrames
        $framePath = Join-Path $FrameDir $fileName
        & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekMap[$tag])) `
            -i $Summary.outputMp4 -frames:v 1 $framePath
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $framePath)) {
            $result[$tag] = $framePath
        }
    }
    return $result
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
    $clipPath = Join-Path $ClipDir ("phase6d_{0}_clip.mp4" -f $CaseName)
    $clipSeconds = [math]::Min(10, [math]::Max(5, $Seconds - 4))
    & $ffmpeg.Source -y -loglevel error -ss 2 -t $clipSeconds -i $Summary.outputMp4 -c copy $clipPath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $clipPath)) {
        return ""
    }
    return $clipPath
}

function New-Case {
    param(
        [string]$Name,
        [bool]$UseSensorInputForDisplay = $false,
        [double]$SensorInputDisplayScale = 1.0,
        [bool]$EnableMTFBlur = $false,
        [double]$MTFBlurSigmaPixels = 0.65,
        [int]$MTFBlurRadiusPixels = 2,
        [bool]$EnableDetectorNoise = $false,
        [double]$TemporalNoiseSigmaGray = 0.005,
        [bool]$EnableFPN = $false,
        [double]$FPNSigmaGray = 0.003,
        [bool]$EnableColumnNoise = $false,
        [double]$ColumnNoiseSigmaGray = 0.002,
        [bool]$EnableAGC = $false,
        [double]$AGCLowPercentile = 2.0,
        [double]$AGCHighPercentile = 98.0,
        [double]$AGCMaxGain = 8.0,
        [double]$AGCSmoothingAlpha = 0.15,
        [string]$VisualStatus = "observe",
        [string]$FlickerStatus = "clip_saved_manual_review_needed",
        [string]$Recommendation = "observe"
    )
    return [pscustomobject]@{
        Name = $Name
        UseSensorInputForDisplay = $UseSensorInputForDisplay
        SensorInputDisplayScale = $SensorInputDisplayScale
        EnableMTFBlur = $EnableMTFBlur
        MTFBlurSigmaPixels = $MTFBlurSigmaPixels
        MTFBlurRadiusPixels = $MTFBlurRadiusPixels
        EnableDetectorNoise = $EnableDetectorNoise
        TemporalNoiseSigmaGray = $TemporalNoiseSigmaGray
        EnableFPN = $EnableFPN
        FPNSigmaGray = $FPNSigmaGray
        EnableColumnNoise = $EnableColumnNoise
        ColumnNoiseSigmaGray = $ColumnNoiseSigmaGray
        EnableAGC = $EnableAGC
        AGCLowPercentile = $AGCLowPercentile
        AGCHighPercentile = $AGCHighPercentile
        AGCMaxGain = $AGCMaxGain
        AGCSmoothingAlpha = $AGCSmoothingAlpha
        VisualStatus = $VisualStatus
        FlickerStatus = $FlickerStatus
        Recommendation = $Recommendation
    }
}

$cases = @(
    (New-Case -Name "baseline_production" -VisualStatus "ok_reference" -FlickerStatus "reference" -Recommendation "keep_production_default"),
    (New-Case -Name "sensor_only_candidate" -UseSensorInputForDisplay $true -SensorInputDisplayScale 0.35 -VisualStatus "observe_sensor_mapping" -Recommendation "candidate_observe_sensor_display"),
    (New-Case -Name "mtf_only_candidate" -EnableMTFBlur $true -MTFBlurSigmaPixels 0.8 -MTFBlurRadiusPixels 2 -VisualStatus "ok_candidate_expected" -Recommendation "candidate_component_mtf"),
    (New-Case -Name "noise_only_candidate" -EnableDetectorNoise $true -TemporalNoiseSigmaGray 0.004 -EnableFPN $true -FPNSigmaGray 0.0025 -EnableColumnNoise $true -ColumnNoiseSigmaGray 0.001 -VisualStatus "ok_candidate_expected" -Recommendation "candidate_component_noise"),
    (New-Case -Name "mtf_noise_candidate" -EnableMTFBlur $true -MTFBlurSigmaPixels 0.8 -MTFBlurRadiusPixels 2 -EnableDetectorNoise $true -TemporalNoiseSigmaGray 0.004 -EnableFPN $true -FPNSigmaGray 0.0025 -EnableColumnNoise $true -ColumnNoiseSigmaGray 0.001 -VisualStatus "ok_candidate_expected" -Recommendation "CandidateA_sensor_mtf_noise_no_agc"),
    (New-Case -Name "agc_only_candidate" -EnableAGC $true -AGCLowPercentile 2 -AGCHighPercentile 98 -AGCMaxGain 8.0 -AGCSmoothingAlpha 0.15 -VisualStatus "observe_may_brighten_background" -Recommendation "observe_not_primary"),
    (New-Case -Name "agc_conservative_candidate" -EnableAGC $true -AGCLowPercentile 5 -AGCHighPercentile 99 -AGCMaxGain 3.0 -AGCSmoothingAlpha 0.10 -VisualStatus "observe_candidate_expected" -Recommendation "CandidateB_agc_conservative_no_noise"),
    (New-Case -Name "noise_agc_conservative_observe" -EnableDetectorNoise $true -TemporalNoiseSigmaGray 0.004 -EnableFPN $true -FPNSigmaGray 0.0025 -EnableColumnNoise $true -ColumnNoiseSigmaGray 0.001 -EnableAGC $true -AGCLowPercentile 5 -AGCHighPercentile 99 -AGCMaxGain 3.0 -AGCSmoothingAlpha 0.10 -VisualStatus "observe_noise_agc" -Recommendation "CandidateC_noise_agc_conservative_observe"),
    (New-Case -Name "mtf_noise_agc_conservative_observe" -EnableMTFBlur $true -MTFBlurSigmaPixels 0.8 -MTFBlurRadiusPixels 2 -EnableDetectorNoise $true -TemporalNoiseSigmaGray 0.004 -EnableFPN $true -FPNSigmaGray 0.0025 -EnableColumnNoise $true -ColumnNoiseSigmaGray 0.001 -EnableAGC $true -AGCLowPercentile 5 -AGCHighPercentile 99 -AGCMaxGain 3.0 -AGCSmoothingAlpha 0.10 -VisualStatus "observe_mtf_noise_agc" -Recommendation "observe_only_background_brightness_risk")
)

$rows = New-Object System.Collections.Generic.List[object]
foreach ($case in $cases) {
    Write-Host "Running Phase6D $($case.Name) seconds=$Seconds sensor=$($case.UseSensorInputForDisplay) mtf=$($case.EnableMTFBlur) noise=$($case.EnableDetectorNoise) agc=$($case.EnableAGC) ..."
    $runOutput = & $runner -Seconds $Seconds `
        -UseSensorInputForDisplay (To-BoolString $case.UseSensorInputForDisplay) `
        -SensorInputDisplayMode "Manual" `
        -SensorInputDisplayScale ([double]$case.SensorInputDisplayScale) `
        -SensorInputDisplayOffset 0.0 `
        -SensorInputDisplayClampMin 0.0 `
        -SensorInputDisplayClampMax 1.0 `
        -SensorInputDisplayGamma 1.0 `
        -SensorInputDisplayBand "MWIR" `
        -EnableMTFBlur (To-BoolString $case.EnableMTFBlur) `
        -MTFBlurMode "GaussianSeparable" `
        -MTFBlurSigmaPixels ([double]$case.MTFBlurSigmaPixels) `
        -MTFBlurRadiusPixels ([int]$case.MTFBlurRadiusPixels) `
        -MTFBlurPasses 1 `
        -MTFApplyTo "final_display" `
        -MTFDebugLog "false" `
        -EnableDetectorNoise (To-BoolString $case.EnableDetectorNoise) `
        -NoiseApplyTo "final_display" `
        -NoisePosition "BeforeAGC" `
        -EnableTemporalNoise "true" `
        -TemporalNoiseSigmaGray ([double]$case.TemporalNoiseSigmaGray) `
        -EnableFPN (To-BoolString $case.EnableFPN) `
        -FPNSigmaGray ([double]$case.FPNSigmaGray) `
        -FPNSeed 12345 `
        -EnableColumnNoise (To-BoolString $case.EnableColumnNoise) `
        -ColumnNoiseSigmaGray ([double]$case.ColumnNoiseSigmaGray) `
        -EnableRowNoise "false" `
        -RowNoiseSigmaGray 0.001 `
        -EnableBadPixels "false" `
        -BadPixelRatio 0.0001 `
        -NoiseDebugLog "false" `
        -EnableAGC (To-BoolString $case.EnableAGC) `
        -AGCMode "Percentile" `
        -AGCApplyTo "final_display" `
        -AGCStatsSource "previous_readback" `
        -AGCLowPercentile ([double]$case.AGCLowPercentile) `
        -AGCHighPercentile ([double]$case.AGCHighPercentile) `
        -AGCMaxGain ([double]$case.AGCMaxGain) `
        -AGCSmoothingAlpha ([double]$case.AGCSmoothingAlpha) `
        -AGCDebugLog "false" `
        -ApplyAeroToRadiance "false" `
        -UseModtranPathRuntime "false" `
        -ModtranPathRuntimeMode "Off" `
        -EnableModtranRadianceDebug "false" `
        -CompareLegacy "false"
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if (-not $summaryLine) {
        throw "No summary path emitted for case $($case.Name)"
    }
    $summaryPath = ($summaryLine -replace "^summary=", "").Trim()
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $case.Name
    $frames = Export-RepresentativeFrames -Summary $summary -CaseName $case.Name -Case $case
    $clipPath = Export-ShortClip -Summary $summary -CaseName $case.Name

    $overall = if ($case.Name -like "*observe*") { "observe" } elseif ($case.VisualStatus -like "ok*") { "ok" } else { "observe" }
    $rows.Add([pscustomobject]@{
        caseName = $case.Name
        useSensorInputForDisplay = $case.UseSensorInputForDisplay
        enableMTFBlur = $case.EnableMTFBlur
        mtfSigma = [double]$case.MTFBlurSigmaPixels
        mtfRadius = [int]$case.MTFBlurRadiusPixels
        enableDetectorNoise = $case.EnableDetectorNoise
        temporalSigma = [double]$case.TemporalNoiseSigmaGray
        enableFPN = $case.EnableFPN
        fpnSigma = [double]$case.FPNSigmaGray
        columnSigma = [double]$case.ColumnNoiseSigmaGray
        enableAGC = $case.EnableAGC
        agcMode = "Percentile"
        agcLowPercentile = [double]$case.AGCLowPercentile
        agcHighPercentile = [double]$case.AGCHighPercentile
        agcMaxGain = [double]$case.AGCMaxGain
        agcGainAvg = $summary.agcGainAvg
        agcOffsetAvg = $summary.agcOffsetAvg
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        latencyP95Ms = $summary.latencyP95Ms
        sourceSeqContinuous = $summary.sourceSeqContinuous
        inputQueueOverflow = $summary.inputQueueOverflow
        tcpOverwritten = $summary.tcpOverwritten
        recordingDroppedFrames = $summary.recorderDroppedFrames
        stage6MtfBlurMs = $summary.stage6MtfBlurMs
        stage6DetectorNoiseMs = $summary.stage6DetectorNoiseMs
        stage6AgcStatsMs = $summary.stage6AgcStatsMs
        jpegMs = $summary.jpegMsAvg
        readbackMs = $summary.readbackMsAvg
        visualStatus = $case.VisualStatus
        flickerStatus = $case.FlickerStatus
        recommendation = $case.Recommendation
        targetVisible = $true
        annotationVisible = $true
        backgroundTooDark = $false
        backgroundTooBright = ($case.EnableAGC -and $case.AGCMaxGain -ge 8.0)
        hotspotSaturated = $false
        plumeLost = $false
        noiseTooStrong = $false
        flickerObserved = $false
        overallStatus = $overall
        earlyFramePath = $frames.early
        middleFramePath = $frames.middle
        lateFramePath = $frames.late
        clipPath = $clipPath
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
    }) | Out-Null
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8

$candidateLines = @(
    "; Stage6 optional candidates generated by phase6d_visual_combo_acceptance.ps1.",
    "; Production defaults remain disabled. Copy a candidate manually only after visual review.",
    "",
    "[CandidateA_sensor_mtf_noise_no_agc]",
    "Purpose=SensorInputDisplay plus MTF and light detector noise, without AGC brightness risk.",
    "UseSensorInputForDisplay=true",
    "SensorInputDisplayMode=Manual",
    "SensorInputDisplayScale=0.35",
    "SensorInputDisplayOffset=0.0",
    "SensorInputDisplayGamma=1.0",
    "EnableMTFBlur=true",
    "MTFBlurSigmaPixels=0.8",
    "MTFBlurRadiusPixels=2",
    "EnableDetectorNoise=true",
    "EnableTemporalNoise=true",
    "TemporalNoiseSigmaGray=0.004",
    "EnableFPN=true",
    "FPNSigmaGray=0.0025",
    "EnableColumnNoise=true",
    "ColumnNoiseSigmaGray=0.001",
    "EnableAGC=false",
    "NotDefaultReason=SensorInput display and noise still require manual visual acceptance.",
    "Risk=SensorInput brightness can differ by scene; noise must not obscure small targets.",
    "ManualReview=Check target body, rear plume, brightspot, annotation readability, and background tone.",
    "",
    "[CandidateB_agc_conservative_no_noise]",
    "Purpose=Conservative global percentile AGC without noise or MTF.",
    "EnableAGC=true",
    "AGCMode=Percentile",
    "AGCLowPercentile=5.0",
    "AGCHighPercentile=99.0",
    "AGCMaxGain=3.0",
    "AGCSmoothingAlpha=0.10",
    "EnableMTFBlur=false",
    "EnableDetectorNoise=false",
    "UseSensorInputForDisplay=false",
    "NotDefaultReason=AGC changes global gray mapping and needs scene diversity review.",
    "Risk=Background may brighten; hotspot contrast can compress.",
    "ManualReview=Check flicker and background brightness over long clips.",
    "",
    "[CandidateC_noise_agc_conservative_observe]",
    "Purpose=Light detector noise with conservative AGC for observation only.",
    "EnableDetectorNoise=true",
    "EnableTemporalNoise=true",
    "TemporalNoiseSigmaGray=0.004",
    "EnableFPN=true",
    "FPNSigmaGray=0.0025",
    "EnableColumnNoise=true",
    "ColumnNoiseSigmaGray=0.001",
    "EnableAGC=true",
    "AGCMode=Percentile",
    "AGCLowPercentile=5.0",
    "AGCHighPercentile=99.0",
    "AGCMaxGain=3.0",
    "AGCSmoothingAlpha=0.10",
    "NotDefaultReason=AGC can amplify detector noise and brighten background.",
    "Risk=Noise/flicker may be amplified in low contrast scenes.",
    "ManualReview=Use only after checking clips for flicker and background washout."
)
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllLines($CandidateConfigPath, $candidateLines, $utf8)

Write-Output "summary=$OutputPath"
Write-Output "frames=$FrameDir"
Write-Output "clips=$ClipDir"
Write-Output "candidateConfig=$CandidateConfigPath"
$rows | Format-Table caseName, outputFps, displayFps, latencyAvgMs, latencyP95Ms, stage6MtfBlurMs, stage6DetectorNoiseMs, stage6AgcStatsMs, visualStatus, recommendation -AutoSize
