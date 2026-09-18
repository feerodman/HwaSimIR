[CmdletBinding()]
param(
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$BoardPassword = $env:HWASIMIR_SSH_PASSWORD,
    [string]$BoardRoot = '/userdata/HwaSimIR',
    [string]$ElfPath = '',
    [string]$LauncherPath = '',
    [string]$OutputDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
if (-not $ElfPath) { $ElfPath = Join-Path $repo 'logs\p12\build\rk3588-cloud-los\HwaSim_IR' }
if (-not $LauncherPath) { $LauncherPath = Join-Path $PSScriptRoot 'rk3588_run_hwasimir_precise.sh' }
$ElfPath = (Resolve-Path -LiteralPath $ElfPath).Path
$LauncherPath = (Resolve-Path -LiteralPath $LauncherPath).Path
$askPass = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$stageId = "p12-$stamp"
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo "logs\p12\p12d\deployment\$stageId" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8NoBom = New-Object Text.UTF8Encoding($false)
$newline = [Environment]::NewLine

function Write-Text([string]$Path, [string[]]$Lines) {
    [IO.File]::WriteAllText($Path, ($Lines -join $newline) + $newline, $utf8NoBom)
}
function Get-SshBase {
    $args = @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new')
    if ($BoardPassword) { $args += @('-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password') }
    else { $args += @('-i','C:\Users\kahn1\.ssh\codex_hwasimir_ed25519','-o','BatchMode=yes') }
    return $args
}
function Use-Authentication([scriptblock]$Action) {
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD; $oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE; $oldDisplay=$env:DISPLAY
    try {
        if ($BoardPassword) {
            $env:HWASIMIR_SSH_PASSWORD=$BoardPassword; $env:SSH_ASKPASS=$askPass
            $env:SSH_ASKPASS_REQUIRE='force'; $env:DISPLAY='p12-deploy'
        }
        & $Action
    }
    finally {
        $env:HWASIMIR_SSH_PASSWORD=$oldPassword; $env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire; $env:DISPLAY=$oldDisplay
    }
}
function Invoke-Ssh([string]$Command, [string]$LogPath='') {
    $result = Use-Authentication {
        $saved=$ErrorActionPreference; $ErrorActionPreference='Continue'
        $lines = & ssh.exe @(Get-SshBase) "$BoardUser@$BoardHost" $Command 2>&1
        $code=$LASTEXITCODE; $ErrorActionPreference=$saved
        [pscustomobject]@{Lines=@($lines); Code=$code}
    }
    if ($LogPath) { Write-Text $LogPath @($result.Lines | ForEach-Object { [string]$_ }) }
    if ($result.Code -ne 0) { throw "Board command failed with exit $($result.Code): $Command" }
    return @($result.Lines | ForEach-Object { [string]$_ })
}
function Invoke-Scp([string]$Source,[string]$Destination,[switch]$FromBoard) {
    Use-Authentication {
        $args=@(Get-SshBase)
        if($FromBoard){$args+=@("$BoardUser@$BoardHost`:$Source",$Destination)}
        else{$args+=@($Source,"$BoardUser@$BoardHost`:$Destination")}
        & scp.exe @args
        if($LASTEXITCODE -ne 0){throw "SCP failed with exit $LASTEXITCODE"}
    }
}
function Parse-Facts([string[]]$Lines) {
    $facts=@{}
    foreach($line in $Lines){if($line -match '^([^=]+)=(.*)$'){$facts[$Matches[1]]=$Matches[2]}}
    return $facts
}

$localElfHash=(Get-FileHash -LiteralPath $ElfPath -Algorithm SHA256).Hash.ToLowerInvariant()
$localLauncherHash=(Get-FileHash -LiteralPath $LauncherPath -Algorithm SHA256).Hash.ToLowerInvariant()
$gitCommit=(& git -C $repo rev-parse HEAD).Trim()
$treeState=if(@(& git -C $repo status --short).Count -eq 0){'clean'}else{'dirty'}

$factCommand=@"
set -eu
cd '$BoardRoot'
test ! -e .deployment_in_progress
! pgrep -x HwaSim_IR >/dev/null
(cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null)
echo ElfSha256=`$(sha256sum HwaSim_IR | awk '{print `$1}')
echo ConfigManifestSha256=`$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}')
echo RuntimeConfigSha256=`$(sha256sum Config/HwaSimIRRuntime.ini | awk '{print `$1}')
echo NetworkConfigSha256=`$(sha256sum Config/NetworkConfig.ini | awk '{print `$1}')
echo PreciseNetworkConfigSha256=`$(sha256sum Config/NetworkConfig_precise.ini | awk '{print `$1}')
echo PerformanceToolSha256=`$(sha256sum rk3588_hwasimir_performance_mode.sh | awk '{print `$1}')
echo BuildId=`$(readelf -n HwaSim_IR | awk '/Build ID:/ {print `$3; exit}')
echo BoardUtc=`$(date -u +%Y-%m-%dT%H:%M:%SZ)
"@
$facts=Parse-Facts (Invoke-Ssh $factCommand (Join-Path $OutputDirectory 'board_preflight.log'))
if($facts.ElfSha256 -ne $localElfHash){throw "Local ELF is not the deployed ELF: local=$localElfHash board=$($facts.ElfSha256)"}
foreach($key in @('ConfigManifestSha256','RuntimeConfigSha256','NetworkConfigSha256','PreciseNetworkConfigSha256','PerformanceToolSha256','BuildId')){
    if(-not $facts[$key]){throw "Board fact missing: $key"}
}

$versionLines=@(
    'SchemaVersion=1',
    "GitCommit=$gitCommit",
    "WorkingTreeState=$treeState",
    "SourceIdentity=$gitCommit-$treeState",
    "P12StageId=$stageId",
    "ElfSha256=$localElfHash",
    "BuildId=$($facts.BuildId)",
    "RuntimeConfigSha256=$($facts.RuntimeConfigSha256)",
    "NetworkConfigSha256=$($facts.NetworkConfigSha256)",
    "PreciseNetworkConfigSha256=$($facts.PreciseNetworkConfigSha256)",
    "ConfigManifestSha256=$($facts.ConfigManifestSha256)",
    "LauncherSha256=$localLauncherHash",
    "PerformanceToolSha256=$($facts.PerformanceToolSha256)",
    "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
)
$versionPath=Join-Path $OutputDirectory 'deployment_version.env'
[IO.File]::WriteAllText($versionPath,($versionLines -join "`n")+"`n",$utf8NoBom)
$localVersionHash=(Get-FileHash -LiteralPath $versionPath -Algorithm SHA256).Hash.ToLowerInvariant()
$remoteLauncherTmp="$BoardRoot/run_precise.sh.new_$stageId"
$remoteVersionTmp="$BoardRoot/Config/deployment_version.env.new_$stageId"
Invoke-Scp $LauncherPath $remoteLauncherTmp
Invoke-Scp $versionPath $remoteVersionTmp

$rollback="$BoardRoot/rollback-$stageId"
$switchCommand=@"
set -eu
cd '$BoardRoot'
test ! -e .deployment_in_progress
! pgrep -x HwaSim_IR >/dev/null
mkdir .deployment_in_progress
cleanup(){ rmdir .deployment_in_progress 2>/dev/null || true; }
trap cleanup EXIT INT TERM HUP
test `$(sha256sum '$remoteLauncherTmp' | awk '{print `$1}') = '$localLauncherHash'
test `$(sha256sum '$remoteVersionTmp' | awk '{print `$1}') = '$localVersionHash'
mkdir '$rollback'
cp -p run_precise.sh '$rollback/run_precise.sh'
cp -p Config/deployment_version.env '$rollback/deployment_version.env'
cp -p HwaSim_IR '$rollback/HwaSim_IR'
cp -p Config/deployment_manifest.sha256 '$rollback/deployment_manifest.sha256'
mv '$remoteLauncherTmp' run_precise.sh
mv '$remoteVersionTmp' Config/deployment_version.env
chmod 755 run_precise.sh
sh -n run_precise.sh
test `$(sha256sum HwaSim_IR | awk '{print `$1}') = '$localElfHash'
test `$(sha256sum run_precise.sh | awk '{print `$1}') = '$localLauncherHash'
test `$(sha256sum Config/deployment_manifest.sha256 | awk '{print `$1}') = '$($facts.ConfigManifestSha256)'
(cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null)
grep -F 'ElfSha256=$localElfHash' Config/deployment_version.env >/dev/null
grep -F 'ConfigManifestSha256=$($facts.ConfigManifestSha256)' Config/deployment_version.env >/dev/null
grep -F 'LauncherSha256=$localLauncherHash' Config/deployment_version.env >/dev/null
echo '[P12DeployReconcile] result=PASS stageId=$stageId rollback=$rollback elfSha256=$localElfHash launcherSha256=$localLauncherHash configManifestSha256=$($facts.ConfigManifestSha256) fullConfigCheck=PASS'
"@
$switchLines=Invoke-Ssh $switchCommand (Join-Path $OutputDirectory 'deployment_switch.log')
if(-not ($switchLines -match '\[P12DeployReconcile\] result=PASS')){throw 'Deployment reconcile did not report PASS'}

Invoke-Scp "$BoardRoot/Config/deployment_version.env" (Join-Path $OutputDirectory 'board_deployment_version.env') -FromBoard
$postFacts=Parse-Facts (Invoke-Ssh $factCommand (Join-Path $OutputDirectory 'board_postflight.log'))
$receipt=[ordered]@{
    schema='hwasimir.p12.rk3588.deployment-receipt.v1'; controller_utc=[DateTime]::UtcNow.ToString('o')
    stage_id=$stageId; board_host=$BoardHost; board_root=$BoardRoot
    elf_sha256=$postFacts.ElfSha256; build_id=$postFacts.BuildId
    config_manifest_sha256=$postFacts.ConfigManifestSha256
    runtime_config_sha256=$postFacts.RuntimeConfigSha256
    network_config_sha256=$postFacts.NetworkConfigSha256
    precise_network_config_sha256=$postFacts.PreciseNetworkConfigSha256
    launcher_sha256=$localLauncherHash; performance_tool_sha256=$postFacts.PerformanceToolSha256
    full_config_sha256_check='PASS'; rollback_path=$rollback; rollback_retained=$true
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'deployment_receipt.json'),($receipt|ConvertTo-Json -Depth 5)+$newline,$utf8NoBom)
Write-Host "[P12 RK3588 Deploy Reconcile] result=PASS stageId=$stageId rollback=$rollback output=$OutputDirectory"
