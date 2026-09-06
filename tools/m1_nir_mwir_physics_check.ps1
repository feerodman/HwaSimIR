$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
python (Join-Path $PSScriptRoot "test_modtran_units.py")
if ($LASTEXITCODE -ne 0) { throw "MODTRAN unit tests failed" }
python (Join-Path $PSScriptRoot "modtran_qc.py")
if ($LASTEXITCODE -ne 0) { throw "M1 MODTRAN QC failed" }
Write-Host "M1 NIR/MWIR physics checks PASS"
