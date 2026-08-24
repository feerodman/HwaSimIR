param(
    [Parameter(Mandatory=$true)][string]$SshKey,
    [string]$BoardHost = "192.168.1.116",
    [string]$WindowsHost = "192.168.1.188",
    [int]$WarmupSeconds = 5,
    [int]$MeasureSeconds = 30,
    [double]$SensorPixelAngleUrad = 100.0,
    [ValidateSet("", "layered", "visible1", "visible2", "visible4")][string]$CaseName = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\w15-streamed-cloud-rk3588-$stamp"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$inputSource = Join-Path $root "DataDrivenTestQT\1.txt"
foreach ($path in @($SshKey, $videoExe, $stimExe, $inputSource)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing test prerequisite: $path" }
}

function Invoke-Board([string]$Command) {
    & ssh.exe -i $SshKey -o BatchMode=yes "root@$BoardHost" $Command
    if ($LASTEXITCODE -ne 0) { throw "Board command failed: $Command" }
}

function Copy-FromBoard([string]$Remote, [string]$Local) {
    & scp.exe -i $SshKey -o BatchMode=yes "root@$BoardHost`:$Remote" $Local
    if ($LASTEXITCODE -ne 0) { throw "Board log copy failed: $Remote" }
}

function Metric([string]$Line, [string]$Name) {
    $match = [regex]::Match($Line, '(?:^| )' + [regex]::Escape($Name) + '=([-+0-9.]+)')
    if ($match.Success) { return [double]$match.Groups[1].Value }
    return $null
}

function Stats($Lines, [string]$Name) {
    $values = @($Lines | ForEach-Object { Metric $_ $Name } | Where-Object { $null -ne $_ })
    if (-not $values.Count) { return $null }
    $result = $values | Measure-Object -Average -Minimum -Maximum
    return [pscustomobject]@{ count=$result.Count; avg=[math]::Round($result.Average,3); min=[math]::Round($result.Minimum,3); max=[math]::Round($result.Maximum,3) }
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$streamingInput = Join-Path $logDir "input_streamed_cloud_route.csv"
$sourceLines = [IO.File]::ReadAllLines($inputSource)
$dataRows = @($sourceLines | Select-Object -Skip 1 | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$routeLines = New-Object System.Collections.Generic.List[string]
$routeLines.Add($sourceLines[0])
for ($rowIndex = 0; $rowIndex -lt $dataRows.Count; ++$rowIndex) {
    $fields = $dataRows[$rowIndex].Split(',')
    if ($fields.Length -ne 63) { continue }
    $routePhase = if ($dataRows.Count -gt 1) { $rowIndex / [double]($dataRows.Count - 1) } else { 0.0 }
    # Cross several cells and return so runtime unload/reload identity is
    # exercised in addition to the deterministic model unit check.
    $trianglePhase = ($routePhase * 8.0) % 2.0
    $fraction = 1.0 - [math]::Abs($trianglePhase - 1.0)
    $platformLat = [double]::Parse($fields[2], [Globalization.CultureInfo]::InvariantCulture)
    $platformLon = [double]::Parse($fields[3], [Globalization.CultureInfo]::InvariantCulture)
    $platformAlt = [double]::Parse($fields[4], [Globalization.CultureInfo]::InvariantCulture)
    $fields[10] = ($platformLat + 0.02 + 0.09 * $fraction).ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
    $fields[11] = $platformLon.ToString("F8", [Globalization.CultureInfo]::InvariantCulture)
    $fields[12] = ($platformAlt - 8000.0).ToString("F5", [Globalization.CultureInfo]::InvariantCulture)
    $fields[59] = $fields[10]; $fields[60] = $fields[11]; $fields[61] = $fields[12]
    $routeLines.Add([string]::Join(',', $fields))
}
[IO.File]::WriteAllLines($streamingInput, $routeLines, $utf8)

$videoNetwork = Join-Path $logDir "NetworkConfig_video.ini"
$stimNetwork = Join-Path $logDir "NetworkConfig_stim.ini"
[IO.File]::WriteAllText($videoNetwork, "[Network]`r`nip=$WindowsHost`r`nport=5555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n", $utf8)
[IO.File]::WriteAllText($stimNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[UDP]`r`nlocalIp=$WindowsHost`r`nlocalPort=9998`r`nremoteIp=$BoardHost`r`nremotePort=8888`r`n", $utf8)

$route = & ssh.exe -i $SshKey -o BatchMode=yes "root@$BoardHost" "ip route"
if ($LASTEXITCODE -ne 0) { throw "Cannot connect to RK3588" }
$route | Set-Content -LiteralPath (Join-Path $logDir "ip_route.txt") -Encoding UTF8
if ($route -match '(?m)^default\s') { throw "RK3588 has a default route; test stopped" }
Invoke-Board "test -e /dev/mpp_service && pgrep -f 'Xorg :0' >/dev/null"

$cases = @(
    [pscustomobject]@{ Name="layered"; Mode="Layered2_5D"; Enable="false"; Visible=0 },
    [pscustomobject]@{ Name="visible1"; Mode="StreamedWorld3D"; Enable="true"; Visible=1 },
    [pscustomobject]@{ Name="visible2"; Mode="StreamedWorld3D"; Enable="true"; Visible=2 },
    [pscustomobject]@{ Name="visible4"; Mode="StreamedWorld3D"; Enable="true"; Visible=4 }
)
if ($CaseName) { $cases = @($cases | Where-Object Name -eq $CaseName) }
$runSeconds = $WarmupSeconds + $MeasureSeconds
$rows = New-Object System.Collections.Generic.List[object]
$env:QT_FORCE_STDERR_LOGGING = "1"

foreach ($case in $cases) {
    $caseDir = Join-Path $logDir $case.Name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    Get-Process HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue | Stop-Process -Force
    Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true"
    Start-Sleep -Seconds 2
    $video = Start-Process -FilePath $videoExe -ArgumentList @("--network-config", $videoNetwork) -WorkingDirectory (Split-Path $videoExe) -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $caseDir "VideoDisplay.out.log") -RedirectStandardError (Join-Path $caseDir "VideoDisplay.err.log")
    try {
        Start-Sleep -Seconds 3
        if (-not (netstat.exe -ano -p tcp | Select-String "$WindowsHost`:5555.*LISTENING")) { throw "VideoDisplay is not listening" }
        $remoteDir = "/userdata/HwaSimIR/logs/w15_streamed/$stamp/$($case.Name)"
        $start = "mkdir -p $remoteDir; cd /userdata/HwaSimIR; " +
            "export PANDA3D_ROOT=/opt/panda3d-aarch64 PRC_DIR=/opt/panda3d-aarch64/etc PANDA_PRC_DIR=/opt/panda3d-aarch64/etc; " +
            "export LD_LIBRARY_PATH=/opt/panda3d-aarch64/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu; " +
            "export DISPLAY=:0 XAUTHORITY=/root/.Xauthority RenderPresentationMode=HeadlessOffscreen; " +
            "export EnablePerfLog=1 QuietPerfMode=false H264Encoder=mpp H264FallbackToJpeg=false TcpPacketVersion=3 TcpSendVideo=true TcpSendAnnotation=true TcpSendRealtimeData=true; " +
            "export Stage7CloudRenderMode=$($case.Mode) Stage7VolumetricCloudEnable=$($case.Enable) Stage7VolumetricCloudMaxVisibleVolumes=$($case.Visible); " +
            "unset EGL_PLATFORM LIBGL_ALWAYS_SOFTWARE MESA_LOADER_DRIVER_OVERRIDE; " +
            "nohup ./HwaSim_IR --channel precise </dev/null >$remoteDir/HwaSim_IR.log 2>&1 & echo `$! >$remoteDir/HwaSim_IR.pid"
        Invoke-Board $start
        Start-Sleep -Seconds 6
        Invoke-Board "grep '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1' $remoteDir/HwaSim_IR.log"
        Invoke-Board "pid=`$(cat $remoteDir/HwaSim_IR.pid); nohup sh -c 'while kill -0 '`"`$pid`"' 2>/dev/null; do ps -p '`"`$pid`"' -o pid=,pcpu=,pmem=,rss=,nlwp=,etime=; sleep 2; done' >$remoteDir/cpu.log 2>&1 </dev/null &"
        $stimArgs = @(
            "--duration-sec=$runSeconds", "--phase1d-h264=1", "--save-mp4=0",
            "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
            "--env-sky=5", "--sensor-band=2", "--sensor-pixel-angle-urad=$SensorPixelAngleUrad",
            "--network-config=$stimNetwork", "--channel=precise", "--input-file=$streamingInput"
        )
        $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory (Join-Path $root "DataDrivenTestQT") -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $caseDir "DataDrivenTestQT.out.log") -RedirectStandardError (Join-Path $caseDir "DataDrivenTestQT.err.log")
        if (-not $stim.WaitForExit(($runSeconds + 25) * 1000)) { Stop-Process -Id $stim.Id -Force; throw "Stimulus timeout" }
        Start-Sleep -Seconds 3
        Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true"
        Start-Sleep -Seconds 2
        Copy-FromBoard "$remoteDir/HwaSim_IR.log" (Join-Path $caseDir "HwaSim_IR.log")
        Copy-FromBoard "$remoteDir/cpu.log" (Join-Path $caseDir "cpu.log")
    }
    finally {
        if ($video -and -not $video.HasExited) { Stop-Process -Id $video.Id -Force }
        try { Invoke-Board "pkill -TERM -x HwaSim_IR 2>/dev/null || true" } catch { Write-Warning $_ }
    }

    $board = @(Get-Content -LiteralPath (Join-Path $caseDir "HwaSim_IR.log"))
    $display = @((Get-Content -LiteralPath (Join-Path $caseDir "VideoDisplay.err.log")) + (Get-Content -LiteralPath (Join-Path $caseDir "VideoDisplay.out.log")))
    $perf = @($board | Where-Object { $_ -match '^\[Perf\].*mode=async' -and (Metric $_ 'udpFps') -ge 50 } | Select-Object -Skip $WarmupSeconds)
    $tcp = @($board | Where-Object { $_ -match '^\[TcpPerf\].*activeCodec=h264_annexb' } | Select-Object -Skip $WarmupSeconds)
    $videoPerf = @($display | Where-Object { $_ -match '^\[VideoPerf\].*activeCodec=h264_annexb' } | Select-Object -Skip $WarmupSeconds)
    $weather = @($board | Where-Object { $_ -match '^\[Stage7 Perf\].*textureLoadCountThisFrame=0' })
    $activationIds = @([regex]::Matches(($board -join "`n"), '\[World3DCloudCell\][^\r\n]*action=activate[^\r\n]*cloudId=([0-9A-F]+)') | ForEach-Object { $_.Groups[1].Value })
    $deactivationIds = @([regex]::Matches(($board -join "`n"), '\[World3DCloudCell\][^\r\n]*action=deactivate[^\r\n]*cloudId=([0-9A-F]+)') | ForEach-Object { $_.Groups[1].Value })
    $reactivatedIds = @($activationIds | Group-Object | Where-Object { $_.Count -gt 1 -and $deactivationIds -contains $_.Name } | ForEach-Object Name)
    $cpuValues = @(Get-Content (Join-Path $caseDir "cpu.log") | ForEach-Object { if ($_ -match '^\s*\d+\s+([0-9.]+)\s+') { [double]$Matches[1] } })
    $row = [pscustomobject]@{
        caseName=$case.Name
        hardwareGpu=[bool]($board -match '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1')
        mppEncode=[bool]($board -match '\[H264EncodeSuccess\].*backend=mpp')
        ffmpegDecode=[bool]($display -match '\[H264DecodeSuccess\].*backend=ffmpeg')
        packetV3=[bool]($display -match 'packetVersion=3.*flags=0x7.*codec=h264_annexb')
        codecFallback=[bool]($board -match '\[CodecFallback\]|activeBackend=(jpeg|ffmpeg)')
        renderFps=(Stats $perf 'renderFps').avg
        outputFps=(Stats $perf 'outputFps').avg
        displayFps=(Stats $videoPerf 'displayFps').avg
        renderMs=(Stats $perf 'renderMs').avg
        weatherMs=(Stats $weather 'weatherUpdateMs').avg
        readbackMs=(Stats $perf 'readbackMs').avg
        preprocessMs=(Stats $tcp 'flipMs').avg
        mppEncodeMs=(Stats $tcp 'h264EncodeMs').avg
        activeCloudVolumes=(Stats $weather 'activeCloudVolumes').avg
        activeCloudVolumesMax=(Stats $weather 'activeCloudVolumes').max
        visibleCloudVolumes=(Stats $weather 'visibleCloudVolumes').avg
        visibleCloudVolumesMax=(Stats $weather 'visibleCloudVolumes').max
        averageRaySteps=(Stats $weather 'averageRaySteps').avg
        averageRayStepsMax=(Stats $weather 'averageRaySteps').max
        reactivatedCloudCount=$reactivatedIds.Count
        inputQueueMax=(Stats $perf 'inputQueueDepth').max
        outputQueueMax=(Stats $perf 'outputQueueDepth').max
        sourceSeqLagMax=(Stats $perf 'sourceSeqLag').max
        droppedMax=(Stats $perf 'dropped').max
        cpuPercent=$(if ($cpuValues.Count) { [math]::Round(($cpuValues | Measure-Object -Average).Average,3) } else { $null })
    }
    $rows.Add($row)
    $row | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $caseDir "metrics.json") -Encoding UTF8
    $row | Format-List
}

$rows | Export-Csv -LiteralPath (Join-Path $logDir "performance_matrix.csv") -NoTypeInformation -Encoding UTF8
@("# W1.5 RK3588 Acceptance", "", "- warmupSeconds: $WarmupSeconds", "- measureSeconds: $MeasureSeconds", "- sensorPixelAngleUrad: $SensorPixelAngleUrad", "", ($rows | Format-Table -AutoSize | Out-String)) | Set-Content -LiteralPath (Join-Path $logDir "test_summary.md") -Encoding UTF8
Write-Output "LOGDIR=$logDir"
