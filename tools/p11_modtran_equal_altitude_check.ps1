param(
    [string]$OutputDirectory = "logs\p11\modtran\equal_altitude_query"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$exe = Join-Path $out "p11_modtran_equal_altitude_unit.exe"
$log = Join-Path $out "p11_modtran_equal_altitude_unit.log"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\p11_modtran_equal_altitude_unit.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp") `
    -o $exe
if ($LASTEXITCODE -ne 0) { throw "Equal-altitude query probe compilation failed" }
$output = & $exe $out | Out-String
$exitCode = $LASTEXITCODE
[IO.File]::WriteAllText($log, $output, [Text.UTF8Encoding]::new($false))
Write-Output $output.TrimEnd()
if ($exitCode -ne 0) { throw "Equal-altitude query check failed: $log" }
