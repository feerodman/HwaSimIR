[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
foreach($band in @('NIR','SWIR','MWIR')){
    foreach($weather in @('Clear','Rain','Snow')){
        $name="p17_ground_aim120_$($band.ToLowerInvariant())_$($weather.ToLowerInvariant())"
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'p17_dds_case.ps1') -Band $band -Weather $weather -EngineState 0 -TargetTypeCode 0x22 -InputMode P14GroundWeather -DurationGuardSec 45 -Name $name -OutputRoot logs/p17/diagnostics/weather
        if($LASTEXITCODE -ne 0){throw "Acquisition failed: $name"}
    }
}
