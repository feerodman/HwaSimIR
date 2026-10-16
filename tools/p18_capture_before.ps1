param([string]$Suffix='')
$ErrorActionPreference='Stop'
foreach($band in @('SWIR','MWIR')) {
    foreach($angle in @(25,10)) {
        & "$PSScriptRoot/p17_dds_case.ps1" -Band $band -Weather Clear -EngineState 1 `
            -PixelAngleUrad $angle -InputMode Original -LinearDiagnosticSeqs '900,1200,3600,4200' `
            -Name "p18_before_$($band.ToLower())_${angle}urad$Suffix" -OutputRoot logs/p18/before
    }
}
