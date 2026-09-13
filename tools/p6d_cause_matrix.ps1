param([string]$Prefix='isolate',[int[]]$Bands=@(1,2),[string[]]$Cases=@('base','art1','uniform','volumes','near','far','background','sheet','steps8','steps16','steps32','steps64','density','alpha','source','zero','numeric'))
$ErrorActionPreference='Stop'
foreach($band in $Bands){foreach($case in $Cases){
    $name="${Prefix}_b${band}_$case"
    & "$PSScriptRoot/p6d_run_case.ps1" -Name $name -Normal -ProductionDefaults -Preset Runtime -Band $band -Rate 30 -Seconds 4 -CaptureSeconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson "tools/p6d_inputs/b${band}_$case.json" *> "logs/p6d/$name.runner.log"
    if(-not $?){throw "Isolation failed: $name"}
}}
