param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage7_weather_perf"
$summaryPath = Join-Path $logDir "stage7_weather_perf_summary.md"
$csvPath = Join-Path $logDir "stage7_weather_perf_metrics.csv"

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
    param(
        [int]$EnvSky,
        [int]$EnvTerrain,
        [int]$ViewMax,
        [double]$PixelAngleUrad
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
        WInt 50
        WInt $ViewMax
        WDouble $PixelAngleUrad
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
    WInt $EnvTerrain
    WInt $EnvSky
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 0.0
    WDouble 1.0
    WDouble 1.0
    WDouble 1.0
    WDouble 25.0
    WDouble 85.0
    WDouble 8000.0
    WDouble 0.0
    WDouble 0.0
    WInt 30
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
    $bw.Write([int]$Command)
    $bw.Write([int]1)
    $bw.Write([int]0)
    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function New-DisplayPacket {
    param([double]$TimeMs)

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
    function WTargetState([int]$TargetType, [int]$TargetId, [double]$LonOffset) {
        WInt $TargetType
        WInt 1
        WInt $TargetId
        WBool $true
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
    WTargetState 0x22 0 0.010
    WTargetState 0x22 1 0.015
    WTargetState 0x22 2 0.020
    WTargetState 0x33 3 0.025
    WTargetState 0x33 4 0.030

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

function Invoke-HwaRun {
    param(
        [hashtable]$Scenario
    )

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage7-weather-perf-$($Scenario.Label)-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage7-weather-perf-$($Scenario.Label)-$stamp.err.log"
    $process = $null
    $udp = $null
    $envNames = @(
        "EnableStage7SkyHorizon",
        "Stage6DisplayApplyToWindow",
        "Stage6BackgroundDisplayEnable",
        "HwaSimIRExitOnStop",
        "Stage7DebugMode",
        "EnableStage7WeatherEffects",
        "Stage7EnableCloudLayer",
        "Stage7EnableFog",
        "Stage7EnablePrecipitation",
        "Stage7PrecipitationMode",
        "Stage7CloudLayerMaxCards",
        "Stage7PrecipitationMaxParticles"
    )
    $previous = @{}
    foreach ($name in $envNames) {
        $previous[$name] = [Environment]::GetEnvironmentVariable($name, "Process")
    }

    try {
        Normalize-ProcessPathEnvironment
        [Environment]::SetEnvironmentVariable("EnableStage7SkyHorizon", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage6DisplayApplyToWindow", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage6BackgroundDisplayEnable", "1", "Process")
        [Environment]::SetEnvironmentVariable("HwaSimIRExitOnStop", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage7DebugMode", "Off", "Process")
        [Environment]::SetEnvironmentVariable("EnableStage7WeatherEffects", $Scenario.EnableWeatherEffects, "Process")
        [Environment]::SetEnvironmentVariable("Stage7EnableCloudLayer", $Scenario.EnableCloudLayer, "Process")
        [Environment]::SetEnvironmentVariable("Stage7EnableFog", $Scenario.EnableFog, "Process")
        [Environment]::SetEnvironmentVariable("Stage7EnablePrecipitation", $Scenario.EnablePrecipitation, "Process")
        [Environment]::SetEnvironmentVariable("Stage7PrecipitationMode", $Scenario.PrecipitationMode, "Process")
        [Environment]::SetEnvironmentVariable("Stage7CloudLayerMaxCards", $Scenario.CloudCards, "Process")
        [Environment]::SetEnvironmentVariable("Stage7PrecipitationMaxParticles", $Scenario.PrecipitationCards, "Process")

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -EnvSky $Scenario.EnvSky -EnvTerrain 0 -ViewMax 50000 -PixelAngleUrad $Scenario.PixelAngleUrad
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs
        $control = New-ControlPacket -Command 2
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs
        for ($frame = 1; $frame -le 5; $frame++) {
            $display = New-DisplayPacket -TimeMs ([double](1000 + ($frame * 33)))
            [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 250))
        }
        Write-Host "sent scenario=$($Scenario.Label) envSky=$($Scenario.EnvSky) pixelAngleUrad=$($Scenario.PixelAngleUrad) mode=$($Scenario.PrecipitationMode)"
        Start-Sleep -Seconds 4
    }
    finally {
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-HwaProcessGracefully -Process $process -Udp $udp
            }
        }
        foreach ($name in $envNames) {
            [Environment]::SetEnvironmentVariable($name, $previous[$name], "Process")
        }
        if ($udp) { $udp.Close() }
    }

    return [PSCustomObject]@{
        Label = $Scenario.Label
        ExpectedOverlay = $Scenario.ExpectedOverlay
        Stdout = $stdout
        Stderr = $stderr
        Text = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Write-Host "Stage7 weather perf smoke"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$scenarios = @(
    @{ Label = "weather_off"; EnvSky = 2; PixelAngleUrad = 10.0; EnableWeatherEffects = "0"; EnableCloudLayer = "0"; EnableFog = "0"; EnablePrecipitation = "0"; PrecipitationMode = "ScreenOverlay"; CloudCards = "0"; PrecipitationCards = "0"; ExpectedOverlay = "0" },
    @{ Label = "rain_overlay_0p1deg"; EnvSky = 2; PixelAngleUrad = 2.18; EnableWeatherEffects = "1"; EnableCloudLayer = "0"; EnableFog = "1"; EnablePrecipitation = "1"; PrecipitationMode = "ScreenOverlay"; CloudCards = "0"; PrecipitationCards = "0"; ExpectedOverlay = "1" },
    @{ Label = "rain_overlay_9deg"; EnvSky = 2; PixelAngleUrad = 196.35; EnableWeatherEffects = "1"; EnableCloudLayer = "0"; EnableFog = "1"; EnablePrecipitation = "1"; PrecipitationMode = "ScreenOverlay"; CloudCards = "0"; PrecipitationCards = "0"; ExpectedOverlay = "1" }
)

$runs = foreach ($scenario in $scenarios) {
    $run = Invoke-HwaRun -Scenario $scenario
    Start-Sleep -Seconds 1
    $run
}
$text = ($runs.Text -join [Environment]::NewLine)
$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "Stage6 final output remains same source" ($text -match "\[Stage6 FinalPipeline\][^\r\n]*sameOutput=1") "sameOutput=1")) | Out-Null
$checks.Add((Add-Check "Stage7 Perf logs present" ($text -match "\[Stage7 Perf\]") (($runs.Stdout) -join "; "))) | Out-Null
$checks.Add((Add-Check "No per-frame texture load warnings" ($text -notmatch "STAGE7_TEXTURE_LOAD_IN_FRAME") "textureLoadCountThisFrame should remain 0 for default and ScreenOverlay")) | Out-Null

$metrics = foreach ($run in $runs) {
    $perfMatches = [System.Text.RegularExpressions.Regex]::Matches($run.Text, "\[Stage7 Perf\][^\r\n]+")
    $overlayMatches = [System.Text.RegularExpressions.Regex]::Matches($run.Text, "\[Stage7 PrecipitationOverlay\][^\r\n]+")
    $sensorMatches = [System.Text.RegularExpressions.Regex]::Matches($run.Text, "\[Stage6 SensorGeometry\][^\r\n]+")
    $perfLine = if ($perfMatches.Count -gt 0) { $perfMatches[$perfMatches.Count - 1].Value } else { "" }
    $overlayLine = if ($overlayMatches.Count -gt 0) { $overlayMatches[$overlayMatches.Count - 1].Value } else { "" }
    $sensorLine = if ($sensorMatches.Count -gt 0) { $sensorMatches[$sensorMatches.Count - 1].Value } else { "" }
    [PSCustomObject]@{
        label = $run.Label
        expectedOverlay = $run.ExpectedOverlay
        weatherNodeCount = $(if ($perfLine -match "weatherNodeCount=([0-9]+)") { $Matches[1] } else { "" })
        cloudNodeCount = $(if ($perfLine -match "cloudNodeCount=([0-9]+)") { $Matches[1] } else { "" })
        precipitationNodeCount = $(if ($perfLine -match "precipitationNodeCount=([0-9]+)") { $Matches[1] } else { "" })
        textureLoadCountThisFrame = $(if ($perfLine -match "textureLoadCountThisFrame=([0-9]+)") { $Matches[1] } else { "" })
        updateWeatherNodesMs = $(if ($perfLine -match "updateWeatherNodesMs=([0-9.]+)") { $Matches[1] } else { "" })
        overlayActive = $(if ($overlayLine -match "active=([01])") { $Matches[1] } else { "" })
        overlayMode = $(if ($overlayLine -match "mode=([A-Za-z]+)") { $Matches[1] } else { "" })
        sensorFovDeg = $(if ($overlayLine -match "sensorFovDeg=([0-9.]+)") { $Matches[1] } elseif ($sensorLine -match "horizontalFovDeg=([0-9.]+)") { $Matches[1] } else { "" })
        giantVerticalBars = $(if ($overlayLine -match "giantVerticalBars=([01])") { $Matches[1] } else { "" })
        sameOutput = $(if ($run.Text -match "\[Stage6 FinalPipeline\][^\r\n]*sameOutput=([01])") { $Matches[1] } else { "" })
        stdout = $run.Stdout
    }
}

$metrics | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$weatherOff = @($metrics | Where-Object { $_.label -eq "weather_off" })[0]
$rainSmall = @($metrics | Where-Object { $_.label -eq "rain_overlay_0p1deg" })[0]
$rainWide = @($metrics | Where-Object { $_.label -eq "rain_overlay_9deg" })[0]

$checks.Add((Add-Check "EnableWeatherEffects=0 creates no weather nodes" (($weatherOff.weatherNodeCount -eq "0") -and ($weatherOff.cloudNodeCount -eq "0") -and ($weatherOff.precipitationNodeCount -eq "0")) $weatherOff.stdout)) | Out-Null
$checks.Add((Add-Check "Rain ScreenOverlay creates no precipitation cards" (($rainSmall.precipitationNodeCount -eq "0") -and ($rainWide.precipitationNodeCount -eq "0")) "ScreenOverlay card count")) | Out-Null
$checks.Add((Add-Check "Rain ScreenOverlay does not load textures per frame" (($rainSmall.textureLoadCountThisFrame -eq "0") -and ($rainWide.textureLoadCountThisFrame -eq "0")) "textureLoadCountThisFrame=0")) | Out-Null
$checks.Add((Add-Check "0.1 degree FOV overlay route active" (($rainSmall.overlayActive -eq "1") -and ($rainSmall.overlayMode -eq "ScreenOverlay") -and ([double]$rainSmall.sensorFovDeg -lt 0.2)) "0.1deg overlay")) | Out-Null
$checks.Add((Add-Check "9 degree FOV overlay route avoids giant vertical bars" (($rainWide.overlayActive -eq "1") -and ($rainWide.overlayMode -eq "ScreenOverlay") -and ($rainWide.giantVerticalBars -eq "0") -and ([double]$rainWide.sensorFovDeg -gt 8.0)) "9deg overlay")) | Out-Null

$checks | Format-Table -AutoSize

$summary = @()
$summary += "# Stage7 Weather Perf Smoke"
$summary += ""
$summary += "- stdout logs: $($runs.Stdout -join '; ')"
$summary += "- stderr logs: $($runs.Stderr -join '; ')"
$summary += "- metrics: $csvPath"
$summary += ""
$summary += "## Checks"
foreach ($check in $checks) {
    $summary += "- $($check.Status): $($check.Check) - $($check.Detail)"
}
$summary | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "Summary: $summaryPath"
Write-Host "Metrics: $csvPath"

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage7 weather perf smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage7 weather perf smoke passed." -ForegroundColor Green
