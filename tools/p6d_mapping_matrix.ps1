$ErrorActionPreference='Stop'
# Extra display isolation; these are explicit diagnostics, not startup defaults.
foreach($mode in @('old','new')){
    $legacy=if($mode -eq 'old'){'1'}else{'0'}
    $diag=@{SensorDisplayPreset='Auto';P6DLegacyCache=$legacy;WorldCloudReferenceSteps='8';WorldCloudAudit='1'}
    $path="logs/p6d/mwir_auto_${mode}.json";$diag|ConvertTo-Json|Set-Content -Encoding UTF8 $path
    & "$PSScriptRoot/p6d_run_case.ps1" -Name "mapping_b2_${mode}_dynamic" -Normal -ProductionDefaults -Preset Runtime -Band 2 -Rate 30 -Seconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson $path *> "logs/p6d/mapping_b2_${mode}_dynamic.runner.log"
    if(-not $?){throw 'MWIR Auto dynamic case failed'}
}
$line=(Select-String -Path logs/p6d/mapping_b2_old_dynamic/board.log -Pattern '\[DisplayFrameMapping\]').Line
if($line -notmatch 'agcGain=([^ ]+) agcOffset=([^ ]+)'){throw 'Missing actual Auto mapping'}
$gain=$Matches[1];$offset=$Matches[2]
foreach($mode in @('old','new')){
    $legacy=if($mode -eq 'old'){'1'}else{'0'}
    $diag=@{SensorDisplayPreset='Auto';P6DLegacyCache=$legacy;WorldCloudReferenceSteps='8';WorldCloudAudit='1';P6DFrozenGain=$gain;P6DFrozenOffset=$offset}
    $path="logs/p6d/mwir_frozen_${mode}.json";$diag|ConvertTo-Json|Set-Content -Encoding UTF8 $path
    & "$PSScriptRoot/p6d_run_case.ps1" -Name "mapping_b2_${mode}_frozen" -Normal -ProductionDefaults -Preset Runtime -Band 2 -Rate 30 -Seconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson $path *> "logs/p6d/mapping_b2_${mode}_frozen.runner.log"
    if(-not $?){throw 'MWIR Auto frozen case failed'}
    $diag=@{SensorDisplayPreset='Game';P6DLegacyCache=$legacy;WorldCloudReferenceSteps='8';WorldCloudAudit='1'}
    $path="logs/p6d/nvg_fixed_${mode}.json";$diag|ConvertTo-Json|Set-Content -Encoding UTF8 $path
    & "$PSScriptRoot/p6d_run_case.ps1" -Name "mapping_b1_${mode}_fixed" -Normal -ProductionDefaults -Preset Runtime -Band 1 -Rate 30 -Seconds 4 -RawDump -CameraInput tools/p6b_inputs/pair_mid.json -DiagnosticJson $path *> "logs/p6d/mapping_b1_${mode}_fixed.runner.log"
    if(-not $?){throw 'NVG fixed case failed'}
}
