param(
    [ValidateSet('display','cloud','vfx','regression','long','existing','nozzles','release','spatial','smoke2')][string]$Phase,
    [switch]$Windows
)
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p6_run_case.ps1'
$prefix=if($Windows){'win'}else{'rk'}
function Run-Case([hashtable]$Case) {
    $Case.Name="$prefix`_$($Case.Name)"
    if($Windows){$Case.Windows=$true}
    # Each case gets a fresh environment; optional overrides cannot leak to the
    # next Windows process (for example Gain, VolumeOff, or float-dump paths).
    $caseArgs=@()
    foreach($key in $Case.Keys){
        if($Case[$key] -is [bool]){if($Case[$key]){$caseArgs+="-$key"}}
        else{$caseArgs+=@("-$key",[string]$Case[$key])}
    }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner @caseArgs
    if($LASTEXITCODE -ne 0){throw "Case failed: $($Case.Name)"}
}
if($Phase -eq 'display') {
    foreach($preset in @('Legacy','Game','Auto','Black')) {
        Run-Case @{Name="display_$preset";Scene='display';Preset=$preset;Rate=20;Seconds=6;Clouds=0;RawDump=$true}
    }
    Run-Case @{Name='display_gain05';Scene='display';Preset='Legacy';Gain=0.5;Rate=20;Seconds=6;Clouds=0;RawDump=$true}
    Run-Case @{Name='display_NIR';Scene='display';Preset='Game';Band=1;Rate=20;Seconds=6;Clouds=0;RawDump=$true}
}
if($Phase -eq 'cloud') {
    foreach($count in @(1,2,4)) {Run-Case @{Name="cloud$count";Clouds=$count;View='separated';Rate=60;Seconds=10;RawDump=$true}}
    foreach($mask in @(0,1,2)) {
        $name=if($mask -eq 0){'two_pixels'}else{"two_off$mask"}
        Run-Case @{Name=$name;Clouds=2;View='separated';DisableCloudMask=$mask;Vfx='off';Rate=30;Seconds=6;RawDump=$true}
    }
    foreach($view in @('overlap','translate','entry','occluded','behind')) {
        Run-Case @{Name="cloud_$view";Clouds=2;View=$view;Rate=30;Seconds=8}
    }
    Run-Case @{Name='cloud_large';Clouds=2;View='separated';LargeClouds=$true;Rate=60;Seconds=10}
    Run-Case @{Name='cloud4_large';Clouds=4;View='separated';LargeClouds=$true;Rate=60;Seconds=10}
    Run-Case @{Name='cloud_sheets';Clouds=2;View='separated';VolumeOff=$true;Rate=30;Seconds=6;RawDump=$true}
    Run-Case @{Name='cloud_volumes';Clouds=2;View='separated';SheetOff=$true;Rate=30;Seconds=6;RawDump=$true}
}
if($Phase -eq 'vfx') {
    foreach($era in @('before','after')) {foreach($view in @('end','side','oblique','orbit')) {
        Run-Case @{Name="plume_$($era)_$view";Scene='plume';View=$view;LegacyArt=($era -eq 'before');Rate=30;Seconds=8}
    }}
    foreach($view in @('near','far','occluded')) {Run-Case @{Name="plume_after_$view";Scene='plume';View=$view;Rate=30;Seconds=8}}
    foreach($vfx in @('glow','smoke','off')) {Run-Case @{Name="plume_after_$vfx";Scene='plume';View='side';Vfx=$vfx;Rate=30;Seconds=8}}
}
if($Phase -eq 'regression') {
    foreach($rate in @(20,30,60)){Run-Case @{Name="sync$rate";View='separated';Clouds=2;Rate=$rate;Seconds=8}}
    Run-Case @{Name='pause';View='separated';Clouds=2;Rate=60;Seconds=10;PauseStart=3;PauseDuration=2}
    Run-Case @{Name='reinit';View='separated';Clouds=2;Rate=30;Seconds=6;RepeatWeather=0}
}
if($Phase -eq 'long') {
    foreach($duration in @(60,600)){Run-Case @{Name="mixed_$($duration)s";View='separated';Clouds=2;Rate=60;Seconds=$duration}}
}
if($Phase -eq 'existing') {
    foreach($rate in @(20,30,60)){Run-Case @{Name="existing_targets_$rate";View='separated';Clouds=2;Rate=$rate;Seconds=15;ExistingTargets=$true;RawDump=$true}}
}
if($Phase -eq 'nozzles') {
    foreach($asset in @(@('f35','0x11'),@('f22','0x12'),@('aim120','0x22'),@('aim9x','0x33'))) {
        foreach($view in @('end','side','oblique')) {
            Run-Case @{Name="nozzle_$($asset[0])_$view";View=$view;Clouds=2;Rate=30;Seconds=6;ExistingTargets=$true;TargetType=$asset[1];UiResponsive=($asset[0] -eq 'f35' -and $view -eq 'oblique')}
        }
    }
    Run-Case @{Name='nozzle_f35_before';View='oblique';Clouds=2;Rate=30;Seconds=6;ExistingTargets=$true;LegacyNozzleAttachment=$true}
}
if($Phase -eq 'release') {
    foreach($count in @(1,2,4)){Run-Case @{Name="release_cloud$count";View='separated';Clouds=$count;Rate=60;Seconds=10}}
    Run-Case @{Name='release_sheet';View='separated';Clouds=2;Rate=30;Seconds=6;VolumeOff=$true;RawDump=$true}
    Run-Case @{Name='release_no_sheet';View='separated';Clouds=2;Rate=30;Seconds=6;SheetOff=$true;RawDump=$true}
    Run-Case @{Name='release_mixed_raw';View='separated';Clouds=2;Rate=30;Seconds=6;RawDump=$true}
    Run-Case @{Name='normal_start';Rate=30;Seconds=6;Normal=$true}
    Run-Case @{Name='display_Auto_drain';Scene='display';Preset='Auto';Rate=20;Seconds=6;Clouds=0;RawDump=$true}
}
if($Phase -eq 'spatial') {
    foreach($mask in @(0,1,2)){
        $name=if($mask -eq 0){'release_two_pixels'}else{"release_two_off$mask"}
        Run-Case @{Name=$name;Clouds=2;View='separated';DisableCloudMask=$mask;Vfx='off';Rate=30;Seconds=6;RawDump=$true}
    }
    foreach($view in @('entry','translate')){
        Run-Case @{Name="release_$view";Clouds=2;View=$view;Rate=30;Seconds=8}
    }
    Run-Case @{Name='release_legacy_art';Clouds=2;View='separated';LegacyArt=$true;Rate=60;Seconds=10}
}
if($Phase -eq 'smoke2') {
    foreach($view in @('end','side','oblique','orbit','near','far','occluded')){
        Run-Case @{Name="plume_smoke2_$view";Scene='plume';View=$view;Rate=30;Seconds=8}
    }
    foreach($vfx in @('smoke','off')){
        Run-Case @{Name="plume_smoke2_$vfx";Scene='plume';View='side';Vfx=$vfx;Rate=30;Seconds=6;RawDump=$true}
    }
    foreach($count in @(1,2,4)){
        Run-Case @{Name="release2_cloud$count";Clouds=$count;View='separated';Rate=60;Seconds=10}
    }
    foreach($duration in @(60,600)){
        Run-Case @{Name="mixed_smoke2_$($duration)s";View='separated';Clouds=2;Rate=60;Seconds=$duration}
    }
}
