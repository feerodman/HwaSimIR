$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$last=Join-Path $root 'logs/p17/ir_retest/after/mwir_plume1_ir_v7/case_result.json'
$deadline=[DateTime]::UtcNow.AddMinutes(20)
while(-not (Test-Path $last)) {
    if([DateTime]::UtcNow -gt $deadline){throw 'Initial matrix did not complete'}
    Start-Sleep -Seconds 5
}
if((Get-Content -Raw $last | ConvertFrom-Json).result -ne 'PASS'){throw 'Initial matrix failed'}
Start-Sleep -Seconds 3
if(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue){throw 'Initial matrix still owns renderer clients'}
& (Join-Path $PSScriptRoot 'p17_ir_cloud_matrix.ps1')
foreach($weather in @('Clear','Rain','Snow')) {
    & (Join-Path $PSScriptRoot 'p17_dds_case.ps1') -Band MWIR -Weather $weather -EngineState 0 `
        -InputMode P14GroundWeather -DurationGuardSec 45 -DisplayPreset Game -LinearDiagnosticSeqs '900,1200' `
        -Name "mwir_$($weather.ToLower())_game_ir_v7" -OutputRoot logs/p17/ir_retest/after
}
& (Join-Path $PSScriptRoot 'p17_ir_retest.ps1') -Suite Detail
