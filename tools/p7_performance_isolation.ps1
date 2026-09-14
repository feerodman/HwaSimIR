$ErrorActionPreference='Stop'
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized test board.'}
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
foreach($case in @('new_auto_record','new_auto_norecord','old_auto_norecord','new_frozen_record','new_game_record')){
    $options=@{Name="D_cost_$case";Normal=$true;ProductionDefaults=$true;CameraInput='tools/p7_inputs/pair_rain_asset.json';Band=1;Rate=60;Seconds=20;CaptureSeconds=8;Weather=2;SaveMp4=1}
    if($case.EndsWith('norecord')){$options.SaveMp4=0}
    if($case.StartsWith('old')){$options.ReceiverExe='releases/windows/P7_HEAD_rollback2/receiver/HwaSim_IR_VideoDisplay.exe'}
    if($case -eq 'new_frozen_record'){$options.DiagnosticJson='tools/p7_inputs/frozen_common.json'}
    if($case -eq 'new_game_record'){$options.DiagnosticJson='tools/p7_inputs/display_game.json'}
    & tools/p7_run_case.ps1 @options *> "logs/p7/D_cost_${case}_runner.log"
}
