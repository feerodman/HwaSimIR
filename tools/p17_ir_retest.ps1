[CmdletBinding()]
param([string]$Suffix='ir_v7', [ValidateSet('Weather','Plume','Detail','All')][string]$Suite='All',
      [ValidateSet('SWIR','MWIR')][string[]]$Bands=@('SWIR','MWIR'))
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p17_dds_case.ps1'
foreach($band in $Bands) {
    if($Suite -in @('All','Weather')) {
        foreach($weather in @('Clear','Rain','Snow')) {
            & $runner -Band $band -Weather $weather -EngineState 0 -InputMode P14GroundWeather `
                -DurationGuardSec 45 -LinearDiagnosticSeqs '900,1200' `
                -Name "$($band.ToLower())_$($weather.ToLower())_$Suffix" -OutputRoot logs/p17/ir_retest/after
        }
    }
    if($Suite -in @('All','Plume')) {
        foreach($engine in @(0,1)) {
            & $runner -Band $band -Weather Clear -EngineState $engine -InputMode Original `
                -LinearDiagnosticSeqs '900,1200,3600,4200' `
                -Name "$($band.ToLower())_plume${engine}_$Suffix" -OutputRoot logs/p17/ir_retest/after
        }
    }
    if($Suite -eq 'Detail') {
        foreach($engine in @(0,1)) {
            & $runner -Band $band -Weather Clear -EngineState $engine -InputMode Original `
                -PixelAngleUrad 2 -LinearDiagnosticSeqs '900,1200,3600,4200' `
                -Name "$($band.ToLower())_plume${engine}_detail_$Suffix" -OutputRoot logs/p17/ir_retest/after
        }
    }
}
