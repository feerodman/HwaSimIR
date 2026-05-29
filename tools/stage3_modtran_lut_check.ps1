param(
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$modtranRoot = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN"
$processedDir = Join-Path $modtranRoot "processed"
$configPath = Join-Path $rootPath "tools\modtran\modtran_grid_nir_mwir_priority.json"

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

function Test-WorkspacePath {
    param([string]$RelativePath)
    return Test-Path -LiteralPath (Join-Path $rootPath $RelativePath)
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

function Test-Columns {
    param(
        [string[]]$Actual,
        [string[]]$Expected
    )
    foreach ($column in $Expected) {
        if ($Actual -notcontains $column) {
            return $false
        }
    }
    return $true
}

function Import-CsvIfAny {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return @()
    }
    if ((Get-Item -LiteralPath $Path).Length -eq 0) {
        return @()
    }
    return @(Import-Csv -LiteralPath $Path)
}

function Get-SpectralCsvStats {
    param(
        [string]$Path,
        [ValidateSet("path","solar")]
        [string]$Kind
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [PSCustomObject]@{
            rows           = 0
            tau_count      = 0
            bad_tau        = 0
            bad_wavelength = 0
        }
    }

    $code = @'
import csv
import json
import math
import sys

path = sys.argv[1]
kind = sys.argv[2]
stats = {
    'rows': 0,
    'tau_count': 0,
    'bad_tau': 0,
    'bad_wavelength': 0,
}

def parse_float(value):
    if value is None or value == '':
        return None
    try:
        value = float(value)
    except ValueError:
        return math.nan
    return value

with open(path, 'r', newline='', encoding='utf-8-sig') as handle:
    reader = csv.DictReader(handle)
    tau_name = 'tau_up' if kind == 'path' else 'tau_down'
    for row in reader:
        stats['rows'] += 1

        tau = parse_float(row.get(tau_name))
        if tau is not None:
            stats['tau_count'] += 1
            if not math.isfinite(tau) or tau < 0.0 or tau > 1.0:
                stats['bad_tau'] += 1

        if kind == 'path':
            wn = parse_float(row.get('wavenumber_cm'))
            wl = parse_float(row.get('wavelength_um'))
            if wn is not None and wl is not None:
                if (
                    not math.isfinite(wn)
                    or not math.isfinite(wl)
                    or wn == 0.0
                    or abs(wl - (10000.0 / wn)) > 0.0001
                ):
                    stats['bad_wavelength'] += 1

print(json.dumps(stats, sort_keys=True))
'@
    $json = & python -c $code $Path $Kind
    if ($LASTEXITCODE -ne 0) {
        throw "python CSV stats failed for $Path"
    }
    return ($json | ConvertFrom-Json)
}

$checks = New-Object System.Collections.Generic.List[object]

$requiredPaths = @(
    "tools\modtran\modtran_grid_nir_mwir_priority.json",
    "tools\modtran\parse_modout2.py",
    "tools\modtran\build_modtran_cases.py",
    "tools\modtran\build_band_lut.py",
    "tools\modtran\build_band_lut_si_candidate.py",
    "tools\modtran\build_validation_band_lut.py",
    "tools\modtran\evaluate_lut_readiness.py",
    "tools\modtran\snapshot_processed.py",
    "tools\modtran\analyze_invalid_geometry.py",
    "tools\modtran\diagnose_visibility_failures.py",
    "tools\modtran\diagnose_path_radiance_failures.py",
    "tools\modtran\audit_modout_units.py",
    "tools\modtran\run_targeted_diagnosis_cases.ps1",
    "tools\modtran\build_targeted_diagnosis_band_lut.py",
    "tools\modtran\build_aerosol_override_smoke_band_lut.py",
    "tools\modtran\search_pcmodwin_units_docs.ps1",
    "tools\modtran\check_validation_outputs.py",
    "tools\modtran\check_visibility_effect.py",
    "tools\modtran\run_modtran_cases.ps1",
    "tools\run_modtran_cases.ps1",
    "tools\modtran\find_modtran_entry.ps1",
    "docs\modtran_lut_format.md",
    "docs\modtran_pcmodwin5_aerosol_template_review.md",
    "docs\modtran_cpp_tau_only_loader_plan.md",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\templates",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\test",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\samples",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\raw\failed",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\generated\modin",
    "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN\processed"
)
foreach ($relativePath in $requiredPaths) {
    $checks.Add((Add-Check "MODTRAN LUT path" (Test-WorkspacePath $relativePath) $relativePath)) | Out-Null
}

$requiredTemplates = @(
    "NIR_transmittance_modin.txt",
    "NIR_transmittance_MODOUT2.txt",
    "NIR_solar_modin.txt",
    "NIR_solar_MODOUT2.txt",
    "NIR_scattering_modin.txt",
    "NIR_scattering_MODOUT2.txt",
    "MWIR_transmittance_modin.txt",
    "MWIR_transmittance_MODOUT2.txt",
    "MWIR_thermal_modin.txt",
    "MWIR_thermal_MODOUT2.txt",
    "MWIR_solar_modin.txt",
    "MWIR_solar_MODOUT2.txt"
)
foreach ($name in $requiredTemplates) {
    $path = Join-Path $modtranRoot "raw\templates\$name"
    $exists = Test-Path -LiteralPath $path -PathType Leaf
    $checks.Add((Add-Check "Priority template exists" $exists $name)) | Out-Null
    if ($exists) {
        $item = Get-Item -LiteralPath $path
        $checks.Add((Add-Check "Priority template non-empty" ($item.Length -gt 0) "$name bytes=$($item.Length)")) | Out-Null
        if ($name -like "*_MODOUT2.txt") {
            $text = Get-Content -LiteralPath $path -TotalCount 40 -Encoding UTF8
            $hasKnownHeader = (($text -join "`n") -match 'FREQ' -and (($text -join "`n") -match 'COMBIN|TOT_TRANS|SOL TR'))
            $checks.Add((Add-Check "Priority MODOUT2 table header" $hasKnownHeader $name)) | Out-Null
        }
    }
}

$blockingManualTemplates = @(
    "NIR_transmittance_modin.txt",
    "NIR_transmittance_MODOUT2.txt",
    "NIR_solar_modin.txt",
    "NIR_solar_MODOUT2.txt"
)
$missingBlockingManualTemplates = @()
foreach ($name in $blockingManualTemplates) {
    $path = Join-Path $modtranRoot "raw\templates\$name"
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $missingBlockingManualTemplates += $name
    }
}
$checks.Add((Add-Check "Manual NIR trans/solar templates gate" ($missingBlockingManualTemplates.Count -eq 0) $(if ($missingBlockingManualTemplates.Count -eq 0) { "ready for single-case/pilot checks" } else { "missing: $($missingBlockingManualTemplates -join ', ')" }))) | Out-Null

$config = $null
if (Test-Path -LiteralPath $configPath -PathType Leaf) {
    $config = Get-Content -LiteralPath $configPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
$checks.Add((Add-Check "Config parses as JSON" ($null -ne $config) "tools/modtran/modtran_grid_nir_mwir_priority.json")) | Out-Null
if ($null -ne $config) {
    $checks.Add((Add-Check "Priority bands include NIR/MWIR" ($config.priority_bands.PSObject.Properties.Name -contains "NIR" -and $config.priority_bands.PSObject.Properties.Name -contains "MWIR") "priority_bands")) | Out-Null
    $checks.Add((Add-Check "Support bands include VIS/SWIR/LWIR" ($config.support_bands.PSObject.Properties.Name -contains "VIS" -and $config.support_bands.PSObject.Properties.Name -contains "SWIR" -and $config.support_bands.PSObject.Properties.Name -contains "LWIR") "support_bands")) | Out-Null
    $checks.Add((Add-Check "Production limit <=3000" ([int]$config.max_production_cases -le 3000) "max_production_cases=$($config.max_production_cases)")) | Out-Null
    $checks.Add((Add-Check "Solar zenith grid present" (($config.solar_zenith_deg | Measure-Object).Count -eq 3 -and $config.solar_zenith_deg -contains 45) "solar_zenith_deg")) | Out-Null
}

$expected = @{
    "path_lut_spectral.csv" = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","wavenumber_cm","wavelength_um","tau_up","path_radiance","unit_radiance","source_file")
    "solar_lut_spectral.csv" = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","target_alt_km","solar_zenith_deg","wavenumber_cm","wavelength_um","tau_down","solar_irradiance","unit_irradiance","source_file")
    "sky_lut_spectral.csv" = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","view_zenith_deg","wavenumber_cm","wavelength_um","sky_radiance","path_scattering_radiance","unit_radiance","source_file")
    "band_lut.csv" = @("band","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","tau_up_band","tau_down_band","path_radiance_band","sky_radiance_band","solar_irradiance_band")
    "manifest.csv" = @("case_id","band","mode","grid","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","wavelength_low_um","wavelength_high_um","wavenumber_start_cm","wavenumber_end_cm","wavenumber_increment_cm","fwhm_cm","template_file","modin_file","status")
}

foreach ($fileName in $expected.Keys) {
    $path = Join-Path $processedDir $fileName
    $header = Read-Header $path
    $checks.Add((Add-Check "Processed CSV columns" (Test-Columns $header $expected[$fileName]) $fileName)) | Out-Null
}

$siCandidatePath = Join-Path $processedDir "band_lut_si_candidate.csv"
if (Test-Path -LiteralPath $siCandidatePath -PathType Leaf) {
    $siHeader = Read-Header $siCandidatePath
    $siExpected = @(
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
        "tau_down_band",
        "path_radiance_band_W_m2_sr_um",
        "sky_radiance_band_W_m2_sr_um",
        "path_scattering_radiance_band_W_m2_sr_um",
        "solar_irradiance_band_W_m2_um",
        "unit_status",
        "integration_method"
    )
    $checks.Add((Add-Check "SI candidate CSV columns" (Test-Columns $siHeader $siExpected) "band_lut_si_candidate.csv")) | Out-Null
}

$qcPath = Join-Path $processedDir "qc_report.md"
$checks.Add((Add-Check "QC report exists" (Test-Path -LiteralPath $qcPath -PathType Leaf) "processed/qc_report.md")) | Out-Null

$pathStats = Get-SpectralCsvStats (Join-Path $processedDir "path_lut_spectral.csv") "path"
if ($pathStats.rows -gt 0) {
    $checks.Add((Add-Check "tau_up range" ([int]$pathStats.bad_tau -eq 0) "path_lut_spectral.csv rows=$($pathStats.tau_count) bad=$($pathStats.bad_tau)")) | Out-Null
    $checks.Add((Add-Check "wavelength/wavenumber consistency" ([int]$pathStats.bad_wavelength -eq 0) "path_lut_spectral.csv bad=$($pathStats.bad_wavelength)")) | Out-Null
}

$solarStats = Get-SpectralCsvStats (Join-Path $processedDir "solar_lut_spectral.csv") "solar"
if ($solarStats.rows -gt 0) {
    $checks.Add((Add-Check "tau_down range" ([int]$solarStats.bad_tau -eq 0) "solar_lut_spectral.csv rows=$($solarStats.tau_count) bad=$($solarStats.bad_tau)")) | Out-Null
}

$manifestPath = Join-Path $processedDir "manifest.csv"
$manifestRows = Import-CsvIfAny $manifestPath
if ($manifestRows.Count -gt 0) {
    $checks.Add((Add-Check "Production manifest <=3000" ($manifestRows.Count -le 3000) "processed/manifest.csv rows=$($manifestRows.Count)")) | Out-Null
    $missingModin = @($manifestRows | Where-Object {
        $_.modin_file -ne "" -and -not (Test-Path -LiteralPath (Join-Path $rootPath $_.modin_file) -PathType Leaf)
    })
    $checks.Add((Add-Check "Manifest modin files exist" ($missingModin.Count -eq 0) "missing=$($missingModin.Count)")) | Out-Null
}

$bandRows = Import-CsvIfAny (Join-Path $processedDir "band_lut.csv")
if ($bandRows.Count -gt 0) {
    $badBandTau = @($bandRows | Where-Object {
        ($_.tau_up_band -ne "" -and ([double]$_.tau_up_band -lt 0 -or [double]$_.tau_up_band -gt 1)) -or
        ($_.tau_down_band -ne "" -and ([double]$_.tau_down_band -lt 0 -or [double]$_.tau_down_band -gt 1))
    })
    $checks.Add((Add-Check "band_lut tau range" ($badBandTau.Count -eq 0) "band_lut.csv rows=$($bandRows.Count)")) | Out-Null
}

Write-Host "Stage 3 MODTRAN LUT check"
Write-Host "Workspace: $rootPath"
Write-Host ""
$checks | Format-Table -AutoSize

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Failed checks: $($failed.Count)"
    if ($missingBlockingManualTemplates.Count -gt 0) {
        Write-Host ""
        Write-Host "Manual PcModWin5 templates required before pilot:"
        foreach ($name in $missingBlockingManualTemplates) {
            Write-Host "  - raw/templates/$name"
        }
        Write-Host "Generate these from the PcModWin5 GUI first; do not continue to pilot execution."
    }
    if ($Strict) {
        exit 1
    }
    exit 2
}

Write-Host ""
Write-Host "All Stage 3 MODTRAN LUT checks passed."
