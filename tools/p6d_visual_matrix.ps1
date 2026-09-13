param([string]$Prefix='visual')
$ErrorActionPreference='Stop'
foreach($band in @(1,2)){
    foreach($view in @('pair_separated','pair_overlap','pair_far','near_translate','proxy_crossing','pair_pan')){
        $input=if($view -in @('near_translate','proxy_crossing')){"tools/p6d_inputs/$view.json"}else{"tools/p6b_inputs/$view.json"}
        $duration=if($view -in @('near_translate','proxy_crossing','pair_pan')){24}else{6}
        $name="${Prefix}_b${band}_$view"
        & "$PSScriptRoot/p6d_run_case.ps1" -Name $name -Normal -ProductionDefaults -Preset Runtime -Band $band -Rate 30 -Seconds $duration -CaptureSeconds $duration -RawDump -CameraInput $input -DiagnosticJson "tools/p6d_inputs/b${band}_base.json" *> "logs/p6d/$name.runner.log"
        if(-not $?){throw "Visual case failed: $name"}
    }
    foreach($case in @('base','volumes','near','far','zero','sheet','plate_front','plate_back','steps64','numeric')){
        $name="${Prefix}_b${band}_$case"
        & "$PSScriptRoot/p6d_run_case.ps1" -Name $name -Normal -ProductionDefaults -Preset Runtime -Band $band -Rate 30 -Seconds 4 -CaptureSeconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson "tools/p6d_inputs/b${band}_$case.json" *> "logs/p6d/$name.runner.log"
        if(-not $?){throw "Layer case failed: $name"}
    }
}
