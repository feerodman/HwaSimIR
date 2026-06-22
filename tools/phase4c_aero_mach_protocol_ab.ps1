param(
    [int]$Seconds = 8,
    [string]$OutputPath = "",
    [string]$FrameDir = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\aero_mach_protocol_ab_summary.csv"
}
if ([string]::IsNullOrWhiteSpace($FrameDir)) {
    $FrameDir = Join-Path $root.Path "logs\stage5\phase4c_frames"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $FrameDir | Out-Null
$runner = Join-Path $PSScriptRoot "phase2a_sync60_save_smoke.ps1"

function Convert-ToDouble {
    param($Value)
    if ($null -eq $Value) { return 0.0 }
    $text = [string]$Value
    $parsed = 0.0
    if ([double]::TryParse($text, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
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

function Get-NumericValues {
    param([string]$Text, [string]$Tag, [string]$Field)
    $number = "([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)"
    $pattern = "(?m)^\[" + [regex]::Escape($Tag) + "\].*?\b" +
        [regex]::Escape($Field) + "=" + $number
    return @([regex]::Matches($Text, $pattern) | ForEach-Object {
        Convert-ToDouble $_.Groups[1].Value
    })
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
    $checks = @(
        @{ Name = "sentFps"; Value = [double]$Summary.sentFps; Min = 59.5 },
        @{ Name = "udpFps"; Value = [double]$Summary.udpFps; Min = 59.5 },
        @{ Name = "renderFps"; Value = [double]$Summary.renderFps; Min = 59.5 },
        @{ Name = "outputFps"; Value = [double]$Summary.outputFps; Min = 59.5 },
        @{ Name = "videoDisplayFps"; Value = [double]$Summary.videoDisplayFps; Min = 59.5 }
    )
    foreach ($check in $checks) {
        if ($check.Value -lt $check.Min) {
            throw "Phase4C $CaseName failed: $($check.Name)=$($check.Value) < $($check.Min)"
        }
    }
    if ([double]$Summary.latencyAvgMs -gt 80.0) {
        throw "Phase4C $CaseName failed: latencyAvgMs=$($Summary.latencyAvgMs) > 80"
    }
    if ([int]$Summary.recorderDroppedFrames -ne 0) {
        throw "Phase4C $CaseName failed: recorderDroppedFrames=$($Summary.recorderDroppedFrames)"
    }
    if ([int]$Summary.sourceSeqContinuous -ne 1) {
        throw "Phase4C $CaseName failed: sourceSeqContinuous=$($Summary.sourceSeqContinuous)"
    }
}

function Get-IsaAirTempK {
    param([double]$AltitudeM)
    $alt = [math]::Max(0.0, [math]::Min(20000.0, $AltitudeM))
    if ($alt -le 11000.0) {
        return 288.15 - 0.0065 * $alt
    }
    return 216.65
}

function Get-SpeedOfSoundMps {
    param([double]$AirTempK)
    return [math]::Sqrt(1.4 * 287.05287 * [math]::Max(120.0, [math]::Min(400.0, $AirTempK)))
}

function Format-CaseToken {
    param([double]$Value)
    return ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $Value)).Replace(".", "p")
}

function Export-RepresentativeFrame {
    param($Summary, [double]$AltitudeKm, [double]$Mach, [string]$Apply, [int]$SourceSeq)
    if ([string]::IsNullOrWhiteSpace([string]$Summary.outputMp4) -or -not (Test-Path -LiteralPath $Summary.outputMp4)) {
        return ""
    }
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if (-not $ffmpeg) {
        return ""
    }
    $seekSec = [math]::Max(1.0, [math]::Min([double]$Seconds - 1.0, [double]$Seconds / 2.0))
    $frameName = "alt{0}km_mach{1}_apply{2}_sourceSeq{3}.png" -f `
        (Format-CaseToken $AltitudeKm), (Format-CaseToken $Mach), $Apply, $SourceSeq
    $framePath = Join-Path $FrameDir $frameName
    & $ffmpeg.Source -y -loglevel error -ss ([string]::Format([Globalization.CultureInfo]::InvariantCulture, "{0:0.###}", $seekSec)) `
        -i $Summary.outputMp4 -frames:v 1 $framePath
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $framePath)) {
        return ""
    }
    return $framePath
}

function Invoke-Case {
    param([double]$AltitudeKm, [double]$Mach, [string]$Apply)
    $caseName = "alt=${AltitudeKm}km mach=$Mach apply=$Apply"
    Write-Host "Running Phase4C $caseName seconds=$Seconds ..."
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
        -StimExtraArgs $extraArgs
    $summaryLine = @($runOutput | Where-Object { $_ -match "^summary=" } | Select-Object -Last 1)
    if ($summaryLine.Count -eq 0) {
        $runOutput | Out-String | Write-Host
        throw "Phase4C $caseName did not produce a summary path"
    }
    $summaryPath = $summaryLine[-1].Substring("summary=".Length)
    $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
    Assert-Link60 -Summary $summary -CaseName $caseName

    $stimText = Get-Content -LiteralPath (Join-Path $summary.logDir "stim.err.log") -Raw
    $hwaText = Get-Content -LiteralPath (Join-Path $summary.logDir "hwa.out.log") -Raw
    $sendRows = @(Get-TaggedRows $stimText "AeroSpeedSend" | Where-Object { [string]$_.phase4cAeroMach -eq "1" })
    $aeroRows = @(Get-TaggedRows $hwaText "Stage5 AeroThermal" | Where-Object {
        (Convert-ToDouble $_.speedRawKmh) -gt 0.0 -and (Convert-ToDouble $_.mach) -gt 0.0
    })
    if ($sendRows.Count -eq 0) {
        throw "Phase4C $caseName failed: no phase4c [AeroSpeedSend] rows"
    }
    if ($aeroRows.Count -eq 0) {
        throw "Phase4C $caseName failed: no usable [Stage5 AeroThermal] rows"
    }

    $airTempK = Get-IsaAirTempK ($AltitudeKm * 1000.0)
    $commandSpeedMps = $Mach * (Get-SpeedOfSoundMps $airTempK)
    $commandSpeedKmh = $commandSpeedMps * 3.6
    $sendSpeedKmh = Get-Average @($sendRows | ForEach-Object { Convert-ToDouble $_.speedKmh }) -PositiveOnly
    $machMeasured = Get-Average @($aeroRows | ForEach-Object { Convert-ToDouble $_.mach }) -PositiveOnly
    $machDeviation = if ($Mach -gt 0.0) { [math]::Abs($machMeasured - $Mach) / $Mach } else { 0.0 }
    if ($machDeviation -gt 0.05) {
        throw "Phase4C $caseName failed: machMeasured=$machMeasured deviates from command=$Mach by $([math]::Round($machDeviation * 100.0, 3))%"
    }

    $sourceSeqForFrame = [int](Get-Average @($aeroRows | ForEach-Object { Convert-ToDouble $_.sourceSeq }) -PositiveOnly)
    $framePath = Export-RepresentativeFrame -Summary $summary -AltitudeKm $AltitudeKm -Mach $Mach -Apply $Apply -SourceSeq $sourceSeqForFrame

    return [pscustomobject]@{
        altitudeKm = $AltitudeKm
        machCommand = $Mach
        speedKmh = [math]::Round($commandSpeedKmh, 6)
        sendSpeedKmhAvg = [math]::Round($sendSpeedKmh, 6)
        machMeasured = [math]::Round($machMeasured, 6)
        machDeviationPct = [math]::Round($machDeviation * 100.0, 3)
        applyAero = $Apply
        bodyAeroDeltaKRaw = [math]::Round([double]$summary.bodyAeroDeltaKRawAvg, 6)
        bodyAeroDeltaKEffective = [math]::Round([double]$summary.bodyAeroDeltaKEffectiveAvg, 6)
        bodyRadianceNoAero = [math]::Round([double]$summary.bodyRadianceNoAeroAvg, 6)
        bodyRadianceWithAero = [math]::Round([double]$summary.bodyRadianceWithAeroAvg, 6)
        sensorInputNoAero = [math]::Round((Get-Average (Get-NumericValues $hwaText "Stage5 RadianceComponents" "sensorInputNoAero") -PositiveOnly), 6)
        sensorInputWithAero = [math]::Round((Get-Average (Get-NumericValues $hwaText "Stage5 RadianceComponents" "sensorInputWithAero") -PositiveOnly), 6)
        aeroRadianceRatio = [math]::Round([double]$summary.aeroRadianceRatioAvg, 6)
        sentFps = $summary.sentFps
        udpFps = $summary.udpFps
        renderFps = $summary.renderFps
        outputFps = $summary.outputFps
        displayFps = $summary.videoDisplayFps
        latencyAvgMs = $summary.latencyAvgMs
        sourceSeqContinuous = $summary.sourceSeqContinuous
        recordingDroppedFrames = $summary.recorderDroppedFrames
        stage5AeroThermalMs = $summary.stage5AeroThermalMs
        irUpdateMs = $summary.irUpdateMsAvg
        stage5ModtranLookupMs = $summary.stage5ModtranLookupMs
        logDir = $summary.logDir
        outputMp4 = $summary.outputMp4
        representativeFrame = $framePath
    }
}

$altitudes = @(3.0, 10.0, 20.0)
$machs = @(0.5, 1.0, 2.0, 3.0)
$rows = New-Object System.Collections.Generic.List[object]
foreach ($alt in $altitudes) {
    foreach ($mach in $machs) {
        foreach ($apply in @("false", "true")) {
            $rows.Add((Invoke-Case -AltitudeKm $alt -Mach $mach -Apply $apply)) | Out-Null
        }
    }
}

foreach ($alt in $altitudes) {
    $series = @($rows | Where-Object { [double]$_.altitudeKm -eq $alt -and $_.applyAero -eq "true" } | Sort-Object {[double]$_.machCommand})
    for ($i = 1; $i -lt $series.Count; ++$i) {
        if ([double]$series[$i].bodyAeroDeltaKEffective -lt [double]$series[$i - 1].bodyAeroDeltaKEffective) {
            throw "Phase4C trend failed: altitude=$alt bodyAeroDeltaKEffective is not increasing with Mach"
        }
    }
}

$applyTrueRows = @($rows | Where-Object { $_.applyAero -eq "true" })
foreach ($row in $applyTrueRows) {
    if ([double]$row.bodyRadianceWithAero -lt [double]$row.bodyRadianceNoAero) {
        throw "Phase4C radiance failed: apply=true withAero < noAero at altitude=$($row.altitudeKm) mach=$($row.machCommand)"
    }
    if ([double]$row.sensorInputWithAero -lt [double]$row.sensorInputNoAero) {
        throw "Phase4C sensor input failed: apply=true withAero < noAero at altitude=$($row.altitudeKm) mach=$($row.machCommand)"
    }
}

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8
$rows | Format-Table altitudeKm, machCommand, applyAero, speedKmh, machMeasured, bodyAeroDeltaKEffective, aeroRadianceRatio, outputFps, displayFps, latencyAvgMs -AutoSize
Write-Host "Phase4C aero Mach protocol A/B passed: $OutputPath"
Write-Host "Representative frames: $FrameDir"
