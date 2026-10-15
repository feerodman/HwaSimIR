param([string]$Suffix='ir_v7')
$ErrorActionPreference='Stop'
foreach($band in @(@{name='swir';code=0},@{name='mwir';code=2})) {
    foreach($weather in @(@{name='clear';code=0},@{name='cloudy';code=1},@{name='overcast';code=5})) {
        $name="p17_$($band.name)_$($weather.name)_above_$Suffix"
        if(Test-Path (Join-Path (Split-Path -Parent $PSScriptRoot) "logs/p7/$name")) {
            throw "Refusing to overwrite evidence $name"
        }
        & (Join-Path $PSScriptRoot 'p7_run_case.ps1') -Name $name -Normal -ProductionDefaults `
            -CameraInput logs/p17/ir_retest/fixtures/cloud_above.json `
            -DiagnosticJson logs/p17/ir_retest/fixtures/audit.json `
            -Band $band.code -Weather $weather.code -Rate 60 -Seconds 31 -CaptureSeconds 31 -PixelAngle 200
    }
}
