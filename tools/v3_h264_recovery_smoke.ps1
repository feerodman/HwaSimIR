param(
    [string]$FFmpegRoot = $env:FFMPEG_ROOT,
    [switch]$SkipFallback,
    [switch]$SkipMp4Validation
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWork = Split-Path -Parent $hwaExe
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWork = Split-Path -Parent $videoExe
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWork = Join-Path $root "DataDrivenTestQT"
$gxx = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$runDir = Join-Path $root "logs\v3-h264-recovery-$stamp"
$senderExe = Join-Path $runDir "r1_protocol_sender.exe"
$utf8 = New-Object Text.UTF8Encoding($false)
$testStart = Get-Date

foreach ($required in @($hwaExe, $videoExe, $stimExe, $gxx, (Join-Path $root "tools\r1_protocol_sender.cpp"))) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Required path not found: $required" }
}
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$processEnvironment = [Environment]::GetEnvironmentVariables("Process")
$pathEntries = @($processEnvironment.GetEnumerator() | Where-Object { $_.Key -imatch '^path$' })
if ($pathEntries.Count -gt 1) {
    $pathValue = ($pathEntries | Where-Object { $_.Key -ceq "Path" } | Select-Object -First 1).Value
    if ([string]::IsNullOrEmpty($pathValue)) { $pathValue = $pathEntries[0].Value }
    [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
    [Environment]::SetEnvironmentVariable("Path", [string]$pathValue, "Process")
}

& $gxx -std=c++11 -O2 -I $root (Join-Path $root "tools\r1_protocol_sender.cpp") -lws2_32 -o $senderExe
if ($LASTEXITCODE -ne 0) { throw "r1_protocol_sender build failed: $LASTEXITCODE" }

function Write-HwaConfig([string]$Path, [int]$UdpPort, [int]$StimPort, [int]$TcpPort) {
    $text = @"
[Identity]
channel=precise
platID=1001
sensorID=2
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=127.0.0.1
localPort=$UdpPort
remoteIp=127.0.0.1
remotePort=$StimPort

[TCP]
serverIp=127.0.0.1
serverPort=$TcpPort
"@
    [IO.File]::WriteAllText($Path, $text, $utf8)
}

function Write-StimConfig([string]$Path, [int]$StimPort, [int]$UdpPort) {
    $text = @"
[Identity]
channel=precise
platID=1001
sensorID=2

[RenderControl]
simMode=2
videoFps=60

[UDP]
localIp=127.0.0.1
localPort=$StimPort
remoteIp=127.0.0.1
remotePort=$UdpPort
"@
    [IO.File]::WriteAllText($Path, $text, $utf8)
}

function Write-VideoConfig([string]$Path, [int]$TcpPort) {
    $text = @"
[Identity]
channel=precise
platID=1001
sensorID=2

[Network]
ip=127.0.0.1
port=$TcpPort

[Recorder]
MaxRecordingQueueFrames=180
FlushTimeoutMs=10000
"@
    [IO.File]::WriteAllText($Path, $text, $utf8)
}

function Start-LoggedProcess(
    [string]$Name,
    [string]$FilePath,
    [string[]]$Arguments,
    [string]$WorkingDirectory
) {
    $stdout = Join-Path $runDir "$Name.out.log"
    $stderr = Join-Path $runDir "$Name.err.log"
    $process = Start-Process -FilePath $FilePath -ArgumentList $Arguments `
        -WorkingDirectory $WorkingDirectory -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    return [pscustomobject]@{ Name=$Name; Process=$process; Stdout=$stdout; Stderr=$stderr }
}

function Stop-LoggedProcess($Entry) {
    if ($null -eq $Entry -or $null -eq $Entry.Process) { return }
    $Entry.Process.Refresh()
    if (-not $Entry.Process.HasExited) { Stop-Process -Id $Entry.Process.Id -Force }
    $Entry.Process.WaitForExit()
}

function Read-Log([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return "" }
    return Get-Content -LiteralPath $Path -Raw
}

function Invoke-Sender([string[]]$Arguments) {
    $output = & $senderExe @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Protocol sender failed: $output" }
    [IO.File]::AppendAllLines((Join-Path $runDir "protocol-sender.log"), [string[]]$output, $utf8)
}

$basePort = 36000 + (Get-Random -Minimum 0 -Maximum 500)
$udpPort = $basePort
$stimPort = $basePort + 100
$tcpPort = $basePort + 200
$hwaConfig = Join-Path $runDir "precise.hwa.ini"
$stimConfig = Join-Path $runDir "precise.stim.ini"
$videoConfig = Join-Path $runDir "precise.video.ini"
Write-HwaConfig $hwaConfig $udpPort $stimPort $tcpPort
Write-StimConfig $stimConfig $stimPort $udpPort
Write-VideoConfig $videoConfig $tcpPort

$oldQtLog = $env:QT_FORCE_STDERR_LOGGING
$oldH264Encoder = $env:H264Encoder
$videoA = $null
$videoB = $null
$hwa = $null
$stim = $null
$fallbackVideo = $null
$fallbackHwa = $null
$fallbackStim = $null
$recoveryPass = $false
$fallbackPass = $SkipFallback.IsPresent
$mp4Pass = $SkipMp4Validation.IsPresent
$mp4Path = ""
$mp4Frames = 0

try {
    $env:QT_FORCE_STDERR_LOGGING = "1"
    $videoA = Start-LoggedProcess "video-before-reconnect" $videoExe @(
        "--network-config=$videoConfig", "--channel=precise", "--plat-id=1001", "--sensor-id=2") $videoWork
    Start-Sleep -Seconds 2
    $hwa = Start-LoggedProcess "hwa-recovery" $hwaExe @("--network-config", $hwaConfig) $hwaWork
    Start-Sleep -Seconds 5
    $stim = Start-LoggedProcess "stim-recovery" $stimExe @(
        "--network-config=$stimConfig", "--channel=precise", "--plat-id=1001", "--sensor-id=2",
        "--sim-mode=2", "--video-fps=60", "--phase1d-h264=1", "--save-mp4=1",
        "--phase1b-auto-seconds=26") $stimWork

    Start-Sleep -Seconds 12
    Stop-LoggedProcess $videoA
    $videoA = $null
    Start-Sleep -Seconds 2
    $videoB = Start-LoggedProcess "video-after-reconnect" $videoExe @(
        "--network-config=$videoConfig", "--channel=precise", "--plat-id=1001", "--sensor-id=2") $videoWork
    Start-Sleep -Seconds 4

    Invoke-Sender @("--type=control", "--port=$udpPort", "--plat-id=1001", "--command=1")
    Start-Sleep -Seconds 1
    Invoke-Sender @("--type=init", "--port=$udpPort", "--plat-id=1001", "--sensor-id=2",
        "--sim-mode=2", "--video-fps=60", "--h264=1", "--save-mp4=1")
    Start-Sleep -Seconds 1
    Invoke-Sender @("--type=control", "--port=$udpPort", "--plat-id=1001", "--command=2")

    if (-not $stim.Process.WaitForExit(25000)) { throw "Recovery stimulus timeout" }
    Start-Sleep -Seconds 5
    Stop-LoggedProcess $hwa
    $hwa = $null
    Stop-LoggedProcess $videoB
    $videoB = $null

    $hwaText = Read-Log (Join-Path $runDir "hwa-recovery.out.log")
    $videoText = Read-Log (Join-Path $runDir "video-after-reconnect.err.log")
	# HwaSim_IR uses several stdout producer threads; accept a valid low-frequency
	# marker even when another diagnostic interleaves ahead of its line prefix.
    $tcpConnectKeyRequests = ([regex]::Matches($hwaText, 'reason=tcp_connected')).Count
    $hasResetRequest = $hwaText -match 'reason=control_or_round_change'
    $hasInitRequest = $hwaText -match 'reason=initialization_forwarded'
    $hasRecoveredIdr = $videoText -match '(?m)^\[VideoDecoder\].*payloadCodec=h264_annexb.*keyFrame=1.*waitingForIdr=0'
    $hasH264Display = $videoText -match '(?m)^\[VideoPerf\].*displayFps=(?:59|6[0-9]).*activeCodec=h264_annexb'
    $decodeError = $videoText -match '(?m)^\[VideoPerf\].*h264DecodeErrors=[1-9]'
    $recoveryPass = $tcpConnectKeyRequests -ge 2 -and $hasResetRequest -and $hasInitRequest -and
        $hasRecoveredIdr -and $hasH264Display -and -not $decodeError

    if (-not $SkipMp4Validation) {
        $ffprobe = if ($FFmpegRoot) { Join-Path $FFmpegRoot "bin\ffprobe.exe" } else { "" }
        if (-not $ffprobe -or -not (Test-Path -LiteralPath $ffprobe)) {
            $foundProbe = Get-Command ffprobe -ErrorAction SilentlyContinue
            if ($foundProbe) { $ffprobe = $foundProbe.Source }
        }
        if (-not $ffprobe -or -not (Test-Path -LiteralPath $ffprobe)) {
            throw "ffprobe not found; pass -FFmpegRoot or use -SkipMp4Validation"
        }
        $candidateFiles = Get-ChildItem -LiteralPath (Join-Path $videoWork "MP4") -Filter output.mp4 -File -Recurse |
            Where-Object { $_.LastWriteTime -ge $testStart.AddSeconds(-2) } |
            Sort-Object LastWriteTime -Descending
        foreach ($candidate in $candidateFiles) {
            $probeText = & $ffprobe -v error -select_streams v:0 -count_frames `
                -show_entries stream=nb_read_frames -of default=nokey=1:noprint_wrappers=1 $candidate.FullName 2>$null
            $frames = 0
            [void][int]::TryParse(($probeText | Select-Object -First 1), [ref]$frames)
            if ($frames -gt $mp4Frames) { $mp4Frames = $frames; $mp4Path = $candidate.FullName }
			if ($mp4Frames -ge 60) { break }
        }
        $mp4Pass = $mp4Frames -ge 60
    }

    if (-not $SkipFallback) {
        $env:H264Encoder = "mediafoundation"
        $fallbackVideo = Start-LoggedProcess "video-fallback" $videoExe @(
            "--network-config=$videoConfig", "--channel=precise", "--plat-id=1001", "--sensor-id=2") $videoWork
        Start-Sleep -Seconds 2
        $fallbackHwa = Start-LoggedProcess "hwa-fallback" $hwaExe @("--network-config", $hwaConfig) $hwaWork
        Start-Sleep -Seconds 5
        $fallbackStim = Start-LoggedProcess "stim-fallback" $stimExe @(
            "--network-config=$stimConfig", "--channel=precise", "--plat-id=1001", "--sensor-id=2",
            "--sim-mode=2", "--video-fps=60", "--phase1d-h264=1", "--save-mp4=0",
            "--phase1b-auto-seconds=10") $stimWork
        if (-not $fallbackStim.Process.WaitForExit(30000)) { throw "Fallback stimulus timeout" }
        Start-Sleep -Seconds 3
        Stop-LoggedProcess $fallbackHwa
        $fallbackHwa = $null
        Stop-LoggedProcess $fallbackVideo
        $fallbackVideo = $null
        $fallbackText = Read-Log (Join-Path $runDir "hwa-fallback.out.log")
        $fallbackVideoText = Read-Log (Join-Path $runDir "video-fallback.err.log")
        $fallbackPass = $fallbackText -match 'requestedCodec=h264' -and
            $fallbackText -match 'activeCodec=jpeg' -and
            $fallbackText -match 'codecFallbackReason=requested_encoder_not_integrated:mediafoundation' -and
            $fallbackVideoText -match 'activeCodec=jpeg'
    }
}
finally {
    foreach ($entry in @($fallbackStim, $fallbackHwa, $fallbackVideo, $stim, $hwa, $videoB, $videoA)) {
        Stop-LoggedProcess $entry
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQtLog
    $env:H264Encoder = $oldH264Encoder
}

$overallPass = $recoveryPass -and $fallbackPass -and $mp4Pass
$summary = [pscustomobject]@{
    status = $(if ($overallPass) { "PASS" } else { "FAIL" })
    logDir = $runDir
    tcpReconnectAndIdr = $recoveryPass
    resetInitAndIdr = $recoveryPass
    unavailableBackendJpegFallback = $fallbackPass
    decodedMp4Playable = $mp4Pass
    mp4Frames = $mp4Frames
    mp4Path = $mp4Path
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runDir "summary.json") -Encoding UTF8
$summary | Format-List
if (-not $overallPass) { throw "V3 H.264 recovery smoke failed; see $runDir\summary.json" }
Write-Output "[V3H264Recovery] status=PASS logDir=$runDir"
