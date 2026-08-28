[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BoardLog,
    [Parameter(Mandatory=$true)][string]$VideoDisplayLog,
    [Parameter(Mandatory=$true)][string]$FirstFramePng
)

$ErrorActionPreference = 'Stop'
$failures = New-Object System.Collections.Generic.List[string]

if (-not (Test-Path -LiteralPath $BoardLog)) { throw "missing board log: $BoardLog" }
if (-not (Test-Path -LiteralPath $VideoDisplayLog)) { throw "missing VideoDisplay log: $VideoDisplayLog" }

$board = [IO.File]::ReadAllText($BoardLog)
$video = [IO.File]::ReadAllText($VideoDisplayLog)

if ($board -notmatch '\[DdsVideoConfig\].*exists=1') { $failures.Add('DDS QoS preflight missing or failed') }
if ($board -notmatch '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1') { $failures.Add('ARM/Mali hardware GPU gate failed') }
if ($board -match 'llvmpipe|hardwareGpu=0') { $failures.Add('software renderer detected') }
if ($board -match '\[StartupFatal\]|Address already in use|绑定UDP端口失败') { $failures.Add('UDP startup/bind failure detected') }
if ($board -match 'GL error 0x502|GL_INVALID_OPERATION') { $failures.Add('GL invalid-operation error detected') }
if ($video -notmatch '\[DdsVideoReceiverSample\]') { $failures.Add('no DDS VideoDisplay Sample') }
if ($video -notmatch '\[H264DecodeSuccess\]') { $failures.Add('no H264 decode success') }

$senderMatches = [regex]::Matches($board, 'sentSamples=(\d+).*writeErrors=(\d+).*droppedSamples=(\d+)')
$receiverMatches = [regex]::Matches($video, '\[DdsVideoReceiverPerf\] receivedSamples=(\d+).*ddsErrors=(\d+)')
if ($senderMatches.Count -eq 0 -or $receiverMatches.Count -eq 0) {
    $failures.Add('final sender/receiver statistics missing')
}
else {
    $s = $senderMatches[$senderMatches.Count - 1]
    $r = $receiverMatches[$receiverMatches.Count - 1]
    $sent = [int64]$s.Groups[1].Value
    $received = [int64]$r.Groups[1].Value
    $writerErrors = [int64]$s.Groups[2].Value
    $dropped = [int64]$s.Groups[3].Value
    $readerErrors = [int64]$r.Groups[2].Value
    if ($sent -le 0 -or $sent -ne $received -or $writerErrors -ne 0 -or $dropped -ne 0 -or $readerErrors -ne 0) {
        $failures.Add("DDS counts invalid sent=$sent received=$received writerErrors=$writerErrors dropped=$dropped readerErrors=$readerErrors")
    }
}

$diagMatches = [regex]::Matches($video, '\[DdsFrameDiag\].*min=([-+0-9.]+).*max=([-+0-9.]+).*mean=([-+0-9.]+).*stddev=([-+0-9.]+).*nonZeroRatio=([-+0-9.]+)')
if ($diagMatches.Count -eq 0) {
    $failures.Add('decoded pixel diagnostics missing')
}
else {
    $diag = $diagMatches[$diagMatches.Count - 1]
    $minimum = [double]$diag.Groups[1].Value
    $maximum = [double]$diag.Groups[2].Value
    $stddev = [double]$diag.Groups[4].Value
    $nonZeroRatio = [double]$diag.Groups[5].Value
    if ($maximum -le $minimum -or $stddev -le 0 -or $nonZeroRatio -le 0) {
        $failures.Add("decoded frame is empty or constant min=$minimum max=$maximum stddev=$stddev nonZeroRatio=$nonZeroRatio")
    }
}
if (-not (Test-Path -LiteralPath $FirstFramePng)) { $failures.Add("first decoded PNG missing: $FirstFramePng") }

if ($failures.Count -ne 0) {
    foreach ($failure in $failures) { Write-Error "[D31RuntimeGate][FAIL] $failure" -ErrorAction Continue }
    Write-Output "[D31RuntimeGate] result=FAIL failures=$($failures.Count)"
    exit 1
}

$s = $senderMatches[$senderMatches.Count - 1]
$r = $receiverMatches[$receiverMatches.Count - 1]
Write-Output "[D31RuntimeGate] result=PASS sent=$($s.Groups[1].Value) received=$($r.Groups[1].Value) hardwareGpu=1 decodedPixelContent=1"
exit 0
