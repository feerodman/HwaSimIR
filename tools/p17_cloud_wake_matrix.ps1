param([string]$Suffix='v11')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$camera='logs/p17/cloud_rework/fixtures_v2/cloud_above.json'
foreach($band in @(@{name='mwir';code=2},@{name='swir';code=0})) {
    foreach($weather in @(@{name='cloudy';code=1},@{name='overcast';code=5},@{name='clear';code=0})) {
        $name="p17_$($band.name)_$($weather.name)_white_$Suffix"
        if(Test-Path (Join-Path $root "logs/p7/$name")){throw "Refusing to overwrite $name"}
        & "$PSScriptRoot/p7_run_case.ps1" -Name $name -Normal -ProductionDefaults -CameraInput $camera `
            -DiagnosticJson logs/p17/cloud_rework/fixtures_v2/white_raw.json -Band $band.code -Weather $weather.code `
            -Rate 60 -Seconds 31 -CaptureSeconds 31 -PixelAngle 200 -RawDump -DumpSeq 180
    }
    & "$PSScriptRoot/p7_run_case.ps1" -Name "p17_$($band.name)_cloudy_radiometric_$Suffix" -Normal -ProductionDefaults `
        -CameraInput $camera -DiagnosticJson logs/p17/cloud_rework/fixtures_v2/radiometric.json -Band $band.code -Weather 1 `
        -Rate 60 -Seconds 8 -CaptureSeconds 8 -PixelAngle 200 -RawDump -DumpSeq 180
}
foreach($band in @('MWIR','SWIR')) {
    foreach($engine in @(0,1)) {
        & "$PSScriptRoot/p17_dds_case.ps1" -Band $band -Weather Clear -EngineState $engine -InputMode Original `
            -LinearDiagnosticSeqs '900,1200,3600,4200' -Name "$($band.ToLower())_plume${engine}_$Suffix" `
            -OutputRoot logs/p17/cloud_rework/after
    }
    & "$PSScriptRoot/p17_dds_case.ps1" -Band $band -Weather Clear -EngineState 1 -InputMode Original -PixelAngleUrad 10 `
        -LinearDiagnosticSeqs '900,1200,3600,4200' -Name "$($band.ToLower())_wake_full_$Suffix" `
        -OutputRoot logs/p17/cloud_rework/after
}
foreach($weather in @('Clear','Rain','Snow')) {
    & "$PSScriptRoot/p17_dds_case.ps1" -Band MWIR -Weather $weather -EngineState 0 -InputMode P14GroundWeather `
        -DurationGuardSec 45 -LinearDiagnosticSeqs '900,1200' -Name "mwir_$($weather.ToLower())_$Suffix" `
        -OutputRoot logs/p17/cloud_rework/after
}
