[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Run')]
    [string]$Mode = 'Plan',
    [string]$RepoRoot = '',
    [string]$DeploymentReceipt = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$RollbackSuffix = '.before_p11-20260916-053343',
    [string]$RollbackElfSha256 = '840b649b7ed21f3b4a8b16a83bba25aee17319efa2932f8ff6705d14d0fe00ba',
    [string]$RollbackConfigManifestSha256 = '38f6a89d49826bc00d1555c391a27538933e1c15d5dc03656a117cd350b7e9ca',
    [ValidateSet(0, 1, 2, 3)]
    [int]$RollbackProtocolBand = 1,
    [ValidateSet(0, 1, 2, 3)]
    [int]$RestoredProtocolBand = 0,
    [int]$DurationSec = 15,
    [int]$LoopReadyTimeoutSec = 240,
    [string]$OutputDirectory = '',
    [switch]$ConfirmRollbackExercise
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ($BoardRoot -ne '/userdata/HwaSimIR') { throw 'Rollback exercise is restricted to /userdata/HwaSimIR' }
if ($RollbackSuffix -notmatch '^\.before_p11-[0-9]{8}-[0-9]{6}$') { throw 'RollbackSuffix must identify an explicit P11 pre-deployment snapshot' }
if ($RollbackElfSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'RollbackElfSha256 must be a SHA-256 value' }
if ($RollbackConfigManifestSha256 -notmatch '^[0-9a-fA-F]{64}$') { throw 'RollbackConfigManifestSha256 must be a SHA-256 value' }
$auditedP10Suffix = '.before_p11-20260916-053343'
$auditedP10ElfSha256 = '840b649b7ed21f3b4a8b16a83bba25aee17319efa2932f8ff6705d14d0fe00ba'
$auditedP10ConfigManifestSha256 = '38f6a89d49826bc00d1555c391a27538933e1c15d5dc03656a117cd350b7e9ca'
if ($RollbackSuffix -cne $auditedP10Suffix -or
    $RollbackElfSha256.ToLowerInvariant() -cne $auditedP10ElfSha256 -or
    $RollbackConfigManifestSha256.ToLowerInvariant() -cne $auditedP10ConfigManifestSha256) {
    throw 'Rollback target is not the audited pre-P11 P10 snapshot'
}
if ($RollbackProtocolBand -ne 1) { throw 'Audited P10 rollback loop requires NIR protocol band 1' }
if ($RestoredProtocolBand -notin @(0, 1)) { throw 'Restored P11 rollback loop must declare SWIR band 0 or NIR band 1' }
if ($DurationSec -lt 8 -or $DurationSec -gt 60) { throw 'DurationSec must be 8..60' }
if ($LoopReadyTimeoutSec -lt 60 -or $LoopReadyTimeoutSec -gt 600) { throw 'LoopReadyTimeoutSec must be 60..600' }
$remoteScriptLocal = Join-Path $RepoRoot 'tools\p11_rk3588_rollback_exercise.sh'
$askPass = Join-Path $RepoRoot 'tools\p5_ssh_askpass.cmd'
foreach ($path in @($remoteScriptLocal, $askPass)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing rollback input: $path" }
}
$runId = 'p11rb-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot "logs\p11\rk3588\rollback-$runId" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8NoBom = New-Object Text.UTF8Encoding($false)

if ($Mode -eq 'Plan') {
    $plan = [ordered]@{
        schema = 'hwasimir.p11.rk3588.rollback-plan.v1'
        created_utc = [DateTime]::UtcNow.ToString('o')
        board_root = $BoardRoot
        duration_sec_each = $DurationSec
        validation_contract = 'hwasimir.p11.rk3588.rollback-loop.v1'
        rollback_snapshot = [ordered]@{
            suffix = $RollbackSuffix
            elf_sha256 = $RollbackElfSha256.ToLowerInvariant()
            config_manifest_sha256 = $RollbackConfigManifestSha256.ToLowerInvariant()
            protocol_band = $RollbackProtocolBand
        }
        restored_protocol_band = $RestoredProtocolBand
        sequence = @(
            'verify_active_and_backup', 'activate_backup',
            'render_send_decode_backup', 'restore_p11',
            'render_send_decode_p11', 'verify_both_snapshots'
        )
        remote_modified = $false
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'rollback_plan.json'), ($plan | ConvertTo-Json -Depth 5) + "`n", $utf8NoBom)
    Write-Host "[P11 RK3588 RollbackPlan] result=PASS remoteModified=0 output=$OutputDirectory"
    return
}
if (-not $ConfirmRollbackExercise) { throw 'Run mode requires -ConfirmRollbackExercise' }
if (-not $DeploymentReceipt -or -not (Test-Path -LiteralPath $DeploymentReceipt)) { throw 'DeploymentReceipt is required' }
$DeploymentReceipt = (Resolve-Path -LiteralPath $DeploymentReceipt).Path
$receipt = Get-Content -Raw -LiteralPath $DeploymentReceipt | ConvertFrom-Json
if ($receipt.schema -ne 'hwasimir.p11.rk3588.deployment-receipt.v1') { throw 'Unexpected deployment receipt schema' }
$stageId = [string]$receipt.stage_id
if ($stageId -notmatch '^p11-[0-9]{8}-[0-9]{6}$') { throw 'Unsafe deployment stage id' }
$activeElfSha256 = [string]$receipt.elf_sha256
$activeConfigManifestSha256 = [string]$receipt.config_manifest_sha256
if ($activeElfSha256 -notmatch '^[0-9a-fA-F]{64}$' -or $activeConfigManifestSha256 -notmatch '^[0-9a-fA-F]{64}$') {
    throw 'DeploymentReceipt is missing audited active ELF/config hashes'
}

$receiverExe = Join-Path $RepoRoot 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimExe = Join-Path $RepoRoot 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$windowsQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_PROTOCOL_QOS_WINDOWS_192.168.1.188.xml'
$cameraInput = Join-Path $RepoRoot 'tools\p11_inputs\civil_van_close_lookat.json'
foreach ($path in @($receiverExe, $stimExe, $windowsQos, $cameraInput)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing rollback-loop input: $path" }
}
$windowsQosIni = $windowsQos -replace '\\', '/'
$networkConfig = Join-Path $OutputDirectory 'NetworkConfig_rollback_dds.ini'
$networkConfigLines = @(
    '[Identity]', 'channel=precise', 'platID=1001', 'sensorID=2', '',
    '[DdsProtocol]', 'DomainId=150', "QosFile=$windowsQosIni",
    'TopicControl=HwaSimIR.Control', 'TopicInit=HwaSimIR.Init',
    'TopicRealtime=HwaSimIR.Realtime', 'TopicInitAck=HwaSimIR.InitAck',
    'TopicVideoStatus=HwaSimIR.VideoStatus'
)
[IO.File]::WriteAllText($networkConfig, ($networkConfigLines -join "`r`n") + "`r`n", $utf8NoBom)

$fatalPattern = '\[RunPreflight\]\[FATAL\]|\[StartupFatal\]|Assertion failed:|\[Stage6 RawAttachment\]\[ERROR\]|\[Stage6 [^\]]*\]\[ERROR\]|\[P6LinearCapture\]\[ERROR\]|hardwareGpu=0|llvmpipe|GL_INVALID_OPERATION|GL error 0x502|Could not bind framebuffer|raw_buffer_unavailable|missing_float_buffer'

function Get-ExitedProcessCode {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return $null }
    $Process.Refresh()
    if (-not $Process.HasExited) { return $null }
    $Process.WaitForExit()
    $Process.Refresh()
    try { return [int]$Process.ExitCode }
    catch { return $null }
}

function Read-SharedText {
    param([Parameter(Mandatory = $true)][string]$Path)
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $reader = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8, $true)
        try { return $reader.ReadToEnd() }
        finally { $reader.Dispose() }
    }
    finally { $stream.Dispose() }
}

function Wait-LocalToken {
    param(
        [string]$Path,
        [string]$Token,
        [System.Diagnostics.Process]$Owner,
        [int]$TimeoutSec
    )
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            $text = Read-SharedText -Path $Path
            if ($text.Contains($Token)) { return }
        }
        $Owner.Refresh()
        if ($Owner.HasExited) {
            $Owner.WaitForExit()
            throw "Rollback board owner exited before token '$Token'; exit=$($Owner.ExitCode)"
        }
        Start-Sleep -Milliseconds 500
    }
    throw "Timed out waiting for rollback token '$Token'"
}

function Stop-OwnedLocalProcess {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return }
    $Process.Refresh()
    if ($Process.HasExited) { $Process.WaitForExit(); return }
    $Process.CloseMainWindow() | Out-Null
    $Process.WaitForExit(10000) | Out-Null
    $Process.Refresh()
    if (-not $Process.HasExited) { Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue }
    $Process.WaitForExit()
}

function Get-RendererLoopEvidence {
    param([string]$Path, [string]$Release)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing renderer log for $Release`: $Path" }
    $text = Read-SharedText -Path $Path
    if ($text -match $fatalPattern) { throw "Fatal renderer pattern found for $Release" }
    foreach ($required in @(
        '\[RunPreflight\] result=PASS',
        '\[DeploymentVersion\] result=PASS',
        '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1'
    )) {
        if ($text -notmatch $required) { throw "Renderer evidence missing '$required' for $Release" }
    }
    $drains = [regex]::Matches($text, '\[OutputRoundDrain\] reason=stop [^\r\n]*targetFrames=(\d+)[^\r\n]*completedFrames=(\d+)')
    $senders = [regex]::Matches($text, '\[DdsVideoPerf\][^\r\n]*sentSamples=(\d+)[^\r\n]*sentBytes=(\d+)[^\r\n]*writeErrors=(\d+)[^\r\n]*droppedSamples=(\d+)')
    if ($drains.Count -eq 0 -or $senders.Count -eq 0) { throw "Renderer loop counts missing for $Release" }
    $drain = $drains[$drains.Count - 1]
    $sender = $senders[$senders.Count - 1]
    $target = [int64]$drain.Groups[1].Value
    $completed = [int64]$drain.Groups[2].Value
    $sent = [int64]$sender.Groups[1].Value
    $sentBytes = [int64]$sender.Groups[2].Value
    $writeErrors = [int64]$sender.Groups[3].Value
    $dropped = [int64]$sender.Groups[4].Value
    if ($target -le 0 -or $completed -ne $target -or $sent -ne $completed -or $sentBytes -le 0 -or $writeErrors -ne 0 -or $dropped -ne 0) {
        throw "Renderer loop count contract failed for $Release`: target=$target completed=$completed sent=$sent bytes=$sentBytes writeErrors=$writeErrors dropped=$dropped"
    }
    return [ordered]@{
        target_frames = $target
        completed_frames = $completed
        sent_samples = $sent
        sent_bytes = $sentBytes
        write_errors = $writeErrors
        dropped_samples = $dropped
    }
}

function Get-ReceiverLoopEvidence {
    param([string]$Path, [string]$Release)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Missing receiver log for $Release`: $Path" }
    $text = Read-SharedText -Path $Path
    $perf = [regex]::Matches($text, '\[DdsVideoReceiverPerf\] receivedSamples=(\d+) receivedBytes=(\d+) ddsErrors=(\d+)[^\r\n]*')
    $video = [regex]::Matches($text, '\[VideoPerf\][^\r\n]*decodeCodec=h264_annexb[^\r\n]*h264KeyFrameSeen=1[^\r\n]*h264DecodeErrors=(\d+)')
    $metrics = [regex]::Matches($text, '(?m)^\[RuntimeMetricsV2\] (?<json>\{[^\r\n]+\})\s*$')
    if ($perf.Count -eq 0 -or $video.Count -eq 0 -or $metrics.Count -eq 0) {
        throw "Receiver loop evidence missing for $Release"
    }
    $lastPerf = $perf[$perf.Count - 1]
    $received = [int64]$lastPerf.Groups[1].Value
    $receivedBytes = [int64]$lastPerf.Groups[2].Value
    $ddsErrors = [int64]$lastPerf.Groups[3].Value
    $decoded = 0L
    foreach ($match in $metrics) {
        $row = $match.Groups['json'].Value | ConvertFrom-Json
        $candidate = [int64]$row.decodedFrames
        if ($candidate -gt $decoded) { $decoded = $candidate }
    }
    $decodeErrors = 0L
    foreach ($match in $video) {
        $candidate = [int64]$match.Groups[1].Value
        if ($candidate -gt $decodeErrors) { $decodeErrors = $candidate }
    }
    if ($received -le 0 -or $receivedBytes -le 0 -or $decoded -le 0 -or $received -ne $decoded -or $ddsErrors -ne 0 -or $decodeErrors -ne 0) {
        throw "Receiver loop count contract failed for $Release`: received=$received decoded=$decoded bytes=$receivedBytes ddsErrors=$ddsErrors decodeErrors=$decodeErrors"
    }
    return [ordered]@{
        received_samples = $received
        received_bytes = $receivedBytes
        decoded_frames = $decoded
        dds_errors = $ddsErrors
        decode_errors = $decodeErrors
    }
}

function Invoke-RollbackLoopPhase {
    param([string]$Release, [ValidateSet(0, 1, 2, 3)][int]$ProtocolBand)
    $phaseDir = Join-Path $OutputDirectory $Release
    New-Item -ItemType Directory -Force -Path $phaseDir | Out-Null
    $receiverOut = Join-Path $phaseDir 'receiver.out.log'
    $receiverErr = Join-Path $phaseDir 'receiver.err.log'
    $stimOut = Join-Path $phaseDir 'stim.out.log'
    $stimErr = Join-Path $phaseDir 'stim.err.log'
    $decodedPng = Join-Path $phaseDir 'received_decode.png'
    $receiver = $null
    $stimulus = $null
    $saved = @{}
    $environmentNames = @('QT_FORCE_STDERR_LOGGING', 'P5DdsVideoPath', 'P5DdsVideoSamples', 'WeatherCameraInput', 'P5NoTargets', 'P6TestTargetType')
    foreach ($name in $environmentNames) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
    try {
        $env:QT_FORCE_STDERR_LOGGING = '1'
        $env:P5DdsVideoPath = Join-Path $phaseDir 'received_annexb.h264'
        $env:P5DdsVideoSamples = [string]($DurationSec * 60 + 120)
        $env:WeatherCameraInput = $cameraInput
        $env:P5NoTargets = '0'
        $env:P6TestTargetType = '0x55'
        $receiverArguments = @(
            '--receive-transport=dds', '--stream-role=direct', '--channel=precise',
            '--plat-id=1001', '--sensor-id=2', '--dds-domain=150',
            '--dds-topic=HwaSimIR.Video.1001.2.H264', '--dds-codec=h264',
            '--dds-width=800', '--dds-height=800', '--dds-fps=60',
            "--dds-qos=$windowsQos", "--dds-dump-first-frame=$decodedPng",
            '--dds-dump-frame-index=1', "--acceptance-exit-ms=$(($DurationSec + 30) * 1000)"
        )
        $receiver = Start-Process -FilePath $receiverExe -WorkingDirectory (Split-Path -Parent $receiverExe) -ArgumentList $receiverArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $receiverOut -RedirectStandardError $receiverErr
        Start-Sleep -Seconds 3
        $receiver.Refresh()
        if ($receiver.HasExited) { throw "Rollback receiver exited early for $Release" }
        $stimArguments = @(
            "--network-config=$networkConfig", '--control-transport=dds',
            '--dds-discovery-wait-ms=12000', '--channel=precise', '--plat-id=1001',
            '--sensor-id=2', '--sim-mode=2', '--video-fps=60',
            '--send-step-ms=16.666667', '--phase1d-h264=1', '--save-mp4=0',
            "--duration-sec=$DurationSec", '--env-sky=0', "--sensor-band=$ProtocolBand",
            '--sensor-pixel-angle-urad=10', '--utc-hour=6', '--engine-state=1',
            '--strike-flag=0', '--illuminator-en=0', '--freeze-geometry'
        )
        $stimulus = Start-Process -FilePath $stimExe -WorkingDirectory (Split-Path -Parent $stimExe) -ArgumentList $stimArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $stimOut -RedirectStandardError $stimErr
        if (-not $stimulus.WaitForExit(($DurationSec + 120) * 1000)) { throw "Rollback stimulus timed out for $Release" }
        $stimulusExit = Get-ExitedProcessCode -Process $stimulus
        if ($stimulusExit -ne 0) { throw "Rollback stimulus failed for $Release with exit $stimulusExit" }
        $stimulusText = Read-SharedText -Path $stimErr
        if ($stimulusText -notmatch "\[StimWeather\][^\r\n]*sensorBand=$ProtocolBand(?:\s|$)") {
            throw "Rollback stimulus did not declare protocol band $ProtocolBand for $Release"
        }
        if ($stimulusText -notmatch '\[StimDDS\] type=init sent=1' -or $stimulusText -notmatch '\[StimInitAck\][^\r\n]*ready=1') {
            throw "Rollback stimulus DDS INIT/ACK evidence missing for $Release"
        }
        if (-not $receiver.WaitForExit(($DurationSec + 45) * 1000)) { throw "Rollback receiver timed out for $Release" }
        $receiverExit = Get-ExitedProcessCode -Process $receiver
        if ($receiverExit -ne 0) { throw "Rollback receiver failed for $Release with exit $receiverExit" }
        if (-not (Test-Path -LiteralPath $decodedPng -PathType Leaf) -or (Get-Item -LiteralPath $decodedPng).Length -le 0) {
            throw "Rollback receiver decoded PNG missing for $Release"
        }
        $evidence = Get-ReceiverLoopEvidence -Path $receiverErr -Release $Release
        $evidence.protocol_band = $ProtocolBand
        return $evidence
    }
    finally {
        Stop-OwnedLocalProcess -Process $stimulus
        Stop-OwnedLocalProcess -Process $receiver
        foreach ($name in $environmentNames) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
    }
}

$savedPassword = $env:HWASIMIR_SSH_PASSWORD
$savedAskPass = $env:SSH_ASKPASS
$savedRequire = $env:SSH_ASKPASS_REQUIRE
$savedDisplay = $env:DISPLAY
$controllerToolSnapshot = Join-Path $OutputDirectory 'rollback_controller_tool.ps1'
$remoteToolSnapshot = Join-Path $OutputDirectory 'rollback_remote_runner_tool.sh'
$deploymentReceiptSnapshot = Join-Path $OutputDirectory 'deployment_receipt_input.json'
Copy-Item -LiteralPath $PSCommandPath -Destination $controllerToolSnapshot
Copy-Item -LiteralPath $remoteScriptLocal -Destination $remoteToolSnapshot
Copy-Item -LiteralPath $DeploymentReceipt -Destination $deploymentReceiptSnapshot
$boardOwner = $null
try {
    if (-not $BoardPassword) { throw 'Board password is required by the currently audited board SSH configuration' }
    $env:HWASIMIR_SSH_PASSWORD = $BoardPassword
    $env:SSH_ASKPASS = $askPass
    $env:SSH_ASKPASS_REQUIRE = 'force'
    $env:DISPLAY = 'p11-rk3588-rollback'
    $ssh = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new', '-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password')
    $remoteScript = "$BoardRoot/logs/p11_rk3588_rollback_$runId.sh"
    & scp.exe @ssh $remoteToolSnapshot "$BoardUser@$BoardHost`:$remoteScript"
    if ($LASTEXITCODE -ne 0) { throw 'Rollback script upload failed' }
    $controllerBegin = [DateTime]::UtcNow.ToString('o')
    $exerciseLog = Join-Path $OutputDirectory 'rollback_exercise.log'
    $exerciseErrorLog = Join-Path $OutputDirectory 'rollback_exercise.err.log'
    $command = "chmod 755 '$remoteScript'; '$remoteScript' '$BoardRoot' '$stageId' '$runId' '$RollbackSuffix' '$($RollbackElfSha256.ToLowerInvariant())' '$($RollbackConfigManifestSha256.ToLowerInvariant())' '$($activeElfSha256.ToLowerInvariant())' '$($activeConfigManifestSha256.ToLowerInvariant())' '$RollbackProtocolBand' '$RestoredProtocolBand' '$DurationSec' '$LoopReadyTimeoutSec'"
    $sshArguments = @($ssh) + @('-T', "$BoardUser@$BoardHost", $command)
    $boardOwner = Start-Process -FilePath 'ssh.exe' -ArgumentList $sshArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $exerciseLog -RedirectStandardError $exerciseErrorLog

    Wait-LocalToken -Path $exerciseLog -Token "[P11RollbackLoopReady] release=rollback result=PASS protocolBand=$RollbackProtocolBand" -Owner $boardOwner -TimeoutSec $LoopReadyTimeoutSec
    $rollbackReceiver = Invoke-RollbackLoopPhase -Release 'rollback' -ProtocolBand $RollbackProtocolBand
    Wait-LocalToken -Path $exerciseLog -Token "[P11RollbackLoopReady] release=p11_restored result=PASS protocolBand=$RestoredProtocolBand" -Owner $boardOwner -TimeoutSec $LoopReadyTimeoutSec
    $restoredReceiver = Invoke-RollbackLoopPhase -Release 'p11_restored' -ProtocolBand $RestoredProtocolBand

    if (-not $boardOwner.WaitForExit(($DurationSec + $LoopReadyTimeoutSec + 180) * 1000)) {
        throw 'Rollback board owner did not finish its final validated loop and restore'
    }
    $boardOwnerExit = Get-ExitedProcessCode -Process $boardOwner
    if ($boardOwnerExit -ne 0) { throw "Rollback exercise failed with exit $boardOwnerExit; the board-side trap was allowed to restore P11" }
    $exerciseText = Read-SharedText -Path $exerciseLog
    $exerciseErrorText = Read-SharedText -Path $exerciseErrorLog
    if ($exerciseText -notmatch '\[P11RollbackFinal\] result=PASS' -or $exerciseText -match $fatalPattern -or $exerciseErrorText -match $fatalPattern) {
        throw 'Rollback exercise did not produce a clean final PASS'
    }

    $remoteEvidence = "$BoardRoot/logs/P11_rollback_$runId"
    & scp.exe @ssh -r "$BoardUser@$BoardHost`:$remoteEvidence" $OutputDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Rollback evidence download failed' }
    $remoteEvidenceLocal = Join-Path $OutputDirectory "P11_rollback_$runId"
    $rollbackRendererPath = Join-Path $remoteEvidenceLocal 'rollback_startup.log'
    $restoredRendererPath = Join-Path $remoteEvidenceLocal 'p11_restored_startup.log'
    $rollbackRenderer = Get-RendererLoopEvidence -Path $rollbackRendererPath -Release 'rollback'
    $restoredRenderer = Get-RendererLoopEvidence -Path $restoredRendererPath -Release 'p11_restored'
    if ($rollbackRenderer.sent_samples -ne $rollbackReceiver.received_samples) {
        throw "Rollback send/receive count mismatch: $($rollbackRenderer.sent_samples)!=$($rollbackReceiver.received_samples)"
    }
    if ($restoredRenderer.sent_samples -ne $restoredReceiver.received_samples) {
        throw "Restored-P11 send/receive count mismatch: $($restoredRenderer.sent_samples)!=$($restoredReceiver.received_samples)"
    }

    function New-ReleaseEvidence {
        param(
            [string]$Release,
            [int]$ProtocolBand,
            [System.Collections.IDictionary]$Renderer,
            [System.Collections.IDictionary]$Receiver,
            [string]$RendererRelative
        )
        $rendererFull = Join-Path $OutputDirectory ($RendererRelative -replace '/', '\')
        $receiverRelative = "$Release/receiver.err.log"
        $stimulusRelative = "$Release/stim.err.log"
        $decodedRelative = "$Release/received_decode.png"
        $receiverFull = Join-Path $OutputDirectory ($receiverRelative -replace '/', '\')
        $stimulusFull = Join-Path $OutputDirectory ($stimulusRelative -replace '/', '\')
        $decodedFull = Join-Path $OutputDirectory ($decodedRelative -replace '/', '\')
        return [ordered]@{
            protocol_band = $ProtocolBand
            renderer_log = $RendererRelative
            renderer_log_sha256 = (Get-FileHash -LiteralPath $rendererFull -Algorithm SHA256).Hash.ToLowerInvariant()
            stimulus_log = $stimulusRelative
            stimulus_log_sha256 = (Get-FileHash -LiteralPath $stimulusFull -Algorithm SHA256).Hash.ToLowerInvariant()
            receiver_log = $receiverRelative
            receiver_log_sha256 = (Get-FileHash -LiteralPath $receiverFull -Algorithm SHA256).Hash.ToLowerInvariant()
            decoded_png = $decodedRelative
            decoded_png_sha256 = (Get-FileHash -LiteralPath $decodedFull -Algorithm SHA256).Hash.ToLowerInvariant()
            target_frames = $Renderer.target_frames
            completed_frames = $Renderer.completed_frames
            sent_samples = $Renderer.sent_samples
            sent_bytes = $Renderer.sent_bytes
            write_errors = $Renderer.write_errors
            dropped_samples = $Renderer.dropped_samples
            received_samples = $Receiver.received_samples
            received_bytes = $Receiver.received_bytes
            decoded_frames = $Receiver.decoded_frames
            dds_errors = $Receiver.dds_errors
            decode_errors = $Receiver.decode_errors
        }
    }

    $summary = [ordered]@{
        schema = 'hwasimir.p11.rk3588.rollback-receipt.v2'
        validation_contract = 'hwasimir.p11.rk3588.rollback-loop.v1'
        controller_begin_utc = $controllerBegin
        controller_end_utc = [DateTime]::UtcNow.ToString('o')
        stage_id = $stageId
        run_id = $runId
        duration_sec_each = $DurationSec
        rollback_started = $true
        p11_restored_and_started = $true
        rollback_snapshot_retained = $true
        active_elf_sha256 = $activeElfSha256.ToLowerInvariant()
        active_config_manifest_sha256 = $activeConfigManifestSha256.ToLowerInvariant()
        rollback_suffix = $RollbackSuffix
        rollback_elf_sha256 = $RollbackElfSha256.ToLowerInvariant()
        rollback_config_manifest_sha256 = $RollbackConfigManifestSha256.ToLowerInvariant()
        deployment_receipt = 'deployment_receipt_input.json'
        deployment_receipt_sha256 = (Get-FileHash -LiteralPath $deploymentReceiptSnapshot -Algorithm SHA256).Hash.ToLowerInvariant()
        exercise_log = 'rollback_exercise.log'
        exercise_log_sha256 = (Get-FileHash -LiteralPath $exerciseLog -Algorithm SHA256).Hash.ToLowerInvariant()
        exercise_error_log = 'rollback_exercise.err.log'
        exercise_error_log_sha256 = (Get-FileHash -LiteralPath $exerciseErrorLog -Algorithm SHA256).Hash.ToLowerInvariant()
        tool_identities = [ordered]@{
            controller = [ordered]@{
                path = 'rollback_controller_tool.ps1'
                sha256 = (Get-FileHash -LiteralPath $controllerToolSnapshot -Algorithm SHA256).Hash.ToLowerInvariant()
            }
            remote_runner = [ordered]@{
                path = 'rollback_remote_runner_tool.sh'
                sha256 = (Get-FileHash -LiteralPath $remoteToolSnapshot -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
        release_evidence = [ordered]@{
            rollback = New-ReleaseEvidence -Release 'rollback' -ProtocolBand $RollbackProtocolBand -Renderer $rollbackRenderer -Receiver $rollbackReceiver -RendererRelative "P11_rollback_$runId/rollback_startup.log"
            p11_restored = New-ReleaseEvidence -Release 'p11_restored' -ProtocolBand $RestoredProtocolBand -Renderer $restoredRenderer -Receiver $restoredReceiver -RendererRelative "P11_rollback_$runId/p11_restored_startup.log"
        }
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'rollback_receipt.json'), ($summary | ConvertTo-Json -Depth 8) + "`n", $utf8NoBom)
    Write-Host "[P11 RK3588 Rollback] result=PASS stageId=$stageId runId=$runId output=$OutputDirectory"
}
finally {
    $ownerRecoveryIssue = $null
    if ($null -ne $boardOwner) {
        $boardOwner.Refresh()
        if (-not $boardOwner.HasExited) {
            # Do not kill the sole remote owner while components may be
            # switched.  Its bounded renderer timeout reaches the EXIT trap,
            # which restores the exact P11 component set before SSH returns.
            $recoveryWaitSec = 120 + $DurationSec + $LoopReadyTimeoutSec + 120
            if (-not $boardOwner.WaitForExit($recoveryWaitSec * 1000)) {
                $ownerRecoveryIssue = 'Rollback owner recovery wait expired; SSH was intentionally left alive so the board-side atomic restore trap remains authoritative'
            }
        }
        $boardOwner.Refresh()
        if ($boardOwner.HasExited) { $boardOwner.WaitForExit() }
    }
    $env:HWASIMIR_SSH_PASSWORD = $savedPassword
    $env:SSH_ASKPASS = $savedAskPass
    $env:SSH_ASKPASS_REQUIRE = $savedRequire
    $env:DISPLAY = $savedDisplay
    if ($ownerRecoveryIssue) { throw $ownerRecoveryIssue }
}
