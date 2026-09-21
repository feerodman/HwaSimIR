[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('SWIR','MWIR')]
    [string]$Band,
    [ValidateSet('Clear','Cloudy','Rain','Snow')]
    [string]$Weather = 'Clear',
    [ValidateSet('Original','P13Performance300s','P14GroundWeather')]
    [string]$InputMode = 'Original',
    [ValidateSet('','materials','nozzle')]
    [string]$P5Scene = '',
    [ValidateSet('oblique','end','side','near','far')]
    [string]$P5View = 'end',
    [ValidateSet('A','B')]
    [string]$P5MaterialCase = 'A',
    [ValidateSet('On','Off')]
    [string]$P5SyntheticHeatSource = 'On',
    [ValidateSet('0x22','0x55','0x66')]
    [string]$TargetTypeCode = '0x22',
    [ValidateSet(0,1,2)]
    [int]$CloudMaxVisibleVolumes = 0,
    [switch]$RenderPerfProbe,
    [string]$LinearDiagnosticSeqs = '',
    [switch]$EnableAgcDiagnostic,
    [ValidateRange(0.1,100.0)]
    [double]$VisibilityKm = 6.0,
    [ValidateRange(0.0,100.0)]
    [double]$RelativeHumidityPercent = 85.0,
    [ValidateRange(0.0,23.999999)]
    [double]$UtcHour = 3.5,
    [int]$DurationGuardSec = 100,
    [string]$Name = '',
    [string]$OutputRoot = 'logs\p14\runs',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$senderDir = Join-Path $repo 'build-DataDrivenTestQT-codex-mingw73_64-Release\release'
$receiverDir = Join-Path $repo 'HwaSim_IR_VideoDisplay\x64\Release'
$senderExe = Join-Path $senderDir 'DataDrivenTestQT.exe'
$receiverExe = Join-Path $receiverDir 'HwaSim_IR_VideoDisplay.exe'
$senderConfig = Join-Path $senderDir 'NetworkConfig.ini'
$receiverConfig = Join-Path $receiverDir 'NetworkConfig.ini'
$inputName = switch($InputMode) {
    'Original' {'1.txt'}
    'P13Performance300s' {'p13_performance_highalt_300s.txt'}
    'P14GroundWeather' {'p14_ground_truck_weather_30s.txt'}
}
$runtimeInput = Join-Path $senderDir $inputName
$sourceInput = Join-Path $repo ("DataDrivenTestQT\"+$inputName)
$inputManifest = if($InputMode -eq 'Original'){''}else{"$sourceInput.json"}
$expectedRows = switch($InputMode) {'Original' {4318} 'P13Performance300s' {18000} 'P14GroundWeather' {1800}}
$minimumDurationGuardSec = switch($InputMode) {'Original' {80} 'P13Performance300s' {320} 'P14GroundWeather' {45}}
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
if (-not $BoardPassword) { $BoardPassword = '123' }
$requiredFiles = @($senderExe,$receiverExe,$senderConfig,$receiverConfig,$runtimeInput,$sourceInput,$askPass)
if ($inputManifest) { $requiredFiles += $inputManifest }
foreach ($required in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing P13 runtime input: $required" }
}
if ($DurationGuardSec -lt $minimumDurationGuardSec) { throw "DurationGuardSec must be at least $minimumDurationGuardSec seconds for all $expectedRows rows at 60 Hz" }
if ($LinearDiagnosticSeqs -and $LinearDiagnosticSeqs -notmatch '^\d+(,\d+)*$') {
    throw 'LinearDiagnosticSeqs must be a comma-separated list of positive source sequence numbers'
}
$linearDiagnosticSequenceList = @()
if ($LinearDiagnosticSeqs) {
    $linearDiagnosticSequenceList = @($LinearDiagnosticSeqs.Split(',') | ForEach-Object { [int]$_ })
    if (@($linearDiagnosticSequenceList | Where-Object { $_ -lt 1 -or $_ -gt $expectedRows }).Count -ne 0) {
        throw "LinearDiagnosticSeqs must stay within 1..$expectedRows"
    }
    if (@($linearDiagnosticSequenceList | Sort-Object -Unique).Count -ne $linearDiagnosticSequenceList.Count) {
        throw 'LinearDiagnosticSeqs must not contain duplicates'
    }
}
if ($EnableAgcDiagnostic -and -not $LinearDiagnosticSeqs) {
    throw 'EnableAgcDiagnostic requires LinearDiagnosticSeqs so AGC evidence is explicitly sampled'
}

$bandNumber = if ($Band -eq 'SWIR') { 0 } else { 2 }
$envSky = switch ($Weather) { 'Clear' {0} 'Cloudy' {1} 'Rain' {2} 'Snow' {3} }
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $Name) { $Name = "original_1_${Band}_${Weather}_$stamp" }
if ($Name -notmatch '^[A-Za-z0-9_.-]+$') { throw "Unsafe case name: $Name" }
$root = if ([IO.Path]::IsPathRooted($OutputRoot)) { $OutputRoot } else { Join-Path $repo $OutputRoot }
$out = Join-Path $root $Name
if (Test-Path -LiteralPath $out) { throw "Refusing to overwrite evidence: $out" }
New-Item -ItemType Directory -Path $out | Out-Null
$out = (Resolve-Path -LiteralPath $out).Path
$recordingRoot = Join-Path $out 'recording'
New-Item -ItemType Directory -Path $recordingRoot | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)

function Use-BoardAuth([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p13-original-dds'
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
function Set-IniValue([string]$Text,[string]$Key,[string]$Value) {
    $pattern = '(?im)^' + [regex]::Escape($Key) + '\s*=\s*[^\r\n]*$'
    if ($Text -notmatch $pattern) { throw "Effective sender INI lacks key: $Key" }
    [regex]::Replace($Text,$pattern,"$Key=$Value")
}

$senderConfigBackup = [IO.File]::ReadAllBytes($senderConfig)
$inputHash = (Get-FileHash -LiteralPath $runtimeInput -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceInputHash = (Get-FileHash -LiteralPath $sourceInput -Algorithm SHA256).Hash.ToLowerInvariant()
if ($sourceInputHash -ne $inputHash) { throw 'Source/runtime input identity mismatch; refusing replay' }
if($InputMode -eq 'Original') {
    if($inputHash -ne 'f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901') {
        throw 'Original 1.txt identity mismatch; refusing replay'
    }
} elseif($InputMode -eq 'P13Performance300s') {
    $fixtureManifest=Get-Content -LiteralPath $inputManifest -Raw|ConvertFrom-Json
    if($fixtureManifest.purpose -ne 'performance_only_300s_complex_weather' -or
       -not [bool]$fixtureManifest.boundaries.notOriginalReplay -or
       [string]$fixtureManifest.identity.sha256 -ne $inputHash -or
       [int]$fixtureManifest.identity.rows -ne $expectedRows) {
        throw 'P13 performance fixture manifest/identity mismatch'
    }
} else {
    $fixtureManifest=Get-Content -LiteralPath $inputManifest -Raw|ConvertFrom-Json
    $query=$fixtureManifest.productionQueryEvidence
    if($fixtureManifest.purpose -ne 'p14_ground_truck_mixed_weather_30s' -or
       -not [bool]$fixtureManifest.boundaries.notOriginalReplay -or
       [bool]$fixtureManifest.boundaries.mayReplaceOriginal1Txt -or
       [bool]$fixtureManifest.provenance.readsOriginal1Txt -or
       [bool]$fixtureManifest.provenance.sourceInputBusinessDependency -or
       [string]$fixtureManifest.identity.sha256 -ne $inputHash -or
       [int]$fixtureManifest.identity.rows -ne $expectedRows -or
       [string]$query.result -ne 'PASS' -or [int]$query.productionQueryRows -ne 14400 -or
       [int]$query.productionQueryValid -ne 14400 -or [int]$query.productionQueryFailures -ne 0 -or
       [string]$fixtureManifest.boundaries.calibrationStatus -ne 'NOT_VERIFIED_CALIBRATION') {
        throw 'P14 ground-weather fixture manifest/identity/query evidence mismatch'
    }
}

$sender = $null
$receiver = $null
$board = $null
$oldQt = $env:QT_FORCE_STDERR_LOGGING
$oldRecordingRoot = $env:P7RecordingRoot
$oldReceiverDump = $env:P6ReceiverUiDump
$oldResponsive = $env:P6ReceiverUiResponsiveCapture
$oldSenderDump = $env:P7SenderUiDump
$oldVideoPath = $env:P5DdsVideoPath
$oldVideoSamples = $env:P5DdsVideoSamples
$oldPath = [Environment]::GetEnvironmentVariable('Path','Process')
$caseSucceeded = $false

try {
    $text=[IO.File]::ReadAllText($senderConfig)
    $text=Set-IniValue $text 'InputFile' $inputName
    $text=Set-IniValue $text 'TargetType' $TargetTypeCode
    $text=Set-IniValue $text 'envSky' ([string]$envSky)
    $text=Set-IniValue $text 'envVisibility' ([string]($VisibilityKm*1000.0))
    $text=Set-IniValue $text 'envHumidity' ([string]$RelativeHumidityPercent)
    $text=Set-IniValue $text 'UtcHour' ([string]$UtcHour)
    $text=Set-IniValue $text 'trackerSensorBand' ([string]$bandNumber)
    $text=Set-IniValue $text 'trackerSensorWidth' '800'
    $text=Set-IniValue $text 'trackerSensorHeight' '800'
    $text=Set-IniValue $text 'saveMP4En' '1'
    $text=Set-IniValue $text 'h264En' '1'
    [IO.File]::WriteAllText($senderConfig,$text,$utf8)

    Copy-Item -LiteralPath $senderConfig -Destination (Join-Path $out 'DataDrivenTestQT.NetworkConfig.ini')
    Copy-Item -LiteralPath $receiverConfig -Destination (Join-Path $out 'VideoDisplay.NetworkConfig.ini')
    $preflight=Invoke-Board "set -eu; cd /userdata/HwaSimIR; ! pgrep -x HwaSim_IR >/dev/null; test -x HwaSim_IR; echo ElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo BuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo RuntimeConfigSha256=`$(sha256sum Config/HwaSimIRRuntime.ini|awk '{print `$1}'); echo ConfigManifestSha256=`$(sha256sum Config/deployment_manifest.sha256|awk '{print `$1}'); echo FormalLutSha256=`$(sha256sum Config/Atmosphere/MODTRAN/processed/band_lut_si.csv|awk '{print `$1}'); echo CoverageManifestSha256=`$(sha256sum Config/Atmosphere/MODTRAN/processed/p14_coverage_manifest.json|awk '{print `$1}')"
    [IO.File]::WriteAllLines((Join-Path $out 'board_preflight.log'),@($preflight|ForEach-Object{[string]$_}),$utf8)

    $plan=[ordered]@{
        schema='hwasimir.p14.dds-case.v2'; name=$Name; band=$Band; protocolBand=$bandNumber
        weather=$Weather; envSky=$envSky; inputMode=$InputMode; input=("DataDrivenTestQT/"+$inputName); inputSha256=$inputHash
        environment=[ordered]@{visibilityKm=$VisibilityKm;relativeHumidityPercent=$RelativeHumidityPercent;utcDate='2026-09-06';utcHour=$UtcHour}
        targetTypeProtocolCode=$TargetTypeCode
        inputRows=$expectedRows; resolution='800x800'; materialView=0; ddsOnly=$true
        inputBoundary=switch($InputMode) {
            'Original' {'immutable_original_replay'}
            'P13Performance300s' {'performance_only_not_original_no_physical_claim'}
            'P14GroundWeather' {'independent_synthetic_civil_ground_fixture_not_original_no_calibration_claim'}
        }
        controlledFixture=if($P5Scene){[ordered]@{scene=$P5Scene;view=$P5View;materialCase=$P5MaterialCase;syntheticHeatSource=$P5SyntheticHeatSource;values='artificial_not_measurement'}}else{$null}
        cloudMaxVisibleVolumes=if($CloudMaxVisibleVolumes -gt 0){$CloudMaxVisibleVolumes}else{'runtime_ini'}
        renderPerfProbe=[bool]$RenderPerfProbe
        diagnosticCapture=if($LinearDiagnosticSeqs){[ordered]@{
            sourceSeqs=@($linearDiagnosticSequenceList); rawDomain='RGBA16F_SI_to_PFM'
            mapping=if($EnableAgcDiagnostic){'AGC'}else{'fixed'}
            performanceEvidence=$false; persistentConfigModified=$false
        }}else{$null}
        identity=[ordered]@{platID=1001;sensorID=2;videoStatusAutoDiscovery=$true}
        durationGuardSec=$DurationGuardSec; startedUtc=[DateTime]::UtcNow.ToString('o')
        senderExeSha256=(Get-FileHash -LiteralPath $senderExe -Algorithm SHA256).Hash.ToLowerInvariant()
        receiverExeSha256=(Get-FileHash -LiteralPath $receiverExe -Algorithm SHA256).Hash.ToLowerInvariant()
        senderConfigSha256=(Get-FileHash -LiteralPath $senderConfig -Algorithm SHA256).Hash.ToLowerInvariant()
        receiverConfigSha256=(Get-FileHash -LiteralPath $receiverConfig -Algorithm SHA256).Hash.ToLowerInvariant()
        boardPreflight=@($preflight)
    }
    [IO.File]::WriteAllText((Join-Path $out 'case_plan.json'),($plan|ConvertTo-Json -Depth 8)+"`n",$utf8)

    $conflicts=@(Get-Process -Name DataDrivenTestQT,HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue)
    if($conflicts.Count -ne 0){throw "Local runtime conflict: $($conflicts.Id -join ',')"}

    [Environment]::SetEnvironmentVariable('PATH',$null,'Process')
    [Environment]::SetEnvironmentVariable('Path',$oldPath,'Process')
    $env:QT_FORCE_STDERR_LOGGING='1'
    $env:P7RecordingRoot=$recordingRoot
    $env:P6ReceiverUiDump=Join-Path $out 'receiver_normal_material.png'
    $env:P6ReceiverUiResponsiveCapture='1'
    $env:P7SenderUiDump=Join-Path $out ($(if($InputMode -eq 'Original'){'sender_original_1.png'}else{'sender_fixture.png'}))
    $env:P5DdsVideoPath=Join-Path $out 'received.h264'
    $env:P5DdsVideoSamples=[string]($expectedRows+100)

    $receiver=Start-Process -FilePath $receiverExe -WorkingDirectory $receiverDir -WindowStyle Normal -PassThru `
        -RedirectStandardOutput (Join-Path $out 'receiver.out.log') `
        -RedirectStandardError (Join-Path $out 'receiver.err.log')
    $null=$receiver.Handle
    Start-Sleep -Seconds 2

    $remoteTimeout=$DurationGuardSec+90
    $fixtureEnv=if($P5Scene){"P5Scene=$P5Scene P5View=$P5View P5MaterialCase=$P5MaterialCase P5SyntheticHeatSource=$P5SyntheticHeatSource"}else{''}
    $cloudEnv=if($CloudMaxVisibleVolumes -gt 0){"Stage7VolumetricCloudMaxVisibleVolumes=$CloudMaxVisibleVolumes"}else{''}
    $perfProbeEnv=if($RenderPerfProbe){'RenderPerfProbe=1'}else{''}
    $remoteDiagnosticBase="/userdata/HwaSimIR/logs/p14_${Name}_linear"
    $linearDiagnosticEnv=if($LinearDiagnosticSeqs){"LinearDiagnosticPath=$remoteDiagnosticBase LinearDiagnosticSeqs=$LinearDiagnosticSeqs"}else{''}
    $agcDiagnosticEnv=if($EnableAgcDiagnostic){'EnableAGC=true Stage6DiagnosticsEnable=true AGCDebugLog=true'}else{''}
    $remote="cd /userdata/HwaSimIR && timeout -s TERM -k 15s ${remoteTimeout}s env RenderPresentationMode=HeadlessOffscreen P5MaterialView=0 $fixtureEnv $cloudEnv $perfProbeEnv $linearDiagnosticEnv $agcDiagnosticEnv HwaSimIRExitOnStop=true HwaSimIRLocalRecordingEnable=false ./run_precise.sh"
    $board=Use-BoardAuth {
        Start-Process -FilePath 'ssh.exe' -WindowStyle Hidden -PassThru `
            -ArgumentList (@(Ssh-Args)+@("$BoardUser@$BoardHost",$remote)) `
            -RedirectStandardOutput (Join-Path $out 'board.log') `
            -RedirectStandardError (Join-Path $out 'board.ssh.err.log')
    }
    $null=$board.Handle
    Start-Sleep -Seconds 4

    $sender=Start-Process -FilePath $senderExe -WorkingDirectory $senderDir -WindowStyle Normal -PassThru `
        -ArgumentList @("--duration-sec=$DurationGuardSec") `
        -RedirectStandardOutput (Join-Path $out 'sender.out.log') `
        -RedirectStandardError (Join-Path $out 'sender.err.log')
    $null=$sender.Handle
    if(-not $sender.WaitForExit(($DurationGuardSec+75)*1000)){throw 'Sender did not reach EOF/STOP and complete DDS drain'}
    $sender.Refresh()
    if($sender.ExitCode -ne 0){throw "Sender exit code $($sender.ExitCode)"}

    if(-not $board.WaitForExit(60000)){throw 'Board did not complete STOP drain within 60 seconds after sender exit'}
    $board.Refresh()
    if($board.ExitCode -notin @(0,124,143)){throw "Board launcher exit code $($board.ExitCode)"}

    if($LinearDiagnosticSeqs) {
        $diagnosticDir=Join-Path $out 'diagnostic_linear'
        New-Item -ItemType Directory -Path $diagnosticDir | Out-Null
        foreach($seq in $linearDiagnosticSequenceList) {
            $remotePfm="${remoteDiagnosticBase}_seq${seq}.pfm"
            $remoteRgb="${remoteDiagnosticBase}_seq${seq}_rgb8.png"
            $requiredRemote=@($remotePfm,$remoteRgb)
            if($EnableAgcDiagnostic){$requiredRemote+="${remoteDiagnosticBase}_seq${seq}_stats.pfm"}
            $null=Invoke-Board ("set -eu; " + (($requiredRemote | ForEach-Object { "test -s '$_'" }) -join '; '))
            foreach($remoteFile in $requiredRemote) {
                Copy-BoardFile $remoteFile (Join-Path $diagnosticDir ([IO.Path]::GetFileName($remoteFile)))
            }
        }
    }

    Start-Sleep -Seconds 3
    if(-not $receiver.HasExited){
        [void]$receiver.CloseMainWindow()
        if(-not $receiver.WaitForExit(30000)){throw 'Receiver did not finish MP4 flush within 30 seconds'}
    }
    $receiver.Refresh()

    $senderText=(Get-Content -LiteralPath (Join-Path $out 'sender.out.log') -Raw)+"`n"+(Get-Content -LiteralPath (Join-Path $out 'sender.err.log') -Raw)
    $boardText=Get-Content -LiteralPath (Join-Path $out 'board.log') -Raw
    $receiverText=(Get-Content -LiteralPath (Join-Path $out 'receiver.out.log') -Raw)+"`n"+(Get-Content -LiteralPath (Join-Path $out 'receiver.err.log') -Raw)
    if($senderText -notmatch ('\[StimInputParse\] result=ACCEPTED.*rows='+$expectedRows+'.*malformedRows=0 partialReplay=0')){throw "Strict parser did not accept exactly $expectedRows complete rows"}
    if($senderText -notmatch ('\[StimFinal\].*successfulRealtimeWrites='+$expectedRows)){throw "Sender did not write all $expectedRows accepted rows"}
    $rolePattern=switch($InputMode) {
        'Original' {'immutable_original_replay'}
        'P13Performance300s' {'performance_only_300s_complex_weather'}
        'P14GroundWeather' {'p14_ground_truck_mixed_weather_30s'}
    }
    if($senderText -notmatch ('\[StimAtmosphereCoverage\] result=ACCEPTED.*visibilityKm='+[regex]::Escape(([string]$VisibilityKm))+'.*humidityPercent='+[regex]::Escape(([string]$RelativeHumidityPercent))+'.*inputRole='+$rolePattern)){
        throw 'Controller did not prove current-row production atmosphere queries for the requested environment/input role'
    }
    if($senderText -match '\[StimDrain\]\[ERROR\]|\[StimDDS\]\[ERROR\]|\[StimInitAck\]\[FATAL\]'){throw 'Sender reported DDS or drain failure'}
    if($senderText -notmatch '\[StimStopLifecycle\] phase=renderer_stop_status result=PASS' -or
       $senderText -notmatch '\[StimStopLifecycle\] phase=dds_ack_drain result=PASS'){throw 'STOP response and DDS acknowledgment drain are not both proven'}
    if($boardText -notmatch '\[P14 AtmosphereIdentity\] status=PASS'){throw 'Renderer did not bind the P14 atmosphere identity'}
    if($boardText -notmatch '\[M1 PhysicsConfig\] CompareOnly=0 EnableRuntime=1'){throw 'Renderer did not run formal M1 output'}
    if($P5Scene -and $boardText -notmatch ('\[P5GraphicsTest\] scene='+[regex]::Escape($P5Scene)+'.*materialView=0')){throw 'Controlled fixture did not run in normal material view 0'}
    $expectedHeatSourceFlag = if($P5SyntheticHeatSource -eq 'On'){'1'}else{'0'}
    if($P5Scene -and $boardText -notmatch ('\[P5GraphicsTest\] scene='+[regex]::Escape($P5Scene)+'.*syntheticHeatSourceEnabled='+$expectedHeatSourceFlag)){throw 'Controlled fixture heat-source state was not applied'}
    # Renderer worker and main-thread diagnostics may interleave at character
    # granularity on the shared PTY.  Bind the codec backend and geometry with
    # two independently emitted runtime facts instead of depending on one
    # vulnerable compound line; both remain mandatory.
    if($boardText -notmatch '\[H264EncodeSuccess\].*backend=mpp.*codec=h264_annexb'){
        throw 'Renderer did not prove RK MPP H.264 encoding success'
    }
    if($boardText -notmatch '\[VideoStatus\] published=1 running=1 codec=h264.*width=800 height=800 fps=60'){
        throw 'Renderer did not publish an 800x800@60 H.264 VideoStatus'
    }
    if($receiverText -notmatch '\[VideoStatus\].*applied=1.*width=800 height=800 fps=60'){throw 'Receiver did not auto-apply 800x800@60 VideoStatus'}
    if($receiverText -match '"decodeErrors"\s*:\s*"?[1-9][0-9]*'){throw 'Receiver reported H.264 decode errors'}

    $mp4=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter output.mp4)
    $status=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter recording_status.json)
    $index=@(Get-ChildItem -LiteralPath $recordingRoot -Recurse -File -Filter frame_index.jsonl)
    foreach($set in @($mp4,$status,$index)){if($set.Count -ne 1){throw 'Expected exactly one finalized recording round'}}
    if($mp4[0].Length -le 0){throw 'Final MP4 is empty'}
    $recordingStatus=Get-Content -LiteralPath $status[0].FullName -Raw|ConvertFrom-Json
    if(-not [bool]$recordingStatus.muxerFinalized -or [bool]$recordingStatus.fileError){throw 'Recorder did not finalize MP4 cleanly'}

    $plan.finishedUtc=[DateTime]::UtcNow.ToString('o')
    $plan.result='PASS'
    $plan.senderExitCode=[int]$sender.ExitCode
    $plan.boardExitCode=[int]$board.ExitCode
    $plan.products=[ordered]@{
        mp4=$mp4[0].FullName;mp4Bytes=$mp4[0].Length
        mp4Sha256=(Get-FileHash -LiteralPath $mp4[0].FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        receivedH264=Join-Path $out 'received.h264'
        recordingStatus=$status[0].FullName;frameIndex=$index[0].FullName
    }
    [IO.File]::WriteAllText((Join-Path $out 'case_result.json'),($plan|ConvertTo-Json -Depth 10)+"`n",$utf8)
    $caseSucceeded=$true
    Write-Output "[P14 DDS] result=PASS name=$Name band=$Band weather=$Weather inputMode=$InputMode inputRows=$expectedRows mp4=$($mp4[0].FullName) output=$out"
}
finally {
    [IO.File]::WriteAllBytes($senderConfig,$senderConfigBackup)
    foreach($process in @($sender,$receiver)){
        if($process){$process.Refresh();if(-not $process.HasExited){try{[void]$process.CloseMainWindow()}catch{};if(-not $process.WaitForExit(10000)){Stop-Process -Id $process.Id -Force}}}
    }
    if($board){
        $board.Refresh()
        if(-not $board.HasExited){
            try{Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true"|Out-Null}catch{}
            if(-not $board.WaitForExit(15000)){Stop-Process -Id $board.Id -Force}
        }
    }
    $env:QT_FORCE_STDERR_LOGGING=$oldQt
    $env:P7RecordingRoot=$oldRecordingRoot
    $env:P6ReceiverUiDump=$oldReceiverDump
    $env:P6ReceiverUiResponsiveCapture=$oldResponsive
    $env:P7SenderUiDump=$oldSenderDump
    $env:P5DdsVideoPath=$oldVideoPath
    $env:P5DdsVideoSamples=$oldVideoSamples
    [Environment]::SetEnvironmentVariable('Path',$oldPath,'Process')
    if(-not $caseSucceeded){
        [IO.File]::WriteAllText((Join-Path $out 'FAILED.txt'),"P14 case did not satisfy all acceptance checks.`n",$utf8)
    }
}
