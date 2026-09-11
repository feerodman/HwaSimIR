[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ElfPath,
    [string]$RepoRoot = '',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$SshKey = '',
    [string]$LogDirectory = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }

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

$elf = (Resolve-Path -LiteralPath $ElfPath).Path
$configSource = Join-Path $RepoRoot 'HwaSim_IR\Bin\Config'
$launcherSource = Join-Path $RepoRoot 'tools\rk3588_run_hwasimir_precise.sh'
$performanceSource = Join-Path $RepoRoot 'tools\rk3588_hwasimir_performance_mode.sh'
foreach ($required in @($elf, $configSource, $launcherSource, $performanceSource)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required deployment source missing: $required" }
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $LogDirectory) { $LogDirectory = Join-Path $RepoRoot "logs\rk3588-deploy-$stamp" }
New-Item -ItemType Directory -Force -Path $LogDirectory | Out-Null
$LogDirectory = (Resolve-Path -LiteralPath $LogDirectory).Path
$stageRoot = Join-Path $LogDirectory 'stage'
$stageConfig = Join-Path $stageRoot 'Config'
New-Item -ItemType Directory -Force -Path $stageConfig | Out-Null
Copy-Item -Path (Join-Path $configSource '*') -Destination $stageConfig -Recurse -Force
$boardBoundQos = Join-Path $RepoRoot 'tools\dds_d1_qos\ZRDDS_QOS_RK3588_192.168.1.116.xml'
if (Test-Path -LiteralPath $boardBoundQos) {
    Copy-Item -LiteralPath $boardBoundQos -Destination (Join-Path $stageConfig 'DDS') -Force
}

$requiredConfig = @(
    'HwaSimIRRuntime.ini',
    'NetworkConfig_precise.ini',
    'DDS/ZRDDS_QOS_PROFILES.xml',
    'Materials/MaterialDatabase.csv',
    'Materials/MaterialBandOptics.csv',
    'Atmosphere/MODTRAN/processed/band_lut_si.csv',
    'Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv',
    'SensorWave/default_NVG.json',
    'SensorWave/default_MWIR.json',
    'TargetLib/Targets.json',
    'Weather/weather_profiles.json',
    'Weather/weather_textures.json',
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

$boardElfNew = "$BoardRoot/HwaSim_IR.new_$stamp"
$boardLauncherNew = "$BoardRoot/run_precise.sh.new_$stamp"
$boardPerformanceNew = "$BoardRoot/rk3588_hwasimir_performance_mode.sh.new_$stamp"
Invoke-Scp $elf $boardElfNew
Invoke-Scp $launcherSource $boardLauncherNew
Invoke-Scp $performanceSource $boardPerformanceNew
Invoke-Ssh "chmod 755 '$boardElfNew' '$boardLauncherNew' '$boardPerformanceNew'"

$elfSha = Invoke-SshCapture "sha256sum '$boardElfNew' | awk '{print `$1}'"
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
Invoke-Native -FilePath 'tar.exe' -Arguments @('-C', $stageRoot, '-czf', $archive, 'Config')
$configBytes = (Get-ChildItem -LiteralPath $stageConfig -Recurse -File | Measure-Object -Property Length -Sum).Sum
$remoteFreeBytes = [Int64](Invoke-SshCapture "df -PB1 '$BoardRoot' | awk 'NR==2 {print `$4}'")
$safetyBytes = 256MB
if ($remoteFreeBytes -lt ($configBytes + $safetyBytes)) {
    throw "Insufficient board space for atomic Config staging: free=$remoteFreeBytes required=$($configBytes + $safetyBytes) configBytes=$configBytes"
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
Invoke-Scp $archive $remoteArchive

$requiredTests = ($requiredConfig | ForEach-Object { "test -f '$remoteConfigNew/$_'" }) -join ' && '
$prepare = "set -eu; mkdir -p '$remoteConfigNew'; tar --warning=no-timestamp -xzf '$remoteArchive' -C '$remoteConfigNew' --strip-components=1; cd '$remoteConfigNew'; sha256sum -c deployment_manifest.sha256 >/tmp/hwasimir_config_verify_$stamp.log; $requiredTests; test `$(sha256sum deployment_manifest.sha256 | awk '{print `$1}') = '$manifestSha'; test `$(sha256sum '$boardElfNew' | awk '{print `$1}') = '$elfSha'; echo '[DeploymentVerify] result=PASS configManifestSha256=$manifestSha elfSha256=$elfSha buildId=$buildId staging=$remoteConfigNew'"
Invoke-Ssh $prepare

$switch = "set -eu; pkill -TERM -x HwaSim_IR 2>/dev/null || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR >/dev/null; cd '$BoardRoot'; cp -p HwaSim_IR '$remoteElfBackup'; cp -p run_precise.sh '$remoteLauncherBackup'; if [ -f rk3588_hwasimir_performance_mode.sh ]; then cp -p rk3588_hwasimir_performance_mode.sh '$remotePerformanceBackup'; fi; mv Config '$remoteConfigBackup'; if mv '$remoteConfigNew' Config && mv '$boardElfNew' HwaSim_IR && mv '$boardLauncherNew' run_precise.sh && mv '$boardPerformanceNew' rk3588_hwasimir_performance_mode.sh; then chmod 755 HwaSim_IR run_precise.sh rk3588_hwasimir_performance_mode.sh; rm -f '$remoteArchive'; echo '[DeploymentSwitch] result=PASS backupConfig=$remoteConfigBackup backupElf=$remoteElfBackup'; else rm -rf Config; mv '$remoteConfigBackup' Config; cp -p '$remoteElfBackup' HwaSim_IR; cp -p '$remoteLauncherBackup' run_precise.sh; [ ! -f '$remotePerformanceBackup' ] || cp -p '$remotePerformanceBackup' rk3588_hwasimir_performance_mode.sh; echo '[DeploymentSwitch][ERROR] reason=atomic_switch_failed action=rollback' >&2; exit 31; fi"
Invoke-Ssh $switch

$finalVerify = Invoke-SshCapture "cd '$BoardRoot'; test `$(sha256sum HwaSim_IR | awk '{print `$1}') = '$elfSha'; test `$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}') = '$manifestSha'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); echo '[DeploymentFinal] result=PASS gitCommit=$gitCommit sourceIdentity=$sourceIdentity elfSha256=$elfSha buildId=$buildId runtimeConfigSha256=$runtimeSha configManifestSha256=$manifestSha'"
$finalVerify | Tee-Object -FilePath (Join-Path $LogDirectory 'deployment_final.txt')
$backupArchive = "$BoardRoot/Config.before_$stamp.tgz"
$archiveBackup = "set -eu; tar -C '$BoardRoot' -czf '/tmp/Config.before_$stamp.tgz' 'Config.before_$stamp'; tar -tzf '/tmp/Config.before_$stamp.tgz' >/dev/null; sha256sum '/tmp/Config.before_$stamp.tgz' > '/tmp/Config.before_$stamp.tgz.sha256'; resolved=`$(readlink -f '$remoteConfigBackup'); test `"`$resolved`" = '$remoteConfigBackup'; rm -rf -- '$remoteConfigBackup'; mv '/tmp/Config.before_$stamp.tgz' '$backupArchive'; mv '/tmp/Config.before_$stamp.tgz.sha256' '$backupArchive.sha256'; echo '[DeploymentBackup] result=PASS archive=$backupArchive archiveSha256='`$(awk '{print `$1}' '$backupArchive.sha256')"
Invoke-Ssh $archiveBackup
Write-Host "[Deployment] result=PASS logDirectory=$LogDirectory"
