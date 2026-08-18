param(
    [int]$WeatherSeconds = 10,
    [ValidateSet(0, 1, 2, 3, 4)]
    [int]$SensorBand = 2
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\w1-cloud-windows-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWork = Join-Path $root "HwaSim_IR\Bin"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWork = Join-Path $root "DataDrivenTestQT"
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWork = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release"
foreach ($path in @($hwaExe, $stimExe, $videoExe)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Executable missing: $path" }
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$hwaNetwork = Join-Path $logDir "NetworkConfig_hwa.ini"
$stimNetwork = Join-Path $logDir "NetworkConfig_stim.ini"
$weatherStimNetwork = Join-Path $logDir "NetworkConfig_weather_stim.ini"
$videoNetwork = Join-Path $logDir "NetworkConfig_video.ini"
[IO.File]::WriteAllText($hwaNetwork, @"
[Identity]
channel=precise
platID=1001
sensorID=2
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=127.0.0.1
localPort=18888
remoteIp=127.0.0.1
remotePort=19998

[TCP]
serverIp=127.0.0.1
serverPort=15555
"@, $utf8)
[IO.File]::WriteAllText($stimNetwork, @"
[Identity]
channel=precise
platID=1001
sensorID=2

[UDP]
localIp=127.0.0.1
localPort=19998
remoteIp=127.0.0.1
remotePort=18888
"@, $utf8)
[IO.File]::WriteAllText($weatherStimNetwork, @"
[Identity]
channel=precise
platID=1001
sensorID=2

[UDP]
localIp=127.0.0.1
localPort=19997
remoteIp=127.0.0.1
remotePort=18888
"@, $utf8)
[IO.File]::WriteAllText($videoNetwork, @"
[Network]
ip=127.0.0.1
port=15555

[Recorder]
MaxRecordingQueueFrames=180
FlushTimeoutMs=10000
"@, $utf8)

$oldQtLog = $env:QT_FORCE_STDERR_LOGGING
$oldMode = $env:RenderPresentationMode
$oldH264Encoder = $env:H264Encoder
$oldH264Fallback = $env:H264FallbackToJpeg
$video = $null
$hwa = $null
$stim = $null
$switchProcesses = @()
try {
    $env:QT_FORCE_STDERR_LOGGING = "1"
    $env:RenderPresentationMode = "HeadlessOffscreen"
    $env:H264Encoder = "ffmpeg"
    $env:H264FallbackToJpeg = "false"
    $video = Start-Process -FilePath $videoExe -ArgumentList @("--network-config", $videoNetwork) `
        -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logDir "video.out.log") `
        -RedirectStandardError (Join-Path $logDir "video.err.log")
    Start-Sleep -Seconds 2
    $hwa = Start-Process -FilePath $hwaExe -ArgumentList @("--channel", "precise", "--network-config", $hwaNetwork) `
        -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $logDir "hwa.out.log") `
        -RedirectStandardError (Join-Path $logDir "hwa.err.log")
    Start-Sleep -Seconds 4

    $totalSeconds = 10 + 4 * $WeatherSeconds
    $stimArgs = @(
        "--duration-sec=$totalSeconds", "--phase1d-h264=1", "--save-mp4=0",
        "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
        "--env-sky=0", "--sensor-band=$SensorBand", "--network-config=$stimNetwork", "--channel=precise"
    )
    $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir "stim-main.out.log") `
        -RedirectStandardError (Join-Path $logDir "stim-main.err.log")
    Start-Sleep -Seconds 8

    foreach ($weather in @(
        @{ Name = "Cloudy"; EnvSky = 1 },
        @{ Name = "Overcast"; EnvSky = 5 },
        @{ Name = "ClearReturn"; EnvSky = 0 }
    )) {
        $switchOut = Join-Path $logDir ("stim-switch-{0}.out.log" -f $weather.Name)
        $switchErr = Join-Path $logDir ("stim-switch-{0}.err.log" -f $weather.Name)
        $args = @(
            "--init-only", "--phase1d-h264=1", "--save-mp4=0",
            "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
            "--env-sky=$($weather.EnvSky)", "--sensor-band=$SensorBand",
            "--network-config=$weatherStimNetwork", "--channel=precise"
        )
        $p = Start-Process -FilePath $stimExe -ArgumentList $args -WorkingDirectory $stimWork `
            -WindowStyle Hidden -PassThru -RedirectStandardOutput $switchOut -RedirectStandardError $switchErr
        $switchProcesses += $p
        if (-not $p.WaitForExit(8000)) { throw "Weather switch process timed out: $($weather.Name)" }
        Start-Sleep -Seconds $WeatherSeconds
    }
    if ($stim -and -not $stim.WaitForExit(($totalSeconds + 15) * 1000)) {
        throw "DataDrivenTestQT main stimulus timed out"
    }
    Start-Sleep -Seconds 3
}
finally {
    foreach ($p in @($stim, $hwa, $video) + $switchProcesses) {
        if ($p) {
            $p.Refresh()
            if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
        }
    }
    $env:QT_FORCE_STDERR_LOGGING = $oldQtLog
    $env:RenderPresentationMode = $oldMode
    $env:H264Encoder = $oldH264Encoder
    $env:H264FallbackToJpeg = $oldH264Fallback
}

$hwaText = ((Get-Content (Join-Path $logDir "hwa.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" +
    (Get-Content (Join-Path $logDir "hwa.err.log") -Raw -ErrorAction SilentlyContinue))
$videoText = ((Get-Content (Join-Path $logDir "video.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" +
    (Get-Content (Join-Path $logDir "video.err.log") -Raw -ErrorAction SilentlyContinue))
$checks = [ordered]@{
    ShaderCompiled = $hwaText -notmatch "shader.*(error|failed)|compile.*failed"
    Clear = $hwaText -match "\[WeatherCloud\][^\r\n]*envSky=0[^\r\n]*enabled=0"
    Cloudy = $hwaText -match "\[WeatherCloud\][^\r\n]*envSky=1[^\r\n]*enabled=1[^\r\n]*cloud_scattered"
    Overcast = $hwaText -match "\[WeatherCloud\][^\r\n]*envSky=5[^\r\n]*enabled=1[^\r\n]*cloud_overcast"
    TwoLayers = $hwaText -match "\[WeatherCloud\][^\r\n]*layers=2"
    TextureLoaded = $hwaText -match "\[WeatherTextureLoaded\][^\r\n]*success=1"
    TextureCached = ([regex]::Matches($hwaText, "\[WeatherTextureLoaded\][^\r\n]*cloud_scattered").Count -eq 1) -and
        ([regex]::Matches($hwaText, "\[WeatherTextureLoaded\][^\r\n]*cloud_overcast").Count -eq 1)
    H264Encode = $hwaText -match "\[TcpPerf\][^\r\n]*activeCodec=h264_annexb[^\r\n]*activeBackend=ffmpeg/libx264"
    H264Decode = $videoText -match "\[H264DecodeSuccess\][^\r\n]*backend=ffmpeg"
    PacketV3 = $videoText -match "packetVersion=3"
    NoCodecFallback = $hwaText -notmatch "\[CodecFallback\]"
}
$summary = @("# W1 Windows Cloud Smoke", "", "- logDir: $logDir", "- sensorBand: $SensorBand", "")
foreach ($entry in $checks.GetEnumerator()) {
    $summary += "- $(if ($entry.Value) { 'PASS' } else { 'FAIL' }): $($entry.Key)"
}
$summaryPath = Join-Path $logDir "summary.md"
$summary | Set-Content -LiteralPath $summaryPath -Encoding UTF8
$checks.GetEnumerator() | ForEach-Object { "{0}={1}" -f $_.Key, $(if ($_.Value) { "PASS" } else { "FAIL" }) }
Write-Output "LOGDIR=$logDir"
if ($checks.Values -contains $false) { exit 1 }
