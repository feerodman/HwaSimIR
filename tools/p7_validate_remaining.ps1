$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$ffmpeg=(Resolve-Path '.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe').Path
$names=@('D_final_async20_30_explicit','D_stop_isolation_quiet_tail','D_rain_geometry_before','D_rain_geometry_after',
    'D_rain_default_frozen_on','D_rain_default_frozen_off','D_packaged_pair20',
    'C3_cloud_before_band2','C3_cloud_after_band2','D_cost_new_auto_record','D_cost_new_frozen_record','D_cost_new_game_record',
    'D_illum_nvg_wide','D_illum_nvg_half','D_illum_support_off','D_rain_altitude_on','D_rain_altitude_off',
    'D_vfx_side_legacy')
foreach($kind in @('rain','snow')){foreach($variant in @('both','hide_first','hide_second','off')){$names+="D_pixels_${kind}_$variant"}}
foreach($view in @('end','side','oblique','near','far','occluded','orbit')){$names+="D_vfx_$view"}
$paths=@($names|ForEach-Object {"logs/p7/$_"})
& F:\Programs\anaconda3\python.exe tools/p7_validate_cases.py @paths --ffmpeg $ffmpeg *> logs/p7/validation_D_remaining.log
if($LASTEXITCODE -ne 0){throw 'Some full MP4 / annotation / index validation failed; see validation_D_remaining.log.'}
