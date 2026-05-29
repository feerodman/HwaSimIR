param(
    [string]$Manifest = "",
    [string]$PcModWinRoot = "F:\Programs\PcModWin5",
    [Alias("Executable")]
    [string]$ModtranExe = "",
    [string]$Python = "python",
    [string[]]$CaseIds = @(),
    [switch]$DefaultProductionFailSet,
    [ValidateRange(1, 20)]
    [int]$CaseLimit = 20
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootPath = (Resolve-Path -LiteralPath (Join-Path $scriptDir "..\..")).Path
$modtranRoot = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\Config\Atmosphere\MODTRAN"
$processedDir = Join-Path $modtranRoot "processed"
$generatedDir = Join-Path $modtranRoot "generated"
$samplesRoot = Join-Path $modtranRoot "raw\samples"
$failedRoot = Join-Path $modtranRoot "raw\failed"
$parserPath = Join-Path $scriptDir "parse_modout2.py"
$bandBuilderPath = Join-Path $scriptDir "build_targeted_diagnosis_band_lut.py"
if ([string]::IsNullOrWhiteSpace($Manifest)) {
    $Manifest = Join-Path $processedDir "manifest.csv"
}

function Resolve-ModtranExe {
    param([string]$Root, [string]$Exe)
    if ([string]::IsNullOrWhiteSpace($Exe)) {
        throw "Targeted diagnosis requires -ModtranExe."
    }
    if (-not (Test-Path -LiteralPath $Exe -PathType Leaf)) {
        throw "MODTRAN executable not found: $Exe"
    }
    return (Resolve-Path -LiteralPath $Exe).Path
}

function Get-FileStamp {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $null
    }
    $item = Get-Item -LiteralPath $Path
    return [PSCustomObject]@{ LastWriteTimeUtc = $item.LastWriteTimeUtc; Length = $item.Length }
}

function Test-Refreshed {
    param([object]$Before, [string]$Path)
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
    param([string]$PcBin, [string]$PcUsr, [object]$BeforeTape6, [object]$BeforeTape7, [object]$BeforeTape8)
    $map = @(
        @{ Source = "TAPE6"; Dest = "MODOUT1"; Before = $BeforeTape6 },
        @{ Source = "TAPE7"; Dest = "MODOUT2"; Before = $BeforeTape7 },
        @{ Source = "TAPE8"; Dest = "MODOUT3"; Before = $BeforeTape8 }
    )
    foreach ($entry in $map) {
        $source = Join-Path $PcBin $entry.Source
        if (Test-Refreshed -Before $entry.Before -Path $source) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $PcUsr $entry.Dest) -Force
        }
    }
}

function Test-Modout2TableHeader {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return $false
    }
    $text = Get-Content -LiteralPath $Path -TotalCount 80 -Encoding UTF8 -ErrorAction SilentlyContinue
    $joined = $text -join "`n"
    return ($joined -match "FREQ" -and ($joined -match "COMBIN|TOT_TRANS|SOL TR"))
}

function Get-OutputCsvForMode {
    param([string]$Mode)
    switch ($Mode) {
        "DirectSolarIrradiance" { return (Join-Path $processedDir "targeted_diagnosis_solar_lut_spectral.csv") }
        "RadianceWithScattering" { return (Join-Path $processedDir "targeted_diagnosis_sky_lut_spectral.csv") }
        default { return (Join-Path $processedDir "targeted_diagnosis_path_lut_spectral.csv") }
    }
}

function Copy-CaseArtifacts {
    param([object]$Case, [string]$DestinationRoot, [string]$PcUsr, [string]$Reason = "")
    $caseDir = Join-Path $DestinationRoot $Case.case_id
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $modinSource = Join-Path $rootPath $Case.modin_file
    if (Test-Path -LiteralPath $modinSource -PathType Leaf) {
        Copy-Item -LiteralPath $modinSource -Destination (Join-Path $caseDir "modin") -Force
    }
    foreach ($name in @("MODOUT1", "MODOUT2")) {
        $source = Join-Path $PcUsr $name
        if (Test-Path -LiteralPath $source -PathType Leaf) {
            Copy-Item -LiteralPath $source -Destination (Join-Path $caseDir $name) -Force
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($Reason)) {
        $Reason | Set-Content -LiteralPath (Join-Path $caseDir "failure.txt") -Encoding UTF8
    }
}

$defaultCases = @(
    "MWIR_transmittance_obs5_tar5_rng1_vis2_aerRural_humdefault",
    "MWIR_transmittance_obs5_tar5_rng1_vis5_aerRural_humdefault",
    "MWIR_transmittance_obs5_tar5_rng1_vis10_aerRural_humdefault",
    "MWIR_transmittance_obs5_tar5_rng1_vis23_aerRural_humdefault",
    "MWIR_transmittance_obs5_tar5_rng1_vis50_aerRural_humdefault",
    "NIR_transmittance_obs10_tar5_rng50_vis2_aerRural_humdefault",
    "NIR_transmittance_obs10_tar5_rng50_vis5_aerRural_humdefault",
    "NIR_transmittance_obs10_tar5_rng50_vis10_aerRural_humdefault",
    "NIR_transmittance_obs10_tar5_rng50_vis23_aerRural_humdefault",
    "NIR_transmittance_obs10_tar5_rng50_vis50_aerRural_humdefault",
    "MWIR_thermal_obs3_tar3_rng1_vis2_aerRural_humdefault",
    "MWIR_thermal_obs3_tar3_rng2_vis2_aerRural_humdefault",
    "MWIR_thermal_obs3_tar3_rng5_vis2_aerRural_humdefault",
    "MWIR_thermal_obs3_tar3_rng10_vis2_aerRural_humdefault"
)

if ($DefaultProductionFailSet) {
    $CaseIds = $defaultCases
}
if ($CaseIds.Count -le 0) {
    throw "No targeted CaseIds selected. Use -DefaultProductionFailSet or -CaseIds."
}
if ($CaseIds.Count -gt 20 -or $CaseIds.Count -gt $CaseLimit) {
    throw "Targeted diagnosis is capped at 20 cases. Requested=$($CaseIds.Count) CaseLimit=$CaseLimit"
}

$pcRoot = Resolve-Path -LiteralPath $PcModWinRoot
$pcBin = Join-Path $pcRoot.Path "bin"
$pcUsr = Join-Path $pcRoot.Path "usr"
$exePath = Resolve-ModtranExe -Root $pcRoot.Path -Exe $ModtranExe
$cases = @(Import-Csv -LiteralPath $Manifest)
$selectedCases = @()
foreach ($caseId in $CaseIds) {
    $match = @($cases | Where-Object { $_.case_id -eq $caseId })
    if ($match.Count -ne 1) {
        throw "CaseId not found exactly once in manifest: $caseId"
    }
    $selectedCases += $match[0]
}

foreach ($path in @(
    "targeted_diagnosis_path_lut_spectral.csv",
    "targeted_diagnosis_solar_lut_spectral.csv",
    "targeted_diagnosis_sky_lut_spectral.csv",
    "targeted_diagnosis_band_lut.csv",
    "targeted_diagnosis_manifest.csv"
)) {
    $full = Join-Path $processedDir $path
    if (Test-Path -LiteralPath $full -PathType Leaf) {
        Remove-Item -LiteralPath $full -Force
    }
}

Write-Host "Targeted diagnosis cases selected: $($selectedCases.Count)"
Write-Host "PcModWin root: $($pcRoot.Path)"
Write-Host "Executable: $exePath"

$failures = New-Object System.Collections.Generic.List[object]
$index = 0
foreach ($case in $selectedCases) {
    $index += 1
    Write-Host "[$index/$($selectedCases.Count)] $($case.case_id)"
    try {
        $modinSource = Join-Path $rootPath $case.modin_file
        if (-not (Test-Path -LiteralPath $modinSource -PathType Leaf)) {
            throw "Generated modin not found: $modinSource"
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
            $caseRunLog = Join-Path $generatedDir "last_targeted_diagnosis_stdout.log"
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
            throw "MODOUT2 did not appear to refresh."
        }
        if (-not (Test-Modout2TableHeader $modout2)) {
            throw "MODOUT2 does not contain a supported success table header."
        }

        $outputCsv = Get-OutputCsvForMode -Mode $case.mode
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
            "--solar-zenith-deg", $case.solar_zenith_deg
        )
        & $Python @parserArgs
        if ($LASTEXITCODE -ne 0) {
            throw "parse_modout2.py failed with exit code $LASTEXITCODE"
        }
        Copy-CaseArtifacts -Case $case -DestinationRoot $samplesRoot -PcUsr $pcUsr
    }
    catch {
        $message = $_.Exception.Message
        Copy-CaseArtifacts -Case $case -DestinationRoot $failedRoot -PcUsr $pcUsr -Reason $message
        $failures.Add([PSCustomObject]@{ case_id = $case.case_id; reason = $message }) | Out-Null
        $failureCsv = Join-Path $processedDir "targeted_diagnosis_failed_cases.csv"
        $failures | Export-Csv -LiteralPath $failureCsv -NoTypeInformation -Encoding UTF8
        throw "Stopping after failed targeted diagnosis case $($case.case_id). See $failureCsv and raw\failed."
    }
}

$selectedCases | Select-Object *, @{Name="targeted_status";Expression={"targeted_diagnosis_succeeded"}} |
    Export-Csv -LiteralPath (Join-Path $processedDir "targeted_diagnosis_manifest.csv") -NoTypeInformation -Encoding UTF8

& $Python $bandBuilderPath "--processed-dir" $processedDir
if ($LASTEXITCODE -ne 0) {
    throw "build_targeted_diagnosis_band_lut.py failed with exit code $LASTEXITCODE"
}

Write-Host "Targeted diagnosis completed successfully."
