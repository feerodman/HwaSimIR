param(
    [int]$Seconds = 10,
    [string]$OutputPath = "",
    [string]$FrameDir = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage6\mtf_blur_ab_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage6\phase6a_frames"
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
    if ($issues.Count -gt 0) {
        $message = "Phase6A $CaseName validation: " + ($issues -join "; ")
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
    param($Summary, [string]$CaseName, [double]$Sigma, [int]$Radius)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) {
        return ""
    }
    $seekSec = if ($Seconds -ge 6) { [math]::Min([double]$Seconds - 1.0, 5.5) } else { [math]::Max(1.0, [double]$Seconds / 2.0) }
    $frameName = "phase6a_{0}_sigma{1}_radius{2}_sourceSeq{3}.png" -f `
        $CaseName, (Format-Token $Sigma), $Radius, $Summary.writtenFrames
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

$cases = @(
    @{ Name = "baseline"; Enable = "false"; Sigma = 0.65; Radius = 2; Strict = $true; Visual = "reference" },
    @{ Name = "weak";     Enable = "true";  Sigma = 0.5;  Radius = 1; Strict = $true; Visual = "ok" },
    @{ Name = "normal";   Enable = "true";  Sigma = 0.8;  Radius = 2; Strict = $true; Visual = "ok" },
    @{ Name = "strong";   Enable = "true";  Sigma = 1.2;  Radius = 3; Strict = $false; Visual = "strong_observe" }
)

$rows = New-Object System.Collections.Generic.List[object]
foreach ($case in $cases) {
    Write-Host "Running Phase6A $($case.Name) seconds=$Seconds enable=$($case.Enable) sigma=$($case.Sigma) radius=$($case.Radius) ..."
    $runOutput = & $runner -Seconds $Seconds `
        -EnableMTFBlur $case.Enable `
        -MTFBlurMode "GaussianSeparable" `
        -MTFBlurSigmaPixels ([double]$case.Sigma) `
        -MTFBlurRadiusPixels ([int]$case.Radius) `
        -MTFBlurPasses 1 `
        -MTFApplyTo "final_display" `
        -MTFDebugLog "false"
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase6A $($case.Name) did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $case.Name -Strict ([bool]$case.Strict)
    $framePath = Export-RepresentativeFrame -Summary $summary -CaseName $case.Name -Sigma ([double]$case.Sigma) -Radius ([int]$case.Radius)
    $rows.Add([pscustomobject]@{
        caseName = $case.Name
        enableMTFBlur = $case.Enable
        sigmaPixels = [double]$case.Sigma
        radiusPixels = [int]$case.Radius
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        stage6MtfBlurMs = $summary.stage6MtfBlurMs
        readbackMs = $summary.readbackMsAvg
        jpegMs = $summary.jpegMsAvg
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        framePath = $framePath
        visualStatus = $case.Visual
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath
Write-Output "summaryCsv=$OutputPath"
Write-Output "frameDir=$FrameDir"
$rows | Format-Table caseName,enableMTFBlur,sigmaPixels,radiusPixels,sentFps,udpFps,renderFps,outputFps,displayFps,latencyAvgMs,stage6MtfBlurMs,recordingDroppedFrames,visualStatus -AutoSize
