param([ValidateSet('smoke','regression','long')][string]$Phase='smoke')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Set-Location $root
$runner=Join-Path $PSScriptRoot 'p6b_run_case.ps1'
function Case([string]$Name,[string[]]$Flags){
    & powershell -NoProfile -ExecutionPolicy Bypass -File $runner -Name $Name @Flags *> "logs/p6b/$Name.run.log"
    if($LASTEXITCODE -ne 0){throw "Case failed: $Name; see logs/p6b/$Name.run.log"}
}
$camera='tools/p6b_inputs/pair_mid.json'
if($Phase -eq 'smoke'){
    Case 'rk_release_auto_smoke' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-Seconds','8','-Rate','30','-RawDump')
    Case 'rk_release_auto_no_sheet' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-SheetOff','-Seconds','8','-Rate','30','-RawDump')
    Case 'rk_release_fixed_smoke' @('-Normal','-CameraInput',$camera,'-Seconds','8','-Rate','30','-RawDump')
    exit
}
if($Phase -eq 'long'){
    Case 'rk_release_normal_600s' @('-Normal','-CameraInput','tools/p6b_inputs/pair_game_asset.json','-UseRuntimeStreaming','-Seconds','600','-Rate','60')
    exit
}
Case 'rk_release_separated' @('-Normal','-CameraInput','tools/p6b_inputs/pair_separated.json','-Seconds','8','-Rate','30','-RawDump')
Case 'rk_release_hide_near' @('-Normal','-CameraInput',$camera,'-HideCloudId','D73EEA5ECB3EA0B8','-Seconds','6','-Rate','30','-RawDump')
Case 'rk_release_hide_far' @('-Normal','-CameraInput',$camera,'-HideCloudId','198EE8338358C182','-Seconds','6','-Rate','30','-RawDump')
Case 'rk_release_auto60' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-Seconds','60','-Rate','60','-RawDump')
foreach($rate in @(20,30,60)){
    Case "rk_release_sync_$rate" @('-Normal','-CameraInput',$camera,'-Seconds','6','-Rate',"$rate",'-PauseStart','2','-PauseDuration','1','-RepeatWeather','1')
}
Case 'rk_release_sample_cost' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-AgcTiming','-Seconds','8','-Rate','20')
Case 'rk_release_full_cost' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-AutoReference','-Seconds','8','-Rate','20')
Case 'rk_release_pan' @('-Normal','-CameraInput','tools/p6b_inputs/pair_pan.json','-Seconds','24','-Rate','30','-CaptureSeconds','24','-RawDump')
Case 'rk_release_plate' @('-Normal','-CameraInput',$camera,'-OrdinaryPlate','-Seconds','6','-Rate','30','-RawDump')
Case 'rk_release_normal_asset' @('-Normal','-CameraInput','tools/p6b_inputs/pair_game_asset.json','-Seconds','60','-Rate','60','-RawDump','-UiResponsive')

Case 'rk_release_auto60_runtime' @('-Normal','-CameraInput',$camera,'-Preset','Auto','-Seconds','60','-Rate','60')
