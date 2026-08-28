[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$LogDir = '',
    [string]$EvidenceLogDir = '',
    [string]$FFmpegRoot = $env:FFMPEG_ROOT,
    [string]$VmHost = '192.168.203.128',
    [string]$VmUser = 'linaro',
    [string]$BoardHost = '192.168.1.116',
    [string]$BoardUser = 'root',
    [string]$SshKey = '',
    [int]$DurationSec = 60,
    [switch]$SkipBuild,
    [switch]$SkipDeploy,
    [switch]$SkipRegressions,
    [switch]$RunRemoteSmoke,
    [switch]$ValidateEvidenceOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
if (-not $LogDir) { $LogDir = Join-Path $RepoRoot ('logs\dds-d3-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if (-not $EvidenceLogDir) { $EvidenceLogDir = $LogDir }
if (-not $FFmpegRoot) {
    $FFmpegRoot = Join-Path $RepoRoot '.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1'
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$utf8 = New-Object Text.UTF8Encoding($false)
$results = New-Object System.Collections.Generic.List[object]
$baseline = 'b46458b781e9943667305a4e7341c7f3acd8789d'
$logNames = @('environment.txt','build_windows.txt','build_aarch64.txt','deploy.txt','matrix.txt','resources.txt','regression.txt','summary.txt')
foreach ($name in $logNames) {
    $path = Join-Path $LogDir $name
    if (-not (Test-Path -LiteralPath $path)) { New-Item -ItemType File -Path $path | Out-Null }
}

function Add-Log([string]$Name, [object[]]$Lines) {
    [IO.File]::AppendAllLines((Join-Path $LogDir $Name), [string[]]$Lines, $utf8)
}

function Add-Result([string]$Gate, [string]$Status, [string]$Detail) {
    $results.Add([pscustomobject]@{ Gate=$Gate; Status=$Status; Detail=$Detail })
}

function Invoke-Gate([string]$Gate, [string]$LogName, [scriptblock]$Command, [string]$Display) {
    Add-Log $LogName @('', ('COMMAND: ' + $Display))
    try {
        $global:LASTEXITCODE = 0
        $output = & $Command 2>&1
        $code = $LASTEXITCODE
        if ($null -eq $code) { $code = 0 }
        Add-Log $LogName @($output, ('EXIT_CODE: ' + $code))
        if ($code -ne 0) { throw "exit=$code" }
        Add-Result $Gate 'PASS' $Display
    }
    catch {
        Add-Log $LogName @(('ERROR: ' + $_.Exception.Message))
        Add-Result $Gate 'FAIL' $_.Exception.Message
    }
}

function Get-SshArgs {
    $args = @('-o','ConnectTimeout=10','-o','ServerAliveInterval=10','-o','StrictHostKeyChecking=accept-new')
    if ($SshKey) { $args += @('-i',$SshKey,'-o','BatchMode=yes') }
    return $args
}

function Invoke-Ssh([string]$Target, [string]$Command) {
    & ssh.exe @(Get-SshArgs) $Target $Command
    if ($LASTEXITCODE -ne 0) { throw "ssh failed: $Target" }
}

function Invoke-Scp([string[]]$Sources, [string]$Target) {
    & scp.exe @(Get-SshArgs) @Sources $Target
    if ($LASTEXITCODE -ne 0) { throw "scp failed: $Target" }
}

function Assert-CountPair([string]$Name, [string]$SenderLog, [string]$ReceiverLog) {
    if (-not (Test-Path -LiteralPath $SenderLog)) { throw "missing sender log: $SenderLog" }
    if (-not (Test-Path -LiteralPath $ReceiverLog)) { throw "missing receiver log: $ReceiverLog" }
    $senderText = [IO.File]::ReadAllText($SenderLog)
    $receiverText = [IO.File]::ReadAllText($ReceiverLog)
    $senderMatches = [regex]::Matches($senderText, 'sentSamples=(\d+).*writeErrors=(\d+).*droppedSamples=(\d+)')
    $receiverMatches = [regex]::Matches($receiverText, 'receivedSamples=(\d+).*ddsErrors=(\d+)')
    if ($senderMatches.Count -eq 0 -or $receiverMatches.Count -eq 0) { throw "$Name statistics not found" }
    $s = $senderMatches[$senderMatches.Count - 1]
    $r = $receiverMatches[$receiverMatches.Count - 1]
    $sent = [int64]$s.Groups[1].Value
    $received = [int64]$r.Groups[1].Value
    $writerErrors = [int64]$s.Groups[2].Value
    $dropped = [int64]$s.Groups[3].Value
    $readerErrors = [int64]$r.Groups[2].Value
    if ($sent -ne $received -or $writerErrors -ne 0 -or $dropped -ne 0 -or $readerErrors -ne 0) {
        throw "$Name sent=$sent received=$received writerErrors=$writerErrors dropped=$dropped readerErrors=$readerErrors"
    }
    "$Name sent=$sent received=$received writerErrors=0 dropped=0 readerErrors=0"
}

function Export-ResourceSummary([string]$SourceDir, [string]$Destination) {
    $lines = @('case,durationSec,cpuPctOneCore,rssPeakMiB,hwmPeakMiB,threadsPeak,fdPeak,netTxMiBs,netRxMiBs')
    Get-ChildItem -LiteralPath $SourceDir -Filter '*.resource.tsv' -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
        $rows = @(Get-Content -LiteralPath $_.FullName | Where-Object { $_ -match '^\d+\t' } | ForEach-Object {
            $c = $_ -split "`t", -1
            if ($c.Count -ge 12 -and $c[2] -match '^\d+,\d+,\d+$') {
                [pscustomobject]@{ Up=[double]$c[1]; Proc=$c[2]; Rss=[double]$c[4]; Hwm=[double]$c[5]; Threads=[double]$c[6]; Fd=[double]$c[7]; Tx=[double]$c[10]; Rx=[double]$c[11] }
            }
        } | Where-Object { $_.Rss -gt 100000 })
        if ($rows.Count -lt 2) { return }
        $a = $rows[0]; $b = $rows[-1]
        $pa = $a.Proc -split ','; $pb = $b.Proc -split ','
        $duration = $b.Up - $a.Up
        $cpu = (([double]$pb[0] + [double]$pb[1]) - ([double]$pa[0] + [double]$pa[1])) / 100 / $duration * 100
        $lines += ('{0},{1:F2},{2:F2},{3:F2},{4:F2},{5},{6},{7:F3},{8:F3}' -f $_.BaseName,$duration,$cpu,(($rows|Measure-Object Rss -Maximum).Maximum/1024),(($rows|Measure-Object Hwm -Maximum).Maximum/1024),(($rows|Measure-Object Threads -Maximum).Maximum),(($rows|Measure-Object Fd -Maximum).Maximum),(($b.Tx-$a.Tx)/1MB/$duration),(($b.Rx-$a.Rx)/1MB/$duration))
    }
    [IO.File]::WriteAllLines($Destination, $lines, $utf8)
    return $lines
}

# Start-Process on Windows rejects an environment containing both PATH and Path.
$pathEntries = @([Environment]::GetEnvironmentVariables('Process').GetEnumerator() | Where-Object { $_.Key -imatch '^path$' })
if ($pathEntries.Count -gt 1) {
    $pathValue = ($pathEntries | Where-Object { $_.Key -ceq 'Path' } | Select-Object -First 1).Value
    if (-not $pathValue) { $pathValue = $pathEntries[0].Value }
    [Environment]::SetEnvironmentVariable('PATH', $null, 'Process')
    [Environment]::SetEnvironmentVariable('Path', [string]$pathValue, 'Process')
}

$head = (& git -C $RepoRoot rev-parse HEAD).Trim()
$status = & git -C $RepoRoot status --short
Add-Log 'environment.txt' @('COMMAND: git rev-parse HEAD; git status --short; git diff --check', ('HEAD=' + $head), $status, ('ZRDDS_HOME=' + $env:ZRDDS_HOME), ('VM=' + $VmUser + '@' + $VmHost), ('BOARD=' + $BoardUser + '@' + $BoardHost), 'Licence contents and passwords are never logged.')
Add-Result 'baseline HEAD recorded' $(if ($head -eq $baseline) {'PASS'} else {'FAIL'}) ('HEAD=' + $head)
Invoke-Gate 'git diff --check' 'environment.txt' {
    $quotedRoot = $RepoRoot.Replace('"', '""')
    & cmd.exe /d /c "git -C `"$quotedRoot`" diff --check 2>&1"
} 'git diff --check'

if (-not $ValidateEvidenceOnly -and -not $SkipBuild) {
    $msbuild = 'C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe'
    Invoke-Gate 'Windows HwaSim_IR Release' 'build_windows.txt' { & $msbuild (Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\HwaSim_IR.vcxproj') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo } 'VS2015 v140 Release x64 ZRDDS+FFmpeg+AVFormat'
    Invoke-Gate 'Windows VideoDisplay Release' 'build_windows.txt' { & $msbuild (Join-Path $RepoRoot 'HwaSim_IR_VideoDisplay\HwaSim_IR_VideoDisplay.sln') /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:QtInstall='D:\Qt\Qt5.12.12\5.12.12\msvc2015_64' /p:FFMPEG_ROOT=$FFmpegRoot /verbosity:minimal /nologo } 'VS2015 v140 VideoDisplay Release x64'
    Invoke-Gate 'Customer Receiver Release' 'build_windows.txt' { & $msbuild (Join-Path $RepoRoot 'DDS\HwaSimIRVideoD1Smoke.sln') /m /t:Build /p:Configuration=Release /p:Platform=x64 /verbosity:minimal /nologo } 'VS2015 v140 customer receiver/sender Release x64'
}

if (-not $ValidateEvidenceOnly -and -not $SkipDeploy) {
    if (-not $SshKey) {
        Add-Result 'VM build / board deploy' 'FAIL' 'Unattended remote execution requires -SshKey; passwords are intentionally not embedded.'
    }
    else {
        Invoke-Gate 'VM build / board deploy' 'deploy.txt' {
            & (Join-Path $RepoRoot 'tools\codex_rk3588_pipeline.ps1') -RepoRoot $RepoRoot -VmHost $VmHost -VmUser $VmUser -BoardHost $BoardHost -BoardUser $BoardUser -SshKey $SshKey -RunSeconds 10 -DdsH264Smoke
        } 'Reuse codex_rk3588_pipeline.ps1 for build/deploy and hard-gated Mali DDS H264 pixel smoke'
    }
}

if (-not $ValidateEvidenceOnly -and -not $SkipRegressions) {
    $productionExe = Join-Path $RepoRoot 'HwaSim_IR\Bin\HwaSim_IR.exe'
    $d3Exe = Join-Path $RepoRoot 'HwaSim_IR\HwaSim_IR\Bin\HwaSim_IR.exe'
    $backupExe = Join-Path $LogDir 'HwaSim_IR.pre_d3_acceptance.exe'
    if ((Test-Path -LiteralPath $productionExe) -and (Test-Path -LiteralPath $d3Exe)) {
        Copy-Item -LiteralPath $productionExe -Destination $backupExe -Force
        $originalHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $productionExe).Hash
        try {
            Copy-Item -LiteralPath $d3Exe -Destination $productionExe -Force
            Invoke-Gate 'R1 route/protocol smoke' 'regression.txt' {
                & (Join-Path $RepoRoot 'tools\r1_runtime_route_smoke.ps1') -StartupDelaySec 6
            } 'r1_runtime_route_smoke.ps1'
            Invoke-Gate 'TCP Packet v3 H264' 'regression.txt' {
                # Run this legacy matrix in a child PowerShell process because
                # it predates StrictMode and otherwise inherits this script's
                # StrictMode scope.
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $RepoRoot 'tools\v4_packet_v3_acceptance.ps1') -Cases v3_h264_all -Seconds 8 -FFmpegRoot $FFmpegRoot
            } 'v4_packet_v3_acceptance.ps1 v3_h264_all'
            Invoke-Gate 'TCP reconnect / reset IDR' 'regression.txt' {
                & (Join-Path $RepoRoot 'tools\v3_h264_recovery_smoke.ps1') -FFmpegRoot $FFmpegRoot -SkipFallback -SkipMp4Validation
            } 'v3_h264_recovery_smoke.ps1'
        }
        finally {
            Copy-Item -LiteralPath $backupExe -Destination $productionExe -Force
            $restoredHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $productionExe).Hash
            Add-Log 'regression.txt' @(('productionOriginalSha256=' + $originalHash), ('productionRestoredSha256=' + $restoredHash))
            if ($restoredHash -ne $originalHash) { throw 'production executable restore hash mismatch' }
        }
    }
    else {
        Add-Result 'Windows TCP/R1 regressions' 'FAIL' 'required built/production executable missing'
    }
}

if ($RunRemoteSmoke) {
    if (-not $SshKey) { throw '-RunRemoteSmoke requires passwordless -SshKey; no password is stored by this script.' }
    Add-Log 'matrix.txt' @(
        'The unattended remote smoke reuses the established D2 runtime/config/receiver helpers and deployed AArch64 binary.',
        'The complete long-running D3 matrix is validated from EvidenceLogDir so an interrupted site run can resume without deleting earlier evidence.',
        'Remote smoke is intentionally fail-fast: any ssh/scp/build/process/statistics error is a failed gate.'
    )
    # The established pipeline performs the complete Windows -> VM -> board path.
    # D3 case launching remains explicit in logs so an interrupted run can resume
    # from a single case without deleting earlier evidence.
    Invoke-Gate 'remote production smoke after deploy' 'matrix.txt' {
        & (Join-Path $RepoRoot 'tools\codex_rk3588_pipeline.ps1') -RepoRoot $RepoRoot -VmHost $VmHost -VmUser $VmUser -BoardHost $BoardHost -BoardUser $BoardUser -SshKey $SshKey -SkipSourceSync -SkipBuild -SkipDeploy -RunSeconds $DurationSec -DdsH264Smoke
    } 'Run deployed precise MPP+DDS H264 path with existing UDP stimulus'
}

if (Test-Path -LiteralPath $EvidenceLogDir) {
    $boardEvidence = Join-Path $EvidenceLogDir 'board-import\dds-d3-20260827-043540'
    if (-not (Test-Path -LiteralPath $boardEvidence)) { $boardEvidence = $EvidenceLogDir }
    $pairs = @(
        @('precise DDS H264 only','B5_dds_h264_only.out.log','B5_dds_h264_only.receiver.out.txt'),
        @('precise TCP+DDS H264','C_tcp_dds_h264.out.log','C_tcp_dds_h264.receiver.out.txt'),
        @('precise TCP+DDS+record','D_tcp_dds_record.out.log','D_tcp_dds_record.receiver.out.txt'),
        @('RawGray8 800x800','RawGray8_60s.out.log','rawgray60.receiver.out.txt'),
        @('RawBGR24 target30','RawBGR24_target30.out.log','rawbgr30.receiver.out.txt'),
        @('RawBGR24 target45','RawBGR24_target45.out.log','rawbgr45.receiver.out.txt'),
        @('Sync H264','Sync_H264_20s.out.log','sync_h264.receiver.out.txt'),
        @('two-channel precise','Dual2DDS_precise.out.log','dual2_dds_precise.receiver.out.txt'),
        @('two-channel coarse','Dual2DDS_coarse.out.log','dual2_dds_coarse.receiver.out.txt'),
        @('maximum-load precise','DualMax_precise.out.log','dualmax_precise.receiver.out.txt'),
        @('maximum-load coarse','DualMax_coarse.out.log','dualmax_coarse.receiver.out.txt')
    )
    foreach ($pair in $pairs) {
        $sender = Join-Path $boardEvidence $pair[1]
        $receiver = Join-Path $EvidenceLogDir $pair[2]
        Invoke-Gate ($pair[0] + ' exact counts') 'matrix.txt' { Assert-CountPair $pair[0] $sender $receiver } ($pair[1] + ' <-> ' + $pair[2])
    }
    Invoke-Gate 'resource metrics parsed' 'resources.txt' {
        Export-ResourceSummary $boardEvidence (Join-Path $LogDir 'resource_summary.csv')
    } 'Parse /proc and NIC resource TSV files'
}

$failed = @($results | Where-Object { $_.Status -eq 'FAIL' })
$summary = @('D3 acceptance summary', ('logDir=' + $LogDir), ('evidenceLogDir=' + $EvidenceLogDir))
foreach ($result in $results) { $summary += ('[' + $result.Status + '] ' + $result.Gate + ' - ' + $result.Detail) }
$summary += ('overall=' + $(if ($failed.Count -eq 0) {'PASS'} else {'FAIL'}))
[IO.File]::WriteAllLines((Join-Path $LogDir 'summary.txt'), $summary, $utf8)
$results | Format-Table -AutoSize
Write-Host "D3 logs: $LogDir"
if ($failed.Count -ne 0) { exit 1 }
