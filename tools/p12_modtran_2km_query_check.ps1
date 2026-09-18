param(
    [string]$FormalPath='HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv',
    [string]$OutputDirectory='logs\p12\p12c\modtran_2km\query_check'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$formal=if([IO.Path]::IsPathRooted($FormalPath)){$FormalPath}else{Join-Path $root $FormalPath}
$out=if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $root $OutputDirectory}
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler='D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
$exe=Join-Path $out 'p12_modtran_2km_query.exe'
$log=Join-Path $out 'p12_modtran_2km_query.log'
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic -I (Join-Path $root 'HwaSim_IR\HwaSim_IR\IR') `
    (Join-Path $root 'tools\p12_modtran_2km_query.cpp') `
    (Join-Path $root 'HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp') -o $exe
if($LASTEXITCODE -ne 0){throw 'P12 2 km query probe compilation failed'}
$output=& $exe $formal | Out-String;$exitCode=$LASTEXITCODE
[IO.File]::WriteAllText($log,$output,[Text.UTF8Encoding]::new($false))
Write-Output $output.TrimEnd()
if($exitCode -ne 0){throw "P12 2 km query probe failed: $log"}
