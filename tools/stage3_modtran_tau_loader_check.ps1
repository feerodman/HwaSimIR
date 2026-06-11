param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$bandLutPath = Join-Path $rootPath "HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut.csv"
$legacyTransmittancePath = Join-Path $rootPath "transmittance\transmittance_0.3_15.txt"
$loaderHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRModtranTauLut.h"
$loaderSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRModtranTauLut.cpp"
$atmosphereHeader = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.h"
$atmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"
$cmakePath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\CMakeLists.txt"
$vcxprojPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj"
$filtersPath = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj.filters"
$debugSmokePath = Join-Path $rootPath "tools\stage3_modtran_tau_debug_smoke.ps1"
$activeSmokePath = Join-Path $rootPath "tools\stage3_modtran_tau_active_smoke.ps1"
$abSmokePath = Join-Path $rootPath "tools\stage3_modtran_tau_ab_smoke.ps1"

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail
    )

    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Read-Header {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return @()
    }
    $first = Get-Content -LiteralPath $Path -TotalCount 1 -Encoding UTF8
    if ($null -eq $first) {
        return @()
    }
    return @($first -split ',' | ForEach-Object { $_.Trim().Trim('"').Trim([char]0xFEFF) })
}

function Has-AllColumns {
    param(
        [string[]]$Actual,
        [string[]]$Expected
    )
    foreach ($name in $Expected) {
        if ($Actual -notcontains $name) {
            return $false
        }
    }
    return $true
}

function Test-TextContains {
    param(
        [string]$Path,
        [string]$Pattern
    )
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    $text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    return ($text -match $Pattern)
}

function Get-TauStats {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [PSCustomObject]@{ rows = 0; bad_tau = 0; min_tau = $null; max_tau = $null }
    }

    $code = @'
import csv
import json
import math
import sys

path = sys.argv[1]
stats = {
    'rows': 0,
    'bad_tau': 0,
    'min_tau': None,
    'max_tau': None,
}

def update(value):
    if value is None or value == '':
        return
    try:
        number = float(value)
    except ValueError:
        stats['bad_tau'] += 1
        return
    if not math.isfinite(number) or number < 0.0 or number > 1.0:
        stats['bad_tau'] += 1
        return
    stats['min_tau'] = number if stats['min_tau'] is None else min(stats['min_tau'], number)
    stats['max_tau'] = number if stats['max_tau'] is None else max(stats['max_tau'], number)

with open(path, 'r', newline='', encoding='utf-8-sig') as handle:
    reader = csv.DictReader(handle)
    for row in reader:
        stats['rows'] += 1
        update(row.get('tau_up_band'))
        update(row.get('tau_down_band'))

print(json.dumps(stats, sort_keys=True))
'@
    $json = & python -c $code $Path
    if ($LASTEXITCODE -ne 0) {
        throw "failed to inspect tau values in $Path"
    }
    return ($json | ConvertFrom-Json)
}

$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "band_lut.csv exists" (Test-Path -LiteralPath $bandLutPath -PathType Leaf) $bandLutPath)) | Out-Null
$header = Read-Header $bandLutPath
$requiredBandColumns = @(
    "band",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "tau_up_band",
    "tau_down_band"
)
$checks.Add((Add-Check "band_lut.csv tau/key columns" (Has-AllColumns $header $requiredBandColumns) ($requiredBandColumns -join ", "))) | Out-Null

$tauStats = Get-TauStats $bandLutPath
$checks.Add((Add-Check "band_lut.csv row count" ([int]$tauStats.rows -gt 0) "rows=$($tauStats.rows)")) | Out-Null
$checks.Add((Add-Check "tau range 0..1" ([int]$tauStats.bad_tau -eq 0) "bad_tau=$($tauStats.bad_tau), min=$($tauStats.min_tau), max=$($tauStats.max_tau)")) | Out-Null

$checks.Add((Add-Check "legacy transmittance fallback exists" (Test-Path -LiteralPath $legacyTransmittancePath -PathType Leaf) $legacyTransmittancePath)) | Out-Null
$checks.Add((Add-Check "loader header exists" (Test-Path -LiteralPath $loaderHeader -PathType Leaf) $loaderHeader)) | Out-Null
$checks.Add((Add-Check "loader source exists" (Test-Path -LiteralPath $loaderSource -PathType Leaf) $loaderSource)) | Out-Null
$checks.Add((Add-Check "loader in CMake" (Test-TextContains $cmakePath "IR/IRModtranTauLut\.cpp") $cmakePath)) | Out-Null
$checks.Add((Add-Check "loader in VS project" ((Test-TextContains $vcxprojPath "IR\\IRModtranTauLut\.cpp") -and (Test-TextContains $vcxprojPath "IR\\IRModtranTauLut\.h")) $vcxprojPath)) | Out-Null
$checks.Add((Add-Check "loader in VS filters" ((Test-TextContains $filtersPath "IR\\IRModtranTauLut\.cpp") -and (Test-TextContains $filtersPath "IR\\IRModtranTauLut\.h")) $filtersPath)) | Out-Null

$checks.Add((Add-Check "IRAtmosphereModel exposes MODTRAN tau debug" ((Test-TextContains $atmosphereHeader "loadModtranBandLut") -and (Test-TextContains $atmosphereHeader "setModtranTauDebugEnabled") -and (Test-TextContains $atmosphereHeader "setUseModtranTauForAtmosphere")) $atmosphereHeader)) | Out-Null
$checks.Add((Add-Check "EnableModtranTauDebug default off" ((Test-TextContains $atmosphereSource "m_modtranTauDebugEnabled\(false\)") -and (Test-TextContains $appSource "EnableModtranTauDebug") -and (Test-TextContains $appSource 'getBool\("Stage3",\s*"EnableModtranTauDebug",\s*"EnableModtranTauDebug",\s*false')) "$atmosphereSource; $appSource")) | Out-Null
$checks.Add((Add-Check "UseModtranTauForAtmosphere default off" ((Test-TextContains $atmosphereSource "m_useModtranTauForAtmosphere\(false\)") -and (Test-TextContains $appSource "UseModtranTauForAtmosphere") -and (Test-TextContains $appSource 'getBool\("Stage3",\s*"UseModtranTauForAtmosphere",\s*"UseModtranTauForAtmosphere",\s*false')) "$atmosphereSource; $appSource")) | Out-Null
$checks.Add((Add-Check "runtime debug smoke script exists" (Test-Path -LiteralPath $debugSmokePath -PathType Leaf) $debugSmokePath)) | Out-Null
$checks.Add((Add-Check "runtime active smoke script exists" (Test-Path -LiteralPath $activeSmokePath -PathType Leaf) $activeSmokePath)) | Out-Null
$checks.Add((Add-Check "runtime A/B smoke script exists" (Test-Path -LiteralPath $abSmokePath -PathType Leaf) $abSmokePath)) | Out-Null

$forbiddenFieldPattern = "path_radiance_band|sky_radiance_band|path_scattering_radiance_band|solar_irradiance_band"
$loaderText = ""
if (Test-Path -LiteralPath $loaderSource -PathType Leaf) {
    $loaderText = Get-Content -LiteralPath $loaderSource -Raw -Encoding UTF8
}
$atmosphereText = ""
if (Test-Path -LiteralPath $atmosphereSource -PathType Leaf) {
    $atmosphereText = Get-Content -LiteralPath $atmosphereSource -Raw -Encoding UTF8
}
$appText = ""
if (Test-Path -LiteralPath $appSource -PathType Leaf) {
    $appText = Get-Content -LiteralPath $appSource -Raw -Encoding UTF8
}
$forbiddenInLoader = ($loaderText -match $forbiddenFieldPattern) -or ($atmosphereText -match $forbiddenFieldPattern) -or ($appText -match $forbiddenFieldPattern)
$checks.Add((Add-Check "no path/sky/solar radiance fields in tau loader" (-not $forbiddenInLoader) "checked IRModtranTauLut.cpp, IRSimulation.cpp, HwaSimIR.cpp")) | Out-Null

$debugPrintsOnly = (
    ($atmosphereText -match "MODTRAN Tau Debug") -and
    ($atmosphereText -match "old_tau=") -and
    ($atmosphereText -match "new_tau=") -and
    ($atmosphereText -match "diff=") -and
    ($atmosphereText -match "band_lut\.csv")
)
$checks.Add((Add-Check "debug code prints old/new tau comparison" $debugPrintsOnly "source/tau_up/tau_down/old_tau/new_tau/diff")) | Out-Null

$inactiveLegacyPath = (
    ($atmosphereText -match "m_useModtranTauForAtmosphere\(false\)") -and
    ($atmosphereText -match "returnedTau\s*=\s*useModtranTau\s*\?\s*result\.tauUp\s*:\s*legacyTau") -and
    ($atmosphereText -match "return\s+returnedTau;")
)
$checks.Add((Add-Check "inactive path keeps legacy tau" $inactiveLegacyPath "UseModtranTauForAtmosphere=0 selects legacyTau")) | Out-Null

$activeBandGate = (
    ($atmosphereText -match "query\.band\s*==\s*IRBand::NearInfrared") -and
    ($atmosphereText -match "query\.band\s*==\s*IRBand::MidWaveInfrared") -and
    ($atmosphereText -match "m_useModtranTauForAtmosphere\s*&&\s*supportedBand\s*&&\s*result\.found") -and
    ($atmosphereText -notmatch "return\s+result\.tauDown")
)
$checks.Add((Add-Check "VIS/SWIR/LWIR fallback legacy gate" $activeBandGate "active gate requires NIR/MWIR and never returns tau_down")) | Out-Null

$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage3 MODTRAN tau-only loader check failed:" -ForegroundColor Red
    $failed | Format-List
    if ($Strict) {
        exit 1
    }
}
else {
    Write-Host ""
    Write-Host "Stage3 MODTRAN tau-only loader check passed." -ForegroundColor Green
}
