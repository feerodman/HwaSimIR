param(
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage6_sensor_geometry"
$summaryPath = Join-Path $logDir "stage6_sensor_geometry_summary.md"
$csvPath = Join-Path $logDir "stage6_sensor_geometry_metrics.csv"

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
        [int]$Band,
        [int]$Width,
        [int]$Height,
        [int]$ViewMin,
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
        WInt $Width
        WInt $Height
        WInt $ViewMin
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

function Get-ExpectedFovDeg {
    param([int]$Pixels, [double]$PixelAngleUrad)
    $pixelAngleRad = $PixelAngleUrad * 1e-6
    return 2.0 * [Math]::Atan([double]$Pixels * [Math]::Tan($pixelAngleRad / 2.0)) * 180.0 / [Math]::PI
}

function Get-Number {
    param([string]$Line, [string]$Name)
    if ($Line -match "$Name=([0-9eE+\.-]+)") {
        return [double]$matches[1]
    }
    return [double]::NaN
}

function Invoke-HwaStage6Run {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage6-sensor-geometry-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage6-sensor-geometry-$stamp.err.log"
    $process = $null
    $udp = $null

    try {
        Normalize-ProcessPathEnvironment
        [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        Start-Sleep -Seconds 3

        $udp = New-Object System.Net.Sockets.UdpClient
        foreach ($scenario in $script:Scenarios) {
            $packet = New-InitPacket -Band $scenario.Band -Width $scenario.Width -Height $scenario.Height -ViewMin $scenario.ViewMin -ViewMax $scenario.ViewMax -PixelAngleUrad $scenario.PixelAngleUrad
            $sent = $udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
            Write-Host "sent init $($scenario.Label) bytes=$sent"
            Start-Sleep -Milliseconds ([Math]::Max($DelayMs, 3000))
        }
        Start-Sleep -Milliseconds ([Math]::Max($DelayMs * 4, 2500))
    }
    finally {
        if ($udp) {
            $udp.Dispose()
        }
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
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

$script:Scenarios = @(
    [PSCustomObject]@{ Label = "640x512_px20"; Band = 2; Width = 640; Height = 512; ViewMin = 100; ViewMax = 50000; PixelAngleUrad = 20.0 },
    [PSCustomObject]@{ Label = "800x800_px10"; Band = 2; Width = 800; Height = 800; ViewMin = 50; ViewMax = 30000; PixelAngleUrad = 10.0 },
    [PSCustomObject]@{ Label = "1024x768_px5"; Band = 2; Width = 1024; Height = 768; ViewMin = 100; ViewMax = 80000; PixelAngleUrad = 5.0 }
)

Write-Host "HwaSimIR: $hwaExe"
Write-Host "Logs: $logDir"

$run = Invoke-HwaStage6Run
$lines = $run.Text -split "\r?\n"
$rows = New-Object System.Collections.Generic.List[object]

foreach ($scenario in $script:Scenarios) {
    $geomLine = @($lines | Where-Object {
        $_ -match "\[Stage6 SensorGeometry\]" -and
        $_ -match "width=$($scenario.Width)\b" -and
        $_ -match "height=$($scenario.Height)\b" -and
        $_ -match "viewMin=$($scenario.ViewMin)\b" -and
        $_ -match "viewMax=$($scenario.ViewMax)\b" -and
        $_ -match "pixelAngleUrad=$($scenario.PixelAngleUrad -replace '\.0$','')(\.0+)?\b" -and
        $_ -match "fov_source=pixelAngle_urad_per_pixel"
    } | Select-Object -First 1)
    $resizeLine = @($lines | Where-Object {
        $_ -match "\[Stage6 Resize\]" -and
        $_ -match "newWindow=$($scenario.Width)x$($scenario.Height)" -and
        $_ -match "renderTexture=$($scenario.Width)x$($scenario.Height)"
    } | Select-Object -First 1)
    $captureLine = @($lines | Where-Object {
        $_ -match "\[Stage6 Capture\]" -and
        $_ -match "frameWidth=$($scenario.Width)\b" -and
        $_ -match "frameHeight=$($scenario.Height)\b" -and
        $_ -match "tcpWidth=$($scenario.Width)\b" -and
        $_ -match "tcpHeight=$($scenario.Height)\b" -and
        $_ -match "channels=RGB8"
    } | Select-Object -First 1)

    $expectedHFov = Get-ExpectedFovDeg -Pixels $scenario.Width -PixelAngleUrad $scenario.PixelAngleUrad
    $expectedVFov = Get-ExpectedFovDeg -Pixels $scenario.Height -PixelAngleUrad $scenario.PixelAngleUrad
    $actualHFov = if ($geomLine) { Get-Number -Line $geomLine -Name "horizontalFovDeg" } else { [double]::NaN }
    $actualVFov = if ($geomLine) { Get-Number -Line $geomLine -Name "verticalFovDeg" } else { [double]::NaN }
    $fovOk = (-not [double]::IsNaN($actualHFov)) -and
        (-not [double]::IsNaN($actualVFov)) -and
        ([Math]::Abs($actualHFov - $expectedHFov) -lt 0.01) -and
        ([Math]::Abs($actualVFov - $expectedVFov) -lt 0.01)

    $ok = [bool]$geomLine -and [bool]$resizeLine -and [bool]$captureLine -and $fovOk
    $rows.Add([PSCustomObject]@{
        label                  = $scenario.Label
        width                  = $scenario.Width
        height                 = $scenario.Height
        viewMin                = $scenario.ViewMin
        viewMax                = $scenario.ViewMax
        pixelAngleUrad         = $scenario.PixelAngleUrad
        expectedHorizontalFov  = [Math]::Round($expectedHFov, 6)
        actualHorizontalFov    = if ([double]::IsNaN($actualHFov)) { "" } else { [Math]::Round($actualHFov, 6) }
        expectedVerticalFov    = [Math]::Round($expectedVFov, 6)
        actualVerticalFov      = if ([double]::IsNaN($actualVFov)) { "" } else { [Math]::Round($actualVFov, 6) }
        geometryLog            = [bool]$geomLine
        resizeLog              = [bool]$resizeLine
        captureLog             = [bool]$captureLine
        fovOk                  = $fovOk
        status                 = $(if ($ok) { "PASS" } else { "FAIL" })
    }) | Out-Null
}

$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage6 Sensor Geometry Smoke") | Out-Null
$md.Add("") | Out-Null
$md.Add(("Stdout: ``{0}``" -f $run.Stdout)) | Out-Null
$md.Add(("Stderr: ``{0}``" -f $run.Stderr)) | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | Geometry | Resize | Capture | FOV |") | Out-Null
$md.Add("| --- | --- | --- | --- | --- | --- |") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.label) | $($row.status) | $($row.geometryLog) | $($row.resizeLog) | $($row.captureLog) | $($row.fovOk) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("The smoke only validates Stage6A geometry logs and capture/TCP dimensions. It does not perform AGC, MTF, H264, UDP video, or frame-quality assertions.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage6 sensor geometry smoke failed. See $summaryPath" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Stage6 sensor geometry smoke passed. See $summaryPath" -ForegroundColor Green
