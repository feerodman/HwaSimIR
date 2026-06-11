param(
    [int[]]$Bands = @(0, 1, 2, 3),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage5_body_visibility"
$summaryPath = Join-Path $logDir "stage5_body_visibility_summary.md"
$csvPath = Join-Path $logDir "stage5_body_visibility_metrics.csv"
$minBodyGray = 0.12

if ($Bands.Count -eq 1 -and $Bands[0] -eq 0) {
    $Bands = @(0, 1, 2, 3)
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

function Set-Stage5DebugEnv {
    param([hashtable]$Scenario)

    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugViewMode", $Scenario.ViewMode, "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugToneMap", "asinh", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugMinBodyGray", $minBodyGray.ToString([System.Globalization.CultureInfo]::InvariantCulture), "Process")
    [Environment]::SetEnvironmentVariable("Stage5UseBaseTextureModulation", "0", "Process")
    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", "0", "Process")
}

function Clear-Stage5DebugEnv {
    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugViewMode", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugToneMap", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugMinBodyGray", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5UseBaseTextureModulation", $null, "Process")
    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", "0", "Process")
}

function Invoke-HwaStage5BodyVisibilityRun {
    param([hashtable]$Scenario)

    $safeName = ($Scenario.Label -replace '[^A-Za-z0-9_-]', '_')
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage5-body-$safeName-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage5-body-$safeName-$stamp.err.log"
    $process = $null
    $udp = $null

    try {
        Normalize-ProcessPathEnvironment
        Set-Stage5DebugEnv -Scenario $Scenario
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -Band $Scenario.Band
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $control = New-ControlStartPacket
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $display = New-DisplayPacket -TimeMs ([double](($Scenario.Band * 1000) + 55)) -EngineState $Scenario.Engine -StrikeFlag $Scenario.Strike -StrikePart $Scenario.Part
        [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs
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
        Clear-Stage5DebugEnv
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

function Get-Stage5BodyVisibilityMetrics {
    param([string]$Text)

    $pattern = "\[Stage5 Radiance\].*debugViewMode=([A-Za-z]+).*toneMap=([A-Za-z]+).*bodyRadiance=([0-9eE+\-.]+).*hotspotRadiance=([0-9eE+\-.]+).*brightspotRadiance=([0-9eE+\-.]+).*bodyGrayBeforeFloor=([0-9eE+\-.]+).*bodyGrayAfterFloor=([0-9eE+\-.]+).*hotspotGray=([0-9eE+\-.]+).*brightspotGray=([0-9eE+\-.]+).*finalGrayDebug=([0-9eE+\-.]+).*debugFloorApplied=([01])"
    $matches = [regex]::Matches($Text, $pattern)
    if ($matches.Count -eq 0) {
        return $null
    }

    $viewMode = $matches[0].Groups[1].Value
    $toneMap = $matches[0].Groups[2].Value
    $bodyRadiance = 0.0
    $hotspotRadiance = 0.0
    $brightspotRadiance = 0.0
    $bodyGrayBeforeFloor = 0.0
    $bodyGrayAfterFloor = 0.0
    $hotspotGray = 0.0
    $brightspotGray = 0.0
    $finalGrayDebug = 0.0
    $debugFloorApplied = 0

    foreach ($m in $matches) {
        $bodyRadiance = [Math]::Max($bodyRadiance, [double]$m.Groups[3].Value)
        $hotspotRadiance = [Math]::Max($hotspotRadiance, [double]$m.Groups[4].Value)
        $brightspotRadiance = [Math]::Max($brightspotRadiance, [double]$m.Groups[5].Value)
        $bodyGrayBeforeFloor = [Math]::Max($bodyGrayBeforeFloor, [double]$m.Groups[6].Value)
        $bodyGrayAfterFloor = [Math]::Max($bodyGrayAfterFloor, [double]$m.Groups[7].Value)
        $hotspotGray = [Math]::Max($hotspotGray, [double]$m.Groups[8].Value)
        $brightspotGray = [Math]::Max($brightspotGray, [double]$m.Groups[9].Value)
        $finalGrayDebug = [Math]::Max($finalGrayDebug, [double]$m.Groups[10].Value)
        $debugFloorApplied = [Math]::Max($debugFloorApplied, [int]$m.Groups[11].Value)
    }

    return [PSCustomObject]@{
        debugViewMode       = $viewMode
        toneMap             = $toneMap
        bodyRadiance        = $bodyRadiance
        hotspotRadiance     = $hotspotRadiance
        brightspotRadiance  = $brightspotRadiance
        bodyGrayBeforeFloor = $bodyGrayBeforeFloor
        bodyGrayAfterFloor  = $bodyGrayAfterFloor
        hotspotGray         = $hotspotGray
        brightspotGray      = $brightspotGray
        finalGrayDebug      = $finalGrayDebug
        debugFloorApplied   = $debugFloorApplied
        samples             = $matches.Count
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$allScenarios = @(
    @{ Label = "mwir_bodyonly"; Band = 2; ViewMode = "BodyOnly"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "mwir_hotspotonly"; Band = 2; ViewMode = "HotspotOnly"; Engine = $true; Strike = $false; Part = 0; Expect = "hotspot" },
    @{ Label = "mwir_composite_engine_off"; Band = 2; ViewMode = "Composite"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "mwir_composite_engine_on"; Band = 2; ViewMode = "Composite"; Engine = $true; Strike = $false; Part = 0; Expect = "body_hotspot" },
    @{ Label = "lwir_bodyonly"; Band = 3; ViewMode = "BodyOnly"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "nir_brightspotonly"; Band = 1; ViewMode = "BrightSpotOnly"; Engine = $false; Strike = $true; Part = 1; Expect = "brightspot" },
    @{ Label = "swir_brightspotonly"; Band = 0; ViewMode = "BrightSpotOnly"; Engine = $false; Strike = $true; Part = 2; Expect = "brightspot" }
)
$scenarios = @($allScenarios | Where-Object { $Bands -contains $_.Band })

Write-Host "Stage 5 body visibility smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
foreach ($scenario in $scenarios) {
    Write-Host "running scenario: $($scenario.Label)"
    $run = Invoke-HwaStage5BodyVisibilityRun -Scenario $scenario
    $metrics = Get-Stage5BodyVisibilityMetrics -Text $run.Text
    $metricsPresent = $null -ne $metrics
    $status = "PASS"
    $reason = "log metrics verified"

    if (-not $metricsPresent) {
        $status = "FAIL"
        $reason = "missing Stage5 Radiance metrics"
    }
    elseif ($metrics.debugViewMode -ne $scenario.ViewMode) {
        $status = "FAIL"
        $reason = "debugViewMode mismatch"
    }
    elseif ($metrics.toneMap -ne "asinh") {
        $status = "FAIL"
        $reason = "toneMap mismatch"
    }
    elseif (($scenario.Expect -eq "body" -or $scenario.Expect -eq "body_hotspot") -and $metrics.bodyGrayAfterFloor -lt $minBodyGray) {
        $status = "FAIL"
        $reason = "bodyGrayAfterFloor below Stage5DebugMinBodyGray"
    }
    elseif ($metrics.finalGrayDebug -le 0.0) {
        $status = "FAIL"
        $reason = "finalGrayDebug is not positive"
    }
    elseif ($scenario.Expect -eq "hotspot" -and $metrics.hotspotGray -le 0.0) {
        $status = "FAIL"
        $reason = "hotspotGray is not positive"
    }
    elseif ($scenario.Expect -eq "body_hotspot" -and $metrics.hotspotGray -le 0.0) {
        $status = "FAIL"
        $reason = "Composite engine on did not produce hotspotGray"
    }
    elseif ($scenario.Expect -eq "brightspot" -and $metrics.brightspotGray -le 0.0) {
        $status = "FAIL"
        $reason = "brightspotGray is not positive"
    }
    elseif ($scenario.ViewMode -eq "BodyOnly" -and ($scenario.Engine -or $scenario.Strike)) {
        $status = "FAIL"
        $reason = "BodyOnly isolation scenario must run with hotspot and brightspot off"
    }

    $rows.Add([PSCustomObject]@{
        scenario = $scenario.Label
        band = $scenario.Band
        view_mode = $scenario.ViewMode
        status = $status
        reason = $reason
        bodyRadiance = $(if ($metricsPresent) { $metrics.bodyRadiance } else { "NA" })
        hotspotRadiance = $(if ($metricsPresent) { $metrics.hotspotRadiance } else { "NA" })
        brightspotRadiance = $(if ($metricsPresent) { $metrics.brightspotRadiance } else { "NA" })
        bodyGrayBeforeFloor = $(if ($metricsPresent) { $metrics.bodyGrayBeforeFloor } else { "NA" })
        bodyGrayAfterFloor = $(if ($metricsPresent) { $metrics.bodyGrayAfterFloor } else { "NA" })
        hotspotGray = $(if ($metricsPresent) { $metrics.hotspotGray } else { "NA" })
        brightspotGray = $(if ($metricsPresent) { $metrics.brightspotGray } else { "NA" })
        finalGrayDebug = $(if ($metricsPresent) { $metrics.finalGrayDebug } else { "NA" })
        debugFloorApplied = $(if ($metricsPresent) { $metrics.debugFloorApplied } else { "NA" })
        mean_luma = "NA"
        max_luma = "NA"
        bright_pixel_count = "NA"
        frame_metrics_note = "frame metrics unavailable; log metrics verified"
        stdout = $run.Stdout
        stderr = $run.Stderr
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage5 Body Visibility Smoke Summary") | Out-Null
$md.Add("") | Out-Null
$md.Add("Bands: $($Bands -join ', ')") | Out-Null
$md.Add("Stage5DebugToneMap=asinh; Stage5DebugMinBodyGray=$minBodyGray; Stage5UseBaseTextureModulation=0.") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | Band | View | Body gray | Hotspot gray | Brightspot gray | Final gray | Reason | Log |") | Out-Null
$md.Add("|---|---|---:|---|---:|---:|---:|---:|---|---|") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.scenario) | $($row.status) | $($row.band) | $($row.view_mode) | $($row.bodyGrayAfterFloor) | $($row.hotspotGray) | $($row.brightspotGray) | $($row.finalGrayDebug) | $($row.reason) | $($row.stdout) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("Note: frame/JPEG luma metrics are marked NA when no output frame is available; this smoke validates the Stage5 log metrics and debug view mode wiring.") | Out-Null
$md.Add("Boundary: this is debug tone mapping only, not final AGC/noise/MTF calibration.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "Summary:"
Write-Host $summaryPath
Write-Host $csvPath

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 body visibility smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage5 body visibility smoke passed." -ForegroundColor Green
