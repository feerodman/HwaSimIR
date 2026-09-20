param(
    [string]$FormalPath='HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed\band_lut_si.csv',
    [string]$QueryManifest='logs\p13\input_audit\input_1_query_manifest.csv',
    [string]$OutputDirectory='logs\p13\atmosphere\track50\query_check'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$formal=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($FormalPath)){$FormalPath}else{Join-Path $root $FormalPath}))
$queries=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($QueryManifest)){$QueryManifest}else{Join-Path $root $QueryManifest}))
$out=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($OutputDirectory)){$OutputDirectory}else{Join-Path $root $OutputDirectory}))
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler='D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
$exe=Join-Path $out 'p13_track_query.exe'
$log=Join-Path $out 'p13_track_query.log'
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic -I (Join-Path $root 'HwaSim_IR\HwaSim_IR\IR') `
    (Join-Path $root 'tools\p13_track_query.cpp') `
    (Join-Path $root 'HwaSim_IR\HwaSim_IR\IR\IRModtranRadianceLut.cpp') -o $exe
if($LASTEXITCODE -ne 0){throw 'P13 query probe compilation failed'}
$output=& $exe $formal $queries | Out-String
$exitCode=$LASTEXITCODE
[IO.File]::WriteAllText($log,$output,[Text.UTF8Encoding]::new($false))
Write-Output $output.TrimEnd()
if($exitCode -ne 0){throw "P13 production query probe failed: $log"}
