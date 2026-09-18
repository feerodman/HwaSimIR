[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$ElfPath,
    [string]$BoardHost='192.168.1.116',
    [string]$BoardUser='root',
    [string]$BoardPassword=$env:HWASIMIR_SSH_PASSWORD,
    [string]$BoardRoot='/userdata/HwaSimIR',
    [string]$OutputDirectory=''
)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$ElfPath=(Resolve-Path -LiteralPath $ElfPath).Path
if(-not $BoardPassword){$BoardPassword='123'}
$askPass=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd')).Path
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$stageId="p12-elf-$stamp"
if(-not $OutputDirectory){$OutputDirectory=Join-Path $repo "logs\p12\p12d\deployment\$stageId"}
if(Test-Path -LiteralPath $OutputDirectory){throw "Refusing to overwrite deployment evidence: $OutputDirectory"}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path -LiteralPath $OutputDirectory).Path
$utf8=New-Object Text.UTF8Encoding($false)
$elfHash=(Get-FileHash -LiteralPath $ElfPath -Algorithm SHA256).Hash.ToLowerInvariant()

function Auth([scriptblock]$Action){
    $oldPassword=$env:HWASIMIR_SSH_PASSWORD;$oldAsk=$env:SSH_ASKPASS
    $oldRequire=$env:SSH_ASKPASS_REQUIRE;$oldDisplay=$env:DISPLAY
    try{$env:HWASIMIR_SSH_PASSWORD=$BoardPassword;$env:SSH_ASKPASS=$askPass
        $env:SSH_ASKPASS_REQUIRE='force';$env:DISPLAY='p12-elf-deploy';& $Action}
    finally{$env:HWASIMIR_SSH_PASSWORD=$oldPassword;$env:SSH_ASKPASS=$oldAsk
        $env:SSH_ASKPASS_REQUIRE=$oldRequire;$env:DISPLAY=$oldDisplay}
}
function Args(){@('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new','-o','PubkeyAuthentication=no','-o','PreferredAuthentications=password')}
function Ssh([string]$Command,[string]$Log){
    $r=Auth{$saved=$ErrorActionPreference;$ErrorActionPreference='Continue';$lines=& ssh.exe @(Args) "$BoardUser@$BoardHost" $Command 2>&1;$code=$LASTEXITCODE;$ErrorActionPreference=$saved;[pscustomobject]@{Lines=@($lines);Code=$code}}
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory $Log),@($r.Lines|ForEach-Object{[string]$_}),$utf8)
    if($r.Code -ne 0){throw "Board command failed ($($r.Code)): $Command"};@($r.Lines)
}
function Scp([string]$Source,[string]$Destination){Auth{& scp.exe @(Args) $Source "$BoardUser@$BoardHost`:$Destination";if($LASTEXITCODE -ne 0){throw "SCP failed ($LASTEXITCODE)"}}}

$pre=Ssh "set -eu; cd '$BoardRoot'; test ! -e .deployment_in_progress; ! pgrep -x HwaSim_IR >/dev/null; echo PreviousElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo PreviousBuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo ConfigManifestSha256=`$(sha256sum Config/deployment_manifest.sha256|awk '{print `$1}'); (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); echo ConfigCheck=PASS" 'preflight.log'
$remoteNew="$BoardRoot/HwaSim_IR.new_$stageId"
Scp $ElfPath $remoteNew
$rollback="$BoardRoot/rollback-$stageId"
$switch=@"
set -eu
cd '$BoardRoot'
test ! -e .deployment_in_progress
! pgrep -x HwaSim_IR >/dev/null
mkdir .deployment_in_progress
cleanup(){ rmdir .deployment_in_progress 2>/dev/null || true; }
trap cleanup EXIT INT TERM HUP
test `$(sha256sum '$remoteNew'|awk '{print `$1}') = '$elfHash'
mkdir '$rollback'
cp -p HwaSim_IR '$rollback/HwaSim_IR'
cp -p Config/deployment_manifest.sha256 '$rollback/deployment_manifest.sha256'
if [ -f Config/deployment_version.env ]; then cp -p Config/deployment_version.env '$rollback/deployment_version.env'; fi
mv '$remoteNew' HwaSim_IR
chmod 755 HwaSim_IR
test `$(sha256sum HwaSim_IR|awk '{print `$1}') = '$elfHash'
(cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null)
echo '[P12ElfDeploy] result=PASS stageId=$stageId rollback=$rollback elfSha256=$elfHash configUnchanged=1'
"@
$sw=Ssh $switch 'switch.log'
if(-not ($sw -match '\[P12ElfDeploy\] result=PASS')){throw 'Atomic ELF switch did not report PASS'}
$post=Ssh "set -eu; cd '$BoardRoot'; ! pgrep -x HwaSim_IR >/dev/null; echo ElfSha256=`$(sha256sum HwaSim_IR|awk '{print `$1}'); echo BuildId=`$(readelf -n HwaSim_IR|awk '/Build ID:/ {print `$3;exit}'); echo ConfigManifestSha256=`$(sha256sum Config/deployment_manifest.sha256|awk '{print `$1}'); test -x HwaSim_IR; test -f '$rollback/HwaSim_IR'; (cd Config && sha256sum -c deployment_manifest.sha256 >/dev/null); echo Postflight=PASS" 'postflight.log'
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'receipt.json'),([ordered]@{schema='hwasimir.p12.incremental-elf-deploy.v1';stageId=$stageId;board="$BoardUser@$BoardHost";boardRoot=$BoardRoot;elfSha256=$elfHash;rollbackPath=$rollback;configUnchanged=$true;preflight=@($pre);postflight=@($post);createdUtc=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 6)+"`n",$utf8)
Write-Output "[P12 ELF Deploy] result=PASS stageId=$stageId rollback=$rollback elfSha256=$elfHash output=$OutputDirectory"
