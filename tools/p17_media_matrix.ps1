[CmdletBinding()]
param([string[]]$Bands=@('NIR','SWIR','MWIR'))
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$rows=@()
foreach($band in $Bands){
    foreach($spec in @(@('Clear',0,'clear_off'),@('Clear',1,'clear_on'),@('Cloudy',0,'cloudy'),@('Overcast',0,'overcast'),@('Rain',0,'rain'),@('Snow',0,'snow'),@('Rain',1,'rain_plume'))){
        $name="p17_final_$($band.ToLowerInvariant())_$($spec[2])"
        $dest=Join-Path $repo "logs/p17/production/after/$name"
        if(Test-Path (Join-Path $dest 'case_result.json')){continue}
        if(Test-Path $dest){throw "Existing incomplete evidence: $dest. Inspect before choosing another name."}
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'p17_dds_case.ps1') -Band $band -Weather $spec[0] -EngineState $spec[1] -Name $name
        $rows+=@{caseId=$name;exitCode=$LASTEXITCODE;time=[DateTime]::UtcNow.ToString('o')}
        $rows|ConvertTo-Json|Set-Content -Encoding UTF8 (Join-Path $repo 'logs/p17/matrix_progress.json')
        if($LASTEXITCODE -ne 0){Write-Warning "$name did not complete; recorded failure, continuing other bands/cases"}
    }
}
