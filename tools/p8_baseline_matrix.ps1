$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized board.'}
$env:SSH_ASKPASS=(Resolve-Path tools/p5_ssh_askpass.cmd).Path;$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p8'
& scp.exe -o StrictHostKeyChecking=yes tools/p8_inputs/supported_high.conf root@192.168.1.116:/userdata/HwaSimIR/logs/p8_supported_high.conf
if($LASTEXITCODE -ne 0){throw 'Candidate policy upload failed'}
foreach($item in @(@('current_no_precip',1,2,'precip_zero',$false),@('high_no_precip',1,2,'precip_zero',$true),@('current_rain192',1,2,'precip_192',$false),@('high_rain192',1,2,'precip_192',$true),@('current_snow192',2,3,'precip_192',$false),@('high_snow192',2,3,'precip_192',$true))) {
    $name='P8_'+$item[0]
    $options=@{Name=$name;LogGroup='p8';NoDuplicateH264=$true;Normal=$true;ProductionDefaults=$true;Band=$item[1];Weather=$item[2];Rate=60;Seconds=65;CaptureSeconds=0;CameraInput=$(if($item[2]-eq 3){'tools/p7_inputs/pair_snow_asset.json'}else{'tools/p7_inputs/pair_rain_asset.json'});DiagnosticJson="tools/p8_inputs/$($item[3]).json";SenderExe='releases/windows/P7_C4R2/sender/DataDrivenTestQT.exe';ReceiverExe='releases/windows/P7_C4R2/receiver/HwaSim_IR_VideoDisplay.exe'}
    if($item[4]){$options.PerformancePolicy='/userdata/HwaSimIR/logs/p8_supported_high.conf'}
    & tools/p7_run_case.ps1 @options *> "logs/p8/${name}_runner.log"
}
