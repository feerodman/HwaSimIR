param(
    [int]$Seconds = 6,
    [string]$OutputPath = "",
    [string]$FrameDir = "",
    [double]$SensorInputDisplayScale = 1.0,
    [double]$SensorInputDisplayOffset = 0.0,
    [double]$SensorInputDisplayGamma = 1.0,
    [switch]$Quick
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\sensor_display_calib_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage5\phase5b_frames"
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
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

function Get-Median {
    param([double[]]$Values)
    $usable = @($Values | Where-Object { -not [double]::IsNaN($_) -and -not [double]::IsInfinity($_) -and $_ -gt 0.0 } | Sort-Object)
    if ($usable.Count -eq 0) { return 0.0 }
    $mid = [int]($usable.Count / 2)
    if (($usable.Count % 2) -eq 1) { return [double]$usable[$mid] }
    return ([double]$usable[$mid - 1] + [double]$usable[$mid]) / 2.0
}

function Select-Range {
    param([double[]]$Values, [double]$Min = [double]::NegativeInfinity, [double]$Max = [double]::PositiveInfinity, [switch]$PositiveOnly)
    $usable = @()
    foreach ($value in $Values) {
        if ([double]::IsNaN($value) -or [double]::IsInfinity($value)) { continue }
        if ($PositiveOnly -and $value -le 0.0) { continue }
        if ($value -lt $Min -or $value -gt $Max) { continue }
        $usable += $value
    }
    return $usable
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
            throw "Phase5B $CaseName failed: $($check.Name)=$($check.Value) < 59.5"
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        Write-Warning "Phase5B $CaseName short-window latencyAvgMs=$($Summary.latencyAvgMs) > 80; keeping calibration row and leaving strict latency to 30s production validation"
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        throw "Phase5B $CaseName failed: recorderDroppedFrames=$($Summary.recorderDroppedFrames)"
    }
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        throw "Phase5B $CaseName failed: sourceSeqContinuous=$($Summary.sourceSeqContinuous)"
    }
}

function Format-Token {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $Value)).Replace(".", "p")
}

function Get-VisualStatus {
    param([double]$Gray, [bool]$UseSensor)
    if (-not $UseSensor) { return "reference" }
    if ($Gray -le 0.03) { return "too_dark" }
    if ($Gray -ge 0.98) { return "overexposed" }
    if ($Gray -ge 0.85) { return "too_bright" }
    return "ok"
}

function Should-ExportFrame {
    param([double]$AltitudeKm, [double]$Mach, [string]$Apply, [string]$UseSensor)
    if ($UseSensor -ne "true") {
        return ($AltitudeKm -eq 10.0 -and $Mach -eq 1.0 -and $Apply -eq "false")
    }
    if ($AltitudeKm -eq 10.0 -and $Mach -eq 1.0 -and $Apply -eq "false") { return $true }
    if ($Apply -eq "true" -and $AltitudeKm -eq 3.0 -and $Mach -eq 3.0) { return $true }
    if ($Apply -eq "true" -and $AltitudeKm -eq 10.0 -and $Mach -eq 2.0) { return $true }
    if ($Apply -eq "true" -and $AltitudeKm -eq 20.0 -and $Mach -eq 3.0) { return $true }
    if ($Apply -eq "true" -and $AltitudeKm -eq 10.0 -and $Mach -eq 3.0) { return $true }
    return $false
}

function Export-RepresentativeFrame {
    param($Summary, [double]$AltitudeKm, [double]$Mach, [string]$Apply, [string]$UseSensor, [int]$SourceSeq)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) { return "" }
    $seekSec = if ($Seconds -ge 6) { 5.5 } else { [math]::Max(1.0, [double]$Seconds / 2.0) }
    $frameName = "phase5b_alt{0}km_mach{1}_apply{2}_sensor{3}_scale{4}_offset{5}_gamma{6}_sourceSeq{7}.png" -f `
        (Format-Token $AltitudeKm), (Format-Token $Mach), $Apply, $UseSensor, `
        (Format-Token $SensorInputDisplayScale), (Format-Token $SensorInputDisplayOffset), `
        (Format-Token $SensorInputDisplayGamma), $SourceSeq
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

function Invoke-CalibCase {
    param([string]$CaseName, [double]$AltitudeKm, [double]$Mach, [string]$Apply, [string]$UseSensor)
    Write-Host "Running Phase5B $CaseName seconds=$Seconds scale=$SensorInputDisplayScale offset=$SensorInputDisplayOffset gamma=$SensorInputDisplayGamma ..."
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
        -SensorInputDisplayMode "LegacyMatch" `
        -SensorInputDisplayScale $SensorInputDisplayScale `
        -SensorInputDisplayOffset $SensorInputDisplayOffset `
        -SensorInputDisplayClampMin 0.0 `
        -SensorInputDisplayClampMax 1.0 `
        -SensorInputDisplayGamma $SensorInputDisplayGamma `
        -SensorInputDisplayBand "MWIR" `
        -StimExtraArgs $extraArgs
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase5B $CaseName did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $CaseName

    $hwaText = Get-Content -LiteralPath (Join-Path $summary.logDir "hwa.out.log") -Raw
    $componentRows = @(Get-TaggedRows $hwaText "Stage5 RadianceComponents" | Where-Object {
        [string]$_.band -eq "MWIR" -and (Convert-ToDouble $_.tauUp) -gt 0.0 -and (Convert-ToDouble $_.sensorInputRadiance) -gt 0.0
    })
    if ($componentRows.Count -eq 0) {
        throw "Phase5B $CaseName failed: no MWIR Stage5 RadianceComponents rows"
    }
    $legacyPreview = Get-Average (Select-Range @($componentRows | ForEach-Object { Convert-ToDouble $_.displayPreview }) 0.0 1.0 -PositiveOnly)
    $sensorInput = Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.sensorInputRadiance }) -PositiveOnly
    $sensorGray = Get-Average (Select-Range @($componentRows | ForEach-Object { Convert-ToDouble $_.sensorInputDisplayGray }) 0.0 1.0 -PositiveOnly)

    $quietRows = @($componentRows | Where-Object {
        (Convert-ToDouble $_.rearHotspotRadiance) -le 1.0e-6 -and
        (Convert-ToDouble $_.plumeRadiance) -le 1.0e-6 -and
        (Convert-ToDouble $_.brightspotRadiance) -le 1.0e-6 -and
        (Convert-ToDouble $_.sensorInputRadiance) -gt 1.0e-12 -and
        (Convert-ToDouble $_.sensorInputRadiance) -lt 5.0
    })
    $calibRows = if ($quietRows.Count -gt 0) { $quietRows } else { $componentRows }
    $calibSampleType = if ($quietRows.Count -gt 0) { "body_only_no_hotspot" } else { "all_components_fallback" }
    $calibLegacyPreview = Get-Average (Select-Range @($calibRows | ForEach-Object { Convert-ToDouble $_.displayPreview }) 0.0 1.0 -PositiveOnly)
    $calibSensorInput = Get-Average @($calibRows | ForEach-Object { Convert-ToDouble $_.sensorInputRadiance }) -PositiveOnly
    $calibSensorGray = Get-Average (Select-Range @($calibRows | ForEach-Object { Convert-ToDouble $_.sensorInputDisplayGray }) 0.0 1.0 -PositiveOnly)
    $recommendedScale = if ($calibSensorInput -gt 1.0e-12) { [math]::Min(1000000.0, [math]::Max(0.0, $calibLegacyPreview / $calibSensorInput)) } else { 0.0 }
    $sourceSeq = [int](Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.sourceSeq }) -PositiveOnly)
    $framePath = ""
    if (Should-ExportFrame -AltitudeKm $AltitudeKm -Mach $Mach -Apply $Apply -UseSensor $UseSensor) {
        $framePath = Export-RepresentativeFrame -Summary $summary -AltitudeKm $AltitudeKm -Mach $Mach -Apply $Apply -UseSensor $UseSensor -SourceSeq $sourceSeq
    }
    $firstMetric = $componentRows | Select-Object -First 1
    $visualStatus = Get-VisualStatus -Gray $sensorGray -UseSensor ($UseSensor -eq "true")

    return [pscustomobject]@{
        caseName = $CaseName
        band = "MWIR"
        altitudeKm = $AltitudeKm
        mach = $Mach
        applyAero = $Apply
        engineState = $(if ($Seconds -ge 6) { "mixed_after_5s" } else { "mostly_false" })
        strikeFlag = $(if ($Seconds -ge 6) { "mixed_after_5s" } else { "mostly_false" })
        strikePart = $(if ($Seconds -ge 6) { "mid_after_5s" } else { "none" })
        useSensorInputDisplay = $UseSensor
        surfaceRadiance = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.surfaceRadiance }) -PositiveOnly), 6)
        sensorInputRadiance = [math]::Round($sensorInput, 6)
        legacyDisplayPreview = [math]::Round($legacyPreview, 6)
        sensorInputDisplayGray = [math]::Round($sensorGray, 6)
        recommendedScale = [math]::Round($recommendedScale, 6)
        recommendedOffset = 0.0
        recommendedGamma = 1.0
        calibrationSampleType = $calibSampleType
        calibrationSampleCount = $calibRows.Count
        calibrationLegacyPreview = [math]::Round($calibLegacyPreview, 6)
        calibrationSensorInputRadiance = [math]::Round($calibSensorInput, 6)
        calibrationSensorInputDisplayGray = [math]::Round($calibSensorGray, 6)
        displayGrayDiff = [math]::Round(($sensorGray - $legacyPreview), 6)
        visualStatus = $visualStatus
        tauUp = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.tauUp }) -PositiveOnly), 6)
        tauUpSource = [string]$firstMetric.tauUpSource
        bodyAeroDeltaKEffective = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.bodyAeroDeltaKEffective }) -PositiveOnly), 6)
        aeroRadianceRatio = [math]::Round((Get-Average @($componentRows | ForEach-Object { Convert-ToDouble $_.aeroRadianceRatio }) -PositiveOnly), 6)
        sensorInputDisplayScale = $SensorInputDisplayScale
        sensorInputDisplayOffset = $SensorInputDisplayOffset
        sensorInputDisplayGamma = $SensorInputDisplayGamma
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
        representativeFrame = $framePath
    }
}

$altitudes = if ($Quick) { @(10.0, 20.0) } else { @(3.0, 10.0, 20.0) }
$machs = if ($Quick) { @(1.0, 3.0) } else { @(0.5, 1.0, 2.0, 3.0) }
$modes = @(
    @{ Apply = "false"; Sensor = "false" },
    @{ Apply = "false"; Sensor = "true" },
    @{ Apply = "true"; Sensor = "true" }
)

$rows = New-Object System.Collections.Generic.List[object]
foreach ($alt in $altitudes) {
    foreach ($mach in $machs) {
        foreach ($mode in $modes) {
            $caseToken = "alt{0}km_mach{1}_apply{2}_sensor{3}" -f (Format-Token $alt), (Format-Token $mach), $mode.Apply, $mode.Sensor
            $rows.Add((Invoke-CalibCase -CaseName $caseToken -AltitudeKm $alt -Mach $mach -Apply $mode.Apply -UseSensor $mode.Sensor)) | Out-Null
        }
    }
}

foreach ($alt in $altitudes) {
    $series = @($rows | Where-Object {
        [double]$_.altitudeKm -eq $alt -and $_.applyAero -eq "true" -and $_.useSensorInputDisplay -eq "true"
    } | Sort-Object {[double]$_.mach})
    for ($i = 1; $i -lt $series.Count; ++$i) {
        if ([double]$series[$i].bodyAeroDeltaKEffective + 1.0e-9 -lt [double]$series[$i - 1].bodyAeroDeltaKEffective) {
            throw "Phase5B trend failed: altitude=$alt bodyAeroDeltaKEffective decreased with Mach"
        }
    }
}

$scaleValues = @($rows | Where-Object { [double]$_.recommendedScale -gt 0.0 } | ForEach-Object { [double]$_.recommendedScale })
$globalRecommendedScale = [math]::Round((Get-Median $scaleValues), 6)
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $OutputPath

$metaPath = [IO.Path]::ChangeExtension($OutputPath, ".meta.json")
[pscustomobject]@{
    summaryCsv = $OutputPath
    frameDir = $FrameDir
    inputScale = $SensorInputDisplayScale
    inputOffset = $SensorInputDisplayOffset
    inputGamma = $SensorInputDisplayGamma
    recommendedScaleMedian = $globalRecommendedScale
    recommendedOffset = 0.0
    recommendedGamma = 1.0
    rowCount = $rows.Count
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metaPath -Encoding UTF8

Write-Output "summaryCsv=$OutputPath"
Write-Output "meta=$metaPath"
Write-Output "frameDir=$FrameDir"
Write-Output "recommendedScaleMedian=$globalRecommendedScale"
$rows | Format-Table caseName,applyAero,useSensorInputDisplay,legacyDisplayPreview,sensorInputDisplayGray,recommendedScale,visualStatus,outputFps,displayFps,latencyAvgMs -AutoSize
