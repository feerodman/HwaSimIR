$ErrorActionPreference='Stop'
# Ordinary renderer startup: no test scene, diagnostic or display override.
# The sender supplies a valid 800x800 camera viewing the existing public world.
foreach($band in @(1,2)) {
    foreach($rate in @(20,30,60)) {
        $name="release_b${band}_${rate}_pause_init"
        & "$PSScriptRoot/p6c_run_case.ps1" -Name $name -Normal -ProductionDefaults -UseRuntimeWeather -Preset Runtime -CameraInput tools/p6b_inputs/pair_mid.json -Band $band -Rate $rate -Seconds 6 -PauseStart 2 -PauseDuration 1 -RepeatWeather 1 *> "logs/p6c/$name.run.log"
        if(-not $?){throw "Release regression failed: $name"}
    }
    foreach($seconds in @(60,600)) {
        $name="release_b${band}_60hz_${seconds}s"
        & "$PSScriptRoot/p6c_run_case.ps1" -Name $name -Normal -ProductionDefaults -UseRuntimeWeather -Preset Runtime -CameraInput tools/p6b_inputs/pair_mid.json -Band $band -Rate 60 -Seconds $seconds *> "logs/p6c/$name.run.log"
        if(-not $?){throw "Release regression failed: $name"}
    }
}
