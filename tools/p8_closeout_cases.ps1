param([ValidateSet('cost','regression','visual','pixels','zero')][string]$Phase='regression',[int]$StartAt=0)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$selected=Get-Content (Join-Path $root 'releases/windows/current.json') -Raw | ConvertFrom-Json
$shared=@{LogGroup='p8';InputAudit=$true;NoDuplicateH264=$true;Normal=$true;ProductionDefaults=$true;CaptureSeconds=0;
    SenderExe=(Join-Path $root "releases/windows/$($selected.version)/sender/DataDrivenTestQT.exe");
    ReceiverExe=(Join-Path $root "releases/windows/$($selected.version)/receiver/HwaSim_IR_VideoDisplay.exe")}
$cases=@()
if($Phase -eq 'cost'){
    $cases=@(@{Name='P8_A8_rain128_recordoff';Band=1;Weather=2;Rate=60;Seconds=65;SaveMp4=0;CameraInput='tools/p7_inputs/pair_rain_asset.json'})
}elseif($Phase -eq 'regression'){
    $cases=@(
        @{Name='P8_final_sync20';Band=1;Weather=2;Rate=20;Seconds=9;CameraInput='tools/p7_inputs/pair_rain_asset.json'},
        @{Name='P8_final_sync30';Band=2;Weather=3;Rate=30;Seconds=9;CameraInput='tools/p7_inputs/pair_snow_asset.json'},
        @{Name='P8_final_pause_twoinit';Band=1;Weather=2;Rate=30;Seconds=10;PauseStart=2;PauseDuration=1;RepeatWeather=3;CameraInput='tools/p7_inputs/pair_rain_asset.json'},
        @{Name='P8_final_async60';Band=2;Weather=3;Rate=20;Seconds=9;SimMode=2;OutputFps=60;CameraInput='tools/p7_inputs/pair_snow_asset.json'},
        @{Name='P8_final_async_unlimited';Band=2;Weather=3;Rate=20;Seconds=9;SimMode=2;OutputFps=0;CameraInput='tools/p7_inputs/pair_snow_asset.json'},
        @{Name='P8_ordinary1_rk';Band=2;Weather=1;Rate=20;Seconds=9;CameraInput='tools/p8_inputs/ordinary1.json';DiagnosticJson='tools/p8_inputs/ordinary1_diagnostic.json'},
        @{Name='P8_ordinary2_rk';Band=2;Weather=1;Rate=20;Seconds=9;CameraInput='tools/p8_inputs/ordinary2.json';DiagnosticJson='tools/p8_inputs/ordinary2_diagnostic.json'},
        @{Name='P8_ordinary5_rk';Band=2;Weather=1;Rate=20;Seconds=9;CameraInput='tools/p8_inputs/ordinary5.json';DiagnosticJson='tools/p8_inputs/ordinary5_diagnostic.json'},
        @{Name='P8_ordinary2_invalid_rk';Band=2;Weather=1;Rate=20;Seconds=9;CameraInput='tools/p8_inputs/ordinary2_invalid.json';DiagnosticJson='tools/p8_inputs/ordinary2_diagnostic.json'}
    )
}elseif($Phase -eq 'zero'){
    foreach($band in @(1,2)){
        $weather=2;$label='rain';$camera='pair_rain_asset.json'
        if($band -eq 2){$weather=3;$label='snow';$camera='pair_snow_asset.json'}
        $cases+=@{Name="P8_visual_${label}_0";Band=$band;Weather=$weather;Rate=60;Seconds=9;
            CameraInput="tools/p7_inputs/$camera";DiagnosticJson="tools/p8_inputs/visual_${label}_0.json"}
    }
}elseif($Phase -eq 'pixels'){
    foreach($band in @(1,2)){
        $weather=2;$label='rain';$camera='pair_rain_asset.json'
        if($band -eq 2){$weather=3;$label='snow';$camera='pair_snow_asset.json'}
        foreach($count in @('192','128','0')){
            $cases+=@{Name="P8_pixels_${label}_$count";Band=$band;Weather=$weather;Rate=60;Seconds=6;RawDump=$true;DumpSeq=240;
                CameraInput="tools/p7_inputs/$camera";DiagnosticJson="tools/p8_inputs/visual_${label}_$count.json"}
        }
    }
}else{
    foreach($band in @(1,2)){
        $weather=2;$label='rain';$camera='pair_rain_asset.json'
        if($band -eq 2){$weather=3;$label='snow';$camera='pair_snow_asset.json'}
        foreach($kind in @('192','128','hide_first','hide_second')){
            $cases+=@{Name="P8_visual_${label}_$kind";Band=$band;Weather=$weather;Rate=60;Seconds=9;
                CameraInput="tools/p7_inputs/$camera";DiagnosticJson="tools/p8_inputs/visual_${label}_$kind.json"}
        }
    }
}
$caseIndex=-1
foreach($c in $cases){
    ++$caseIndex;if($caseIndex -lt $StartAt){continue}
    $name=$c.Name
    if(Test-Path (Join-Path $root "logs/p8/$name/request.json")){throw "Evidence already exists: $name"}
    $c.CameraInput=Join-Path $root $c.CameraInput
    if($c.DiagnosticJson){$c.DiagnosticJson=Join-Path $root $c.DiagnosticJson}
    Write-Output "BEGIN $name"
    & (Join-Path $PSScriptRoot 'p7_run_case.ps1') @shared @c *> (Join-Path $root "logs/p8/${name}_runner.log")
    $warm=0;$duration=$c.Seconds
    if($duration -ge 65){$warm=5;$duration-=5}
    & F:\Programs\anaconda3\python.exe (Join-Path $PSScriptRoot 'p8_case_metrics.py') (Join-Path $root "logs/p8/$name") --warmup $warm --seconds $duration *> (Join-Path $root "logs/p8/${name}_metrics_runner.log")
    if($LASTEXITCODE -ne 0){throw "Metrics failed: $name"}
    if($name.StartsWith('P8_ordinary')){
        & F:\Programs\anaconda3\python.exe (Join-Path $PSScriptRoot 'p8_validate_ordinary_annotations.py') (Join-Path $root "logs/p8/$name") $c.CameraInput *> (Join-Path $root "logs/p8/${name}_coordinates_runner.log")
        if($LASTEXITCODE -ne 0){throw "Coordinate validator failed: $name"}
    }
    Write-Output "END $name"
}
