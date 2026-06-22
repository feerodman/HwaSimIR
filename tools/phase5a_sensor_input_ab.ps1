param(
    [int]$Seconds = 6,
    [string]$OutputPath = "",
    [string]$FrameDir = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\sensor_input_ab_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage5\phase5a_frames"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null
$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Convert-ToDouble {
    param($Value)
    if ($null -eq $Value) { return 0.0 }
    $parsed = 0.0
    if ([double]::TryParse([string]$Value, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
        return $parsed
    }
    return 0.0
}

function Get-Average {
    param([double[]]$Values, [switch]$PositiveOnly)
    $usable = @($Values)
    if ($PositiveOnly) {
        $usable = @($usable | Where-Object { $_ -gt 0.0 })
    }
    if ($usable.Count -eq 0) { return 0.0 }
    return ($usable | Measure-Object -Average).Average
}

function Get-TaggedRows {
    param([string]$Text, [string]$Tag)
    $rows = @()
    $pattern = "(?m)^\[" + [regex]::Escape($Tag) + "\]\s.*$"
    foreach ($match in [regex]::Matches($Text, $pattern)) {
        $line = $match.Value
        $dict = @{ rawLine = $line }
        foreach ($kv in [regex]::Matches($line, "(?<key>[A-Za-z0-9_.]+)=(?<value>\S+)")) {
            $dict[$kv.Groups["key"].Value] = $kv.Groups["value"].Value
        }
        $rows += [pscustomobject]$dict
    }
    return $rows
}

function Assert-Link60 {
    param($Summary, [string]$CaseName)
    foreach ($check in @(
        @{ Name = "sentFps"; Value = [double]$Summary.sentFps },
        @{ Name = "udpFps"; Value = [double]$Summary.udpFps },
        @{ Name = "renderFps"; Value = [double]$Summary.renderFps },
        @{ Name = "outputFps"; Value = [double]$Summary.outputFps },
        @{ Name = "videoDisplayFps"; Value = [double]$Summary.videoDisplayFps }
    )) {
        if ($check.Value -lt 59.5) {
            throw "Phase5A $CaseName failed: $($check.Name)=$($check.Value) < 59.5"
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        throw "Phase5A $CaseName failed: latencyAvgMs=$($Summary.latencyAvgMs) > 80"
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        throw "Phase5A $CaseName failed: recorderDroppedFrames=$($Summary.recorderDroppedFrames)"
    }
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        throw "Phase5A $CaseName failed: sourceSeqContinuous=$($Summary.sourceSeqContinuous)"
    }
}

function Format-CaseToken {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $Value)).Replace(".", "p")
}

function Export-RepresentativeFrame {
    param($Summary, [double]$AltitudeKm, [double]$Mach, [string]$Apply, [string]$UseSensor, [int]$SourceSeq)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) { return "" }
    $seekSec = [math]::Max(1.0, [math]::Min([double]$Seconds - 1.0, [double]$Seconds / 2.0))
    $frameName = "alt{0}km_mach{1}_apply{2}_sensorDisplay{3}_sourceSeq{4}.png" -f `
        (Format-CaseToken $AltitudeKm), (Format-CaseToken $Mach), $Apply, $UseSensor, $SourceSeq
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

function Invoke-Case {
    param([double]$AltitudeKm, [double]$Mach, [string]$Apply, [string]$UseSensor)
    $caseName = "alt=${AltitudeKm}km mach=$Mach apply=$Apply sensorDisplay=$UseSensor"
    Write-Host "Running Phase5A $caseName seconds=$Seconds ..."
    $altText = [string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $AltitudeKm)
    $machText = [string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:R}", $Mach)
    $extraArgs = @(
        "--phase4c-aero-mach",
        "--aero-alt-km=$altText",
        "--aero-mach=$machText",
        "--duration-sec=$Seconds"
    )
    $runOutput = & $runner -Seconds $Seconds `
        -ApplyAeroToRadiance $Apply `
        -AeroApplyScale 0.25 `
        -AeroApplyClampBodyDeltaK 40.0 `
        -AeroApplyOnlyBand "MWIR" `
        -AeroDebugLog "true" `
        -Stage5LogComponents "true" `
        -Stage5ComponentLogEveryFrames 120 `
        -UseSensorInputForDisplay $UseSensor `
        -SensorInputDisplayScale 1.0 `
        -SensorInputDisplayOffset 0.0 `
        -SensorInputDisplayClampMin 0.0 `
        -SensorInputDisplayClampMax 1.0 `
        -StimExtraArgs $extraArgs
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase5A $caseName did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $caseName

    $stimText = Get-Content -LiteralPath (Join-Path $summary.logDir "stim.err.log") -Raw
    $hwaText = Get-Content -LiteralPath (Join-Path $summary.logDir "hwa.out.log") -Raw
    $sendRows = @(Get-TaggedRows $stimText "AeroSpeedSend" | Where-Object { [string]$_.phase4cAeroMach -eq "1" })
    $componentRows = @(Get-TaggedRows $hwaText "Stage5 RadianceComponents" | Where-Object {
        (Convert-ToDouble $_.mach) -gt 0.0 -and (Convert-ToDouble $_.tauUp) -gt 0.0
    })
    $aeroRows = @(Get-TaggedRows $hwaText "Stage5 AeroThermal" | Where-Object {
        (Convert-ToDouble $_.mach) -gt 0.0 -and (Convert-ToDouble $_.tauUp) -gt 0.0
    })
    $rowsForMetrics = if ($componentRows.Count -gt 0) { $componentRows } else { $aeroRows }
    if ($sendRows.Count -eq 0) {
        throw "Phase5A $caseName failed: no phase4c [AeroSpeedSend] rows"
    }
    if ($rowsForMetrics.Count -eq 0) {
        throw "Phase5A $caseName failed: no Stage5 sensor-input metrics"
    }

    $machMeasured = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.mach }) -PositiveOnly
    $machDeviation = if ($Mach -gt 0.0) { [math]::Abs($machMeasured - $Mach) / $Mach } else { 0.0 }
    if ($machDeviation -gt 0.05) {
        throw "Phase5A $caseName failed: machMeasured=$machMeasured deviates from command=$Mach"
    }
    $tauAvg = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.tauUp }) -PositiveOnly
    if ($tauAvg -le 0.0) {
        throw "Phase5A $caseName failed: tauUp average is zero"
    }
    $sensorNo = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.sensorInputNoAero }) -PositiveOnly
    $sensorWith = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.sensorInputWithAero }) -PositiveOnly
    $surfaceNo = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.surfaceRadianceNoAero }) -PositiveOnly
    $surfaceWith = Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.surfaceRadianceWithAero }) -PositiveOnly
    if ($Apply -eq "true" -and $surfaceWith + 1.0e-9 -lt $surfaceNo) {
        throw "Phase5A $caseName failed: surfaceRadianceWithAero < surfaceRadianceNoAero"
    }
    if ($Apply -eq "true" -and $sensorWith + 1.0e-9 -lt $sensorNo) {
        throw "Phase5A $caseName failed: sensorInputWithAero < sensorInputNoAero"
    }

    $sourceSeqForFrame = [int](Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.sourceSeq }) -PositiveOnly)
    $framePath = Export-RepresentativeFrame -Summary $summary -AltitudeKm $AltitudeKm -Mach $Mach -Apply $Apply -UseSensor $UseSensor -SourceSeq $sourceSeqForFrame
    $firstMetric = $rowsForMetrics | Select-Object -First 1
    $firstSend = $sendRows | Select-Object -First 1
    $sensorInputRatio = 1.0
    if ($sensorNo -gt 1.0e-12) {
        $sensorInputRatio = $sensorWith / $sensorNo
    }

    return [pscustomobject]@{
        altitudeKm = $AltitudeKm
        machCommand = $Mach
        speedKmh = [math]::Round((Convert-ToDouble $firstSend.speedKmh), 6)
        machMeasured = [math]::Round($machMeasured, 6)
        applyAero = $Apply
        useSensorInputForDisplay = $UseSensor
        bodyRadianceNoAero = [math]::Round([double]$summary.bodyRadianceNoAeroAvg, 6)
        bodyRadianceWithAero = [math]::Round([double]$summary.bodyRadianceWithAeroAvg, 6)
        surfaceRadianceNoAero = [math]::Round($surfaceNo, 6)
        surfaceRadianceWithAero = [math]::Round($surfaceWith, 6)
        tauUp = [math]::Round($tauAvg, 6)
        tauUpSource = [string]$firstMetric.tauUpSource
        tauFallbackReason = [string]$firstMetric.tauFallbackReason
        pathRadiance = [math]::Round((Get-Average @($rowsForMetrics | ForEach-Object { Convert-ToDouble $_.pathRadiance }) -PositiveOnly), 6)
        sensorInputNoAero = [math]::Round($sensorNo, 6)
        sensorInputWithAero = [math]::Round($sensorWith, 6)
        sensorInputRatio = [math]::Round($sensorInputRatio, 6)
        displayPreviewNoAero = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.displayPreviewNoAero }) -PositiveOnly), 6)
        displayPreviewWithAero = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.displayPreviewWithAero }) -PositiveOnly), 6)
        sensorInputDisplayGray = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.sensorInputDisplayGray }) -PositiveOnly), 6)
        sentFps = $summary.sentFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        stage5AeroThermalMs = $summary.stage5AeroThermalMs
        stage5RadianceComponentMs = $summary.stage5RadianceComponentMs
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
        representativeFrame = $framePath
    }
}

$altitudes = @(3.0, 10.0, 20.0)
$machs = @(0.5, 1.0, 2.0, 3.0)
$modes = @(
    @{ Apply = "false"; Sensor = "false" },
    @{ Apply = "true"; Sensor = "false" },
    @{ Apply = "true"; Sensor = "true" }
)
$rows = New-Object System.Collections.Generic.List[object]
foreach ($alt in $altitudes) {
    foreach ($mach in $machs) {
        foreach ($mode in $modes) {
            $rows.Add((Invoke-Case -AltitudeKm $alt -Mach $mach -Apply $mode.Apply -UseSensor $mode.Sensor)) | Out-Null
        }
    }
}

foreach ($alt in $altitudes) {
    $series = @($rows | Where-Object {
        [double]$_.altitudeKm -eq $alt -and $_.applyAero -eq "true" -and $_.useSensorInputForDisplay -eq "false"
    } | Sort-Object {[double]$_.machCommand})
    for ($i = 1; $i -lt $series.Count; ++$i) {
        if ([double]$series[$i].sensorInputRatio -lt [double]$series[$i - 1].sensorInputRatio) {
            throw "Phase5A trend failed: altitude=$alt sensorInputRatio is not increasing with Mach"
        }
    }
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath
Write-Output "summaryCsv=$OutputPath"
Write-Output "frameDir=$FrameDir"
$rows | Format-Table altitudeKm,machCommand,applyAero,useSensorInputForDisplay,tauUp,tauUpSource,sensorInputRatio,outputFps,displayFps,latencyAvgMs -AutoSize
