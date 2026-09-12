# Serial end-to-end checks for the final ELF and final Windows receiver.
param([switch]$DiagnosticsOnly,[switch]$SkipDiagnostics)
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p5_run_case.ps1'
if(-not $SkipDiagnostics){
    & $runner -Name rk_release_sheet -Scene cloud -View above -Seconds 4 -Rate 20 -Gamma 2 -NoTargets -SmallClouds -VolumeOff
    & $runner -Name rk_release_gamma2_blend -Scene materials -Seconds 4 -Rate 20 -Band 1 -Blend -Gamma 2 -VolumeOff -SheetOff -RawDump
}
if($DiagnosticsOnly){return}
foreach($rate in @(20,30,60)){
    & $runner -Name "rk_release_sync$rate" -Scene mixed -View side -Seconds 10 -Rate $rate -SmallClouds
}
& $runner -Name rk_release_60s -Scene mixed -View side -Seconds 60 -Rate 60 -SmallClouds
& $runner -Name rk_release_600s -Scene mixed -View side -Seconds 600 -Rate 60 -SmallClouds
