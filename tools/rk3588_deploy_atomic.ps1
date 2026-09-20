[CmdletBinding()]
param(
    [string]$ElfPath = '',
    [string]$RepoRoot = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$SshKey = '',
    [string]$LogDirectory = '',
    [switch]$ReuseVerifiedConfig,
    [switch]$ReuseVerifiedFiles,
    [switch]$FinalizeRetention,
    [string]$AcceptanceReceipt = '',
    [string]$RollbackVersion = '',
    [string]$ResumeStaging = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
if($env:HWASIMIR_SSH_PASSWORD -and !$SshKey){
    $env:SSH_ASKPASS=Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
    $env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='hwasimir_deploy'
}

function Invoke-Native {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')"
    }
}

function New-SshArguments {
    $arguments = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if ($SshKey) { $arguments += @('-i', $SshKey, '-o', 'BatchMode=yes') }
    return $arguments
}

function Invoke-Ssh {
    param([string]$Command)
    $arguments = @(New-SshArguments) + @("$BoardUser@$BoardHost", $Command)
    & ssh.exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "Board command failed ($LASTEXITCODE): $Command" }
}

function Invoke-SshCapture {
    param([string]$Command)
    $arguments = @(New-SshArguments) + @("$BoardUser@$BoardHost", $Command)
    $output = @(& ssh.exe @arguments)
    if ($LASTEXITCODE -ne 0) { throw "Board command failed ($LASTEXITCODE): $Command" }
    return ($output -join "`n").Trim()
}

function Invoke-Scp {
    param([string]$Source, [string]$Destination)
    $arguments = @(New-SshArguments) + @($Source, "$BoardUser@$BoardHost`:$Destination")
    Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
}

function Get-RelativeUnixPath {
    param([string]$Root, [string]$Path)
    $rootUri = New-Object Uri(($Root.TrimEnd('\') + '\'))
    $pathUri = New-Object Uri($Path)
    return [Uri]::UnescapeDataString($rootUri.MakeRelativeUri($pathUri).ToString())
}

if($BoardRoot -ne '/userdata/HwaSimIR'){throw 'Deployment/retention is restricted to the named board workspace'}
if($FinalizeRetention){
    if(!$AcceptanceReceipt){throw 'Retention requires an actual short-test PASS receipt identifying the deployed ELF and Config manifest'}
    if($RollbackVersion -and $RollbackVersion -notmatch '^\d{8}-\d{6}$'){throw 'Invalid rollback version'}
    if(!$LogDirectory){$LogDirectory=Join-Path $RepoRoot 'logs/p10/retention'}
    New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
    Invoke-Scp (Join-Path $PSScriptRoot 'rk3588_retain_versions.py') "$BoardRoot/logs/retain_versions.py"
    Invoke-Scp (Resolve-Path -LiteralPath $AcceptanceReceipt).Path "$BoardRoot/logs/retention_acceptance.json"
    $retentionSha=(Get-FileHash (Join-Path $PSScriptRoot 'rk3588_retain_versions.py')).Hash.ToLowerInvariant()
    $receiptSha=(Get-FileHash (Resolve-Path -LiteralPath $AcceptanceReceipt).Path).Hash.ToLowerInvariant()
    $actualTool=Invoke-SshCapture "sha256sum '$BoardRoot/logs/retain_versions.py' | cut -d ' ' -f 1"
    $actualReceipt=Invoke-SshCapture "sha256sum '$BoardRoot/logs/retention_acceptance.json' | cut -d ' ' -f 1"
    if($actualTool -ne $retentionSha -or $actualReceipt -ne $receiptSha){throw 'Retention tool or receipt transfer hash mismatch'}
    $selection=if($RollbackVersion){"--rollback '$RollbackVersion'"}else{''}
    Invoke-Ssh "python3 '$BoardRoot/logs/retain_versions.py' --root '$BoardRoot' --apply --receipt '$BoardRoot/logs/retention_acceptance.json' $selection --output '$BoardRoot/logs/retention_result.json'"
    $arguments=@(New-SshArguments)+@("${BoardUser}@${BoardHost}:$BoardRoot/logs/retention_result.json",(Join-Path $LogDirectory 'retention_result.json'))
    Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
    return
}
$elf = (Resolve-Path -LiteralPath $ElfPath).Path
$assetPython=if(Test-Path 'F:\Programs\anaconda3\python.exe'){'F:\Programs\anaconda3\python.exe'}else{'python'}
Invoke-Native -FilePath $assetPython -Arguments @((Join-Path $PSScriptRoot 'p10_prepare_portable_models.py'))
$configSource = Join-Path $RepoRoot 'HwaSim_IR\Bin\Config'
$launcherSource = Join-Path $RepoRoot 'tools\rk3588_run_hwasimir_precise.sh'
$performanceSource = Join-Path $RepoRoot 'tools\rk3588_hwasimir_performance_mode.sh'
foreach ($required in @($elf, $configSource, $launcherSource, $performanceSource)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required deployment source missing: $required" }
}

if($ResumeStaging -and $ResumeStaging -notmatch '^\d{8}-\d{6}$'){throw 'Invalid resume version'}
$stamp = if($ResumeStaging){$ResumeStaging}else{Get-Date -Format 'yyyyMMdd-HHmmss'}
if (-not $LogDirectory) { $LogDirectory = Join-Path $RepoRoot "logs\rk3588-deploy-$stamp" }
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$LogDirectory = (Resolve-Path -LiteralPath $LogDirectory).Path
$stageRoot = Join-Path $LogDirectory 'stage'
$stageConfig = Join-Path $stageRoot 'Config'
New-Item -ItemType Directory -Force -Path $stageConfig | Out-Null
if(!$ResumeStaging){Copy-Item -Path (Join-Path $configSource '*') -Destination $stageConfig -Recurse -Force}
elseif (-not (Test-Path -LiteralPath (Join-Path $stageConfig 'deployment_manifest.sha256'))) {throw 'Resume requires the original local complete stage'}
$boardBoundQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_QOS_RK3588_192.168.1.116.xml'
if (Test-Path -LiteralPath $boardBoundQos) {
    Copy-Item -LiteralPath $boardBoundQos -Destination (Join-Path $stageConfig 'DDS') -Force
}

# Machine identity/address files belong to the board. Stage those exact files
# before hashing the package; never replace them from a workstation Config.
foreach ($machineFile in @('NetworkConfig_precise.ini','NetworkConfig_search.ini')) {
    $exists = Invoke-SshCapture "if test -f '$BoardRoot/Config/$machineFile'; then echo yes; else echo no; fi"
    if ($exists -eq 'yes') {
        $arguments = @(New-SshArguments) + @("$BoardUser@$BoardHost`:$BoardRoot/Config/$machineFile", (Join-Path $stageConfig $machineFile))
        Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
    }
}

$requiredConfig = @(
    'HwaSimIRRuntime.ini',
    'NetworkConfig_precise.ini',
    'DDS/ZRDDS_QOS_PROFILES.xml',
    'Materials/MaterialDatabase.csv',
    'Materials/MaterialBandOptics.csv',
    'Atmosphere/MODTRAN/processed/band_lut_si.csv',
    'Atmosphere/MODTRAN/processed/p13_coverage_manifest.json',
    'Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv',
    'SensorWave/default_NVG.json',
    'SensorWave/default_MWIR.json',
    'TargetLib/Targets.json',
    'Weather/weather_profiles.json',
    'Weather/weather_textures.json',
    'GameVFX/soft_sprite_atlas.png',
    'GameVFX/sprite.vert',
    'GameVFX/sprite.frag',
    'GameVFX/ordinary_nozzle.json',
    'GameVFX/nozzle_attachments.json',
    'Weather/game_environment.json',
    'Weather/Textures/cloud_scattered.png',
    'Weather/game_background.frag',
    'Weather/Textures/cloud_overcast.png',
    'Weather/Textures/cloud3d_001.png',
    'Weather/Textures/cloud3d_002.png',
    'Weather/Textures/cloud3d_014.png',
    'Weather/Textures/cloud3d_020.png',
    'Tests/P6/fullscreen.vert',
    'Tests/P6/display.frag',
    'SensorWave/Archive/P5/default_NVG.json',
    'SensorWave/Archive/P5/default_MWIR.json',
    'IRHotspots/target_hotspots.json',
    'IRRadiance/stage5_debug_display.json',
    'IRPlume/engine_plume_profiles.json',
    'Annotation/annotation_profiles.json'
)
foreach ($relative in $requiredConfig) {
    if (-not (Test-Path -LiteralPath (Join-Path $stageConfig ($relative -replace '/', '\')))) {
        throw "Required runtime Config item missing: $relative"
    }
}

# Atomic marker excludes concurrent deploy/retention; a failed deployment keeps
# it in place until its incomplete switch has been inspected and recovered.
if(!$ResumeStaging){Invoke-Ssh "set -eu; test ! -e '$BoardRoot/.retention_in_progress'; mkdir '$BoardRoot/.deployment_in_progress'; test ! -e '$BoardRoot/.retention_in_progress'"}
else{Invoke-Ssh "set -eu; test -d '$BoardRoot/.deployment_in_progress'; test ! -e '$BoardRoot/.retention_in_progress'; test -d '$BoardRoot/Config.new_$stamp'; test ! -e '$BoardRoot/Config.before_$stamp'"}
$boardElfNew = "$BoardRoot/HwaSim_IR.new_$stamp"
$boardLauncherNew = "$BoardRoot/run_precise.sh.new_$stamp"
$boardPerformanceNew = "$BoardRoot/rk3588_hwasimir_performance_mode.sh.new_$stamp"
Invoke-Scp $elf $boardElfNew
Invoke-Scp $launcherSource $boardLauncherNew
Invoke-Scp $performanceSource $boardPerformanceNew
Invoke-Ssh "chmod 755 '$boardElfNew' '$boardLauncherNew' '$boardPerformanceNew'"

$elfSha = (Get-FileHash -LiteralPath $elf -Algorithm SHA256).Hash.ToLowerInvariant()
$uploadedElfSha = Invoke-SshCapture "sha256sum '$boardElfNew' | awk '{print `$1}'"
if ($uploadedElfSha -ne $elfSha) { throw 'Uploaded ELF does not match the local build' }
$buildId = Invoke-SshCapture "readelf -n '$boardElfNew' | awk '/Build ID:/ {print `$3; exit}'"
$launcherSha = (Get-FileHash -LiteralPath $launcherSource -Algorithm SHA256).Hash.ToLowerInvariant()
$performanceSha = (Get-FileHash -LiteralPath $performanceSource -Algorithm SHA256).Hash.ToLowerInvariant()
$runtimeSha = (Get-FileHash -LiteralPath (Join-Path $stageConfig 'HwaSimIRRuntime.ini') -Algorithm SHA256).Hash.ToLowerInvariant()
$gitCommit = (& git -C $RepoRoot rev-parse HEAD).Trim()
$treeState = if ((& git -C $RepoRoot status --porcelain).Count -eq 0) { 'clean' } else { 'dirty' }
$sourceIdentity = "$gitCommit-$treeState"

$manifestLines = New-Object System.Collections.Generic.List[string]
Get-ChildItem -LiteralPath $stageConfig -Recurse -File |
    Where-Object { $_.Name -notin @('deployment_manifest.sha256', 'deployment_version.env') } |
    Sort-Object FullName |
    ForEach-Object {
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $relative = Get-RelativeUnixPath -Root $stageConfig -Path $_.FullName
        $manifestLines.Add("$hash  $relative")
    }
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$manifestPath = Join-Path $stageConfig 'deployment_manifest.sha256'
[IO.File]::WriteAllText($manifestPath, (($manifestLines -join "`n") + "`n"), $utf8NoBom)
$manifestSha = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$versionPath = Join-Path $stageConfig 'deployment_version.env'
$versionLines = @(
    'SchemaVersion=1',
    "GitCommit=$gitCommit",
    "WorkingTreeState=$treeState",
    "SourceIdentity=$sourceIdentity",
    "ElfSha256=$elfSha",
    "BuildId=$buildId",
    "RuntimeConfigSha256=$runtimeSha",
    "ConfigManifestSha256=$manifestSha",
    "LauncherSha256=$launcherSha",
    "PerformanceToolSha256=$performanceSha",
    "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
)
[IO.File]::WriteAllText($versionPath, (($versionLines -join "`n") + "`n"), $utf8NoBom)

$archive = Join-Path $LogDirectory "Config.$stamp.tgz"
$reuseIdenticalConfig = $false
$reuseFiles = [bool]$ReuseVerifiedFiles
$changedPaths = @()
$removedPaths = @()
if ($reuseFiles) {
    # Validate the old complete release before reusing any inode.
    Invoke-Ssh "cd '$BoardRoot/Config' && sha256sum -c deployment_manifest.sha256 >/dev/null"
    $oldManifestPath=Join-Path $LogDirectory 'previous_manifest.sha256'
    $remoteOldManifest="${BoardUser}@${BoardHost}:$BoardRoot/Config/deployment_manifest.sha256"
    $arguments=@(New-SshArguments)+@($remoteOldManifest,$oldManifestPath)
    Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
    $old=@{}
    foreach($line in [IO.File]::ReadAllLines($oldManifestPath)) {
        if($line -match '^([0-9a-f]{64})  (.+)$'){$old[$Matches[2]]=$Matches[1]}
    }
    $new=@{}
    foreach($line in $manifestLines) {
        if($line -match '^([0-9a-f]{64})  (.+)$') {
            $relative=$Matches[2];$hash=$Matches[1]
            if($relative.StartsWith('/') -or $relative -match '(^|/)\.\.(/|$)' -or $relative.Contains("'")){throw "Unsafe manifest path: $relative"}
            $new[$relative]=$hash
            if(-not $old.ContainsKey($relative) -or $old[$relative] -ne $hash){$changedPaths+=$relative}
        }
    }
    foreach($relative in $old.Keys) {
        if(-not $new.ContainsKey($relative)){
            if($relative.StartsWith('/') -or $relative -match '(^|/)\.\.(/|$)' -or $relative.Contains("'")){throw 'Unsafe removed path'}
            $removedPaths+=$relative
        }
    }
    $changedPaths+=@('deployment_manifest.sha256','deployment_version.env')
    $deltaList=Join-Path $LogDirectory 'delta_files.txt'
    [IO.File]::WriteAllText($deltaList,(($changedPaths -join "`n")+"`n"),$utf8NoBom)
    $removedList=Join-Path $LogDirectory 'removed_files.txt'
    [IO.File]::WriteAllText($removedList,($removedPaths -join "`n"),$utf8NoBom)
    Invoke-Native -FilePath 'tar.exe' -Arguments @('-C',$stageConfig,'-czf',$archive,'-T',$deltaList)
} elseif ($ReuseVerifiedConfig) {
    $existingManifestSha = Invoke-SshCapture "sha256sum '$BoardRoot/Config/deployment_manifest.sha256' | awk '{print `$1}'"
    if ($existingManifestSha -ne $manifestSha) { throw 'ReuseVerifiedConfig requires identical complete Config contents' }
    Invoke-Ssh "cd '$BoardRoot/Config' && sha256sum -c deployment_manifest.sha256 >/dev/null"
    $reuseIdenticalConfig = $true
} else {
    Invoke-Native -FilePath 'tar.exe' -Arguments @('-C', $stageRoot, '-czf', $archive, 'Config')
}
$configBytes = (Get-ChildItem -LiteralPath $stageConfig -Recurse -File | Measure-Object -Property Length -Sum).Sum
$remoteFreeBytes = [Int64](Invoke-SshCapture "df -PB1 '$BoardRoot' | awk 'NR==2 {print `$4}'")
$safetyBytes = 256MB
$changedBytes=0
if($reuseFiles){foreach($relative in $changedPaths){$changedBytes+=(Get-Item -LiteralPath (Join-Path $stageConfig $relative)).Length}}
$requiredBytes = if ($reuseIdenticalConfig) { $safetyBytes } elseif($reuseFiles){$changedBytes+$safetyBytes} else { $configBytes + $safetyBytes }
if ($remoteFreeBytes -lt $requiredBytes) {
    throw "Insufficient board space for atomic Config staging: free=$remoteFreeBytes required=$requiredBytes configBytes=$configBytes changedBytes=$changedBytes reuseVerifiedFiles=$reuseFiles"
}
Write-Host "[DeploymentSpace] result=PASS freeBytes=$remoteFreeBytes configBytes=$configBytes safetyBytes=$safetyBytes"
# Keep the compressed transfer artifact on tmpfs so it does not consume the
# same /userdata capacity needed by the fully verified Config.new directory.
$remoteArchive = "/tmp/HwaSimIR_Config.new_$stamp.tgz"
$remoteConfigNew = "$BoardRoot/Config.new_$stamp"
$remoteConfigBackup = "$BoardRoot/Config.before_$stamp"
$remoteElfBackup = "$BoardRoot/HwaSim_IR.before_$stamp"
$remoteLauncherBackup = "$BoardRoot/run_precise.sh.before_$stamp"
$remotePerformanceBackup = "$BoardRoot/rk3588_hwasimir_performance_mode.sh.before_$stamp"
if($reuseFiles) {
    if(!$ResumeStaging){Invoke-Ssh "test ! -e '$remoteConfigNew' && cp -al '$BoardRoot/Config' '$remoteConfigNew'"}
    Invoke-Scp $archive $remoteArchive
    $deltaDir="$BoardRoot/Config.delta_$stamp"
    # Extract new files separately: tar must never truncate a hard-linked inode.
    if(!$ResumeStaging){Invoke-Ssh "set -eu; test ! -e '$deltaDir'; mkdir '$deltaDir'"}
    else{Invoke-Ssh "set -eu; test -d '$deltaDir'; test -z `"`$(find '$deltaDir' -type f -links +1 -print -quit)`"; test -z `"`$(find '$deltaDir' -type l -print -quit)`""}
    Invoke-Ssh "set -eu; tar --warning=no-timestamp --warning=no-unknown-keyword -xzf '$remoteArchive' -C '$deltaDir'"
    foreach($relative in $changedPaths){
        $parent=Split-Path -Parent $relative
        $parent=$parent -replace '\\','/'
        Invoke-Ssh "mkdir -p '$remoteConfigNew/$parent'; mv -f '$deltaDir/$relative' '$remoteConfigNew/$relative'"
    }
    foreach($relative in $removedPaths){Invoke-Ssh "rm -f -- '$remoteConfigNew/$relative'"}
    Invoke-Ssh "find '$deltaDir' -depth -type d -empty -delete"
    Write-Host "[DeploymentStage] mode=verified_file_snapshot changedFiles=$($changedPaths.Count) removedFiles=$($removedPaths.Count) allConfigFilesIncluded=1 sharedInodesReplacedByRename=1"
} elseif ($reuseIdenticalConfig) {
    # Immutable snapshot of the entire already-verified tree, not an ELF-only
    # update. Replace version metadata through rename so no shared inode is edited.
    Invoke-Ssh "test ! -e '$remoteConfigNew' && cp -al '$BoardRoot/Config' '$remoteConfigNew'"
    Invoke-Scp $versionPath "$remoteConfigNew/deployment_version.env.replacement"
    Invoke-Ssh "mv '$remoteConfigNew/deployment_version.env.replacement' '$remoteConfigNew/deployment_version.env'"
    Write-Host '[DeploymentStage] mode=verified_identical_config_snapshot allConfigFilesIncluded=1 metadataReplacedByRename=1'
} else {
    Invoke-Scp $archive $remoteArchive
}

$requiredTests = ($requiredConfig | ForEach-Object { "test -f '$remoteConfigNew/$_'" }) -join ' && '
$extract = if ($reuseIdenticalConfig -or $reuseFiles) { ':' } else { "mkdir -p '$remoteConfigNew'; tar --warning=no-timestamp --warning=no-unknown-keyword -xzf '$remoteArchive' -C '$remoteConfigNew' --strip-components=1" }
$prepare = "set -eu; $extract; cd '$remoteConfigNew'; sha256sum -c deployment_manifest.sha256 >/tmp/hwasimir_config_verify_$stamp.log; $requiredTests; test `$(sha256sum deployment_manifest.sha256 | awk '{print `$1}') = '$manifestSha'; test `$(sha256sum '$boardElfNew' | awk '{print `$1}') = '$elfSha'; echo '[DeploymentVerify] result=PASS configManifestSha256=$manifestSha elfSha256=$elfSha buildId=$buildId staging=$remoteConfigNew'"
Invoke-Ssh $prepare

$switch = "set -eu; pkill -TERM -x HwaSim_IR 2>/dev/null || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR >/dev/null; cd '$BoardRoot'; cp -p HwaSim_IR '$remoteElfBackup'; cp -p run_precise.sh '$remoteLauncherBackup'; if [ -f rk3588_hwasimir_performance_mode.sh ]; then cp -p rk3588_hwasimir_performance_mode.sh '$remotePerformanceBackup'; fi; mv Config '$remoteConfigBackup'; if mv '$remoteConfigNew' Config && mv '$boardElfNew' HwaSim_IR && mv '$boardLauncherNew' run_precise.sh && mv '$boardPerformanceNew' rk3588_hwasimir_performance_mode.sh; then chmod 755 HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh; rm -f '$remoteArchive'; echo '[DeploymentSwitch] result=PASS backupConfig=$remoteConfigBackup backupElf=$remoteElfBackup'; else rm -rf Config; mv '$remoteConfigBackup' Config; cp -p '$remoteElfBackup' HwaSim_IR; cp -p '$remoteLauncherBackup' run_precise.sh; [ ! -f '$remotePerformanceBackup' ] || cp -p '$remotePerformanceBackup' rk3588_hwasimir_performance_mode.sh; echo '[DeploymentSwitch][ERROR] reason=atomic_switch_failed action=rollback' >&2; exit 31; fi"
Invoke-Ssh $switch

$finalVerify = Invoke-SshCapture "set -eu; cd '$BoardRoot'; test `$(sha256sum HwaSim_IR | awk '{print `$1}') = '$elfSha'; test `$(sha256sum run_precise.sh | awk '{print `$1}') = '$launcherSha'; test `$(sha256sum rk3588_hwasimir_performance_mode.sh | awk '{print `$1}') = '$performanceSha'; test `$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}') = '$manifestSha'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); echo '[DeploymentFinal] result=PASS gitCommit=$gitCommit sourceIdentity=$sourceIdentity elfSha256=$elfSha buildId=$buildId runtimeConfigSha256=$runtimeSha configManifestSha256=$manifestSha'"
$finalVerify | Tee-Object -FilePath (Join-Path $LogDirectory 'deployment_final.txt')
# Retain the whole immutable package until the deployed version passes its short test.
# A separate -FinalizeRetention invocation consumes the actual acceptance receipt.
Invoke-Ssh "rmdir '$BoardRoot/.deployment_in_progress'"
Write-Host "[DeploymentBackup] result=PASS directory=$remoteConfigBackup format=complete_immutable_snapshot retention=pending_short_acceptance"
Write-Host "[Deployment] result=PASS logDirectory=$LogDirectory"
