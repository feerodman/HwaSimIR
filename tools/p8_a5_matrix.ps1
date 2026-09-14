param()
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p7_run_case.ps1'
$root=Split-Path $PSScriptRoot -Parent
foreach($entry in @(@('rain192',1,2,'precip_192.json'),@('snow128',2,3,'precip_128.json'),@('snow96',2,3,'precip_96.json'),@('rain128_ui_off',1,2,'precip_128.json'))){
    $name='P8_A5_high_'+$entry[0]
    $extra=@{}
    if($entry[0] -eq 'rain128_ui_off'){$extra.FreezeTelemetryTables=$true}
    & $runner -Name $name -LogGroup p8 -InputAudit -NoDuplicateH264 -Normal -ProductionDefaults -Band $entry[1] -Weather $entry[2] -Rate 60 -Seconds 65 -CaptureSeconds 0 -CameraInput (Join-Path $root 'tools/p7_inputs/pair_rain_asset.json') -DiagnosticJson (Join-Path $root ('tools/p8_inputs/'+$entry[3])) -PerformancePolicy /userdata/HwaSimIR/logs/p8_supported_high.conf -SenderExe (Join-Path $root 'releases/windows/P8_A5/sender/DataDrivenTestQT.exe') -ReceiverExe (Join-Path $root 'releases/windows/P8_A5/receiver/HwaSim_IR_VideoDisplay.exe') @extra *> (Join-Path $root "logs/p8/${name}_runner.log")
    if($LASTEXITCODE -ne 0){throw "Case failed: $name"}
}
