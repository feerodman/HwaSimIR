param([string]$OutputDirectory = "")

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $root ("logs\l2-active-qc-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

python (Join-Path $PSScriptRoot "l2_material_deployment_check.py")
if ($LASTEXITCODE -ne 0) { throw "material deployment check failed" }

$gxx = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$unitExe = Join-Path $OutputDirectory "l2_active_illuminator_unit.exe"
$unitCsv = Join-Path $OutputDirectory "l2_active_illuminator_qc.csv"
& $gxx -std=c++11 -O2 `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\l2_active_illuminator_unit.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRActiveIlluminator.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRTypes.cpp") `
    -o $unitExe
if ($LASTEXITCODE -ne 0) { throw "L2 unit compile failed" }
& $unitExe $unitCsv
if ($LASTEXITCODE -ne 0) { throw "L2 active illuminator unit failed" }

$failed = @(Import-Csv $unitCsv | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) { throw "L2 QC contains failed rows" }
Write-Host "L2 active illuminator QC PASS"
Write-Host "qcCsv=$unitCsv"
