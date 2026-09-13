param([switch]$Long)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Set-Location $root
$runner=Join-Path $PSScriptRoot 'p6b_run_case.ps1'
function Case([string]$Name,[string[]]$Flags){
    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Name $Name @Flags *> "logs/p6b/$Name.run.log"
    if($LASTEXITCODE -ne 0){throw "Case failed: $Name; see logs/p6b/$Name.run.log"}
}
$camera='tools/p6b_inputs/pair_mid.json'
if($Long){
    Case 'rk_final_600s' @('-Normal','-CameraInput',$camera,'-Seconds','600','-Rate','60')
    exit
}
Case 'rk_precision_smoke' @('-Normal','-CameraInput',$camera,'-Seconds','6','-Rate','30','-RawDump')
Case 'rk_final_fixed60' @('-Normal','-CameraInput',$camera,'-Seconds','60','-Rate','60','-RawDump')
Case 'rk_final_auto60' @('-Normal','-CameraInput',$camera,'-Seconds','60','-Rate','60','-Preset','Auto','-RawDump')
foreach($view in @('near','far','overlap','pan')){
    Case "rk_final_$view" @('-Normal','-CameraInput',"tools/p6b_inputs/pair_$view.json",'-Seconds',$(if($view -eq 'pan'){'24'}else{'8'}),'-Rate','30','-CaptureSeconds','24','-RawDump')
}
Case 'rk_final_plate' @('-Normal','-CameraInput',$camera,'-OrdinaryPlate','-Seconds','8','-Rate','30','-RawDump')
foreach($rate in @(20,30,60)){
    Case "rk_sync_pause_init_$rate" @('-Normal','-CameraInput',$camera,'-Seconds','8','-Rate',"$rate",'-PauseStart','3','-PauseDuration','2','-RepeatWeather','1')
}
Case 'rk_hdr_after' @('-Scene','display','-Preset','Auto','-AutoTargetHigh','0.35','-Seconds','6','-Rate','20','-RawDump')
foreach($preset in @('Legacy','Game','Black')){
    Case "rk_chart_$preset" @('-Scene','display','-Preset',$preset,'-Seconds','6','-Rate','20','-RawDump')
}
Case 'rk_auto_sample_timing' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-AgcTiming','-Seconds','8','-Rate','20','-RawDump')
Case 'rk_auto_reference_full' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-AutoReference','-Seconds','8','-Rate','20','-RawDump')
Case 'rk_normal_one' @('-Normal','-CameraInput',$camera,'-Clouds','1','-Seconds','8','-Rate','60','-RawDump')
Case 'rk_normal_four_budget' @('-Normal','-CameraInput',$camera,'-Clouds','4','-Seconds','8','-Rate','60','-RawDump')
Case 'rk_normal_nvg' @('-Normal','-CameraInput',$camera,'-Band','1','-Seconds','8','-Rate','30','-RawDump')
Case 'rk_target_vfx_regression' @('-ExistingTargets','-Scene','mixed','-TargetType','0x11','-Seconds','60','-Rate','60','-RawDump','-UiResponsive')

Case 'rk_four_test_entry' @('-Scene','mixed','-Clouds','4','-Seconds','10','-Rate','60','-RawDump')
