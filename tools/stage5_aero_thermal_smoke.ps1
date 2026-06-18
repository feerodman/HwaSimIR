param(
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root.Path "logs\stage5\aero_thermal_summary.csv"
}
$outDir = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$gamma = 1.4
$recoveryFactor = 0.85
$bodyCoeff = 0.20
$noseCoeff = 0.60
$edgeCoeff = 0.45
$rearCoeff = 0.10
$deltaMax = 250.0
$gasConstantDryAir = 287.05287

function Clamp-Double {
    param([double]$Value, [double]$Low, [double]$High)
    return [math]::Max($Low, [math]::Min($High, $Value))
}

function Get-IsaAirTempK {
    param([double]$AltitudeM)
    $alt = Clamp-Double $AltitudeM 0.0 20000.0
    if ($alt -le 11000.0) {
        return 288.15 - 0.0065 * $alt
    }
    return 216.65
}

function Get-SpeedOfSoundMps {
    param([double]$AirTempK)
    return [math]::Sqrt($gamma * $gasConstantDryAir * (Clamp-Double $AirTempK 120.0 400.0))
}

function New-AeroRow {
    param(
        [string]$CaseName,
        [double]$AltitudeM,
        [double]$Mach,
        [bool]$ExpectValid = $true
    )
    if (-not $ExpectValid -or $AltitudeM -lt -500.0 -or $AltitudeM -gt 100000.0 -or $Mach -lt 0.0) {
        return [pscustomobject]@{
            caseName = $CaseName
            altitudeM = $AltitudeM
            mach = $Mach
            speedMps = 0.0
            speedRawKmh = 0.0
            airTempK = 0.0
            speedOfSoundMps = 0.0
            recoveryTempK = 0.0
            aeroDeltaK = 0.0
            bodyAeroDeltaK = 0.0
            noseAeroDeltaK = 0.0
            edgeAeroDeltaK = 0.0
            rearAeroDeltaK = 0.0
            valid = $false
            fallbackReason = "out_of_range"
        }
    }

    $airTempK = Get-IsaAirTempK $AltitudeM
    $speedOfSound = Get-SpeedOfSoundMps $airTempK
    $speedMps = $Mach * $speedOfSound
    $recoveryTempK = $airTempK * (1.0 + $recoveryFactor * ($gamma - 1.0) * 0.5 * $Mach * $Mach)
    $aeroDeltaK = Clamp-Double ([math]::Max(0.0, $recoveryTempK - $airTempK)) 0.0 $deltaMax
    [pscustomobject]@{
        caseName = $CaseName
        altitudeM = $AltitudeM
        mach = $Mach
        speedMps = [math]::Round($speedMps, 6)
        speedRawKmh = [math]::Round($speedMps * 3.6, 6)
        airTempK = [math]::Round($airTempK, 6)
        speedOfSoundMps = [math]::Round($speedOfSound, 6)
        recoveryTempK = [math]::Round($recoveryTempK, 6)
        aeroDeltaK = [math]::Round($aeroDeltaK, 6)
        bodyAeroDeltaK = [math]::Round((Clamp-Double ($aeroDeltaK * $bodyCoeff) 0.0 $deltaMax), 6)
        noseAeroDeltaK = [math]::Round((Clamp-Double ($aeroDeltaK * $noseCoeff) 0.0 $deltaMax), 6)
        edgeAeroDeltaK = [math]::Round((Clamp-Double ($aeroDeltaK * $edgeCoeff) 0.0 $deltaMax), 6)
        rearAeroDeltaK = [math]::Round((Clamp-Double ($aeroDeltaK * $rearCoeff) 0.0 $deltaMax), 6)
        valid = $true
        fallbackReason = "none"
    }
}

$altitudes = @(3000.0, 10000.0, 20000.0)
$machs = @(0.5, 1.0, 2.0, 3.0)
$rows = New-Object System.Collections.Generic.List[object]
foreach ($alt in $altitudes) {
    foreach ($mach in $machs) {
        $rows.Add((New-AeroRow -CaseName "grid" -AltitudeM $alt -Mach $mach)) | Out-Null
    }
}
$rows.Add((New-AeroRow -CaseName "invalidAltitude" -AltitudeM -1000.0 -Mach 1.0 -ExpectValid $false)) | Out-Null
$rows.Add((New-AeroRow -CaseName "invalidMach" -AltitudeM 3000.0 -Mach -1.0 -ExpectValid $false)) | Out-Null

$rows | Export-Csv -LiteralPath $OutputPath -NoTypeInformation -Encoding UTF8

$gridRows = @($rows | Where-Object { $_.caseName -eq "grid" })
foreach ($alt in $altitudes) {
    $series = @($gridRows | Where-Object { [double]$_.altitudeM -eq $alt } | Sort-Object {[double]$_.mach})
    for ($i = 1; $i -lt $series.Count; ++$i) {
        if ([double]$series[$i].aeroDeltaK -le [double]$series[$i - 1].aeroDeltaK) {
            throw "Aero trend failed: altitude=$alt Mach delta is not increasing"
        }
    }
}
if (([double]($gridRows | Where-Object { $_.altitudeM -eq 3000.0 -and $_.mach -eq 0.5 }).aeroDeltaK) -ge
    ([double]($gridRows | Where-Object { $_.altitudeM -eq 3000.0 -and $_.mach -eq 3.0 }).aeroDeltaK)) {
    throw "Aero trend failed: Mach 3 should heat more than Mach 0.5"
}
if (-not ($rows | Where-Object { $_.caseName -like "invalid*" -and -not $_.valid -and $_.fallbackReason -eq "out_of_range" })) {
    throw "Aero fallback failed: invalid cases did not report out_of_range"
}

$gridRows | Format-Table caseName, altitudeM, mach, speedMps, airTempK, speedOfSoundMps, aeroDeltaK, bodyAeroDeltaK, noseAeroDeltaK, edgeAeroDeltaK, rearAeroDeltaK -AutoSize
Write-Host "Stage5 aero thermal smoke passed: $OutputPath"
