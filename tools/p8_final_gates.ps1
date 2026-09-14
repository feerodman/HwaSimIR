param([ValidateRange(0,2)][int]$StartAt=0)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$selected=Get-Content (Join-Path $root 'releases/windows/current.json') -Raw | ConvertFrom-Json
$sender=Join-Path $root "releases/windows/$($selected.version)/sender/DataDrivenTestQT.exe"
$receiver=Join-Path $root "releases/windows/$($selected.version)/receiver/HwaSim_IR_VideoDisplay.exe"
$caseIndex=-1
foreach($c in @(@('cloudy60',1,1,'pair_asset_weather.json'),@('NVG_rain60',1,2,'pair_rain_asset.json'),@('MWIR_snow60',2,3,'pair_snow_asset.json'))){
    ++$caseIndex;if($caseIndex -lt $StartAt){continue}
    $name='P8_gate_'+$c[0]
    & (Join-Path $PSScriptRoot 'p7_run_case.ps1') -Name $name -LogGroup p8 -InputAudit -NoDuplicateH264 -Normal -ProductionDefaults -Band $c[1] -Weather $c[2] -Rate 60 -Seconds 65 -CaptureSeconds 0 -CameraInput (Join-Path $root ('tools/p7_inputs/'+$c[3])) -SenderExe $sender -ReceiverExe $receiver *> (Join-Path $root "logs/p8/${name}_runner.log")
    & F:\Programs\anaconda3\python.exe (Join-Path $PSScriptRoot 'p8_case_metrics.py') (Join-Path $root "logs/p8/$name") *> (Join-Path $root "logs/p8/${name}_metrics_runner.log")
    $result=Get-Content (Join-Path $root "logs/p8/$name/p8_metrics.json") -Raw | ConvertFrom-Json
    Write-Output "$name gate=$($result.realtime60Hz) receivedFPS=$($result.rates.newImage.fps)"
    if($result.realtime60Hz -ne 'PASS'){throw "60Hz gate failed: $name"}
}
