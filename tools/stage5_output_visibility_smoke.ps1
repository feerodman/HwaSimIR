param(
    [int[]]$Bands = @(0, 1, 2, 3),
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$rootPath = $root.Path
$hwaExe = Join-Path $rootPath "ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe"
$hwaWorkDir = Join-Path $rootPath "ConsoleApplication1_LLA\Bin"
$logDir = Join-Path $rootPath "logs\stage5_output_visibility"
$frameDir = Join-Path $logDir "frames"
$summaryPath = Join-Path $logDir "stage5_output_visibility_summary.md"
$csvPath = Join-Path $logDir "stage5_output_visibility_metrics.csv"
$minBodyGray = 0.12
$brightThreshold = 24.0
$contrastThreshold = 10.0
$minContrastPixelCount = 100
$minLocalFeatureGray = 0.04
$frameDumpEvery = 5
$primaryTargetRangeKm = 3.0
$kmPerLongitudeDegreeAtEquator = 111.32
$primaryTargetLonOffsetDeg = $primaryTargetRangeKm / $kmPerLongitudeDegreeAtEquator
$hiddenTargetSpacingKm = 0.5
$hiddenTargetLonSpacingDeg = $hiddenTargetSpacingKm / $kmPerLongitudeDegreeAtEquator

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
        WBool $true
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
    WTargetState 0x22 0 $EngineState $primaryTargetLonOffsetDeg
    WTargetState 0x22 1 $false ($primaryTargetLonOffsetDeg + $hiddenTargetLonSpacingDeg)
    WTargetState 0x22 2 $false ($primaryTargetLonOffsetDeg + 2.0 * $hiddenTargetLonSpacingDeg)
    WTargetState 0x33 3 $false ($primaryTargetLonOffsetDeg + 3.0 * $hiddenTargetLonSpacingDeg)
    WTargetState 0x33 4 $false ($primaryTargetLonOffsetDeg + 4.0 * $hiddenTargetLonSpacingDeg)

    $bytes = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return $bytes
}

function Clear-Stage5OverrideEnv {
    [Environment]::SetEnvironmentVariable("Stage5DebugToneMap", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5BodyRadianceScale", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5HotspotRadianceScale", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5BrightspotRadianceScale", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugMinBodyGray", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5UseBaseTextureModulation", $null, "Process")
}

function Clear-Stage5OutputCaptureEnv {
    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDump", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDumpPath", $null, "Process")
    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDumpEvery", $null, "Process")
}

function Set-Stage5DebugEnv {
    param([hashtable]$Scenario)

    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugViewMode", $Scenario.ViewMode, "Process")
    Clear-Stage5OverrideEnv
    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", "0", "Process")
}

function Set-Stage5OutputCaptureEnv {
    param([string]$FrameDumpPath)

    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDump", "1", "Process")
    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDumpPath", $FrameDumpPath, "Process")
    [Environment]::SetEnvironmentVariable("Stage5OutputFrameDumpEvery", [string]$frameDumpEvery, "Process")
}

function Clear-Stage5DebugEnv {
    [Environment]::SetEnvironmentVariable("EnableStage5RadianceDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("Stage5DebugViewMode", $null, "Process")
    Clear-Stage5OverrideEnv
    Clear-Stage5OutputCaptureEnv
    [Environment]::SetEnvironmentVariable("EnableStage4HotspotVisualDebug", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4BrightSpotVisible", "0", "Process")
    [Environment]::SetEnvironmentVariable("ForceStage4RearHotspotVisible", "0", "Process")
}

function Read-Exact {
    param(
        [System.IO.Stream]$Stream,
        [int]$Count
    )

    $buffer = New-Object byte[] $Count
    $offset = 0
    while ($offset -lt $Count) {
        $read = $Stream.Read($buffer, $offset, $Count - $offset)
        if ($read -le 0) {
            return $null
        }
        $offset += $read
    }
    return $buffer
}

function Test-JpegBytes {
    param([byte[]]$Bytes)

    return ($Bytes.Length -gt 100) -and
        ($Bytes[0] -eq 0xFF) -and
        ($Bytes[1] -eq 0xD8) -and
        ($Bytes[$Bytes.Length - 2] -eq 0xFF) -and
        ($Bytes[$Bytes.Length - 1] -eq 0xD9)
}

function Format-ByteHex {
    param([byte[]]$Bytes)

    if ($null -eq $Bytes -or $Bytes.Length -eq 0) {
        return "NA"
    }
    return (($Bytes | ForEach-Object { $_.ToString("X2") }) -join " ")
}

function Receive-LatestJpegFrame {
    param(
        [System.Net.Sockets.TcpClient]$Client,
        [string]$Label,
        [int]$DurationMs
    )

    if (-not $Client) {
        return [PSCustomObject]@{
            path = $null
            frame_count = 0
            source = "tcp_jpeg"
            success = $false
            failure_reason = "tcp_client_not_connected"
            header_count = 0
            invalid_header_count = 0
            invalid_jpeg_count = 0
            timeout_count = 0
            bytes_read = 0
            first_invalid_header_hex = "NA"
            last_length = "NA"
        }
    }

    $stream = $Client.GetStream()
    $stream.ReadTimeout = 500
    $deadline = [DateTime]::UtcNow.AddMilliseconds($DurationMs)
    $latestPath = $null
    $validFrameCount = 0
    $headerCount = 0
    $invalidHeaderCount = 0
    $invalidJpegCount = 0
    $timeoutCount = 0
    $bytesRead = 0
    $firstInvalidHeaderHex = "NA"
    $lastLength = "NA"
    while ([DateTime]::UtcNow -lt $deadline) {
        try {
            $header = Read-Exact -Stream $stream -Count 4
            if ($null -eq $header) {
                Start-Sleep -Milliseconds 50
                continue
            }
            ++$headerCount
            $bytesRead += 4
            $length = ($header[0] -shl 24) -bor ($header[1] -shl 16) -bor ($header[2] -shl 8) -bor $header[3]
            $lastLength = $length
            if ($length -le 0 -or $length -gt 10000000) {
                ++$invalidHeaderCount
                if ($firstInvalidHeaderHex -eq "NA") {
                    $firstInvalidHeaderHex = Format-ByteHex -Bytes $header
                }
                continue
            }
            $jpeg = Read-Exact -Stream $stream -Count $length
            if ($null -eq $jpeg) {
                ++$timeoutCount
                continue
            }
            $bytesRead += $jpeg.Length
            if (-not (Test-JpegBytes -Bytes $jpeg)) {
                ++$invalidJpegCount
                continue
            }
            ++$validFrameCount
            $latestPath = Join-Path $frameDir "$Label-frame-$validFrameCount.jpg"
            [System.IO.File]::WriteAllBytes($latestPath, $jpeg)
        }
        catch {
            ++$timeoutCount
            Start-Sleep -Milliseconds 50
        }
    }

    if ($latestPath) {
        return [PSCustomObject]@{
            path = $latestPath
            frame_count = $validFrameCount
            source = "tcp_jpeg"
            success = $true
            failure_reason = "none"
            header_count = $headerCount
            invalid_header_count = $invalidHeaderCount
            invalid_jpeg_count = $invalidJpegCount
            timeout_count = $timeoutCount
            bytes_read = $bytesRead
            first_invalid_header_hex = $firstInvalidHeaderHex
            last_length = $lastLength
        }
    }
    $reason = "tcp_no_valid_jpeg"
    if ($headerCount -eq 0) {
        $reason = "tcp_connected_but_no_length_header"
    }
    elseif ($invalidHeaderCount -gt 0 -and $invalidJpegCount -eq 0) {
        $reason = "tcp_invalid_length_header"
    }
    elseif ($invalidJpegCount -gt 0) {
        $reason = "tcp_payload_not_decodable_jpeg"
    }
    return [PSCustomObject]@{
        path = $null
        frame_count = 0
        source = "tcp_jpeg"
        success = $false
        failure_reason = $reason
        header_count = $headerCount
        invalid_header_count = $invalidHeaderCount
        invalid_jpeg_count = $invalidJpegCount
        timeout_count = $timeoutCount
        bytes_read = $bytesRead
        first_invalid_header_hex = $firstInvalidHeaderHex
        last_length = $lastLength
    }
}

function Get-ImageLumaMetrics {
    param([string]$Path)

    try {
        Add-Type -AssemblyName System.Drawing -ErrorAction Stop
        $bitmap = [System.Drawing.Bitmap]::FromFile($Path)
        try {
            $sum = 0.0
            $max = 0.0
            $bright = 0
            $hist = New-Object int[] 256
            $count = [double]($bitmap.Width * $bitmap.Height)
            for ($y = 0; $y -lt $bitmap.Height; ++$y) {
                for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                    $pixel = $bitmap.GetPixel($x, $y)
                    $luma = 0.299 * $pixel.R + 0.587 * $pixel.G + 0.114 * $pixel.B
                    $sum += $luma
                    if ($luma -gt $max) { $max = $luma }
                    if ($luma -ge $brightThreshold) { ++$bright }
                    $bin = [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($luma)))
                    ++$hist[$bin]
                }
            }

            $backgroundBin = 0
            $backgroundCount = -1
            for ($i = 0; $i -lt 256; ++$i) {
                if ($hist[$i] -gt $backgroundCount) {
                    $backgroundCount = $hist[$i]
                    $backgroundBin = $i
                }
            }

            $contrast = 0
            $maxDelta = 0.0
            for ($y = 0; $y -lt $bitmap.Height; ++$y) {
                for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                    if ($x -ge ($bitmap.Width - 120) -and $y -le 50) {
                        continue
                    }
                    $pixel = $bitmap.GetPixel($x, $y)
                    $luma = 0.299 * $pixel.R + 0.587 * $pixel.G + 0.114 * $pixel.B
                    $delta = [Math]::Abs($luma - [double]$backgroundBin)
                    if ($delta -gt $maxDelta) { $maxDelta = $delta }
                    if ($delta -ge $contrastThreshold) { ++$contrast }
                }
            }
            return [PSCustomObject]@{
                mean_luma = [Math]::Round($sum / $count, 4)
                max_luma = [Math]::Round($max, 4)
                bright_pixel_count = $bright
                background_luma = $backgroundBin
                contrast_pixel_count = $contrast
                max_contrast_delta = [Math]::Round($maxDelta, 4)
            }
        }
        finally {
            $bitmap.Dispose()
        }
    }
    catch {
        return $null
    }
}

function Get-Stage5BandName {
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

function Invoke-HwaStage5OutputRun {
    param([hashtable]$Scenario)

    $safeName = ($Scenario.Label -replace '[^A-Za-z0-9_-]', '_')
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
    $stdout = Join-Path $logDir "HwaSimIR-stage5-output-$safeName-$stamp.out.log"
    $stderr = Join-Path $logDir "HwaSimIR-stage5-output-$safeName-$stamp.err.log"
    $dumpPath = Join-Path $frameDir "$safeName-stage5-dump.png"
    $listener = $null
    $accept = $null
    $client = $null
    $process = $null
    $udp = $null
    $frame = $null
    $tcpFrame = $null
    $frameMetrics = $null
    $captureFailureReason = "none"

    try {
        Normalize-ProcessPathEnvironment
        if (Test-Path -LiteralPath $dumpPath) {
            Remove-Item -LiteralPath $dumpPath -Force
        }
        Set-Stage5DebugEnv -Scenario $Scenario
        Set-Stage5OutputCaptureEnv -FrameDumpPath $dumpPath

        $listener = New-Object System.Net.Sockets.TcpListener([System.Net.IPAddress]::Parse("127.0.0.1"), 5555)
        $listener.Server.SetSocketOption([System.Net.Sockets.SocketOptionLevel]::Socket, [System.Net.Sockets.SocketOptionName]::ReuseAddress, $true)
        $listener.Start()
        $accept = $listener.BeginAcceptTcpClient($null, $null)

        $process = Start-Process -FilePath $hwaExe -WorkingDirectory $hwaWorkDir -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if ($accept.AsyncWaitHandle.WaitOne(12000)) {
            $client = $listener.EndAcceptTcpClient($accept)
        }
        Start-Sleep -Seconds 2

        $udp = New-Object System.Net.Sockets.UdpClient
        $packet = New-InitPacket -Band $Scenario.Band
        [void]$udp.Send($packet, $packet.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $control = New-ControlStartPacket
        [void]$udp.Send($control, $control.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $display = New-DisplayPacket -TimeMs ([double](($Scenario.Band * 1000) + 77)) -EngineState $Scenario.Engine -StrikeFlag $Scenario.Strike -StrikePart $Scenario.Part
        [void]$udp.Send($display, $display.Length, "127.0.0.1", 8888)
        Start-Sleep -Milliseconds $DelayMs

        $tcpFrame = Receive-LatestJpegFrame -Client $client -Label $safeName -DurationMs 6500
        if ($tcpFrame -and $tcpFrame.success -and $tcpFrame.path) {
            $frame = $tcpFrame
            $frameMetrics = Get-ImageLumaMetrics -Path $frame.path
        }
        elseif (Test-Path -LiteralPath $dumpPath -PathType Leaf) {
            $frame = [PSCustomObject]@{
                path = $dumpPath
                frame_count = 1
                source = "render_texture_dump"
                success = $true
                failure_reason = $(if ($tcpFrame) { $tcpFrame.failure_reason } else { "tcp_capture_not_attempted" })
                header_count = $(if ($tcpFrame) { $tcpFrame.header_count } else { 0 })
                invalid_header_count = $(if ($tcpFrame) { $tcpFrame.invalid_header_count } else { 0 })
                invalid_jpeg_count = $(if ($tcpFrame) { $tcpFrame.invalid_jpeg_count } else { 0 })
                timeout_count = $(if ($tcpFrame) { $tcpFrame.timeout_count } else { 0 })
                bytes_read = $(if ($tcpFrame) { $tcpFrame.bytes_read } else { 0 })
                first_invalid_header_hex = $(if ($tcpFrame) { $tcpFrame.first_invalid_header_hex } else { "NA" })
                last_length = $(if ($tcpFrame) { $tcpFrame.last_length } else { "NA" })
            }
            $frameMetrics = Get-ImageLumaMetrics -Path $dumpPath
        }

        if ($null -eq $frameMetrics) {
            if ($tcpFrame) {
                $captureFailureReason = "$($tcpFrame.failure_reason); render_texture_dump_path=$dumpPath; render_texture_dump_exists=$((Test-Path -LiteralPath $dumpPath -PathType Leaf))"
            }
            else {
                $captureFailureReason = "tcp_capture_not_attempted; render_texture_dump_path=$dumpPath; render_texture_dump_exists=$((Test-Path -LiteralPath $dumpPath -PathType Leaf))"
            }
        }
    }
    finally {
        if ($udp) { $udp.Close() }
        if ($client) { $client.Close() }
        if ($listener) { $listener.Stop() }
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
        Scenario     = $Scenario
        Stdout       = $stdout
        Stderr       = $stderr
        Text         = $text
        Frame        = $frame
        TcpFrame     = $tcpFrame
        FrameMetrics = $frameMetrics
        CaptureFailureReason = $captureFailureReason
    }
}

function Get-Stage5Metrics {
    param(
        [string]$Text,
        [string]$ExpectedBand
    )

    $pattern = "\[Stage5 Radiance\].*band=([A-Za-z]+).*debugViewMode=([A-Za-z]+).*toneMap=([A-Za-z]+).*useBaseTextureModulation=([01]).*bodyRadiance=([0-9eE+\-.]+).*hotspotRadiance=([0-9eE+\-.]+).*brightspotRadiance=([0-9eE+\-.]+).*bodyGrayBeforeFloor=([0-9eE+\-.]+).*bodyGrayAfterFloor=([0-9eE+\-.]+).*hotspotGray=([0-9eE+\-.]+).*brightspotGray=([0-9eE+\-.]+).*finalGrayDebug=([0-9eE+\-.]+).*debugFloorApplied=([01])"
    $allMatches = [regex]::Matches($Text, $pattern)
    $matches = @($allMatches | Where-Object { $_.Groups[1].Value -eq $ExpectedBand })
    if ($matches.Count -eq 0) {
        return $null
    }

    $metrics = [PSCustomObject]@{
        bandName            = $matches[0].Groups[1].Value
        debugViewMode       = $matches[0].Groups[2].Value
        toneMap             = $matches[0].Groups[3].Value
        useBaseTexture      = [int]$matches[0].Groups[4].Value
        bodyRadiance        = 0.0
        hotspotRadiance     = 0.0
        brightspotRadiance  = 0.0
        bodyGrayBeforeFloor = 0.0
        bodyGrayAfterFloor  = 0.0
        hotspotGray         = 0.0
        brightspotGray      = 0.0
        finalGrayDebug      = 0.0
        debugFloorApplied   = 0
        samples             = $matches.Count
    }
    foreach ($m in $matches) {
        $metrics.bodyRadiance = [Math]::Max($metrics.bodyRadiance, [double]$m.Groups[5].Value)
        $metrics.hotspotRadiance = [Math]::Max($metrics.hotspotRadiance, [double]$m.Groups[6].Value)
        $metrics.brightspotRadiance = [Math]::Max($metrics.brightspotRadiance, [double]$m.Groups[7].Value)
        $metrics.bodyGrayBeforeFloor = [Math]::Max($metrics.bodyGrayBeforeFloor, [double]$m.Groups[8].Value)
        $metrics.bodyGrayAfterFloor = [Math]::Max($metrics.bodyGrayAfterFloor, [double]$m.Groups[9].Value)
        $metrics.hotspotGray = [Math]::Max($metrics.hotspotGray, [double]$m.Groups[10].Value)
        $metrics.brightspotGray = [Math]::Max($metrics.brightspotGray, [double]$m.Groups[11].Value)
        $metrics.finalGrayDebug = [Math]::Max($metrics.finalGrayDebug, [double]$m.Groups[12].Value)
        $metrics.debugFloorApplied = [Math]::Max($metrics.debugFloorApplied, [int]$m.Groups[13].Value)
    }
    return $metrics
}

Assert-Path $hwaExe "HwaSimIR executable"
Assert-Path $hwaWorkDir "HwaSimIR working directory"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
New-Item -ItemType Directory -Force -Path $frameDir | Out-Null

$allScenarios = @(
    @{ Label = "mwir_bodyonly"; Band = 2; ViewMode = "BodyOnly"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "mwir_composite_engine_off"; Band = 2; ViewMode = "Composite"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "mwir_composite_engine_on"; Band = 2; ViewMode = "Composite"; Engine = $true; Strike = $false; Part = 0; Expect = "body_hotspot" },
    @{ Label = "lwir_bodyonly"; Band = 3; ViewMode = "BodyOnly"; Engine = $false; Strike = $false; Part = 0; Expect = "body" },
    @{ Label = "nir_brightspotonly"; Band = 1; ViewMode = "BrightSpotOnly"; Engine = $false; Strike = $true; Part = 1; Expect = "brightspot" },
    @{ Label = "swir_brightspotonly"; Band = 0; ViewMode = "BrightSpotOnly"; Engine = $false; Strike = $true; Part = 2; Expect = "brightspot" }
)
$scenarios = @($allScenarios | Where-Object { $Bands -contains $_.Band })

Write-Host "Stage 5 output visibility smoke"
Write-Host "Bands: $($Bands -join ', ')"
Write-Host "HwaSimIR: $hwaExe"
Write-Host ""

$rows = New-Object System.Collections.Generic.List[object]
foreach ($scenario in $scenarios) {
    Write-Host "running scenario: $($scenario.Label)"
    $run = Invoke-HwaStage5OutputRun -Scenario $scenario
    $expectedBandName = Get-Stage5BandName -Band $scenario.Band
    $metrics = Get-Stage5Metrics -Text $run.Text -ExpectedBand $expectedBandName
    $metricsPresent = $null -ne $metrics
    $frameMetricsPresent = $null -ne $run.FrameMetrics
    $status = "PASS"
    $reason = "log and frame metrics verified"

    if (-not $metricsPresent) {
        $status = "FAIL"
        $reason = "missing Stage5 Radiance metrics for expected band $expectedBandName"
    }
    elseif ($metrics.debugViewMode -ne $scenario.ViewMode) {
        $status = "FAIL"
        $reason = "debugViewMode mismatch"
    }
    elseif (($scenario.Expect -eq "body" -or $scenario.Expect -eq "body_hotspot") -and $metrics.bodyGrayAfterFloor -lt $minBodyGray) {
        $status = "FAIL"
        $reason = "bodyGrayAfterFloor below Stage5DebugMinBodyGray"
    }
    elseif ($metrics.finalGrayDebug -le 0.0) {
        $status = "FAIL"
        $reason = "finalGrayDebug is not positive"
    }
    elseif ($scenario.Expect -eq "body_hotspot" -and $metrics.hotspotGray -lt $minLocalFeatureGray) {
        $status = "FAIL"
        $reason = "engine on hotspotGray below local feature threshold"
    }
    elseif ($scenario.Expect -eq "brightspot" -and $metrics.brightspotGray -lt $minLocalFeatureGray) {
        $status = "FAIL"
        $reason = "strikeFlag brightspotGray below local feature threshold"
    }
    elseif (-not $frameMetricsPresent) {
        $status = "FAIL"
        $reason = "frame metrics unavailable: $($run.CaptureFailureReason)"
    }
    elseif ($run.FrameMetrics.max_luma -le $brightThreshold -or $run.FrameMetrics.bright_pixel_count -le 0) {
        $status = "FAIL"
        $reason = "captured frame has no visible luma above threshold"
    }
    elseif ($run.FrameMetrics.contrast_pixel_count -lt $minContrastPixelCount) {
        $status = "FAIL"
        $reason = "captured frame has no visible non-background target contrast"
    }

    $frameNote = $(if ($frameMetricsPresent) { "frame_metrics_available:$($run.Frame.source)" } else { "frame_metrics_unavailable:$($run.CaptureFailureReason)" })
    $rows.Add([PSCustomObject]@{
        scenario = $scenario.Label
        band = $scenario.Band
        target_range_km = $primaryTargetRangeKm
        target_lon_offset_deg = [Math]::Round($primaryTargetLonOffsetDeg, 6)
        expected_band = $expectedBandName
        observed_band = $(if ($metricsPresent) { $metrics.bandName } else { "NA" })
        view_mode = $scenario.ViewMode
        status = $status
        reason = $reason
        bodyRadiance = $(if ($metricsPresent) { $metrics.bodyRadiance } else { "NA" })
        hotspotRadiance = $(if ($metricsPresent) { $metrics.hotspotRadiance } else { "NA" })
        brightspotRadiance = $(if ($metricsPresent) { $metrics.brightspotRadiance } else { "NA" })
        bodyGrayAfterFloor = $(if ($metricsPresent) { $metrics.bodyGrayAfterFloor } else { "NA" })
        hotspotGray = $(if ($metricsPresent) { $metrics.hotspotGray } else { "NA" })
        brightspotGray = $(if ($metricsPresent) { $metrics.brightspotGray } else { "NA" })
        finalGrayDebug = $(if ($metricsPresent) { $metrics.finalGrayDebug } else { "NA" })
        mean_luma = $(if ($frameMetricsPresent) { $run.FrameMetrics.mean_luma } else { "NA" })
        max_luma = $(if ($frameMetricsPresent) { $run.FrameMetrics.max_luma } else { "NA" })
        bright_pixel_count = $(if ($frameMetricsPresent) { $run.FrameMetrics.bright_pixel_count } else { "NA" })
        background_luma = $(if ($frameMetricsPresent) { $run.FrameMetrics.background_luma } else { "NA" })
        contrast_pixel_count = $(if ($frameMetricsPresent) { $run.FrameMetrics.contrast_pixel_count } else { "NA" })
        max_contrast_delta = $(if ($frameMetricsPresent) { $run.FrameMetrics.max_contrast_delta } else { "NA" })
        frame_metrics_note = $frameNote
        frame_path = $(if ($run.Frame) { $run.Frame.path } else { "NA" })
        frame_capture_source = $(if ($run.Frame) { $run.Frame.source } else { "NA" })
        capture_failure_reason = $(if ($frameMetricsPresent) { "none" } else { $run.CaptureFailureReason })
        captured_frame_count = $(if ($run.Frame) { $run.Frame.frame_count } else { "NA" })
        tcp_failure_reason = $(if ($run.TcpFrame) { $run.TcpFrame.failure_reason } else { "NA" })
        tcp_header_count = $(if ($run.TcpFrame) { $run.TcpFrame.header_count } else { "NA" })
        tcp_invalid_header_count = $(if ($run.TcpFrame) { $run.TcpFrame.invalid_header_count } else { "NA" })
        tcp_invalid_jpeg_count = $(if ($run.TcpFrame) { $run.TcpFrame.invalid_jpeg_count } else { "NA" })
        tcp_timeout_count = $(if ($run.TcpFrame) { $run.TcpFrame.timeout_count } else { "NA" })
        tcp_bytes_read = $(if ($run.TcpFrame) { $run.TcpFrame.bytes_read } else { "NA" })
        tcp_first_invalid_header_hex = $(if ($run.TcpFrame) { $run.TcpFrame.first_invalid_header_hex } else { "NA" })
        tcp_last_length = $(if ($run.TcpFrame) { $run.TcpFrame.last_length } else { "NA" })
        stdout = $run.Stdout
        stderr = $run.Stderr
    }) | Out-Null
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -LiteralPath $csvPath

$md = New-Object System.Collections.Generic.List[string]
$md.Add("# Stage5 Output Visibility Smoke Summary") | Out-Null
$md.Add("") | Out-Null
$md.Add("Bands: $($Bands -join ', ')") | Out-Null
$md.Add("Target geometry: primary visible target is placed at approximately $primaryTargetRangeKm km (`lonOffset=$([Math]::Round($primaryTargetLonOffsetDeg, 6)) deg`); this avoids the old 1.1 km close-target case while keeping local hotspot/brightspot visible under the default legacy tau path. Hidden companion targets remain outside `targetNumValid`.") | Out-Null
$md.Add("Frame metrics threshold: bright_pixel_count uses luma >= $brightThreshold.") | Out-Null
$md.Add("Contrast threshold: contrast_pixel_count uses abs(luma - dominant_background_luma) >= $contrastThreshold outside the top-right FPS overlay; PASS requires >= $minContrastPixelCount.") | Out-Null
$md.Add("Local feature threshold: engine-on `hotspotGray` and strike `brightspotGray` must be >= $minLocalFeatureGray, so near-zero local features cannot pass from body floor alone.") | Out-Null
$md.Add("") | Out-Null
$md.Add("| Scenario | Status | Band | Range km | Expected | Observed | View | Body gray | Hotspot gray | Brightspot gray | Final gray | Mean luma | Max luma | Bright px | Background | Contrast px | Max delta | Frame metrics | Source | Frame | TCP reason | Log |") | Out-Null
$md.Add("|---|---|---:|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|---|") | Out-Null
foreach ($row in $rows) {
    $md.Add("| $($row.scenario) | $($row.status) | $($row.band) | $($row.target_range_km) | $($row.expected_band) | $($row.observed_band) | $($row.view_mode) | $($row.bodyGrayAfterFloor) | $($row.hotspotGray) | $($row.brightspotGray) | $($row.finalGrayDebug) | $($row.mean_luma) | $($row.max_luma) | $($row.bright_pixel_count) | $($row.background_luma) | $($row.contrast_pixel_count) | $($row.max_contrast_delta) | $($row.frame_metrics_note) | $($row.frame_capture_source) | $($row.frame_path) | $($row.tcp_failure_reason) | $($row.stdout) |") | Out-Null
}
$md.Add("") | Out-Null
$md.Add("Capture rule: TCP length-prefixed JPEG is attempted first; if no decodable JPEG is received, HwaSimIR writes a smoke-only render texture PNG via Stage5OutputFrameDumpPath. Frame metrics are required for PASS.") | Out-Null
$md.Add("Band rule: Stage5 log metrics are filtered to the scenario expected band before validation, so concurrent external UDP stimuli do not satisfy the scenario checks.") | Out-Null
$md.Add("When capture still fails, capture_failure_reason and TCP diagnostics in the CSV identify the concrete failure instead of reporting a bare unavailable state.") | Out-Null
$md.Add("Boundary: this remains Stage 5A debug output validation; AGC/noise/MTF stay in Stage 6.") | Out-Null
$md | Set-Content -LiteralPath $summaryPath -Encoding UTF8

$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "Summary:"
Write-Host $summaryPath
Write-Host $csvPath

$failed = @($rows | Where-Object { $_.status -ne "PASS" })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Stage5 output visibility smoke failed:" -ForegroundColor Red
    $failed | Format-List
    exit 1
}

Write-Host ""
Write-Host "Stage5 output visibility smoke passed." -ForegroundColor Green
