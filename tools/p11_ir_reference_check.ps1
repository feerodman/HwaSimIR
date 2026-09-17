param(
    [string]$OutputDirectory = "logs/p11/reference/current",
    [switch]$RecordBaseline,
    [switch]$SkipGpu
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null
$compiler = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
if (-not (Test-Path -LiteralPath $compiler)) { throw "Missing compiler: $compiler" }
$probeExe = Join-Path $out "p11_ir_production_probe.exe"
$probeJson = Join-Path $out "production_probe.json"
$gpuProbeJson = Join-Path $out "gpu_probe.json"
& $compiler -std=c++11 -O2 -Wall -Wextra -pedantic `
    -I (Join-Path $root "HwaSim_IR\HwaSim_IR\IR") `
    (Join-Path $root "tools\p11_ir_production_probe.cpp") `
    (Join-Path $root "HwaSim_IR\HwaSim_IR\IR\IRRadianceModelV2.cpp") `
    -o $probeExe
if ($LASTEXITCODE -ne 0) { throw "P11 production probe compilation failed" }
$probeText = & $probeExe | Out-String
if ($LASTEXITCODE -ne 0) { throw "P11 production probe failed" }
[IO.File]::WriteAllText($probeJson, $probeText, [Text.UTF8Encoding]::new($false))
if (-not $SkipGpu) {
    $pandaPython = "F:\Programs\Panda3D-1.10.15-x64\python\ppython.exe"
    if (-not (Test-Path -LiteralPath $pandaPython)) { throw "Missing Panda Python: $pandaPython" }
    & $pandaPython (Join-Path $root "tools\p11_gpu_radiance_probe.py") --output-dir $out
    if ($LASTEXITCODE -ne 0) { throw "P11 GPU radiance probe failed" }
}
$arguments = @(
    (Join-Path $root "tools\p11_ir_physics_reference.py"),
    "--production-probe", $probeJson,
    "--output-dir", $out
)
if (-not $SkipGpu) { $arguments += @("--gpu-probe", $gpuProbeJson) }
if ($RecordBaseline) { $arguments += "--record-baseline" }
& python @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
