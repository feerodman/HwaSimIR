param(
    [int]$DurationSec = 30,
    [ValidateRange(1, 4)]
    [int]$MaxVisibleVolumes = 2,
    [ValidateSet(1, 2, 3)]
    [int]$SensorBand = 2,
    [double]$CloudTemperatureK = [double]::NaN
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\w15-streamed-cloud-windows-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWork = Join-Path $root "HwaSim_IR\Bin"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$stimWork = Join-Path $root "DataDrivenTestQT"
$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWork = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release"
$inputFile = Join-Path $root "DataDrivenTestQT\1.txt"
foreach ($path in @($hwaExe, $stimExe, $videoExe, $inputFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing prerequisite: $path" }
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$weatherProfilePath = Join-Path $root "HwaSim_IR\Bin\Config\Weather\weather_profiles.json"
if (-not [double]::IsNaN($CloudTemperatureK)) {
    if ($CloudTemperatureK -lt 180.0 -or $CloudTemperatureK -gt 330.0) { throw "CloudTemperatureK is outside the diagnostic range" }
    $weatherProfile = Get-Content -LiteralPath $weatherProfilePath -Raw -Encoding UTF8 | ConvertFrom-Json
    $weatherProfile.profiles.Overcast.cloudTemperatureK = $CloudTemperatureK
    $weatherProfilePath = Join-Path $logDir ("weather_profiles_{0}K.json" -f ([int]$CloudTemperatureK))
    [IO.File]::WriteAllText($weatherProfilePath, ($weatherProfile | ConvertTo-Json -Depth 12), $utf8)
}
$streamingInput = Join-Path $logDir "input_streamed_cloud_route.csv"
$sourceLines = [IO.File]::ReadAllLines($inputFile)
$routeLines = New-Object System.Collections.Generic.List[string]
$routeLines.Add($sourceLines[0])
$validRows = @($sourceLines | Select-Object -Skip 1 | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
for ($rowIndex = 0; $rowIndex -lt $validRows.Count; ++$rowIndex) {
    $fields = $validRows[$rowIndex].Split(',')
    if ($fields.Length -ne 63) { continue }
    $routePhase = if ($validRows.Count -gt 1) { $rowIndex / [double]($validRows.Count - 1) } else { 0.0 }
    # Out-and-back route: returning to unloaded cells must recreate the same
    # deterministic cloud descriptors, rather than following the target.
    # Four complete out-and-back traversals fit into the source file.  The
    # first unload/reload cycle therefore completes even in a 45 s smoke run.
    $trianglePhase = ($routePhase * 8.0) % 2.0
    $fraction = 1.0 - [math]::Abs($trianglePhase - 1.0)
    $platformLat = [double]::Parse($fields[2], [Globalization.CultureInfo]::InvariantCulture)
    $platformLon = [double]::Parse($fields[3], [Globalization.CultureInfo]::InvariantCulture)
    $platformAlt = [double]::Parse($fields[4], [Globalization.CultureInfo]::InvariantCulture)
    # Traverse roughly 10 km across multiple cells at cloud altitude.
    $fields[10] = ($platformLat + 0.02 + 0.09 * $fraction).ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
    $fields[11] = $platformLon.ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
    $fields[12] = ($platformAlt - 8000.0).ToString("F5", [Globalization.CultureInfo]::InvariantCulture)
    $fields[59] = $fields[10]
    $fields[60] = $fields[11]
    $fields[61] = $fields[12]
    $routeLines.Add([string]::Join(',', $fields))
}
[IO.File]::WriteAllLines($streamingInput, $routeLines, $utf8)
$hwaNetwork = Join-Path $logDir "NetworkConfig_hwa.ini"
$stimNetwork = Join-Path $logDir "NetworkConfig_stim.ini"
$videoNetwork = Join-Path $logDir "NetworkConfig_video.ini"
[IO.File]::WriteAllText($hwaNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`nacceptSensorBroadcast=true`r`nallowDynamicRemote=false`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=28888`r`nremoteIp=127.0.0.1`r`nremotePort=29998`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=25555`r`n", $utf8)
[IO.File]::WriteAllText($stimNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[UDP]`r`nlocalIp=127.0.0.1`r`nlocalPort=29998`r`nremoteIp=127.0.0.1`r`nremotePort=28888`r`n", $utf8)
[IO.File]::WriteAllText($videoNetwork, "[Network]`r`nip=127.0.0.1`r`nport=25555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n", $utf8)

$oldValues = @{}
$overrides = [ordered]@{
    QT_FORCE_STDERR_LOGGING = "1"
    RenderPresentationMode = "HeadlessOffscreen"
    Stage7CloudRenderMode = "StreamedWorld3D"
    Stage7VolumetricCloudEnable = "true"
    Stage7VolumetricCloudMaxVisibleVolumes = [string]$MaxVisibleVolumes
    Stage7WeatherProfilePath = $weatherProfilePath
    H264Encoder = "ffmpeg"
    H264FallbackToJpeg = "false"
}
foreach ($name in $overrides.Keys) {
    $oldValues[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    [Environment]::SetEnvironmentVariable($name, $overrides[$name], "Process")
}

$video = $null; $hwa = $null; $stim = $null
try {
    $video = Start-Process -FilePath $videoExe -ArgumentList @("--network-config", $videoNetwork) -WorkingDirectory $videoWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir "video.out.log") -RedirectStandardError (Join-Path $logDir "video.err.log")
    Start-Sleep -Seconds 2
    $hwa = Start-Process -FilePath $hwaExe -ArgumentList @("--channel", "precise", "--network-config", $hwaNetwork) -WorkingDirectory $hwaWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir "hwa.out.log") -RedirectStandardError (Join-Path $logDir "hwa.err.log")
    Start-Sleep -Seconds 4
    $stimArgs = @(
        "--duration-sec=$DurationSec", "--phase1d-h264=1", "--save-mp4=1",
        "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
        "--env-sky=1", "--sensor-band=$SensorBand", "--sensor-pixel-angle-urad=100",
        "--network-config=$stimNetwork", "--channel=precise", "--input-file=$streamingInput"
    )
    $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory $stimWork -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $logDir "stim.out.log") -RedirectStandardError (Join-Path $logDir "stim.err.log")
    if (-not $stim.WaitForExit(($DurationSec + 25) * 1000)) { throw "Stimulus timed out" }
    Start-Sleep -Seconds 4
}
finally {
    foreach ($process in @($stim, $hwa, $video)) {
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) { Stop-Process -Id $process.Id -Force; $process.WaitForExit() }
        }
    }
    foreach ($name in $oldValues.Keys) {
        [Environment]::SetEnvironmentVariable($name, $oldValues[$name], "Process")
    }
}

$hwaText = (Get-Content -LiteralPath (Join-Path $logDir "hwa.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" + (Get-Content -LiteralPath (Join-Path $logDir "hwa.err.log") -Raw -ErrorAction SilentlyContinue)
$videoText = (Get-Content -LiteralPath (Join-Path $logDir "video.out.log") -Raw -ErrorAction SilentlyContinue) + "`n" + (Get-Content -LiteralPath (Join-Path $logDir "video.err.log") -Raw -ErrorAction SilentlyContinue)
$activationCount = [regex]::Matches($hwaText, "\[World3DCloudCell\][^\r\n]*action=activate").Count
$activationIds = @([regex]::Matches($hwaText, "\[World3DCloudCell\][^\r\n]*action=activate[^\r\n]*cloudId=([0-9A-F]+)") | ForEach-Object { $_.Groups[1].Value })
$deactivationIds = @([regex]::Matches($hwaText, "\[World3DCloudCell\][^\r\n]*action=deactivate[^\r\n]*cloudId=([0-9A-F]+)") | ForEach-Object { $_.Groups[1].Value })
$reactivatedIds = @($activationIds | Group-Object | Where-Object { $_.Count -gt 1 -and $deactivationIds -contains $_.Name } | ForEach-Object Name)
$checks = [ordered]@{
    StreamedRenderer = $hwaText -match "\[World3DCloudRenderer\][^\r\n]*placement=StreamedWorld[^\r\n]*poolSize=8"
    SharedDensity = ([regex]::Matches($hwaText, "\[World3DCloudDensityLoaded\]").Count -ge 4)
    GpuDepthComposite = $hwaText -match "\[World3DCloudDepthComposite\][^\r\n]*gpuDepthRead=1[^\r\n]*cpuDepthReadback=0"
    TargetStreamingCenter = $hwaText -match "\[World3DCloudStreaming\][^\r\n]*centerReady=1[^\r\n]*targetKey="
    CloudActivation = $activationCount -gt 0
    RuntimeUnloadReloadIdentity = $reactivatedIds.Count -gt 0
    VisibleVolume = $hwaText -match "visibleCloudVolumes=[1-4]"
    ShaderRuntime = $hwaText -notmatch "shader.*(error|failed)|compile.*failed|GLSL.*error"
    H264Encode = $hwaText -match "\[TcpPerf\][^\r\n]*activeCodec=h264_annexb[^\r\n]*activeBackend=ffmpeg/libx264"
    H264Decode = $videoText -match "\[H264DecodeSuccess\][^\r\n]*backend=ffmpeg"
    PacketV3 = $videoText -match "packetVersion=3"
    NoCodecFallback = $hwaText -notmatch "\[CodecFallback\]"
}
$summary = @("# W1.5 Streamed World-Space 3D Cloud Windows Smoke", "", "- logDir: $logDir", "- durationSec: $DurationSec", "- maxVisibleVolumes: $MaxVisibleVolumes", "- sensorBand: $SensorBand", "- cloudTemperatureK: $(if ([double]::IsNaN($CloudTemperatureK)) { 'profile-default' } else { $CloudTemperatureK })", "- activationCount: $activationCount", "- reactivatedCloudIds: $($reactivatedIds -join ',')", "")
foreach ($entry in $checks.GetEnumerator()) {
    $summary += "- $(if ($entry.Value) { 'PASS' } else { 'FAIL' }): $($entry.Key)"
    Write-Output ("{0}={1}" -f $entry.Key, $(if ($entry.Value) { "PASS" } else { "FAIL" }))
}
[IO.File]::WriteAllLines((Join-Path $logDir "summary.md"), $summary, $utf8)
Write-Output "LOGDIR=$logDir"
if ($checks.Values -contains $false) { exit 1 }
