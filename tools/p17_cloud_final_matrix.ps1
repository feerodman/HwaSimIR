$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$camera='logs/p17/cloud_rework/fixtures_v2/cloud_above.json'
foreach($band in @(@{name='mwir';code=2},@{name='swir';code=0})) {
    foreach($weather in @(@{name='cloudy';code=1},@{name='overcast';code=5},@{name='clear';code=0})) {
        $name="p17_$($band.name)_$($weather.name)_white_v12"
        if(Test-Path (Join-Path $root "logs/p7/$name")){throw "Refusing to overwrite $name"}
        & "$PSScriptRoot/p7_run_case.ps1" -Name $name -Normal -ProductionDefaults -CameraInput $camera `
            -DiagnosticJson logs/p17/cloud_rework/fixtures_v2/white_raw.json -Band $band.code -Weather $weather.code `
            -Rate 60 -Seconds 31 -CaptureSeconds 31 -PixelAngle 200 -RawDump -DumpSeq 180
    }
    foreach($mode in @('white','radiometric')) {
        $name="p17_$($band.name)_cloud_${mode}_frozen_v12"
        if(Test-Path (Join-Path $root "logs/p7/$name")){throw "Refusing to overwrite $name"}
        & "$PSScriptRoot/p7_run_case.ps1" -Name $name -Normal -ProductionDefaults -CameraInput $camera `
            -DiagnosticJson "logs/p17/cloud_rework/fixtures_v2/${mode}_frozen.json" -Band $band.code -Weather 1 `
            -Rate 60 -Seconds 8 -CaptureSeconds 8 -PixelAngle 200 -RawDump -DumpSeq 180
    }
}
