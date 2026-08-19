param(
    [int]$DurationSec = 12,
    [ValidateSet("World2D", "Layered2_5D")]
    [string]$CloudRenderMode = "Layered2_5D",
    [double]$SensorPixelAngleUrad = 100.0,
    [Alias("Scenario")]
    [string]$ScenarioName = "",
    [switch]$Quick
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\w1-world-cloud-imaging-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWork = Join-Path $root "HwaSim_IR\Bin"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWork = Join-Path $root "DataDrivenTestQT"
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWork = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release"
$inputSource = Join-Path $root "DataDrivenTestQT\1.txt"
$profileSource = Join-Path $root "HwaSim_IR\Bin\Config\Weather\weather_profiles.json"
$ffmpeg = Get-ChildItem -LiteralPath (Join-Path $root ".deps") -Filter ffmpeg.exe -File -Recurse |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
foreach ($path in @($hwaExe, $stimExe, $videoExe, $inputSource, $profileSource, $ffmpeg)) {
    if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing prerequisite: $path" }
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
function New-AltitudeInput([string]$Destination, [double]$RelativeTargetAltitudeM) {
    $lines = [IO.File]::ReadAllLines($inputSource)
    $output = New-Object System.Collections.Generic.List[string]
    $output.Add($lines[0])
    for ($i = 1; $i -lt $lines.Length; ++$i) {
        if ([string]::IsNullOrWhiteSpace($lines[$i])) { continue }
        $fields = $lines[$i].Split(',')
        if ($fields.Length -ne 63) { continue }
        $platformAltitudeM = [double]::Parse($fields[4], [Globalization.CultureInfo]::InvariantCulture)
        $targetAltitudeM = $platformAltitudeM + $RelativeTargetAltitudeM
        $platformLatitudeDeg = [double]::Parse($fields[2], [Globalization.CultureInfo]::InvariantCulture)
        $platformLongitudeDeg = [double]::Parse($fields[3], [Globalization.CultureInfo]::InvariantCulture)
        # Keep every weather case on the same approximately 8.9 km horizontal route.
        $fields[10] = ($platformLatitudeDeg + 0.08).ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
        $fields[11] = $platformLongitudeDeg.ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
        $fields[12] = $targetAltitudeM.ToString("F5", [Globalization.CultureInfo]::InvariantCulture)
        $fields[59] = $fields[10]
        $fields[60] = $fields[11]
        $fields[61] = $fields[12]
        $output.Add([string]::Join(',', $fields))
    }
    [IO.File]::WriteAllLines($Destination, $output, $utf8)
}

function New-TemperatureProfile([string]$Destination, [double]$TemperatureK) {
    $json = Get-Content -LiteralPath $profileSource -Raw | ConvertFrom-Json
    $json.profiles.Cloudy.cloudTemperatureK = $TemperatureK
    [IO.File]::WriteAllText($Destination, ($json | ConvertTo-Json -Depth 16), $utf8)
}

$inputCloudFront = Join-Path $logDir "input_target_before_cloud.csv"
$inputCloudBehind = Join-Path $logDir "input_target_behind_cloud.csv"
# Ground reference is 0 m and production cloud base is 2500 m.  The camera is
# near 11000 m, so 7000 m is in front of the cloud and 1000 m is behind it.
New-AltitudeInput $inputCloudFront -4000.0
New-AltitudeInput $inputCloudBehind -10000.0
$profile240 = Join-Path $logDir "weather_profiles_cloudy_240K.json"
$profile260 = Join-Path $logDir "weather_profiles_cloudy_260K.json"
New-TemperatureProfile $profile240 240.0
New-TemperatureProfile $profile260 260.0

$scenarios = @(
    [pscustomobject]@{ Name="clear_mwir_behind"; EnvSky=0; Band=2; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=2500 },
    [pscustomobject]@{ Name="cloudy_mwir_behind"; EnvSky=1; Band=2; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=2500 },
    [pscustomobject]@{ Name="overcast_mwir_behind"; EnvSky=5; Band=2; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=2500 },
    [pscustomobject]@{ Name="cloudy_mwir_front"; EnvSky=1; Band=2; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=500 },
    [pscustomobject]@{ Name="overcast_mwir_front"; EnvSky=5; Band=2; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=500 },
    [pscustomobject]@{ Name="cloudy_mwir_240K"; EnvSky=1; Band=2; Input=$inputCloudBehind; Profile=$profile240; CloudBaseM=2500 },
    [pscustomobject]@{ Name="cloudy_mwir_260K"; EnvSky=1; Band=2; Input=$inputCloudBehind; Profile=$profile260; CloudBaseM=2500 },
    [pscustomobject]@{ Name="cloudy_nir"; EnvSky=1; Band=1; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=2500 },
    [pscustomobject]@{ Name="cloudy_lwir"; EnvSky=1; Band=3; Input=$inputCloudBehind; Profile=$profileSource; CloudBaseM=2500 }
)
if ($Quick) { $scenarios = @($scenarios | Where-Object { $_.Name -in @("clear_mwir_behind", "cloudy_mwir_behind", "overcast_mwir_behind") }) }
if ($ScenarioName) { $scenarios = @($scenarios | Where-Object { $_.Name -eq $ScenarioName }) }

$oldValues = @{
    QT_FORCE_STDERR_LOGGING=$env:QT_FORCE_STDERR_LOGGING
    RenderPresentationMode=$env:RenderPresentationMode
    H264Encoder=$env:H264Encoder
    H264FallbackToJpeg=$env:H264FallbackToJpeg
    Stage7CloudRenderMode=$env:Stage7CloudRenderMode
    Stage7CloudBaseAltitudeM=$env:Stage7CloudBaseAltitudeM
    Stage7WeatherProfilePath=$env:Stage7WeatherProfilePath
}
$results = New-Object System.Collections.Generic.List[object]
try {
    $env:QT_FORCE_STDERR_LOGGING = "1"
    $env:RenderPresentationMode = "HeadlessOffscreen"
    $env:H264Encoder = "ffmpeg"
    $env:H264FallbackToJpeg = "false"
    $env:Stage7CloudRenderMode = $CloudRenderMode
    $index = 0
    foreach ($scenario in $scenarios) {
        ++$index
        $scenarioDir = Join-Path $logDir $scenario.Name
        New-Item -ItemType Directory -Force -Path $scenarioDir | Out-Null
        $basePort = 18000 + $index * 10
        $hwaNetwork = Join-Path $scenarioDir "NetworkConfig_hwa.ini"
        $stimNetwork = Join-Path $scenarioDir "NetworkConfig_stim.ini"
        $videoNetwork = Join-Path $scenarioDir "NetworkConfig_video.ini"
        [IO.File]::WriteAllText($hwaNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`nacceptSensorBroadcast=true`r`nallowDynamicRemote=false`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=$basePort`r`nremoteIp=127.0.0.1`r`nremotePort=$($basePort + 1)`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=$($basePort + 2)`r`n", $utf8)
        [IO.File]::WriteAllText($stimNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=$($basePort + 1)`r`nremoteIp=127.0.0.1`r`nremotePort=$basePort`r`n", $utf8)
        [IO.File]::WriteAllText($videoNetwork, "[Network]`r`nip=127.0.0.1`r`nport=$($basePort + 2)`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n", $utf8)

        $env:Stage7WeatherProfilePath = $scenario.Profile
        $env:Stage7CloudBaseAltitudeM = [string]$scenario.CloudBaseM
        $beforeRounds = @(Get-ChildItem -LiteralPath (Join-Path $videoWork "MP4") -Directory -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName)
        $video = $null; $hwa = $null; $stim = $null
        try {
            $video = Start-Process -FilePath $videoExe -ArgumentList @("--network-config", $videoNetwork) -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scenarioDir "video.out.log") -RedirectStandardError (Join-Path $scenarioDir "video.err.log")
            Start-Sleep -Seconds 2
            $hwa = Start-Process -FilePath $hwaExe -ArgumentList @("--channel", "precise", "--network-config", $hwaNetwork) -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scenarioDir "hwa.out.log") -RedirectStandardError (Join-Path $scenarioDir "hwa.err.log")
            Start-Sleep -Seconds 4
            $stimArgs = @(
                "--duration-sec=$DurationSec", "--phase1d-h264=1", "--save-mp4=1",
                "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
                "--env-sky=$($scenario.EnvSky)", "--sensor-band=$($scenario.Band)",
                "--sensor-pixel-angle-urad=$SensorPixelAngleUrad",
                "--network-config=$stimNetwork", "--channel=precise", "--input-file=$($scenario.Input)"
            )
            $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scenarioDir "stim.out.log") -RedirectStandardError (Join-Path $scenarioDir "stim.err.log")
            if (-not $stim.WaitForExit(($DurationSec + 20) * 1000)) { throw "Stimulus timeout: $($scenario.Name)" }
            Start-Sleep -Seconds 3
        }
        finally {
            foreach ($p in @($stim, $hwa, $video)) {
                if ($p) { $p.Refresh(); if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() } }
            }
        }
        $newRound = Get-ChildItem -LiteralPath (Join-Path $videoWork "MP4") -Directory -ErrorAction SilentlyContinue |
            Where-Object FullName -notin $beforeRounds | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        $mp4 = if ($newRound) { Join-Path $newRound.FullName "output.mp4" } else { $null }
        $png = Join-Path $scenarioDir "$($scenario.Name).png"
        if ($mp4 -and (Test-Path -LiteralPath $mp4)) {
            & $ffmpeg -hide_banner -loglevel error -y -sseof -1 -i $mp4 -frames:v 1 $png
            Copy-Item -LiteralPath $mp4 -Destination (Join-Path $scenarioDir "$($scenario.Name).mp4")
        }
        $hwaText = (Get-Content -LiteralPath (Join-Path $scenarioDir "hwa.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" + (Get-Content -LiteralPath (Join-Path $scenarioDir "hwa.err.log") -Raw -ErrorAction SilentlyContinue)
        $videoText = (Get-Content -LiteralPath (Join-Path $scenarioDir "video.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" + (Get-Content -LiteralPath (Join-Path $scenarioDir "video.err.log") -Raw -ErrorAction SilentlyContinue)
        $pass = (Test-Path -LiteralPath $png) -and ($hwaText -match "\[WeatherCloud\][^\r\n]*envSky=$($scenario.EnvSky)") -and ($hwaText -match "\[TcpPerf\][^\r\n]*activeCodec=h264_annexb") -and ($videoText -match "\[H264DecodeSuccess\]") -and ($videoText -match "packetVersion=3") -and ($hwaText -notmatch "\[CodecFallback\]")
        $results.Add([pscustomobject]@{ Name=$scenario.Name; Pass=$pass; Png=$png; Mp4=$mp4 })
        Write-Output ("{0}={1} PNG={2}" -f $scenario.Name, $(if ($pass) { "PASS" } else { "FAIL" }), $png)
    }
}
finally {
    foreach ($key in $oldValues.Keys) { Set-Item -Path "Env:$key" -Value $oldValues[$key] }
}

$summary = @("# W1 World-space Cloud Imaging Test", "", "- mode: $CloudRenderMode", "- durationSec: $DurationSec", "- sensorPixelAngleUrad: $SensorPixelAngleUrad", "")
foreach ($result in $results) { $summary += "- $(if ($result.Pass) { 'PASS' } else { 'FAIL' }): $($result.Name) - $($result.Png)" }
$summaryPath = Join-Path $logDir "summary.md"
[IO.File]::WriteAllLines($summaryPath, $summary, $utf8)
Write-Output "LOGDIR=$logDir"
if ($results.Count -eq 0 -or ($results | Where-Object { -not $_.Pass })) { exit 1 }
