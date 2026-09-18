[CmdletBinding()]
param(
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [int]$RoundSeconds = 10,
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($RoundSeconds -lt 8 -or $RoundSeconds -gt 60) { throw 'RoundSeconds must be 8..60' }
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$senderDir = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release'
$receiverDir = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release'
$senderExe = Join-Path $senderDir 'DataDrivenTestQT.exe'
$receiverExe = Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe'
$senderConfig = Join-Path $senderDir 'NetworkConfig.ini'
$receiverConfig = Join-Path $receiverDir 'NetworkConfig.ini'
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
if (-not $BoardPassword) { $BoardPassword = '123' }
foreach ($path in @($senderExe,$receiverExe,$senderConfig,$receiverConfig)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing lifecycle runtime file: $path" }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $repo "logs\p12\p12d\lifecycle\ordinary-lifecycle-$stamp"
}
if (Test-Path -LiteralPath $OutputDirectory) { throw "Refusing to overwrite lifecycle evidence: $OutputDirectory" }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8 = New-Object Text.UTF8Encoding($false)
$senderConfigBackup = [IO.File]::ReadAllBytes($senderConfig)
$oldQt = $env:QT_FORCE_STDERR_LOGGING
$oldReceiverDump = $env:P6ReceiverUiDump
$oldResponsive = $env:P6ReceiverUiResponsiveCapture
$oldSenderDump = $env:P7SenderUiDump
$oldRecordingRoot = $env:P7RecordingRoot

$businessOverrides = @(
    'P5Scene','P5View','P5AssetBandCase','P6Scene','P6ExistingTargets','P6TestTargetType',
    'WeatherCameraInput','WeatherStartSec','SensorDisplayPreset','HwaSimIRDdsVideoTopic',
    'HwaSimIRProtocolPlatId','HwaSimIRProtocolSensorId','ZRDDS_QOS_FILE'
)
foreach ($name in $businessOverrides) {
    if (-not [string]::IsNullOrWhiteSpace([Environment]::GetEnvironmentVariable($name,'Process'))) {
        throw "Unexpected inherited business override: $name"
    }
}
$conflicts = @(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue)
if ($conflicts.Count -ne 0) { throw "Ordinary Windows process already running: $($conflicts.Id -join ',')" }

function Use-BoardAuth([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p12-lifecycle'
        & $Action
    } finally {
        $env:HWASIMIR_SSH_PASSWORD=$oldPassword; $env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire; $env:DISPLAY=$oldDisplay
    }
}
function Ssh-Args {
    @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new',
      '-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password')
}
function Invoke-Board([string]$Command) {
    $result=Use-BoardAuth {
        $saved=$ErrorActionPreference; $ErrorActionPreference='Continue'
        $lines=& ssh.exe @(Ssh-Args) "$BoardUser@$BoardHost" $Command 2>&1
        $code=$LASTEXITCODE; $ErrorActionPreference=$saved
        [pscustomobject]@{Lines=@($lines);Code=$code}
    }
    if($result.Code -ne 0){throw "Board command failed ($($result.Code)): $Command`n$($result.Lines -join "`n")"}
    return @($result.Lines | ForEach-Object { [string]$_ })
}
function Set-SenderBand([int]$Band,[string]$EvidencePath) {
    $text=[IO.File]::ReadAllText($senderConfig)
    $updated=[regex]::Replace($text,'(?im)^trackerSensorBand\s*=\s*\d+\s*$',"trackerSensorBand=$Band")
    if($updated -eq $text -and $text -notmatch "(?im)^trackerSensorBand\s*=\s*$Band\s*$"){
        throw 'Effective sender INI lacks trackerSensorBand'
    }
    [IO.File]::WriteAllText($senderConfig,$updated,$utf8)
    [IO.File]::WriteAllText($EvidencePath,$updated,$utf8)
}
function Start-Receiver([string]$Name) {
    $env:P6ReceiverUiDump=Join-Path $OutputDirectory "$Name.widget.png"
    $env:P6ReceiverUiResponsiveCapture='1'
    $proc=Start-Process -FilePath $receiverExe -WorkingDirectory $receiverDir -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $OutputDirectory "$Name.out.log") `
        -RedirectStandardError (Join-Path $OutputDirectory "$Name.err.log")
    $null=$proc.Handle
    return $proc
}
function Stop-OwnedProcess([System.Diagnostics.Process]$Process,[int]$TimeoutMs=10000) {
    if(-not $Process){return}
    $Process.Refresh()
    if($Process.HasExited){return}
    [void]$Process.CloseMainWindow()
    if(-not $Process.WaitForExit($TimeoutMs)){Stop-Process -Id $Process.Id -Force}
}
function Invoke-SenderRound([string]$Name,[int]$Band) {
    $roundDir=Join-Path $OutputDirectory $Name
    New-Item -ItemType Directory -Path $roundDir | Out-Null
    Set-SenderBand $Band (Join-Path $roundDir 'effective_sender_NetworkConfig.ini')
    $env:P7SenderUiDump=Join-Path $roundDir 'sender.widget.png'
    $sender=Start-Process -FilePath $senderExe -WorkingDirectory $senderDir -WindowStyle Normal -PassThru `
        -ArgumentList @("--duration-sec=$RoundSeconds") `
        -RedirectStandardOutput (Join-Path $roundDir 'sender.out.log') `
        -RedirectStandardError (Join-Path $roundDir 'sender.err.log')
    $null=$sender.Handle
    return [pscustomobject]@{Process=$sender;Directory=$roundDir;Name=$Name;Band=$Band}
}
function Complete-SenderRound($Round) {
    if(-not $Round.Process.WaitForExit(($RoundSeconds+45)*1000)){
        Stop-Process -Id $Round.Process.Id -Force
        throw "Sender round timed out: $($Round.Name)"
    }
    $text=(Get-Content -LiteralPath (Join-Path $Round.Directory 'sender.err.log') -Raw)
    if($text -notmatch '\[StimInitAck\].*received=1.*ready=1'){throw "$($Round.Name): INIT ACK missing"}
    $final=[regex]::Match($text,'\[StimFinal\].*successfulRealtimeWrites=(\d+)')
    if(-not $final.Success -or [int]$final.Groups[1].Value -le 0){throw "$($Round.Name): no realtime writes"}
    if($text -notmatch '\[StimDDS\] type=control command=3 sent=1'){throw "$($Round.Name): STOP was not sent"}
    return [ordered]@{
        name=$Round.Name; protocolBand=$Round.Band; senderPid=$Round.Process.Id
        successfulRealtimeWrites=[int]$final.Groups[1].Value
        initAck=$true; stopSent=$true
        configSha256=(Get-FileHash -LiteralPath (Join-Path $Round.Directory 'effective_sender_NetworkConfig.ini') -Algorithm SHA256).Hash.ToLowerInvariant()
        senderExeSha256=(Get-FileHash -LiteralPath $senderExe -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}
function Get-MaxMetric([string]$Text,[string]$Field) {
    $values=@([regex]::Matches($Text,'"'+[regex]::Escape($Field)+'":"?(\d+)') | ForEach-Object {[int64]$_.Groups[1].Value})
    if($values.Count -eq 0){return -1}
    return [int64](($values|Measure-Object -Maximum).Maximum)
}
function Read-ReceiverEvidence([string]$Name) {
    $text=(Get-Content -LiteralPath (Join-Path $OutputDirectory "$Name.out.log") -Raw)+"`n"+
        (Get-Content -LiteralPath (Join-Path $OutputDirectory "$Name.err.log") -Raw)
    $decoded=Get-MaxMetric $text 'decodedFrames'
    $errors=Get-MaxMetric $text 'decodeErrors'
    $identityRejected=Get-MaxMetric $text 'statusIdentityRejected'
    $statusAccepted=Get-MaxMetric $text 'statusAccepted'
    if($decoded -le 0){throw "$Name did not decode an actual frame"}
    if($errors -gt 0){throw "$Name reported decode errors: $errors"}
    if($identityRejected -gt 0){throw "$Name rejected status identity: $identityRejected"}
    if($statusAccepted -le 0 -or $text -notmatch '\[VideoStatus\].*topic=HwaSimIR\.Video\.1001\.2\.H264'){
        throw "$Name did not auto-discover the identity video topic"
    }
    $widget=Join-Path $OutputDirectory "$Name.widget.png"
    return [ordered]@{
        name=$Name; decodedFramesMax=$decoded; decodeErrorsMax=$errors
        statusAcceptedMax=$statusAccepted; statusIdentityRejectedMax=$identityRejected
        identityTopic='HwaSimIR.Video.1001.2.H264'
        widgetSha256=if(Test-Path -LiteralPath $widget){(Get-FileHash -LiteralPath $widget -Algorithm SHA256).Hash.ToLowerInvariant()}else{$null}
    }
}

$receiver1=$null; $receiver2=$null; $senderRound=$null; $boardProcess=$null
$roundResults=@(); $pidSamples=@()
try {
    $taskPath=[Environment]::GetEnvironmentVariable('Path','Process')
    [Environment]::SetEnvironmentVariable('PATH',$null,'Process')
    [Environment]::SetEnvironmentVariable('Path',$taskPath,'Process')
    $env:QT_FORCE_STDERR_LOGGING='1'
    $env:P7RecordingRoot=Join-Path $OutputDirectory 'recording'
    Copy-Item -LiteralPath $senderConfig -Destination (Join-Path $OutputDirectory 'DataDrivenTestQT.NetworkConfig.original.ini')
    Copy-Item -LiteralPath $receiverConfig -Destination (Join-Path $OutputDirectory 'VideoDisplay.NetworkConfig.ini')

    $preflight=Invoke-Board "set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR >/dev/null; test -x HwaSim_IR; test -x run_precise.sh; echo ElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo BuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo DeploymentVersionSha256=`$(sha256sum Config/deployment_version.env|awk '{print `$1}')"
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory 'board_preflight.log'),$preflight,$utf8)
    $remoteTimeout=($RoundSeconds*3)+150
    $remote="cd /userdata/HwaSimIR && timeout -s TERM -k 8s ${remoteTimeout}s ./run_precise.sh"
    $boardProcess=Use-BoardAuth {
        Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru `
            -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
            -RedirectStandardOutput (Join-Path $OutputDirectory 'board.log') `
            -RedirectStandardError (Join-Path $OutputDirectory 'board.ssh.err.log')
    }
    $null=$boardProcess.Handle
    Start-Sleep -Seconds 4
    $pidSamples += [ordered]@{milestone='board_cold_start';pid=[int](Invoke-Board 'pgrep -x HwaSim_IR')[-1]}

    # Round 1 deliberately starts the ordinary sender first.  The zero-argument
    # display is launched late and must discover the status/topic on its own.
    $senderRound=Invoke-SenderRound '01_swir_late_receiver' 0
    Start-Sleep -Seconds 3
    $receiver1=Start-Receiver 'receiver_late'
    $roundResults += Complete-SenderRound $senderRound
    $senderRound=$null
    Start-Sleep -Seconds 3
    $pidSamples += [ordered]@{milestone='after_swir_stop_input_gap';pid=[int](Invoke-Board 'pgrep -x HwaSim_IR')[-1]}

    # Round 2 proves STOP->START recovery and restarts the receiver while MWIR is
    # live.  The replacement again has no topic/identity/QoS command arguments.
    $senderRound=Invoke-SenderRound '02_mwir_receiver_restart' 2
    Start-Sleep -Seconds 4
    Stop-OwnedProcess $receiver1
    $receiver1=$null
    Start-Sleep -Seconds 1
    $receiver2=Start-Receiver 'receiver_restarted'
    $roundResults += Complete-SenderRound $senderRound
    $senderRound=$null
    Start-Sleep -Seconds 3
    $pidSamples += [ordered]@{milestone='after_mwir_stop_input_gap';pid=[int](Invoke-Board 'pgrep -x HwaSim_IR')[-1]}

    # Round 3 proves same-process MWIR->SWIR re-INIT and another STOP->START.
    $senderRound=Invoke-SenderRound '03_swir_reinit' 0
    $roundResults += Complete-SenderRound $senderRound
    $senderRound=$null
    Start-Sleep -Seconds 3
    $pidSamples += [ordered]@{milestone='after_final_stop';pid=[int](Invoke-Board 'pgrep -x HwaSim_IR')[-1]}
    Stop-OwnedProcess $receiver2
    $receiver2=$null

    Invoke-Board 'pkill -TERM -x HwaSim_IR 2>/dev/null || true' | Out-Null
    if(-not $boardProcess.WaitForExit(30000)){Stop-Process -Id $boardProcess.Id -Force;throw 'Board launcher did not exit'}
    $boardProcess.Refresh()
    if([int]$boardProcess.ExitCode -notin @(0,124,143)){throw "Board launcher exit code $($boardProcess.ExitCode)"}

    $boardText=Get-Content -LiteralPath (Join-Path $OutputDirectory 'board.log') -Raw
    $activeBands=@([regex]::Matches($boardText,'\[Stage1\] Sensor profile \(init-command\): protocolBand=(\d+), band=(SWIR|MWIR)') | ForEach-Object {[int]$_.Groups[1].Value})
    if(($activeBands -join ',') -ne '0,2,0'){throw "Active INIT band order is not 0,2,0: $($activeBands -join ',')"}
    $drains=@([regex]::Matches($boardText,'\[OutputRoundDrain\] reason=stop round=(\d+) targetFrames=(\d+) completedFrames=(\d+)'))
    if($drains.Count -ne 3){throw "Expected three STOP drains, got $($drains.Count)"}
    foreach($drain in $drains){
        if([int]$drain.Groups[2].Value -le 0 -or $drain.Groups[2].Value -ne $drain.Groups[3].Value){
            throw "Incomplete STOP drain: $($drain.Value)"
        }
    }
    if($boardText -match 'Assertion failed|\[Stage6 FinalPipeline\]\[ERROR\]|raw_buffer_unavailable|Segmentation fault'){
        throw 'Board lifecycle log contains a fatal renderer marker'
    }
    $pids=@($pidSamples|ForEach-Object pid|Select-Object -Unique)
    if($pids.Count -ne 1){throw "HwaSim_IR PID changed during retained lifecycle: $($pids -join ',')"}

    $receiverEvidence=@(Read-ReceiverEvidence 'receiver_late';Read-ReceiverEvidence 'receiver_restarted')
    $summary=[ordered]@{
        schema='hwasimir.p12d.ordinary-dds-lifecycle.v1';result='PASS'
        startedUtc=(Get-Item -LiteralPath (Join-Path $OutputDirectory 'board.log')).CreationTimeUtc.ToString('o')
        finishedUtc=[DateTime]::UtcNow.ToString('o');ddsOnly=$true;ordinaryEntryPoints=$true
        forcedIdentityTopicQos=$false;windowsAppsVisible=$true;singleBoardOwner=$true
        board="$BoardUser@$BoardHost";boardPid=[int]$pids[0];pidSamples=$pidSamples
        activeInitBandOrder=$activeBands;rounds=$roundResults;receivers=$receiverEvidence
        gates=[ordered]@{
            coldStart=$true;lateReceiverAutoDiscovery=$true;receiverRestartAutoDiscovery=$true
            inputGapRecovery=$true;sameProcessSwirMwirSwirReinit=$true
            stopDrainThreeOfThree=$true;startAfterStopTwice=$true;identityFilterPreserved=$true
        }
        boardLogSha256=(Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'board.log') -Algorithm SHA256).Hash.ToLowerInvariant()
        receiverExeSha256=(Get-FileHash -LiteralPath $receiverExe -Algorithm SHA256).Hash.ToLowerInvariant()
        senderExeSha256=(Get-FileHash -LiteralPath $senderExe -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'lifecycle_summary.json'),($summary|ConvertTo-Json -Depth 10)+"`n",$utf8)
    Write-Output "[P12 Lifecycle] result=PASS output=$OutputDirectory"
}
finally {
    if($senderRound -and $senderRound.Process){
        $senderRound.Process.Refresh();if(-not $senderRound.Process.HasExited){Stop-Process -Id $senderRound.Process.Id -Force -ErrorAction SilentlyContinue}
    }
    Stop-OwnedProcess $receiver1
    Stop-OwnedProcess $receiver2
    try { Invoke-Board 'pkill -TERM -x HwaSim_IR 2>/dev/null || true' | Out-Null } catch {}
    if($boardProcess){$boardProcess.Refresh();if(-not $boardProcess.HasExited){Stop-Process -Id $boardProcess.Id -Force -ErrorAction SilentlyContinue}}
    [IO.File]::WriteAllBytes($senderConfig,$senderConfigBackup)
    $env:QT_FORCE_STDERR_LOGGING=$oldQt
    $env:P6ReceiverUiDump=$oldReceiverDump
    $env:P6ReceiverUiResponsiveCapture=$oldResponsive
    $env:P7SenderUiDump=$oldSenderDump
    $env:P7RecordingRoot=$oldRecordingRoot
}
