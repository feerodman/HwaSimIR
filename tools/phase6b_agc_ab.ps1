param(
    [int]$Seconds = 10,
    [string]$OutputPath = "",
    [string]$FrameDir = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage6\agc_ab_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage6\phase6b_frames"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null

$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Assert-Link60 {
    param($Summary, [string]$CaseName, [bool]$Strict)
    $issues = New-Object System.Collections.Generic.List[string]
    foreach ($check in @(
        @{ Name = "sentFps"; Value = [double]$Summary.sentFps },
        @{ Name = "udpFps"; Value = [double]$Summary.udpFps },
        @{ Name = "renderFps"; Value = [double]$Summary.renderFps },
        @{ Name = "outputFps"; Value = [double]$Summary.outputFps },
        @{ Name = "videoDisplayFps"; Value = [double]$Summary.videoDisplayFps }
    )) {
        if ($check.Value -lt 59.5) {
            $issues.Add("$($check.Name)=$($check.Value) < 59.5") | Out-Null
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        $issues.Add("latencyAvgMs=$($Summary.latencyAvgMs) > 80") | Out-Null
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        $issues.Add("recorderDroppedFrames=$($Summary.recorderDroppedFrames)") | Out-Null
    }
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        $issues.Add("sourceSeqContinuous=$($Summary.sourceSeqContinuous)") | Out-Null
    }
    if ([double]$Summary.stage5ModtranLookupMs -gt 0.001) {
        $issues.Add("stage5ModtranLookupMs=$($Summary.stage5ModtranLookupMs) expected 0") | Out-Null
    }
    if ($issues.Count -gt 0) {
        $message = "Phase6B $CaseName validation: " + ($issues -join "; ")
        if ($Strict) {
            throw $message
        }
        Write-Warning $message
    }
}

function Format-Token {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $Value)).Replace(".", "p")
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
    $frameName = "phase6b_{0}_agc{1}_mtf{2}_sensor{3}_gain{4}_sourceSeq{5}.png" -f `
        $CaseName,
        $Summary.agcEnabled,
        $Summary.mtfBlurEnabled,
        $Summary.useSensorInputForDisplay,
        (Format-Token ([double]$Summary.agcGainAvg)),
        $Summary.writtenFrames
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

$cases = @(
    @{
        Name = "baseline"
        EnableAGC = "false"
        AGCMode = "Percentile"
        EnableMTFBlur = "false"
        UseSensorInputForDisplay = "false"
        SensorInputDisplayScale = 1.0
        Strict = $true
        Visual = "reference"
    },
    @{
        Name = "agc_percentile"
        EnableAGC = "true"
        AGCMode = "Percentile"
        EnableMTFBlur = "false"
        UseSensorInputForDisplay = "false"
        SensorInputDisplayScale = 1.0
        Strict = $true
        Visual = "needs_review"
    },
    @{
        Name = "mtf_agc"
        EnableAGC = "true"
        AGCMode = "Percentile"
        EnableMTFBlur = "true"
        MTFBlurSigmaPixels = 0.8
        MTFBlurRadiusPixels = 2
        UseSensorInputForDisplay = "false"
        SensorInputDisplayScale = 1.0
        Strict = $true
        Visual = "needs_review"
    },
    @{
        Name = "sensor_agc_observe"
        EnableAGC = "true"
        AGCMode = "Percentile"
        EnableMTFBlur = "false"
        UseSensorInputForDisplay = "true"
        SensorInputDisplayScale = 0.35
        Strict = $false
        Visual = "observe_only"
    }
)

$rows = New-Object System.Collections.Generic.List[object]
foreach ($case in $cases) {
    $sigma = if ($case.ContainsKey("MTFBlurSigmaPixels")) { [double]$case.MTFBlurSigmaPixels } else { 0.65 }
    $radius = if ($case.ContainsKey("MTFBlurRadiusPixels")) { [int]$case.MTFBlurRadiusPixels } else { 2 }
    Write-Host "Running Phase6B $($case.Name) seconds=$Seconds agc=$($case.EnableAGC) mtf=$($case.EnableMTFBlur) sensor=$($case.UseSensorInputForDisplay) ..."
    $runOutput = & $runner -Seconds $Seconds `
        -EnableAGC $case.EnableAGC `
        -AGCMode $case.AGCMode `
        -AGCApplyTo "final_display" `
        -AGCStatsSource "previous_readback" `
        -AGCUpdateHz 30 `
        -AGCStride 8 `
        -AGCDebugLog "false" `
        -EnableMTFBlur $case.EnableMTFBlur `
        -MTFBlurMode "GaussianSeparable" `
        -MTFBlurSigmaPixels $sigma `
        -MTFBlurRadiusPixels $radius `
        -MTFBlurPasses 1 `
        -MTFApplyTo "final_display" `
        -MTFDebugLog "false" `
        -UseSensorInputForDisplay $case.UseSensorInputForDisplay `
        -SensorInputDisplayMode "Manual" `
        -SensorInputDisplayScale ([double]$case.SensorInputDisplayScale) `
        -SensorInputDisplayOffset 0.0 `
        -SensorInputDisplayGamma 1.0
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase6B $($case.Name) did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $case.Name -Strict ([bool]$case.Strict)
    $framePath = Export-RepresentativeFrame -Summary $summary -CaseName $case.Name
    $rows.Add([pscustomobject]@{
        caseName = $case.Name
        enableAGC = $case.EnableAGC
        agcMode = $case.AGCMode
        enableMTFBlur = $case.EnableMTFBlur
        useSensorInputForDisplay = $case.UseSensorInputForDisplay
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        stage6AgcStatsMs = $summary.stage6AgcStatsMs
        stage6MtfBlurMs = $summary.stage6MtfBlurMs
        agcGainAvg = $summary.agcGainAvg
        agcOffsetAvg = $summary.agcOffsetAvg
        agcLowInputAvg = $summary.agcLowInputAvg
        agcHighInputAvg = $summary.agcHighInputAvg
        jpegMs = $summary.jpegMsAvg
        readbackMs = $summary.readbackMsAvg
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        framePath = $framePath
        visualStatus = $case.Visual
        flickerStatus = "not_observed_from_still_frame"
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath
Write-Output "summaryCsv=$OutputPath"
Write-Output "frameDir=$FrameDir"
$rows | Format-Table caseName,enableAGC,enableMTFBlur,useSensorInputForDisplay,sentFps,udpFps,renderFps,outputFps,displayFps,latencyAvgMs,stage6AgcStatsMs,agcGainAvg,agcOffsetAvg,recordingDroppedFrames,visualStatus -AutoSize
