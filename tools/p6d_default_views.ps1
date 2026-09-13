$ErrorActionPreference='Stop'
# NVG uses dynamic Auto for view acceptance. A mapping frozen at pair_mid is
# intentionally used only by the same-camera algorithm comparison, not reused
# to judge cameras whose background distribution is different.
foreach($view in @('pair_separated','pair_overlap','pair_far','near_translate','proxy_crossing')) {
    $input=if($view -in @('near_translate','proxy_crossing')){"tools/p6d_inputs/$view.json"}else{"tools/p6b_inputs/$view.json"}
    $duration=if($view -in @('near_translate','proxy_crossing')){24}else{6}
    $name="default_b1_$view"
    & "$PSScriptRoot/p6d_run_case.ps1" -Name $name -Normal -ProductionDefaults -Preset Runtime -Band 1 -Rate 30 -Seconds $duration -CaptureSeconds $duration -RawDump -CameraInput $input -DiagnosticJson tools/p6d_inputs/audit.json *> "logs/p6d/$name.runner.log"
    if(-not $?){throw "Default-view case failed: $name"}
}
