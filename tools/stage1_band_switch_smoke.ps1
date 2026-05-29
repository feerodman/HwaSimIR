param(
    [int[]]$Bands = @(0, 1, 2, 3, 4),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage1"

function Assert-Path {
    param(
        [string]$Path,
        [string]$Label
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
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

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$hwaOut = Join-Path $logDir "HwaSimIR-band-switch-$stamp.out.log"
$hwaErr = Join-Path $logDir "HwaSimIR-band-switch-$stamp.err.log"

Write-Host "Stage 1 band switch smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

# Some shells leave both Path and PATH in the process block. Windows process
# creation treats them as the same key, so normalize before Start-Process.
$processPathValue = [Environment]::GetEnvironmentVariable("Path", "Process")
if ([string]::IsNullOrEmpty($processPathValue)) {
    $processPathValue = [Environment]::GetEnvironmentVariable("PATH", "Process")
}
if (-not [string]::IsNullOrEmpty($processPathValue)) {
    [Environment]::SetEnvironmentVariable("PATH", $null, "Process")
    [Environment]::SetEnvironmentVariable("Path", $processPathValue, "Process")
}

$process = $null
$udp = $null
try {
    $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $hwaOut -RedirectStandardError $hwaErr
    Start-Sleep -Seconds 3

    $udp = New-Object System.Net.Sockets.UdpClient
    foreach ($band in $Bands) {
        $packet = New-InitPacket -Band $band
        $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Write-Host "sent init band=$band bytes=$sent"
        Start-Sleep -Milliseconds $DelayMs
    }
    Start-Sleep -Seconds 2
}
finally {
    if ($udp) {
        $udp.Close()
    }
    if ($process) {
        $process.Refresh()
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            Write-Host "stopped pid=$($process.Id)"
        }
    }
}

Write-Host ""
Write-Host "Logs:"
Write-Host $hwaOut
Write-Host $hwaErr
