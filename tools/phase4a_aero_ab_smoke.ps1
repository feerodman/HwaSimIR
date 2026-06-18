param(
    [int]$Seconds = 10,
    [int]$RetrySeconds = 20,
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\aero_runtime_ab_summary.csv"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Get-NumberAvg {
    param([string]$Text, [string]$Field)
    $matches = [regex]::Matches($Text, "\b" + [regex]::Escape($Field) + "=([-+]?[0-9]+(?:\.[0-9]+)?)")
    $values = @($matches | ForEach-Object { [double]$_.Groups[1].Value })
    if ($values.Count -eq 0) { return 0.0 }
    return ($values | Measure-Object -Average).Average
}

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
            throw "Phase4A A/B $CaseName failed: $($check.Name)=$($check.Value) < $($check.Min)"
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        throw "Phase4A A/B $CaseName failed: latencyAvgMs=$($Summary.latencyAvgMs) > 80"
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        throw "Phase4A A/B $CaseName failed: recorderDroppedFrames=$($Summary.recorderDroppedFrames)"
    }
}

function Invoke-AeroCase {
    param([string]$Apply, [int]$RunSeconds, [switch]$IsRetry)
    $retryText = if ($IsRetry) { " retry" } else { "" }
    Write-Host "Running Phase4A aero A/B$retryText case ApplyAeroToRadiance=$Apply seconds=$RunSeconds ..."
    $runOutput = & $runner -Seconds $RunSeconds `
        -ApplyAeroToRadiance $Apply `
        -AeroDebugLog "true" `
        -Stage5LogComponents "true" `
        -Stage5ComponentLogEveryFrames 120
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase4A A/B did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName "ApplyAeroToRadiance=$Apply"
    $hwaLog = Join-Path $summary.logDir "hwa.out.log"
    $hwaText = if (Test-Path -LiteralPath $hwaLog) { Get-Content -LiteralPath $hwaLog -Raw } else { "" }
    $expectedApplied = if ($Apply -eq "true") { "1" } else { "0" }
    $aeroAppliedLogs = [regex]::Matches($hwaText, "aeroAppliedToRadiance=$expectedApplied").Count
    return [pscustomobject]@{
        applyAeroToRadiance = $Apply
        seconds = $RunSeconds
        retry = $IsRetry.IsPresent
        logDir = $summary.logDir
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        videoDisplayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        irUpdateMsAvg = $summary.irUpdateMsAvg
        stage5RadianceComponentMs = $summary.stage5RadianceComponentMs
        stage5AeroThermalMs = $summary.stage5AeroThermalMs
        shaderInputCacheHitRateAvg = $summary.shaderInputCacheHitRateAvg
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        sourceSeqLagMax = $summary.sourceSeqLagMax
        inputQueueDepthMax = $summary.inputQueueDepthMax
        recorderDroppedFrames = $summary.recorderDroppedFrames
        bodyRadianceAvg = [math]::Round((Get-NumberAvg $hwaText "bodyRadiance"), 6)
        materialTempEffectiveKAvg = [math]::Round((Get-NumberAvg $hwaText "materialTempEffectiveK"), 6)
        bodyAeroDeltaKAvg = [math]::Round((Get-NumberAvg $hwaText "bodyAeroDeltaK"), 6)
        machAvg = [math]::Round((Get-NumberAvg $hwaText "mach"), 6)
        aeroAppliedLogCount = $aeroAppliedLogs
    }
}

$rows = New-Object System.Collections.Generic.List[object]
foreach ($apply in @("false", "true")) {
    try {
        $rows.Add((Invoke-AeroCase -Apply $apply -RunSeconds $Seconds)) | Out-Null
    } catch {
        if ($RetrySeconds -le $Seconds) {
            throw
        }
        Write-Warning "$($_.Exception.Message); retrying with seconds=$RetrySeconds"
        $rows.Add((Invoke-AeroCase -Apply $apply -RunSeconds $RetrySeconds -IsRetry)) | Out-Null
    }
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host "Phase4A aero runtime A/B smoke passed: $OutputPath"
