[CmdletBinding()]
param([ValidateSet('Weather','Cloud','Depth')][string]$Suite='Weather',
      [string]$OutputRoot='logs/p17/review/after', [string]$Suffix='review')
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'p17_dds_case.ps1'
# Every case retains the entire input and production recording. Ground cases
# use the independently validated P14 fixture, never a replacement for 1.txt.
if($Suite -eq 'Weather') {
    foreach($band in @('NIR','SWIR')) {
        foreach($weather in @('Clear',$(if($band -eq 'NIR'){'Rain'}else{'Snow'}))) {
            $capture=@{}
            if($band -eq 'SWIR'){$capture.LinearDiagnosticSeqs='900,1200'}
            & $runner -Band $band -Weather $weather -EngineState 0 -InputMode P14GroundWeather `
                -DurationGuardSec 45 -Name "$($band.ToLower())_$($weather.ToLower())_$Suffix" `
                -OutputRoot $OutputRoot @capture
        }
    }
} elseif($Suite -eq 'Cloud') {
    foreach($weather in @('Clear','Cloudy','Overcast')) {
        & $runner -Band NIR -Weather $weather -TargetTypeCode 0x12 -EngineState 0 `
            -Name "nir_f22_$($weather.ToLower())_$Suffix" -OutputRoot $OutputRoot
    }
} else {
    $seqs=((3600..3615)+(4200..4203)) -join ','
    foreach($model in @(@{name='aim120';code='0x22';pixel=.5},@{name='f22';code='0x12';pixel=2.0})) {
        foreach($linear in @($true,$false)) {
            $mode=if($linear){'linear'}else{'log'}
            & $runner -Band MWIR -Weather Clear -TargetTypeCode $model.code -PixelAngleUrad $model.pixel `
                -Name "$($model.name)_${mode}_$Suffix" -OutputRoot $OutputRoot `
                -LinearDiagnosticSeqs $seqs -CaptureGeometry -LinearDepthReference:$linear
        }
    }
}
