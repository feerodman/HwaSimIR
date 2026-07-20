param(
    [int]$StartupDelaySec = 6,
    [int]$AckTimeoutMs = 15000
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$hwaExe = Join-Path $root "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $root "HwaSim_IR\Bin"
$gxx = "D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\g++.exe"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logDir = Join-Path $root "logs\r1-runtime-$stamp"
$senderExe = Join-Path $logDir "r1_protocol_sender.exe"
$utf8 = New-Object Text.UTF8Encoding($false)

foreach ($required in @($hwaExe, $gxx, (Join-Path $root "tools\r1_protocol_sender.cpp"))) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required path not found: $required"
    }
}
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

& $gxx -std=c++11 -O2 -I $root (Join-Path $root "tools\r1_protocol_sender.cpp") -lws2_32 -o $senderExe
if ($LASTEXITCODE -ne 0) {
    throw "r1_protocol_sender build failed: $LASTEXITCODE"
}

function Start-HwaInstance {
    param(
        [string[]]$Arguments,
        [string]$Name
    )

    return Start-Process -FilePath $hwaExe `
        -ArgumentList $Arguments `
        -WorkingDirectory $hwaWorkDir `
        -WindowStyle Hidden `
        -PassThru `
        -RedirectStandardOutput (Join-Path $logDir "$Name.out.log") `
        -RedirectStandardError (Join-Path $logDir "$Name.err.log")
}

function Stop-HwaInstances {
    param([object[]]$Processes)

    foreach ($process in $Processes) {
        if ($null -ne $process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
            }
            $process.WaitForExit()
        }
    }
}

function Assert-LogContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Label
    )

    $content = if (Test-Path -LiteralPath $Path) {
        [IO.File]::ReadAllText($Path)
    }
    else {
        ""
    }
    if ($content -notmatch $Pattern) {
        throw "$Label not found in $Path (pattern=$Pattern)"
    }
}

function Invoke-Sender {
    param(
        [string]$Name,
        [string[]]$Arguments
    )

    $output = & $senderExe @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    [IO.File]::WriteAllLines((Join-Path $logDir "$Name.log"), [string[]]$output, $utf8)
    if ($exitCode -ne 0) {
        throw "Protocol sender failed for $Name (exit=$exitCode): $output"
    }
    return $output
}

function New-InstanceConfig {
    param(
        [string]$Path,
        [string]$Channel,
        [int]$SensorID,
        [int]$UdpPort,
        [int]$AckPort,
        [int]$TcpPort
    )

    $content = @"
[Identity]
channel=$Channel
platID=1001
sensorID=$SensorID
acceptSensorBroadcast=true
allowDynamicRemote=false

[UDP]
localIp=127.0.0.1
localPort=$UdpPort
remoteIp=127.0.0.1
remotePort=$AckPort

[TCP]
serverIp=127.0.0.1
serverPort=$TcpPort
"@
    [IO.File]::WriteAllText($Path, $content, $utf8)
}

$oldPresentationMode = $env:RenderPresentationMode
$oldWindowPreview = $env:RenderWindowPreview
$oldPerfLog = $env:EnablePerfLog
$channelProcesses = @()
$routeProcesses = @()

try {
    $env:RenderPresentationMode = "HeadlessOffscreen"
    $env:RenderWindowPreview = "0"
    $env:EnablePerfLog = "0"

    $channelProcesses = @(
        (Start-HwaInstance -Arguments @("--channel", "precise") -Name "channel-precise"),
        (Start-HwaInstance -Arguments @("--channel", "coarse") -Name "channel-coarse")
    )
    Start-Sleep -Seconds $StartupDelaySec
    foreach ($process in $channelProcesses) {
        $process.Refresh()
        if ($process.HasExited) {
            throw "Channel smoke process exited early: pid=$($process.Id) code=$($process.ExitCode)"
        }
    }
    Stop-HwaInstances -Processes $channelProcesses
    Assert-LogContains (Join-Path $logDir "channel-precise.out.log") `
        '\[RuntimeInstance\].*channel=precise.*udpLocal=.*:8888.*configSource=cli-channel' `
        "precise channel selection"
    Assert-LogContains (Join-Path $logDir "channel-coarse.out.log") `
        '\[RuntimeInstance\].*channel=coarse.*udpLocal=.*:8889.*configSource=cli-channel' `
        "coarse channel selection"
    $channelProcesses = @()

    $basePort = Get-Random -Minimum 22000 -Maximum 32000
    $preciseUdpPort = $basePort
    $coarseUdpPort = $basePort + 1
    $preciseAckPort = $basePort + 100
    $coarseAckPort = $basePort + 101
    $preciseConfig = Join-Path $logDir "NetworkConfig_precise_test.ini"
    $coarseConfig = Join-Path $logDir "NetworkConfig_coarse_test.ini"
    New-InstanceConfig $preciseConfig "precise" 2 $preciseUdpPort $preciseAckPort ($basePort + 200)
    New-InstanceConfig $coarseConfig "coarse" 1 $coarseUdpPort $coarseAckPort ($basePort + 201)

    $routeProcesses = @(
        (Start-HwaInstance -Arguments @("--network-config", $preciseConfig) -Name "route-precise"),
        (Start-HwaInstance -Arguments @("--network-config", $coarseConfig) -Name "route-coarse")
    )
    Start-Sleep -Seconds $StartupDelaySec
    foreach ($process in $routeProcesses) {
        $process.Refresh()
        if ($process.HasExited) {
            throw "Route smoke process exited early: pid=$($process.Id) code=$($process.ExitCode)"
        }
    }

    Invoke-Sender "control-precise-accept" @("--type=control", "--port=$preciseUdpPort", "--plat-id=1001", "--command=1") | Out-Null
    Invoke-Sender "control-coarse-accept" @("--type=control", "--port=$coarseUdpPort", "--plat-id=1001", "--command=1") | Out-Null
    Invoke-Sender "control-precise-wrong-plat" @("--type=control", "--port=$preciseUdpPort", "--plat-id=9999", "--command=1") | Out-Null
    Invoke-Sender "control-coarse-wrong-plat" @("--type=control", "--port=$coarseUdpPort", "--plat-id=9999", "--command=1") | Out-Null

    Invoke-Sender "init-precise-sync-exact" @(
        "--type=init", "--port=$preciseUdpPort", "--source-port=$($basePort + 300)",
        "--plat-id=1001", "--sensor-id=2", "--sim-mode=1", "--video-fps=60",
        "--ack-port=$preciseAckPort", "--expect-ack=1", "--ack-plat-id=1001", "--ack-sensor-id=2",
        "--timeout-ms=$AckTimeoutMs") | Out-Null
    Invoke-Sender "init-coarse-sync-exact" @(
        "--type=init", "--port=$coarseUdpPort", "--source-port=$($basePort + 301)",
        "--plat-id=1001", "--sensor-id=1", "--sim-mode=1", "--video-fps=60",
        "--ack-port=$coarseAckPort", "--expect-ack=1", "--ack-plat-id=1001", "--ack-sensor-id=1",
        "--timeout-ms=$AckTimeoutMs") | Out-Null
    Invoke-Sender "init-precise-wrong-sensor" @(
        "--type=init", "--port=$preciseUdpPort", "--source-port=$($basePort + 302)",
        "--plat-id=1001", "--sensor-id=1", "--sim-mode=2", "--video-fps=60",
        "--ack-port=$preciseAckPort", "--expect-ack=0", "--timeout-ms=1500") | Out-Null
    Invoke-Sender "init-precise-broadcast-async" @(
        "--type=init", "--port=$preciseUdpPort", "--source-port=$($basePort + 303)",
        "--plat-id=1001", "--sensor-id=255", "--sim-mode=2", "--video-fps=60",
        "--ack-port=$preciseAckPort", "--expect-ack=1", "--ack-plat-id=1001", "--ack-sensor-id=2",
        "--timeout-ms=$AckTimeoutMs") | Out-Null
    Invoke-Sender "init-coarse-broadcast-async" @(
        "--type=init", "--port=$coarseUdpPort", "--source-port=$($basePort + 304)",
        "--plat-id=1001", "--sensor-id=255", "--sim-mode=2", "--video-fps=60",
        "--ack-port=$coarseAckPort", "--expect-ack=1", "--ack-plat-id=1001", "--ack-sensor-id=1",
        "--timeout-ms=$AckTimeoutMs") | Out-Null
    Invoke-Sender "init-precise-invalid-mode-fallback" @(
        "--type=init", "--port=$preciseUdpPort", "--source-port=$($basePort + 305)",
        "--plat-id=1001", "--sensor-id=2", "--sim-mode=99", "--video-fps=30",
        "--ack-port=$preciseAckPort", "--expect-ack=1", "--ack-plat-id=1001", "--ack-sensor-id=2",
        "--timeout-ms=$AckTimeoutMs") | Out-Null

    Invoke-Sender "display-precise-exact" @("--type=display", "--port=$preciseUdpPort", "--plat-id=1001", "--sensor-id=2") | Out-Null
    Invoke-Sender "display-coarse-exact" @("--type=display", "--port=$coarseUdpPort", "--plat-id=1001", "--sensor-id=1") | Out-Null
    Invoke-Sender "display-precise-wrong-sensor" @("--type=display", "--port=$preciseUdpPort", "--plat-id=1001", "--sensor-id=1") | Out-Null
    Invoke-Sender "display-precise-broadcast" @("--type=display", "--port=$preciseUdpPort", "--plat-id=1001", "--sensor-id=255") | Out-Null
    Invoke-Sender "display-coarse-broadcast" @("--type=display", "--port=$coarseUdpPort", "--plat-id=1001", "--sensor-id=255") | Out-Null
    Invoke-Sender "display-precise-wrong-plat" @("--type=display", "--port=$preciseUdpPort", "--plat-id=9999", "--sensor-id=2") | Out-Null
    Invoke-Sender "display-coarse-wrong-plat" @("--type=display", "--port=$coarseUdpPort", "--plat-id=9999", "--sensor-id=1") | Out-Null

    Start-Sleep -Seconds 3
    foreach ($process in $routeProcesses) {
        $process.Refresh()
        if ($process.HasExited) {
            throw "Route smoke process exited during routing tests: pid=$($process.Id) code=$($process.ExitCode)"
        }
    }
    Stop-HwaInstances -Processes $routeProcesses
    $preciseLog = Join-Path $logDir "route-precise.out.log"
    $coarseLog = Join-Path $logDir "route-coarse.out.log"
    Assert-LogContains $preciseLog '\[RuntimeInstance\].*channel=precise.*allowDynamicRemote=0.*configSource=cli-network-config' "precise custom config"
    Assert-LogContains $coarseLog '\[RuntimeInstance\].*channel=coarse.*allowDynamicRemote=0.*configSource=cli-network-config' "coarse custom config"
    Assert-LogContains $preciseLog '\[PacketRoute\] flag=0x41 accepted=1.*packetPlatID=1001.*reason=plat_match' "control route accept"
    Assert-LogContains $preciseLog '\[PacketRouteReject\] flag=0x41 accepted=0.*packetPlatID=9999.*reason=plat_mismatch' "control route reject"
    Assert-LogContains $preciseLog '\[PacketRoute\] flag=0x36 accepted=1.*packetSensorID=2.*reason=exact_match' "precise init exact"
    Assert-LogContains $coarseLog '\[PacketRoute\] flag=0x36 accepted=1.*packetSensorID=1.*reason=exact_match' "coarse init exact"
    Assert-LogContains $preciseLog '\[PacketRouteReject\] flag=0x36 accepted=0.*packetSensorID=1.*reason=sensor_mismatch' "init wrong sensor"
    Assert-LogContains $preciseLog '\[PacketRoute\] flag=0x36 accepted=1.*packetSensorID=255.*reason=sensor_broadcast' "init broadcast"
    Assert-LogContains $preciseLog '\[PacketRoute\] flag=0x38 accepted=1.*packetSensorID=2.*reason=exact_match' "display exact"
    Assert-LogContains $preciseLog '\[PacketRouteReject\] flag=0x38 accepted=0.*packetSensorID=1.*reason=sensor_mismatch' "display wrong sensor"
    Assert-LogContains $preciseLog '\[PacketRoute\] flag=0x38 accepted=1.*packetSensorID=255.*reason=sensor_broadcast' "display broadcast"
    Assert-LogContains $preciseLog '\[PacketRouteReject\] flag=0x38 accepted=0.*packetPlatID=9999.*reason=plat_mismatch' "display wrong plat"
    Assert-LogContains $preciseLog '\[RenderControl\].*externalSimMode=1.*effectiveSimMode=1.*source=udp_init' "sync mode apply"
    Assert-LogContains $preciseLog '\[RenderControl\].*externalSimMode=2.*externalVideoFps=60.*effectiveSimMode=2.*effectiveVideoFps=60.*source=udp_init' "async 60 mode apply"
    Assert-LogContains $preciseLog '\[RenderControl\].*externalSimMode=99.*externalModeValid=0.*effectiveSimMode=2.*effectiveVideoFps=60.*source=config_fallback' "invalid mode config fallback"

    $binaryHash = (Get-FileHash -LiteralPath $hwaExe -Algorithm SHA256).Hash
    $summary = @(
        "[R1Smoke] status=PASS",
        "[R1Smoke] hwaBinary=$hwaExe",
        "[R1Smoke] hwaSha256=$binaryHash",
        "[R1Smoke] channelPrecisePid=$($routeProcesses[0].Id)",
        "[R1Smoke] channelCoarsePid=$($routeProcesses[1].Id)",
        "[R1Smoke] preciseUdpPort=$preciseUdpPort coarseUdpPort=$coarseUdpPort",
        "[R1Smoke] protocolSizes=24,385,506,17",
        "[R1Smoke] tests=channel_dual,control,init_exact,init_wrong_sensor,init_broadcast,display_exact,display_wrong_sensor,display_broadcast,wrong_plat,ack_identity,dynamic_remote_disabled,simMode1,simMode2_fps60,invalid_simMode_fallback"
    )
    [IO.File]::WriteAllLines((Join-Path $logDir "summary.log"), $summary, $utf8)
    $summary | ForEach-Object { Write-Output $_ }
    Write-Output "LOGDIR=$logDir"
}
finally {
    Stop-HwaInstances -Processes $routeProcesses
    Stop-HwaInstances -Processes $channelProcesses
    $env:RenderPresentationMode = $oldPresentationMode
    $env:RenderWindowPreview = $oldWindowPreview
    $env:EnablePerfLog = $oldPerfLog
}
