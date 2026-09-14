param(
    [string]$Name='smoke', [double]$ReceiverJoinDelay=0, [double]$ReceiverRestartAt=0, [double]$ReceiverRestartGap=2, [string]$ReceiverFaultConfig='', [int]$SaveMp4=1, [ValidateSet(1,2)][int]$SimMode=1, [int]$OutputFps=60, [string]$SensorFieldsJson='', [string]$DiagnosticJson='', [int]$DisplayCase=0, [int]$FreezeAfter=0, [int]$DumpSeq=0, [switch]$ProductionDefaults, [string]$Scene='mixed', [string]$View='oblique',
    [int]$MaterialView=0, [string]$MaterialCase='A', [int]$Band=2,
    [int]$Rate=60, [int]$Seconds=10, [int]$CaptureSeconds=10, [int]$Weather=1, [switch]$Legacy,
    [switch]$Windows, [switch]$SmallClouds, [double]$PauseStart=-1,
    [double]$PauseDuration=0, [switch]$VolumeOff, [switch]$SheetOff,
    [switch]$Blend, [Nullable[double]]$Gamma=$null, [switch]$RawDump, [string]$AssetBandCase='', [switch]$NoTargets,
    [int]$RepeatWeather=-1, [string]$Preset='Game', [int]$Clouds=2, [int]$DisableCloudMask=0,
    [switch]$LegacyArt, [switch]$LargeClouds, [switch]$NativeFov, [string]$Vfx='both', [Nullable[double]]$Gain=$null, [switch]$ExistingTargets, [switch]$Normal,
    [string]$Appearance='GameWorld', [string]$CameraInput='', [int]$SampleSize=64, [double]$PixelAngle=200, [string]$HideCloudId='', [int]$ReferenceSteps=0, [switch]$AutoReference, [string]$TargetType='0x11', [switch]$LegacyNozzleAttachment, [switch]$UiResponsive,
    [int]$PlatId=1001, [int]$SensorId=2, [string]$RendererExe='', [string]$ReceiverExe='', [string]$SenderExe='', [switch]$SkipRemoteCleanup, [switch]$UseRuntimeWeather, [switch]$UseRuntimeStreaming, [switch]$AgcTiming, [switch]$OrdinaryPlate, [Nullable[double]]$AutoTargetHigh=$null
)
$ErrorActionPreference='Stop'
if($Weather -lt 0 -or $Weather -gt 3 -or $RepeatWeather -lt -1 -or $RepeatWeather -gt 3){
    throw 'Weather must use the existing protocol enum: Clear=0, Cloudy=1, Rain=2, Snow=3; repeat=-1 disables the second run.'
}
$taskInheritedPath=[Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskInheritedPath,'Process')
$caseStartUtc=[DateTime]::UtcNow
$caseEnvironmentBefore=@{}
Get-ChildItem Env: | ForEach-Object {$caseEnvironmentBefore[$_.Name]=$_.Value}
if($Windows -and $PlatId -eq 1001){$PlatId=2001}
if($DumpSeq -eq 0){$DumpSeq=$Rate*3}
$root=Split-Path -Parent $PSScriptRoot
if(!$RendererExe){$RendererExe=Join-Path $root 'HwaSim_IR/Bin/HwaSim_IR.exe'}
$RendererExe=(Resolve-Path -LiteralPath $RendererExe).Path
if(!$ReceiverExe){$ReceiverExe=Join-Path $root 'HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe'}
$ReceiverExe=(Resolve-Path -LiteralPath $ReceiverExe).Path
if(!$SenderExe){$SenderExe=Join-Path $root 'build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe'}
$SenderExe=(Resolve-Path -LiteralPath $SenderExe).Path
if($SensorFieldsJson){$SensorFieldsJson=(Resolve-Path -LiteralPath $SensorFieldsJson).Path}
if($env:HWASIMIR_SSH_PASSWORD){
    $env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
    $env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p5'
}
$dir=Join-Path $root "logs\p7\$Name"
New-Item -ItemType Directory -Force $dir | Out-Null
$dir=(Resolve-Path $dir).Path
@{host=$(if($Windows){'Windows'}else{'RK3588'});platId=$PlatId;sensorId=$SensorId;displayCase=$DisplayCase;freezeAfter=$FreezeAfter;dumpSeq=$DumpSeq;productionDefaults=[bool]$ProductionDefaults;useRuntimeWeather=[bool]$UseRuntimeWeather;useRuntimeStreaming=[bool]$UseRuntimeStreaming;scene=$Scene;view=$View;rate=$Rate;seconds=$Seconds;captureSeconds=$CaptureSeconds;appearance=$Appearance;cameraInput=$CameraInput;pixelAngle=$PixelAngle;sampleSize=$SampleSize;referenceSteps=$ReferenceSteps;hideCloudId=$HideCloudId;autoReference=[bool]$AutoReference;ordinaryPlate=[bool]$OrdinaryPlate;agcTiming=[bool]$AgcTiming;autoTargetHigh=$AutoTargetHigh;
  band=$Band;weather=$Weather;gamma=$Gamma;gain=$Gain;simMode=$SimMode;outputFps=$OutputFps;saveMp4=$SaveMp4;utcHour=6.0;smallClouds=[bool]$SmallClouds;
  volumeOff=[bool]$VolumeOff;sheetOff=[bool]$SheetOff;legacy=[bool]$Legacy;materialView=$MaterialView;
  materialCase=$MaterialCase;assetBandCase=$AssetBandCase;pauseStart=$PauseStart;pauseDuration=$PauseDuration
  noTargets=(!$ExistingTargets -and !$Normal);normal=[bool]$Normal;targetType=$TargetType;legacyNozzleAttachment=[bool]$LegacyNozzleAttachment;repeatWeather=$RepeatWeather;preset=$Preset;clouds=$Clouds;disableMask=$DisableCloudMask;legacyArt=[bool]$LegacyArt;largeClouds=[bool]$LargeClouds;nativeFov=[bool]$NativeFov;vfx=$Vfx;
  profileSha256=(Get-FileHash (Join-Path "$root\HwaSim_IR\Bin\Config\SensorWave" $(if($Band -eq 1){'default_NVG.json'}else{'default_MWIR.json'}))).Hash;
  receiverSha256=(Get-FileHash -Algorithm SHA256 $ReceiverExe).Hash;senderSha256=(Get-FileHash -Algorithm SHA256 $SenderExe).Hash
} | ConvertTo-Json | Set-Content -Encoding UTF8 "$dir\request.json"
# Normalize the inherited PATH/Path duplicate seen in Windows PowerShell 5.
$taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
[Environment]::SetEnvironmentVariable('PATH',$null,'Process')
[Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')
function Remote([string]$Command) {
    & ssh.exe -o ConnectTimeout=10 -o StrictHostKeyChecking=yes root@192.168.1.116 $Command
    if($LASTEXITCODE -ne 0){throw "SSH command failed with exit $LASTEXITCODE"}
}
function Wait-RepeatDrain([int]$CompletedRuns) {
    for($attempt=0;$attempt -lt 60;$attempt++){
        $count=if($Windows){
            @(Select-String -Path "$dir\hwa.out.log" -SimpleMatch '[SyncRoundConservation]').Count
        }else{
            [int](Remote "grep -c SyncRoundConservation /userdata/HwaSimIR/logs/p5_$Name.log || true")
        }
        if($count -ge $CompletedRuns){return}
        Start-Sleep -Seconds 2
    }
    throw "STOP did not finish draining run $CompletedRuns"
}
$env:QT_FORCE_STDERR_LOGGING='1'
$overrides=@{
    P5Scene=$(if($Scene -eq 'display'){'plume'}else{$Scene});P5View='p6_base';
    P6Scene=$Scene;P6View=$View;P6CloudCount="$Clouds";P6DisableCloudMask="$DisableCloudMask";
    P6LegacyArt=$(if($LegacyArt){'1'}else{'0'});P6LargeClouds=$(if($LargeClouds){'1'}else{'0'});P6NativeFov=$(if($NativeFov){'1'}else{'0'});
    P6ExistingTargets=$(if($ExistingTargets){'1'}else{'0'});P6Vfx=$Vfx;SensorDisplayPreset=$Preset;P5MaterialView="$MaterialView";P5MaterialCase=$MaterialCase;
    P6LegacyNozzleAttachment=$(if($LegacyNozzleAttachment){'1'}else{'0'});
    P5LegacyVisuals=$(if($Legacy){'1'}else{'0'});
    HwaSimIRExitOnStop=$(if($RepeatWeather -ge 0){'false'}else{'true'});HwaSimIRLocalRecordingEnable='false';
    RenderPresentationMode='HeadlessOffscreen';TcpSendVideo='false';
    HwaSimIRDdsVideoEnable='true';H264FallbackToJpeg='false';
    H264Encoder=$(if($Windows){'ffmpeg'}else{'mpp'});
    Stage6DiagnosticsEnable='false';AnnotationOverlayInSensorImage='false';EnablePerfLog='true'
}
if($SmallClouds){
    $overrides.Stage7VolumetricCloudMinRadiusXYM='160';$overrides.Stage7VolumetricCloudMaxRadiusXYM='300'
    $overrides.Stage7VolumetricCloudMinRadiusZM='90';$overrides.Stage7VolumetricCloudMaxRadiusZM='160'
    $overrides.Stage7CloudTextureWorldSizeM='1800'
    $overrides.Stage7CloudOpticalDepthScale='3.0'
}
if($VolumeOff){$overrides.Stage7VolumetricCloudEnable='false'}
if($SheetOff){$overrides.P5SheetOff='1'}
if($Blend){$overrides.P5Blend='1'}
if($AssetBandCase){$overrides.P5AssetBandCase=$AssetBandCase}
if($null -ne $Gain){$overrides.Stage6DisplayGain="$Gain"}
if($null -ne $Gamma){$overrides.SensorInputDisplayGamma="$Gamma"}
if($Normal){
    foreach($key in @($overrides.Keys)){if($key.StartsWith('P6') -or $key.StartsWith('P5')){$overrides.Remove($key)}}
    $overrides.WorldCloudAudit='1'
    if($SheetOff){$overrides.WorldCloudSheetOff='1'}
    $overrides.WeatherAppearancePreset=$Appearance
    $overrides.Stage7VolumetricCloudMaxVisibleVolumes="$Clouds"
    $overrides.Stage7VolumetricCloudStreamingCenter='Camera'
    $overrides.Stage6AGCSampleSize="$SampleSize"
    if($HideCloudId){$overrides.WorldCloudAuditHideId=$HideCloudId}
    if($ReferenceSteps -gt 0){$overrides.WorldCloudReferenceSteps="$ReferenceSteps"}
}
if($AutoReference){$overrides.AgcFullReadbackReference='1'}
if($AgcTiming -or $AutoReference){$overrides.Stage6DiagnosticsEnable='true';$overrides.AGCDebugLog='true'}
if($OrdinaryPlate){$overrides.WorldCloudOrdinaryPlate='1'}
if($null -ne $AutoTargetHigh){$overrides.AGCTargetHighGray="$AutoTargetHigh"}
if($UseRuntimeWeather){
    foreach($key in @('WeatherAppearancePreset','Stage7VolumetricCloudMaxVisibleVolumes','Stage7VolumetricCloudStreamingCenter')){$overrides.Remove($key)}
}
if($UseRuntimeStreaming){$overrides.Remove('Stage7VolumetricCloudStreamingCenter')}
if($Preset -eq 'Runtime'){
    $overrides.Remove('SensorDisplayPreset')
    $overrides.Remove('Stage6AGCSampleSize')
}
if($Scene -eq 'display'){$overrides.P6CDisplayCase="$DisplayCase";$overrides.P6CFreezeAfter="$FreezeAfter";$overrides.P6CMappingLog='1'}
if($ProductionDefaults){$overrides.Clear()}
if($DiagnosticJson){
    $diagnostic=Get-Content -Encoding UTF8 $DiagnosticJson -Raw | ConvertFrom-Json
    foreach($property in $diagnostic.PSObject.Properties){$overrides[$property.Name]=[string]$property.Value}
}
$overrides | ConvertTo-Json | Set-Content -Encoding UTF8 "$dir\effective_environment.json"
if($CameraInput){$env:WeatherCameraInput=(Resolve-Path $CameraInput).Path}else{Remove-Item Env:WeatherCameraInput -ErrorAction SilentlyContinue}
if($RawDump){
    $overrides.LinearDiagnosticPath=$(if($Windows){"$dir\linear"}else{"/userdata/HwaSimIR/logs/p6d_$Name"})
    $overrides.LinearDiagnosticSeq="$DumpSeq"
    $overrides.Stage5OutputFrameDump='true';$overrides.Stage5OutputFrameDumpEvery="$DumpSeq"
    $overrides.Stage5OutputFrameDumpPath=$(if($Windows){"$dir\gpu_rgb8.png"}else{"/userdata/HwaSimIR/logs/p5_$Name.png"})
}
$overrides | ConvertTo-Json | Set-Content -Encoding UTF8 "$dir\effective_environment.json"
$env:P6ReceiverUiDump=Join-Path $dir 'receiver_ui.png'
$env:P7RecordingRoot=Join-Path $dir 'recording'
$env:P6ReceiverUiResponsiveCapture='1'
if($ReceiverFaultConfig){$env:P7ReceiverFaultConfig=(Resolve-Path $ReceiverFaultConfig).Path}else{Remove-Item Env:P7ReceiverFaultConfig -ErrorAction SilentlyContinue}
$env:P7SenderUiDump=Join-Path $dir 'sender_ui.png'
$env:P5DdsVideoPath=Join-Path $dir 'received.h264'
$env:P5DdsVideoSamples="$($Rate*[Math]::Min($CaptureSeconds,$Seconds))"
$videoExe=$ReceiverExe
$stimExe=$SenderExe
$video=$null;$stim=$null;$hwa=$null;$remoteSsh=$null;$thermal=$null
try {
    if(-not $SkipRemoteCleanup){Remote 'pkill -TERM -x HwaSim_IR 2>/dev/null || true'}
    if(-not $Windows){Remote 'cd /userdata/HwaSimIR && sha256sum HwaSim_IR Config/deployment_manifest.sha256' | Set-Content -Encoding ASCII "$dir\release.sha256"}
    else{Get-FileHash $RendererExe | Format-List | Out-File "$dir\release.sha256"}
    if(-not $Windows){
        $thermal=Start-Process 'F:\Programs\anaconda3\python.exe' -WindowStyle Hidden -PassThru -ArgumentList @(
            "$root\tools\p6d_thermal.py",'--output',"$dir\thermal.jsonl",'--seconds',"$($Seconds*2+150)"
        ) -RedirectStandardError "$dir\thermal.err.log" -RedirectStandardOutput "$dir\thermal.out.log"
    }
    function Start-CaseReceiver {
    Start-Process -FilePath $videoExe -WorkingDirectory (Split-Path $videoExe) -WindowStyle Hidden -PassThru -ArgumentList @(
        '--receive-transport=dds','--stream-role=direct',
        '--channel=precise',"--plat-id=$PlatId","--sensor-id=$SensorId",
        "--dds-qos=$root\HwaSim_IR\Bin\Config\DDS\ZRDDS_PROTOCOL_QOS.xml",
        "--dds-dump-first-frame=$dir\received.png","--dds-dump-frame-index=$DumpSeq",
        "--acceptance-exit-ms=$(($Seconds*$(if($RepeatWeather -ge 0){2}else{1})+120)*1000)"
    ) -RedirectStandardOutput "$dir\video.out.log" -RedirectStandardError "$dir\video.err.log"
    }
    if($ReceiverJoinDelay -le 0){$video=Start-CaseReceiver}
    Start-Sleep -Seconds 2
    if($Windows){
        foreach($key in $overrides.Keys){[Environment]::SetEnvironmentVariable($key,$overrides[$key],'Process')}
        $localConfig=Join-Path $dir 'NetworkConfig.ini'
        (Get-Content "$root\HwaSim_IR\Bin\Config\NetworkConfig_precise.ini") -replace 'localIp=192.168.1.116','localIp=0.0.0.0' -replace 'platID=1001',"platID=$PlatId" -replace 'sensorID=2',"sensorID=$SensorId" -replace 'localPort=8888',"localPort=$([int](8888+$PlatId-1001))" -replace 'acceptSensorBroadcast=true','acceptSensorBroadcast=false' | Set-Content -Encoding ASCII $localConfig
        $hwa=Start-Process -FilePath $RendererExe -WorkingDirectory "$root\HwaSim_IR\Bin" -WindowStyle Hidden -PassThru -ArgumentList @('--channel=precise',"--network-config=$localConfig") -RedirectStandardOutput "$dir\hwa.out.log" -RedirectStandardError "$dir\hwa.err.log"
    }else{
        $assignments=($overrides.GetEnumerator() | ForEach-Object {
            if($_.Value -notmatch '^[a-zA-Z0-9_/,.-]*$'){throw 'Invalid environment value'}
            "$( $_.Key )='$( $_.Value )'"
        }) -join ' '
        # Keep SSH as an observable foreground command in a hidden host process.
        # A remote shell background job can keep the Windows SSH call open and
        # prevent the stimulus from ever being started.
        $remoteSsh=Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru -ArgumentList @(
            '-o','StrictHostKeyChecking=yes','root@192.168.1.116',
            "cd /userdata/HwaSimIR && $assignments ./run_precise.sh >logs/p5_$Name.log 2>&1"
        ) -RedirectStandardOutput "$dir\ssh.out.log" -RedirectStandardError "$dir\ssh.err.log"
    }
    Start-Sleep -Seconds 5
    $env:P5NoTargets=$(if($ExistingTargets -or $Normal){'0'}else{'1'})
    $env:P6TestTargetType=$TargetType
    $stim=Start-Process -FilePath $stimExe -WorkingDirectory "$root\DataDrivenTestQT" -WindowStyle Hidden -PassThru -ArgumentList @(
        '--channel=precise',"--plat-id=$PlatId","--sensor-id=$SensorId","--sim-mode=$SimMode","--video-fps=$OutputFps","--send-step-ms=$((1000.0/$Rate).ToString('F6',[Globalization.CultureInfo]::InvariantCulture))",
        '--phase1d-h264=1',"--save-mp4=$SaveMp4","--duration-sec=$Seconds","--env-sky=$Weather","--sensor-band=$Band",
        "--sensor-pixel-angle-urad=$PixelAngle","--sensor-fields-json=$SensorFieldsJson",'--engine-state=1','--strike-flag=0','--freeze-geometry','--utc-hour=6.0',"--pause-start-sec=$PauseStart","--pause-duration-sec=$PauseDuration"
    ) -RedirectStandardOutput "$dir\stim.out.log" -RedirectStandardError "$dir\stim.err.log"
    if($ReceiverJoinDelay -gt 0){
        $deadline=[DateTime]::UtcNow.AddSeconds(60)
        while(-not (Select-String -Path "$dir\stim.err.log" -SimpleMatch '[StimDDS] type=control command=2 sent=1' -Quiet)){
            if([DateTime]::UtcNow -gt $deadline){throw 'Late join: START not observed'}
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Milliseconds ([int]($ReceiverJoinDelay*1000))
        $video=Start-CaseReceiver
    }
    if($ReceiverRestartAt -gt 0){
        $deadline=[DateTime]::UtcNow.AddSeconds(60)
        while(-not (Select-String -Path "$dir\stim.err.log" -SimpleMatch '[StimDDS] type=control command=2 sent=1' -Quiet)){
            if([DateTime]::UtcNow -gt $deadline){throw 'Restart: START not observed'}
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Milliseconds ([int]($ReceiverRestartAt*1000))
        $video.CloseMainWindow()|Out-Null
        if(-not $video.WaitForExit(10000)){throw 'Receiver did not flush before restart'}
        foreach($artifactName in @('video.out.log','video.err.log','receiver_ui.png','receiver_ui.png.layout.json')){
            if(Test-Path -LiteralPath "$dir\$artifactName"){Move-Item -LiteralPath "$dir\$artifactName" -Destination "$dir\before_restart_$artifactName"}
        }
        Start-Sleep -Milliseconds ([int]($ReceiverRestartGap*1000))
        $video=Start-CaseReceiver
        @{restartAt=$ReceiverRestartAt;gapSeconds=$ReceiverRestartGap;kind='actual_process_close_and_restart';producerSessionUnchanged=$true}|ConvertTo-Json|Set-Content -Encoding UTF8 "$dir\restart.json"
    }
    while(-not $stim.WaitForExit(1000)){}
    if($null -ne $stim.ExitCode -and $stim.ExitCode -ne 0){throw "Stimulator failed with exit $($stim.ExitCode)"}
    if(Select-String -Path "$dir\stim.err.log" -Pattern '\[StimDrain\]\[ERROR\]|DDS start failed|\[StimInitAck\]\[FATAL\]|Cannot open sensor|Invalid sensor|Invalid telemetry|Telemetry fixture' -Quiet){throw 'Stimulator input, transport or application drain failed'}
    if($RepeatWeather -ge 0){
        # Each sender uses the existing RESET -> INIT -> START -> STOP sequence.
        # Keep the renderer and DDS receiver alive to exercise real state reuse.
        Wait-RepeatDrain 1
        $stim=Start-Process -FilePath $stimExe -WorkingDirectory "$root\DataDrivenTestQT" -WindowStyle Hidden -PassThru -ArgumentList @(
            '--channel=precise',"--plat-id=$PlatId","--sensor-id=$SensorId","--sim-mode=$SimMode","--video-fps=$OutputFps","--send-step-ms=$((1000.0/$Rate).ToString('F6',[Globalization.CultureInfo]::InvariantCulture))",
            '--phase1d-h264=1',"--save-mp4=$SaveMp4","--duration-sec=$Seconds","--env-sky=$RepeatWeather","--sensor-band=$Band",
            "--sensor-pixel-angle-urad=$PixelAngle","--sensor-fields-json=$SensorFieldsJson",'--engine-state=1','--strike-flag=0','--freeze-geometry','--utc-hour=6.0'
        ) -RedirectStandardOutput "$dir\stim2.out.log" -RedirectStandardError "$dir\stim2.err.log"
        while(-not $stim.WaitForExit(1000)){}
        Wait-RepeatDrain 2
    }
    # Leave the receiver alive until producer STOP drains its DDS queues/ACKs.
    # Copying the log after an arbitrary four seconds misses the final counters.
    $producer=if($Windows){$hwa}else{$remoteSsh}
    if($RepeatWeather -lt 0){
        Wait-RepeatDrain 1
        if(-not $ProductionDefaults -and $producer -and -not $producer.HasExited){$producer.WaitForExit(10000)|Out-Null}
    }
    if(-not $Windows){
        & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p5_$Name.log" "$dir\board.log"
        if($RawDump){
            & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p6d_$Name.pfm" "$dir\linear.pfm"
            if($Preset -eq 'Auto'){
                & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p6d_$($Name)_stats.pfm" "$dir\linear_stats.pfm"
            }
            & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p6d_$($Name)_rgb8.png" "$dir\linear_rgb8.png"
        }
    }
    if(-not $video.HasExited){$video.CloseMainWindow()|Out-Null;$video.WaitForExit(10000)|Out-Null}
}finally{
    $cleanupPreviousPreference=$ErrorActionPreference
    $ErrorActionPreference="Continue"
    if(-not $Windows){
        # Keep failure evidence too; a failed drain must not leave an idle renderer.
        & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p5_$Name.log" "$dir\board.log"
        Remote 'pkill -TERM -x HwaSim_IR 2>/dev/null || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR >/dev/null'
        # The remote launcher restores P3 policy in its EXIT trap. Wait for that
        # shell as well as the ELF before another case can apply its policy.
        if($remoteSsh -and -not $remoteSsh.HasExited){
            if(-not $remoteSsh.WaitForExit(10000)){throw 'Previous remote launcher did not finish its cleanup'}
        }
        & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p5_$Name.log" "$dir\board.log"
    }
    foreach($process in @($stim,$hwa,$video,$remoteSsh,$thermal)){
        if($process){$process.Refresh();if(-not $process.HasExited){Stop-Process -Id $process.Id -Force}}
    }
    # A following case in this same PowerShell must not inherit this case's diagnostics.
    foreach($variable in @(Get-ChildItem Env:)){if(-not $caseEnvironmentBefore.ContainsKey($variable.Name)){[Environment]::SetEnvironmentVariable($variable.Name,$null,"Process")}}
    foreach($key in $caseEnvironmentBefore.Keys){[Environment]::SetEnvironmentVariable($key,$caseEnvironmentBefore[$key],"Process")}
    $ErrorActionPreference=$cleanupPreviousPreference
}
if(-not (Test-Path "$dir\received.png")){throw "P5 case produced no receive image: $Name"}
if((Get-Item "$dir\received.png").LastWriteTimeUtc -lt $caseStartUtc){throw "P5 case has only a stale receive image: $Name"}
$renderLog=if($Windows){"$dir\hwa.err.log"}else{"$dir\board.log"}
if(Select-String -Path $renderLog -Pattern ':display[^\r\n]*\(error\)|:gobj\(error\)|:gsg\(error\)|GL error 0x|\[LinearReadback\]\[ERROR\]|\[P6LinearCapture\]\[ERROR\]' -Quiet){throw "P6 case contains a graphics/readback error: $Name"}
if(Select-String -Path $renderLog -Pattern 'Assertion failed:|Shader input .* is not present' -Quiet){throw "P6D case contains a Panda assertion: $Name"}
if(Select-String -Path $renderLog -Pattern '\[(RealtimeIngress|DdsFrameProducts|DdsVideo)\]\[(ERROR|FATAL)\]' -Quiet){throw "P6D case contains a transport failure: $Name"}
if($RawDump -and (!(Test-Path "$dir\linear.pfm") -or !(Test-Path "$dir\linear_rgb8.png"))){throw "P6 case missing required linear/RGB capture: $Name"}
& 'F:\Programs\anaconda3\python.exe' "$PSScriptRoot\p7_validate_conservation.py" $dir --strict
if($LASTEXITCODE -ne 0){throw "P7 writer-to-board conservation or transport event failed: $Name (see conservation.json)"}
Write-Output "[P7Case] name=$Name path=$dir"
