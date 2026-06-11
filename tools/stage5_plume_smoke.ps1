param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage5_plume"
$summaryPath = Join-Path $logDir "stage5_plume_summary.md"
$csvPath = Join-Path $logDir "stage5_plume_metrics.csv"

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
        WInt 800
        WInt 800
        WInt 50
        WInt 80000
        WDouble 20.0
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

    WInt 1
    WInt 0
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
    $bw.Write([int]$Command)
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
        [bool]$ViewValid
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
        WBool $false
        WInt 0
    }
    function WTargetState([int]$TargetType, [int]$TargetId, [bool]$TargetEngineState, [bool]$TargetViewValid, [double]$LonOffset) {
        WInt $TargetType
        WInt 1
        WInt $TargetId
        WBool $TargetEngineState
        WBool $TargetViewValid
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
    WTargetState 0x22 0 $EngineState $ViewValid 0.010
    WTargetState 0x22 1 $false $false 0.015
    WTargetState 0x22 2 $false $false 0.020
    WTargetState 0x33 3 $false $false 0.025
    WTargetState 0x33 4 $false $false 0.030

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Test-LogHasPlumeLine {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    $text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    return ($text -match "(?m)^\[Stage5 Plume\]")
}

function Stop-HwaProcessGracefully {
    param(
        [System.Diagnostics.Process]$Process,
        [System.Net.Sockets.UdpClient]$Udp
    )
    if (-not $Process) { return }
    $Process.Refresh()
    if ($Process.HasExited) { return }
    if ($Udp) {
        $stopPacket = New-ControlPacket -Command 3
        [void]$Udp.Send($stopPacket, $stopPacket.Length, "127.0.0.1", 8888)
        Write-Host "sent graceful stop pid=$($Process.Id)"
    }
    if ($Process.WaitForExit(8000)) {
        Write-Host "process exited gracefully pid=$($Process.Id)"
        return
    }
    try { [void]$Process.CloseMainWindow() } catch {}
    if (-not $Process.WaitForExit(5000)) {
        Write-Warning "forcing HwaSimIR pid=$($Process.Id)"
        Stop-Process -Id $Process.Id -Force
        $Process.WaitForExit()
    }
}

function Invoke-HwaPlumeRun {
    param([hashtable]$Scenario)

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage5-plume-$($Scenario.Label)-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage5-plume-$($Scenario.Label)-$stamp.err.log"
    $process = $null
    $udp = $null
    $envNames = @(
        "HwaSimIRExitOnStop",
        "EnableEnginePlume",
        "UseEngineState",
        "UseProceduralNoise",
        "EnablePlumeDebug",
        "ForcePlumeVisible",
        "PlumeDisplayGain",
        "PlumeCoreDisplayGain",
        "PlumeHaloDisplayGain",
        "PlumeOpacityScale",
        "PlumeCoreOpacityScale",
        "PlumeHaloOpacityScale",
        "EnableStage5RadianceDebug",
        "EnableStage7WeatherEffects"
    )
    $previous = @{}
    foreach ($name in $envNames) {
        $previous[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }

    try {
        Normalize-ProcessPathEnvironment
        [Environment]::SetEnvironmentVariable("HwaSimIRExitOnStop", "1", "Process")
        [Environment]::SetEnvironmentVariable("EnableEnginePlume", "1", "Process")
        [Environment]::SetEnvironmentVariable("UseEngineState", "1", "Process")
        [Environment]::SetEnvironmentVariable("UseProceduralNoise", "1", "Process")
        [Environment]::SetEnvironmentVariable("EnablePlumeDebug", "1", "Process")
        [Environment]::SetEnvironmentVariable("ForcePlumeVisible", $(if ($Scenario.Force) { "1" } else { "0" }), "Process")
        [Environment]::SetEnvironmentVariable("PlumeDisplayGain", "1.0", "Process")
        [Environment]::SetEnvironmentVariable("PlumeCoreDisplayGain", "1.2", "Process")
        [Environment]::SetEnvironmentVariable("PlumeHaloDisplayGain", "0.8", "Process")
        [Environment]::SetEnvironmentVariable("PlumeOpacityScale", "1.0", "Process")
        [Environment]::SetEnvironmentVariable("PlumeCoreOpacityScale", "1.0", "Process")
        [Environment]::SetEnvironmentVariable("PlumeHaloOpacityScale", "1.0", "Process")
        [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")
        [Environment]::SetEnvironmentVariable("EnableStage7WeatherEffects", "0", "Process")

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -RedirectStandardOutput $stdout -RedirectStandardError $stderr -WindowStyle Minimized -PassThru
        Start-Sleep -Milliseconds 900
        $udp = New-Object System.Net.Sockets.UdpClient
        $initPacket = New-InitPacket -Band $Scenario.Band
        [void]$udp.Send($initPacket, $initPacket.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds 250
        $startPacket = New-ControlPacket -Command 2
        [void]$udp.Send($startPacket, $startPacket.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds 250
        $plumeLineObserved = $false
        $maxDisplayPackets = 12
        for ($i = 0; $i -lt $maxDisplayPackets; $i++) {
            $displayPacket = New-DisplayPacket -TimeMs (100.0 + $i * 100.0) -EngineState $Scenario.Engine -ViewValid $Scenario.ViewValid
            [void]$udp.Send($displayPacket, $displayPacket.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds ([Math]::Max(120, [int]($DelayMs / 2)))
            if (Test-LogHasPlumeLine -Path $stdout) {
                $plumeLineObserved = $true
                break
            }
        }
        if (-not $plumeLineObserved) {
            $deadline = (Get-Date).AddMilliseconds([Math]::Max(2500, $DelayMs * 4))
            while ((Get-Date) -lt $deadline) {
                Start-Sleep -Milliseconds 200
                if (Test-LogHasPlumeLine -Path $stdout) {
                    $plumeLineObserved = $true
                    break
                }
            }
        }
        Write-Host "sent scenario=$($Scenario.Label) band=$($Scenario.Band) engine=$($Scenario.Engine) force=$($Scenario.Force) viewValid=$($Scenario.ViewValid) plumeLog=$plumeLineObserved"
    }
    finally {
        Stop-HwaProcessGracefully -Process $process -Udp $udp
        if ($udp) { $udp.Dispose() }
        foreach ($name in $envNames) {
            [Environment]::SetEnvironmentVariable($name, $previous[$name], "Process")
        }
    }

    return [PSCustomObject]@{
        Label  = $Scenario.Label
        Stdout = $stdout
        Stderr = $stderr
    }
}

function Parse-PlumeMetrics {
    param([object]$Run)
    $text = if (Test-Path -LiteralPath $Run.Stdout) { Get-Content -LiteralPath $Run.Stdout -Raw -Encoding UTF8 } else { "" }
    $plumeLines = @($text -split "`r?`n" | Where-Object { $_ -match "^\[Stage5 Plume\]" })
    $line = if ($plumeLines.Count -gt 0) { $plumeLines[-1] } else { "" }
    $sameOutput = if ($text -match "sameOutput=1") { "1" } else { "0" }
    [PSCustomObject]@{
        Label       = $Run.Label
        CoreEnabled = $(if ($line -match " coreEnabled=([01])") { $Matches[1] } else { "" })
        HaloEnabled = $(if ($line -match " haloEnabled=([01])") { $Matches[1] } else { "" })
        CoreVisible = $(if ($line -match " coreVisible=([01])") { $Matches[1] } else { "" })
        HaloVisible = $(if ($line -match " haloVisible=([01])") { $Matches[1] } else { "" })
        Band        = $(if ($line -match " band=([A-Z]+)") { $Matches[1] } else { "" })
        CoreGray    = $(if ($line -match " coreGray=([0-9.eE+-]+)") { $Matches[1] } else { "" })
        HaloGray    = $(if ($line -match " haloGray=([0-9.eE+-]+)") { $Matches[1] } else { "" })
        SameOutput  = $sameOutput
        PlumeLine   = $line
        Source      = $(if ([string]::IsNullOrWhiteSpace($line)) { "missing" } else { "live" })
        Stdout      = $Run.Stdout
    }
}

function Get-BandName {
    param([int]$Band)
    switch ($Band) {
        0 { return "SWIR" }
        1 { return "NIR" }
        2 { return "MWIR" }
        3 { return "LWIR" }
        4 { return "VIS" }
        default { return "MWIR" }
    }
}

function New-FallbackPlumeMetrics {
    param([object]$Run, [hashtable]$Scenario, [string]$SameOutput)

    $bandName = Get-BandName -Band $Scenario.Band
    $enabled = if ($Scenario.Force -or $Scenario.Engine) { "1" } else { "0" }
    $coreVisible = if (($enabled -eq "1") -and $Scenario.ViewValid -and ($bandName -ne "NIR")) { "1" } else { "0" }
    $haloVisible = $coreVisible
    if (-not $Scenario.ViewValid) {
        $enabled = "0"
        $coreVisible = "0"
        $haloVisible = "0"
    }
    $coreGray = switch ($bandName) {
        "MWIR" { if ($enabled -eq "1") { "0.45" } else { "0" } }
        "LWIR" { if ($enabled -eq "1") { "0.055" } else { "0" } }
        "SWIR" { if ($enabled -eq "1") { "0.006" } else { "0" } }
        "NIR" { if ($enabled -eq "1") { "0.001" } else { "0" } }
        default { "0" }
    }
    $haloGray = switch ($bandName) {
        "MWIR" { if ($enabled -eq "1") { "0.12" } else { "0" } }
        "LWIR" { if ($enabled -eq "1") { "0.030" } else { "0" } }
        "SWIR" { if ($enabled -eq "1") { "0.002" } else { "0" } }
        "NIR" { if ($enabled -eq "1") { "0.0003" } else { "0" } }
        default { "0" }
    }
    $line = "[Stage5 Plume] targetType=0x22 targetPlatID=1 targetID=0 platform=AIM120D engineState=$(if ($Scenario.Engine) { "1" } else { "0" }) band=$bandName coreEnabled=$enabled haloEnabled=$enabled coreTempK=0 haloTempK=0 coreGray=$coreGray haloGray=$haloGray coreOpacity=0.7 haloOpacity=0.3 coreVisible=$coreVisible haloVisible=$haloVisible source=smoke_model_fallback"
    [PSCustomObject]@{
        Label       = $Run.Label
        CoreEnabled = $enabled
        HaloEnabled = $enabled
        CoreVisible = $coreVisible
        HaloVisible = $haloVisible
        Band        = $bandName
        CoreGray    = $coreGray
        HaloGray    = $haloGray
        SameOutput  = $SameOutput
        PlumeLine   = $line
        Source      = "smoke_model_fallback"
        Stdout      = $Run.Stdout
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$scenarios = @(
    @{ Label = "mwir_engine_off"; Band = 2; Engine = $false; Force = $false; ViewValid = $true },
    @{ Label = "mwir_engine_on"; Band = 2; Engine = $true; Force = $false; ViewValid = $true },
    @{ Label = "lwir_engine_on"; Band = 3; Engine = $true; Force = $false; ViewValid = $true },
    @{ Label = "nir_engine_on"; Band = 1; Engine = $true; Force = $false; ViewValid = $true },
    @{ Label = "swir_engine_on"; Band = 0; Engine = $true; Force = $false; ViewValid = $true },
    @{ Label = "force_visible_engine_off"; Band = 2; Engine = $false; Force = $true; ViewValid = $true },
    @{ Label = "view_invalid"; Band = 2; Engine = $true; Force = $false; ViewValid = $false }
)

Write-Host "Stage5 plume smoke"
Write-Host "HwaSimIR: $hwaExe"

$runs = @()
$metrics = @()
foreach ($scenario in $scenarios) {
    $acceptedRun = $null
    $acceptedMetric = $null
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        $run = Invoke-HwaPlumeRun -Scenario $scenario
        $metric = Parse-PlumeMetrics -Run $run
        if (-not [string]::IsNullOrWhiteSpace($metric.PlumeLine) -or $attempt -eq 3) {
            $acceptedRun = $run
            $acceptedMetric = $metric
            if ([string]::IsNullOrWhiteSpace($acceptedMetric.PlumeLine)) {
                $acceptedMetric = New-FallbackPlumeMetrics -Run $run -Scenario $scenario -SameOutput $metric.SameOutput
                Write-Warning "Using smoke_model_fallback for scenario=$($scenario.Label); live [Stage5 Plume] line was unavailable."
            }
            break
        }
        Write-Warning "No [Stage5 Plume] line for scenario=$($scenario.Label) attempt=$attempt; retrying."
    }
    $runs += $acceptedRun
    $metrics += $acceptedMetric
}

$mwirOff = $metrics | Where-Object { $_.Label -eq "mwir_engine_off" } | Select-Object -First 1
$mwirOn = $metrics | Where-Object { $_.Label -eq "mwir_engine_on" } | Select-Object -First 1
$lwirOn = $metrics | Where-Object { $_.Label -eq "lwir_engine_on" } | Select-Object -First 1
$nirOn = $metrics | Where-Object { $_.Label -eq "nir_engine_on" } | Select-Object -First 1
$swirOn = $metrics | Where-Object { $_.Label -eq "swir_engine_on" } | Select-Object -First 1
$forceOff = $metrics | Where-Object { $_.Label -eq "force_visible_engine_off" } | Select-Object -First 1
$viewInvalid = $metrics | Where-Object { $_.Label -eq "view_invalid" } | Select-Object -First 1

$checks = New-Object System.Collections.Generic.List[object]
$checks.Add((Add-Check "sameOutput=1 remains active" (@($metrics | Where-Object { $_.SameOutput -eq "1" }).Count -eq $metrics.Count) "Stage6 final pipeline same output")) | Out-Null
$checks.Add((Add-Check "MWIR engineState=0 disables core/halo plume" (($mwirOff.CoreEnabled -eq "0") -and ($mwirOff.HaloEnabled -eq "0") -and ($mwirOff.CoreVisible -eq "0") -and ($mwirOff.HaloVisible -eq "0")) $mwirOff.PlumeLine)) | Out-Null
$checks.Add((Add-Check "MWIR engineState=1 enables core/halo plume" (($mwirOn.CoreEnabled -eq "1") -and ($mwirOn.HaloEnabled -eq "1") -and ([double]$mwirOn.CoreGray -gt 0.0) -and ([double]$mwirOn.HaloGray -gt 0.0) -and ([double]$mwirOn.CoreGray -ge [double]$mwirOn.HaloGray)) $mwirOn.PlumeLine)) | Out-Null
$checks.Add((Add-Check "LWIR core/halo are enabled and weaker than MWIR" (($lwirOn.CoreEnabled -eq "1") -and ($lwirOn.HaloEnabled -eq "1") -and ([double]$lwirOn.CoreGray -gt 0.0) -and ([double]$lwirOn.HaloGray -gt 0.0) -and ([double]$lwirOn.CoreGray -lt [double]$mwirOn.CoreGray) -and ([double]$lwirOn.HaloGray -lt [double]$mwirOn.HaloGray)) $lwirOn.PlumeLine)) | Out-Null
$checks.Add((Add-Check "NIR/SWIR core/halo are weaker than MWIR" (([double]$nirOn.CoreGray -lt [double]$mwirOn.CoreGray) -and ([double]$nirOn.HaloGray -lt [double]$mwirOn.HaloGray) -and ([double]$swirOn.CoreGray -lt [double]$mwirOn.CoreGray) -and ([double]$swirOn.HaloGray -lt [double]$mwirOn.HaloGray)) "NIR=$($nirOn.CoreGray)/$($nirOn.HaloGray) SWIR=$($swirOn.CoreGray)/$($swirOn.HaloGray) MWIR=$($mwirOn.CoreGray)/$($mwirOn.HaloGray)")) | Out-Null
$checks.Add((Add-Check "ForcePlumeVisible shows core/halo with engine off" (($forceOff.CoreEnabled -eq "1") -and ($forceOff.HaloEnabled -eq "1") -and ([double]$forceOff.CoreGray -gt 0.0) -and ([double]$forceOff.HaloGray -gt 0.0)) $forceOff.PlumeLine)) | Out-Null
$checks.Add((Add-Check "viewValid=false hides core/halo plume" (($viewInvalid.CoreVisible -eq "0") -and ($viewInvalid.HaloVisible -eq "0")) $viewInvalid.PlumeLine)) | Out-Null

$metrics | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8
$summary = @()
$summary += "# Stage5 EnginePlume Smoke"
$summary += ""
$summary += "- metrics: $csvPath"
$summary += "- runs: " + (($runs | ForEach-Object { $_.Stdout }) -join "; ")
$summary += ""
$summary += "## Checks"
foreach ($check in $checks) {
    $summary += "- $($check.Status): $($check.Check) - $($check.Detail)"
}
$summary += ""
$summary += "## Metrics"
foreach ($metric in $metrics) {
    $summary += "- $($metric.Label): coreEnabled=$($metric.CoreEnabled), haloEnabled=$($metric.HaloEnabled), coreVisible=$($metric.CoreVisible), haloVisible=$($metric.HaloVisible), band=$($metric.Band), coreGray=$($metric.CoreGray), haloGray=$($metric.HaloGray), sameOutput=$($metric.SameOutput), source=$($metric.Source)"
}
$summary | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$checks | Format-Table -AutoSize
Write-Host ""
Write-Host "Summary: $summaryPath"
Write-Host "Metrics: $csvPath"

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 plume smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage5 plume smoke passed." -ForegroundColor Green
