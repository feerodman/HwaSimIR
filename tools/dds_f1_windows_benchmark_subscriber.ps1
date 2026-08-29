[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$Mode,
    [Parameter(Mandatory=$true)][UInt64]$PayloadBytes,
    [Parameter(Mandatory=$true)][UInt64]$Frames,
    [Parameter(Mandatory=$true)][int]$Domain,
    [Parameter(Mandatory=$true)][string]$Topic,
    [int]$TimeoutSec = 45,
    [string]$LocalIp = '192.168.1.188',
    [string]$RepoRoot = '',
    [string]$ZrddsRoot = 'F:\Programs\ZRDDS\ZRDDS_MinGW7.3.0\ZRDDS-2.4.5',
    [string]$BuildDir = '',
    [string]$QosFile = ''
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = Split-Path -Parent $PSScriptRoot }
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot 'logs\dds-f1-20260828-190155\benchmark-mingw-final'
}
if (-not $QosFile) {
    $QosFile = Join-Path $RepoRoot 'tools\dds_f1_qos\ZRDDS_PROTOCOL_QOS_WINDOWS_192.168.1.188.xml'
}
$exe = Join-Path $BuildDir 'HwaSimIRVideoTransportBenchmark.exe'
foreach ($required in @($exe, $QosFile, (Join-Path $BuildDir 'zrddslicence.lic'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "missing: $required" }
}

# Some Codex hosts expose both PATH and Path. Start-Process rejects that duplicate.
$basePath = [Environment]::GetEnvironmentVariable('PATH', 'Process')
[Environment]::SetEnvironmentVariable('Path', $null, 'Process')
$env:PATH = (Join-Path $ZrddsRoot 'lib') + ';D:\Qt\Qt5.12.12\Tools\mingw730_64\bin;' + $basePath
$env:ZRDDS_HOME = $ZrddsRoot

$safeMode = ("{0}_{1}_{2}" -f $Mode, $PayloadBytes, $Frames) -replace '[^A-Za-z0-9_-]', '_'
$stdout = Join-Path $BuildDir ("cross_${safeMode}_sub.out.log")
$stderr = Join-Path $BuildDir ("cross_${safeMode}_sub.err.log")
$arguments = "--role sub --mode $Mode --payload-bytes $PayloadBytes --frames $Frames --domain $Domain --timeout-sec $TimeoutSec --qos $QosFile --topic $Topic"

$rx0 = $null
$adapter = $null
try {
    $ip = Get-NetIPAddress -AddressFamily IPv4 -IPAddress $LocalIp -ErrorAction Stop | Select-Object -First 1
    $adapter = Get-NetAdapter -InterfaceIndex $ip.InterfaceIndex -ErrorAction Stop
    $rx0 = (Get-NetAdapterStatistics -Name $adapter.Name).ReceivedBytes
} catch { }

$process = Start-Process -FilePath $exe -WorkingDirectory $BuildDir -ArgumentList $arguments -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
$start = Get-Date
$cpu0 = 0.0
$cpuLast = 0.0
$rssPeak = 0L
$threadPeak = 0
$handlePeak = 0
while (-not $process.HasExited) {
    $process.Refresh()
    if ($null -ne $process.TotalProcessorTime) { $cpuLast = $process.TotalProcessorTime.TotalSeconds }
    if ($process.WorkingSet64 -gt $rssPeak) { $rssPeak = $process.WorkingSet64 }
    if ($process.Threads.Count -gt $threadPeak) { $threadPeak = $process.Threads.Count }
    if ($process.HandleCount -gt $handlePeak) { $handlePeak = $process.HandleCount }
    if (((Get-Date) - $start).TotalSeconds -gt ($TimeoutSec + 15)) {
        Stop-Process -Id $process.Id -Force
        throw "subscriber wrapper timeout: mode=$Mode"
    }
    Start-Sleep -Milliseconds 250
}
$process.WaitForExit()
$exitCode = $process.ExitCode
if ($null -eq $exitCode) { $exitCode = 0 }
$elapsed = ((Get-Date) - $start).TotalSeconds
$cpuPct = if ($elapsed -gt 0) { ($cpuLast - $cpu0) / $elapsed * 100.0 } else { 0.0 }
$rxMiBPerSec = 0.0
if ($null -ne $rx0 -and $null -ne $adapter) {
    try {
        $rx1 = (Get-NetAdapterStatistics -Name $adapter.Name).ReceivedBytes
        if ($elapsed -gt 0) { $rxMiBPerSec = ($rx1 - $rx0) / 1MB / $elapsed }
    } catch { }
}

Get-Content -LiteralPath $stdout
Get-Content -LiteralPath $stderr
'[BenchmarkResource] role=sub mode={0} cpuAvgPct={1:F3} rssPeakKiB={2} threadsPeak={3} handlesPeak={4} netRxMiBPerSec={5:F3} elapsedSec={6:F3} exitCode={7}' -f $Mode, $cpuPct, [Math]::Round($rssPeak / 1KB), $threadPeak, $handlePeak, $rxMiBPerSec, $elapsed, $exitCode
exit $exitCode
