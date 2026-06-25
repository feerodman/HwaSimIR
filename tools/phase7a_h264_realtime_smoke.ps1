param(
    [int]$BaselineSeconds = 30,
    [int]$CaseSeconds = 30,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\phase7\h264_realtime_summary.csv"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null

$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"
if (-not (Test-Path -LiteralPath $runner)) {
    throw "Runner missing: $runner"
}

function Get-SummaryFromOutput {
    param([object[]]$RunOutput)
    $line = @($RunOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($line.Count -eq 0) {
        throw "phase2a runner did not print summary=..."
    }
    $summaryPath = ($line[-1] -replace "^summary=", "").Trim()
    if (-not (Test-Path -LiteralPath $summaryPath)) {
        throw "Summary JSON missing: $summaryPath"
    }
    return Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
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
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        $issues.Add("sourceSeqContinuous=$($Summary.sourceSeqContinuous)") | Out-Null
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        $issues.Add("recordingDroppedFrames=$($Summary.recorderDroppedFrames)") | Out-Null
    }
    if ($issues.Count -gt 0) {
        throw "Phase7A $CaseName link validation failed: $($issues -join '; ')"
    }
}

function Assert-CodecExpectation {
    param($Summary, [string]$CaseName)
    $active = [string]$Summary.activeCodec
    $fallback = [string]$Summary.codecFallbackReason
    switch ($CaseName) {
        "jpeg_baseline" {
            if ($active -ne "jpeg" -or [string]$Summary.requestedCodec -ne "jpeg") {
                throw "jpeg_baseline expected requested=jpeg active=jpeg, got requested=$($Summary.requestedCodec) active=$active"
            }
        }
        "h264_requested_experimental_off" {
            if ($active -ne "jpeg" -or $fallback -ne "experimental_disabled") {
                throw "experimental_off expected active=jpeg fallback=experimental_disabled, got active=$active fallback=$fallback"
            }
        }
        "h264_requested_experimental_on" {
            if ($active -eq "h264_annexb") {
                if ([int]$Summary.h264KeyFrameSeen -ne 1 -or [int]$Summary.h264DecodeErrors -ne 0) {
                    throw "experimental_on active=h264_annexb but VideoDisplay did not report clean H.264 decode"
                }
            }
            elseif ($active -eq "jpeg") {
                if ($fallback -notmatch "^encoder_unavailable") {
                    throw "experimental_on fallback must explain encoder_unavailable, got fallback=$fallback"
                }
            }
            else {
                throw "experimental_on unexpected activeCodec=$active"
            }
        }
    }
}

$cases = @(
    @{
        Name = "jpeg_baseline"
        Seconds = $BaselineSeconds
        StimH264En = "0"
        EnableH264Experimental = "false"
        H264Encoder = "auto"
    },
    @{
        Name = "h264_requested_experimental_off"
        Seconds = $CaseSeconds
        StimH264En = "1"
        EnableH264Experimental = "false"
        H264Encoder = "auto"
    },
    @{
        Name = "h264_requested_experimental_on"
        Seconds = $CaseSeconds
        StimH264En = "1"
        EnableH264Experimental = "true"
        H264Encoder = "auto"
    }
)

$rows = New-Object System.Collections.Generic.List[object]
$h264Available = $false

foreach ($case in $cases) {
    Write-Host "Running Phase7A $($case.Name) seconds=$($case.Seconds) h264En=$($case.StimH264En) experimental=$($case.EnableH264Experimental) ..."
    $runOutput = & $runner -Seconds ([int]$case.Seconds) `
        -StimH264En $case.StimH264En `
        -EnableH264Experimental $case.EnableH264Experimental `
        -H264Encoder $case.H264Encoder `
        -H264BitrateKbps 4000 `
        -H264GopFrames 30 `
        -H264LowLatency "true" `
        -H264FallbackToJpeg "true" `
        -H264ForceKeyFrameOnStart "true"
    $summary = Get-SummaryFromOutput $runOutput
    Assert-Link60 $summary $case.Name
    Assert-CodecExpectation $summary $case.Name
    if ([string]$summary.activeCodec -eq "h264_annexb") {
        $h264Available = $true
    }
    $rows.Add([pscustomobject]@{
        caseName = $case.Name
        h264En = $case.StimH264En
        enableH264Experimental = $case.EnableH264Experimental
        requestedCodec = $summary.requestedCodec
        activeCodec = $summary.activeCodec
        fallbackReason = $summary.codecFallbackReason
        encoderName = $summary.h264EncoderName
        bitrateKbps = $summary.h264BitrateKbps
        gopFrames = $summary.h264GopFrames
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        latencyP95Ms = $summary.latencyP95Ms
        encodeMsAvg = $(if ([string]$summary.activeCodec -eq "h264_annexb") { $summary.h264EncodeMsAvg } else { $summary.jpegMsAvg })
        h264EncodeMsAvg = $summary.h264EncodeMsAvg
        jpegMsAvg = $summary.jpegMsAvg
        encodedBytesAvg = $summary.encodedBytesAvg
        decodeMsAvg = $summary.decodeMsAvg
        h264KeyFrameSeen = $summary.h264KeyFrameSeen
        h264DecodeErrors = $summary.h264DecodeErrors
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
        visualStatus = $(if ([string]$summary.activeCodec -eq "h264_annexb") { "h264_decoded" } else { "jpeg_fallback_or_baseline" })
    }) | Out-Null
}

if ($h264Available) {
    Write-Host "Running Phase7A h264_ab_if_available seconds=30 ..."
    $runOutput = & $runner -Seconds 30 `
        -StimH264En "1" `
        -EnableH264Experimental "true" `
        -H264Encoder "auto" `
        -H264BitrateKbps 4000 `
        -H264GopFrames 30 `
        -H264LowLatency "true" `
        -H264FallbackToJpeg "true" `
        -H264ForceKeyFrameOnStart "true"
    $summary = Get-SummaryFromOutput $runOutput
    Assert-Link60 $summary "h264_ab_if_available"
    if ([string]$summary.activeCodec -ne "h264_annexb") {
        throw "h264_ab_if_available expected activeCodec=h264_annexb, got $($summary.activeCodec)"
    }
    $rows.Add([pscustomobject]@{
        caseName = "h264_ab_if_available"
        h264En = "1"
        enableH264Experimental = "true"
        requestedCodec = $summary.requestedCodec
        activeCodec = $summary.activeCodec
        fallbackReason = $summary.codecFallbackReason
        encoderName = $summary.h264EncoderName
        bitrateKbps = $summary.h264BitrateKbps
        gopFrames = $summary.h264GopFrames
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        latencyP95Ms = $summary.latencyP95Ms
        encodeMsAvg = $summary.h264EncodeMsAvg
        h264EncodeMsAvg = $summary.h264EncodeMsAvg
        jpegMsAvg = $summary.jpegMsAvg
        encodedBytesAvg = $summary.encodedBytesAvg
        decodeMsAvg = $summary.decodeMsAvg
        h264KeyFrameSeen = $summary.h264KeyFrameSeen
        h264DecodeErrors = $summary.h264DecodeErrors
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
        visualStatus = "h264_decoded"
    }) | Out-Null
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host "Phase7A H.264 realtime summary: $OutputPath"
