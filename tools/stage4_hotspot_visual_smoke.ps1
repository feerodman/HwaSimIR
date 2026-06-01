param(
    [int[]]$Bands = @(1, 2, 3),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage4"
$summaryPath = Join-Path $logDir "stage4_hotspot_visual_smoke_summary.md"
$csvPath = Join-Path $logDir "stage4_hotspot_visual_smoke_summary.csv"

if ($Bands.Count -eq 1 -and $Bands[0] -eq 1) {
    $Bands = @(1, 2, 3)
}

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
    param([int]$Band)

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
        WInt 640
        WInt 512
        WInt 1
        WInt 50000
        WDouble 0.0
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

function New-ControlStartPacket {
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([int]0x41)
    $bw.Write([int]1)
    $bw.Write([int]1)
    $bw.Write([int]2)
    $bw.Write([int]1)
    $bw.Write([int]0)
    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-DisplayPacket {
    param(
        [double]$TimeMs,
        [bool]$EngineState,
        [bool]$StrikeFlag,
        [int]$StrikePart
    )

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
        WBool $StrikeFlag
        WInt $StrikePart
    }
    function WTargetState([int]$TargetType, [int]$TargetId, [bool]$TargetEngineState, [double]$LonOffset) {
        WInt $TargetType
        WInt 1
        WInt $TargetId
        WBool $TargetEngineState
        WBool $true
        WSpatial 0.0 $LonOffset 1000.0 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
    WWeaponState
    WInt 1
    WTargetState 0x22 0 $EngineState 0.010
    WTargetState 0x22 1 $false 0.015
    WTargetState 0x22 2 $false 0.020
    WTargetState 0x33 3 $false 0.025
    WTargetState 0x33 4 $false 0.030

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Set-Stage4DebugEnv {
    param(
        [bool]$VisualDebug,
        [bool]$ForceBright,
        [bool]$ForceRear
    )

    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", $(if ($VisualDebug) { "1" } else { "0" }), "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", $(if ($ForceBright) { "1" } else { "0" }), "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", $(if ($ForceRear) { "1" } else { "0" }), "Process")
}

function Invoke-HwaVisualRun {
    param(
        [hashtable]$Scenario
    )

    $safeName = ($Scenario.Label -replace '[^A-Za-z0-9_-]', '_')
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage4-visual-$safeName-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage4-visual-$safeName-$stamp.err.log"
    $process = $null
    $udp = $null

    try {
        Normalize-ProcessPathEnvironment
        Set-Stage4DebugEnv -VisualDebug $Scenario.VisualDebug -ForceBright $Scenario.ForceBright -ForceRear $Scenario.ForceRear
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        foreach ($band in $Bands) {
            $packet = New-InitPacket -Band $band
            [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds $DelayMs

            $control = New-ControlStartPacket
            [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds $DelayMs

            $display = New-DisplayPacket -TimeMs ([double](($band * 1000) + 33)) -EngineState $Scenario.Engine -StrikeFlag $Scenario.Strike -StrikePart $Scenario.Part
            [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds $DelayMs
        }
        Start-Sleep -Seconds 4
    }
    finally {
        if ($udp) {
            $udp.Close()
        }
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
            }
        }
        Set-Stage4DebugEnv -VisualDebug $false -ForceBright $false -ForceRear $false
    }

    $text = ""
    if (Test-Path -LiteralPath $stdout) {
        $text = Get-Content -LiteralPath $stdout -Raw -Encoding UTF8
    }

    return [PSCustomObject]@{
        Scenario = $Scenario
        Stdout   = $stdout
        Stderr   = $stderr
        Text     = $text
    }
}

function Test-ScenarioLog {
    param(
        [hashtable]$Scenario,
        [string]$Text
    )

    $inputPattern = "\[Stage4 Input\].*engineState=$([int]$Scenario.Engine).*strikeFlag=$([int]$Scenario.Strike).*strikePart=$($Scenario.Part)"
    $uniformOk = $Text -match "\[Stage4 Uniform\].*hotspotRearEn=" -and $Text -match "\[Stage4 Uniform\].*brightspotEn="
    $visualOk = (-not $Scenario.VisualDebug) -or ($Text -match "\[Stage4 VisualDebug\].*enabled=1")
    $forceBrightOk = (-not $Scenario.ForceBright) -or ($Text -match "\[Stage4 Uniform\].*brightspotEn=1")
    $forceRearOk = (-not $Scenario.ForceRear) -or ($Text -match "\[Stage4 Uniform\].*hotspotRearEn=1")
    $protocolHeadOk = (-not $Scenario.ExpectHead) -or ($Text -match "\[Stage4 BrightSpot\].*strikePart=1.*part=Head.*enabled=1")
    $protocolMidOk = (-not $Scenario.ExpectMid) -or ($Text -match "\[Stage4 BrightSpot\].*strikePart=2.*part=MidBody.*enabled=1")
    $protocolEngineOk = (-not $Scenario.ExpectEngine) -or ($Text -match "\[Stage4 ThermalHotspot\].*engineState=1.*enabled=1")

    return @{
        InputOk = ($Text -match $inputPattern)
        UniformOk = $uniformOk
        VisualDebugOk = $visualOk
        ForceBrightOk = $forceBrightOk
        ForceRearOk = $forceRearOk
        ProtocolHeadOk = $protocolHeadOk
        ProtocolMidOk = $protocolMidOk
        ProtocolEngineOk = $protocolEngineOk
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$scenarios = @(
    @{ Label = "baseline_no_signal"; Engine = $false; Strike = $false; Part = 0; VisualDebug = $false; ForceBright = $false; ForceRear = $false; ExpectHead = $false; ExpectMid = $false; ExpectEngine = $false },
    @{ Label = "debug_force_brightspot"; Engine = $false; Strike = $false; Part = 0; VisualDebug = $true; ForceBright = $true; ForceRear = $false; ExpectHead = $false; ExpectMid = $false; ExpectEngine = $false },
    @{ Label = "debug_force_rear_hotspot"; Engine = $false; Strike = $false; Part = 0; VisualDebug = $true; ForceBright = $false; ForceRear = $true; ExpectHead = $false; ExpectMid = $false; ExpectEngine = $false },
    @{ Label = "protocol_head_brightspot"; Engine = $false; Strike = $true; Part = 1; VisualDebug = $true; ForceBright = $false; ForceRear = $false; ExpectHead = $true; ExpectMid = $false; ExpectEngine = $false },
    @{ Label = "protocol_mid_brightspot"; Engine = $false; Strike = $true; Part = 2; VisualDebug = $true; ForceBright = $false; ForceRear = $false; ExpectHead = $false; ExpectMid = $true; ExpectEngine = $false },
    @{ Label = "protocol_engine_rear_hotspot"; Engine = $true; Strike = $false; Part = 0; VisualDebug = $true; ForceBright = $false; ForceRear = $false; ExpectHead = $false; ExpectMid = $false; ExpectEngine = $true }
)

Write-Host "Stage 4 hotspot/brightspot visual smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
foreach ($scenario in $scenarios) {
    Write-Host "running scenario: $($scenario.Label)"
    $run = Invoke-HwaVisualRun -Scenario $scenario
    $checks = Test-ScenarioLog -Scenario $scenario -Text $run.Text
    $status = if ($checks.Values -notcontains $false) { "PASS" } else { "FAIL" }

    $rows.Add([PSCustomObject]@{
        scenario = $scenario.Label
        status = $status
        input_log = $checks.InputOk
        uniform_log = $checks.UniformOk
        visual_debug_log = $checks.VisualDebugOk
        force_bright_ok = $checks.ForceBrightOk
        force_rear_ok = $checks.ForceRearOk
        protocol_head_ok = $checks.ProtocolHeadOk
        protocol_mid_ok = $checks.ProtocolMidOk
        protocol_engine_ok = $checks.ProtocolEngineOk
        mean_luma = "NA"
        max_luma = "NA"
        bright_pixel_count = "NA"
        target_crop_mean_luma = "NA"
        frame_metrics_note = "frame capture unavailable in this smoke; log wiring verified"
        stdout = $run.Stdout
        stderr = $run.Stderr
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage4 Hotspot/BrightSpot Visual Smoke Summary") | Out-Null
$md.Add("") | Out-Null
$md.Add("Bands: $($Bands -join ', ')") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | Input | Uniform | VisualDebug | Frame metrics | Log |") | Out-Null
$md.Add("|---|---|---:|---:|---:|---|---|") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.scenario) | $($row.status) | $($row.input_log) | $($row.uniform_log) | $($row.visual_debug_log) | $($row.frame_metrics_note) | $($row.stdout) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("Note: this script does not connect Stage 5 path/sky/solar radiance and does not rerun MODTRAN. It verifies Stage4 input, uniform, and debug-mask wiring through logs.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "Summary:"
Write-Host $summaryPath
Write-Host $csvPath

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage4 hotspot/brightspot visual smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage4 hotspot/brightspot visual smoke passed." -ForegroundColor Green
