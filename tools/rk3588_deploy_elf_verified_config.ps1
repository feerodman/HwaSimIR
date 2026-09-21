[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ElfPath,
    [string]$RepoRoot='',
    [string]$BoardHost='192.168.1.116',
    [string]$BoardUser='root',
    [string]$BoardRoot='/userdata/HwaSimIR',
    [string]$BoardPassword=$env:HWASIMIR_SSH_PASSWORD,
    [string]$ExpectedConfigManifestSha256='9b344f42964dbf4e05d7502ba815bea6d4c6e37276bebd3aa63a8f702ec600c9',
    [string]$LogDirectory=''
)

$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
if(!$RepoRoot){$RepoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path}
if($BoardRoot -ne '/userdata/HwaSimIR'){throw 'Deployment is restricted to the named board workspace'}
if(!$BoardPassword){throw 'Board password was not supplied'}
$elf=(Resolve-Path -LiteralPath $ElfPath).Path
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
if(!$LogDirectory){$LogDirectory=Join-Path $RepoRoot "logs\rk3588-elf-refresh-$stamp"}
New-Item -ItemType Directory -Force -Path $LogDirectory|Out-Null
$LogDirectory=(Resolve-Path -LiteralPath $LogDirectory).Path
$askPass=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$oldPassword=$env:HWASIMIR_SSH_PASSWORD;$oldAsk=$env:SSH_ASKPASS
$oldRequire=$env:SSH_ASKPASS_REQUIRE;$oldDisplay=$env:DISPLAY
$env:HWASIMIR_SSH_PASSWORD=$BoardPassword;$env:SSH_ASKPASS=$askPass
$env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='hwasimir-elf-refresh'

function Ssh-Args {@('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new','-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password')}
function Invoke-Board([string]$Command){
    $lines=@(& ssh.exe @(Ssh-Args) "$BoardUser@$BoardHost" $Command)
    if($LASTEXITCODE -ne 0){throw "Board command failed ($LASTEXITCODE): $Command"}
    ($lines -join "`n").Trim()
}
function Copy-ToBoard([string]$Source,[string]$Destination){
    & scp.exe @(Ssh-Args) $Source "${BoardUser}@${BoardHost}:$Destination"
    if($LASTEXITCODE -ne 0){throw "Board copy failed ($LASTEXITCODE): $Destination"}
}

try {
    $elfSha=(Get-FileHash -LiteralPath $elf -Algorithm SHA256).Hash.ToLowerInvariant()
    $spritePath=Join-Path $RepoRoot 'HwaSim_IR\Bin\Config\GameVFX\sprite.frag'
    $precipitationPath=Join-Path $RepoRoot 'HwaSim_IR\Bin\Config\Weather\precipitation.frag'
    $spriteSha=(Get-FileHash -LiteralPath $spritePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $precipitationSha=(Get-FileHash -LiteralPath $precipitationPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $preflight=Invoke-Board "set -eu; cd '$BoardRoot'; test ! -e .deployment_in_progress; test ! -e .retention_in_progress; test -x HwaSim_IR; test -f Config/deployment_manifest.sha256; test `$(sha256sum Config/deployment_manifest.sha256|awk '{print `$1}') = '$ExpectedConfigManifestSha256'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); test `$(sha256sum Config/GameVFX/sprite.frag|awk '{print `$1}') = '$spriteSha'; test `$(sha256sum Config/Weather/precipitation.frag|awk '{print `$1}') = '$precipitationSha'; echo '[VerifiedConfigPreflight] result=PASS manifestSha256=$ExpectedConfigManifestSha256 spriteSha256=$spriteSha precipitationSha256=$precipitationSha'; echo PreviousElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo PreviousBuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}')"
    [IO.File]::WriteAllText((Join-Path $LogDirectory 'preflight.txt'),$preflight+"`n",(New-Object Text.UTF8Encoding($false)))

    $remoteNew="$BoardRoot/HwaSim_IR.new_$stamp"
    $remoteBackup="$BoardRoot/HwaSim_IR.before_$stamp"
    Copy-ToBoard $elf $remoteNew
    $uploaded=Invoke-Board "set -eu; chmod 755 '$remoteNew'; test `$(sha256sum '$remoteNew'|awk '{print `$1}') = '$elfSha'; echo BuildId=`$(readelf -n '$remoteNew'|awk '/Build ID:/ {print `$3;exit}')"
    $buildId=([regex]::Match($uploaded,'BuildId=([0-9a-f]+)')).Groups[1].Value
    if(!$buildId){throw 'Uploaded ELF Build ID was not available'}

    $utf8=New-Object Text.UTF8Encoding($false)
    $currentVersion=Invoke-Board "cat '$BoardRoot/Config/deployment_version.env'"
    $version=@{}
    foreach($line in ($currentVersion -split "`n")){
        if($line -match '^([^=]+)=(.*)$'){$version[$Matches[1]]=$Matches[2].Trim()}
    }
    foreach($required in @('SchemaVersion','RuntimeConfigSha256','LauncherSha256','PerformanceToolSha256')){
        if(!$version.ContainsKey($required)){throw "Current deployment version lacks $required"}
    }
    $gitCommit=(& git -C $RepoRoot rev-parse HEAD).Trim()
    $treeState=if((& git -C $RepoRoot status --porcelain).Count -eq 0){'clean'}else{'dirty'}
    $sourceIdentity="$gitCommit-$treeState"
    $versionLines=@(
        "SchemaVersion=$($version.SchemaVersion)","GitCommit=$gitCommit","WorkingTreeState=$treeState",
        "SourceIdentity=$sourceIdentity","ElfSha256=$elfSha","BuildId=$buildId",
        "RuntimeConfigSha256=$($version.RuntimeConfigSha256)",
        "ConfigManifestSha256=$ExpectedConfigManifestSha256",
        "LauncherSha256=$($version.LauncherSha256)",
        "PerformanceToolSha256=$($version.PerformanceToolSha256)",
        "CreatedUtc=$([DateTime]::UtcNow.ToString('o'))"
    )
    $versionPath=Join-Path $LogDirectory 'deployment_version.env'
    [IO.File]::WriteAllText($versionPath,(($versionLines -join "`n")+"`n"),$utf8)
    $remoteConfigNew="$BoardRoot/Config.new_$stamp"
    $remoteConfigBackup="$BoardRoot/Config.before_$stamp"
    Invoke-Board "set -eu; test ! -e '$remoteConfigNew'; test ! -e '$remoteConfigBackup'; cp -al '$BoardRoot/Config' '$remoteConfigNew'"|Out-Null
    Copy-ToBoard $versionPath "$remoteConfigNew/deployment_version.env.replacement"
    Invoke-Board "set -eu; mv '$remoteConfigNew/deployment_version.env.replacement' '$remoteConfigNew/deployment_version.env'; grep -qx 'ElfSha256=$elfSha' '$remoteConfigNew/deployment_version.env'; grep -qx 'BuildId=$buildId' '$remoteConfigNew/deployment_version.env'; test `$(sha256sum '$remoteConfigNew/deployment_manifest.sha256'|awk '{print `$1}') = '$ExpectedConfigManifestSha256'; (cd '$remoteConfigNew' && sha256sum -c deployment_manifest.sha256 >/dev/null)"|Out-Null

    $switch=Invoke-Board "set -eu; cd '$BoardRoot'; pkill -TERM -x HwaSim_IR 2>/dev/null || true; for n in 1 2 3 4 5; do pgrep -x HwaSim_IR >/dev/null || break; sleep 1; done; ! pgrep -x HwaSim_IR >/dev/null; test ! -e '$remoteBackup'; cp -p HwaSim_IR '$remoteBackup'; mv Config '$remoteConfigBackup'; if mv '$remoteConfigNew' Config && mv '$remoteNew' HwaSim_IR; then chmod 755 HwaSim_IR; else test ! -e Config || mv Config '$BoardRoot/Config.failed_$stamp'; mv '$remoteConfigBackup' Config; cp -p '$remoteBackup' HwaSim_IR; exit 31; fi; test `$(sha256sum HwaSim_IR|awk '{print `$1}') = '$elfSha'; grep -qx 'ElfSha256=$elfSha' Config/deployment_version.env; grep -qx 'BuildId=$buildId' Config/deployment_version.env; test `$(sha256sum Config/deployment_manifest.sha256|awk '{print `$1}') = '$ExpectedConfigManifestSha256'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); echo '[ElfVerifiedConfigSwitch] result=PASS elfSha256=$elfSha buildId=$buildId configManifestSha256=$ExpectedConfigManifestSha256 backupElf=$remoteBackup backupConfig=$remoteConfigBackup configWrite=version_metadata_only'"
    [IO.File]::WriteAllText((Join-Path $LogDirectory 'deployment_final.txt'),$switch+"`n",$utf8)
    $receipt=[ordered]@{
        schema='HwaSimIR.P14.ElfVerifiedConfigDeployment.1';result='PASS';createdUtc=[DateTime]::UtcNow.ToString('o')
        elfSha256=$elfSha;buildId=$buildId;configManifestSha256=$ExpectedConfigManifestSha256
        spriteShaderSha256=$spriteSha;precipitationShaderSha256=$precipitationSha
        configWrite='version_metadata_only';backupElf=$remoteBackup;backupConfig=$remoteConfigBackup;preflight=$preflight
    }
    [IO.File]::WriteAllText((Join-Path $LogDirectory 'deployment_receipt.json'),($receipt|ConvertTo-Json -Depth 5)+"`n",$utf8)
    Write-Output $switch
} finally {
    $env:HWASIMIR_SSH_PASSWORD=$oldPassword;$env:SSH_ASKPASS=$oldAsk
    $env:SSH_ASKPASS_REQUIRE=$oldRequire;$env:DISPLAY=$oldDisplay
}
