$ErrorActionPreference='Stop'
$cases=@(
    @{Name='after_b2_Legacy';Preset='Legacy';Band=2},
    @{Name='after_b1_Game';Preset='Game';Band=1},
    @{Name='after_b1_Auto';Preset='Auto';Band=1},
    @{Name='after_b2_Auto';Preset='Auto';Band=2},
    @{Name='hdr_Game';Preset='Game';DisplayCase=1},
    @{Name='hdr_Auto';Preset='Auto';DisplayCase=1},
    @{Name='flat_Auto';Preset='Auto';DisplayCase=2},
    @{Name='nearflat_Auto';Preset='Auto';DisplayCase=3},
    @{Name='small_Auto';Preset='Auto';DisplayCase=4;Seconds=12;CaptureSeconds=12;DumpSeq=100},
    @{Name='small_frozen';Preset='Auto';DisplayCase=4;Seconds=12;CaptureSeconds=12;DumpSeq=100;FreezeAfter=40},
    @{Name='large_Auto';Preset='Auto';DisplayCase=5;Seconds=12;CaptureSeconds=12;DumpSeq=100},
    @{Name='large_frozen';Preset='Auto';DisplayCase=5;Seconds=12;CaptureSeconds=12;DumpSeq=100;FreezeAfter=40},
    @{Name='large_Game';Preset='Game';DisplayCase=5;Seconds=12;CaptureSeconds=12;DumpSeq=100},
    @{Name='alias_Auto';Preset='Auto';DisplayCase=6},
    @{Name='auto_init';Preset='Auto';RepeatWeather=0;PauseStart=2;PauseDuration=1}
)
foreach($case in $cases){
    $params=@{Scene='display';Band=2;Rate=20;Seconds=5;Weather=0;RawDump=$true;AgcTiming=$true}
    foreach($key in $case.Keys){$params[$key]=$case[$key]}
    & "$PSScriptRoot/p6c_run_case.ps1" @params *> "logs/p6c/$($case.Name).run.log"
    if(-not $?){throw "Display case failed: $($case.Name)"}
}
foreach($band in @(1,2)){
    & "$PSScriptRoot/p6c_run_case.ps1" -Name "candidate_normal_b$band" -Normal -UseRuntimeWeather -CameraInput tools/p6b_inputs/pair_mid.json -Preset Game -Band $band -Rate 60 -Seconds 60 *> "logs/p6c/candidate_normal_b$band.run.log"
    if(-not $?){throw "Normal candidate failed for band $band"}
}
