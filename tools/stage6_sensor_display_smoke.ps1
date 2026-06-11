param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage6_sensor_display"
$summaryPath = Join-Path $logDir "stage6_sensor_display_summary.md"
$csvPath = Join-Path $logDir "stage6_sensor_display_metrics.csv"

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
    param([int]$Band = 2)

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
    function WSensor([int]$sensorBand) {
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
        WInt $sensorBand
        WInt 800
        WInt 800
        WInt 50
        WInt 30000
        WDouble 10.0
        for ($i = 0; $i -lt 32; $i++) {
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
    WInt 30
    WSensor $Band

    WInt 3
    WInt 2
    WInt 0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-ControlStopPacket {
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([int]0x41)
    $bw.Write([int]1)
    $bw.Write([int]1)
    $bw.Write([int]3)
    $bw.Write([int]1)
    $bw.Write([int]0)
    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Stop-HwaProcessGracefully {
    param(
        [System.Diagnostics.Process]$Process,
        [System.Net.Sockets.UdpClient]$Udp
    )

    if (-not $Process) {
        return
    }
    $Process.Refresh()
    if ($Process.HasExited) {
        return
    }

    if ($Udp) {
        try {
            $stopPacket = New-ControlStopPacket
            [void]$Udp.Send($stopPacket, $stopPacket.Length, "127.0.0.1", 8888)
            Write-Host "sent graceful stop pid=$($Process.Id)"
        }
        catch {
            Write-Warning "Failed to send graceful stop packet: $($_.Exception.Message)"
        }
    }

    if ($Process.WaitForExit(8000)) {
        Write-Host "process exited gracefully pid=$($Process.Id)"
        return
    }

    try {
        [void]$Process.CloseMainWindow()
    }
    catch {
        Write-Warning "CloseMainWindow failed for pid=$($Process.Id): $($_.Exception.Message)"
    }

    if ($Process.WaitForExit(5000)) {
        Write-Host "process closed after window close pid=$($Process.Id)"
        return
    }

    Write-Warning "HwaSimIR did not exit after graceful stop; forcing pid=$($Process.Id)"
    Stop-Process -Id $Process.Id -Force
    $Process.WaitForExit()
}

function Clear-Stage6DisplayEnvironment {
    foreach ($name in @("Stage6WhiteHot", "Stage6DisplayGain", "Stage6DisplayOffset", "Stage6NoiseEnable", "Stage6NoiseSigmaGray", "Stage6DisplayApplyToWindow", "Stage6BackgroundDisplayEnable", "EnableStage5RadianceDebug", "HwaSimIRExitOnStop")) {
        [Environment]::SetEnvironmentVariable($name, $null, "Process")
    }
}

function Set-ScenarioEnvironment {
    param([hashtable]$Values)
    Clear-Stage6DisplayEnvironment
    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("HwaSimIRExitOnStop", "1", "Process")
    foreach ($key in $Values.Keys) {
        [Environment]::SetEnvironmentVariable($key, [string]$Values[$key], "Process")
    }
}

function Invoke-HwaStage6DisplayRun {
    param([PSCustomObject]$Scenario)

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage6-display-$($Scenario.Label)-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage6-display-$($Scenario.Label)-$stamp.err.log"
    $process = $null
    $udp = $null

    try {
        Normalize-ProcessPathEnvironment
        Set-ScenarioEnvironment -Values $Scenario.Env
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket
        $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Write-Host "sent init $($Scenario.Label) bytes=$sent"
        Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 3000))
    }
    finally {
        if ($process -and -not $process.HasExited) {
            Stop-HwaProcessGracefully -Process $process -Udp $udp
        }
        if ($udp) {
            $udp.Dispose()
        }
        Clear-Stage6DisplayEnvironment
    }

    return [PSCustomObject]@{
        Stdout = $stdout
        Stderr = $stderr
        Text   = if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" }
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$scenarios = @(
    [PSCustomObject]@{
        Label = "whitehot"
        Env = @{ Stage6WhiteHot = "1" }
        Expected = @{ effectiveWhiteHot = "1"; configSource = "env" }
    },
    [PSCustomObject]@{
        Label = "blackhot"
        Env = @{ Stage6WhiteHot = "0" }
        Expected = @{ effectiveWhiteHot = "0"; configSource = "env" }
    },
    [PSCustomObject]@{
        Label = "gain18"
        Env = @{ Stage6DisplayGain = "1.8" }
        Expected = @{ displayGain = "1.8"; configSource = "env" }
    },
    [PSCustomObject]@{
        Label = "offset30"
        Env = @{ Stage6DisplayOffset = "30" }
        Expected = @{ displayOffset = "30"; configSource = "env" }
    },
    [PSCustomObject]@{
        Label = "noise25"
        Env = @{ Stage6NoiseEnable = "1"; Stage6NoiseSigmaGray = "25" }
        Expected = @{ effectiveNoiseEnable = "1"; effectiveNoiseSigmaGray = "25"; configSource = "env" }
    }
)

Write-Host "HwaSimIR: $hwaExe"
Write-Host "Logs: $logDir"

$rows = New-Object System.Collections.Generic.List[object]

foreach ($scenario in $scenarios) {
    $run = Invoke-HwaStage6DisplayRun -Scenario $scenario
    $line = ($run.Text -split "\r?\n" | Where-Object { $_ -match "\[Stage6 Display\]" } | Select-Object -First 1)
    $pipelineLine = ($run.Text -split "\r?\n" | Where-Object { $_ -match "\[Stage6 FinalPipeline\]" } | Select-Object -First 1)
    if ($null -eq $line) {
        $line = ""
    }
    if ($null -eq $pipelineLine) {
        $pipelineLine = ""
    }
    $expectedOk = [bool]$line -and [bool]$pipelineLine
    foreach ($key in $scenario.Expected.Keys) {
        $value = [regex]::Escape([string]$scenario.Expected[$key])
        if ($line -notmatch "$key=$value\b") {
            $expectedOk = $false
        }
    }
    if ($pipelineLine -notmatch "sameOutput=1\b" -or
        $pipelineLine -notmatch "windowSource=final_sensor\b" -or
        $pipelineLine -notmatch "tcpSource=final_sensor\b") {
        $expectedOk = $false
    }

    $rows.Add([PSCustomObject]@{
        scenario    = $scenario.Label
        status      = $(if ($expectedOk) { "PASS" } else { "FAIL" })
        displayLog  = [bool]$line
        finalPipelineLog = [bool]$pipelineLine
        stdout      = $run.Stdout
        stderr      = $run.Stderr
        matchedLine = $line
        matchedPipeline = $pipelineLine
    }) | Out-Null
}

$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage6 Sensor Display Smoke") | Out-Null
$md.Add("") | Out-Null
$md.Add("This smoke validates Stage6B.3 final sensor pipeline logs only. It does not parse JPEG frames or perform visual metrics.") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | DisplayLog | FinalPipelineLog |") | Out-Null
$md.Add("| --- | --- | --- | --- |") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.scenario) | $($row.status) | $($row.displayLog) | $($row.finalPipelineLog) |") | Out-Null
}
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage6 sensor display smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage6 sensor display smoke passed." -ForegroundColor Green
