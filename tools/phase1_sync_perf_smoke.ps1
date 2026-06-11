param(
    [string[]]$VideoFps = @("25", "60"),
    [int]$DurationSeconds = 6,
    [switch]$Strict
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Split-Path -Parent $hwaExe
$hwaNetworkConfig = Join-Path $hwaWorkDir "Config\NetworkConfig.ini"
$videoExe = Join-Path $rootPath "HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe"
$videoWorkDir = Split-Path -Parent $videoExe
$videoNetworkConfig = Join-Path $videoWorkDir "NetworkConfig.ini"
$logDir = Join-Path $rootPath "logs\phase1"
$videoFpsValues = @(
    foreach ($fpsArgument in $VideoFps) {
        foreach ($fpsToken in ($fpsArgument -split ',')) {
            $parsedFps = 0
            if (-not [int]::TryParse($fpsToken.Trim(), [ref]$parsedFps) -or $parsedFps -lt 1 -or $parsedFps -gt 240) {
                throw "VideoFps must be an integer in range 1..240: $fpsToken"
            }
            $parsedFps
        }
    }
)

function Assert-Path {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Normalize-ProcessPathEnvironment {
    $processPathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
    if ([string]::IsNullOrEmpty($processPathValue)) {
        $processPathValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
    }
    if (-not [string]::IsNullOrEmpty($processPathValue)) {
        [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
        [Environment]::SetEnvironmentVariable("Path", $processPathValue, "Process")
    }
}

function New-InitPacket {
    param([int]$Fps)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    function WInt([int]$value) { $bw.Write($value) }
    function WBool([bool]$value) { $bw.Write([byte]$(if ($value) { 1 } else { 0 })) }
    function WDouble([double]$value) { $bw.Write($value) }
    function WSpatial([double]$lat, [double]$lon, [double]$alt, [double]$yaw, [double]$pitch, [double]$roll, [double]$speed) {
        WDouble $lat; WDouble $lon; WDouble $alt
        WDouble $yaw; WDouble $pitch; WDouble $roll
        WDouble $speed
    }
    function WPlat([int]$id, [int]$type) {
        WInt $id
        WInt $type
        WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
    }
    function WSensor {
        WInt 0
        WBool $true
        WBool $true
        WBool $false
        WDouble 0.1
        WDouble 0.1
        WBool $false
        WDouble 0.0
        WBool $false
        WBool $false
        WInt 2
        WInt 800
        WInt 800
        WInt 1
        WInt 50000
        WDouble 0.0
        for ($i = 0; $i -lt 32; ++$i) {
            WDouble 0.0
        }
        WInt 0
        WDouble 0.0
    }

    WInt 0x36
    WInt 1
    WInt 1
    WInt 1
    WInt 1
    WPlat 1 0x11
    WPlat 2 0x11

    WBool $true
    WInt 0
    WInt 0
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 1.0
    WDouble 1.0
    WDouble 1.0
    WDouble 25.0
    WDouble 40.0
    WDouble 23000.0
    WDouble 0.0
    WDouble 0.0
    WInt $Fps
    WSensor

    WInt 3
    WInt 2
    WInt 0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-ControlPacket {
    param([int]$Command)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([int]0x41)
    $bw.Write([int]1)
    $bw.Write([int]1)
    $bw.Write([int]2)
    $bw.Write([int]$Command)
    $bw.Write([int]0)
    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-DisplayPacket {
    param([double]$TimeMs, [int]$FrameIndex)

    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    function WInt([int]$value) { $bw.Write($value) }
    function WBool([bool]$value) { $bw.Write([byte]$(if ($value) { 1 } else { 0 })) }
    function WDouble([double]$value) { $bw.Write($value) }
    function WSpatial([double]$lat, [double]$lon, [double]$alt, [double]$yaw, [double]$pitch, [double]$roll, [double]$speed) {
        WDouble $lat; WDouble $lon; WDouble $alt
        WDouble $yaw; WDouble $pitch; WDouble $roll
        WDouble $speed
    }
    function WWeaponState {
        WInt 0x22
        WInt 1
        WInt 0
        WDouble 0.0
        WDouble 0.0
        WBool $false
        WBool $false
        WDouble 0.0
        WDouble 0.0
        WBool $true
        WInt 0
        WBool $false
        WInt 0
    }
    function WTargetState([int]$targetType, [int]$targetId, [bool]$engineState, [double]$lonOffset) {
        WInt $targetType
        WInt 1
        WInt $targetId
        WBool $engineState
        WBool $true
        WSpatial 0.0 $lonOffset 1000.0 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    $yaw = [double](($FrameIndex % 360) * 0.1)
    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 1000.0 $yaw 0.0 0.0 0.0
    WWeaponState
    WInt 1
    WTargetState 0x22 0 $true 0.010
    WTargetState 0x22 1 $false 0.015
    WTargetState 0x22 2 $false 0.020
    WTargetState 0x33 3 $false 0.025
    WTargetState 0x33 4 $false 0.030

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Get-MetricValue {
    param([string]$Line, [string]$Name)
    if ($Line -match "(?:^|\s)$([regex]::Escape($Name))=(-?[0-9]+(?:\.[0-9]+)?)") {
        return [double]$Matches[1]
    }
    return $null
}

function Get-AverageMetric {
    param([string[]]$Lines, [string]$Name)
    $values = @(
        foreach ($line in $Lines) {
            $value = Get-MetricValue -Line $line -Name $Name
            if ($null -ne $value) { $value }
        }
    )
    if ($values.Count -eq 0) { return 0.0 }
    return [double](($values | Measure-Object -Average).Average)
}

function Stop-TestProcess {
    param($Process)
    if ($null -eq $Process) { return }
    $Process.Refresh()
    if (-not $Process.HasExited) {
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

function Wait-LogPattern {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutSeconds,
        $Process,
        [string]$Label
    )

    $clock = [System.Diagnostics.Stopwatch]::StartNew()
    while ($clock.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($null -ne $Process) {
            $Process.Refresh()
            if ($Process.HasExited) {
                throw "$Label process exited before readiness (exitCode=$($Process.ExitCode))"
            }
        }
        if (Test-Path -LiteralPath $Path) {
            if (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) {
                return
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out after $TimeoutSeconds seconds waiting for $Label readiness: $Pattern"
}

function Invoke-PerfRun {
    param([int]$Fps)

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $hwaOut = Join-Path $logDir "HwaSimIR-phase1-${Fps}fps-$stamp.out.log"
    $hwaErr = Join-Path $logDir "HwaSimIR-phase1-${Fps}fps-$stamp.err.log"
    $videoOut = Join-Path $logDir "VideoDisplay-phase1-${Fps}fps-$stamp.out.log"
    $videoErr = Join-Path $logDir "VideoDisplay-phase1-${Fps}fps-$stamp.err.log"
    $videoProcess = $null
    $hwaProcess = $null
    $udp = $null
    $sentFrames = 0
    $sendElapsedSec = 0.0

    try {
        Normalize-ProcessPathEnvironment
        $videoProcess = Start-Process -FilePath $videoExe -WorkingDirectory $videoWorkDir -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput $videoOut -RedirectStandardError $videoErr
        Start-Sleep -Seconds 2

        $hwaProcess = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden `
            -RedirectStandardOutput $hwaOut -RedirectStandardError $hwaErr
        Start-Sleep -Seconds 5

        $udp = New-Object System.Net.Sockets.UdpClient
        $initPacket = New-InitPacket -Fps $Fps
        [void]$udp.Send($initPacket, $initPacket.Length, "127.0.0.1", 8888)
        Wait-LogPattern -Path $hwaOut -Pattern '\[Stage0\] Init render setup complete' `
            -TimeoutSeconds 60 -Process $hwaProcess -Label "HwaSimIR init"

        $startPacket = New-ControlPacket -Command 2
        [void]$udp.Send($startPacket, $startPacket.Length, "127.0.0.1", 8888)
        Wait-LogPattern -Path $hwaOut -Pattern '\[Stage0\] Control command received: command=2' `
            -TimeoutSeconds 10 -Process $hwaProcess -Label "HwaSimIR start"
        Start-Sleep -Milliseconds 250

        $frameCount = $Fps * $DurationSeconds
        $packets = New-Object 'System.Collections.Generic.List[byte[]]'
        for ($i = 0; $i -lt $frameCount; ++$i) {
            $timeMs = 1000.0 * ($i + 1) / [double]$Fps
            $packets.Add((New-DisplayPacket -TimeMs $timeMs -FrameIndex $i))
        }

        $clock = [System.Diagnostics.Stopwatch]::StartNew()
        $frequency = [double][System.Diagnostics.Stopwatch]::Frequency
        for ($i = 0; $i -lt $frameCount; ++$i) {
            $deadlineTicks = [long](($i * $frequency) / [double]$Fps)
            while ($clock.ElapsedTicks -lt $deadlineTicks) {
                $remainingMs = 1000.0 * ($deadlineTicks - $clock.ElapsedTicks) / $frequency
                if ($remainingMs -gt 2.0) {
                    Start-Sleep -Milliseconds 1
                }
                else {
                    [System.Threading.Thread]::SpinWait(100)
                }
            }
            $packet = $packets[$i]
            $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            if ($sent -eq $packet.Length) {
                ++$sentFrames
            }
        }
        $clock.Stop()
        $sendElapsedSec = [Math]::Max(0.001, $clock.Elapsed.TotalSeconds)

        $stopPacket = New-ControlPacket -Command 3
        Start-Sleep -Seconds 5
        [void]$udp.Send($stopPacket, $stopPacket.Length, "127.0.0.1", 8888)
        Start-Sleep -Seconds 2
    }
    finally {
        if ($udp) { $udp.Close() }
        Stop-TestProcess -Process $hwaProcess
        Stop-TestProcess -Process $videoProcess
    }

    $hwaText = ""
    if (Test-Path -LiteralPath $hwaOut) { $hwaText += Get-Content -LiteralPath $hwaOut -Raw -Encoding UTF8 }
    if (Test-Path -LiteralPath $hwaErr) { $hwaText += "`n" + (Get-Content -LiteralPath $hwaErr -Raw -Encoding UTF8) }
    $videoText = ""
    if (Test-Path -LiteralPath $videoOut) { $videoText += Get-Content -LiteralPath $videoOut -Raw -Encoding UTF8 }
    if (Test-Path -LiteralPath $videoErr) { $videoText += "`n" + (Get-Content -LiteralPath $videoErr -Raw -Encoding UTF8) }

    $perfLines = @($hwaText -split "`r?`n" | Where-Object { $_ -match '^\[Perf\]' })
    $activePerfLines = @(
        $perfLines | Where-Object {
            (Get-MetricValue -Line $_ -Name "outputFps") -gt 0.5 -and
            (Get-MetricValue -Line $_ -Name "outputFps") -lt ($Fps * 2.0)
        }
    )
    if ($activePerfLines.Count -gt 1) {
        $activePerfLines = @($activePerfLines | Select-Object -Skip 1)
    }
    $lastPerf = $perfLines | Select-Object -Last 1
    $videoPerfLines = @($videoText -split "`r?`n" | Where-Object { $_ -match '\[VideoPerf\]' })
    $lastVideoPerf = $videoPerfLines | Select-Object -Last 1

    $result = [PSCustomObject]@{
        videoFpsTarget = $Fps
        sentFrames = $sentFrames
        sentFps = [Math]::Round(
            $(if ($sentFrames -gt 1) { ($sentFrames - 1) / $sendElapsedSec } else { $sentFrames / $sendElapsedSec }),
            3)
        udpFps = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "udpFps"), 3)
        renderFps = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "renderFps"), 3)
        outputFps = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "outputFps"), 3)
        latencyAvgMs = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "latencyAvgMs"), 3)
        readbackMs = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "readbackMs"), 3)
        jpegMs = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "jpegMs"), 3)
        tcpSendMs = [Math]::Round((Get-AverageMetric -Lines $activePerfLines -Name "tcpSendMs"), 3)
        syncOverrunCount = [int](Get-MetricValue -Line $lastPerf -Name "syncOverrunCount")
        inputQueueOverflowCount = [int](Get-MetricValue -Line $lastPerf -Name "inputQueueOverflowCount")
        udpFrames = [int](Get-MetricValue -Line $lastPerf -Name "udpFrames")
        renderFrames = [int](Get-MetricValue -Line $lastPerf -Name "renderFrames")
        outputFrames = [int](Get-MetricValue -Line $lastPerf -Name "outputFrames")
        videoReceiveFps = [Math]::Round((Get-MetricValue -Line $lastVideoPerf -Name "receiveFps"), 3)
        videoDisplayFps = [Math]::Round((Get-MetricValue -Line $lastVideoPerf -Name "displayFps"), 3)
        videoDiscontinuities = [int](Get-MetricValue -Line $lastVideoPerf -Name "discontinuities")
        hasGpuLog = $hwaText -match '\[GPU\]'
        softwareRenderer = $hwaText -match '\[GPU\]\[WARN\] software renderer detected'
        hasPerfLog = $perfLines.Count -gt 0
        hasSyncFrameLog = $hwaText -match '\[SyncFrame\]'
        hasTcpPerfLog = $hwaText -match '\[TcpPerf\]'
        hasVideoPerfLog = $videoPerfLines.Count -gt 0
        sequenceMismatch = $hwaText -match '\[SyncFrame\].*sequenceMatch=0'
        overwritten = @(
            $hwaText -split "`r?`n" |
                Where-Object { $_ -match '^\[(TcpPerf|SyncFrame)\].*\boverwritten=1(?:\s|$)' }
        ).Count -gt 0
        hwaLog = $hwaOut
        videoLog = $videoErr
    }
    return $result
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $videoExe "VideoDisplay executable"
Assert-Path $hwaNetworkConfig "HwaSimIR network config"
Assert-Path $videoNetworkConfig "VideoDisplay network config"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$hwaConfigBackup = [System.IO.File]::ReadAllBytes($hwaNetworkConfig)
$videoConfigBackup = [System.IO.File]::ReadAllBytes($videoNetworkConfig)
$oldQtLogging = [Environment]::GetEnvironmentVariable("QT_FORCE_STDERR_LOGGING", "Process")
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$results = @()

try {
    [System.IO.File]::WriteAllText(
        $hwaNetworkConfig,
        "[UDP]`r`nlocalIp=0.0.0.0`r`nlocalPort=8888`r`nremoteIp=127.0.0.1`r`nremotePort=9999`r`n`r`n[TCP]`r`nserverIp=127.0.0.1`r`nserverPort=5555`r`n",
        $utf8NoBom)
    [System.IO.File]::WriteAllText(
        $videoNetworkConfig,
        "[Network]`r`nip=0.0.0.0`r`nport=5555`r`n",
        $utf8NoBom)
    [Environment]::SetEnvironmentVariable("QT_FORCE_STDERR_LOGGING", "1", "Process")

    foreach ($fps in $videoFpsValues) {
        Write-Host "Running Phase 1 sync performance smoke at $fps FPS..."
        $results += Invoke-PerfRun -Fps $fps
    }
}
finally {
    [System.IO.File]::WriteAllBytes($hwaNetworkConfig, $hwaConfigBackup)
    [System.IO.File]::WriteAllBytes($videoNetworkConfig, $videoConfigBackup)
    [Environment]::SetEnvironmentVariable("QT_FORCE_STDERR_LOGGING", $oldQtLogging, "Process")
}

$reportPath = Join-Path $logDir ("phase1-sync-perf-{0}.json" -f (Get-Date -Format "yyyyMMdd-HHmmss"))
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding UTF8
$results | Format-Table videoFpsTarget,sentFps,udpFps,renderFps,outputFps,latencyAvgMs,readbackMs,jpegMs,tcpSendMs,syncOverrunCount,inputQueueOverflowCount,udpFrames,renderFrames,outputFrames -AutoSize

$failures = New-Object System.Collections.Generic.List[string]
foreach ($result in $results) {
    $prefix = "$($result.videoFpsTarget) FPS"
    if (-not $result.hasGpuLog) { $failures.Add("$prefix missing [GPU]") }
    if ($result.softwareRenderer) { $failures.Add("$prefix used software renderer") }
    if (-not $result.hasPerfLog) { $failures.Add("$prefix missing [Perf]") }
    if (-not $result.hasSyncFrameLog) { $failures.Add("$prefix missing [SyncFrame]") }
    if (-not $result.hasTcpPerfLog) { $failures.Add("$prefix missing [TcpPerf]") }
    if (-not $result.hasVideoPerfLog) { $failures.Add("$prefix missing [VideoPerf]") }
    if ($result.sequenceMismatch) { $failures.Add("$prefix frame sequence mismatch") }
    if ($result.overwritten) { $failures.Add("$prefix overwrote a queued frame") }
    if ($result.inputQueueOverflowCount -ne 0) { $failures.Add("$prefix input queue overflow=$($result.inputQueueOverflowCount)") }
    if ($result.udpFrames -ne $result.sentFrames) { $failures.Add("$prefix UDP received $($result.udpFrames)/$($result.sentFrames)") }
    if ($result.renderFrames -ne $result.sentFrames) { $failures.Add("$prefix rendered $($result.renderFrames)/$($result.sentFrames)") }
    if ($result.outputFrames -ne $result.sentFrames) { $failures.Add("$prefix output $($result.outputFrames)/$($result.sentFrames)") }
    if ($result.videoDiscontinuities -ne 0) { $failures.Add("$prefix VideoDisplay discontinuities=$($result.videoDiscontinuities)") }
}

Write-Host "Report: $reportPath"
if ($failures.Count -gt 0) {
    Write-Host "Phase 1 sync performance diagnostics:" -ForegroundColor Yellow
    $failures | ForEach-Object { Write-Host " - $_" }
    if ($Strict) { exit 1 }
}
else {
    Write-Host "Phase 1 sync performance smoke passed." -ForegroundColor Green
}
