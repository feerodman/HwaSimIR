$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$candidates = @(
    (Join-Path $root "HwaSim_IR\HwaSim_IR\Bin\HwaSim_IR.exe"),
    (Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe")
)
$exe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $exe) {
    throw "HwaSim_IR Release executable was not found. Build Release first."
}
$pandaBin = "F:\Programs\Panda3D-1.10.15-x64\bin"
$opencvBin = Join-Path $root "HwaSim_IR\HwaSim_IR\opencv2-440\opencv_x64\vc14\bin"
$oldPath = $env:PATH
try {
    $env:PATH = "$pandaBin;$opencvBin;$env:PATH"
    & $exe --w15-cloud-model-check
} finally {
    $env:PATH = $oldPath
}
if ($LASTEXITCODE -ne 0) {
    throw "W1.5 deterministic world-grid check failed, exitCode=$LASTEXITCODE"
}
