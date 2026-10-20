param([string]$OutputDirectory = ('logs/p20m3/verify_' + (Get-Date -Format yyyyMMdd_HHmmss)))
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$out = Join-Path $root $OutputDirectory
if (Test-Path -LiteralPath $out) { throw 'Use a new verification directory; existing evidence is retained.' }
New-Item -ItemType Directory -Path $out | Out-Null
$compiler = 'D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe'
$parameters = Join-Path $root 'HwaSim_IR/Bin/Config/Atmosphere/Engineering/P20M3_GREY_GAS_1/parameters.txt'
$quality = Get-Content -LiteralPath (Join-Path (Split-Path $parameters) 'quality.json') -Raw | ConvertFrom-Json
Push-Location $root
try {
    & $compiler -std=c++11 -O2 -static tools/p20m3_provider_check.cpp -o (Join-Path $out 'provider_check.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Provider check build failed' }
    & (Join-Path $out 'provider_check.exe') $parameters $quality.parameterSha256 1> (Join-Path $out 'provider_scan.json') 2> (Join-Path $out 'provider_boundary_scan.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Provider numerical verification failed' }
    & $compiler -std=c++11 -O2 -static tools/p20m3_planck_check.cpp HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.cpp -o (Join-Path $out 'planck_check.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Planck check build failed' }
    & (Join-Path $out 'planck_check.exe') > (Join-Path $out 'planck_check.json')
    if ($LASTEXITCODE -ne 0) { throw 'Planck table accuracy failed' }
    [ordered]@{status='PASS';quality='ENGINEERING_APPROX';parameterSha256=(Get-FileHash -LiteralPath $parameters -Algorithm SHA256).Hash.ToLower();strictModtranAccuracy='PENDING';scope='Numerical provider and Planck table only; no runtime/performance claim'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'verification.json') -Encoding UTF8
} finally { Pop-Location }
Write-Output $out
