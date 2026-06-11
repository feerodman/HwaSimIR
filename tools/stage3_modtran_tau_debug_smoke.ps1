param(
    [int[]]$Bands = @(1, 2),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "HwaSim_IR\Bin\HwaSim_IR.exe"
$hwaWorkDir = Join-Path $rootPath "HwaSim_IR\Bin"
$logDir = Join-Path $rootPath "logs\stage3"
$loaderSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IR\IRModtranTauLut.cpp"
$atmosphereSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\IRSimulation.cpp"
$appSource = Join-Path $rootPath "HwaSim_IR\HwaSim_IR\HwaSimIR.cpp"

if ($Bands.Count -eq 1 -and $Bands[0] -eq 1) {
    # `powershell -File script.ps1 -Bands @(1,2)` can arrive as a single 1 on
    # Windows PowerShell command lines. Keep the requested NIR/MWIR smoke pair.
    $Bands = @(1, 2)
}

function Assert-Path {
    param(
        [string]$Path,
        [string]$Label
    )
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

    WInt 5
    WInt 5
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
    function WTargetState([int]$targetType, [int]$targetId, [double]$lonOffset, [double]$altMeters) {
        WInt $targetType
        WInt 1
        WInt $targetId
        WBool $false
        WBool $true
        WSpatial 0.0 $lonOffset $altMeters 0.0 0.0 0.0 0.0
        WInt 0x01
    }

    WInt 0x38
    WInt 1
    WInt 1
    WDouble $TimeMs
    WSpatial 0.0 0.0 1000.0 0.0 0.0 0.0 0.0
    WWeaponState
    WInt 5
    WTargetState 0x22 0 0.010 1000.0
    WTargetState 0x22 1 0.015 1000.0
    WTargetState 0x22 2 0.020 1000.0
    WTargetState 0x33 3 0.025 1000.0
    WTargetState 0x33 4 0.030 1000.0

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Invoke-HwaRun {
    param(
        [string]$Label,
        [string]$DebugValue
    )

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdout = Join-Path $logDir "HwaSimIR-modtran-tau-$Label-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-modtran-tau-$Label-$stamp.err.log"
    $oldDebug = [Environment]::GetEnvironmentVariable("EnableModtranTauDebug", "Process")

    $process = $null
    $udp = $null
    try {
        [Environment]::SetEnvironmentVariable("EnableModtranTauDebug", $DebugValue, "Process")
        Normalize-ProcessPathEnvironment
        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        foreach ($band in $Bands) {
            $packet = New-InitPacket -Band $band
            $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            Write-Host "sent $Label init band=$band bytes=$sent"
            Start-Sleep -Milliseconds $DelayMs

            $control = New-ControlStartPacket
            $sentControl = $udp.Send($control, $control.Length, "127.0.0.1", 8888)
            Write-Host "sent $Label control-start bytes=$sentControl"
            Start-Sleep -Milliseconds $DelayMs

            for ($frame = 0; $frame -lt 3; ++$frame) {
                $display = New-DisplayPacket -TimeMs ([double](($frame + 1) * 33))
                $sentDisplay = $udp.Send($display, $display.Length, "127.0.0.1", 8888)
                Write-Host "sent $Label display frame=$($frame + 1) bytes=$sentDisplay"
                Start-Sleep -Milliseconds $DelayMs
            }
        }
        Start-Sleep -Seconds 5
    }
    finally {
        if ($udp) {
            $udp.Close()
        }
        if ($process) {
            $process.Refresh()
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                Write-Host "stopped $Label pid=$($process.Id)"
            }
        }
        [Environment]::SetEnvironmentVariable("EnableModtranTauDebug", $oldDebug, "Process")
    }

    return [PSCustomObject]@{
        Label  = $Label
        Stdout = $stdout
        Stderr = $stderr
        Text   = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

function Add-Check {
    param(
        [string]$Name,
        [bool]$Ok,
        [string]$Detail
    )

    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Test-SourceHasNoBandRadianceHook {
    $forbiddenPattern = "path_radiance_band|sky_radiance_band|path_scattering_radiance_band|solar_irradiance_band"
    $files = @($loaderSource, $atmosphereSource, $appSource)
    foreach ($file in $files) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            return $false
        }
        $text = Get-Content -LiteralPath $file -Raw -Encoding UTF8
        if ($text -match $forbiddenPattern) {
            return $false
        }
    }
    return $true
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

Write-Host "Stage 3 MODTRAN tau debug smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$onRun = Invoke-HwaRun -Label "debug-on" -DebugValue "1"
$offRun = Invoke-HwaRun -Label "debug-off" -DebugValue "0"

$checks = New-Object System.Collections.Generic.List[object]
$debugText = $onRun.Text
$offText = $offRun.Text

$checks.Add((Add-Check "debug enabled startup" ($debugText -match "EnableModtranTauDebug=1") $onRun.Stdout)) | Out-Null
$checks.Add((Add-Check "debug line emitted" ($debugText -match "MODTRAN Tau Debug") "MODTRAN Tau Debug")) | Out-Null
$checks.Add((Add-Check "debug source is band_lut.csv" ($debugText -match "source=band_lut\.csv") "source=band_lut.csv")) | Out-Null
$checks.Add((Add-Check "debug includes NIR or MWIR band" ($debugText -match "band=(NIR|MWIR)") "band=NIR or band=MWIR")) | Out-Null
$checks.Add((Add-Check "debug covers NIR" ($debugText -match "band=NIR") "band=NIR")) | Out-Null
$checks.Add((Add-Check "debug covers MWIR" ($debugText -match "band=MWIR") "band=MWIR")) | Out-Null
$checks.Add((Add-Check "debug includes tau fields" (($debugText -match "tau_up=") -and ($debugText -match "tau_down=")) "tau_up/tau_down")) | Out-Null
$checks.Add((Add-Check "debug includes old/new/diff" (($debugText -match "old_tau=") -and ($debugText -match "new_tau=") -and ($debugText -match "diff=")) "old_tau/new_tau/diff")) | Out-Null
$checks.Add((Add-Check "debug includes fallback state" ($debugText -match "fallback") "fallback_state")) | Out-Null
$checks.Add((Add-Check "debug disabled startup" ($offText -match "EnableModtranTauDebug=0") $offRun.Stdout)) | Out-Null
$checks.Add((Add-Check "debug disabled suppresses tau log" ($offText -notmatch "MODTRAN Tau Debug") "no MODTRAN Tau Debug in disabled run")) | Out-Null
$checks.Add((Add-Check "no MODTRAN radiance band fields used by runtime loader" (Test-SourceHasNoBandRadianceHook) "checked loader, atmosphere, app source")) | Out-Null

$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Logs:"
Write-Host $onRun.Stdout
Write-Host $onRun.Stderr
Write-Host $offRun.Stdout
Write-Host $offRun.Stderr

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage3 MODTRAN tau debug smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage3 MODTRAN tau debug smoke passed." -ForegroundColor Green
