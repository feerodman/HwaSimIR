param(
    [switch]$Strict
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$modelSource = Join-Path $root 'HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp'
$appSource = Join-Path $root 'HwaSim_IR\HwaSim_IR\HwaSimIR.cpp'
$outputDir = Join-Path $root 'logs\p4_mwir_band_radiance_qc'
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

function Planck([double]$lambdaUm, [double]$temperatureK) {
    $c1 = 1.191042e8
    $c2 = 1.4387752e4
    return $c1 / ([Math]::Pow($lambdaUm, 5.0) * ([Math]::Exp($c2 / ($lambdaUm * $temperatureK)) - 1.0))
}

function MwirMean([double]$temperatureK) {
    return ((Planck 3.0 $temperatureK) + 4.0 * (Planck 3.5 $temperatureK) +
        2.0 * (Planck 4.0 $temperatureK) + 4.0 * (Planck 4.5 $temperatureK) +
        (Planck 5.0 $temperatureK)) / 12.0
}

$rows = foreach ($temperature in 250.0, 300.0, 500.0, 1000.0) {
    $mean = MwirMean $temperature
    $point = Planck 4.0 $temperature
    [pscustomobject]@{
        temperature_K = $temperature
        mwir_3_5_band_mean_W_m2_sr_um = $mean
        planck_4um_W_m2_sr_um = $point
        band_mean_to_4um_ratio = $mean / $point
    }
}
$csvPath = Join-Path $outputDir 'mwir_3_5_band_mean.csv'
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $csvPath

$modelText = Get-Content -Raw -LiteralPath $modelSource
$appText = Get-Content -Raw -LiteralPath $appSource
$checks = @(
    [pscustomobject]@{ Name='CPU uses 3-5 um band mean'; Pass=($modelText -match 'bandAveragePlanckRadianceWm2SrUm' -and $modelText -match 'b30 \+ 4\.0 \* b35 \+ 2\.0 \* b40 \+ 4\.0 \* b45 \+ b50') },
    [pscustomobject]@{ Name='GPU uses matching Simpson samples'; Pass=($appText -match 'P4MwirBandMeanPlanckWm2SrUm' -and $appText -match 'P4PlanckWm2SrUm\(3\.0' -and $appText -match 'P4PlanckWm2SrUm\(5\.0') },
    [pscustomobject]@{ Name='MWIR GPU restores local rear hotspot'; Pass=($appText -match 'rear_coverage \* max\(u_stage5_rear_hotspot_radiance') },
    [pscustomobject]@{ Name='MWIR GPU restores local brightspot'; Pass=($appText -match 'bright_coverage \* max\(u_stage5_brightspot_radiance') },
    [pscustomobject]@{ Name='Local heat replaces covered body radiance'; Pass=($appText -match 'm1_surface \* \(1\.0 - rear_coverage\) \+ local_rear_hotspot' -and $appText -match 'm1_surface \* \(1\.0 - bright_coverage\) \+ local_brightspot') },
    [pscustomobject]@{ Name='Body shader does not duplicate plume'; Pass=($appText -match 'plume node is intentionally not added here') }
)

$checks | Format-Table -AutoSize
$rows | Format-Table -AutoSize
$failed = @($checks | Where-Object { -not $_.Pass })
if ($failed.Count -gt 0 -and $Strict) {
    throw "P4 MWIR band radiance QC failed: $($failed.Name -join ', ')"
}
if ($failed.Count -gt 0) { exit 1 }
Write-Host "P4 MWIR band radiance QC PASS output=$csvPath"
