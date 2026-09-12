param(
    [string]$Name='smoke', [string]$Scene='mixed', [string]$View='oblique',
    [int]$MaterialView=0, [string]$MaterialCase='A', [int]$Band=2,
    [int]$Rate=60, [int]$Seconds=10, [int]$Weather=1, [switch]$Legacy,
    [switch]$Windows, [switch]$SmallClouds, [double]$PauseStart=-1,
    [double]$PauseDuration=0, [switch]$VolumeOff, [switch]$SheetOff,
    [switch]$Blend, [double]$Gamma=1.0, [switch]$RawDump, [string]$AssetBandCase='', [switch]$NoTargets,
    [int]$RepeatWeather=-1
)
$ErrorActionPreference='Stop'
$caseStartUtc=[DateTime]::UtcNow
$root=Split-Path -Parent $PSScriptRoot
if($env:HWASIMIR_SSH_PASSWORD){
    $env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
    $env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p5'
}
$dir=Join-Path $root "logs\p5\$Name"
New-Item -ItemType Directory -Force $dir | Out-Null
$dir=(Resolve-Path $dir).Path
@{host=$(if($Windows){'Windows'}else{'RK3588'});scene=$Scene;view=$View;rate=$Rate;seconds=$Seconds;
  band=$Band;weather=$Weather;gamma=$Gamma;simMode=1;utcHour=6.0;smallClouds=[bool]$SmallClouds;
  volumeOff=[bool]$VolumeOff;sheetOff=[bool]$SheetOff;legacy=[bool]$Legacy;materialView=$MaterialView;
  materialCase=$MaterialCase;assetBandCase=$AssetBandCase;pauseStart=$PauseStart;pauseDuration=$PauseDuration
  noTargets=[bool]$NoTargets;repeatWeather=$RepeatWeather;
  receiverSha256=(Get-FileHash -Algorithm SHA256 "$root\HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe").Hash
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
    for($attempt=0;$attempt -lt 15;$attempt++){
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
    P5Scene=$Scene;P5View=$View;P5MaterialView="$MaterialView";P5MaterialCase=$MaterialCase;
    P5LegacyVisuals=$(if($Legacy){'1'}else{'0'});
    HwaSimIRExitOnStop=$(if($RepeatWeather -ge 0){'false'}else{'true'});HwaSimIRLocalRecordingEnable='false';
    RenderPresentationMode='HeadlessOffscreen';TcpSendVideo='false';
    HwaSimIRDdsVideoEnable='true';H264FallbackToJpeg='false';
    H264Encoder=$(if($Windows){'ffmpeg'}else{'mpp'});
    Stage6Diagnostics='false';AnnotationOverlayInSensorImage='false';EnablePerfLog='true'
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
$overrides.SensorInputDisplayGamma="$Gamma"
if($RawDump){
    $overrides.Stage5OutputFrameDump='true';$overrides.Stage5OutputFrameDumpEvery="$($Rate*3)"
    $overrides.Stage5OutputFrameDumpPath=$(if($Windows){"$dir\gpu_rgb8.png"}else{"/userdata/HwaSimIR/logs/p5_$Name.png"})
}
$env:P5DdsVideoPath=Join-Path $dir 'received.h264'
$env:P5DdsVideoSamples="$($Rate*6)"
$videoExe=Join-Path $root 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimExe=Join-Path $root 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$video=$null;$stim=$null;$hwa=$null;$remoteSsh=$null
try {
    Remote 'pkill -TERM -x HwaSim_IR 2>/dev/null || true'
    $video=Start-Process -FilePath $videoExe -WorkingDirectory (Split-Path $videoExe) -WindowStyle Hidden -PassThru -ArgumentList @(
        '--receive-transport=dds','--stream-role=direct',
        '--channel=precise','--plat-id=1001','--sensor-id=2',
        "--dds-qos=$root\HwaSim_IR\Bin\Config\DDS\ZRDDS_PROTOCOL_QOS.xml",
        "--dds-dump-first-frame=$dir\received.png","--dds-dump-frame-index=$($Rate*3)",
        "--acceptance-exit-ms=$(($Seconds*$(if($RepeatWeather -ge 0){2}else{1})+120)*1000)"
    ) -RedirectStandardOutput "$dir\video.out.log" -RedirectStandardError "$dir\video.err.log"
    Start-Sleep -Seconds 2
    if($Windows){
        foreach($key in $overrides.Keys){[Environment]::SetEnvironmentVariable($key,$overrides[$key],'Process')}
        $localConfig=Join-Path $dir 'NetworkConfig.ini'
        (Get-Content "$root\HwaSim_IR\Bin\Config\NetworkConfig_precise.ini") -replace 'localIp=192.168.1.116','localIp=0.0.0.0' | Set-Content -Encoding ASCII $localConfig
        $hwa=Start-Process -FilePath "$root\HwaSim_IR\Bin\HwaSim_IR.exe" -WorkingDirectory "$root\HwaSim_IR\Bin" -WindowStyle Hidden -PassThru -ArgumentList @('--channel=precise',"--network-config=$localConfig") -RedirectStandardOutput "$dir\hwa.out.log" -RedirectStandardError "$dir\hwa.err.log"
    }else{
        $assignments=($overrides.GetEnumerator() | ForEach-Object {
            if($_.Value -notmatch '^[a-zA-Z0-9_/.-]*$'){throw 'Invalid environment value'}
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
    $env:P5NoTargets=if($NoTargets){'1'}else{'0'}
    $stim=Start-Process -FilePath $stimExe -WorkingDirectory "$root\DataDrivenTestQT" -WindowStyle Hidden -PassThru -ArgumentList @(
        '--channel=precise','--plat-id=1001','--sensor-id=2','--sim-mode=1',"--video-fps=$Rate",
        '--phase1d-h264=1','--save-mp4=0',"--duration-sec=$Seconds","--env-sky=$Weather","--sensor-band=$Band",
        '--engine-state=1','--strike-flag=0','--freeze-geometry','--utc-hour=6.0',"--pause-start-sec=$PauseStart","--pause-duration-sec=$PauseDuration"
    ) -RedirectStandardOutput "$dir\stim.out.log" -RedirectStandardError "$dir\stim.err.log"
    while(-not $stim.WaitForExit(1000)){}
    if($RepeatWeather -ge 0){
        # Each sender uses the existing RESET -> INIT -> START -> STOP sequence.
        # Keep the renderer and DDS receiver alive to exercise real state reuse.
        Wait-RepeatDrain 1
        $stim=Start-Process -FilePath $stimExe -WorkingDirectory "$root\DataDrivenTestQT" -WindowStyle Hidden -PassThru -ArgumentList @(
            '--channel=precise','--plat-id=1001','--sensor-id=2','--sim-mode=1',"--video-fps=$Rate",
            '--phase1d-h264=1','--save-mp4=0',"--duration-sec=$Seconds","--env-sky=$RepeatWeather","--sensor-band=$Band",
            '--engine-state=1','--strike-flag=0','--freeze-geometry','--utc-hour=6.0'
        ) -RedirectStandardOutput "$dir\stim2.out.log" -RedirectStandardError "$dir\stim2.err.log"
        while(-not $stim.WaitForExit(1000)){}
        Wait-RepeatDrain 2
    }
    # Leave the receiver alive until producer STOP drains its DDS queues/ACKs.
    # Copying the log after an arbitrary four seconds misses the final counters.
    $producer=if($Windows){$hwa}else{$remoteSsh}
    if($RepeatWeather -lt 0 -and $producer -and -not $producer.HasExited){$producer.WaitForExit(30000)|Out-Null}
    if(-not $Windows){
        & scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p5_$Name.log" "$dir\board.log"
        if($RawDump){& scp.exe -o StrictHostKeyChecking=yes "root@192.168.1.116:/userdata/HwaSimIR/logs/p5_$Name.png" "$dir\gpu_rgb8.png"}
    }
    if(-not $video.HasExited){$video.CloseMainWindow()|Out-Null;$video.WaitForExit(10000)|Out-Null}
}finally{
    if($RepeatWeather -ge 0 -and -not $Windows){Remote 'pkill -TERM -x HwaSim_IR 2>/dev/null || true'}
    foreach($process in @($stim,$hwa,$video,$remoteSsh)){
        if($process){$process.Refresh();if(-not $process.HasExited){Stop-Process -Id $process.Id -Force}}
    }
}
if(-not (Test-Path "$dir\received.png")){throw "P5 case produced no receive image: $Name"}
if((Get-Item "$dir\received.png").LastWriteTimeUtc -lt $caseStartUtc){throw "P5 case has only a stale receive image: $Name"}
$renderLog=if($Windows){"$dir\hwa.err.log"}else{"$dir\board.log"}
if(Select-String -Path $renderLog -Pattern ':display[^\r\n]*\(error\)|GL error 0x' -Quiet){throw "P5 case contains a graphics error: $Name"}
Write-Output "[P5Case] name=$Name path=$dir"
