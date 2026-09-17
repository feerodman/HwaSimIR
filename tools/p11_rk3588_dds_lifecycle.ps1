[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Run')]
    [string]$Mode = 'Plan',
    [string]$RepoRoot = '',
    [string]$DeploymentReceipt = '',
    [string]$StageId = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [ValidateSet('HeadlessOffscreen', 'VisibleWindow')]
    [string]$PresentationMode = 'HeadlessOffscreen',
    [int]$PauseCaseSeconds = 12,
    [double]$PauseStartSec = 4.0,
    [double]$PauseDurationSec = 3.0,
    [int]$RestartCaseSeconds = 20,
    [double]$ReceiverRestartAtSec = 6.0,
    [double]$ReceiverRestartGapSec = 2.0,
    [int]$ReinitRoundSeconds = 8,
    [string]$OutputDirectory = '',
    [switch]$ConfirmLifecycleRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
$inheritedPath = [Environment]::GetEnvironmentVariable('Path', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $inheritedPath, 'Process')
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ($BoardRoot -ne '/userdata/HwaSimIR') { throw 'P11 lifecycle is restricted to /userdata/HwaSimIR' }
if ($PauseCaseSeconds -lt 10 -or $RestartCaseSeconds -lt 14 -or $ReinitRoundSeconds -lt 6) {
    throw 'Lifecycle durations are below their deterministic minimums'
}
if ($PauseStartSec -lt 2 -or $PauseDurationSec -lt 1 -or
    $PauseStartSec + $PauseDurationSec + 2 -gt $PauseCaseSeconds) {
    throw 'Pause window must leave at least two active seconds before and after the gap'
}
if ($ReceiverRestartAtSec -lt 3 -or $ReceiverRestartGapSec -lt 1 -or
    $ReceiverRestartAtSec + $ReceiverRestartGapSec + 4 -gt $RestartCaseSeconds) {
    throw 'Receiver restart window must leave decoded video before and after restart'
}

$receiverExe = Join-Path $RepoRoot 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimExe = Join-Path $RepoRoot 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$windowsQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_PROTOCOL_QOS_WINDOWS_192.168.1.188.xml'
$windowsQosIni = $windowsQos -replace '\\', '/'
$cameraInput = Join-Path $RepoRoot 'tools\p11_inputs\civil_van_close_lookat.json'
$boardHelper = Join-Path $RepoRoot 'tools\p11_rk3588_dds_lifecycle_board_run.sh'
$analyzer = Join-Path $RepoRoot 'tools\p11_rk3588_dds_lifecycle_analyze.py'
$askPass = Join-Path $RepoRoot 'tools\p5_ssh_askpass.cmd'
$requiredLocal = @($receiverExe, $stimExe, $windowsQos, $cameraInput, $boardHelper, $analyzer, $askPass)
foreach ($path in $requiredLocal) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing DDS lifecycle input: $path" }
}

$runId = 'p11-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
if ($StageId -and $StageId -notmatch '^p11-[0-9]{8}-[0-9]{6}$') { throw "Unsafe StageId: $StageId" }
$outputName = if ($StageId) { "dds-lifecycle-$StageId" } else { "dds-lifecycle-$runId" }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot "logs\p11\rk3588\$outputName" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$newline = [Environment]::NewLine

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12) + $newline, $utf8NoBom)
}

function Get-ExitedProcessCode {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return $null }
    $Process.Refresh()
    if (-not $Process.HasExited) { return $null }
    $Process.WaitForExit()
    $Process.Refresh()
    try { return [int]$Process.ExitCode } catch { return $null }
}

$casePlan = @(
    [ordered]@{ name = 'pause_swir'; scenario = 'pause_resume'; band = 'SWIR'; protocol_band = 0 },
    [ordered]@{ name = 'pause_mwir'; scenario = 'pause_resume'; band = 'MWIR'; protocol_band = 2 },
    [ordered]@{ name = 'receiver_restart'; scenario = 'receiver_restart'; band = 'MWIR'; protocol_band = 2 },
    [ordered]@{ name = 'retained_reinit'; scenario = 'retained_reinit'; band = 'MULTI'; protocol_band = -1 }
)
$plan = [ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-lifecycle-plan.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    run_id = $runId
    deployment_stage_id = $StageId
    transport = [ordered]@{ kind = 'DDS'; domain = 150; udp_tested = $false; tcp_tested = $false; tcp_payload_required_disabled = $true }
    cases = $casePlan
    pause = [ordered]@{ duration_sec = $PauseCaseSeconds; start_sec = $PauseStartSec; gap_sec = $PauseDurationSec }
    receiver_restart = [ordered]@{ duration_sec = $RestartCaseSeconds; at_sec = $ReceiverRestartAtSec; gap_sec = $ReceiverRestartGapSec }
    retained_reinit = [ordered]@{ round_seconds = $ReinitRoundSeconds; protocol_band_sequence = @(0, 2, 0) }
    remote_modified = $false
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'dds_lifecycle_plan.json') -Value $plan
if ($Mode -eq 'Plan') {
    Write-Host "[P11 DDS LifecyclePlan] result=PASS transport=DDS domain=150 cases=4 remoteModified=0 output=$OutputDirectory"
    return
}
if (-not $ConfirmLifecycleRun) { throw 'Run mode requires -ConfirmLifecycleRun' }
if (-not $DeploymentReceipt -or -not (Test-Path -LiteralPath $DeploymentReceipt)) {
    throw 'Run mode requires a deployment receipt'
}
if (Test-Path -LiteralPath (Join-Path $OutputDirectory 'dds_lifecycle_overall.json')) {
    throw 'Refusing to overwrite an existing DDS lifecycle result; choose a new OutputDirectory'
}
$DeploymentReceipt = (Resolve-Path -LiteralPath $DeploymentReceipt).Path
$receipt = Get-Content -Raw -LiteralPath $DeploymentReceipt | ConvertFrom-Json
if ($receipt.schema -ne 'hwasimir.p11.rk3588.deployment-receipt.v1') { throw 'Unexpected deployment receipt schema' }
if ($StageId -and [string]$receipt.stage_id -ne $StageId) { throw 'StageId does not match deployment receipt' }
$StageId = [string]$receipt.stage_id
$expectedElf = ([string]$receipt.elf_sha256).ToLowerInvariant()
$expectedManifest = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
if ($expectedElf -notmatch '^[0-9a-f]{64}$' -or $expectedManifest -notmatch '^[0-9a-f]{64}$') {
    throw 'Deployment receipt contains invalid hashes'
}

function Get-SshArguments {
    $arguments = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if (-not $BoardPassword) {
        if (-not $SshKey -or -not (Test-Path -LiteralPath $SshKey)) { throw 'No board authentication is available' }
        $arguments += @('-i', $SshKey, '-o', 'BatchMode=yes')
    }
    else { $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password') }
    return $arguments
}

function Use-BoardAuthentication {
    param([scriptblock]$Action)
    $savedPassword = $env:HWASIMIR_SSH_PASSWORD
    $savedAskPass = $env:SSH_ASKPASS
    $savedRequire = $env:SSH_ASKPASS_REQUIRE
    $savedDisplay = $env:DISPLAY
    try {
        if ($BoardPassword) {
            $env:HWASIMIR_SSH_PASSWORD = $BoardPassword
            $env:SSH_ASKPASS = $askPass
            $env:SSH_ASKPASS_REQUIRE = 'force'
            $env:DISPLAY = 'p11-dds-lifecycle'
        }
        & $Action
    }
    finally {
        $env:HWASIMIR_SSH_PASSWORD = $savedPassword
        $env:SSH_ASKPASS = $savedAskPass
        $env:SSH_ASKPASS_REQUIRE = $savedRequire
        $env:DISPLAY = $savedDisplay
    }
}

function Invoke-BoardSsh {
    param([string]$Command, [string]$LogPath = '', [switch]$AllowFailure)
    $result = Use-BoardAuthentication {
        $arguments = @(Get-SshArguments)
        $arguments += @("$BoardUser@$BoardHost", $Command)
        $saved = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $lines = & ssh.exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $saved
        [pscustomobject]@{ Lines = @($lines); ExitCode = $exitCode }
    }
    if ($LogPath) { [IO.File]::WriteAllText($LogPath, ($result.Lines -join $newline) + $newline, $utf8NoBom) }
    if (-not $AllowFailure -and $result.ExitCode -ne 0) { throw "Board command failed ($($result.ExitCode)): $Command" }
    return $result
}

function Invoke-BoardScp {
    param([string]$Source, [string]$Destination, [switch]$FromBoard, [switch]$Recurse)
    Use-BoardAuthentication {
        $arguments = @(Get-SshArguments)
        if ($Recurse) { $arguments += '-r' }
        if ($FromBoard) { $arguments += @(('{0}@{1}:{2}' -f $BoardUser, $BoardHost, $Source), $Destination) }
        else { $arguments += @($Source, ('{0}@{1}:{2}' -f $BoardUser, $BoardHost, $Destination)) }
        & scp.exe @arguments
        if ($LASTEXITCODE -ne 0) { throw "SCP failed ($LASTEXITCODE): $Source" }
    }
}

function Start-BoardOwner {
    param([string]$RemoteCommand, [string]$Stdout, [string]$Stderr)
    return Use-BoardAuthentication {
        $arguments = @(Get-SshArguments)
        $arguments += @('-T', "$BoardUser@$BoardHost", $RemoteCommand)
        Start-Process -FilePath 'ssh.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
    }
}

function Wait-RemoteToken {
    param([string]$RemoteLog, [string]$Token, [int]$TimeoutSec)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        $probe = Invoke-BoardSsh -Command "test -f '$RemoteLog' && grep -F '$Token' '$RemoteLog' >/dev/null" -AllowFailure
        if ($probe.ExitCode -eq 0) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}

function Wait-RemoteCount {
    param([string]$RemoteLog, [string]$Token, [int]$Count, [int]$TimeoutSec)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        $probe = Invoke-BoardSsh -Command "test -f '$RemoteLog' && test `$(grep -F -c '$Token' '$RemoteLog') -ge $Count" -AllowFailure
        if ($probe.ExitCode -eq 0) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}

function Wait-LocalToken {
    param([string]$Path, [string]$Token, [int]$TimeoutSec)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $Path) {
            if (Select-String -LiteralPath $Path -SimpleMatch $Token -Quiet) { return $true }
        }
        Start-Sleep -Milliseconds 200
    }
    return $false
}

function Stop-OwnedBoardRun {
    param([string]$RemoteCase)
    $command = "set -eu; f='$RemoteCase/runner.pid'; test -f `"`$f`" || exit 0; p=`$(cat `"`$f`"); case `"`$p`" in *[!0-9]*|'') exit 31;; esac; test -r `"/proc/`$p/cmdline`" || exit 0; tr '\000' ' ' < `"/proc/`$p/cmdline`" | grep -F 'p11_rk3588_dds_lifecycle_board_run' >/dev/null; kill -TERM `"`$p`"; exit 0"
    Invoke-BoardSsh -Command $command -AllowFailure | Out-Null
}

function Get-RendererPid {
    param([string]$RemoteCase)
    $probe = Invoke-BoardSsh -Command "set -eu; p=`$(cat '$RemoteCase/hwa.pid'); kill -0 `"`$p`"; test `"`$(pgrep -x HwaSim_IR | head -1)`" = `"`$p`"; echo `"`$p`""
    $line = @($probe.Lines | Where-Object { $_ -match '^[0-9]+$' } | Select-Object -Last 1)
    if ($line.Count -ne 1) { throw 'Unable to establish the retained renderer PID' }
    return [int]$line[0]
}

function New-DdsConfig {
    param([string]$Path)
    $lines = @(
        '[Identity]', 'channel=precise', 'platID=1001', 'sensorID=2', '',
        '[DdsProtocol]', 'DomainId=150', "QosFile=$windowsQosIni",
        'TopicControl=HwaSimIR.Control', 'TopicInit=HwaSimIR.Init',
        'TopicRealtime=HwaSimIR.Realtime', 'TopicInitAck=HwaSimIR.InitAck',
        'TopicVideoStatus=HwaSimIR.VideoStatus'
    )
    [IO.File]::WriteAllText($Path, ($lines -join $newline) + $newline, $utf8NoBom)
}

function Start-DdsReceiver {
    param([string]$Directory, [int]$ExitMs)
    New-Item -ItemType Directory -Force -Path $Directory, (Join-Path $Directory 'audit') | Out-Null
    $savedAudit = $env:HwaInputAuditDirectory
    $savedVideo = $env:P5DdsVideoPath
    $savedSamples = $env:P5DdsVideoSamples
    $savedLogging = $env:QT_FORCE_STDERR_LOGGING
    try {
        $env:HwaInputAuditDirectory = Join-Path $Directory 'audit'
        $env:P5DdsVideoPath = Join-Path $Directory 'received_annexb.h264'
        $env:P5DdsVideoSamples = '10000'
        $env:QT_FORCE_STDERR_LOGGING = '1'
        $arguments = @(
            '--receive-transport=dds', '--stream-role=direct', '--channel=precise',
            '--plat-id=1001', '--sensor-id=2', '--dds-domain=150',
            '--dds-topic=HwaSimIR.Video.1001.2.H264', '--dds-codec=h264',
            '--dds-width=800', '--dds-height=800', '--dds-fps=60', "--dds-qos=$windowsQos",
            "--dds-dump-first-frame=$(Join-Path $Directory 'received_decode.png')",
            '--dds-dump-frame-index=1', "--acceptance-exit-ms=$ExitMs"
        )
        return Start-Process -FilePath $receiverExe -WorkingDirectory (Split-Path -Parent $receiverExe) `
            -ArgumentList $arguments -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $Directory 'receiver.out.log') `
            -RedirectStandardError (Join-Path $Directory 'receiver.err.log')
    }
    finally {
        $env:HwaInputAuditDirectory = $savedAudit
        $env:P5DdsVideoPath = $savedVideo
        $env:P5DdsVideoSamples = $savedSamples
        $env:QT_FORCE_STDERR_LOGGING = $savedLogging
    }
}

function Stop-ReceiverForRestart {
    param([System.Diagnostics.Process]$Process)
    $Process.Refresh()
    if ($Process.HasExited) { return 'already_exited' }
    $Process.CloseMainWindow() | Out-Null
    if ($Process.WaitForExit(10000)) { $Process.WaitForExit(); return 'close_main_window' }
    Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    $Process.WaitForExit()
    return 'forced_after_close_timeout'
}

function Wait-ReceiverFinal {
    param([System.Diagnostics.Process]$Process, [int]$TimeoutSec)
    if (-not $Process.WaitForExit($TimeoutSec * 1000)) { return $false }
    $Process.WaitForExit()
    return (Get-ExitedProcessCode -Process $Process) -eq 0
}

function Start-DdsStimulus {
    param([string]$RoundDirectory, [string]$ConfigPath, [int]$ProtocolBand, [int]$Seconds,
          [double]$PauseAt = -1, [double]$PauseFor = 0)
    New-Item -ItemType Directory -Force -Path $RoundDirectory, (Join-Path $RoundDirectory 'audit') | Out-Null
    $savedAudit = $env:HwaInputAuditDirectory
    $savedCamera = $env:WeatherCameraInput
    $savedTargets = $env:P5NoTargets
    $savedType = $env:P6TestTargetType
    $savedLogging = $env:QT_FORCE_STDERR_LOGGING
    try {
        $env:HwaInputAuditDirectory = Join-Path $RoundDirectory 'audit'
        $env:WeatherCameraInput = $cameraInput
        $env:P5NoTargets = '0'
        $env:P6TestTargetType = '0x55'
        $env:QT_FORCE_STDERR_LOGGING = '1'
        $arguments = @(
            "--network-config=$ConfigPath", '--control-transport=dds', '--dds-discovery-wait-ms=12000',
            '--channel=precise', '--plat-id=1001', '--sensor-id=2', '--sim-mode=2',
            '--video-fps=60', '--send-step-ms=16.666667', '--phase1d-h264=1', '--save-mp4=0',
            "--duration-sec=$Seconds", '--env-sky=0', "--sensor-band=$ProtocolBand",
            '--sensor-pixel-angle-urad=10', '--utc-hour=6', '--engine-state=1', '--strike-flag=0',
            '--illuminator-en=0', '--freeze-geometry'
        )
        if ($PauseAt -ge 0 -and $PauseFor -gt 0) {
            $arguments += @("--pause-start-sec=$($PauseAt.ToString('F3', [Globalization.CultureInfo]::InvariantCulture))",
                            "--pause-duration-sec=$($PauseFor.ToString('F3', [Globalization.CultureInfo]::InvariantCulture))")
        }
        return Start-Process -FilePath $stimExe -WorkingDirectory (Split-Path -Parent $stimExe) `
            -ArgumentList $arguments -WindowStyle Hidden -PassThru `
            -RedirectStandardOutput (Join-Path $RoundDirectory 'stim.out.log') `
            -RedirectStandardError (Join-Path $RoundDirectory 'stim.err.log')
    }
    finally {
        $env:HwaInputAuditDirectory = $savedAudit
        $env:WeatherCameraInput = $savedCamera
        $env:P5NoTargets = $savedTargets
        $env:P6TestTargetType = $savedType
        $env:QT_FORCE_STDERR_LOGGING = $savedLogging
    }
}

function Wait-Stimulus {
    param([System.Diagnostics.Process]$Process, [int]$Seconds)
    if (-not $Process.WaitForExit(($Seconds + 120) * 1000)) { throw 'DDS stimulus exceeded bounded timeout' }
    $code = Get-ExitedProcessCode -Process $Process
    if ($null -eq $code -or $code -ne 0) { throw "DDS stimulus failed with exit $code" }
}

$toolHashes = foreach ($path in $requiredLocal) {
    [ordered]@{ path = $path; bytes = (Get-Item -LiteralPath $path).Length;
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'dds_lifecycle_inputs.json') -Value ([ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-lifecycle-inputs.v1'
    deployment_receipt = $DeploymentReceipt
    deployment_receipt_sha256 = (Get-FileHash -LiteralPath $DeploymentReceipt -Algorithm SHA256).Hash.ToLowerInvariant()
    expected_elf_sha256 = $expectedElf
    expected_config_manifest_sha256 = $expectedManifest
    files = @($toolHashes)
})

$preflight = Invoke-BoardSsh -Command "set -eu; cd '$BoardRoot'; test ! -e .deployment_in_progress; test ! -e .retention_in_progress; test ! -e .p11_rollback_in_progress; ! pgrep -x HwaSim_IR >/dev/null; test `$(sha256sum HwaSim_IR | awk '{print `$1}') = '$expectedElf'; test `$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}') = '$expectedManifest'; echo '[P11DdsLifecyclePreflight] result=PASS transport=DDS domain=150 staleProcess=0'" `
    -LogPath (Join-Path $OutputDirectory 'board_preflight.log')
if (-not ($preflight.Lines -match '\[P11DdsLifecyclePreflight\] result=PASS')) { throw 'Board lifecycle preflight failed' }

$remoteHelper = "/tmp/p11_rk3588_dds_lifecycle_board_run_$runId.sh"
Invoke-BoardScp -Source $boardHelper -Destination $remoteHelper
$helperHash = (Get-FileHash -LiteralPath $boardHelper -Algorithm SHA256).Hash.ToLowerInvariant()
Invoke-BoardSsh -Command "set -eu; chmod 755 '$remoteHelper'; test `$(sha256sum '$remoteHelper' | awk '{print `$1}') = '$helperHash'; echo '[P11DdsLifecycleHelper] result=PASS sha256=$helperHash'" `
    -LogPath (Join-Path $OutputDirectory 'board_helper_verify.log') | Out-Null

$configPath = Join-Path $OutputDirectory 'NetworkConfig_dds.ini'
New-DdsConfig -Path $configPath
$rows = New-Object System.Collections.Generic.List[object]

function Complete-Case {
    param([string]$Name, [string]$Scenario, [string]$BandName, [int]$ProtocolBand,
          [string]$CaseDir, [string]$RemoteCase, [System.Collections.Generic.List[string]]$Errors,
          [hashtable]$RuntimeData)
    $copyOk = $true
    try { Invoke-BoardScp -Source $RemoteCase -Destination $CaseDir -FromBoard -Recurse }
    catch { $Errors.Add("Board evidence download failed: $($_.Exception.Message)"); $copyOk = $false }
    $post = Invoke-BoardSsh -Command "set -eu; ! pgrep -x HwaSim_IR >/dev/null; echo '[P11DdsLifecyclePostflight] result=PASS hwasimirStillRunning=0'" `
        -LogPath (Join-Path $CaseDir 'board_postflight.log') -AllowFailure
    if ($post.ExitCode -ne 0) { $Errors.Add('Board postflight found a residual HwaSim_IR process') }
    $RuntimeData.schema = 'hwasimir.p11.rk3588.dds-lifecycle-runtime.v1'
    $RuntimeData.process_result = if ($Errors.Count -eq 0 -and $copyOk) { 'PASS' } else { 'FAIL' }
    $RuntimeData.errors = @($Errors)
    Write-JsonFile -Path (Join-Path $CaseDir 'runtime_status.json') -Value $RuntimeData
    $saved = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $arguments = @($analyzer, '--case-dir', $CaseDir, '--scenario', $Scenario,
        '--band', $BandName, '--protocol-band', "$ProtocolBand", '--pause-duration-sec', "$PauseDurationSec")
    & python.exe @arguments 1> (Join-Path $CaseDir 'analyzer.out.log') 2> (Join-Path $CaseDir 'analyzer.err.log')
    $analyzerExit = $LASTEXITCODE
    $ErrorActionPreference = $saved
    $summaryPath = Join-Path $CaseDir 'lifecycle_summary.json'
    $caseResult = 'FAIL'
    if (Test-Path -LiteralPath $summaryPath) {
        $caseResult = [string]((Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json).result)
    }
    if ($analyzerExit -ne 0) { $caseResult = 'FAIL' }
    $relativeSummary = $summaryPath.Substring($OutputDirectory.Length).TrimStart('\') -replace '\\', '/'
    $rows.Add([ordered]@{
        scenario = $Name
        lifecycle_scenario = $Scenario
        band = $BandName
        protocol_band = $ProtocolBand
        result = $caseResult
        summary = $relativeSummary
        summarySha256 = if (Test-Path -LiteralPath $summaryPath) { (Get-FileHash -LiteralPath $summaryPath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null }
    })
    Write-Host "[P11 DDS LifecycleCase] scenario=$Name result=$caseResult output=$CaseDir"
}

foreach ($spec in $casePlan | Where-Object { $_.scenario -eq 'pause_resume' }) {
    $name = [string]$spec.name
    $caseDir = Join-Path $OutputDirectory $name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $remoteCase = "$BoardRoot/logs/P11_dds_lifecycle_$runId/$name/board"
    $remoteLog = "$remoteCase/hwa.log"
    $errors = New-Object System.Collections.Generic.List[string]
    $runtime = @{}
    $receiver = $null; $stim = $null; $owner = $null
    try {
        # DDS stimulus deliberately waits 12 s for discovery before RESET/INIT.
        # Keep the receiver alive through discovery, the pause and STOP flush.
        $receiver = Start-DdsReceiver -Directory (Join-Path $caseDir 'receiver') -ExitMs (($PauseCaseSeconds + 35) * 1000)
        $runtime.receiver_before_pid = [int]$receiver.Id
        Start-Sleep -Seconds 2
        $remoteCommand = "'$remoteHelper' '$remoteCase' '$expectedElf' '$expectedManifest' true $($PauseCaseSeconds + 90) '$PresentationMode'"
        $owner = Start-BoardOwner -RemoteCommand $remoteCommand -Stdout (Join-Path $caseDir 'board_ssh.out.log') -Stderr (Join-Path $caseDir 'board_ssh.err.log')
        if (-not (Wait-RemoteToken -RemoteLog $remoteLog -Token '[DdsVideo] initialized=1' -TimeoutSec 60)) { throw 'DDS publisher startup timeout' }
        $runtime.producer_pids = @((Get-RendererPid -RemoteCase $remoteCase))
        $stim = Start-DdsStimulus -RoundDirectory (Join-Path $caseDir 'round_1') -ConfigPath $configPath `
            -ProtocolBand ([int]$spec.protocol_band) -Seconds $PauseCaseSeconds -PauseAt $PauseStartSec -PauseFor $PauseDurationSec
        Wait-Stimulus -Process $stim -Seconds $PauseCaseSeconds
        if (-not $owner.WaitForExit(120000)) { throw 'Board pause case did not finish STOP drain' }
        if ((Get-ExitedProcessCode -Process $owner) -ne 0) { throw 'Board pause owner returned failure' }
        if (-not (Wait-ReceiverFinal -Process $receiver -TimeoutSec 45)) { throw 'DDS receiver did not finalize pause case' }
    }
    catch { $errors.Add($_.Exception.Message) }
    finally {
        if ($stim -and -not $stim.HasExited) { Stop-Process -Id $stim.Id -Force -ErrorAction SilentlyContinue }
        if ($owner) { Stop-OwnedBoardRun -RemoteCase $remoteCase; if (-not $owner.HasExited) { $owner.WaitForExit(10000) | Out-Null } }
        if ($receiver -and -not $receiver.HasExited) { Stop-ReceiverForRestart -Process $receiver | Out-Null }
        Complete-Case -Name $name -Scenario 'pause_resume' -BandName ([string]$spec.band) `
            -ProtocolBand ([int]$spec.protocol_band) -CaseDir $caseDir -RemoteCase $remoteCase -Errors $errors -RuntimeData $runtime
    }
}

& {
    $name = 'receiver_restart'; $caseDir = Join-Path $OutputDirectory $name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $remoteCase = "$BoardRoot/logs/P11_dds_lifecycle_$runId/$name/board"; $remoteLog = "$remoteCase/hwa.log"
    $errors = New-Object System.Collections.Generic.List[string]; $runtime = @{}
    $before = $null; $after = $null; $stim = $null; $owner = $null
    try {
        $before = Start-DdsReceiver -Directory (Join-Path $caseDir 'receiver_before') -ExitMs (($RestartCaseSeconds + 90) * 1000)
        $runtime.receiver_before_pid = [int]$before.Id
        Start-Sleep -Seconds 2
        $remoteCommand = "'$remoteHelper' '$remoteCase' '$expectedElf' '$expectedManifest' true $($RestartCaseSeconds + 90) '$PresentationMode'"
        $owner = Start-BoardOwner -RemoteCommand $remoteCommand -Stdout (Join-Path $caseDir 'board_ssh.out.log') -Stderr (Join-Path $caseDir 'board_ssh.err.log')
        if (-not (Wait-RemoteToken -RemoteLog $remoteLog -Token '[DdsVideo] initialized=1' -TimeoutSec 60)) { throw 'DDS publisher startup timeout' }
        $pidBefore = Get-RendererPid -RemoteCase $remoteCase
        $stim = Start-DdsStimulus -RoundDirectory (Join-Path $caseDir 'round_1') -ConfigPath $configPath -ProtocolBand 2 -Seconds $RestartCaseSeconds
        $stimLog = Join-Path $caseDir 'round_1\stim.err.log'
        if (-not (Wait-LocalToken -Path $stimLog -Token '[StimDDS] type=control command=2 sent=1' -TimeoutSec 60)) { throw 'START not observed before receiver restart' }
        if (-not (Wait-LocalToken -Path (Join-Path $caseDir 'receiver_before\receiver.err.log') -Token '[H264DecodeSuccess]' -TimeoutSec 30)) { throw 'Receiver did not decode before restart' }
        Start-Sleep -Milliseconds ([int]($ReceiverRestartAtSec * 1000))
        $runtime.receiver_before_stop_mode = Stop-ReceiverForRestart -Process $before
        Start-Sleep -Milliseconds ([int]($ReceiverRestartGapSec * 1000))
        $after = Start-DdsReceiver -Directory (Join-Path $caseDir 'receiver_after') -ExitMs (($RestartCaseSeconds + 25) * 1000)
        $runtime.receiver_after_pid = [int]$after.Id
        $pidAfter = Get-RendererPid -RemoteCase $remoteCase
        $runtime.producer_pids = @($pidBefore, $pidAfter)
        if ($pidBefore -ne $pidAfter) { throw 'Renderer PID changed during receiver restart' }
        if (-not (Wait-LocalToken -Path (Join-Path $caseDir 'receiver_after\receiver.err.log') -Token '[H264DecodeSuccess]' -TimeoutSec 30)) { throw 'Restarted receiver did not recover decode' }
        Wait-Stimulus -Process $stim -Seconds $RestartCaseSeconds
        if (-not $owner.WaitForExit(120000)) { throw 'Board restart case did not finish STOP drain' }
        if ((Get-ExitedProcessCode -Process $owner) -ne 0) { throw 'Board restart owner returned failure' }
        if (-not (Wait-ReceiverFinal -Process $after -TimeoutSec 45)) { throw 'Restarted DDS receiver did not finalize' }
    }
    catch { $errors.Add($_.Exception.Message) }
    finally {
        if ($stim -and -not $stim.HasExited) { Stop-Process -Id $stim.Id -Force -ErrorAction SilentlyContinue }
        if ($owner) { Stop-OwnedBoardRun -RemoteCase $remoteCase; if (-not $owner.HasExited) { $owner.WaitForExit(10000) | Out-Null } }
        foreach ($process in @($before, $after)) { if ($process -and -not $process.HasExited) { Stop-ReceiverForRestart -Process $process | Out-Null } }
        Complete-Case -Name $name -Scenario 'receiver_restart' -BandName 'MWIR' -ProtocolBand 2 `
            -CaseDir $caseDir -RemoteCase $remoteCase -Errors $errors -RuntimeData $runtime
    }
}

& {
    $name = 'retained_reinit'; $caseDir = Join-Path $OutputDirectory $name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $remoteCase = "$BoardRoot/logs/P11_dds_lifecycle_$runId/$name/board"; $remoteLog = "$remoteCase/hwa.log"
    $errors = New-Object System.Collections.Generic.List[string]; $runtime = @{}
    $receiver = $null; $owner = $null; $stim = $null
    try {
        # Each fresh stimulus process has its own 12 s DDS discovery interval.
        $receiver = Start-DdsReceiver -Directory (Join-Path $caseDir 'receiver') `
            -ExitMs ((3 * ($ReinitRoundSeconds + 25) + 30) * 1000)
        $runtime.receiver_before_pid = [int]$receiver.Id
        Start-Sleep -Seconds 2
        $remoteCommand = "'$remoteHelper' '$remoteCase' '$expectedElf' '$expectedManifest' false $((3 * $ReinitRoundSeconds) + 150) '$PresentationMode'"
        $owner = Start-BoardOwner -RemoteCommand $remoteCommand -Stdout (Join-Path $caseDir 'board_ssh.out.log') -Stderr (Join-Path $caseDir 'board_ssh.err.log')
        if (-not (Wait-RemoteToken -RemoteLog $remoteLog -Token '[DdsVideo] initialized=1' -TimeoutSec 60)) { throw 'DDS publisher startup timeout' }
        $pids = New-Object System.Collections.Generic.List[int]
        $pids.Add((Get-RendererPid -RemoteCase $remoteCase))
        $sequence = @(0, 2, 0)
        for ($index = 0; $index -lt $sequence.Count; $index++) {
            $round = $index + 1
            $stim = Start-DdsStimulus -RoundDirectory (Join-Path $caseDir "round_$round") -ConfigPath $configPath `
                -ProtocolBand $sequence[$index] -Seconds $ReinitRoundSeconds
            Wait-Stimulus -Process $stim -Seconds $ReinitRoundSeconds
            if (-not (Wait-RemoteCount -RemoteLog $remoteLog -Token '[SyncRoundConservation]' -Count $round -TimeoutSec 60)) {
                throw "Round $round did not finish conservation/drain"
            }
            $pids.Add((Get-RendererPid -RemoteCase $remoteCase))
        }
        $runtime.producer_pids = @($pids | ForEach-Object { $_ })
        Invoke-BoardSsh -Command "set -eu; touch '$remoteCase/controller.done'" | Out-Null
        if (-not $owner.WaitForExit(30000)) { throw 'Retained board owner did not terminate after controller.done' }
        if ((Get-ExitedProcessCode -Process $owner) -ne 0) { throw 'Retained board owner returned failure' }
        if (-not (Wait-ReceiverFinal -Process $receiver -TimeoutSec 45)) { throw 'DDS receiver did not finalize retained re-INIT case' }
    }
    catch { $errors.Add($_.Exception.Message) }
    finally {
        if ($stim -and -not $stim.HasExited) { Stop-Process -Id $stim.Id -Force -ErrorAction SilentlyContinue }
        if ($owner) { Stop-OwnedBoardRun -RemoteCase $remoteCase; if (-not $owner.HasExited) { $owner.WaitForExit(10000) | Out-Null } }
        if ($receiver -and -not $receiver.HasExited) { Stop-ReceiverForRestart -Process $receiver | Out-Null }
        Complete-Case -Name $name -Scenario 'retained_reinit' -BandName 'MULTI' -ProtocolBand -1 `
            -CaseDir $caseDir -RemoteCase $remoteCase -Errors $errors -RuntimeData $runtime
    }
}

$overallFailed = @($rows | Where-Object { $_.result -ne 'PASS' }).Count -gt 0
$overall = [ordered]@{
    schema = 'hwasimir.p11.rk3588.dds-lifecycle-summary.v1'
    result = if ($overallFailed) { 'FAIL' } else { 'PASS' }
    run_id = $runId
    deployment_stage_id = $StageId
    elf_sha256 = $expectedElf
    config_manifest_sha256 = $expectedManifest
    transport = [ordered]@{ kind = 'DDS'; domain = 150; udp_tested = $false; tcp_tested = $false; tcp_payload_required_disabled = $true }
    completed_utc = [DateTime]::UtcNow.ToString('o')
    cases = @($rows | ForEach-Object { $_ })
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'dds_lifecycle_overall.json') -Value $overall
$manifestPath = Join-Path $OutputDirectory 'artifact_manifest.sha256'
$manifestLines = foreach ($path in Get-ChildItem -LiteralPath $OutputDirectory -File -Recurse | Where-Object { $_.FullName -ne $manifestPath } | Sort-Object FullName) {
    '{0}  {1}' -f (Get-FileHash -LiteralPath $path.FullName -Algorithm SHA256).Hash.ToLowerInvariant(),
        (($path.FullName.Substring($OutputDirectory.Length).TrimStart('\')) -replace '\\', '/')
}
[IO.File]::WriteAllText($manifestPath, ($manifestLines -join $newline) + $newline, $utf8NoBom)
Write-Host "[P11 DDS Lifecycle] result=$($overall.result) transport=DDS domain=150 output=$OutputDirectory"
if ($overallFailed) { exit 1 }
