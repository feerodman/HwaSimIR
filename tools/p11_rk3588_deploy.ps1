[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Deploy')]
    [string]$Mode = 'Plan',
    [string]$RepoRoot = '',
    [string]$ElfPath = '',
    [string]$BuildReceipt = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:P11_BOARD_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$StageId = '',
    [string]$OutputDirectory = '',
    [string]$ExpectedCurrentManifestSha256 = '',
    [string]$ExpectedRuntimeSha256 = '',
    [string]$ExpectedNetworkConfigSha256 = '',
    [switch]$ConfirmAtomicDeploy
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ($BoardRoot -ne '/userdata/HwaSimIR') { throw 'P11 deployment is restricted to /userdata/HwaSimIR' }
if (-not $StageId) { $StageId = 'p11-' + (Get-Date -Format 'yyyyMMdd-HHmmss') }
if ($StageId -notmatch '^p11-[0-9]{8}-[0-9]{6}$') { throw "Unsafe StageId: $StageId" }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot "logs\p11\rk3588\deploy-$StageId" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$configRoot = (Resolve-Path -LiteralPath (Join-Path $RepoRoot 'HwaSim_IR\Bin\Config')).Path
$launcherPath = Join-Path $RepoRoot 'tools\rk3588_run_hwasimir_precise.sh'
$performancePath = Join-Path $RepoRoot 'tools\rk3588_hwasimir_performance_mode.sh'
$deltaPlanner = Join-Path $RepoRoot 'tools\p11_rk3588_delta_plan.ps1'
$askPass = Join-Path $RepoRoot 'tools\p5_ssh_askpass.cmd'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

$requiredConfig = @(
    'HwaSimIRRuntime.ini',
    'SensorWave/default_SWIR.json',
    'SensorWave/default_MWIR.json',
    'SensorWave/default_NVG.json',
    'Materials/MaterialDatabase.csv',
    'Materials/MaterialBandOptics.csv',
    'Atmosphere/MODTRAN/processed/band_lut_si.csv',
    'Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv',
    'TargetLib/Targets.json',
    'TargetLib/p11/civil_van/LICENSE.txt',
    'TargetLib/p11/civil_van/manifest.json',
    'TargetLib/p11/civil_van/p11_civil_van.bam',
    'TargetLib/p11/civil_van/p11_civil_van_material_id.pgm',
    'TargetLib/p11/civil_van/p11_civil_van_material_id.pgm.xml',
    'TargetLib/p11/civil_van/p11_civil_van_band_optics.csv',
    'TargetLib/p11/controlled_samples/LICENSE.txt',
    'TargetLib/p11/controlled_samples/manifest.json',
    'TargetLib/p11/controlled_samples/p11_controlled_samples.bam',
    'TargetLib/p11/controlled_samples/p11_controlled_samples_material_id.pgm',
    'TargetLib/p11/controlled_samples/p11_controlled_samples_material_id.pgm.xml',
    'TargetLib/p11/controlled_samples/p11_controlled_samples_band_optics.csv',
    'IRPlume/engine_plume_profiles.json',
    'Annotation/annotation_profiles.json',
    'Weather/weather_profiles.json',
    'DDS/ZRDDS_QOS_PROFILES.xml',
    'DDS/ZRDDS_PROTOCOL_QOS.xml'
)
foreach ($path in @($launcherPath, $performancePath, $deltaPlanner, $askPass)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing deployment input: $path" }
}
foreach ($relative in $requiredConfig) {
    if (-not (Test-Path -LiteralPath (Join-Path $configRoot ($relative -replace '/', '\')))) {
        throw "Missing P11 runtime Config item: $relative"
    }
}
if ($Mode -eq 'Plan') {
    $plan = [ordered]@{
        schema = 'hwasimir.p11.rk3588.deploy-preflight.v1'
        created_utc = [DateTime]::UtcNow.ToString('o')
        stage_id = $StageId
        required_config_count = $requiredConfig.Count
        required_config_pass = $true
        board_root = $BoardRoot
        remote_strategy = 'verified_hard_link_snapshot_plus_delta_rename'
        full_config_copy = $false
        remote_modified = $false
    }
    [IO.File]::WriteAllText((Join-Path $OutputDirectory 'deploy_preflight.json'), ($plan | ConvertTo-Json -Depth 4) + "`n", $utf8NoBom)
    Write-Host "[P11 RK3588 DeployPlan] result=PASS requiredConfig=$($requiredConfig.Count) strategy=hard_link_delta remoteModified=0 output=$OutputDirectory"
    return
}
if (-not $ConfirmAtomicDeploy) { throw 'Deploy mode requires -ConfirmAtomicDeploy from the coordinating agent' }
if ($ExpectedRuntimeSha256 -notmatch '^[0-9a-fA-F]{64}$' -or
    $ExpectedNetworkConfigSha256 -notmatch '^[0-9a-fA-F]{64}$') {
    throw 'Deploy mode requires explicit ExpectedRuntimeSha256 and ExpectedNetworkConfigSha256 freeze hashes'
}
$ExpectedRuntimeSha256 = $ExpectedRuntimeSha256.ToLowerInvariant()
$ExpectedNetworkConfigSha256 = $ExpectedNetworkConfigSha256.ToLowerInvariant()
$runtimePath = Join-Path $configRoot 'HwaSimIRRuntime.ini'
$networkConfigPath = Join-Path $configRoot 'NetworkConfig.ini'
$runtimePreHash = (Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash.ToLowerInvariant()
$networkPreHash = (Get-FileHash -LiteralPath $networkConfigPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($runtimePreHash -ne $ExpectedRuntimeSha256 -or $networkPreHash -ne $ExpectedNetworkConfigSha256) {
    throw "Frozen Config preflight mismatch: runtime=$runtimePreHash network=$networkPreHash"
}
foreach ($required in @($ElfPath, $BuildReceipt)) {
    if (-not $required -or -not (Test-Path -LiteralPath $required)) { throw "Missing build artifact: $required" }
}
$ElfPath = (Resolve-Path -LiteralPath $ElfPath).Path
$BuildReceipt = (Resolve-Path -LiteralPath $BuildReceipt).Path
$receipt = Get-Content -Raw -LiteralPath $BuildReceipt | ConvertFrom-Json
if ($receipt.schema -ne 'hwasimir.p11.rk3588.build-receipt.v1') { throw 'Unexpected build receipt schema' }
$elfHash = (Get-FileHash -LiteralPath $ElfPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($elfHash -ne [string]$receipt.elf_sha256) { throw 'ELF hash does not match the isolated VM build receipt' }
$buildId = [string]$receipt.build_id
if ($buildId -notmatch '^[0-9a-f]+$') { throw 'Invalid Build ID in build receipt' }

function Invoke-Native {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')" }
}

function Get-SshArguments {
    $result = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if (-not $BoardPassword) {
        if (-not $SshKey -or -not (Test-Path -LiteralPath $SshKey)) { throw 'Neither board password nor readable SSH key is available' }
        $result += @('-i', $SshKey, '-o', 'BatchMode=yes')
    }
    return $result
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
            $env:DISPLAY = 'p11-rk3588-deploy'
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
    param([string]$Command, [string]$LogPath = '')
    $output = Use-BoardAuthentication {
        $arguments = @(Get-SshArguments)
        if ($BoardPassword) { $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password') }
        $arguments += @("$BoardUser@$BoardHost", $Command)
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $lines = & ssh.exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        [pscustomobject]@{ Lines = @($lines); ExitCode = $exitCode }
    }
    if ($LogPath) { [IO.File]::WriteAllText($LogPath, ($output.Lines -join "`n") + "`n", $utf8NoBom) }
    if ($output.ExitCode -ne 0) { throw "Board command failed ($($output.ExitCode)): $Command" }
    return @($output.Lines)
}

function Invoke-BoardScp {
    param([string]$Source, [string]$Destination, [switch]$FromBoard)
    Use-BoardAuthentication {
        $arguments = @(Get-SshArguments)
        if ($BoardPassword) { $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password') }
        if ($FromBoard) { $arguments += @("$BoardUser@$BoardHost`:$Source", $Destination) }
        else { $arguments += @($Source, "$BoardUser@$BoardHost`:$Destination") }
        Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
    }
}

$remoteManifest = Join-Path $OutputDirectory 'board_manifest_before.sha256'
Invoke-BoardScp -Source "$BoardRoot/Config/deployment_manifest.sha256" -Destination $remoteManifest -FromBoard
$currentManifestHash = (Get-FileHash -LiteralPath $remoteManifest -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ExpectedCurrentManifestSha256 -and $currentManifestHash -ne $ExpectedCurrentManifestSha256.ToLowerInvariant()) {
    throw "Board Config changed since inventory: expected=$ExpectedCurrentManifestSha256 actual=$currentManifestHash"
}
$remoteFacts = Invoke-BoardSsh -Command "set -eu; test ! -e '$BoardRoot/.deployment_in_progress'; test ! -e '$BoardRoot/.retention_in_progress'; test ! -e '$BoardRoot/.p11_rollback_in_progress'; ! pgrep -x HwaSim_IR >/dev/null; test `$(sha256sum '$BoardRoot/Config/deployment_manifest.sha256' | awk '{print `$1}') = '$currentManifestHash'; (cd '$BoardRoot/Config' && sha256sum -c deployment_manifest.sha256 >/dev/null); printf 'RemoteFreeBytes='; df -PB1 '$BoardRoot' | awk 'NR==2 {print `$4}'; printf 'BoardUtc='; date -u +%Y-%m-%dT%H:%M:%SZ" -LogPath (Join-Path $OutputDirectory 'board_preflight.log')
$freeLine = $remoteFacts | Where-Object { $_ -match '^RemoteFreeBytes=[0-9]+$' } | Select-Object -Last 1
if (-not $freeLine) { throw 'Board preflight did not report free space' }
$remoteFreeBytes = [Int64](($freeLine -split '=', 2)[1])

$deltaDirectory = Join-Path $OutputDirectory 'delta-plan'
& $deltaPlanner -RepoRoot $RepoRoot -ConfigRoot $configRoot -BoardManifest $remoteManifest `
    -OutputDirectory $deltaDirectory -RemoteFreeBytes $remoteFreeBytes
$deltaPlan = Get-Content -Raw -LiteralPath (Join-Path $deltaDirectory 'delta_plan.json') | ConvertFrom-Json
if (-not $deltaPlan.projected_space_pass -or $deltaPlan.full_config_copy_required) { throw 'Delta plan violates P11 space/snapshot constraints' }

$candidateManifest = Join-Path $deltaDirectory 'candidate_manifest.sha256'
$manifestHash = (Get-FileHash -LiteralPath $candidateManifest -Algorithm SHA256).Hash.ToLowerInvariant()
$candidateHashByPath = @{}
foreach ($line in [IO.File]::ReadAllLines($candidateManifest)) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') { throw "Malformed candidate manifest line: $line" }
    $candidateHashByPath[$Matches[2]] = $Matches[1]
}
if ($candidateHashByPath['HwaSimIRRuntime.ini'] -ne $ExpectedRuntimeSha256 -or
    $candidateHashByPath['NetworkConfig.ini'] -ne $ExpectedNetworkConfigSha256) {
    throw 'Candidate manifest does not match the explicitly frozen Runtime/NetworkConfig hashes'
}
$runtimeHash = $runtimePreHash
$networkConfigHash = $networkPreHash
$launcherHash = (Get-FileHash -LiteralPath $launcherPath -Algorithm SHA256).Hash.ToLowerInvariant()
$performanceHash = (Get-FileHash -LiteralPath $performancePath -Algorithm SHA256).Hash.ToLowerInvariant()
$gitCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
$treeState = if (@(& git -C $RepoRoot status --short).Count -eq 0) { 'clean' } else { 'dirty' }
$metadataDirectory = Join-Path $OutputDirectory 'metadata'
New-Item -ItemType Directory -Force -Path $metadataDirectory | Out-Null
Copy-Item -LiteralPath $candidateManifest -Destination (Join-Path $metadataDirectory 'deployment_manifest.sha256') -Force
$versionLines = @(
    'SchemaVersion=1',
    "GitCommit=$gitCommit",
    "WorkingTreeState=$treeState",
    "SourceIdentity=$gitCommit-$treeState",
    "P11BuildReceiptSha256=$((Get-FileHash -LiteralPath $BuildReceipt -Algorithm SHA256).Hash.ToLowerInvariant())",
    "P11SourceManifestSha256=$($receipt.source_manifest_sha256)",
    "ElfSha256=$elfHash",
    "BuildId=$buildId",
    "RuntimeConfigSha256=$runtimeHash",
    "NetworkConfigSha256=$networkConfigHash",
    "ConfigManifestSha256=$manifestHash",
    "LauncherSha256=$launcherHash",
    "PerformanceToolSha256=$performanceHash",
    "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
)
[IO.File]::WriteAllText((Join-Path $metadataDirectory 'deployment_version.env'), ($versionLines -join "`n") + "`n", $utf8NoBom)

$deltaList = Join-Path $deltaDirectory 'delta_files.txt'
$removedList = Join-Path $deltaDirectory 'removed_files.txt'
$changedPaths = @([IO.File]::ReadAllLines($deltaList) | Where-Object { $_ })
$deltaArchive = Join-Path $OutputDirectory "Config.delta.$StageId.tgz"
if ($changedPaths.Count -gt 0) {
    Invoke-Native -FilePath 'tar.exe' -Arguments @('-czf', $deltaArchive, '-C', $configRoot, '-T', $deltaList)
}
else {
    # A valid empty gzip tar keeps the deployment path uniform.
    Invoke-Native -FilePath 'tar.exe' -Arguments @('-czf', $deltaArchive, '-C', $configRoot, '--files-from=NUL')
}
$metadataArchive = Join-Path $OutputDirectory "Config.metadata.$StageId.tgz"
Invoke-Native -FilePath 'tar.exe' -Arguments @('-czf', $metadataArchive, '-C', $metadataDirectory, 'deployment_manifest.sha256', 'deployment_version.env')

# The Windows evidence matrix temporarily rewrites these two shared files.  A
# second read after both archives are closed ensures the tar payload and
# candidate manifest were produced inside one explicit Config freeze window.
$runtimePostHash = (Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash.ToLowerInvariant()
$networkPostHash = (Get-FileHash -LiteralPath $networkConfigPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($runtimePostHash -ne $ExpectedRuntimeSha256 -or $networkPostHash -ne $ExpectedNetworkConfigSha256) {
    throw "Frozen Config changed while packaging: runtime=$runtimePostHash network=$networkPostHash"
}

$remoteElfNew = "$BoardRoot/HwaSim_IR.new_$StageId"
$remoteLauncherNew = "$BoardRoot/run_precise.sh.new_$StageId"
$remotePerformanceNew = "$BoardRoot/rk3588_hwasimir_performance_mode.sh.new_$StageId"
$remoteConfigNew = "$BoardRoot/Config.new_$StageId"
$remoteDeltaDirectory = "$BoardRoot/Config.delta_$StageId"
$remoteMetadataDirectory = "$BoardRoot/Config.metadata_$StageId"
$remoteConfigBackup = "$BoardRoot/Config.before_$StageId"
$remoteElfBackup = "$BoardRoot/HwaSim_IR.before_$StageId"
$remoteLauncherBackup = "$BoardRoot/run_precise.sh.before_$StageId"
$remotePerformanceBackup = "$BoardRoot/rk3588_hwasimir_performance_mode.sh.before_$StageId"
$remoteDeltaArchive = "/tmp/HwaSimIR.Config.delta.$StageId.tgz"
$remoteMetadataArchive = "/tmp/HwaSimIR.Config.metadata.$StageId.tgz"
$remoteDeltaList = "/tmp/HwaSimIR.Config.delta.$StageId.txt"
$remoteRemovedList = "/tmp/HwaSimIR.Config.removed.$StageId.txt"

$begin = "set -eu; cd '$BoardRoot'; test ! -e '.deployment_in_progress'; test ! -e 'Config.new_$StageId'; test ! -e 'Config.before_$StageId'; test ! -e 'HwaSim_IR.before_$StageId'; mkdir '.deployment_in_progress'; printf '[P11DeployBegin] boardUtc='; date -u +%Y-%m-%dT%H:%M:%SZ; echo ' controllerUtc=$([DateTime]::UtcNow.ToString('o')) stageId=$StageId currentManifest=$currentManifestHash'"
Invoke-BoardSsh -Command $begin -LogPath (Join-Path $OutputDirectory 'deploy_begin.log') | Out-Null
Invoke-BoardScp -Source $ElfPath -Destination $remoteElfNew
Invoke-BoardScp -Source $launcherPath -Destination $remoteLauncherNew
Invoke-BoardScp -Source $performancePath -Destination $remotePerformanceNew
Invoke-BoardScp -Source $deltaArchive -Destination $remoteDeltaArchive
Invoke-BoardScp -Source $metadataArchive -Destination $remoteMetadataArchive
Invoke-BoardScp -Source $deltaList -Destination $remoteDeltaList
Invoke-BoardScp -Source $removedList -Destination $remoteRemovedList

$stageCommand = @"
set -eu
chmod 755 '$remoteElfNew' '$remoteLauncherNew' '$remotePerformanceNew'
test `$(sha256sum '$remoteElfNew' | awk '{print `$1}') = '$elfHash'
cp -al '$BoardRoot/Config' '$remoteConfigNew'
mkdir '$remoteDeltaDirectory' '$remoteMetadataDirectory'
tar -xzf '$remoteDeltaArchive' -C '$remoteDeltaDirectory'
while IFS= read -r rel; do
    [ -n "`$rel" ] || continue
    case "`$rel" in /*|../*|*/../*) exit 41;; esac
    parent=`$(dirname -- "`$rel")
    mkdir -p '$remoteConfigNew'/"`$parent"
    mv -f '$remoteDeltaDirectory'/"`$rel" '$remoteConfigNew'/"`$rel"
done < '$remoteDeltaList'
while IFS= read -r rel; do
    [ -n "`$rel" ] || continue
    case "`$rel" in /*|../*|*/../*) exit 42;; esac
    rm -f -- '$remoteConfigNew'/"`$rel"
done < '$remoteRemovedList'
tar -xzf '$remoteMetadataArchive' -C '$remoteMetadataDirectory'
mv -f '$remoteMetadataDirectory/deployment_manifest.sha256' '$remoteConfigNew/deployment_manifest.sha256'
mv -f '$remoteMetadataDirectory/deployment_version.env' '$remoteConfigNew/deployment_version.env'
rmdir '$remoteMetadataDirectory'
find '$remoteDeltaDirectory' -mindepth 1 -depth -type d -empty -delete
test -z "`$(find '$remoteDeltaDirectory' -mindepth 1 -print -quit)"
rmdir '$remoteDeltaDirectory'
cd '$remoteConfigNew'
sha256sum -c deployment_manifest.sha256 >/dev/null
test `$(sha256sum deployment_manifest.sha256 | awk '{print `$1}') = '$manifestHash'
echo '[P11DeployStage] result=PASS changed=$($deltaPlan.changed_file_count) removed=$($deltaPlan.removed_file_count) deltaBytes=$($deltaPlan.delta_payload_bytes) hardLinkSnapshot=1 fullConfigCopy=0 manifest=$manifestHash'
"@
Invoke-BoardSsh -Command $stageCommand -LogPath (Join-Path $OutputDirectory 'deploy_stage.log') | Out-Null

$remoteRequired = ($requiredConfig | ForEach-Object { "test -f '$remoteConfigNew/$_'" }) -join '; '
$verifyCommand = "set -eu; $remoteRequired; test -f '$remoteConfigNew/NetworkConfig_precise.ini'; test -f '$remoteConfigNew/DDS/ZRDDS_QOS_RK3588_192.168.1.116.xml'; echo '[P11DeployRequired] result=PASS count=$($requiredConfig.Count) boardIdentityPreserved=1'"
Invoke-BoardSsh -Command $verifyCommand -LogPath (Join-Path $OutputDirectory 'deploy_required.log') | Out-Null

$switchCommand = "set -eu; cd '$BoardRoot'; ! pgrep -x HwaSim_IR >/dev/null; cp -p HwaSim_IR '$remoteElfBackup'; cp -p run_precise.sh '$remoteLauncherBackup'; cp -p rk3588_hwasimir_performance_mode.sh '$remotePerformanceBackup'; mv Config '$remoteConfigBackup'; if mv '$remoteConfigNew' Config && mv '$remoteElfNew' HwaSim_IR && mv '$remoteLauncherNew' run_precise.sh && mv '$remotePerformanceNew' rk3588_hwasimir_performance_mode.sh; then chmod 755 HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh; echo '[P11DeploySwitch] result=PASS backup=$StageId'; else echo '[P11DeploySwitch] result=FAIL action=recover' >&2; [ ! -e Config ] || mv Config 'Config.failed_$StageId'; [ ! -e '$remoteConfigBackup' ] || mv '$remoteConfigBackup' Config; cp -p '$remoteElfBackup' HwaSim_IR; cp -p '$remoteLauncherBackup' run_precise.sh; cp -p '$remotePerformanceBackup' rk3588_hwasimir_performance_mode.sh; exit 51; fi"
Invoke-BoardSsh -Command $switchCommand -LogPath (Join-Path $OutputDirectory 'deploy_switch.log') | Out-Null

$finalCommand = "set -eu; cd '$BoardRoot'; test `$(sha256sum HwaSim_IR | awk '{print `$1}') = '$elfHash'; test `$(sha256sum run_precise.sh | awk '{print `$1}') = '$launcherHash'; test `$(sha256sum rk3588_hwasimir_performance_mode.sh | awk '{print `$1}') = '$performanceHash'; test `$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}') = '$manifestHash'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); test -d '$remoteConfigBackup'; test -f '$remoteElfBackup'; printf '[P11DeployFinal] result=PASS boardUtc='; date -u +%Y-%m-%dT%H:%M:%SZ; echo ' controllerUtc=$([DateTime]::UtcNow.ToString('o')) stageId=$StageId elfSha256=$elfHash buildId=$buildId configManifestSha256=$manifestHash rollbackRetained=1'; rmdir '.deployment_in_progress'"
$finalOutput = Invoke-BoardSsh -Command $finalCommand -LogPath (Join-Path $OutputDirectory 'deployment_final.log')
if (-not ($finalOutput -match '\[P11DeployFinal\] result=PASS')) { throw 'Final board identity verification did not report PASS' }

$deploymentReceipt = [ordered]@{
    schema = 'hwasimir.p11.rk3588.deployment-receipt.v1'
    controller_utc = [DateTime]::UtcNow.ToString('o')
    stage_id = $StageId
    board_host = $BoardHost
    board_root = $BoardRoot
    elf_sha256 = $elfHash
    build_id = $buildId
    config_manifest_sha256 = $manifestHash
    runtime_config_sha256 = $runtimeHash
    network_config_sha256 = $networkConfigHash
    previous_config_manifest_sha256 = $currentManifestHash
    changed_file_count = [int]$deltaPlan.changed_file_count
    removed_file_count = [int]$deltaPlan.removed_file_count
    delta_payload_bytes = [Int64]$deltaPlan.delta_payload_bytes
    full_config_copy = $false
    hard_link_snapshot = $true
    rollback_suffix = ".before_$StageId"
    rollback_retained = $true
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'deployment_receipt.json'), ($deploymentReceipt | ConvertTo-Json -Depth 4) + "`n", $utf8NoBom)
Write-Host "[P11 RK3588 Deploy] result=PASS stageId=$StageId elfSha256=$elfHash manifest=$manifestHash rollback=.before_$StageId output=$OutputDirectory"
