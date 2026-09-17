[CmdletBinding()]
param(
    [string]$OutputDirectory = 'logs/p11/work/controlled_samples_sender_init',
    [ValidateSet('0x55', '0x66')]
    [string]$TargetType = '0x66',
    [ValidateSet(0, 1, 2)]
    [int]$SensorBand = 0,
    [string]$FixturePath = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = if ([IO.Path]::IsPathRooted($OutputDirectory)) { $OutputDirectory } else { Join-Path $root $OutputDirectory }
New-Item -ItemType Directory -Force -Path $out | Out-Null

$exe = Join-Path $root 'build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe'
$fixture = if ($FixturePath) {
    if ([IO.Path]::IsPathRooted($FixturePath)) { $FixturePath } else { Join-Path $root $FixturePath }
} elseif ($TargetType -eq '0x55') {
    Join-Path $root 'tools\p11_inputs\civil_van_close_lookat.json'
} else {
    Join-Path $root 'tools\p11_inputs\controlled_samples_close_lookat.json'
}
$stimulusInput = Join-Path $root 'DataDrivenTestQT\1.txt'
$networkPath = Join-Path $out 'NetworkConfig.loopback.ini'
$stdoutPath = Join-Path $out 'sender_init.out.log'
$stderrPath = Join-Path $out 'sender_init.err.log'
foreach ($path in @($exe, $fixture, $stimulusInput)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing smoke-test input: $path" }
}

$network = @"
[Identity]
platID=1001
sensorID=2

[RenderControl]
simMode=1
videoFps=60
sendStepMs=16.666666666666668

[UDP]
localIp=127.0.0.1
localPort=42991
remoteIp=127.0.0.1
remotePort=42992

[Demo]
InputFile=1.txt
TargetType=0x11
envSky=1

[SensorInit]
trackerSensorBand=$SensorBand
trackerSensorWidth=800
trackerSensorHeight=800
realtimeAnnotation=1
saveMP4En=0
h264En=0
"@
[IO.File]::WriteAllText($networkPath, $network, [Text.UTF8Encoding]::new($false))
Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

$oldTargetType = $env:P6TestTargetType
$oldQtLog = $env:QT_FORCE_STDERR_LOGGING
$env:P6TestTargetType = $TargetType
$env:QT_FORCE_STDERR_LOGGING = '1'
$process = $null
try {
    $arguments = @(
        "--network-config=$networkPath",
        "--input-file=$stimulusInput",
        '--control-transport=udp',
        "--sensor-band=$SensorBand",
        '--init-only'
    )
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $exe
    $startInfo.Arguments = $arguments -join ' '
    $startInfo.WorkingDirectory = Split-Path -Parent $exe
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw 'DataDrivenTestQT --init-only could not start' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(10000)) {
        $process.Kill()
        $process.WaitForExit()
        throw 'DataDrivenTestQT --init-only did not exit within 10 seconds'
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    [IO.File]::WriteAllText($stdoutPath, $stdoutTask.Result, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderrPath, $stderrTask.Result, [Text.UTF8Encoding]::new($false))
    if ($exitCode -ne 0) { throw "DataDrivenTestQT --init-only failed exit=$exitCode" }
}
finally {
    $env:P6TestTargetType = $oldTargetType
    $env:QT_FORCE_STDERR_LOGGING = $oldQtLog
    if ($process -and -not $process.HasExited) { $process.Kill() }
}

$combined = ((Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue) + "`n" +
    (Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue))
$expectedResv1 = if ($TargetType -eq '0x55') { 1 } else { 0 }
$expectedResv2 = if ($TargetType -eq '0x66') { 1 } else { 0 }
$expectedType = $TargetType.ToUpperInvariant()
$expected = @(
    '[ProtocolLayout] component=DataDrivenTestQT ControlP2cX1ObjTrackingCmd=24 InitP2cObjectTrackingCmd=385 DisplayC2cObjTrackingData=506 InitAckC2pObjectTrackingCmd=17',
    "[StimTargetPool] requestedType=$expectedType resv1Count=$expectedResv1 resv2Count=$expectedResv2 protocolLayoutUnchanged=1",
    '[StimTransportConfig] ControlTransport=udp source=command_line',
    "sensorBand=$SensorBand bytes=385"
)
foreach ($needle in $expected) {
    if (-not $combined.Contains($needle)) { throw "Sender init smoke log missing: $needle" }
}

$summary = [ordered]@{
    result = 'PASS'
    durationPolicy = 'init-only; automatic exit after 1.5 s; no realtime or formal matrix'
    targetType = $TargetType
    resv1Count = $expectedResv1
    resv2Count = $expectedResv2
    sensorBand = $SensorBand
    protocolLayout = [ordered]@{ control=24; init=385; realtime=506; ack=17 }
    executable = [ordered]@{
        path = $exe
        sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    fixture = [ordered]@{
        path = $fixture
        sha256 = (Get-FileHash -LiteralPath $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    logs = @(
        [ordered]@{ path=$stdoutPath; sha256=(Get-FileHash -LiteralPath $stdoutPath -Algorithm SHA256).Hash.ToLowerInvariant() },
        [ordered]@{ path=$stderrPath; sha256=(Get-FileHash -LiteralPath $stderrPath -Algorithm SHA256).Hash.ToLowerInvariant() }
    )
}
[IO.File]::WriteAllText((Join-Path $out 'sender_init_smoke.json'), ($summary | ConvertTo-Json -Depth 6) + "`n", [Text.UTF8Encoding]::new($false))
$summary | ConvertTo-Json -Depth 6 -Compress
