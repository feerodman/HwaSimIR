param([switch]$AssetPixels)
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p5_run_case.ps1'
function Run-P5([hashtable]$Case){
    & $runner @Case
}
# Cases run serially: the board/Windows DDS topics identify one producer/receiver.
if($AssetPixels){
    Run-P5 @{Name='rk_cloud_sheet_only';Scene='cloud';View='above';Seconds=4;Rate=20;Gamma=2;NoTargets=$true;SmallClouds=$true;VolumeOff=$true}
    Run-P5 @{Name='rk_cloud_volume_only';Scene='cloud';View='side';Seconds=4;Rate=20;Gamma=2;NoTargets=$true;SmallClouds=$true;SheetOff=$true}
    foreach($asset in @('f22','aim120','aim9x')){
        foreach($lookup in @('A','B')){
            Run-P5 @{Name="rk_${asset}_lookup$lookup";Scene=$asset;AssetBandCase=$lookup;MaterialView=4;Band=1;Rate=20;Seconds=4;RawDump=$true}
        }
    }
    return
}
Run-P5 @{Name='rk_volumeoff';Scene='mixed';View='side';Seconds=10;Rate=60;SmallClouds=$true;VolumeOff=$true}
Run-P5 @{Name='rk_legacy';Scene='mixed';View='side';Seconds=10;Rate=60;SmallClouds=$true;Legacy=$true}
Run-P5 @{Name='rk_cloud_large';Scene='cloud';View='side';Seconds=6;Rate=20;Gamma=2;NoTargets=$true}
Run-P5 @{Name='rk_cloud_small';Scene='cloud';View='side';Seconds=6;Rate=20;Gamma=2;NoTargets=$true;SmallClouds=$true}
foreach($view in @('occluded','behind')){
    Run-P5 @{Name="rk_cloud_$view";Scene='cloud';View=$view;Seconds=4;Rate=20;Gamma=2;NoTargets=$true;SmallClouds=$true}
}
Run-P5 @{Name='rk_plume_end';Scene='plume';View='end';Seconds=4;Rate=20;VolumeOff=$true;SheetOff=$true}
Run-P5 @{Name='rk_pause';Scene='mixed';View='side';Seconds=12;Rate=30;SmallClouds=$true;PauseStart=3;PauseDuration=3}
Run-P5 @{Name='rk_weather_reinit';Scene='cloud';View='side';Seconds=4;Rate=20;SmallClouds=$true;NoTargets=$true;Weather=1;RepeatWeather=0}
Run-P5 @{Name='rk_plume_off';Scene='plume';View='off';Seconds=4;Rate=20;VolumeOff=$true;SheetOff=$true}
