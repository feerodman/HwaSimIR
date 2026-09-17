param(
    [string]$LutPath = "logs\p11\modtran\swir_ground_grid\formal_swir_rows.csv",
    [string]$OutputDirectory = "logs\p11\modtran\swir_ground_grid\query_check"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$lut = if ([IO.Path]::IsPathRooted($LutPath)) { $LutPath } else { Join-Path $root $LutPath }
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$exe = Join-Path $out "p11_modtran_swir_grid_query.exe"
$log = Join-Path $out "p11_modtran_swir_grid_query.log"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\p11_modtran_swir_grid_query.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp") `
    -o $exe
if ($LASTEXITCODE -ne 0) { throw "SWIR grid query probe compilation failed" }
$output = & $exe $lut | Out-String
$exitCode = $LASTEXITCODE
[IO.File]::WriteAllText($log, $output, [Text.UTF8Encoding]::new($false))
Write-Output $output.TrimEnd()
if ($exitCode -ne 0) { throw "SWIR grid query check failed: $log" }
