$ErrorActionPreference='Stop'
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized test board.'}
$root=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
foreach($weatherCase in @(@('rain',1,2),@('snow',2,3))){
    $kind=$weatherCase[0];$band=[int]$weatherCase[1];$weather=[int]$weatherCase[2]
    foreach($variant in @('both','hide_first','hide_second','off')){
        $diagnostic=if($variant -eq 'both'){'frozen_common'}elseif($variant -eq 'off'){'frozen_precip_off'}else{"${kind}_$variant"}
        $name="D_pixels_${kind}_$variant"
        & tools/p7_run_case.ps1 -Name $name -Normal -ProductionDefaults -CameraInput "tools/p7_inputs/pair_${kind}_asset.json" -Band $band -Rate 30 -Seconds 6 -CaptureSeconds 6 -Weather $weather -DiagnosticJson "tools/p7_inputs/$diagnostic.json" -RawDump -DumpSeq 90 *> "logs/p7/${name}_runner.log"
    }
}
foreach($variant in @('on','off')){
    $diagnostic=if($variant -eq 'on'){'frozen_common'}else{'frozen_precip_off'}
    $name="D_rain_altitude_$variant"
    & tools/p7_run_case.ps1 -Name $name -Normal -ProductionDefaults -CameraInput tools/p7_inputs/rain_altitude_crossing.json -Band 1 -Rate 30 -Seconds 10 -CaptureSeconds 10 -Weather 2 -DiagnosticJson "tools/p7_inputs/$diagnostic.json" -RawDump -DumpSeq 120 *> "logs/p7/${name}_runner.log"
}
& tools/p7_run_case.ps1 -Name D_illum_nvg_wide -Normal -ProductionDefaults -CameraInput tools/p7_inputs/ordinary_illumination.json -Band 1 -Rate 30 -Seconds 10 -CaptureSeconds 10 -Weather 0 -SensorFieldsJson tools/p7_inputs/illumination_wide_sensor.json -DiagnosticJson tools/p7_inputs/ordinary_illumination_frozen.json *> logs/p7/D_illum_nvg_wide_runner.log
& tools/p7_run_case.ps1 -Name D_illum_nvg_half -Normal -ProductionDefaults -CameraInput tools/p7_inputs/ordinary_illumination.json -Band 1 -Rate 30 -Seconds 10 -CaptureSeconds 10 -Weather 0 -SensorFieldsJson tools/p7_inputs/illumination_half_sensor.json -DiagnosticJson tools/p7_inputs/ordinary_illumination_frozen.json *> logs/p7/D_illum_nvg_half_runner.log
& tools/p7_run_case.ps1 -Name D_illum_support_off -Normal -ProductionDefaults -CameraInput tools/p7_inputs/ordinary_illumination.json -Band 1 -Rate 30 -Seconds 10 -CaptureSeconds 10 -Weather 0 -SensorFieldsJson tools/p7_inputs/ordinary_illumination_sensor.json -DiagnosticJson tools/p7_inputs/ordinary_illumination_disabled.json *> logs/p7/D_illum_support_off_runner.log
foreach($view in @('end','side','oblique','near','far','occluded','orbit')){
    $name="D_vfx_$view"
    & tools/p7_run_case.ps1 -Name $name -Scene plume -View $view -Band 2 -Rate 30 -Seconds 6 -CaptureSeconds 6 -Weather 1 -Clouds 0 -Preset Game *> "logs/p7/${name}_runner.log"
}
& tools/p7_run_case.ps1 -Name D_vfx_side_legacy -Scene plume -View side -Band 2 -Rate 30 -Seconds 6 -CaptureSeconds 6 -Weather 1 -Clouds 0 -Preset Game -DiagnosticJson tools/p7_inputs/art_legacy.json *> logs/p7/D_vfx_side_legacy_runner.log
& tools/p7_run_case.ps1 -Name D_final_async20_30_explicit -Normal -ProductionDefaults -CameraInput tools/p7_inputs/pair_snow_asset.json -Band 2 -Rate 20 -Seconds 10 -CaptureSeconds 8 -Weather 3 -SimMode 2 -OutputFps 30 *> logs/p7/D_final_async20_30_explicit_runner.log
