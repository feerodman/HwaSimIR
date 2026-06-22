param(
    [int]$Seconds = 20,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\aero_runtime_speed_ab_summary.csv"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Assert-Link60 {
    param($Summary, [string]$CaseName)
    $checks = @(
        @{ Name = "sentFps"; Value = [double]$Summary.sentFps; Min = 59.5 },
        @{ Name = "udpFps"; Value = [double]$Summary.udpFps; Min = 59.5 },
        @{ Name = "renderFps"; Value = [double]$Summary.renderFps; Min = 59.5 },
        @{ Name = "outputFps"; Value = [double]$Summary.outputFps; Min = 59.5 },
        @{ Name = "videoDisplayFps"; Value = [double]$Summary.videoDisplayFps; Min = 59.5 }
    )
    foreach ($check in $checks) {
        if ($check.Value -lt $check.Min) {
            throw "Phase4B A/B $CaseName failed: $($check.Name)=$($check.Value) < $($check.Min)"
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        throw "Phase4B A/B $CaseName failed: latencyAvgMs=$($Summary.latencyAvgMs) > 80"
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        throw "Phase4B A/B $CaseName failed: recorderDroppedFrames=$($Summary.recorderDroppedFrames)"
    }
    if ([double]$Summary.speedKmhAvg -le 0.0) {
        throw "Phase4B A/B $CaseName failed: speedKmhAvg=$($Summary.speedKmhAvg)"
    }
    if ([double]$Summary.machAvg -le 0.0) {
        throw "Phase4B A/B $CaseName failed: machAvg=$($Summary.machAvg)"
    }
}

function Invoke-AeroSpeedCase {
    param([string]$Apply)
    Write-Host "Running Phase4B aero runtime speed A/B case ApplyAeroToRadiance=$Apply seconds=$Seconds ..."
    $runOutput = & $runner -Seconds $Seconds `
        -ApplyAeroToRadiance $Apply `
        -AeroApplyScale 0.25 `
        -AeroApplyClampBodyDeltaK 40.0 `
        -AeroApplyOnlyBand "MWIR" `
        -AeroDebugLog "true" `
        -Stage5LogComponents "false" `
        -Stage5ComponentLogEveryFrames 120
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase4B A/B did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName "ApplyAeroToRadiance=$Apply"
    return [pscustomobject]@{
        applyAeroToRadiance = $Apply
        seconds = $Seconds
        logDir = $summary.logDir
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        sourceSeqLagMax = $summary.sourceSeqLagMax
        inputQueueDepthMax = $summary.inputQueueDepthMax
        irUpdateMsAvg = $summary.irUpdateMsAvg
        stage5RadianceComponentMs = $summary.stage5RadianceComponentMs
        stage5AeroThermalMs = $summary.stage5AeroThermalMs
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        speedKmhAvg = $summary.speedKmhAvg
        machAvg = $summary.machAvg
        bodyAeroDeltaKRawAvg = $summary.bodyAeroDeltaKRawAvg
        bodyAeroDeltaKEffectiveAvg = $summary.bodyAeroDeltaKEffectiveAvg
        bodyRadianceNoAero = $summary.bodyRadianceNoAeroAvg
        bodyRadianceWithAero = $summary.bodyRadianceWithAeroAvg
        aeroRadianceRatio = $summary.aeroRadianceRatioAvg
        physicalNote = $(if ([double]$summary.machAvg -lt 0.5) { "low_real_mach_visual_change_expected_weak" } else { "mach_range_visible_change_more_likely" })
    }
}

$rows = New-Object System.Collections.Generic.List[object]
foreach ($apply in @("false", "true")) {
    $rows.Add((Invoke-AeroSpeedCase -Apply $apply)) | Out-Null
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host "Phase4B aero runtime speed A/B smoke passed: $OutputPath"
