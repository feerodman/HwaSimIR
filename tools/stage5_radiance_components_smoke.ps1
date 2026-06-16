param(
    [int[]]$Bands = @(0, 2, 3),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$networkConfig = Join-Path $hwaWorkDir "Config\NetworkConfig.ini"
$logDir = Join-Path $rootPath "logs\stage5"
$summaryPath = Join-Path $logDir "stage5_radiance_components_smoke_summary.md"
$csvPath = Join-Path $logDir "stage5_radiance_components_smoke_summary.csv"

if ($Bands.Count -eq 1 -and $Bands[0] -eq 0) {
    $Bands = @(0, 2, 3)
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

function Set-Stage5ComponentsEnv {
    [Environment]::SetEnvironmentVariable("EnableIRPhysicalPipeline", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5RadianceDebugView", "Off", "Process")
    [Environment]::SetEnvironmentVariable("Stage5RadianceLogComponents", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5RadianceComponentLogEveryFrames", "1", "Process")
    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugViewMode", "Off", "Process")
    [Environment]::SetEnvironmentVariable("EnableIRVerboseLog", "0", "Process")
    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("LegacyEngineBodyHeating", "0", "Process")
}

function Clear-Stage5ComponentsEnv {
    foreach ($name in @(
        "EnableIRPhysicalPipeline",
        "Stage5RadianceDebugView",
        "Stage5RadianceLogComponents",
        "Stage5RadianceComponentLogEveryFrames",
        "EnableStage5RadianceDebug",
        "Stage5DebugViewMode",
        "EnableIRVerboseLog",
        "EnableStage4HotspotVisualDebug",
        "ForceStage4BrightSpotVisible",
        "ForceStage4RearHotspotVisible",
        "LegacyEngineBodyHeating")) {
        [Environment]::SetEnvironmentVariable($name, $null, "Process")
    }
}

function Invoke-HwaStage5ComponentsRun {
    param([hashtable]$Scenario)

    $safeName = ($Scenario.Label -replace '[^A-Za-z0-9_-]', '_')
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage5-components-$safeName-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage5-components-$safeName-$stamp.err.log"
    $process = $null
    $udp = $null
    $networkConfigBackup = [System.IO.File]::ReadAllBytes($networkConfig)

    try {
        Normalize-ProcessPathEnvironment
        Set-Stage5ComponentsEnv
        $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
        $networkText = [System.IO.File]::ReadAllText($networkConfig, $utf8NoBom)
        $networkText = [regex]::Replace($networkText, "(?m)^localIp=.*$", "localIp=127.0.0.1")
        $networkText = [regex]::Replace($networkText, "(?m)^remoteIp=.*$", "remoteIp=127.0.0.1")
        $networkText = [regex]::Replace($networkText, "(?m)^serverIp=.*$", "serverIp=127.0.0.1")
        [System.IO.File]::WriteAllText($networkConfig, $networkText, $utf8NoBom)
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -Band $Scenario.Band
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $control = New-ControlStartPacket
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 2000))

        $display1 = New-DisplayPacket -TimeMs ([double](($Scenario.Band * 1000) + 33)) -EngineState $Scenario.Engine -StrikeFlag $Scenario.Strike -StrikePart $Scenario.Part
        [void]$udp.Send($display1, $display1.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $display2 = New-DisplayPacket -TimeMs ([double](($Scenario.Band * 1000) + 66)) -EngineState $Scenario.Engine -StrikeFlag $Scenario.Strike -StrikePart $Scenario.Part
        [void]$udp.Send($display2, $display2.Length, "127.0.0.1", 8888)
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
        [System.IO.File]::WriteAllBytes($networkConfig, $networkConfigBackup)
        Clear-Stage5ComponentsEnv
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

function Get-Stage5ComponentMetrics {
    param([string]$Text)

    $pattern = "(?m)^\[Stage5 RadianceComponents\].*?\btargetID=(?<targetID>-?\d+).*?\bband=(?<band>\w+).*?\bbodyRadiance=(?<body>[-+0-9eE.]+).*?\breflectedRadiance=(?<reflected>[-+0-9eE.]+).*?\brearHotspotRadiance=(?<rear>[-+0-9eE.]+).*?\bplumeRadiance=(?<plume>[-+0-9eE.]+).*?\bbrightspotRadiance=(?<bright>[-+0-9eE.]+).*?\btauUp=(?<tau>[-+0-9eE.]+).*?\bpathRadiance=(?<path>[-+0-9eE.]+).*?\bpathRadianceSource=(?<pathSource>\S+).*?\bsensorInputRadiance=(?<sensor>[-+0-9eE.]+).*?\bdisplayPreview=(?<display>[-+0-9eE.]+).*?\bsourceFlags=(?<flags>\S+).*?\bDebugView=(?<debug>\S+)"
    $matches = @([regex]::Matches($Text, $pattern) | Where-Object { [int]$_.Groups["targetID"].Value -eq 0 })
    if ($matches.Count -eq 0) {
        return $null
    }

    $best = $matches[-1]
    return [PSCustomObject]@{
        band = $best.Groups["band"].Value
        bodyRadiance = [double]$best.Groups["body"].Value
        reflectedRadiance = [double]$best.Groups["reflected"].Value
        rearHotspotRadiance = [double]$best.Groups["rear"].Value
        plumeRadiance = [double]$best.Groups["plume"].Value
        brightspotRadiance = [double]$best.Groups["bright"].Value
        tauUp = [double]$best.Groups["tau"].Value
        pathRadiance = [double]$best.Groups["path"].Value
        pathRadianceSource = $best.Groups["pathSource"].Value
        sensorInputRadiance = [double]$best.Groups["sensor"].Value
        displayPreview = [double]$best.Groups["display"].Value
        sourceFlags = $best.Groups["flags"].Value
        debugView = $best.Groups["debug"].Value
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
Assert-Path $networkConfig "HwaSimIR network config"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$allScenarios = @(
    @{ Label = "swir_baseline"; Band = 0; Engine = $false; Strike = $false; Part = 0 },
    @{ Label = "mwir_baseline"; Band = 2; Engine = $false; Strike = $false; Part = 0 },
    @{ Label = "mwir_engine"; Band = 2; Engine = $true; Strike = $false; Part = 0 },
    @{ Label = "mwir_strike_head"; Band = 2; Engine = $false; Strike = $true; Part = 1 },
    @{ Label = "mwir_strike_mid"; Band = 2; Engine = $false; Strike = $true; Part = 2 },
    @{ Label = "lwir_baseline"; Band = 3; Engine = $false; Strike = $false; Part = 0 }
)
$scenarios = @($allScenarios | Where-Object { ($Bands -contains $_.Band) -or ($_.Label -match "^mwir_") })

Write-Host "Stage 5 radiance components smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
foreach ($scenario in $scenarios) {
    Write-Host "running scenario: $($scenario.Label)"
    $run = Invoke-HwaStage5ComponentsRun -Scenario $scenario
    $metrics = Get-Stage5ComponentMetrics -Text $run.Text
    $metricsPresent = $null -ne $metrics
    $pathSourceOk = $metricsPresent -and ($metrics.pathRadianceSource -in @("legacy_empirical", "disabled"))
    $debugViewOk = $metricsPresent -and ($metrics.debugView -eq "Off")
    $bodyOk = $metricsPresent -and ($metrics.bodyRadiance -gt 0.0)
    $status = if ($metricsPresent -and $pathSourceOk -and $debugViewOk -and $bodyOk) { "PASS" } else { "FAIL" }

    $rows.Add([PSCustomObject]@{
        scenario = $scenario.Label
        band = $scenario.Band
        status = $status
        bodyRadiance = $(if ($metricsPresent) { $metrics.bodyRadiance } else { "NA" })
        reflectedRadiance = $(if ($metricsPresent) { $metrics.reflectedRadiance } else { "NA" })
        rearHotspotRadiance = $(if ($metricsPresent) { $metrics.rearHotspotRadiance } else { "NA" })
        plumeRadiance = $(if ($metricsPresent) { $metrics.plumeRadiance } else { "NA" })
        brightspotRadiance = $(if ($metricsPresent) { $metrics.brightspotRadiance } else { "NA" })
        tauUp = $(if ($metricsPresent) { $metrics.tauUp } else { "NA" })
        pathRadiance = $(if ($metricsPresent) { $metrics.pathRadiance } else { "NA" })
        pathRadianceSource = $(if ($metricsPresent) { $metrics.pathRadianceSource } else { "NA" })
        sensorInputRadiance = $(if ($metricsPresent) { $metrics.sensorInputRadiance } else { "NA" })
        displayPreview = $(if ($metricsPresent) { $metrics.displayPreview } else { "NA" })
        sourceFlags = $(if ($metricsPresent) { $metrics.sourceFlags } else { "NA" })
        debugView = $(if ($metricsPresent) { $metrics.debugView } else { "NA" })
        stdout = $run.Stdout
        stderr = $run.Stderr
    }) | Out-Null
}

$baseline = @($rows | Where-Object { $_.scenario -eq "mwir_baseline" }) | Select-Object -First 1
$engine = @($rows | Where-Object { $_.scenario -eq "mwir_engine" }) | Select-Object -First 1
$head = @($rows | Where-Object { $_.scenario -eq "mwir_strike_head" }) | Select-Object -First 1
$mid = @($rows | Where-Object { $_.scenario -eq "mwir_strike_mid" }) | Select-Object -First 1

if ($baseline -and $engine -and $baseline.bodyRadiance -ne "NA" -and $engine.bodyRadiance -ne "NA") {
    $bodyDelta = [Math]::Abs([double]$engine.bodyRadiance - [double]$baseline.bodyRadiance)
    $bodyScale = [Math]::Max(1.0e-9, [Math]::Abs([double]$baseline.bodyRadiance))
    if (($bodyDelta / $bodyScale) -gt 0.01) {
        $engine.status = "FAIL"
    }
    if ([double]$engine.rearHotspotRadiance -le [double]$baseline.rearHotspotRadiance) {
        $engine.status = "FAIL"
    }
    if ([double]$engine.plumeRadiance -le [double]$baseline.plumeRadiance) {
        $engine.status = "FAIL"
    }
}
if ($baseline -and $head -and $head.brightspotRadiance -ne "NA") {
    if ([double]$head.brightspotRadiance -le [double]$baseline.brightspotRadiance) {
        $head.status = "FAIL"
    }
}
if ($baseline -and $mid -and $mid.brightspotRadiance -ne "NA") {
    if ([double]$mid.brightspotRadiance -le [double]$baseline.brightspotRadiance) {
        $mid.status = "FAIL"
    }
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage5 Radiance Components Smoke Summary") | Out-Null
$md.Add("") | Out-Null
$md.Add("Bands: $($Bands -join ', ')") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | Band | Body | Reflected | Rear | Plume | BrightSpot | TauUp | Path | Path source | Sensor input | Display preview | DebugView | Log |") | Out-Null
$md.Add("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---|---|") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.scenario) | $($row.status) | $($row.band) | $($row.bodyRadiance) | $($row.reflectedRadiance) | $($row.rearHotspotRadiance) | $($row.plumeRadiance) | $($row.brightspotRadiance) | $($row.tauUp) | $($row.pathRadiance) | $($row.pathRadianceSource) | $($row.sensorInputRadiance) | $($row.displayPreview) | $($row.debugView) | $($row.stdout) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("Checks: engineState must not move bodyRadiance >1%; rear/plume must rise with engineState; brightspot must rise with strikeFlag/strikePart; pathRadianceSource must be legacy_empirical or disabled, not MODTRAN runtime.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "Summary:"
Write-Host $summaryPath
Write-Host $csvPath

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 radiance components smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage5 radiance components smoke passed." -ForegroundColor Green
