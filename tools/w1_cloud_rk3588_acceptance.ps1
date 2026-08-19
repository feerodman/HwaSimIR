param(
    [Parameter(Mandatory=$true)][string]$SshKey,
    [string]$LogDir = "",
    [string]$BoardHost = "192.168.1.116",
    [string]$WindowsHost = "192.168.1.188",
    [int]$WarmupSeconds = 5,
    [int]$MeasureSeconds = 30,
    [ValidateSet("", "clear", "cloudy", "overcast")][string]$Scenario = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $LogDir) {
    $LogDir = Join-Path $root ("logs\w1-world-cloud-rk3588-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$videoExe = Join-Path $root "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$stimExe = Join-Path $root "build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe"
$inputFile = Join-Path $root "DataDrivenTestQT\1.txt"
foreach ($path in @($SshKey, $videoExe, $stimExe, $inputFile)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing test file: $path" }
}

function Invoke-Ssh([string]$Command) {
    & ssh.exe -i $SshKey -o BatchMode=yes "root@$BoardHost" $Command
    if ($LASTEXITCODE -ne 0) { throw "Board command failed: $Command" }
}

function Copy-FromBoard([string]$Remote, [string]$Local) {
    & scp.exe -i $SshKey -o BatchMode=yes "root@$BoardHost`:$Remote" $Local
    if ($LASTEXITCODE -ne 0) { throw "Failed to copy board log: $Remote" }
}

function Get-Value([string]$Line, [string]$Name) {
    $match = [regex]::Match($Line, '(?:^| )' + [regex]::Escape($Name) + '=([-+0-9.]+)')
    if ($match.Success) { return [double]$match.Groups[1].Value }
    return $null
}

function Get-Stats($Lines, [string]$Name) {
    $values = @($Lines | ForEach-Object { Get-Value $_ $Name } | Where-Object { $null -ne $_ })
    if (-not $values.Count) { return $null }
    $stats = $values | Measure-Object -Average -Minimum -Maximum
    return [ordered]@{
        count = $stats.Count
        avg = [math]::Round($stats.Average, 3)
        min = [math]::Round($stats.Minimum, 3)
        max = [math]::Round($stats.Maximum, 3)
    }
}

$route = & ssh.exe -i $SshKey -o BatchMode=yes "root@$BoardHost" "ip route"
if ($LASTEXITCODE -ne 0) { throw "Cannot connect to the board" }
$route | Set-Content -LiteralPath (Join-Path $LogDir "ip_route.txt") -Encoding UTF8
if ($route -match '(?m)^default\s') { throw "Board has a default route; test stopped" }

$videoNetwork = Join-Path $LogDir "NetworkConfig_video.ini"
$stimNetwork = Join-Path $LogDir "NetworkConfig_stim.ini"
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($videoNetwork, "[Network]`r`nip=$WindowsHost`r`nport=5555`r`n`r`n[Recorder]`r`nMaxRecordingQueueFrames=180`r`nFlushTimeoutMs=10000`r`n", $utf8)
[IO.File]::WriteAllText($stimNetwork, "[Identity]`r`nchannel=precise`r`nplatID=1001`r`nsensorID=2`r`n`r`n[UDP]`r`nlocalIp=$WindowsHost`r`nlocalPort=9998`r`nremoteIp=$BoardHost`r`nremotePort=8888`r`n", $utf8)

$cases = @(
    [pscustomobject]@{ Name="clear"; EnvSky=0 },
    [pscustomobject]@{ Name="cloudy"; EnvSky=1 },
    [pscustomobject]@{ Name="overcast"; EnvSky=5 }
)
if ($Scenario) { $cases = @($cases | Where-Object Name -eq $Scenario) }
$rows = New-Object System.Collections.Generic.List[object]
$runSeconds = $WarmupSeconds + $MeasureSeconds
$env:QT_FORCE_STDERR_LOGGING = "1"

foreach ($case in $cases) {
    $caseDir = Join-Path $LogDir $case.Name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    Get-Process HwaSim_IR_VideoDisplay -ErrorAction SilentlyContinue | Stop-Process -Force
    Invoke-Ssh "pkill -TERM -x HwaSim_IR 2>/dev/null || true"
    Start-Sleep -Seconds 2

    $video = Start-Process -FilePath $videoExe -ArgumentList @("--network-config", $videoNetwork) `
        -WorkingDirectory (Split-Path $videoExe) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $caseDir "VideoDisplay.out.log") `
        -RedirectStandardError (Join-Path $caseDir "VideoDisplay.err.log")
    try {
        Start-Sleep -Seconds 3
        $listen = netstat.exe -ano -p tcp | Select-String -SimpleMatch "$WindowsHost`:5555"
        if (-not ($listen -match 'LISTENING')) { throw "VideoDisplay is not listening on ${WindowsHost}:5555" }

        $remoteDir = "/userdata/HwaSimIR/logs/w1_world_cloud/$($case.Name)"
        $start = "mkdir -p $remoteDir; cd /userdata/HwaSimIR; " +
            "export PANDA3D_ROOT=/opt/panda3d-aarch64 PRC_DIR=/opt/panda3d-aarch64/etc PANDA_PRC_DIR=/opt/panda3d-aarch64/etc; " +
            "export LD_LIBRARY_PATH=/opt/panda3d-aarch64/lib:/usr/lib/aarch64-linux-gnu/mali:/usr/lib/aarch64-linux-gnu:/lib/aarch64-linux-gnu; " +
            "export DISPLAY=:0 XAUTHORITY=/root/.Xauthority RenderPresentationMode=HeadlessOffscreen EnablePerfLog=1 QuietPerfMode=false; " +
            "export H264Encoder=mpp H264FallbackToJpeg=false TcpPacketVersion=3 TcpSendVideo=true TcpSendAnnotation=true TcpSendRealtimeData=true; " +
            "unset EGL_PLATFORM LIBGL_ALWAYS_SOFTWARE MESA_LOADER_DRIVER_OVERRIDE; " +
            "nohup ./HwaSim_IR --channel precise </dev/null >$remoteDir/HwaSim_IR.log 2>&1 & echo `$! >$remoteDir/HwaSim_IR.pid"
        Invoke-Ssh $start
        Start-Sleep -Seconds 5
        Invoke-Ssh "grep '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1' $remoteDir/HwaSim_IR.log"
        Invoke-Ssh "pid=`$(cat $remoteDir/HwaSim_IR.pid); nohup sh -c 'while kill -0 '`"`$pid`"' 2>/dev/null; do date +timestamp=%s; ps -p '`"`$pid`"' -o pid=,pcpu=,pmem=,rss=,nlwp=,etime=; sleep 2; done' >$remoteDir/cpu.log 2>&1 </dev/null &"

        $stimArgs = @(
            "--duration-sec=$runSeconds", "--phase1d-h264=1", "--save-mp4=0",
            "--plat-id=1001", "--sensor-id=2", "--sim-mode=2", "--video-fps=60",
            "--env-sky=$($case.EnvSky)", "--sensor-band=2", "--network-config=$stimNetwork",
            "--channel=precise", "--input-file=$inputFile"
        )
        $stim = Start-Process -FilePath $stimExe -ArgumentList $stimArgs -WorkingDirectory (Join-Path $root "DataDrivenTestQT") `
            -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $caseDir "DataDrivenTestQT.out.log") `
            -RedirectStandardError (Join-Path $caseDir "DataDrivenTestQT.err.log")
        if (-not $stim.WaitForExit(($runSeconds + 20) * 1000)) {
            Stop-Process -Id $stim.Id -Force
            throw "Stimulus timed out: $($case.Name)"
        }
        Start-Sleep -Seconds 3
        Invoke-Ssh "pkill -TERM -x HwaSim_IR 2>/dev/null || true"
        Start-Sleep -Seconds 2
        Copy-FromBoard "$remoteDir/HwaSim_IR.log" (Join-Path $caseDir "HwaSim_IR.log")
        Copy-FromBoard "$remoteDir/cpu.log" (Join-Path $caseDir "cpu.log")
    }
    finally {
        if ($video -and -not $video.HasExited) { Stop-Process -Id $video.Id -Force }
        try { Invoke-Ssh "pkill -TERM -x HwaSim_IR 2>/dev/null || true" } catch { Write-Warning $_ }
    }

    $board = Get-Content -LiteralPath (Join-Path $caseDir "HwaSim_IR.log")
    $display = Get-Content -LiteralPath (Join-Path $caseDir "VideoDisplay.err.log")
    $perf = @($board | Where-Object { $_ -match '^\[Perf\].*mode=async' -and (Get-Value $_ 'udpFps') -ge 50 } | Select-Object -Skip $WarmupSeconds)
    $tcp = @($board | Where-Object { $_ -match '^\[TcpPerf\].*activeCodec=h264_annexb' } | Select-Object -Skip $WarmupSeconds)
    $videoPerf = @($display | Where-Object { $_ -match '^\[VideoPerf\].*activeCodec=h264_annexb' } | Select-Object -Skip $WarmupSeconds)
    $weather = @($board | Where-Object {
        $_ -match '^\[Stage7 Perf\]' -and $_ -match 'textureLoadCountThisFrame=0'
    })
    $cpuValues = @(Get-Content (Join-Path $caseDir "cpu.log") | ForEach-Object { if ($_ -match '^\s*\d+\s+([0-9.]+)\s+') { [double]$Matches[1] } })
    $cpuAvg = if ($cpuValues.Count) { [math]::Round(($cpuValues | Measure-Object -Average).Average, 3) } else { $null }
    $row = [pscustomobject]@{
        caseName=$case.Name
        gpuPass=[bool]($board -match '\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1')
        encodePass=[bool]($board -match '\[H264EncodeSuccess\].*backend=mpp')
        decodePass=[bool]($display -match '\[H264DecodeSuccess\].*backend=ffmpeg')
        packetPass=[bool]($display -match 'packetVersion=3.*flags=0x7.*codec=h264_annexb')
        fallbackSeen=[bool]($board -match '\[CodecFallback\]|activeBackend=(jpeg|ffmpeg)')
        weatherStatePass=[bool]($board -match "\[WeatherCloud\].*envSky=$($case.EnvSky)")
        renderFps=(Get-Stats $perf 'renderFps').avg
        outputFps=(Get-Stats $perf 'outputFps').avg
        displayFps=(Get-Stats $videoPerf 'displayFps').avg
        renderMs=(Get-Stats $perf 'renderMs').avg
        weatherMs=(Get-Stats $weather 'weatherUpdateMs').avg
        readbackMs=(Get-Stats $perf 'readbackMs').avg
        preprocessMs=(Get-Stats $tcp 'flipMs').avg
        mppEncodeMs=(Get-Stats $tcp 'h264EncodeMs').avg
        inputQueueMax=(Get-Stats $perf 'inputQueueDepth').max
        outputQueueMax=(Get-Stats $perf 'outputQueueDepth').max
        sourceSeqLagMax=(Get-Stats $perf 'sourceSeqLag').max
        droppedMax=(Get-Stats $perf 'dropped').max
        cpuPercent=$cpuAvg
    }
    $row | Add-Member -NotePropertyName fpsPass -NotePropertyValue ([bool](
        $null -ne $row.outputFps -and $null -ne $row.displayFps -and
        $row.outputFps -ge 59.0 -and $row.displayFps -ge 59.0))
    $row | Add-Member -NotePropertyName queuePass -NotePropertyValue ([bool](
        $row.inputQueueMax -le 1 -and $row.outputQueueMax -le 1 -and
        $row.sourceSeqLagMax -le 1 -and $row.droppedMax -eq 0))
    $rows.Add($row)
    $row | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $caseDir "metrics.json") -Encoding UTF8
    $row | Format-List
}

$rows | Export-Csv -LiteralPath (Join-Path $LogDir "weather_matrix.csv") -NoTypeInformation -Encoding UTF8
$overall = -not ($rows | Where-Object {
    -not $_.gpuPass -or -not $_.encodePass -or -not $_.decodePass -or
    -not $_.packetPass -or $_.fallbackSeen -or -not $_.weatherStatePass -or
    -not $_.fpsPass -or -not $_.queuePass
})
@(
    "# W1 RK3588 World-space Cloud Acceptance",
    "",
    "- Warmup: $WarmupSeconds seconds",
    "- Measurement: $MeasureSeconds seconds",
    "- Overall: $(if ($overall) { 'PASS' } else { 'FAIL' })",
    "",
    ($rows | Format-Table -AutoSize | Out-String)
) | Set-Content -LiteralPath (Join-Path $LogDir "test_summary.md") -Encoding UTF8
Write-Output "LOGDIR=$LogDir"
if (-not $overall) { exit 1 }
