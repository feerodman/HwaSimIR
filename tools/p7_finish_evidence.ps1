$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized test board.'}
foreach($variant in @('on','off')) {
    $diagnostic=if($variant -eq 'on'){'rain_default_frozen'}else{'rain_default_frozen_off'}
    & tools/p7_run_case.ps1 -Name "D_rain_default_frozen_$variant" -Normal -ProductionDefaults -CameraInput tools/p7_inputs/pair_rain_asset.json -Band 1 -Rate 30 -Seconds 8 -CaptureSeconds 8 -Weather 2 -DiagnosticJson "tools/p7_inputs/$diagnostic.json" -RawDump -DumpSeq 90 *> "logs/p7/D_rain_default_frozen_${variant}_runner.log"
}
& tools/p7_run_case.ps1 -Name D_packaged_pair20 -Normal -ProductionDefaults -CameraInput tools/p7_inputs/pair_rain_asset.json -Band 1 -Rate 20 -Seconds 10 -CaptureSeconds 8 -Weather 2 -RepeatWeather 3 -PauseStart 3 -PauseDuration 2 -SenderExe releases/windows/P7_C4R2/sender/DataDrivenTestQT.exe -ReceiverExe releases/windows/P7_C4R2/receiver/HwaSim_IR_VideoDisplay.exe *> logs/p7/D_packaged_pair20_runner.log
& tools/p7_cloud_ab_windows.ps1 -Band 2 *> logs/p7/C3_cloud_ab_band2_runner.log
