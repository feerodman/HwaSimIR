param(
    [string]$CandidatePath = "logs\p11\modtran\mwir_ground_grid\formal_mwir_rows.csv",
    [string]$FormalPath = "HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv",
    [string]$OutputDirectory = "logs\p11\modtran\mwir_ground_grid\query_check"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$candidate = if ([IO.Path]::IsPathRooted($CandidatePath)) { $CandidatePath } else { Join-Path $root $CandidatePath }
$formal = if ([IO.Path]::IsPathRooted($FormalPath)) { $FormalPath } else { Join-Path $root $FormalPath }
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$exe = Join-Path $out "p11_modtran_mwir_grid_query.exe"
$log = Join-Path $out "p11_modtran_mwir_grid_query.log"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\p11_modtran_mwir_grid_query.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp") `
    -o $exe
if ($LASTEXITCODE -ne 0) { throw "MWIR grid query probe compilation failed" }
$output = & $exe $candidate $formal | Out-String
$exitCode = $LASTEXITCODE
[IO.File]::WriteAllText($log, $output, [Text.UTF8Encoding]::new($false))
Write-Output $output.TrimEnd()
if ($exitCode -ne 0) { throw "MWIR grid query check failed: $log" }
