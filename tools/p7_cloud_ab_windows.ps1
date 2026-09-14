param([int]$Band=1)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$file=Join-Path $root 'HwaSim_IR/Bin/Config/Weather/world_cloud_game.json'
$bytes=[IO.File]::ReadAllBytes($file)
$originalHash=(Get-FileHash $file).Hash
$temporary=$file+'.p7-diagnostic-tmp'
$value=[Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff)|ConvertFrom-Json
$value.Appearance.Manifest='Weather/Derived/cloud_manifest_p6d.json'
try {
    [IO.File]::WriteAllText($temporary,($value|ConvertTo-Json -Depth 20),[Text.UTF8Encoding]::new($false))
    & F:\Programs\anaconda3\python.exe -c "import os,sys;os.replace(sys.argv[1],sys.argv[2])" $temporary $file
    if($LASTEXITCODE -ne 0){throw "Atomic appearance replacement failed"}
    & (Join-Path $PSScriptRoot 'p7_run_case.ps1') -Name "C3_cloud_before_band$Band" -Windows -SkipRemoteCleanup -Normal -ProductionDefaults -CameraInput tools/p6b_inputs/pair_pan.json -Band $Band -Rate 30 -Seconds 12 -CaptureSeconds 12 -Weather 1 -DiagnosticJson tools/p7_inputs/frozen_cloud.json
} finally {
    [IO.File]::WriteAllBytes($temporary,$bytes)
    & F:\Programs\anaconda3\python.exe -c "import os,sys;os.replace(sys.argv[1],sys.argv[2])" $temporary $file
    if($LASTEXITCODE -ne 0){throw "Atomic appearance replacement failed"}
    if((Get-FileHash $file).Hash -ne $originalHash){throw 'Cloud appearance restore hash mismatch'}
}
& (Join-Path $PSScriptRoot 'p7_run_case.ps1') -Name "C3_cloud_after_band$Band" -Windows -SkipRemoteCleanup -Normal -ProductionDefaults -CameraInput tools/p6b_inputs/pair_pan.json -Band $Band -Rate 30 -Seconds 12 -CaptureSeconds 12 -Weather 1 -DiagnosticJson tools/p7_inputs/frozen_cloud.json
