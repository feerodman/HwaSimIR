[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$WindowsZrddsRoot = $env:ZRDDS_HOME,
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$VmProjectRoot = '/home/linaro/userdata/HwaSimIR',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$SshKey = '',
    [string]$LogDir = '',
    [switch]$SkipRemote,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

if (-not $RepoRoot) {
    $RepoRoot = Split-Path -Parent $PSScriptRoot
}

if (-not $LogDir) {
    $LogDir = Join-Path $RepoRoot ('logs\dds-d1-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$requiredLogs = @(
    'environment.txt', 'windows_sdk.txt', 'board_sdk.txt',
    'vm_cross_compile.txt', 'windows_to_board.txt', 'board_to_windows.txt',
    'h264_test.txt', 'raw_test.txt', 'build_windows.txt',
    'build_aarch64.txt', 'summary.txt'
)
foreach ($name in $requiredLogs) {
    $path = Join-Path $LogDir $name
    if (-not (Test-Path -LiteralPath $path)) {
        New-Item -ItemType File -Path $path | Out-Null
    }
}

function Add-Log {
    param([string]$Name, [string[]]$Lines)
    $Lines | Out-File -LiteralPath (Join-Path $LogDir $Name) -Encoding utf8 -Append
}

function Invoke-Logged {
    param([string]$Name, [scriptblock]$Command, [string]$Display)
    Add-Log $Name @('', ('COMMAND: ' + $Display))
    $output = & $Command 2>&1
    $code = $LASTEXITCODE
    if ($null -eq $code) { $code = 0 }
    $output | Out-File -LiteralPath (Join-Path $LogDir $Name) -Encoding utf8 -Append
    Add-Log $Name @('EXIT_CODE: ' + $code)
    if ($code -ne 0) { throw "Command failed ($code): $Display" }
}

function New-SshArgs {
    $args = @('-o', 'ConnectTimeout=10', '-o', 'StrictHostKeyChecking=accept-new')
    if ($SshKey) { $args += @('-i', $SshKey, '-o', 'BatchMode=yes') }
    return $args
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
$status = & git -C $RepoRoot status --short --branch
Add-Log 'environment.txt' @(
    'COMMAND: git rev-parse HEAD; git status --short --branch',
    ('HEAD=' + $head),
    $status,
    ('ZRDDS_HOME=' + $WindowsZrddsRoot),
    ('VM=' + $VmUser + '@' + $VmHost),
    ('BOARD=' + $BoardUser + '@' + $BoardHost),
    'Passwords and licence contents are intentionally never logged.'
)

$repoWindowsLicence = Join-Path $RepoRoot 'DDS\ZRDDSSetupX64VS2015-2.4.5-CAEP-Trial-cc-dg\ZRDDS-2.4.5\zrddslicence.lic'
$installedLicence = Join-Path $WindowsZrddsRoot 'zrddslicence.lic'
$repoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $repoWindowsLicence).Hash
$installedHash = if (Test-Path -LiteralPath $installedLicence) {
    (Get-FileHash -Algorithm SHA256 -LiteralPath $installedLicence).Hash
} else { 'MISSING' }
Add-Log 'windows_sdk.txt' @(
    ('include_exists=' + (Test-Path (Join-Path $WindowsZrddsRoot 'include'))),
    ('lib_exists=' + (Test-Path (Join-Path $WindowsZrddsRoot 'lib'))),
    ('bin_exists=' + (Test-Path (Join-Path $WindowsZrddsRoot 'bin'))),
    ('repo_licence_sha256=' + $repoHash),
    ('installed_licence_sha256=' + $installedHash),
    'NOTE: the CAEP trial runtime mutates its writable installed licence copy after initialization.'
)

if (-not $SkipBuild) {
    $msbuild = 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe'
    Invoke-Logged 'build_windows.txt' {
        & $msbuild (Join-Path $RepoRoot 'DDS\HwaSimIRVideoD1Smoke.sln') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo
    } 'MSBuild HwaSimIRVideoD1Smoke.sln Release x64'
    Invoke-Logged 'build_windows.txt' {
        & $msbuild (Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo
    } 'MSBuild HwaSim_IR.vcxproj Release x64'
}

if (-not $SkipRemote) {
    $sshArgs = @(New-SshArgs)
    $boardTarget = "$BoardUser@$BoardHost"
    $vmTarget = "$VmUser@$VmHost"
    Invoke-Logged 'board_sdk.txt' {
        & ssh.exe @sshArgs $boardTarget "test -d /usr/ZRDDS/ZRDDS-2.4.5/include; test -d /usr/ZRDDS/ZRDDS-2.4.5/lib; test -f /usr/ZRDDS/ZRDDS-2.4.5/zrddslicence.lic; sha256sum /usr/ZRDDS/ZRDDS-2.4.5/zrddslicence.lic; file /usr/ZRDDS/ZRDDS-2.4.5/lib/libZRDDSCpp.so; readelf -h /usr/ZRDDS/ZRDDS-2.4.5/lib/libZRDDSCpp.so; readelf -d /usr/ZRDDS/ZRDDS-2.4.5/lib/libZRDDSCpp.so; ldd /usr/ZRDDS/ZRDDS-2.4.5/lib/libZRDDSCpp.so; ip route"
    } 'RK3588 ZRDDS filesystem, ABI, dependencies, licence hash, and route'
    Invoke-Logged 'vm_cross_compile.txt' {
        & ssh.exe @sshArgs $vmTarget "test -f /home/linaro/sysroots/zrdds-aarch64/include/CPlusPlusInterface/ZRDDSCppSimpleInterface.h; test -f /home/linaro/sysroots/zrdds-aarch64/lib/libZRDDSCpp.so; file /home/linaro/sysroots/zrdds-aarch64/lib/libZRDDSCpp.so"
    } 'VM ZRDDS aarch64 sysroot check'
    if (-not $SkipBuild) {
        $opts = '-DCMAKE_BUILD_TYPE=Release -DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp -DHWASIMIR_ENABLE_ZRDDS=ON -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 -DHWASIMIR_ENABLE_FFMPEG=OFF -DPANDA3D_ROOT=/opt/panda3d-aarch64 -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4'
        Invoke-Logged 'build_aarch64.txt' {
            & ssh.exe @sshArgs $vmTarget "cmake -S $VmProjectRoot -B $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh -G Ninja $opts && cmake --build $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh -j4 && file $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh/HwaSim_IR"
        } 'VM HwaSim_IR Release AArch64 + RKMPP + ZRDDS'
    }
}

Add-Log 'summary.txt' @(
    'Automated environment, SDK, and build checks completed.',
    'Cross-machine payload tests require two concurrently running endpoints.',
    'Use the exact commands in DDS/HwaSimIRVideoReceiverDemo/README.md.',
    'Record both endpoint statistics and source/received SHA256 in the named test logs.',
    ('log_dir=' + $LogDir)
)

Write-Host "D1 logs: $LogDir"
