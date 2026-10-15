[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
foreach($case in @(@('Clear',0,'clear'),@('Clear',1,'plume'),@('Overcast',1,'overcast_plume'))){
    $name="p17_perf_original_mwir_$($case[2])"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'p17_dds_case.ps1') -Band MWIR -Weather $case[0] -EngineState $case[1] -Name $name -InputMode Original -DurationGuardSec 100 -OutputRoot logs/p17/production/performance -PerformanceOnly -RenderPerfProbe
    if($LASTEXITCODE -ne 0){throw "Acquisition failed: $name"}
}
