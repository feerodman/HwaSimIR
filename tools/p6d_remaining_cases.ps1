$ErrorActionPreference='Stop'
# Resume after the recorded pre-render performance-policy readback failure.
# No failed capture is overwritten; visual_b2_near_failed_init is preserved.
foreach($case in @('near','far','zero','sheet','plate_front','plate_back','steps64','numeric')) {
    $name="visual_b2_$case"
    & "$PSScriptRoot/p6d_run_case.ps1" -Name $name -Normal -ProductionDefaults -Preset Runtime -Band 2 -Rate 30 -Seconds 4 -CaptureSeconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson "tools/p6d_inputs/b2_$case.json" *> "logs/p6d/$name.runner.log"
    if(-not $?){throw "Resumed visual case failed: $name"}
}
& "$PSScriptRoot/p6d_mapping_matrix.ps1"
& "$PSScriptRoot/p6d_default_views.ps1"
