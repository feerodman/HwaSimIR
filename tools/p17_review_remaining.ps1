param([string]$Suffix='v7')
$ErrorActionPreference='Stop'
$caseRunner=Join-Path $PSScriptRoot 'p7_run_case.ps1'
foreach($weather in @(@{name='clear';value=0},@{name='overcast';value=5})) {
    & $caseRunner -Name "p17_cloud_$($weather.name)_$Suffix" -Normal -ProductionDefaults `
        -CameraInput logs/p17/review/fixtures/cloud_views.json -DiagnosticJson logs/p17/review/fixtures/cloud_audit.json `
        -Band 1 -Rate 60 -Seconds 31 -CaptureSeconds 31 -Weather $weather.value -PixelAngle 200
}
& $caseRunner -Name "p17_cloud_mwir_reference_$Suffix" -Normal -ProductionDefaults `
    -CameraInput logs/p17/review/fixtures/cloud_views_reference.json -DiagnosticJson logs/p17/review/fixtures/cloud_audit.json `
    -Band 2 -Rate 60 -Seconds 31 -CaptureSeconds 31 -Weather 1 -PixelAngle 200
& (Join-Path $PSScriptRoot 'p17_review_matrix.ps1') -Suite Weather -Suffix $Suffix
foreach($weather in @('Cloudy','Overcast')) {
    & (Join-Path $PSScriptRoot 'p17_dds_case.ps1') -Band NIR -Weather $weather -EngineState 0 `
        -TargetTypeCode 0x12 -Name "nir_f22_$($weather.ToLower())_$Suffix" -OutputRoot logs/p17/review/after
}
