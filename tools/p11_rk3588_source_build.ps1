[CmdletBinding()]
param(
    [ValidateSet('Plan', 'Build')]
    [string]$Mode = 'Plan',
    [string]$RepoRoot = '',
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$VmPassword = $env:P11_VM_PASSWORD,
    [string]$SshKey = 'C:\Users\kahn1\.ssh\codex_hwasimir_ed25519',
    [string]$StageId = '',
    [string]$OutputDirectory = '',
    [switch]$ConfirmSourceStable,
    [switch]$ResumeExistingStage
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
$sourceRoot = Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR'
$ddsRoot = Join-Path $RepoRoot 'DDS'
$askPass = Join-Path $PSScriptRoot 'p5_ssh_askpass.cmd'
foreach ($required in @($sourceRoot, $ddsRoot, $askPass)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required source: $required" }
}
if (-not $StageId) { $StageId = 'p11-' + (Get-Date -Format 'yyyyMMdd-HHmmss') }
if ($StageId -notmatch '^p11-[0-9]{8}-[0-9]{6}$') { throw "Unsafe StageId: $StageId" }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $RepoRoot "logs\p11\rk3588\build-$StageId" }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

$remoteRoot = "/home/linaro/userdata/HwaSimIR-$StageId"
$remoteArchive = "/home/linaro/userdata/HwaSimIR-$StageId.source.tgz"
$remoteArchiveSha = "$remoteArchive.sha256"
$remoteBuild = "$remoteRoot/cmake-build-release"
$manifestPath = Join-Path $OutputDirectory 'source_manifest.sha256'
$archivePath = Join-Path $OutputDirectory "HwaSimIR-$StageId.source.tgz"
$archiveShaPath = $archivePath + '.sha256'
$utf8NoBom = New-Object Text.UTF8Encoding($false)

function Get-UnixRelativePath {
    param([string]$Root, [string]$Path)
    $rootUri = New-Object Uri(($Root.TrimEnd('\') + '\'))
    $pathUri = New-Object Uri($Path)
    return [Uri]::UnescapeDataString($rootUri.MakeRelativeUri($pathUri).ToString())
}

function Invoke-Native {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $FilePath $($Arguments -join ' ')" }
}

function Get-SshArguments {
    $result = @('-o', 'ConnectTimeout=10', '-o', 'ServerAliveInterval=10', '-o', 'StrictHostKeyChecking=accept-new')
    if (-not $VmPassword) {
        if (-not $SshKey -or -not (Test-Path -LiteralPath $SshKey)) { throw 'Neither VM password nor readable SSH key is available' }
        $result += @('-i', $SshKey, '-o', 'BatchMode=yes')
    }
    return $result
}

function Invoke-Ssh {
    param([string]$Command, [string]$LogPath = '')
    $savedPassword = $env:HWASIMIR_SSH_PASSWORD
    $savedAskPass = $env:SSH_ASKPASS
    $savedRequire = $env:SSH_ASKPASS_REQUIRE
    $savedDisplay = $env:DISPLAY
    try {
        if ($VmPassword) {
            $env:HWASIMIR_SSH_PASSWORD = $VmPassword
            $env:SSH_ASKPASS = $askPass
            $env:SSH_ASKPASS_REQUIRE = 'force'
            $env:DISPLAY = 'p11-rk3588-build'
        }
        $arguments = @(Get-SshArguments)
        if ($VmPassword) { $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password') }
        $arguments += @("$VmUser@$VmHost", $Command)
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        $output = & ssh.exe @arguments 2>&1
        $exitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        if ($LogPath) { [IO.File]::WriteAllText($LogPath, ($output -join "`n") + "`n", $utf8NoBom) }
        if ($exitCode -ne 0) { throw "VM command failed ($exitCode): $Command" }
        return @($output)
    }
    finally {
        $env:HWASIMIR_SSH_PASSWORD = $savedPassword
        $env:SSH_ASKPASS = $savedAskPass
        $env:SSH_ASKPASS_REQUIRE = $savedRequire
        $env:DISPLAY = $savedDisplay
    }
}

function Invoke-Scp {
    param([string]$Source, [string]$Destination)
    $savedPassword = $env:HWASIMIR_SSH_PASSWORD
    $savedAskPass = $env:SSH_ASKPASS
    $savedRequire = $env:SSH_ASKPASS_REQUIRE
    $savedDisplay = $env:DISPLAY
    try {
        if ($VmPassword) {
            $env:HWASIMIR_SSH_PASSWORD = $VmPassword
            $env:SSH_ASKPASS = $askPass
            $env:SSH_ASKPASS_REQUIRE = 'force'
            $env:DISPLAY = 'p11-rk3588-build'
        }
        $arguments = @(Get-SshArguments)
        if ($VmPassword) { $arguments += @('-o', 'PubkeyAuthentication=no', '-o', 'PreferredAuthentications=password') }
        $arguments += @($Source, $Destination)
        Invoke-Native -FilePath 'scp.exe' -Arguments $arguments
    }
    finally {
        $env:HWASIMIR_SSH_PASSWORD = $savedPassword
        $env:SSH_ASKPASS = $savedAskPass
        $env:SSH_ASKPASS_REQUIRE = $savedRequire
        $env:DISPLAY = $savedDisplay
    }
}

# Hash exactly the compilation snapshot that will be archived.  Build products,
# runtime Config (5+ GiB), IDE state and local SDK mirrors are excluded.
$excludedPrefixes = @(
    '.idea/', '.vs/', 'Bin/', 'x64/', 'Debug/', 'Release/',
    'opencv2-440/', 'cmake-build-', 'build-'
)
$sourceFiles = Get-ChildItem -LiteralPath $sourceRoot -Recurse -File | Where-Object {
    $relative = Get-UnixRelativePath -Root $sourceRoot -Path $_.FullName
    -not ($excludedPrefixes | Where-Object { $relative.StartsWith($_, [StringComparison]::OrdinalIgnoreCase) })
}
$ddsDirectories = @('Runtime', 'Protocol', 'Generated\HwaSimIRProtocolV1')
$ddsFiles = foreach ($directory in $ddsDirectories) {
    Get-ChildItem -LiteralPath (Join-Path $ddsRoot $directory) -Recurse -File
}
$manifestLines = New-Object System.Collections.Generic.List[string]
foreach ($file in ($sourceFiles | Sort-Object FullName)) {
    $relative = Get-UnixRelativePath -Root $sourceRoot -Path $file.FullName
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestLines.Add("$hash  $relative")
}
foreach ($file in ($ddsFiles | Sort-Object FullName)) {
    $relative = 'DDS/' + (Get-UnixRelativePath -Root $ddsRoot -Path $file.FullName)
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestLines.Add("$hash  $relative")
}
[IO.File]::WriteAllText($manifestPath, ($manifestLines -join "`n") + "`n", $utf8NoBom)

$gitHead = (& git -C $RepoRoot rev-parse HEAD).Trim()
$gitStatus = @(& git -C $RepoRoot status --short)
$plan = [ordered]@{
    schema = 'hwasimir.p11.rk3588.source-build-plan.v1'
    created_utc = [DateTime]::UtcNow.ToString('o')
    mode = $Mode
    stage_id = $StageId
    git_head = $gitHead
    working_tree = if ($gitStatus.Count -eq 0) { 'clean' } else { 'dirty' }
    source_file_count = $sourceFiles.Count
    dds_file_count = @($ddsFiles).Count
    source_manifest_sha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    remote_source_root = $remoteRoot
    remote_build_root = $remoteBuild
    existing_vm_source_is_not_modified = $true
    runtime_config_in_source_archive = $false
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'source_build_plan.json'), ($plan | ConvertTo-Json -Depth 4) + "`n", $utf8NoBom)
if ($Mode -eq 'Plan') {
    Write-Host "[P11 RK3588 SourcePlan] result=PASS files=$($manifestLines.Count) manifest=$manifestPath remote=$remoteRoot remoteModified=0"
    return
}
if (-not $ConfirmSourceStable) {
    throw 'Build mode requires -ConfirmSourceStable after the coordinating agent freezes the unified Windows source'
}

if ($ResumeExistingStage) {
    if (-not (Test-Path -LiteralPath $archivePath) -or -not (Test-Path -LiteralPath $archiveShaPath)) {
        throw 'ResumeExistingStage requires the original local source archive and checksum'
    }
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $resumeVerify = "set -eu; test -d '$remoteRoot'; test -x '$remoteBuild/HwaSim_IR'; cd '$remoteRoot'; sha256sum -c source_manifest.sha256 >/dev/null; test `$(sha256sum source_manifest.sha256 | awk '{print `$1}') = '$($plan.source_manifest_sha256)'; echo '[P11VmResume] result=PASS root=$remoteRoot files=$($manifestLines.Count) archiveSha256=$archiveHash'"
    Invoke-Ssh -Command $resumeVerify -LogPath (Join-Path $OutputDirectory 'vm_resume_verify.log') | Out-Null
}
else {
    $tarArguments = @(
        '-czf', $archivePath,
        '--exclude=.idea', '--exclude=.vs', '--exclude=Bin', '--exclude=x64',
        '--exclude=Debug', '--exclude=Release', '--exclude=opencv2-440',
        '--exclude=cmake-build-*', '--exclude=build-*',
        '-C', $sourceRoot, '.',
        '-C', $RepoRoot, 'DDS/Runtime', 'DDS/Protocol', 'DDS/Generated/HwaSimIRProtocolV1',
        '-C', $OutputDirectory, 'source_manifest.sha256'
    )
    Invoke-Native -FilePath 'tar.exe' -Arguments $tarArguments
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText($archiveShaPath, "$archiveHash  $(Split-Path -Leaf $archivePath)`n", $utf8NoBom)

    # The candidate is isolated beside the historical VM project.  No extraction,
    # configure or build command names the existing /home/.../HwaSimIR tree.
    Invoke-Ssh -Command "set -eu; test ! -e '$remoteRoot'; test ! -e '$remoteArchive'; test ! -e '$remoteArchiveSha'; df -PB1 /home/linaro/userdata | tail -1" -LogPath (Join-Path $OutputDirectory 'vm_preflight.log') | Out-Null
    Invoke-Scp -Source $archivePath -Destination "$VmUser@$VmHost`:$remoteArchive"
    Invoke-Scp -Source $archiveShaPath -Destination "$VmUser@$VmHost`:$remoteArchiveSha"
    $extractCommand = "set -eu; cd /home/linaro/userdata; sha256sum -c '$(Split-Path -Leaf $remoteArchiveSha)'; mkdir '$remoteRoot'; tar -xzf '$remoteArchive' -C '$remoteRoot'; cd '$remoteRoot'; sha256sum -c source_manifest.sha256 >/dev/null; echo '[P11VmSource] result=PASS root=$remoteRoot files=$($manifestLines.Count) archiveSha256=$archiveHash'"
    Invoke-Ssh -Command $extractCommand -LogPath (Join-Path $OutputDirectory 'vm_extract_verify.log') | Out-Null
}

$cmakeOptions = @(
    '-DCMAKE_BUILD_TYPE=Release',
    '-DHWASIMIR_ENABLE_RKMPP=ON',
    '-DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp',
    '-DHWASIMIR_ENABLE_ZRDDS=ON',
    '-DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64',
    '-DHWASIMIR_ENABLE_FFMPEG=OFF',
    '-DHWASIMIR_ENABLE_AVFORMAT=ON',
    '-DPANDA3D_ROOT=/opt/panda3d-aarch64',
    '-DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4'
) -join ' '
$configureAndBuild = if ($ResumeExistingStage) { '' } else { "cmake -S '$remoteRoot' -B '$remoteBuild' -G Ninja $cmakeOptions; cmake --build '$remoteBuild' --parallel 4; " }
$buildCommand = "set -eu; $configureAndBuild file '$remoteBuild/HwaSim_IR'; aarch64-linux-gnu-readelf -d '$remoteBuild/HwaSim_IR' | grep -E 'librockchip_mpp.so.1|libZRDDSCpp.so'; printf 'ElfSha256='; sha256sum '$remoteBuild/HwaSim_IR' | awk '{print `$1}'; printf 'BuildId='; aarch64-linux-gnu-readelf -n '$remoteBuild/HwaSim_IR' | awk '/Build ID:/ {print `$3; exit}'; printf 'VmUtc='; date -u +%Y-%m-%dT%H:%M:%SZ"
$buildOutput = Invoke-Ssh -Command $buildCommand -LogPath (Join-Path $OutputDirectory 'vm_build.log')
$remoteElfHashLine = $buildOutput | Where-Object { $_ -match '^ElfSha256=[0-9a-f]{64}$' } | Select-Object -Last 1
$buildIdLine = $buildOutput | Where-Object { $_ -match '^BuildId=[0-9a-f]+$' } | Select-Object -Last 1
if (-not $remoteElfHashLine -or -not $buildIdLine) { throw 'Build log does not contain ELF hash and Build ID' }
$remoteElfHash = ($remoteElfHashLine -split '=', 2)[1]
$buildId = ($buildIdLine -split '=', 2)[1]
$localElf = Join-Path $OutputDirectory 'HwaSim_IR.aarch64'
Invoke-Scp -Source "$VmUser@$VmHost`:$remoteBuild/HwaSim_IR" -Destination $localElf
$localElfHash = (Get-FileHash -LiteralPath $localElf -Algorithm SHA256).Hash.ToLowerInvariant()
if ($localElfHash -ne $remoteElfHash) { throw 'Downloaded ELF hash differs from VM build hash' }

$receipt = [ordered]@{
    schema = 'hwasimir.p11.rk3588.build-receipt.v1'
    controller_utc = [DateTime]::UtcNow.ToString('o')
    stage_id = $StageId
    git_head = $gitHead
    working_tree = $plan.working_tree
    source_manifest_sha256 = $plan.source_manifest_sha256
    source_archive_sha256 = $archiveHash
    remote_source_root = $remoteRoot
    remote_build_root = $remoteBuild
    elf_sha256 = $localElfHash
    build_id = $buildId
    rkmpp = $true
    zrdds = $true
    ffmpeg_encoder = $false
    avformat_mux = $true
}
[IO.File]::WriteAllText((Join-Path $OutputDirectory 'build_receipt.json'), ($receipt | ConvertTo-Json -Depth 4) + "`n", $utf8NoBom)
Write-Host "[P11 RK3588 Build] result=PASS elf=$localElf sha256=$localElfHash buildId=$buildId remote=$remoteBuild"
