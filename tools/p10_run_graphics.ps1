param([ValidateSet('off','msaa2','msaa4','edge')][string]$Mode='off',[int]$Seconds=8,[string]$Name='')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$Name){$Name="P10_graphics_$Mode"}
$inputs=Join-Path $root 'logs/p10/inputs';New-Item -ItemType Directory -Force $inputs | Out-Null
$samples=if($Mode -eq 'msaa2'){2}elseif($Mode -eq 'msaa4'){4}else{0}
$filter=if($Mode -eq 'edge'){'EdgeAA'}else{'Off'}
@{P8OrdinaryAnnotations='5';P10Graphics='1';P10FramebufferAudit='1';RenderMSAASamples="$samples";RenderPostprocessAA=$filter;SensorDisplayPreset='Game';P6DFrozenGain='1';P6DFrozenOffset='0'} | ConvertTo-Json | Set-Content -Encoding UTF8 "$inputs/$Mode.json"
@{trackerSensorWidth=800;trackerSensorHeight=800;noiseEn=0;trackerSensorNoise=0;realtimeAnnotation=1} | ConvertTo-Json | Set-Content -Encoding UTF8 "$inputs/sensor.json"
if(!$env:HWASIMIR_SSH_PASSWORD){throw 'Set HWASIMIR_SSH_PASSWORD for the authorized board.'}
& "$root/tools/p7_run_case.ps1" -LogGroup p10 -Name $Name -Normal -ProductionDefaults -InputAudit -NoDuplicateH264 -Band 2 -Rate 60 -Seconds $Seconds -CaptureSeconds 0 -SaveMp4 1 -Weather 1 -CameraInput "$root/tools/p8_inputs/ordinary5.json" -DiagnosticJson "$inputs/$Mode.json" -SensorFieldsJson "$inputs/sensor.json" -RawDump -DumpSeq 180 -SenderExe "$root/releases/windows/P9_A4/sender/DataDrivenTestQT.exe" -ReceiverExe "$root/releases/windows/P9_A4/receiver/HwaSim_IR_VideoDisplay.exe"
