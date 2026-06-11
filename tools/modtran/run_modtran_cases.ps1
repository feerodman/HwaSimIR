param(
    [string]$Manifest = "",
    [string]$PcModWinRoot = "F:\Programs\PcModWin5",
    [Alias("Executable")]
    [string]$ModtranExe = "",
    [string]$Python = "python",
    [switch]$SingleCase,
    [string]$CaseId = "",
    [ValidateRange(0, 3000)]
    [int]$CaseLimit = 0,
    [switch]$NoDeleteRaw,
    [switch]$ValidationSix,
    [switch]$Pilot72,
    [switch]$VisibilitySmoke18,
    [switch]$AerosolOverrideSmoke,
    [switch]$ProductionNirMwir,
    [ValidateSet("", "NIR_Transmittance", "MWIR_Transmittance", "MWIR_ThermalRadiance", "Solar_NIR_MWIR", "NIR_RadianceWithScattering")]
    [string]$BatchName = "",
    [switch]$Resume,
    [switch]$Pilot
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..\..")
$rootPath = $root.Path
$modtranRoot = Join-Path $rootPath "HwaSim_IR\Bin\Config\Atmosphere\MODTRAN"
$processedDir = Join-Path $modtranRoot "processed"
$samplesRoot = Join-Path $modtranRoot "raw\samples"
$failedRoot = Join-Path $modtranRoot "raw\failed"
$generatedDir = Join-Path $modtranRoot "generated"
$parserPath = Join-Path $rootPath "tools\modtran\parse_modout2.py"
$bandLutBuilderPath = Join-Path $rootPath "tools\modtran\build_band_lut.py"
$aerosolOverrideBandBuilderPath = Join-Path $rootPath "tools\modtran\build_aerosol_override_smoke_band_lut.py"
$validationAuditPath = Join-Path $rootPath "tools\modtran\check_validation_outputs.py"
$visibilityAuditPath = Join-Path $rootPath "tools\modtran\check_visibility_effect.py"
$caseBuilderPath = Join-Path $rootPath "tools\modtran\build_modtran_cases.py"
$configPath = Join-Path $rootPath "tools\modtran\modtran_grid_nir_mwir_priority.json"
$processedManifest = Join-Path $processedDir "manifest.csv"
$generatedManifest = Join-Path $generatedDir "case_manifest.csv"
$productionManifest = Join-Path $generatedDir "production_manifest.csv"
$aerosolOverrideSmokeManifest = Join-Path $generatedDir "aerosol_override_smoke_manifest.csv"

if ([string]::IsNullOrWhiteSpace($Manifest)) {
    if ($ProductionNirMwir) {
        $Manifest = $productionManifest
    }
    elseif ($Pilot72 -or $VisibilitySmoke18) {
        $Manifest = $generatedManifest
    }
    elseif ($AerosolOverrideSmoke) {
        $Manifest = $aerosolOverrideSmokeManifest
    }
    elseif (Test-Path -LiteralPath $processedManifest -PathType Leaf) {
        $Manifest = $processedManifest
    }
    else {
        $Manifest = $generatedManifest
    }
}
elseif (-not [System.IO.Path]::IsPathRooted($Manifest)) {
    $Manifest = Join-Path $rootPath $Manifest
}

function Resolve-ModtranExecutable {
    param(
        [string]$Root,
        [string]$ExplicitExecutable
    )

    if ([string]::IsNullOrWhiteSpace($ExplicitExecutable)) {
        throw "No -ModtranExe was provided. Run tools\modtran\find_modtran_entry.ps1 first, then pass the user-confirmed executable path."
    }

    $candidate = $ExplicitExecutable
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path (Join-Path $Root "bin") $candidate
    }
    if (-not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
        throw "Explicit MODTRAN executable was not found: $candidate"
    }
    $extension = [System.IO.Path]::GetExtension($candidate)
    if ($extension -match '\.bat|\.cmd') {
        $batchText = Get-Content -LiteralPath $candidate -Raw -Encoding ASCII -ErrorAction SilentlyContinue
        if ($batchText -match '(?im)^\s*pause\s*$') {
            throw "The selected batch file contains PAUSE and is not safe for unattended automation: $candidate. Use the underlying MODTRAN engine executable instead."
        }
    }
    return (Resolve-Path -LiteralPath $candidate).Path
}

function Get-OutputCsvForMode {
    param([string]$Mode)
    if ($AerosolOverrideSmoke) {
        if ($Mode -ne "Transmittance") {
            throw "AerosolOverrideSmoke supports only Transmittance mode."
        }
        return Join-Path $processedDir "aerosol_override_smoke_path_lut_spectral.csv"
    }
    switch ($Mode) {
        "Transmittance" { return Join-Path $processedDir "path_lut_spectral.csv" }
        "ThermalRadiance" { return Join-Path $processedDir "path_lut_spectral.csv" }
        "DirectSolarIrradiance" { return Join-Path $processedDir "solar_lut_spectral.csv" }
        "RadianceWithScattering" { return Join-Path $processedDir "sky_lut_spectral.csv" }
        default { throw "Unsupported mode in manifest: $Mode" }
    }
}

function Initialize-AerosolOverrideSmokeTables {
    param([object[]]$SelectedCases)

    $pathColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","wavenumber_cm","wavelength_um","tau_up","path_radiance","unit_radiance","source_file")
    $bandColumns = @("band","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","tau_up_band","tau_down_band","path_radiance_band","sky_radiance_band","path_scattering_radiance_band","solar_irradiance_band","unit_radiance","unit_irradiance","source_case_ids")
    Write-CsvHeader -Path (Join-Path $processedDir "aerosol_override_smoke_path_lut_spectral.csv") -Columns $pathColumns
    Write-CsvHeader -Path (Join-Path $processedDir "band_lut_aerosol_override_smoke.csv") -Columns $bandColumns
    $emptySolar = Join-Path $processedDir "aerosol_override_smoke_solar_lut_spectral.csv"
    $emptySky = Join-Path $processedDir "aerosol_override_smoke_sky_lut_spectral.csv"
    if (Test-Path -LiteralPath $emptySolar -PathType Leaf) { Remove-Item -LiteralPath $emptySolar -Force }
    if (Test-Path -LiteralPath $emptySky -PathType Leaf) { Remove-Item -LiteralPath $emptySky -Force }

    @"
# Aerosol Override Smoke QC

Independent PcModWin5/MODTRAN aerosol/visibility override smoke. Production LUT files are not overwritten.

## Source Columns

- Transmittance: ``FREQ/CM-1``, ``COMBIN TRANS`` -> ``tau_up``

"@ | Set-Content -LiteralPath (Join-Path $processedDir "qc_aerosol_override_smoke.md") -Encoding UTF8

    $manifestPath = Join-Path $processedDir "aerosol_override_smoke_manifest.csv"
    $rows = @()
    foreach ($case in $SelectedCases) {
        $copy = [ordered]@{}
        foreach ($property in $case.PSObject.Properties) {
            $copy[$property.Name] = $property.Value
        }
        $copy["grid"] = "aerosol_override_smoke"
        $copy["status"] = "aerosol_override_smoke_selected"
        $copy["stage"] = "aerosol_override_smoke_selected"
        $rows += [PSCustomObject]$copy
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Write-AerosolOverrideSmokeManifest {
    param(
        [object[]]$SelectedCases,
        [string]$Status
    )

    $manifestPath = Join-Path $processedDir "aerosol_override_smoke_manifest.csv"
    $rows = @()
    foreach ($case in $SelectedCases) {
        $copy = [ordered]@{}
        foreach ($property in $case.PSObject.Properties) {
            $copy[$property.Name] = $property.Value
        }
        $copy["grid"] = "aerosol_override_smoke"
        $copy["status"] = $Status
        $copy["stage"] = $Status
        $rows += [PSCustomObject]$copy
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Write-CsvHeader {
    param(
        [string]$Path,
        [string[]]$Columns
    )

    $Columns -join "," | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Write-QcHeader {
    param(
        [string]$Path,
        [string]$RunLabel
    )

    @"
# MODTRAN LUT QC Report

$RunLabel QC report for MODTRAN checks. Units remain marked as `MODOUT2_native` until the PcModWin5 unit export is confirmed.

## Source Columns

- Transmittance: ``FREQ/CM-1``, ``COMBIN TRANS`` -> ``tau_up``
- ThermalRadiance: ``FREQ``, ``TOT_TRANS``, ``PTH_THRML`` -> ``tau_up``, ``path_radiance``
- DirectSolarIrradiance: ``FREQ``, ``TRANS``, ``SOL TR``, ``SOLAR`` -> ``tau_down``, ``solar_irradiance``
- RadianceWithScattering: ``FREQ``, ``TOT_TRANS``, ``SOL_SCAT``, ``SING_SCAT``, ``TOTAL_RAD`` -> ``path_scattering_radiance``, ``sky_radiance``

"@ | Set-Content -LiteralPath $Path -Encoding UTF8
}

function Reset-ProcessedTables {
    param(
        [object[]]$SelectedCases,
        [string]$Grid,
        [string]$Status,
        [string]$RunLabel
    )

    $pathColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","wavenumber_cm","wavelength_um","tau_up","path_radiance","unit_radiance","source_file")
    $solarColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","target_alt_km","solar_zenith_deg","wavenumber_cm","wavelength_um","tau_down","solar_irradiance","unit_irradiance","source_file")
    $skyColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","view_zenith_deg","wavenumber_cm","wavelength_um","sky_radiance","path_scattering_radiance","unit_radiance","source_file")
    $bandColumns = @("band","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","tau_up_band","tau_down_band","path_radiance_band","sky_radiance_band","path_scattering_radiance_band","solar_irradiance_band","unit_radiance","unit_irradiance","source_case_ids")

    Write-CsvHeader -Path (Join-Path $processedDir "path_lut_spectral.csv") -Columns $pathColumns
    Write-CsvHeader -Path (Join-Path $processedDir "solar_lut_spectral.csv") -Columns $solarColumns
    Write-CsvHeader -Path (Join-Path $processedDir "sky_lut_spectral.csv") -Columns $skyColumns
    Write-CsvHeader -Path (Join-Path $processedDir "band_lut.csv") -Columns $bandColumns
    Write-QcHeader -Path (Join-Path $processedDir "qc_report.md") -RunLabel $RunLabel
    $failureCsv = Join-Path $processedDir "failed_cases.csv"
    if (Test-Path -LiteralPath $failureCsv -PathType Leaf) {
        Remove-Item -LiteralPath $failureCsv -Force
    }
    Write-RunManifest -SelectedCases $SelectedCases -Grid $Grid -Status $Status
}

function Write-RunManifest {
    param(
        [object[]]$SelectedCases,
        [string]$Grid,
        [string]$Status
    )

    $manifestPath = Join-Path $processedDir "manifest.csv"
    $rows = @()
    foreach ($case in $SelectedCases) {
        $copy = [ordered]@{}
        foreach ($property in $case.PSObject.Properties) {
            $copy[$property.Name] = $property.Value
        }
        $copy["grid"] = $Grid
        $copy["status"] = $Status
        $copy["stage"] = $Status
        $rows += [PSCustomObject]$copy
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Append-RunManifest {
    param(
        [object[]]$SelectedCases,
        [string]$Grid,
        [string]$Status
    )

    $manifestPath = Join-Path $processedDir "manifest.csv"
    $rows = @()
    if (Test-Path -LiteralPath $manifestPath -PathType Leaf) {
        $rows += @(Import-Csv -LiteralPath $manifestPath)
    }
    foreach ($case in $SelectedCases) {
        $copy = [ordered]@{}
        foreach ($property in $case.PSObject.Properties) {
            $copy[$property.Name] = $property.Value
        }
        $copy["grid"] = $Grid
        $copy["status"] = $Status
        $copy["stage"] = $Status
        $rows += [PSCustomObject]$copy
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Write-ProductionManifest {
    param(
        [object[]]$AllCases,
        [string]$Status
    )

    $manifestPath = Join-Path $processedDir "manifest.csv"
    $rows = @()
    foreach ($case in $AllCases) {
        $copy = [ordered]@{}
        foreach ($property in $case.PSObject.Properties) {
            if ($property.Name -ne "stage") {
                $copy[$property.Name] = $property.Value
            }
        }
        $copy["grid"] = "production_nir_mwir"
        $copy["status"] = $Status
        $copy["stage"] = $Status
        $rows += [PSCustomObject]$copy
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Update-ManifestCaseStatus {
    param(
        [string]$CaseId,
        [string]$Status
    )

    $manifestPath = Join-Path $processedDir "manifest.csv"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "Processed manifest not found while updating case status: $manifestPath"
    }
    $rows = @(Import-Csv -LiteralPath $manifestPath)
    $found = $false
    foreach ($row in $rows) {
        if ($row.case_id -eq $CaseId) {
            $row.status = $Status
            if ($row.PSObject.Properties.Name -contains "stage") {
                $row.stage = $Status
            }
            else {
                $row | Add-Member -NotePropertyName "stage" -NotePropertyValue $Status -Force
            }
            $found = $true
        }
    }
    if (-not $found) {
        throw "CaseId not found in processed manifest while updating status: $CaseId"
    }
    $rows | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8
}

function Get-SucceededCaseIds {
    $manifestPath = Join-Path $processedDir "manifest.csv"
    $result = New-Object 'System.Collections.Generic.HashSet[string]'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        return @()
    }
    $rows = @(Import-Csv -LiteralPath $manifestPath)
    foreach ($row in $rows) {
        if ($row.status -eq "production_nir_mwir_succeeded" -or $row.stage -eq "production_nir_mwir_succeeded") {
            [void]$result.Add($row.case_id)
        }
    }
    foreach ($id in $result) {
        Write-Output $id
    }
}

function Initialize-ProductionProcessedTables {
    param([object[]]$AllCases)

    $pathColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","wavenumber_cm","wavelength_um","tau_up","path_radiance","unit_radiance","source_file")
    $solarColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","target_alt_km","solar_zenith_deg","wavenumber_cm","wavelength_um","tau_down","solar_irradiance","unit_irradiance","source_file")
    $skyColumns = @("case_id","band","mode","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","view_zenith_deg","wavenumber_cm","wavelength_um","sky_radiance","path_scattering_radiance","unit_radiance","source_file")
    $bandColumns = @("band","atmosphere_model","aerosol_model","humidity_profile","visibility_km","observer_alt_km","target_alt_km","range_km","solar_zenith_deg","tau_up_band","tau_down_band","path_radiance_band","sky_radiance_band","path_scattering_radiance_band","solar_irradiance_band","unit_radiance","unit_irradiance","source_case_ids")

    Write-CsvHeader -Path (Join-Path $processedDir "path_lut_spectral.csv") -Columns $pathColumns
    Write-CsvHeader -Path (Join-Path $processedDir "solar_lut_spectral.csv") -Columns $solarColumns
    Write-CsvHeader -Path (Join-Path $processedDir "sky_lut_spectral.csv") -Columns $skyColumns
    Write-CsvHeader -Path (Join-Path $processedDir "band_lut.csv") -Columns $bandColumns
    Write-QcHeader -Path (Join-Path $processedDir "qc_report.md") -RunLabel "ProductionNirMwir"
    $failureCsv = Join-Path $processedDir "failed_cases.csv"
    if (Test-Path -LiteralPath $failureCsv -PathType Leaf) {
        Remove-Item -LiteralPath $failureCsv -Force
    }
    Write-ProductionManifest -AllCases $AllCases -Status "production_pending"
}

function Test-Modout2TableHeader {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    $text = Get-Content -LiteralPath $Path -TotalCount 80 -Encoding UTF8 -ErrorAction Stop
    $joined = $text -join "`n"
    return ($joined -match 'FREQ' -and $joined -match 'COMBIN|TOT_TRANS|SOL TR')
}

function Copy-CaseArtifacts {
    param(
        [object]$Case,
        [string]$DestinationRoot,
        [string]$PcUsr,
        [string]$Reason = ""
    )

    $caseDir = Join-Path $DestinationRoot $Case.case_id
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null

    $modinSource = Join-Path $rootPath $Case.modin_file
    if (Test-Path -LiteralPath $modinSource) {
        Copy-Item -LiteralPath $modinSource -Destination (Join-Path $caseDir "modin") -Force
    }

    foreach ($name in @("MODOUT1", "MODOUT2")) {
        $source = Join-Path $PcUsr $name
        if (Test-Path -LiteralPath $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $caseDir $name) -Force
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($Reason)) {
        $Reason | Set-Content -LiteralPath (Join-Path $caseDir "failure.txt") -Encoding UTF8
    }
}

function Get-FileStamp {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    $item = Get-Item -LiteralPath $Path
    return [PSCustomObject]@{
        LastWriteTimeUtc = $item.LastWriteTimeUtc
        Length = $item.Length
    }
}

function Test-Refreshed {
    param(
        [object]$Before,
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    if ($null -eq $Before) {
        return $true
    }
    $after = Get-Item -LiteralPath $Path
    return ($after.LastWriteTimeUtc -gt $Before.LastWriteTimeUtc)
}

function Sync-TapeOutputs {
    param(
        [string]$PcBin,
        [string]$PcUsr,
        [object]$BeforeTape6,
        [object]$BeforeTape7,
        [object]$BeforeTape8
    )

    $tape6 = Join-Path $PcBin "TAPE6"
    $tape7 = Join-Path $PcBin "TAPE7"
    $tape8 = Join-Path $PcBin "TAPE8"

    if (Test-Refreshed -Before $BeforeTape6 -Path $tape6) {
        Copy-Item -LiteralPath $tape6 -Destination (Join-Path $PcUsr "MODOUT1") -Force
    }
    if (Test-Refreshed -Before $BeforeTape7 -Path $tape7) {
        Copy-Item -LiteralPath $tape7 -Destination (Join-Path $PcUsr "MODOUT2") -Force
    }
    if (Test-Refreshed -Before $BeforeTape8 -Path $tape8) {
        Copy-Item -LiteralPath $tape8 -Destination (Join-Path $PcUsr "MODOUT3") -Force
    }
}

function Find-PreferredCase {
    param(
        [object[]]$Cases,
        [hashtable]$Spec
    )

    $exact = @($Cases | Where-Object { $_.case_id -eq $Spec.Preferred })
    if ($exact.Count -gt 0) {
        return $exact[0]
    }

    $fallback = @($Cases | Where-Object {
        $_.band -eq $Spec.Band -and
        $_.mode -eq $Spec.Mode -and
        [double]$_.observer_alt_km -eq 10 -and
        [double]$_.target_alt_km -eq 10 -and
        [double]$_.range_km -eq 20 -and
        [double]$_.visibility_km -eq 23
    } | Select-Object -First 1)
    if ($fallback.Count -gt 0) {
        return $fallback[0]
    }

    throw "Could not find preferred single-case entry for $($Spec.Band)/$($Spec.Mode) in manifest."
}

function Select-Pilot72Cases {
    param([object[]]$Cases)

    $allowedRanges = @(1.0, 5.0, 20.0, 50.0)
    $allowedVisibilities = @(5.0, 23.0, 50.0)
    $allowedModes = @{
        "NIR" = @("Transmittance", "DirectSolarIrradiance", "RadianceWithScattering")
        "MWIR" = @("Transmittance", "ThermalRadiance", "DirectSolarIrradiance")
    }

    $selected = @($Cases | Where-Object {
        $band = $_.band
        $mode = $_.mode
        $allowedModes.ContainsKey($band) -and
        ($allowedModes[$band] -contains $mode) -and
        [double]$_.observer_alt_km -eq 10.0 -and
        [double]$_.target_alt_km -eq 10.0 -and
        ($allowedRanges -contains [double]$_.range_km) -and
        ($allowedVisibilities -contains [double]$_.visibility_km) -and
        $_.aerosol_model -eq "Rural" -and
        $_.humidity_profile -eq "default" -and
        [double]$_.solar_zenith_deg -eq 45.0
    } | Sort-Object band, @{Expression = {[double]$_.range_km}}, @{Expression = {[double]$_.visibility_km}}, mode)

    if ($selected.Count -ne 72) {
        throw "Pilot72 selection resolved to $($selected.Count) cases instead of 72; refusing to continue."
    }
    return $selected
}

function Select-VisibilitySmoke18Cases {
    param([object[]]$Cases)

    $allowedAltitudePairs = @("3,3", "10,10", "20,3")
    $allowedVisibilities = @(5.0, 23.0, 50.0)
    $selected = @($Cases | Where-Object {
        $_.band -in @("NIR", "MWIR") -and
        $_.mode -eq "Transmittance" -and
        ($allowedAltitudePairs -contains "$([double]$_.observer_alt_km),$([double]$_.target_alt_km)") -and
        [double]$_.range_km -eq 50.0 -and
        ($allowedVisibilities -contains [double]$_.visibility_km) -and
        $_.aerosol_model -eq "Rural" -and
        $_.humidity_profile -eq "default"
    } | Sort-Object band, @{Expression = {[double]$_.observer_alt_km}}, @{Expression = {[double]$_.target_alt_km}}, @{Expression = {[double]$_.visibility_km}})

    if ($selected.Count -ne 18) {
        throw "VisibilitySmoke18 selection resolved to $($selected.Count) cases instead of 18; refusing to continue."
    }
    return $selected
}

function Select-AerosolOverrideSmokeCases {
    param([object[]]$Cases)

    $allowedAltitudePairs = @("0.1,0.1", "3,3", "10,10", "20,3")
    $allowedRanges = @(20.0, 50.0)
    $allowedVisibilities = @(0.5, 2.0, 5.0, 23.0, 50.0)
    $selected = @($Cases | Where-Object {
        $_.band -in @("NIR", "MWIR") -and
        $_.mode -eq "Transmittance" -and
        ($allowedAltitudePairs -contains "$([double]$_.observer_alt_km),$([double]$_.target_alt_km)") -and
        ($allowedRanges -contains [double]$_.range_km) -and
        ($allowedVisibilities -contains [double]$_.visibility_km) -and
        $_.aerosol_model -eq "Rural" -and
        $_.humidity_profile -eq "default"
    } | Sort-Object band, @{Expression = {[double]$_.observer_alt_km}}, @{Expression = {[double]$_.target_alt_km}}, @{Expression = {[double]$_.range_km}}, @{Expression = {[double]$_.visibility_km}})

    if ($selected.Count -ne 80) {
        throw "AerosolOverrideSmoke selection resolved to $($selected.Count) cases instead of 80; refusing to continue."
    }
    return $selected
}

function Select-ProductionBatchCases {
    param(
        [object[]]$Cases,
        [string]$Name
    )

    if ([string]::IsNullOrWhiteSpace($Name)) {
        throw "-ProductionNirMwir requires -BatchName. Allowed values: NIR_Transmittance, MWIR_Transmittance, MWIR_ThermalRadiance, Solar_NIR_MWIR, NIR_RadianceWithScattering."
    }

    $selected = @()
    switch ($Name) {
        "NIR_Transmittance" {
            $selected = @($Cases | Where-Object { $_.band -eq "NIR" -and $_.mode -eq "Transmittance" })
        }
        "MWIR_Transmittance" {
            $selected = @($Cases | Where-Object { $_.band -eq "MWIR" -and $_.mode -eq "Transmittance" })
        }
        "MWIR_ThermalRadiance" {
            $selected = @($Cases | Where-Object { $_.band -eq "MWIR" -and $_.mode -eq "ThermalRadiance" })
        }
        "Solar_NIR_MWIR" {
            $selected = @($Cases | Where-Object { $_.band -in @("NIR", "MWIR") -and $_.mode -eq "DirectSolarIrradiance" })
        }
        "NIR_RadianceWithScattering" {
            $selected = @($Cases | Where-Object { $_.band -eq "NIR" -and $_.mode -eq "RadianceWithScattering" })
        }
        default {
            throw "Unsupported BatchName: $Name"
        }
    }

    $selected = @($selected | Sort-Object band, mode, @{Expression = {[double]$_.observer_alt_km}}, @{Expression = {[double]$_.target_alt_km}}, @{Expression = {[double]$_.range_km}}, @{Expression = {[double]$_.visibility_km}})
    if ($selected.Count -le 0) {
        throw "Production batch $Name resolved to no runnable cases; refusing to continue."
    }
    return $selected
}

if ($Pilot) {
    throw "-Pilot is disabled for this safety stage. Use -SingleCase -CaseLimit 1 first, then at most -CaseLimit 6."
}
if ($ProductionNirMwir -and ($SingleCase -or $ValidationSix -or $Pilot72 -or $VisibilitySmoke18 -or $AerosolOverrideSmoke -or $Pilot)) {
    throw "-ProductionNirMwir cannot be combined with single-case, validation, pilot, or visibility smoke modes."
}
if (-not $ProductionNirMwir -and -not [string]::IsNullOrWhiteSpace($BatchName)) {
    throw "-BatchName is only valid with -ProductionNirMwir."
}
if ($ProductionNirMwir -and [string]::IsNullOrWhiteSpace($BatchName)) {
    throw "-ProductionNirMwir requires -BatchName."
}
if ($ProductionNirMwir -and $CaseLimit -gt 0 -and $CaseLimit -gt 840) {
    throw "A single production batch cannot exceed 840 cases. Refusing CaseLimit=$CaseLimit."
}
if ($Pilot72 -and $CaseLimit -ne 72) {
    throw "-Pilot72 requires -CaseLimit 72."
}
if ($VisibilitySmoke18 -and $CaseLimit -ne 18) {
    throw "-VisibilitySmoke18 requires -CaseLimit 18."
}
if ($AerosolOverrideSmoke -and $CaseLimit -ne 80) {
    throw "-AerosolOverrideSmoke requires -CaseLimit 80."
}
if ($Pilot72 -and ($SingleCase -or $ValidationSix -or $VisibilitySmoke18 -or $AerosolOverrideSmoke)) {
    throw "-Pilot72 cannot be combined with -SingleCase, -ValidationSix, -VisibilitySmoke18, or -AerosolOverrideSmoke."
}
if ($VisibilitySmoke18 -and ($SingleCase -or $ValidationSix -or $AerosolOverrideSmoke)) {
    throw "-VisibilitySmoke18 cannot be combined with -SingleCase, -ValidationSix, or -AerosolOverrideSmoke."
}
if ($AerosolOverrideSmoke -and ($SingleCase -or $ValidationSix)) {
    throw "-AerosolOverrideSmoke cannot be combined with -SingleCase or -ValidationSix."
}
if ($ValidationSix -and $CaseLimit -ne 6) {
    throw "-ValidationSix requires -CaseLimit 6."
}
if ($ValidationSix -and $SingleCase) {
    throw "-ValidationSix cannot be combined with -SingleCase."
}
if ($SingleCase -and $CaseLimit -ne 1) {
    throw "-SingleCase only allows -CaseLimit 1."
}
if (-not $ProductionNirMwir -and -not $Pilot72 -and -not $VisibilitySmoke18 -and -not $AerosolOverrideSmoke -and -not $ValidationSix -and -not $SingleCase -and $CaseLimit -gt 6) {
    throw "Only -Pilot72, -VisibilitySmoke18, -AerosolOverrideSmoke, or -ProductionNirMwir may run more than 6 cases. Refusing CaseLimit=$CaseLimit."
}

if ($Pilot72 -or $VisibilitySmoke18 -or $AerosolOverrideSmoke) {
    if (-not (Test-Path -LiteralPath $validationAuditPath -PathType Leaf)) {
        throw "Validation audit script not found: $validationAuditPath"
    }
    if (-not (Test-Path -LiteralPath $caseBuilderPath -PathType Leaf)) {
        throw "Case builder not found: $caseBuilderPath"
    }
    if ($Pilot72) {
        & $Python $validationAuditPath "--processed-dir" $processedDir
        if ($LASTEXITCODE -ne 0) {
            throw "Validation output audit failed; stopping before Pilot72."
        }
        & $Python $caseBuilderPath "--config" $configPath "--dry-run" "--pilot"
        if ($LASTEXITCODE -ne 0) {
            throw "Pilot72 dry-run case generation failed with exit code $LASTEXITCODE."
        }
    }
    if ($VisibilitySmoke18) {
        & $Python $caseBuilderPath "--config" $configPath "--dry-run" "--visibility-smoke" "--skip-processed-manifest"
        if ($LASTEXITCODE -ne 0) {
            throw "VisibilitySmoke18 dry-run case generation failed with exit code $LASTEXITCODE."
        }
    }
    if ($AerosolOverrideSmoke) {
        & $Python $caseBuilderPath "--config" $configPath "--dry-run" "--aerosol-override-smoke" "--skip-processed-manifest"
        if ($LASTEXITCODE -ne 0) {
            throw "AerosolOverrideSmoke dry-run case generation failed with exit code $LASTEXITCODE."
        }
    }
    if ($AerosolOverrideSmoke) {
        $Manifest = $aerosolOverrideSmokeManifest
    }
    else {
        $Manifest = $generatedManifest
    }
}

if ($ProductionNirMwir) {
    if (-not (Test-Path -LiteralPath $caseBuilderPath -PathType Leaf)) {
        throw "Case builder not found: $caseBuilderPath"
    }
    & $Python $caseBuilderPath "--config" $configPath "--dry-run" "--production-nir-mwir"
    if ($LASTEXITCODE -ne 0) {
        throw "Production NIR/MWIR dry-run case generation failed with exit code $LASTEXITCODE."
    }
    $Manifest = $productionManifest
}

if (-not (Test-Path -LiteralPath $Manifest -PathType Leaf)) {
    throw "Manifest not found: $Manifest. Generate a dry-run manifest first; do not run production MODTRAN."
}
if (-not (Test-Path -LiteralPath $parserPath -PathType Leaf)) {
    throw "Parser not found: $parserPath"
}
if (-not (Test-Path -LiteralPath $bandLutBuilderPath -PathType Leaf)) {
    throw "Band LUT builder not found: $bandLutBuilderPath"
}
if ($AerosolOverrideSmoke -and -not (Test-Path -LiteralPath $aerosolOverrideBandBuilderPath -PathType Leaf)) {
    throw "Aerosol override band LUT builder not found: $aerosolOverrideBandBuilderPath"
}

$pcRoot = Resolve-Path -LiteralPath $PcModWinRoot
$pcBin = Join-Path $pcRoot.Path "bin"
$pcUsr = Join-Path $pcRoot.Path "usr"
if (-not (Test-Path -LiteralPath $pcBin -PathType Container)) {
    throw "PcModWin bin directory not found: $pcBin"
}
if (-not (Test-Path -LiteralPath $pcUsr -PathType Container)) {
    throw "PcModWin usr directory not found: $pcUsr"
}

$exePath = Resolve-ModtranExecutable -Root $pcRoot.Path -ExplicitExecutable $ModtranExe
New-Item -ItemType Directory -Force -Path $processedDir,$samplesRoot,$failedRoot,$generatedDir | Out-Null

$lockPath = Join-Path $generatedDir "run_modtran_cases.lock"
if (Test-Path -LiteralPath $lockPath) {
    throw "Another MODTRAN run appears to be active: $lockPath. This runner is intentionally single-threaded because MODTRAN writes fixed output filenames."
}
New-Item -ItemType File -Path $lockPath -Force | Out-Null

try {
    $cases = @(Import-Csv -LiteralPath $Manifest)
    if ($ProductionNirMwir) {
        if ($cases.Count -gt 3000) {
            throw "Production NIR/MWIR manifest contains $($cases.Count) cases, above 3000. Stop and ask the user to confirm a smaller grid."
        }
        $supportBandCases = @($cases | Where-Object { $_.band -notin @("NIR", "MWIR") })
        if ($supportBandCases.Count -gt 0) {
            throw "Production NIR/MWIR manifest contains non-NIR/MWIR cases. Refusing to run VIS/SWIR/LWIR production."
        }
        if ($cases.Count -le 0) {
            throw "Production NIR/MWIR manifest contains no runnable cases."
        }
    }
    $allowedSpecs = @(
        @{ Band = "MWIR"; Mode = "Transmittance"; Preferred = "MWIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault" },
        @{ Band = "MWIR"; Mode = "ThermalRadiance"; Preferred = "MWIR_thermal_obs10_tar10_rng20_vis23_aerRural_humdefault" },
        @{ Band = "MWIR"; Mode = "DirectSolarIrradiance"; Preferred = "MWIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45" },
        @{ Band = "NIR"; Mode = "Transmittance"; Preferred = "NIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault" },
        @{ Band = "NIR"; Mode = "DirectSolarIrradiance"; Preferred = "NIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45" },
        @{ Band = "NIR"; Mode = "RadianceWithScattering"; Preferred = "NIR_scattering_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45" }
    )

    $selectedCases = @()
    if ($ProductionNirMwir) {
        $selectedCases = @(Select-ProductionBatchCases -Cases $cases -Name $BatchName)
        $forceFreshProductionSelection = ($BatchName -eq "NIR_Transmittance" -and -not $Resume)
        if (-not $forceFreshProductionSelection) {
            $succeededCaseIds = @(Get-SucceededCaseIds)
            $beforeSkipCount = $selectedCases.Count
            $selectedCases = @($selectedCases | Where-Object { $succeededCaseIds -notcontains $_.case_id })
            $skippedCount = $beforeSkipCount - $selectedCases.Count
            if ($skippedCount -gt 0) {
                Write-Host "Resume guard: skipping $skippedCount already successful production cases."
            }
        }
        if ($CaseLimit -gt 0 -and $selectedCases.Count -gt $CaseLimit) {
            $selectedCases = @($selectedCases | Select-Object -First $CaseLimit)
        }
    }
    elseif ($Pilot72) {
        $selectedCases = @(Select-Pilot72Cases -Cases $cases)
    }
    elseif ($VisibilitySmoke18) {
        $selectedCases = @(Select-VisibilitySmoke18Cases -Cases $cases)
    }
    elseif ($AerosolOverrideSmoke) {
        $selectedCases = @(Select-AerosolOverrideSmokeCases -Cases $cases)
    }
    elseif ($ValidationSix) {
        foreach ($spec in $allowedSpecs) {
            $selectedCases += (Find-PreferredCase -Cases $cases -Spec $spec)
        }
    }
    elseif (-not [string]::IsNullOrWhiteSpace($CaseId)) {
        $match = @($cases | Where-Object { $_.case_id -eq $CaseId })
        if ($match.Count -eq 0) {
            throw "CaseId not found in manifest: $CaseId"
        }
        $candidate = $match[0]
        $allowed = @($allowedSpecs | Where-Object { $_.Band -eq $candidate.band -and $_.Mode -eq $candidate.mode })
        if ($allowed.Count -eq 0) {
            throw "CaseId $CaseId is not one of the six allowed validation band/mode combinations."
        }
        $selectedCases = @($candidate)
    }
    else {
        $limit = $(if ($SingleCase) { 1 } elseif ($CaseLimit -gt 0) { $CaseLimit } else { 1 })
        for ($i = 0; $i -lt $limit; $i++) {
            $selectedCases += (Find-PreferredCase -Cases $cases -Spec $allowedSpecs[$i])
        }
    }

    if (-not $ProductionNirMwir -and -not $Pilot72 -and -not $VisibilitySmoke18 -and -not $AerosolOverrideSmoke -and $selectedCases.Count -gt 6) {
        throw "Refusing to run $($selectedCases.Count) cases. This safety stage is capped at 6 real MODTRAN cases."
    }
    if ($ProductionNirMwir -and $selectedCases.Count -eq 0) {
        Write-Host "No pending cases selected for production batch $BatchName. Nothing to run."
        return
    }
    if ($Pilot72 -and $selectedCases.Count -ne 72) {
        throw "Pilot72 selection resolved to $($selectedCases.Count) cases; refusing to continue."
    }
    if ($VisibilitySmoke18 -and $selectedCases.Count -ne 18) {
        throw "VisibilitySmoke18 selection resolved to $($selectedCases.Count) cases; refusing to continue."
    }
    if ($AerosolOverrideSmoke -and $selectedCases.Count -ne 80) {
        throw "AerosolOverrideSmoke selection resolved to $($selectedCases.Count) cases; refusing to continue."
    }
    if ($SingleCase -and $selectedCases.Count -ne 1) {
        throw "SingleCase selection resolved to $($selectedCases.Count) cases; refusing to continue."
    }
    if ($ValidationSix -and $selectedCases.Count -ne 6) {
        throw "ValidationSix selection resolved to $($selectedCases.Count) cases; refusing to continue."
    }

    if ($Pilot72) {
        Reset-ProcessedTables -SelectedCases $selectedCases -Grid "pilot72" -Status "pilot72_selected" -RunLabel "Pilot72"
    }
    elseif ($ValidationSix) {
        Reset-ProcessedTables -SelectedCases $selectedCases -Grid "validation_six" -Status "validation_selected" -RunLabel "Validation-only"
    }
    elseif ($AerosolOverrideSmoke) {
        Initialize-AerosolOverrideSmokeTables -SelectedCases $selectedCases
    }
    elseif ($ProductionNirMwir) {
        $existingProductionManifest = $false
        if (Test-Path -LiteralPath $processedManifest -PathType Leaf) {
            $existingRows = @(Import-Csv -LiteralPath $processedManifest)
            $existingProductionManifest = @($existingRows | Where-Object { $_.grid -eq "production_nir_mwir" }).Count -gt 0
        }
        $forceFreshProduction = ($BatchName -eq "NIR_Transmittance" -and -not $Resume)
        if (-not $existingProductionManifest -or $forceFreshProduction) {
            Write-Host "Initializing processed tables for ProductionNirMwir from frozen pilot/smoke snapshot."
            Initialize-ProductionProcessedTables -AllCases $cases
        }
    }

    Write-Host "MODTRAN single-thread run"
    Write-Host "Manifest: $Manifest"
    Write-Host "PcModWin root: $($pcRoot.Path)"
    Write-Host "Executable: $exePath"
    Write-Host "Cases selected: $($selectedCases.Count)"
    if ($NoDeleteRaw) {
        Write-Host "NoDeleteRaw: preserving success artifacts for every selected case."
    }
    elseif ($ProductionNirMwir) {
        Write-Host "Production raw retention: preserving failed cases and deterministic success samples only."
    }

    $failures = New-Object System.Collections.Generic.List[object]
    $index = 0
    foreach ($case in $selectedCases) {
        $index += 1
        Write-Host "[$index/$($selectedCases.Count)] $($case.case_id)"
        try {
            $modinSource = Join-Path $rootPath $case.modin_file
            if (-not (Test-Path -LiteralPath $modinSource -PathType Leaf)) {
                throw "Generated modin not found: $modinSource. Re-run dry-run generation before real execution."
            }

            $modout1 = Join-Path $pcUsr "MODOUT1"
            $modout2 = Join-Path $pcUsr "MODOUT2"
            $beforeModout2 = Get-FileStamp $modout2
            $beforeTape6 = Get-FileStamp (Join-Path $pcBin "TAPE6")
            $beforeTape7 = Get-FileStamp (Join-Path $pcBin "TAPE7")
            $beforeTape8 = Get-FileStamp (Join-Path $pcBin "TAPE8")

            Copy-Item -LiteralPath $modinSource -Destination (Join-Path $pcBin "modin") -Force
            Copy-Item -LiteralPath $modinSource -Destination (Join-Path $pcBin "tape5") -Force
            Push-Location -LiteralPath $pcBin
            try {
                $caseRunLog = Join-Path $generatedDir "last_modtran_stdout.log"
                & $exePath *> $caseRunLog
                if ($LASTEXITCODE -ne 0) {
                    throw "MODTRAN executable returned exit code $LASTEXITCODE. See $caseRunLog"
                }
            }
            finally {
                Pop-Location
            }

            Sync-TapeOutputs -PcBin $pcBin -PcUsr $pcUsr -BeforeTape6 $beforeTape6 -BeforeTape7 $beforeTape7 -BeforeTape8 $beforeTape8

            if (-not (Test-Path -LiteralPath $modout2 -PathType Leaf)) {
                throw "MODOUT2 not found after run: $modout2"
            }
            $afterModout2 = Get-FileStamp $modout2
            if ($null -ne $beforeModout2 -and $afterModout2.LastWriteTimeUtc -le $beforeModout2.LastWriteTimeUtc) {
                throw "MODOUT2 did not appear to refresh. Before=$($beforeModout2.LastWriteTimeUtc) After=$($afterModout2.LastWriteTimeUtc)"
            }
            if (-not (Test-Modout2TableHeader $modout2)) {
                throw "MODOUT2 does not contain a supported success table header."
            }

            $outputCsv = Get-OutputCsvForMode -Mode $case.mode
            if ($AerosolOverrideSmoke) {
                $caseQcReport = Join-Path $processedDir "qc_aerosol_override_smoke.md"
            }
            else {
                $caseQcReport = Join-Path $processedDir "qc_report.md"
            }
            $parserArgs = @(
                $parserPath,
                "--input", $modout2,
                "--band", $case.band,
                "--mode", $case.mode,
                "--output", $outputCsv,
                "--append",
                "--case-id", $case.case_id,
                "--source-file", $case.modin_file,
                "--atmosphere-model", $case.atmosphere_model,
                "--aerosol-model", $case.aerosol_model,
                "--humidity-profile", $case.humidity_profile,
                "--visibility-km", $case.visibility_km,
                "--observer-alt-km", $case.observer_alt_km,
                "--target-alt-km", $case.target_alt_km,
                "--range-km", $case.range_km,
                "--solar-zenith-deg", $case.solar_zenith_deg,
                "--qc-report", $caseQcReport
            )
            & $Python @parserArgs
            if ($LASTEXITCODE -ne 0) {
                throw "parse_modout2.py failed with exit code $LASTEXITCODE"
            }

            $copySuccessArtifacts = $true
            if ($ProductionNirMwir -and -not $NoDeleteRaw) {
                $copySuccessArtifacts = ($index -eq 1 -or ($index % 100) -eq 0)
            }
            if ($AerosolOverrideSmoke) {
                $copySuccessArtifacts = $true
            }
            if ($copySuccessArtifacts) {
                Copy-CaseArtifacts -Case $case -DestinationRoot $samplesRoot -PcUsr $pcUsr
            }
            if ($ProductionNirMwir) {
                Update-ManifestCaseStatus -CaseId $case.case_id -Status "production_nir_mwir_succeeded"
            }
        }
        catch {
            $message = $_.Exception.Message
            Copy-CaseArtifacts -Case $case -DestinationRoot $failedRoot -PcUsr $pcUsr -Reason $message
            if ($ProductionNirMwir) {
                Update-ManifestCaseStatus -CaseId $case.case_id -Status "production_nir_mwir_failed"
            }
            $failures.Add([PSCustomObject]@{ case_id = $case.case_id; reason = $message }) | Out-Null
            Write-Warning $message
            $failureCsv = $(if ($AerosolOverrideSmoke) { Join-Path $processedDir "aerosol_override_smoke_failed_cases.csv" } else { Join-Path $processedDir "failed_cases.csv" })
            $failures | Export-Csv -LiteralPath $failureCsv -NoTypeInformation -Encoding UTF8
            throw "Stopping after failed MODTRAN case $($case.case_id). See $failureCsv and raw\failed."
        }
    }

    if ($failures.Count -gt 0) {
        $failureCsv = Join-Path $processedDir "failed_cases.csv"
        $failures | Export-Csv -LiteralPath $failureCsv -NoTypeInformation -Encoding UTF8
        throw "$($failures.Count) MODTRAN case(s) failed. See $failureCsv and raw\failed."
    }

    if ($Pilot72 -or $ValidationSix) {
        $status = $(if ($Pilot72) { "pilot72_succeeded" } else { "validation_succeeded" })
        $grid = $(if ($Pilot72) { "pilot72" } else { "validation_six" })
        Write-RunManifest -SelectedCases $selectedCases -Grid $grid -Status $status
        $bandArgs = @(
            $bandLutBuilderPath,
            "--processed-dir", $processedDir
        )
        & $Python @bandArgs
        if ($LASTEXITCODE -ne 0) {
            throw "build_band_lut.py failed with exit code $LASTEXITCODE"
        }
    }
    elseif ($VisibilitySmoke18) {
        Append-RunManifest -SelectedCases $selectedCases -Grid "visibility_smoke" -Status "visibility_smoke_succeeded"
        if (-not (Test-Path -LiteralPath $visibilityAuditPath -PathType Leaf)) {
            throw "Visibility audit script not found: $visibilityAuditPath"
        }
        & $Python $visibilityAuditPath "--processed-dir" $processedDir "--raw-dir" $samplesRoot
        if ($LASTEXITCODE -ne 0) {
            throw "check_visibility_effect.py reported visibility smoke failure with exit code $LASTEXITCODE"
        }
    }
    elseif ($AerosolOverrideSmoke) {
        Write-AerosolOverrideSmokeManifest -SelectedCases $selectedCases -Status "aerosol_override_smoke_succeeded"
        & $Python $aerosolOverrideBandBuilderPath "--processed-dir" $processedDir
        if ($LASTEXITCODE -ne 0) {
            throw "build_aerosol_override_smoke_band_lut.py failed with exit code $LASTEXITCODE"
        }
    }

    Write-Host "MODTRAN run complete."
}
finally {
    if (Test-Path -LiteralPath $lockPath) {
        Remove-Item -LiteralPath $lockPath -Force
    }
}
