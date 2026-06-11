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
    $Bands = @(1, 2)
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
    param([string]$Label, [string]$UseValue)

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $stdout = Join-Path $logDir "HwaSimIR-modtran-tau-active-$Label-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-modtran-tau-active-$Label-$stamp.err.log"
    $oldDebug = [Environment]::GetEnvironmentVariable("EnableModtranTauDebug", "Process")
    $oldUse = [Environment]::GetEnvironmentVariable("UseModtranTauForAtmosphere", "Process")

    $process = $null
    $udp = $null
    try {
        [Environment]::SetEnvironmentVariable("EnableModtranTauDebug", "1", "Process")
        [Environment]::SetEnvironmentVariable("UseModtranTauForAtmosphere", $UseValue, "Process")
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
        [Environment]::SetEnvironmentVariable("UseModtranTauForAtmosphere", $oldUse, "Process")
    }

    return [PSCustomObject]@{
        Label  = $Label
        Stdout = $stdout
        Stderr = $stderr
        Text   = $(if (Test-Path -LiteralPath $stdout) { Get-Content -LiteralPath $stdout -Raw -Encoding UTF8 } else { "" })
    }
}

function Add-Check {
    param([string]$Name, [bool]$Ok, [string]$Detail)
    [PSCustomObject]@{
        Check  = $Name
        Status = $(if ($Ok) { "OK" } else { "FAIL" })
        Detail = $Detail
    }
}

function Test-SourceHasNoBandRadianceHook {
    $forbiddenPattern = "path_radiance_band|sky_radiance_band|path_scattering_radiance_band|solar_irradiance_band"
    foreach ($file in @($loaderSource, $atmosphereSource, $appSource)) {
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

Write-Host "Stage 3 MODTRAN tau active smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$inactiveRun = Invoke-HwaRun -Label "active0" -UseValue "0"
$activeRun = Invoke-HwaRun -Label "active1" -UseValue "1"

$inactiveText = $inactiveRun.Text
$activeText = $activeRun.Text
$checks = New-Object System.Collections.Generic.List[object]

$checks.Add((Add-Check "active=0 startup" ($inactiveText -match "UseModtranTauForAtmosphere=0") $inactiveRun.Stdout)) | Out-Null
$checks.Add((Add-Check "active=0 returns legacy" (($inactiveText -match "active=0") -and ($inactiveText -match "return_source=legacy")) "active=0 return_source=legacy")) | Out-Null
$checks.Add((Add-Check "active=1 startup" ($activeText -match "UseModtranTauForAtmosphere=1") $activeRun.Stdout)) | Out-Null
$checks.Add((Add-Check "active=1 has NIR/MWIR decision" (($activeText -match "band=NIR") -and ($activeText -match "band=MWIR") -and (($activeText -match "return_source=modtran_tau") -or ($activeText -match "fallback=legacy"))) "NIR/MWIR return_source=modtran_tau or fallback=legacy")) | Out-Null
$checks.Add((Add-Check "active logs old/new/diff" (($activeText -match "old_tau=") -and ($activeText -match "new_tau=") -and ($activeText -match "diff=")) "old_tau/new_tau/diff")) | Out-Null
$checks.Add((Add-Check "active logs fallback state" ($activeText -match "fallback_state=") "fallback_state")) | Out-Null
$checks.Add((Add-Check "VIS/SWIR/LWIR stay legacy if present" ($activeText -notmatch "band=(VIS|SWIR|LWIR).*return_source=modtran_tau") "no VIS/SWIR/LWIR return_source=modtran_tau")) | Out-Null
$checks.Add((Add-Check "radiance fields disabled" (Test-SourceHasNoBandRadianceHook) "checked loader, atmosphere, app source")) | Out-Null

$checks | Format-Table -AutoSize

Write-Host ""
Write-Host "Logs:"
Write-Host $inactiveRun.Stdout
Write-Host $inactiveRun.Stderr
Write-Host $activeRun.Stdout
Write-Host $activeRun.Stderr

$failed = @($checks | Where-Object { $_.Status -ne "OK" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage3 MODTRAN tau active smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage3 MODTRAN tau active smoke passed." -ForegroundColor Green
