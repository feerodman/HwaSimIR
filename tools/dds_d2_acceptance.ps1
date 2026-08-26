[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$LogDir = '',
    [string]$EvidenceLogDir = '',
    [string]$FFmpegRoot = $env:FFMPEG_ROOT,
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$VmProjectRoot = '/home/linaro/userdata/HwaSimIR',
    [string]$SshKey = '',
    [switch]$SkipBuild,
    [switch]$SkipRegressions,
    [switch]$SkipRemote
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
if (-not $LogDir) {
    $LogDir = Join-Path $RepoRoot ('logs\dds-d2-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
if (-not $FFmpegRoot) {
    $FFmpegRoot = Join-Path $RepoRoot '.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1'
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$logNames = @(
    'environment.txt', 'static_contract.txt', 'build_windows.txt',
    'build_aarch64.txt', 'regression.txt', 'payload_audit.txt',
    'mp4_test.txt', 'rk_smoke.txt', 'summary.txt'
)
foreach ($name in $logNames) {
    $path = Join-Path $LogDir $name
    if (-not (Test-Path -LiteralPath $path)) { New-Item -ItemType File -Path $path | Out-Null }
}

$results = New-Object System.Collections.Generic.List[object]
$utf8 = New-Object Text.UTF8Encoding($false)

function Add-Log([string]$Name, [object[]]$Lines) {
    [IO.File]::AppendAllLines((Join-Path $LogDir $Name), [string[]]$Lines, $utf8)
}

function Invoke-Gate(
    [string]$Gate,
    [string]$LogName,
    [scriptblock]$Command,
    [string]$Display
) {
    Add-Log $LogName @('', ('COMMAND: ' + $Display))
    try {
        $global:LASTEXITCODE = 0
        $output = & $Command 2>&1
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 0 }
        Add-Log $LogName @($output)
        Add-Log $LogName @(('EXIT_CODE: ' + $code))
        if ($code -ne 0) { throw "exit=$code" }
        $results.Add([pscustomobject]@{ Gate=$Gate; Status='PASS'; Detail=$Display })
    }
    catch {
        Add-Log $LogName @(('ERROR: ' + $_.Exception.Message))
        $results.Add([pscustomobject]@{ Gate=$Gate; Status='FAIL'; Detail=$_.Exception.Message })
    }
}

function Assert-Text([string]$Path, [string]$Pattern, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "missing $Label at $Path" }
    $text = [IO.File]::ReadAllText($Path)
    if ($text -notmatch $Pattern) { throw "$Label does not match $Pattern" }
}

# Start-Process rejects a process environment containing both PATH and Path.
$processEnvironment = [Environment]::GetEnvironmentVariables('Process')
$pathEntries = @($processEnvironment.GetEnumerator() | Where-Object { $_.Key -imatch '^path$' })
if ($pathEntries.Count -gt 1) {
    $pathValue = ($pathEntries | Where-Object { $_.Key -ceq 'Path' } | Select-Object -First 1).Value
    if ([string]::IsNullOrEmpty($pathValue)) { $pathValue = $pathEntries[0].Value }
    [Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
    [Environment]::SetEnvironmentVariable('Path', [string]$pathValue, 'Process')
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
$status = & git -C $RepoRoot status --short --branch
Add-Log 'environment.txt' @(
    'COMMAND: git rev-parse HEAD; git status --short --branch',
    ('HEAD=' + $head),
    $status,
    ('ZRDDS_HOME=' + $env:ZRDDS_HOME),
    ('FFMPEG_ROOT=' + $FFmpegRoot),
    ('VM=' + $VmUser + '@' + $VmHost),
    'Passwords and licence contents are intentionally never logged.'
)
$results.Add([pscustomobject]@{
    Gate='D1 baseline / git status recorded'
    Status=$(if ($head -eq 'a361ae051279fa0e389a554a0cd791db6e35d2da') { 'PASS' } else { 'FAIL' })
    Detail=('HEAD=' + $head)
})

Invoke-Gate 'DDS RELIABLE + KEEP_ALL contract' 'static_contract.txt' {
    $qosFiles = @(
        (Join-Path $RepoRoot 'DDS\HwaSimIRVideoReceiverDemo\Config\ZRDDS_QOS_PROFILES.xml'),
        (Join-Path $RepoRoot 'HwaSim_IR\Bin\Config\DDS\ZRDDS_QOS_PROFILES.xml')
    )
    foreach ($qos in $qosFiles) {
        Assert-Text $qos 'tcpv4://' 'tcpv4 transport'
        Assert-Text $qos 'RELIABLE_RELIABILITY_QOS' 'reliable QoS'
        Assert-Text $qos 'KEEP_ALL_HISTORY_QOS' 'keep-all QoS'
        if ([IO.File]::ReadAllText($qos) -match 'BEST_EFFORT') { throw "BEST_EFFORT found in $qos" }
    }
    'QoS contract checks passed.'
} 'Validate both production/customer QoS XML files'

Invoke-Gate 'DDS Bytes-only / no custom wire header static contract' 'static_contract.txt' {
    $publisher = Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\Video\DdsVideoPublisher.cpp'
    Assert-Text $publisher 'DDSIF::BytesWrite' 'DDS::Bytes writer'
    Assert-Text $publisher 'DDSIF::Init' 'process DDS init'
    $initCount = ([regex]::Matches([IO.File]::ReadAllText($publisher), 'DDSIF::Init\s*\(')).Count
    if ($initCount -ne 1) { throw "expected one DDSIF::Init call, found $initCount" }
    'DDS::Bytes and one-time middleware init checks passed.'
} 'Inspect DdsVideoPublisher implementation'

Invoke-Gate 'LocalRecording strict double gate static contract' 'static_contract.txt' {
    $recorder = [IO.File]::ReadAllText((Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\Video\LocalMp4Recorder.cpp'))
    if ($recorder -notmatch 'm_impl->config\.enabled\s*&&\s*m_impl->protocolEnabled\.load\(\)') {
        throw 'effective LocalRecording gate expression not found'
    }
    'Strict config-and-protocol recording gate found.'
} 'Inspect sender-side recording gate'

if (-not $SkipBuild) {
    $msbuild = 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe'
    Invoke-Gate 'Windows Release HwaSim_IR build' 'build_windows.txt' {
        & $msbuild (Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo
    } 'VS2015 v140 HwaSim_IR Release x64'
    Invoke-Gate 'Windows Release VideoDisplay build' 'build_windows.txt' {
        & $msbuild (Join-Path $RepoRoot 'HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay.sln') /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:QtInstall='D:\Qt\Qt5.12.12\5.12.12\msvc2015_64' /p:FFMPEG_ROOT=$FFmpegRoot /verbosity:minimal /nologo
    } 'VS2015 v140 VideoDisplay Release x64'
    Invoke-Gate 'Customer Receiver Demo Windows build' 'build_windows.txt' {
        & $msbuild (Join-Path $RepoRoot 'DDS\HwaSimIRVideoD1Smoke.sln') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo
    } 'VS2015 v140 DDS receiver/sender smoke Release x64'
}

if (-not $SkipRegressions) {
    $productionExe = Join-Path $RepoRoot 'HwaSim_IR\Bin\HwaSim_IR.exe'
    $d2Exe = Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\Bin\HwaSim_IR.exe'
    $backupExe = Join-Path $LogDir 'HwaSim_IR.pre_d2_acceptance.exe'
    if ((Test-Path -LiteralPath $productionExe) -and (Test-Path -LiteralPath $d2Exe)) {
        Copy-Item -LiteralPath $productionExe -Destination $backupExe -Force
        $originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $productionExe).Hash
        try {
            Copy-Item -LiteralPath $d2Exe -Destination $productionExe -Force
            Invoke-Gate 'R1 protocol / route regression' 'regression.txt' {
                & (Join-Path $RepoRoot 'tools\r1_runtime_route_smoke.ps1') -StartupDelaySec 6
            } 'r1_runtime_route_smoke.ps1'
            Invoke-Gate 'TCP Packet v3 H264 regression' 'regression.txt' {
                & (Join-Path $RepoRoot 'tools\v4_packet_v3_acceptance.ps1') -Cases @('v3_h264_all') -Seconds 8 -FFmpegRoot $FFmpegRoot
            } 'v4_packet_v3_acceptance.ps1 v3_h264_all'
            Invoke-Gate 'TCP reconnect and reset/init IDR regression' 'regression.txt' {
                & (Join-Path $RepoRoot 'tools\v3_h264_recovery_smoke.ps1') -FFmpegRoot $FFmpegRoot -SkipFallback -SkipMp4Validation
            } 'v3_h264_recovery_smoke.ps1 with Windows FFmpeg'
        }
        finally {
            Copy-Item -LiteralPath $backupExe -Destination $productionExe -Force
            $restoredHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $productionExe).Hash
            Add-Log 'regression.txt' @(
                ('productionOriginalSha256=' + $originalHash),
                ('productionRestoredSha256=' + $restoredHash)
            )
            if ($restoredHash -ne $originalHash) { throw 'production executable restore hash mismatch' }
        }
    }
    else {
        $results.Add([pscustomobject]@{ Gate='Windows D2 regressions'; Status='FAIL'; Detail='required executable missing' })
    }
}

if ($EvidenceLogDir) {
    Invoke-Gate 'DDS payload audit hashes' 'payload_audit.txt' {
        $pairs = @(
            @('windows_h264_full_drain_sender_audit.h264', 'windows_h264_full_drain_receiver.h264'),
            @('windows_rawgray8_full_drain_sender_audit.gray8', 'windows_rawgray8_full_drain_receiver.gray8'),
            @('hwasim_rawbgr24_sender_audit.bgr24', 'hwasim_rawbgr24_receiver.bgr24'),
            @('rk_d2_h264_163_audit.h264', 'rk_h264_163_receiver.h264'),
            @('rk_d2_raw165_audit.gray8', 'rk_raw165_receiver.gray8'),
            @('rk_tcp_dds_sender_audit.h264', 'rk_tcp_dds_receiver.h264')
        )
        foreach ($pair in $pairs) {
            $source = Join-Path $EvidenceLogDir $pair[0]
            $received = Join-Path $EvidenceLogDir $pair[1]
            if (-not (Test-Path -LiteralPath $source) -or -not (Test-Path -LiteralPath $received)) {
                throw "missing audit pair $($pair[0]) / $($pair[1])"
            }
            $sourceHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash
            $receivedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $received).Hash
            "$($pair[0])=$sourceHash"
            "$($pair[1])=$receivedHash"
            if ($sourceHash -ne $receivedHash) { throw "payload mismatch for $($pair[0])" }
        }
    } ('Validate payload audit pairs in ' + $EvidenceLogDir)
}

if (-not $SkipRemote) {
    $sshArgs = @('-o', 'ConnectTimeout=10', '-o', 'StrictHostKeyChecking=accept-new')
    if ($SshKey) { $sshArgs += @('-i', $SshKey, '-o', 'BatchMode=yes') }
    $vmTarget = "$VmUser@$VmHost"
    $cmakeOptions = '-DCMAKE_BUILD_TYPE=Release -DHWASIMIR_ENABLE_RKMPP=ON -DRKMPP_ROOT=/home/linaro/sysroots/rk3588-mpp -DHWASIMIR_ENABLE_ZRDDS=ON -DZRDDS_ROOT=/home/linaro/sysroots/zrdds-aarch64 -DHWASIMIR_ENABLE_FFMPEG=OFF -DPANDA3D_ROOT=/opt/panda3d-aarch64 -DOpenCV_DIR=/usr/lib/aarch64-linux-gnu/cmake/opencv4'
    Invoke-Gate 'AArch64 MPP+ZRDDS command-line build' 'build_aarch64.txt' {
        & ssh.exe @sshArgs $vmTarget "cmake -S $VmProjectRoot -B $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh -G Ninja $cmakeOptions && cmake --build $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh -j4 && file $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh/HwaSim_IR && readelf -d $VmProjectRoot/cmake-build-release-aarch64-rk3588-ssh/HwaSim_IR"
    } 'VM configure/build/file/readelf using the CLion profile directory'
}

Invoke-Gate 'git diff --check' 'summary.txt' {
    $checkOutput = & cmd.exe /d /c "git -C `"$RepoRoot`" diff --check 2>nul"
    if ($LASTEXITCODE -ne 0) { throw ($checkOutput -join [Environment]::NewLine) }
    $checkOutput
} 'git diff --check'

$failed = @($results | Where-Object { $_.Status -eq 'FAIL' })
$summaryLines = @('D2 acceptance summary', ('logDir=' + $LogDir))
foreach ($result in $results) {
    $summaryLines += ('[' + $result.Status + '] ' + $result.Gate + ' - ' + $result.Detail)
}
$summaryLines += ('overall=' + $(if ($failed.Count -eq 0) { 'PASS' } else { 'FAIL' }))
Add-Log 'summary.txt' $summaryLines
$results | Format-Table -AutoSize
Write-Host "D2 logs: $LogDir"
if ($failed.Count -ne 0) { exit 1 }
