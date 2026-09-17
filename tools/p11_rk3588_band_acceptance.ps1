[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Run')]
    [string]$Mode = 'Plan',
    [ValidateSet('All', 'SWIR', 'MWIR')]
    [string]$Band = 'All',
    [string]$RepoRoot = '',
    [string]$DeploymentReceipt = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [ValidateSet('HeadlessOffscreen', 'VisibleWindow')]
    [string]$PresentationMode = 'HeadlessOffscreen',
    [int]$DurationSec = 60,
    [double]$MinFps = 59.0,
    [double]$MaxFps = 61.5,
    [string]$RawSeqs = '',
    [int]$ReceiverDumpFrameIndex = 0,
    [string]$CameraInput = '',
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_.-]{0,95}$')]
    [string]$ScenarioId = 'baseline',
    [ValidateSet('fixed', 'agc', 'annotated')]
    [string]$CaptureVariant = 'agc',
    [string[]]$StimExtraArgs = @(),
    [hashtable]$BoardEnvironment = @{},
    [string]$OutputDirectory = '',
    [switch]$ConfirmAcceptanceRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
# Windows PowerShell can inherit both PATH and Path as distinct process-block
# entries even though Start-Process treats its environment dictionary as
# case-insensitive.  Normalize before spawning the receiver, sender or SSH.
$inheritedPath = [Environment]::GetEnvironmentVariable('Path', 'Process')
[Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
[Environment]::SetEnvironmentVariable('Path', $inheritedPath, 'Process')
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ($BoardRoot -ne '/userdata/HwaSimIR') { throw 'P11 acceptance is restricted to /userdata/HwaSimIR' }
if ($DurationSec -lt 10) { throw 'DurationSec must be at least 10 seconds' }
if ($MinFps -le 0 -or $MaxFps -lt $MinFps) { throw 'Invalid FPS acceptance range' }
if (-not $RawSeqs) {
    $RawSeqs = if ($DurationSec -ge 60) { '180,900,1800,2700,3540' } else { '120,480' }
}
$scenarioOverridesRequested = $PSBoundParameters.ContainsKey('CameraInput') -or
    $PSBoundParameters.ContainsKey('ScenarioId') -or
    $PSBoundParameters.ContainsKey('CaptureVariant') -or
    $PSBoundParameters.ContainsKey('StimExtraArgs') -or
    $PSBoundParameters.ContainsKey('BoardEnvironment')
if ($RawSeqs -notmatch '^[1-9][0-9]*(,[1-9][0-9]*)+$') {
    throw 'RawSeqs must contain at least two comma-separated positive source sequences'
}
$rawSeqValues = @($RawSeqs -split ',' | ForEach-Object { [int]$_ })
$maximumExpectedFrames = $DurationSec * 60
if (($rawSeqValues | Measure-Object -Maximum).Maximum -gt $maximumExpectedFrames) {
    throw "RawSeqs exceeds the nominal acceptance frame count $maximumExpectedFrames"
}
if ($ReceiverDumpFrameIndex -le 0) {
    $ReceiverDumpFrameIndex = if ($DurationSec -ge 60) { 1800 } else { [Math]::Max(1, [Math]::Min(300, $maximumExpectedFrames - 1)) }
}
if ($ReceiverDumpFrameIndex -gt $maximumExpectedFrames) {
    throw "ReceiverDumpFrameIndex exceeds the nominal acceptance frame count $maximumExpectedFrames"
}

$receiverExe = Join-Path $RepoRoot 'HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe'
$stimExe = Join-Path $RepoRoot 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$windowsQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_PROTOCOL_QOS_WINDOWS_192.168.1.188.xml'
$windowsQosIni = $windowsQos -replace '\\', '/'
$defaultCameraInput = Join-Path $RepoRoot 'tools\p11_inputs\civil_van_close_lookat.json'
if (-not $CameraInput) { $CameraInput = $defaultCameraInput }
$cameraInput = (Resolve-Path -LiteralPath $CameraInput).Path
$boardRunner = Join-Path $RepoRoot 'tools\p11_rk3588_band_board_run.sh'
$analyzer = Join-Path $RepoRoot 'tools\p11_rk3588_acceptance_analyze.py'
$askPass = Join-Path $RepoRoot 'tools\p5_ssh_askpass.cmd'
$requiredLocal = @($receiverExe, $stimExe, $windowsQos, $cameraInput, $boardRunner, $analyzer, $askPass)
foreach ($path in $requiredLocal) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing acceptance input: $path" }
}

$allowedStimExtra = @(
    '^--env-sky=[0-9]+$', '^--engine-state=[0-9]+$', '^--strike-flag=[01]$',
    '^--illuminator-en=[01]$', '^--illuminator-angle-mrad=[0-9]+(?:\.[0-9]+)?$',
    '^--illuminator-spot-rad=[0-9]+(?:\.[0-9]+)?$',
    '^--illuminator-on-start-sec=[0-9]+(?:\.[0-9]+)?$',
    '^--illuminator-on-end-sec=[0-9]+(?:\.[0-9]+)?$',
    '^--sensor-pixel-angle-urad=[0-9]+(?:\.[0-9]+)?$',
    '^--utc-hour=[0-9]+(?:\.[0-9]+)?$', '^--freeze-geometry$'
)
foreach ($argument in @($StimExtraArgs)) {
    if (-not ($allowedStimExtra | Where-Object { $argument -match $_ })) {
        throw "Unsupported scenario stimulus argument: $argument"
    }
    if ($argument -match '(?i)(udp|tcp|transport|network-config|dds-)') {
        throw "Scenario stimulus argument may not alter transport: $argument"
    }
}
$allowedBoardEnvironment = @(
    'EnableAGC', 'AnnotationOverlayInSensorImage',
    'NaturalSolarEnable', 'NaturalSolarEnableOpticalShadow',
    'NaturalSolarEnableSolarThermal', 'NaturalSolarDebugLog',
    'M1SolarOverrideEnable', 'M1SolarOverrideAzimuthDeg',
    'M1SolarOverrideElevationDeg', 'M1SunVisibility', 'M1FallbackUtcDate',
    'ActiveIlluminatorEnable', 'ActiveIlluminatorBand',
    'ActiveIlluminatorCenterWavelengthUm', 'ActiveIlluminatorBandwidthUm',
    'ActiveIlluminatorIntensityMode', 'ActiveIlluminatorDebugLog',
    'EnableAeroThermalModel', 'ApplyAeroToRadiance', 'AeroApplyOnlyBand',
    'PostprocessAA'
)
foreach ($key in @($BoardEnvironment.Keys)) {
    if ($allowedBoardEnvironment -notcontains [string]$key) {
        throw "Unsupported board scenario environment key: $key"
    }
    $value = [string]$BoardEnvironment[$key]
    if (-not $value -or $value -notmatch '^[A-Za-z0-9_.,/+:-]+$') {
        throw "Unsafe board scenario environment value: $key"
    }
}
if (-not $BoardEnvironment.ContainsKey('EnableAGC')) {
    $BoardEnvironment['EnableAGC'] = if ($CaptureVariant -eq 'fixed') { 'false' } else { 'true' }
}
if (-not $BoardEnvironment.ContainsKey('AnnotationOverlayInSensorImage')) {
    $BoardEnvironment['AnnotationOverlayInSensorImage'] = if ($CaptureVariant -eq 'annotated') { 'true' } else { 'false' }
}
$boardEnvironmentForJson = [ordered]@{}
foreach ($key in @($BoardEnvironment.Keys | Sort-Object)) {
    $boardEnvironmentForJson[[string]$key] = [string]$BoardEnvironment[$key]
}

$runId = 'p11-' + (Get-Date -Format 'yyyyMMdd-HHmmss')
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot "logs\p11\rk3588\acceptance-$runId" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$newline = [Environment]::NewLine

function Write-JsonFile {
    param([string]$Path, [object]$Value)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 8) + $newline, $utf8NoBom)
}

function Get-ExitedProcessCode {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return $null }
    $Process.Refresh()
    if (-not $Process.HasExited) { return $null }
    # Drain redirected stdout/stderr before reading ExitCode. Windows
    # PowerShell 5.1 can otherwise expose a transient null property.
    $Process.WaitForExit()
    $Process.Refresh()
    try { return [int]$Process.ExitCode }
    catch { return $null }
}

$bands = if ($Band -eq 'All') {
    @(
        [pscustomobject]@{ Name = 'SWIR'; Protocol = 0 },
        [pscustomobject]@{ Name = 'MWIR'; Protocol = 2 }
    )
}
elseif ($Band -eq 'SWIR') { @([pscustomobject]@{ Name = 'SWIR'; Protocol = 0 }) }
else { @([pscustomobject]@{ Name = 'MWIR'; Protocol = 2 }) }

$plan = [ordered]@{
    schema = 'hwasimir.p11.rk3588.band-acceptance-plan.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    run_id = $runId
    bands = @($bands | ForEach-Object { [ordered]@{ name = $_.Name; protocol_band = $_.Protocol } })
    duration_sec = $DurationSec
    resolution = '800x800'
    target_fps = 60
    presentation_mode = $PresentationMode
    fps_gate = [ordered]@{ minimum = $MinFps; maximum = $MaxFps }
    transport = 'DDS domain 150, HwaSimIR.Video.1001.2.H264'
    codec = 'RK3588 MPP H.264 Annex-B to Windows FFmpeg decoder'
    input_policy = 'OrderedQueue, one-to-one ledgers'
    raw_radiance_unit = 'W/(m^2 sr um)'
    raw_diagnostic_sequences = @($rawSeqValues)
    receiver_dump_frame_index = $ReceiverDumpFrameIndex
    scenario = [ordered]@{
        id = $ScenarioId
        capture_variant = $CaptureVariant
        fixture = $cameraInput
        fixture_sha256 = (Get-FileHash -LiteralPath $cameraInput -Algorithm SHA256).Hash.ToLowerInvariant()
        stimulus_extra_args = @($StimExtraArgs)
        board_environment = $boardEnvironmentForJson
    }
    remote_modified = $false
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'acceptance_plan.json') -Value $plan
if ($Mode -eq 'Plan') {
    Write-Host "[P11 RK3588 AcceptancePlan] result=PASS bands=$Band durationSec=$DurationSec transport=DDS remoteModified=0 output=$OutputDirectory"
    return
}
if (-not $ConfirmAcceptanceRun) { throw 'Run mode requires -ConfirmAcceptanceRun' }
if (-not $DeploymentReceipt -or -not (Test-Path -LiteralPath $DeploymentReceipt)) {
    throw 'Run mode requires a deployment receipt'
}
$DeploymentReceipt = (Resolve-Path -LiteralPath $DeploymentReceipt).Path
$receipt = Get-Content -Raw -LiteralPath $DeploymentReceipt | ConvertFrom-Json
if ($receipt.schema -ne 'hwasimir.p11.rk3588.deployment-receipt.v1') {
    throw 'Unexpected deployment receipt schema'
}
$expectedElf = ([string]$receipt.elf_sha256).ToLowerInvariant()
$expectedManifest = ([string]$receipt.config_manifest_sha256).ToLowerInvariant()
if ($expectedElf -notmatch '^[0-9a-f]{64}$' -or $expectedManifest -notmatch '^[0-9a-f]{64}$') {
    throw 'Deployment receipt contains invalid hashes'
}

function Get-SshArguments {
    $arguments = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if (-not $BoardPassword) {
        if (-not $SshKey -or -not (Test-Path -LiteralPath $SshKey)) {
            throw 'Neither board password nor readable SSH key is available'
        }
        $arguments += @('-i', $SshKey, '-o', 'BatchMode=yes')
    }
    else {
        $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password')
    }
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
            $env:DISPLAY = 'p11-rk3588-acceptance'
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
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $lines = & ssh.exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        [pscustomobject]@{ Lines = @($lines); ExitCode = $exitCode }
    }
    if ($LogPath) {
        [IO.File]::WriteAllText($LogPath, ($result.Lines -join $newline) + $newline, $utf8NoBom)
    }
    if (-not $AllowFailure -and $result.ExitCode -ne 0) {
        throw "Board command failed ($($result.ExitCode)): $Command"
    }
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
        Start-Process -FilePath 'ssh.exe' -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
    }
}

function Wait-RemoteToken {
    param([string]$RemoteLog, [string]$Token, [int]$TimeoutSec)
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSec)
    while ([DateTime]::UtcNow -lt $deadline) {
        $probe = Invoke-BoardSsh -Command "test -f '$RemoteLog' && grep -F '$Token' '$RemoteLog' >/dev/null" -AllowFailure
        if ($probe.ExitCode -eq 0) { return $true }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Stop-OwnedBoardRun {
    param([string]$RemoteCase)
    $cleanupTemplate = @'
set -eu
f='{0}/runner.pid'
test -f "$f" || exit 0
p=$(cat "$f")
case "$p" in *[!0-9]*|'') exit 31;; esac
test -r "/proc/$p/cmdline" || exit 0
tr '\000' ' ' < "/proc/$p/cmdline" | grep -F 'p11_rk3588_band_board_run' >/dev/null
tr '\000' ' ' < "/proc/$p/cmdline" | grep -F '{0}' >/dev/null
kill -TERM "$p"
for n in 1 2 3 4 5 6 7 8 9 10; do
    kill -0 "$p" 2>/dev/null || exit 0
    sleep 1
done
echo "[P11Cleanup][FATAL] owned_runner_still_alive pid=$p case={0}" >&2
exit 33
'@
    $command = $cleanupTemplate -f $RemoteCase
    Invoke-BoardSsh -Command $command -AllowFailure | Out-Null
}

function Save-ControllerClock {
    param([string]$Path, [string]$Phase)
    $content = @(
        'schema=hwasimir.p11.controller-clock.v1',
        "phase=$Phase",
        "controllerUtc=$([DateTime]::UtcNow.ToString('o'))",
        "controllerFileTimeUtc=$([DateTime]::UtcNow.ToFileTimeUtc())",
        "timezone=$([TimeZoneInfo]::Local.Id)"
    )
    [IO.File]::WriteAllText($Path, ($content -join $newline) + $newline, $utf8NoBom)
}

$toolHashes = foreach ($path in $requiredLocal) {
    [ordered]@{
        path = $path
        sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
        bytes = (Get-Item -LiteralPath $path).Length
    }
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'acceptance_inputs.json') -Value ([ordered]@{
    schema = 'hwasimir.p11.rk3588.acceptance-inputs.v1'
    deployment_receipt = $DeploymentReceipt
    deployment_receipt_sha256 = (Get-FileHash -LiteralPath $DeploymentReceipt -Algorithm SHA256).Hash.ToLowerInvariant()
    expected_elf_sha256 = $expectedElf
    expected_config_manifest_sha256 = $expectedManifest
    files = @($toolHashes)
})

$preflightTemplate = @'
set -eu
cd '{0}'
test ! -e .deployment_in_progress
test ! -e .retention_in_progress
test ! -e .p11_rollback_in_progress
! pgrep -x HwaSim_IR >/dev/null
test "$(sha256sum HwaSim_IR | cut -d' ' -f1)" = '{1}'
test "$(sha256sum Config/deployment_manifest.sha256 | cut -d' ' -f1)" = '{2}'
test -x run_precise.sh
test -x rk3588_hwasimir_perf_sample.sh
test -e /dev/mali0
pgrep -o -f '[X]org[[:space:]]+:0([[:space:]]|$)' >/dev/null
printf '[P11AcceptancePreflight] result=PASS boardUtc='
date -u +%Y-%m-%dT%H:%M:%SZ
echo ' elfSha256={1} configManifestSha256={2} Xorg=1 Mali=1 staleProcess=0'
'@
$preflightCommand = $preflightTemplate -f $BoardRoot, $expectedElf, $expectedManifest
$preflight = Invoke-BoardSsh -Command $preflightCommand -LogPath (Join-Path $OutputDirectory 'board_preflight.log')
if (-not ($preflight.Lines -match '\[P11AcceptancePreflight\] result=PASS')) {
    throw 'Board preflight did not report PASS'
}

$remoteRunner = "/tmp/p11_rk3588_band_board_run_$runId.sh"
Invoke-BoardScp -Source $boardRunner -Destination $remoteRunner
$runnerHash = (Get-FileHash -LiteralPath $boardRunner -Algorithm SHA256).Hash.ToLowerInvariant()
$runnerVerifyTemplate = @'
set -eu
chmod 755 '{0}'
test "$(sha256sum '{0}' | cut -d' ' -f1)" = '{1}'
echo '[P11BoardRunner] result=PASS sha256={1} path={0}'
'@
$runnerVerifyCommand = $runnerVerifyTemplate -f $remoteRunner, $runnerHash
$runnerVerify = Invoke-BoardSsh -Command $runnerVerifyCommand -LogPath (Join-Path $OutputDirectory 'board_runner_verify.log')
if (-not ($runnerVerify.Lines -match '\[P11BoardRunner\] result=PASS')) {
    throw 'Uploaded board acceptance runner did not verify'
}

$overallRows = New-Object System.Collections.Generic.List[object]
$overallFailed = $false
foreach ($spec in $bands) {
    $bandName = [string]$spec.Name
    $protocolBand = [int]$spec.Protocol
    $caseDir = Join-Path $OutputDirectory $bandName
    $windowsAudit = Join-Path $caseDir 'windows_audit'
    New-Item -ItemType Directory -Force -Path $caseDir, $windowsAudit | Out-Null
    $remoteCase = "$BoardRoot/logs/P11_acceptance_$runId/$bandName/board"
    $remoteLog = "$remoteCase/hwa.log"
    $remoteScenarioEnvironment = "/tmp/p11_rk3588_dds_image_env_$runId`_$bandName.env"
    $caseErrors = New-Object System.Collections.Generic.List[string]
    $receiver = $null
    $stim = $null
    $boardOwner = $null
    $receiverExit = $null
    $stimExit = $null
    $boardOwnerExit = $null
    $analyzerExit = $null
    $caseBegin = [DateTime]::UtcNow
    Save-ControllerClock -Path (Join-Path $caseDir 'controller_clock_begin.txt') -Phase 'begin'

    $stimConfig = Join-Path $caseDir 'NetworkConfig_dds.ini'
    $stimConfigLines = @(
        '[Identity]',
        'channel=precise',
        'platID=1001',
        'sensorID=2',
        '',
        '[DdsProtocol]',
        'DomainId=150',
        # QSettings INI parsing treats backslashes as escape characters.  Use
        # an absolute forward-slash path so D:\H... is not read as D:H... with
        # tab escapes in the DDS runtime.
        "QosFile=$windowsQosIni",
        'TopicControl=HwaSimIR.Control',
        'TopicInit=HwaSimIR.Init',
        'TopicRealtime=HwaSimIR.Realtime',
        'TopicInitAck=HwaSimIR.InitAck',
        'TopicVideoStatus=HwaSimIR.VideoStatus'
    )
    [IO.File]::WriteAllText($stimConfig, ($stimConfigLines -join $newline) + $newline, $utf8NoBom)
    $scenarioEnvironmentPath = Join-Path $caseDir 'board_scenario_environment.env'
    $scenarioEnvironmentLines = @($BoardEnvironment.Keys | Sort-Object | ForEach-Object {
        '{0}={1}' -f [string]$_, [string]$BoardEnvironment[$_]
    })
    [IO.File]::WriteAllText($scenarioEnvironmentPath, ($scenarioEnvironmentLines -join "`n") + "`n", $utf8NoBom)
    $scenarioEnvironmentSha256 = (Get-FileHash -LiteralPath $scenarioEnvironmentPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-JsonFile -Path (Join-Path $caseDir 'case_request.json') -Value ([ordered]@{
        schema = 'hwasimir.p11.rk3588.band-request.v1'
        created_utc = $caseBegin.ToString('o')
        scenario_id = $ScenarioId
        capture_variant = $CaptureVariant
        band = $bandName
        protocol_band = $protocolBand
        duration_sec = $DurationSec
        presentation_mode = $PresentationMode
        target_key = @('0x55', 3101, 5501)
        target_fixture = $cameraInput
        target_fixture_sha256 = (Get-FileHash -LiteralPath $cameraInput -Algorithm SHA256).Hash.ToLowerInvariant()
        sensor_pixel_angle_urad = 10.0
        weather = 'clear'
        utc_hour = 6.0
        active_illuminator = $false
        transport = [ordered]@{
            kind = 'DDS'
            domain = 150
            control_topics = @('HwaSimIR.Control', 'HwaSimIR.Init', 'HwaSimIR.Realtime', 'HwaSimIR.InitAck', 'HwaSimIR.VideoStatus')
            video_topic = 'HwaSimIR.Video.1001.2.H264'
            codec = 'h264_annexb'
        }
        expected_elf_sha256 = $expectedElf
        expected_config_manifest_sha256 = $expectedManifest
        remote_case = $remoteCase
        raw_diagnostic_sequences = @($rawSeqValues)
        receiver_dump_frame_index = $ReceiverDumpFrameIndex
        stimulus_extra_args = @($StimExtraArgs)
        board_environment = $boardEnvironmentForJson
        board_environment_sha256 = $scenarioEnvironmentSha256
    })

    $environmentNames = @(
        'QT_FORCE_STDERR_LOGGING', 'HwaInputAuditDirectory', 'P5DdsVideoPath',
        'P5DdsVideoSamples', 'WeatherCameraInput', 'P5NoTargets', 'P6TestTargetType'
    )
    $savedEnvironment = @{}
    foreach ($name in $environmentNames) {
        $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    }

    try {
        $env:QT_FORCE_STDERR_LOGGING = '1'
        $env:HwaInputAuditDirectory = $windowsAudit
        $env:P5DdsVideoPath = Join-Path $caseDir 'received_annexb.h264'
        $env:P5DdsVideoSamples = [string]($DurationSec * 60)
        $env:WeatherCameraInput = $cameraInput
        $env:P5NoTargets = '0'
        $env:P6TestTargetType = '0x55'

        $receiverArguments = @(
            '--receive-transport=dds',
            '--stream-role=direct',
            '--channel=precise',
            '--plat-id=1001',
            '--sensor-id=2',
            '--dds-domain=150',
            '--dds-topic=HwaSimIR.Video.1001.2.H264',
            '--dds-codec=h264',
            '--dds-width=800',
            '--dds-height=800',
            '--dds-fps=60',
            "--dds-qos=$windowsQos",
            "--dds-dump-first-frame=$(Join-Path $caseDir 'received_decode.png')",
            "--dds-dump-frame-index=$ReceiverDumpFrameIndex",
            # The receiver timer must expire inside the bounded post-producer
            # wait below so Qt can run its normal destructor, flush the audit
            # CSV, and emit [DdsReceiverFinal].  The former +120 s timer could
            # never expire before the runner's 5 s + 30 s close deadline.
            "--acceptance-exit-ms=$(($DurationSec + 40) * 1000)"
        )
        $receiver = Start-Process -FilePath $receiverExe -WorkingDirectory (Split-Path -Parent $receiverExe) -ArgumentList $receiverArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $caseDir 'receiver.out.log') -RedirectStandardError (Join-Path $caseDir 'receiver.err.log')
        Start-Sleep -Seconds 3
        if ($receiver.HasExited) { throw "DDS receiver exited before board startup, exit=$($receiver.ExitCode)" }

        if ($scenarioOverridesRequested) {
            Invoke-BoardScp -Source $scenarioEnvironmentPath -Destination $remoteScenarioEnvironment
            $remoteCommand = "'$remoteRunner' '$remoteCase' '$bandName' '$protocolBand' '$DurationSec' '$expectedElf' '$expectedManifest' '$PresentationMode' '$RawSeqs' '$remoteScenarioEnvironment' '$scenarioEnvironmentSha256'"
        }
        else {
            # Preserve the original eight-argument board runner invocation for
            # ordinary 60 s acceptance; scenario behavior is opt-in only.
            $remoteCommand = "'$remoteRunner' '$remoteCase' '$bandName' '$protocolBand' '$DurationSec' '$expectedElf' '$expectedManifest' '$PresentationMode' '$RawSeqs'"
        }
        $boardOwner = Start-BoardOwner -RemoteCommand $remoteCommand -Stdout (Join-Path $caseDir 'board_ssh.out.log') -Stderr (Join-Path $caseDir 'board_ssh.err.log')
        if (-not (Wait-RemoteToken -RemoteLog $remoteLog -Token '[RunPreflight] result=PASS' -TimeoutSec 60)) {
            throw 'Board launcher did not reach RunPreflight PASS'
        }
        if (-not (Wait-RemoteToken -RemoteLog $remoteLog -Token '[DdsVideo] initialized=1' -TimeoutSec 45)) {
            throw 'Board renderer did not initialize the DDS H.264 publisher'
        }

        $stimArguments = @(
            "--network-config=$stimConfig",
            '--control-transport=dds',
            '--dds-discovery-wait-ms=12000',
            '--channel=precise',
            '--plat-id=1001',
            '--sensor-id=2',
            '--sim-mode=2',
            '--video-fps=60',
            '--send-step-ms=16.666667',
            '--phase1d-h264=1',
            '--save-mp4=0',
            "--duration-sec=$DurationSec",
            '--env-sky=0',
            "--sensor-band=$protocolBand",
            '--sensor-pixel-angle-urad=10',
            '--utc-hour=6',
            '--engine-state=1',
            '--strike-flag=0',
            '--illuminator-en=0',
            '--freeze-geometry'
        )
        $stimArguments += @($StimExtraArgs)
        $stim = Start-Process -FilePath $stimExe -WorkingDirectory (Split-Path -Parent $stimExe) -ArgumentList $stimArguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $caseDir 'stim.out.log') -RedirectStandardError (Join-Path $caseDir 'stim.err.log')
        if (-not $stim.WaitForExit(($DurationSec + 120) * 1000)) {
            throw 'DataDrivenTestQT exceeded the bounded acceptance timeout'
        }
        # Complete asynchronous stdout/stderr draining before reading ExitCode.
        # Without the no-argument wait, PowerShell can expose a null ExitCode
        # even after the bounded wait reported success.
        $stimExit = Get-ExitedProcessCode -Process $stim
        if ($null -eq $stimExit) { throw 'DataDrivenTestQT exit state unavailable after bounded wait' }
        if ($stimExit -ne 0) { throw "DataDrivenTestQT failed with exit $stimExit" }

        if (-not $boardOwner.WaitForExit(120000)) {
            Stop-OwnedBoardRun -RemoteCase $remoteCase
            $boardOwner.WaitForExit(15000) | Out-Null
            throw 'Board renderer did not finish STOP drain within 120 seconds'
        }
        $boardOwnerExit = Get-ExitedProcessCode -Process $boardOwner
        if ($null -eq $boardOwnerExit) { throw 'Board acceptance owner exit state unavailable after bounded wait' }
        if ($boardOwnerExit -ne 0) { throw "Board acceptance owner failed with exit $boardOwnerExit" }

        Start-Sleep -Seconds 5
        if (-not $receiver.HasExited) {
            # Wait for the acceptance timer and Qt shutdown path.  The receiver
            # is a hidden window, so CloseMainWindow cannot provide a reliable
            # graceful-exit signal and previously forced the outer cleanup to
            # kill it before its final ledger flush.
            if ($receiver.WaitForExit(30000)) {
                $receiver.WaitForExit()
            }
        }
        $receiverExit = Get-ExitedProcessCode -Process $receiver
        if ($null -ne $receiverExit) {
            if ($receiverExit -ne 0) { throw "DDS receiver failed with exit $receiverExit" }
        }
        else {
            throw 'DDS receiver did not close after the producer drain'
        }
    }
    catch {
        $caseErrors.Add($_.Exception.Message)
    }
    finally {
        if ($stim) {
            $stim.Refresh()
            if (-not $stim.HasExited) { Stop-Process -Id $stim.Id -Force -ErrorAction SilentlyContinue }
            $stim.Refresh()
            $observedExit = Get-ExitedProcessCode -Process $stim
            if ($null -ne $observedExit) { $stimExit = $observedExit }
        }
        if ($boardOwner) {
            $boardOwner.Refresh()
            # The remote SSH server may close the transport before a child
            # shell exits.  Always consult the board-side owner PID, even when
            # the local ssh.exe has already exited.
            Stop-OwnedBoardRun -RemoteCase $remoteCase
            if (-not $boardOwner.HasExited) {
                $boardOwner.WaitForExit(15000) | Out-Null
            }
            $boardOwner.Refresh()
            if (-not $boardOwner.HasExited) { Stop-Process -Id $boardOwner.Id -Force -ErrorAction SilentlyContinue }
            $boardOwner.Refresh()
            $observedExit = Get-ExitedProcessCode -Process $boardOwner
            if ($null -ne $observedExit) { $boardOwnerExit = $observedExit }
        }
        if ($receiver) {
            $receiver.Refresh()
            if (-not $receiver.HasExited) {
                $receiver.CloseMainWindow() | Out-Null
                $receiver.WaitForExit(15000) | Out-Null
            }
            $receiver.Refresh()
            if (-not $receiver.HasExited) { Stop-Process -Id $receiver.Id -Force -ErrorAction SilentlyContinue }
            $receiver.Refresh()
            $observedExit = Get-ExitedProcessCode -Process $receiver
            if ($null -ne $observedExit) { $receiverExit = $observedExit }
        }
        foreach ($name in $environmentNames) {
            [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
        }
        Save-ControllerClock -Path (Join-Path $caseDir 'controller_clock_end.txt') -Phase 'end'

        try {
            Invoke-BoardScp -Source $remoteCase -Destination $caseDir -FromBoard -Recurse
        }
        catch {
            $caseErrors.Add("Board evidence download failed: $($_.Exception.Message)")
        }
        $postflight = Invoke-BoardSsh -Command "set -eu; ! pgrep -x HwaSim_IR >/dev/null; echo '[P11AcceptancePostflight] result=PASS hwasimirStillRunning=0'" `
            -LogPath (Join-Path $caseDir 'board_postflight.log') -AllowFailure
        if ($postflight.ExitCode -ne 0 -or -not ($postflight.Lines -match '\[P11AcceptancePostflight\] result=PASS')) {
            $caseErrors.Add('Board postflight found a residual HwaSim_IR process')
        }
        Write-JsonFile -Path (Join-Path $caseDir 'runtime_status.json') -Value ([ordered]@{
            schema = 'hwasimir.p11.rk3588.band-runtime-status.v1'
            result = if ($caseErrors.Count -eq 0) { 'PROCESS_PASS' } else { 'PROCESS_FAIL' }
            errors = @($caseErrors)
            receiver_exit = $receiverExit
            stimulus_exit = $stimExit
            board_owner_exit = $boardOwnerExit
            begin_utc = $caseBegin.ToString('o')
            end_utc = [DateTime]::UtcNow.ToString('o')
        })

        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        & python.exe $analyzer --case-dir $caseDir --band $bandName --protocol-band $protocolBand --duration-sec $DurationSec --min-fps $MinFps --max-fps $MaxFps --raw-seqs $RawSeqs --receiver-dump-frame-index $ReceiverDumpFrameIndex 1> (Join-Path $caseDir 'analyzer.out.log') 2> (Join-Path $caseDir 'analyzer.err.log')
        $analyzerExit = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        if ($analyzerExit -ne 0) {
            $caseErrors.Add("Acceptance analyzer returned $analyzerExit")
        }
    }

    $casePassed = $caseErrors.Count -eq 0 -and $analyzerExit -eq 0
    if (-not $casePassed) { $overallFailed = $true }
    $overallRows.Add([ordered]@{
        band = $bandName
        protocol_band = $protocolBand
        result = if ($casePassed) { 'PASS' } else { 'FAIL' }
        errors = @($caseErrors)
        output = $caseDir
    })
    Write-Host "[P11 RK3588 Band] band=$bandName result=$(if ($casePassed) {'PASS'} else {'FAIL'}) output=$caseDir"
}

$overall = [ordered]@{
    schema = 'hwasimir.p11.rk3588.acceptance-summary.v1'
    result = if ($overallFailed) { 'FAIL' } else { 'PASS' }
    run_id = $runId
    deployment_stage_id = [string]$receipt.stage_id
    elf_sha256 = $expectedElf
    config_manifest_sha256 = $expectedManifest
    controller_completed_utc = [DateTime]::UtcNow.ToString('o')
    cases = @($overallRows | ForEach-Object { $_ })
}
Write-JsonFile -Path (Join-Path $OutputDirectory 'acceptance_overall.json') -Value $overall
Write-Host "[P11 RK3588 Acceptance] result=$($overall.result) output=$OutputDirectory"
if ($overallFailed) { exit 1 }
