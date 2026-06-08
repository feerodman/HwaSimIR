param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage7_sky_horizon"
$summaryPath = Join-Path $logDir "stage7_sky_horizon_summary.md"
$csvPath = Join-Path $logDir "stage7_sky_horizon_metrics.csv"

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
        [int]$ViewMax = 30000
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
    WDouble 40.0
    WDouble 23000.0
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

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Invoke-HwaRun {
    param(
        [string]$Label,
        [string]$DebugMode,
        [int]$ViewMax,
        [int]$EnvSky,
        [int]$EnvTerrain
    )

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage7-sky-horizon-smoke-$Label-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage7-sky-horizon-smoke-$Label-$stamp.err.log"
    $process = $null
    $udp = $null
    $previousStage7 = [Environment]::GetEnvironmentVariable("EnableStage7SkyHorizon", "Process")
    $previousApplyWindow = [Environment]::GetEnvironmentVariable("Stage6DisplayApplyToWindow", "Process")
    $previousBackground = [Environment]::GetEnvironmentVariable("Stage6BackgroundDisplayEnable", "Process")
    $previousExitOnStop = [Environment]::GetEnvironmentVariable("HwaSimIRExitOnStop", "Process")
    $previousDebugMode = [Environment]::GetEnvironmentVariable("Stage7DebugMode", "Process")

    try {
        Normalize-ProcessPathEnvironment
        [Environment]::SetEnvironmentVariable("EnableStage7SkyHorizon", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage6DisplayApplyToWindow", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage6BackgroundDisplayEnable", "1", "Process")
        [Environment]::SetEnvironmentVariable("HwaSimIRExitOnStop", "1", "Process")
        [Environment]::SetEnvironmentVariable("Stage7DebugMode", $DebugMode, "Process")

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -EnvSky $EnvSky -EnvTerrain $EnvTerrain -ViewMax $ViewMax
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $control = New-ControlStartPacket
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        for ($frame = 1; $frame -le 5; $frame++) {
            $display = New-DisplayPacket -TimeMs ([double](1000 + ($frame * 33)))
            [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
            Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 250))
        }
        Write-Host "sent scenario=$Label debugMode=$DebugMode viewMax=$ViewMax envSky=$EnvSky envTerrain=$EnvTerrain"
        Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 750))
        Start-Sleep -Seconds 4
    }
    finally {
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-HwaProcessGracefully -Process $process -Udp $udp
            }
        }
        [Environment]::SetEnvironmentVariable("EnableStage7SkyHorizon", $previousStage7, "Process")
        [Environment]::SetEnvironmentVariable("Stage6DisplayApplyToWindow", $previousApplyWindow, "Process")
        [Environment]::SetEnvironmentVariable("Stage6BackgroundDisplayEnable", $previousBackground, "Process")
        [Environment]::SetEnvironmentVariable("HwaSimIRExitOnStop", $previousExitOnStop, "Process")
        [Environment]::SetEnvironmentVariable("Stage7DebugMode", $previousDebugMode, "Process")
        if ($udp) {
            $udp.Close()
        }
    }

    return [PSCustomObject]@{
        Label  = $Label
        DebugMode = $DebugMode
        ViewMax = $ViewMax
        Stdout = $stdout
        Stderr = $stderr
        Text   = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Write-Host "Stage7 sky horizon smoke"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$scenarios = @(
    @{ Label = "off_30km"; DebugMode = "Off"; ViewMax = 30000; EnvSky = 0; EnvTerrain = 0 },
    @{ Label = "color_30km"; DebugMode = "SkyGroundColor"; ViewMax = 30000; EnvSky = 1; EnvTerrain = 0 },
    @{ Label = "sky_80km"; DebugMode = "SkyOnly"; ViewMax = 80000; EnvSky = 0; EnvTerrain = 2 },
    @{ Label = "ground_80km"; DebugMode = "GroundOnly"; ViewMax = 80000; EnvSky = 4; EnvTerrain = 2 }
)
$runs = foreach ($scenario in $scenarios) {
    Invoke-HwaRun -Label $scenario.Label -DebugMode $scenario.DebugMode -ViewMax $scenario.ViewMax -EnvSky $scenario.EnvSky -EnvTerrain $scenario.EnvTerrain
}
$text = ($runs.Text -join [Environment]::NewLine)
$checks = New-Object System.Collections.Generic.List[object]

$skyLines = [System.Text.RegularExpressions.Regex]::Matches($text, "\[Stage7 3DSkyGround\][^\r\n]+")
$real3dLines = [System.Text.RegularExpressions.Regex]::Matches($text, "\[Stage7 Real3DBackground\][^\r\n]+")
$groundReferenceLines = [System.Text.RegularExpressions.Regex]::Matches($text, "\[Stage7 GroundReference\][^\r\n]+")
$checks.Add((Add-Check "Stage7 3DSkyGround logs present" ($skyLines.Count -gt 0) (($runs.Stdout) -join "; "))) | Out-Null
$checks.Add((Add-Check "Stage7 Real3DBackground logs present" ($real3dLines.Count -gt 0) (($runs.Stdout) -join "; "))) | Out-Null
$checks.Add((Add-Check "Stage7 GroundReference logs present" ($groundReferenceLines.Count -gt 0) (($runs.Stdout) -join "; "))) | Out-Null
$checks.Add((Add-Check "skyGray and groundGray differ" ($text -match "skyGray=([0-9.]+).*groundGray=([0-9.]+)" -and $Matches[1] -ne $Matches[2]) "sky/ground contrast")) | Out-Null
$checks.Add((Add-Check "real 3D background uses sky dome and lower shell" (($text -match "skyDome=1") -and ($text -match "lowerShell=1") -and ($text -match "flatGroundPlane=0")) "skyDome=1 lowerShell=1 flatGroundPlane=0")) | Out-Null
$checks.Add((Add-Check "ground reference defaults to reference_zero" ($text -match "\[Stage7 GroundReference\][^\r\n]*mode=reference_zero") "mode=reference_zero")) | Out-Null

foreach ($run in $runs) {
    $runText = $run.Text
    $diagLines = [System.Text.RegularExpressions.Regex]::Matches($runText, "\[Stage7 SkyDomeDiag\][^\r\n]+")
    $matchingRadiusLine = $null
    foreach ($diagLine in $diagLines) {
        if ($diagLine.Value -match "farClipM=([0-9.]+).*skyRadius=([0-9.]+).*radiusLessThanFarClip=1") {
            $far = [double]$Matches[1]
            $radius = [double]$Matches[2]
            if ([Math]::Abs($far - $run.ViewMax) -lt 0.5 -and $radius -lt $far) {
                $matchingRadiusLine = $diagLine.Value
                break
            }
        }
    }
    $checks.Add((Add-Check "$($run.Label): debug mode logged" ($runText -match "\[Stage7 Debug\][^\r\n]*mode=$([Regex]::Escape($run.DebugMode))") $run.Stdout)) | Out-Null
    $checks.Add((Add-Check "$($run.Label): SkyDomeDiag radius below far clip" ($null -ne $matchingRadiusLine) "viewMax=$($run.ViewMax)")) | Out-Null
    $checks.Add((Add-Check "$($run.Label): GroundDiag present" ($runText -match "\[Stage7 GroundDiag\][^\r\n]*farClipM=$($run.ViewMax)") $run.Stdout)) | Out-Null
    $checks.Add((Add-Check "$($run.Label): Real3DBackground lower shell active" ($runText -match "\[Stage7 Real3DBackground\][^\r\n]*lowerShell=1[^\r\n]*flatGroundPlane=0") $run.Stdout)) | Out-Null
    $checks.Add((Add-Check "$($run.Label): GroundReference reference_zero" ($runText -match "\[Stage7 GroundReference\][^\r\n]*mode=reference_zero") $run.Stdout)) | Out-Null
}
$checks.Add((Add-Check "SkyGroundColor remains visible after radius fix" ($text -notmatch "STAGE7_SKY_DOME_NOT_VISIBLE_AFTER_RADIUS_FIX") "no visibility warning")) | Out-Null

$metrics = foreach ($run in $runs) {
    $skyLine = [System.Text.RegularExpressions.Regex]::Match($run.Text, "\[Stage7 3DSkyGround\][^\r\n]+")
    $diagLine = [System.Text.RegularExpressions.Regex]::Match($run.Text, "\[Stage7 SkyDomeDiag\][^\r\n]*farClipM=$($run.ViewMax)[^\r\n]*")
    [PSCustomObject]@{
        label = $run.Label
        debugMode = $run.DebugMode
        farClipM = $run.ViewMax
        skyRadius = $(if ($diagLine.Value -match "skyRadius=([0-9.]+)") { $Matches[1] } else { "" })
        radiusLessThanFarClip = $(if ($diagLine.Value -match "radiusLessThanFarClip=([01])") { $Matches[1] } else { "" })
        lowerShell = $(if ($run.Text -match "\[Stage7 Real3DBackground\][^\r\n]*lowerShell=([01])") { $Matches[1] } else { "" })
        flatGroundPlane = $(if ($run.Text -match "\[Stage7 Real3DBackground\][^\r\n]*flatGroundPlane=([01])") { $Matches[1] } else { "" })
        groundReferenceMode = $(if ($run.Text -match "\[Stage7 GroundReference\][^\r\n]*mode=([A-Za-z0-9_]+)") { $Matches[1] } else { "" })
        skyGray = $(if ($skyLine.Value -match "skyGray=([0-9.]+)") { $Matches[1] } else { "" })
        groundGray = $(if ($skyLine.Value -match "groundGray=([0-9.]+)") { $Matches[1] } else { "" })
        stdout = $run.Stdout
    }
}

$metrics | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$checks | Format-Table -AutoSize
$summary = @()
$summary += "# Stage7 Sky Horizon Smoke"
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
    Write-Host "Stage7 sky horizon smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage7 sky horizon smoke passed." -ForegroundColor Green
