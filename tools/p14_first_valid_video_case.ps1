[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('SWIR','MWIR')]
    [string]$Band,
    [Parameter(Mandatory=$true)]
    [string]$Name,
    [string]$OutputRoot = 'logs\p14\runs',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$receiverDir = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release'
$receiverExe = Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe'
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$ffmpegDir = Join-Path $repo '.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin'
$ffmpeg = Join-Path $ffmpegDir 'ffmpeg.exe'
$ffprobe = Join-Path $ffmpegDir 'ffprobe.exe'
foreach($required in @($receiverExe,$askPass,$ffmpeg,$ffprobe)) {
    if(-not (Test-Path -LiteralPath $required -PathType Leaf)){throw "Missing required file: $required"}
}
if(-not $BoardPassword){throw 'BoardPassword or HWASIMIR_SSH_PASSWORD is required'}
if($Name -notmatch '^[A-Za-z0-9_.-]+$'){throw "Unsafe case name: $Name"}

$root = if([IO.Path]::IsPathRooted($OutputRoot)){$OutputRoot}else{Join-Path $repo $OutputRoot}
$out = Join-Path $root $Name
if(Test-Path -LiteralPath $out){throw "Refusing to overwrite evidence: $out"}
New-Item -ItemType Directory -Path $out | Out-Null
$out = (Resolve-Path -LiteralPath $out).Path
$recordingRoot = Join-Path $out 'recording'
$diagnosticDir = Join-Path $out 'diagnostic_linear'
New-Item -ItemType Directory -Path $recordingRoot,$diagnosticDir | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)
$bandNumber = if($Band -eq 'SWIR'){0}else{2}
$remoteTag = "p14_first_valid_video_${Name}"

function Use-BoardAuth([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p14-first-valid-video'
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
    @($result.Lines)
}
function Copy-BoardFile([string]$RemotePath,[string]$LocalPath) {
    $result=Use-BoardAuth {
        $saved=$ErrorActionPreference; $ErrorActionPreference='Continue'
        $lines=& scp.exe @(Ssh-Args) "${BoardUser}@${BoardHost}:$RemotePath" $LocalPath 2>&1
        $code=$LASTEXITCODE; $ErrorActionPreference=$saved
        [pscustomobject]@{Lines=@($lines);Code=$code}
    }
    if($result.Code -ne 0){throw "Board copy failed ($($result.Code)): $RemotePath`n$($result.Lines -join "`n")"}
}
function Invoke-CapturedProcess(
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$StdoutPath,
    [string]$StderrPath) {
    $process=Start-Process -FilePath $FilePath -ArgumentList $Arguments -WindowStyle Hidden `
        -PassThru -Wait -RedirectStandardOutput $StdoutPath -RedirectStandardError $StderrPath
    $process.Refresh()
    [int]$process.ExitCode
}

$receiver=$null
$board=$null
$oldQt=$env:QT_FORCE_STDERR_LOGGING
$oldRecordingRoot=$env:P7RecordingRoot
$oldReceiverDump=$env:P6ReceiverUiDump
$oldResponsive=$env:P6ReceiverUiResponsiveCapture
$oldVideoPath=$env:P5DdsVideoPath
$oldVideoSamples=$env:P5DdsVideoSamples
$caseSucceeded=$false
try {
    $conflicts=@(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue)
    if($conflicts.Count -ne 0){throw "Local runtime conflict: $($conflicts.Id -join ',')"}
    $preflight=Invoke-Board "set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR >/dev/null; test -x logs/HwaSimIRP14FirstValidFixture.final5; sha256sum HwaSim_IR logs/HwaSimIRP14FirstValidFixture.final5 Config/HwaSimIRRuntime.ini Config/deployment_manifest.sha256 Config/GameVFX/sprite.frag"
    [IO.File]::WriteAllLines((Join-Path $out 'board_preflight.log'),@($preflight|ForEach-Object{[string]$_}),$utf8)

    $env:QT_FORCE_STDERR_LOGGING='1'
    $env:P7RecordingRoot=$recordingRoot
    $env:P6ReceiverUiDump=Join-Path $out 'receiver_ui.png'
    $env:P6ReceiverUiResponsiveCapture='1'
    $env:P5DdsVideoPath=Join-Path $out 'received.h264'
    $env:P5DdsVideoSamples='300'
    $receiver=Start-Process -FilePath $receiverExe -WorkingDirectory $receiverDir -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $out 'receiver.out.log') `
        -RedirectStandardError (Join-Path $out 'receiver.err.log')
    $null=$receiver.Handle
    Start-Sleep -Seconds 2

    $remote="/userdata/HwaSimIR/logs/p14_first_valid_board_run.sh /userdata/HwaSimIR /userdata/HwaSimIR/logs/HwaSimIRP14FirstValidFixture.final5 $bandNumber $remoteTag boundary_demo 180"
    $board=Use-BoardAuth {
        Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru `
            -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
            -RedirectStandardOutput (Join-Path $out 'board_runner.out.log') `
            -RedirectStandardError (Join-Path $out 'board_runner.err.log')
    }
    $null=$board.Handle
    if(-not $board.WaitForExit(120000)){throw 'Board boundary fixture did not complete within 120 seconds'}
    $board.Refresh()
    if($board.ExitCode -ne 0){throw "Board boundary fixture exit code $($board.ExitCode)"}
    Start-Sleep -Seconds 3
    if(-not $receiver.HasExited){
        [void]$receiver.CloseMainWindow()
        if(-not $receiver.WaitForExit(30000)){throw 'Receiver did not finish MP4 flush within 30 seconds'}
    }
    $receiver.Refresh()

    Copy-BoardFile "/userdata/HwaSimIR/logs/${remoteTag}_board.log" (Join-Path $out 'board.log')
    Copy-BoardFile "/userdata/HwaSimIR/logs/${remoteTag}_sender.log" (Join-Path $out 'fixture_sender.log')
    foreach($suffix in @('seq1.pfm','seq1_rgb8.png','seq1_stats.pfm')) {
        Copy-BoardFile "/userdata/HwaSimIR/logs/${remoteTag}_linear_${suffix}" (Join-Path $diagnosticDir "${remoteTag}_linear_${suffix}")
    }

    $senderText=Get-Content -LiteralPath (Join-Path $out 'fixture_sender.log') -Raw
    $boardText=Get-Content -LiteralPath (Join-Path $out 'board.log') -Raw
    $receiverText=(Get-Content -LiteralPath (Join-Path $out 'receiver.out.log') -Raw)+"`n"+(Get-Content -LiteralPath (Join-Path $out 'receiver.err.log') -Raw)
    foreach($requiredPattern in @(
        'event=no_packet_window',
        'event=all_zero_placeholder',
        'policy=ForceVisibleForDemo sourceViewValid=0 effectiveViewValid=1 productionFilterChanged=0',
        '[P14FixtureTail] phase=end frames=180',
        '[P14FixtureSummary] scenarios=1 fileReads=0 inputGenerated=1 strictIdentity=1001/2 resolution=800x800')) {
        if(-not $senderText.Contains($requiredPattern)){throw "Fixture sender evidence missing: $requiredPattern"}
    }
    if(([regex]::Matches($boardText,'\[FormalReferenceCommit\]')).Count -ne 1){throw 'Boundary demo did not commit exactly one formal reference'}
    $commitPos=$boardText.IndexOf('[FormalReferenceCommit]')
    $beforeCommit=$boardText.Substring(0,$commitPos)
    if($beforeCommit -match '\[DdsFrameProducts\]|\[H264EncodeSuccess\]'){throw 'Boundary demo emitted a product before the formal reference commit'}
    if($boardText -notmatch '\[RealtimeValidity\].*accepted=0.*reason=legacy_all_zero_placeholder.*outputFramesCreated=0'){throw 'Boundary demo placeholder rejection missing'}
    if($receiverText -notmatch '\[VideoStatus\].*applied=1.*width=800 height=800 fps=60'){throw 'Receiver did not auto-apply 800x800@60 VideoStatus'}
    if($receiverText -match '"decodeErrors"\s*:\s*"?[1-9][0-9]*'){throw 'Receiver reported decode errors'}

    $mp4=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter output.mp4)
    $status=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter recording_status.json)
    $index=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter frame_index.jsonl)
    foreach($set in @($mp4,$status,$index)){if($set.Count -ne 1){throw 'Expected exactly one finalized recording round'}}
    $recordingStatus=Get-Content -LiteralPath $status[0].FullName -Raw|ConvertFrom-Json
    if(-not [bool]$recordingStatus.muxerFinalized -or [bool]$recordingStatus.fileError){throw 'Recorder did not finalize MP4 cleanly'}
    $probePath=Join-Path $out 'ffprobe.json'
    $probeError=Join-Path $out 'ffprobe.err.log'
    $probeExit=Invoke-CapturedProcess $ffprobe @('-v','error','-show_streams','-show_format','-of','json',$mp4[0].FullName) $probePath $probeError
    if($probeExit -ne 0){throw "ffprobe failed; see $probeError"}
    $decodeOutput=Join-Path $out 'full_decode.out.log'
    $decodeError=Join-Path $out 'full_decode.err.log'
    $decodeExit=Invoke-CapturedProcess $ffmpeg @('-v','error','-i',$mp4[0].FullName,'-f','null','NUL') $decodeOutput $decodeError
    if($decodeExit -ne 0){throw 'Full MP4 decode failed'}
    $receivedPng=Join-Path $out 'received_keyframe.png'
    $frameOutput=Join-Path $out 'keyframe_decode.out.log'
    $frameError=Join-Path $out 'keyframe_decode.err.log'
    $frameExit=Invoke-CapturedProcess $ffmpeg @('-y','-ss','1.0','-i',$mp4[0].FullName,'-frames:v','1',$receivedPng) $frameOutput $frameError
    if($frameExit -ne 0 -or -not (Test-Path -LiteralPath $receivedPng)){throw "Keyframe decode failed; see $frameError"}

    $timeline=[ordered]@{
        schema='HwaSimIR.P14.FirstValidBoundaryTimeline.1';band=$Band;protocolBand=$bandNumber
        timeBases=[ordered]@{fixtureEvents='steady waits plus generated source time';sourceTime='milliseconds in generated realtime sample';videoPts='receiver frame_index.jsonl'}
        events=@(
            [ordered]@{event='START_ready';productAllowed=$false},
            [ordered]@{event='no_packet_window';elapsedMs=1400;productAllowed=$false},
            [ordered]@{event='all_zero_placeholder';accepted=$false;productAllowed=$false},
            [ordered]@{event='first_valid_realtime';sourceTimeMs=8000;formalReferenceCommit=$true;inputViewValid=0;effectiveViewValid=1;policy='ForceVisibleForDemo'},
            [ordered]@{event='tail';frames=180;cadenceHz=60},
            [ordered]@{event='STOP';ddsDrainRequired=$true;recordingFlushRequired=$true}
        )
    }
    [IO.File]::WriteAllText((Join-Path $out 'event_timeline.json'),($timeline|ConvertTo-Json -Depth 8)+"`n",$utf8)

    $result=[ordered]@{
        schema='HwaSimIR.P14.FirstValidVideoCase.1';result='PASS';name=$Name;band=$Band;protocolBand=$bandNumber
        scenario='boundary_demo';source='generated_no_file';readsOriginal1Txt=$false;productionFilterChanged=$false
        displayPolicy='ForceVisibleForDemo';identity=[ordered]@{platID=1001;sensorID=2;resolution='800x800'}
        finalElfSha256='80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c'
        fixtureSha256=(Get-FileHash -LiteralPath (Join-Path $repo 'logs\p14\build\first_valid_fixture_final_v2\HwaSimIRP14FirstValidFixture.aarch64') -Algorithm SHA256).Hash.ToLowerInvariant()
        products=[ordered]@{
            mp4=$mp4[0].FullName;mp4Bytes=$mp4[0].Length;mp4Sha256=(Get-FileHash -LiteralPath $mp4[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            receivedH264=(Join-Path $out 'received.h264');frameIndex=$index[0].FullName;recordingStatus=$status[0].FullName
            receivedKeyframe=$receivedPng;rawPfm=(Join-Path $diagnosticDir "${remoteTag}_linear_seq1.pfm")
            fixedMappingPng=(Join-Path $diagnosticDir "${remoteTag}_linear_seq1_rgb8.png")
        }
        completeProducts=[int]$recordingStatus.completeProducts;muxerFinalized=[bool]$recordingStatus.muxerFinalized
        ffprobeExitCode=$probeExit;fullDecodeExitCode=$decodeExit
        fullDecodeDiagnostics=(Join-Path $out 'full_decode.err.log')
        calibrationStatus='NOT_VERIFIED_CALIBRATION'
    }
    [IO.File]::WriteAllText((Join-Path $out 'case_result.json'),($result|ConvertTo-Json -Depth 8)+"`n",$utf8)
    $caseSucceeded=$true
    Write-Output "[P14 FirstValidVideo] result=PASS band=$Band products=$($recordingStatus.completeProducts) mp4=$($mp4[0].FullName)"
}
finally {
    foreach($process in @($receiver)){
        if($process){$process.Refresh();if(-not $process.HasExited){try{[void]$process.CloseMainWindow()}catch{};if(-not $process.WaitForExit(10000)){Stop-Process -Id $process.Id -Force}}}
    }
    if($board){$board.Refresh();if(-not $board.HasExited){try{Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true"|Out-Null}catch{};if(-not $board.WaitForExit(15000)){Stop-Process -Id $board.Id -Force}}}
    $env:QT_FORCE_STDERR_LOGGING=$oldQt
    $env:P7RecordingRoot=$oldRecordingRoot
    $env:P6ReceiverUiDump=$oldReceiverDump
    $env:P6ReceiverUiResponsiveCapture=$oldResponsive
    $env:P5DdsVideoPath=$oldVideoPath
    $env:P5DdsVideoSamples=$oldVideoSamples
    if(-not $caseSucceeded){
        [IO.File]::WriteAllText((Join-Path $out 'FAILED.txt'),"P14 first-valid video case failed; logs retained.`n",$utf8)
    }
}
